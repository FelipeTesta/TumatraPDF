// test-fixture-guard.ts — guards against tests polluting tracked repo files.
// Usage: bun scripts/test-fixture-guard.ts [--root <dir>] [--allow <path>...] [--sentinel <file>] [--log <file>] [--self-test] -- <command> [args...]
// Everything after `--` is the command to run.
// Optional: --root <dir> to point at another repository.
// Optional: --allow <path> (repeatable) for paths legitimately modified by the suite.
// Optional: --sentinel <file> path to write a sentinel file when pollution is detected (CI uses this).
// Optional: --log <file> path to tee command output to a log file (CI uses this to grep the marker).
// Optional: --self-test runs the hermetic self-test against a throwaway repo in tests/tmp/ and exits.
//
// Exit codes:
//   0 = clean (no tracked file changed) — exits with the command's own exit code
//   1 = pollution detected (prints FIXTURE-GUARD-POLLUTION marker, writes sentinel if --sentinel given)

import { execFileSync, spawnSync } from "node:child_process";
import { resolve, join } from "node:path";
import { writeFileSync, mkdirSync, rmSync } from "node:fs";
import { tmpdir } from "node:os";

interface FileState {
  path: string;
  status: string; // git status code: M, A, D, R, etc.
  staged: boolean;
}

function parsePorcelainV1Z(
  output: string,
): Map<string, { status: string; staged: boolean }> {
  // git status --porcelain=v1 -z outputs NUL-separated records:
  //   XY pathname NUL  (for unmerged: XY pathname NUL original NUL)
  // XY = two status chars (index, worktree), e.g. " M", "M ", "MM", "A ", " R", "??"
  // Paths with special chars are C-quoted (e.g. "path with space").
  const map = new Map<string, { status: string; staged: boolean }>();
  const parts = output.split("\0");
  for (let i = 0; i < parts.length - 1; i++) {
    const record = parts[i];
    if (!record) continue;
    const status = record.slice(0, 2);
    let path = record.slice(3);
    // Handle C-quoted paths (git quotes paths with spaces, backslashes, etc.)
    if (path.startsWith('"') && path.endsWith('"')) {
      // Unquote: remove surrounding quotes and unescape \"
      path = path.slice(1, -1).replace(/\\(.)/g, "$1");
    }
    if (status === "??") continue; // untracked
    // For renamed entries (R), the format is "R  old -> new" - we get the new path
    // The porcelain v1 -z format for renames gives the destination path
    map.set(path, {
      status: status[1] !== " " ? status[1] : status[0],
      staged: status[0] !== " ",
    });
  }
  return map;
}

function getTrackedState(root: string): Map<string, FileState> {
  const map = new Map<string, FileState>();
  // Unstaged + staged in one call with -z for robust parsing
  const statusOut = execFileSync("git", ["status", "--porcelain=v1", "-z"], {
    cwd: root,
    encoding: "utf8",
  });
  const parsed = parsePorcelainV1Z(statusOut);
  for (const [path, { status, staged }] of parsed) {
    map.set(path, { path, status, staged });
  }
  return map;
}

function getDiffSnapshot(root: string): string {
  // Content-level snapshot: staged + unstaged diff against HEAD
  // This catches content changes even if status code doesn't change (e.g. M -> M with more edits)
  const diff = execFileSync("git", ["diff", "HEAD"], {
    cwd: root,
    encoding: "utf8",
  });
  return diff;
}

function getUntrackedFiles(root: string): string[] {
  const statusOut = execFileSync("git", ["status", "--porcelain=v1", "-z"], {
    cwd: root,
    encoding: "utf8",
  });
  const untracked: string[] = [];
  const parts = statusOut.split("\0");
  for (let i = 0; i < parts.length - 1; i++) {
    const record = parts[i];
    if (!record) continue;
    const status = record.slice(0, 2);
    if (status === "??") {
      let path = record.slice(3);
      if (path.startsWith('"') && path.endsWith('"')) {
        path = path.slice(1, -1).replace(/\\(.)/g, "$1");
      }
      untracked.push(path);
    }
  }
  return untracked;
}

function compareSnapshots(
  beforeStatus: Map<string, FileState>,
  afterStatus: Map<string, FileState>,
  beforeDiff: string,
  afterDiff: string,
  allowSet: Set<string>,
): { polluted: FileState[]; diffChanged: boolean; diffDetails: string } {
  const polluted: FileState[] = [];

  // Status-level comparison (catches added/deleted/renamed/staged changes)
  for (const [path, afterState] of afterStatus) {
    const beforeState = beforeStatus.get(path);
    const isAllowed = allowSet.has(path);
    if (!beforeState) {
      // New entry in tracked state = file was clean before, now has changes
      if (!isAllowed) {
        polluted.push(afterState);
      }
    } else if (
      beforeState.status !== afterState.status ||
      beforeState.staged !== afterState.staged
    ) {
      if (!isAllowed) {
        polluted.push(afterState);
      }
    }
  }

  // Check for deleted tracked files
  for (const [path, beforeState] of beforeStatus) {
    if (!afterStatus.has(path) && !allowSet.has(path)) {
      polluted.push({ ...beforeState, status: "D", staged: false });
    }
  }

  // Content-level comparison (catches M->M with further edits)
  const diffChanged = beforeDiff !== afterDiff;
  let diffDetails = "";
  if (diffChanged) {
    // Find which tracked files have content changes not reflected in status
    const beforeFiles = new Set<string>();
    const afterFiles = new Set<string>();
    // Parse diff headers to find affected files
    const diffHeaderRe = /^diff --git a\/(.+) b\//gm;
    let m: RegExpExecArray | null;
    while ((m = diffHeaderRe.exec(afterDiff)) !== null) {
      afterFiles.add(m[1]);
    }
    while ((m = diffHeaderRe.exec(beforeDiff)) !== null) {
      beforeFiles.add(m[1]);
    }
    const changedFiles = new Set(
      [...afterFiles].filter(
        (f) => !beforeFiles.has(f) || afterDiff.includes(f),
      ),
    );
    diffDetails = `Content diff changed (affected: ${[...changedFiles].join(", ") || "unknown"})`;
  }

  return { polluted, diffChanged, diffDetails };
}

function runSelfTest(): void {
  console.log("=== FIXTURE-GUARD SELF-TEST ===");
  // Use a subdirectory of the main repo so git is in PATH
  const mainRoot = resolve(".");
  const testRoot = join(
    mainRoot,
    "sumatrapdf-src",
    "tests",
    "tmp",
    `fixture-guard-self-test-${Date.now()}`,
  );
  // Sentinel and log files go to system temp to avoid polluting the test repo
  const sentinelDir = join(tmpdir(), `fixture-guard-sentinels-${Date.now()}`);
  mkdirSync(testRoot, { recursive: true });
  mkdirSync(sentinelDir, { recursive: true });
  console.log(`Test repo: ${testRoot}`);
  console.log(`Sentinel dir: ${sentinelDir}`);

  try {
    // Init git repo
    execFileSync("git", ["init"], { cwd: testRoot, stdio: "ignore" });
    execFileSync("git", ["config", "user.email", "test@test.com"], {
      cwd: testRoot,
      stdio: "ignore",
    });
    execFileSync("git", ["config", "user.name", "Test"], {
      cwd: testRoot,
      stdio: "ignore",
    });

    // Create tracked file
    writeFileSync(join(testRoot, "tracked.txt"), "original content\n");
    execFileSync("git", ["add", "tracked.txt"], {
      cwd: testRoot,
      stdio: "ignore",
    });
    execFileSync("git", ["commit", "-m", "initial"], {
      cwd: testRoot,
      stdio: "ignore",
    });

    let passed = 0;
    let failed = 0;

    function runCase(
      name: string,
      cmd: string[],
      expectPollution: boolean,
    ): boolean {
      const sentinelFile = join(
        sentinelDir,
        `sentinel-${Date.now()}-${Math.random().toString(36).slice(2)}`,
      );
      const logFile = join(
        sentinelDir,
        `log-${Date.now()}-${Math.random().toString(36).slice(2)}`,
      );
      const args = [
        "--root",
        testRoot,
        "--sentinel",
        sentinelFile,
        "--log",
        logFile,
        "--",
        ...cmd,
      ];
      const result = spawnSync(
        "bun",
        ["scripts/test-fixture-guard.ts", ...args],
        {
          cwd: resolve("."),
          stdio: "pipe",
          shell: false,
        },
      );
      const stdout = result.stdout.toString();
      const stderr = result.stderr.toString();
      const output = stdout + stderr;
      const hasMarker = output.includes("FIXTURE-GUARD-POLLUTION");
      const hasSentinel = require("node:fs").existsSync(sentinelFile);
      const exitCode = result.status ?? 0;

      const pollutionDetected = hasMarker || hasSentinel;
      const ok =
        pollutionDetected === expectPollution &&
        exitCode === (expectPollution ? 1 : 0);
      console.log(
        `  ${ok ? "PASS" : "FAIL"}: ${name} (exit=${exitCode}, marker=${hasMarker}, sentinel=${hasSentinel})`,
      );
      if (!ok) {
        console.log(`    Output: ${output.slice(0, 200)}`);
        failed++;
      } else {
        passed++;
      }
      return ok;
    }

    // Case 1: modify tracked file -> pollution
    runCase(
      "1. modify tracked file",
      ["cmd", "/c", "echo modified > tracked.txt"],
      true,
    );

    // Reset tracked file
    execFileSync("git", ["checkout", "tracked.txt"], {
      cwd: testRoot,
      stdio: "ignore",
    });

    // Case 2: create untracked file -> clean (informational only)
    runCase(
      "2. create untracked file",
      ["cmd", "/c", "echo new > untracked.txt"],
      false,
    );

    // Clean up untracked
    rmSync(join(testRoot, "untracked.txt"), { force: true });

    // Case 3: clean command -> clean
    runCase("3. clean command", ["cmd", "/c", "exit 0"], false);

    // Case 4: modify tracked file that was ALREADY modified before run -> pollution (content-level)
    writeFileSync(join(testRoot, "tracked.txt"), "pre-modified\n");
    execFileSync("git", ["add", "tracked.txt"], {
      cwd: testRoot,
      stdio: "ignore",
    });
    // Now it's staged but not committed - the test will modify it further
    runCase(
      "4. modify already-modified tracked file",
      ["cmd", "/c", "echo further > tracked.txt"],
      true,
    );

    // Reset for case 5
    execFileSync("git", ["reset", "HEAD", "tracked.txt"], {
      cwd: testRoot,
      stdio: "ignore",
    });
    execFileSync("git", ["checkout", "tracked.txt"], {
      cwd: testRoot,
      stdio: "ignore",
    });

    // Case 5: CI-equivalent detection (sentinel + log)
    const sentinel5 = join(sentinelDir, "ci-sentinel");
    const log5 = join(sentinelDir, "ci-log");
    const args5 = [
      "--root",
      testRoot,
      "--sentinel",
      sentinel5,
      "--log",
      log5,
      "--",
      "cmd",
      "/c",
      "echo polluted > tracked.txt",
    ];
    const result5 = spawnSync(
      "bun",
      ["scripts/test-fixture-guard.ts", ...args5],
      {
        cwd: resolve("."),
        stdio: "pipe",
        shell: false,
      },
    );
    const out5 = result5.stdout.toString() + result5.stderr.toString();
    const sentinelExists5 = require("node:fs").existsSync(sentinel5);
    const logContainsMarker5 = out5.includes("FIXTURE-GUARD-POLLUTION");
    // CI check: fail if sentinel exists OR log contains marker
    const ciWouldFail = sentinelExists5 || logContainsMarker5;
    console.log(
      `  ${ciWouldFail ? "PASS" : "FAIL"}: 5. CI-equivalent detection (sentinel=${sentinelExists5}, logMarker=${logContainsMarker5}, ciFail=${ciWouldFail})`,
    );
    if (ciWouldFail) passed++;
    else failed++;

    console.log(
      `\n=== SELF-TEST SUMMARY: ${passed} passed, ${failed} failed ===`,
    );
    if (failed > 0) process.exit(1);
  } finally {
    // Cleanup
    try {
      rmSync(testRoot, { recursive: true, force: true });
    } catch {}
    try {
      rmSync(sentinelDir, { recursive: true, force: true });
    } catch {}
  }
}

function main(): void {
  const args = process.argv.slice(2);

  // Handle --self-test first
  if (args.includes("--self-test")) {
    runSelfTest();
    return;
  }

  // Parse --root, --allow, --sentinel, --log
  let root = process.cwd();
  const allowSet = new Set<string>();
  let sentinelFile: string | null = null;
  let logFile: string | null = null;
  let cmdStart = -1;

  for (let i = 0; i < args.length; i++) {
    if (args[i] === "--root") {
      root = resolve(args[++i]);
    } else if (args[i] === "--allow") {
      allowSet.add(args[++i]);
    } else if (args[i] === "--sentinel") {
      sentinelFile = resolve(args[++i]);
    } else if (args[i] === "--log") {
      logFile = resolve(args[++i]);
    } else if (args[i] === "--") {
      cmdStart = i + 1;
      break;
    }
  }

  if (cmdStart === -1) {
    console.error(
      "Usage: bun scripts/test-fixture-guard.ts [--root <dir>] [--allow <path>...] [--sentinel <file>] [--log <file>] [--self-test] -- <command> [args...]",
    );
    process.exit(2);
  }

  const cmd = args.slice(cmdStart);
  if (cmd.length === 0) {
    console.error("No command provided after --");
    process.exit(2);
  }

  // Snapshot before
  const beforeStatus = getTrackedState(root);
  const beforeDiff = getDiffSnapshot(root);

  // Run the command (tee to log file if --log given)
  let result;
  if (logFile) {
    // We need to capture output AND tee to file. Use a simple approach:
    // spawn with pipe, then write to file and inherit to console
    const child = spawnSync(cmd[0], cmd.slice(1), {
      cwd: root,
      stdio: ["ignore", "pipe", "pipe"],
      shell: process.platform === "win32",
    });
    const stdout = child.stdout.toString();
    const stderr = child.stderr.toString();
    // Write to log file
    writeFileSync(logFile, stdout + stderr, "utf8");
    // Also print to console (inherited behavior)
    process.stdout.write(stdout);
    process.stderr.write(stderr);
    result = { status: child.status, stdout, stderr };
  } else {
    result = spawnSync(cmd[0], cmd.slice(1), {
      cwd: root,
      stdio: "inherit",
      shell: process.platform === "win32",
    });
  }

  // Snapshot after
  const afterStatus = getTrackedState(root);
  const afterDiff = getDiffSnapshot(root);
  const untrackedAfter = getUntrackedFiles(root);

  // Compare tracked files (status + content-level)
  const { polluted, diffChanged, diffDetails } = compareSnapshots(
    beforeStatus,
    afterStatus,
    beforeDiff,
    afterDiff,
    allowSet,
  );

  // Report untracked files as informational
  if (untrackedAfter.length > 0) {
    console.log("fixture-guard: untracked files created (informational):");
    for (const u of untrackedAfter) console.log(`  ${u}`);
  }

  // Determine if polluted
  const isPolluted = polluted.length > 0 || diffChanged;

  if (isPolluted) {
    console.error("fixture-guard: TRACKED FILES CHANGED (pollution):");
    for (const p of polluted) {
      const staged = p.staged ? " (staged)" : "";
      console.error(`  ${p.status}${staged}  ${p.path}`);
    }
    if (diffChanged) {
      console.error(`  CONTENT ${diffDetails}`);
    }
    console.error("FIXTURE-GUARD-POLLUTION");

    // Write sentinel file if requested
    if (sentinelFile) {
      try {
        mkdirSync(resolve(sentinelFile, ".."), { recursive: true });
        writeFileSync(sentinelFile, "polluted\n", "utf8");
      } catch (e) {
        console.error(`fixture-guard: failed to write sentinel: ${e}`);
      }
    }
    process.exit(1);
  }

  // Clean run
  const exitCode = result.status ?? 0;
  if (exitCode === 0) {
    console.log("fixture-guard: OK (no tracked file changed)");
  } else {
    console.log(
      `fixture-guard: OK (no tracked file changed; command exited ${exitCode})`,
    );
  }
  process.exit(exitCode);
}

main();
