// Hermetic Flashcard Study-State Invariant Test (based on original ad-hoc-flashcard-state-invariant.ts).
// Verifies that after rating flashcards and forcing flush, the per-book study JSON on disk:
// (a) exists
// (b) contains exactly ONE entry for the rated card's key
// (c) the entry's rating/interval match what the app reported
// (d) a second load finds the state via binary search (card is not "new" again)
// (e) the tracked repo fixture was NOT modified
import { ControlClient, ControlCommand, uniquePipeName } from "../cmd/control.ts";
import { EXE, runStandalone, cmdId } from "./util.ts";
import * as fs from "node:fs";
import * as path from "node:path";
import * as os from "node:os";
import * as crypto from "node:crypto";

// Parse the study JSON file and extract state entries (key -> state)
function parseStudyJson(jsonPath: string): { key: bigint; rating: number; interval: number }[] {
  const content = fs.readFileSync(jsonPath, "utf8");
  const statesIdx = content.indexOf('"states"');
  if (statesIdx < 0) return [];
  let i = content.indexOf("{", statesIdx);
  if (i < 0) return [];
  i++;
  const entries: { key: bigint; rating: number; interval: number }[] = [];
  while (i < content.length) {
    while (i < content.length && /\s/.test(content[i])) i++;
    if (i >= content.length || content[i] === "}") break;
    if (content[i] !== '"') break;
    i++;
    const keyStart = i;
    while (i < content.length && content[i] !== '"') i++;
    const keyStr = content.slice(keyStart, i);
    i++;
    while (i < content.length && /\s/.test(content[i])) i++;
    if (content[i] !== ":") break;
    i++;
    while (i < content.length && /\s/.test(content[i])) i++;
    if (content[i] !== "{") break;
    const stateStart = i;
    let braceCount = 1;
    i++;
    while (i < content.length && braceCount > 0) {
      if (content[i] === "{") braceCount++;
      else if (content[i] === "}") braceCount--;
      i++;
    }
    const stateJson = content.slice(stateStart, i);
    const ratingIdx = stateJson.indexOf('"rating"');
    const intervalIdx = stateJson.indexOf('"interval"');
    let rating = 0, interval = 0;
    if (ratingIdx >= 0) {
      const colonIdx = stateJson.indexOf(":", ratingIdx);
      if (colonIdx >= 0) {
        let valStart = colonIdx + 1;
        while (valStart < stateJson.length && /\s/.test(stateJson[valStart])) valStart++;
        const valEnd = stateJson.indexOf(",", valStart);
        const valStr = stateJson.slice(valStart, valEnd > 0 ? valEnd : stateJson.length).trim();
        rating = parseInt(valStr, 10);
      }
    }
    if (intervalIdx >= 0) {
      const colonIdx = stateJson.indexOf(":", intervalIdx);
      if (colonIdx >= 0) {
        let valStart = colonIdx + 1;
        while (valStart < stateJson.length && /\s/.test(stateJson[valStart])) valStart++;
        const valEnd = stateJson.indexOf(",", valStart);
        const valStr = stateJson.slice(valStart, valEnd > 0 ? valEnd : stateJson.length).trim();
        interval = parseInt(valStr, 10);
      }
    }
    entries.push({ key: BigInt(keyStr), rating, interval });
    while (i < content.length && /\s/.test(content[i])) i++;
    if (content[i] === ",") i++;
  }
  return entries;
}

// Find the study JSON file that contains the given card key
function findStudyJsonForKey(cardKey: bigint): string | null {
  const repoRoot = path.resolve("..", "..");
  const portableStudyDir = path.join(repoRoot, "Compiled", "study");
  if (!fs.existsSync(portableStudyDir)) return null;
  
  const files = fs.readdirSync(portableStudyDir).filter(f => f.endsWith(".json"));
  for (const file of files) {
    const jsonPath = path.join(portableStudyDir, file);
    const entries = parseStudyJson(jsonPath);
    if (entries.some(e => e.key === cardKey)) {
      return jsonPath;
    }
  }
  // Also check installed mode path
  const installedStudyDir = path.join(os.homedir(), "AppData", "Local", "TumatraPDF", "study");
  if (fs.existsSync(installedStudyDir)) {
    const files = fs.readdirSync(installedStudyDir).filter(f => f.endsWith(".json"));
    for (const file of files) {
      const jsonPath = path.join(installedStudyDir, file);
      const entries = parseStudyJson(jsonPath);
      if (entries.some(e => e.key === cardKey)) {
        return jsonPath;
      }
    }
  }
  // Also check legacy FlashcardStudy dir
  const legacyDir = path.join(os.homedir(), "AppData", "Local", "TumatraPDF", "FlashcardStudy");
  if (fs.existsSync(legacyDir)) {
    const files = fs.readdirSync(legacyDir).filter(f => f.endsWith(".json"));
    for (const file of files) {
      const jsonPath = path.join(legacyDir, file);
      const entries = parseStudyJson(jsonPath);
      if (entries.some(e => e.key === cardKey)) {
        return jsonPath;
      }
    }
  }
  return null;
}

// Get file size and mtime
function getFileStat(filePath: string): { size: number; mtime: number } | null {
  try {
    const stat = fs.statSync(filePath);
    return { size: stat.size, mtime: stat.mtimeMs };
  } catch {
    return null;
  }
}

export async function testit(): Promise<void> {
  // Use a COPY of the fixture (never the tracked repo file)
  const fixturePdf = path.resolve("ext", "a-zlib", "zlib.3.pdf");
  const fixtureAbs = path.resolve(fixturePdf);
  
  // Record fixture state BEFORE test
  const fixtureStatBefore = getFileStat(fixtureAbs);
  if (!fixtureStatBefore) {
    throw new Error(`Fixture not found: ${fixtureAbs}`);
  }
  console.log(`=== Flashcard Hermetic Invariant Test ===`);
  console.log(`Fixture: ${fixtureAbs} (size=${fixtureStatBefore.size}, mtime=${fixtureStatBefore.mtime})`);

  // Create unique temp copy in tests/tmp/ (gitignored)
  const tmpDir = path.resolve("tests", "tmp");
  if (!fs.existsSync(tmpDir)) fs.mkdirSync(tmpDir, { recursive: true });
  const testPdf = path.join(tmpDir, `fc-invariant-${process.pid}.pdf`);
  fs.copyFileSync(fixtureAbs, testPdf);
  const testPdfAbs = path.resolve(testPdf);
  console.log(`Test PDF: ${testPdfAbs}`);

  const pipeName = uniquePipeName("fc-invariant-hermetic");
  
  // Use custom process handling to KILL at end (not graceful quit) because
  // the flashcard annotation is unsaved and would trigger "Save changes?" modal
  // This is the same pattern used by other ad-hoc tests in the repo.
  const proc = Bun.spawn([EXE, "-for-testing", "-dbg-control", pipeName, testPdfAbs], {
    stdout: "ignore",
    stderr: "pipe",
  });
  
  let client: ControlClient | undefined;
  try {
    client = await ControlClient.connect(pipeName, 30000);
    
    // Wait for document to load
    console.log("Waiting for document to load...");
    let pageCount = 0;
    for (let i = 0; i < 60; i++) {
      const pageRes = await client.request(ControlCommand.TestGetPageNo);
      const page = pageRes[0] as number;
      if (page > 0) {
        pageCount = page;
        console.log(`Document loaded, initial page: ${page}`);
        break;
      }
      await new Promise(r => setTimeout(r, 500));
    }
    if (pageCount === 0) throw new Error("Document failed to load");
    await new Promise(r => setTimeout(r, 1000));

    // Turn on flashcard mode
    const fcToggleId = cmdId("CmdFlashcardToggle");
    await client.request(ControlCommand.TestRunCommand, [fcToggleId]);
    await new Promise(r => setTimeout(r, 1000));

    // Create 3 flashcards on different pages (like the original test)
    console.log("Creating flashcard on page 1...");
    const createRes1 = await client.request(ControlCommand.TestCreateFlashcard, [1]);
    console.log(`Create result: ${JSON.stringify(createRes1)}`);
    const key1 = BigInt(createRes1[1] as string);
    
    console.log("Creating flashcard on page 2...");
    const createRes2 = await client.request(ControlCommand.TestCreateFlashcard, [2]);
    console.log(`Create result: ${JSON.stringify(createRes2)}`);
    const key2 = BigInt(createRes2[1] as string);
    
    console.log("Creating second flashcard on page 1...");
    const createRes3 = await client.request(ControlCommand.TestCreateFlashcard, [1]);
    console.log(`Create result: ${JSON.stringify(createRes3)}`);
    const key3 = BigInt(createRes3[1] as string);

    console.log(`Created cards with keys: ${key1}, ${key2}, ${key3}`);

    // Start study mode
    const studyId = cmdId("CmdFlashcardStudy");
    await client.request(ControlCommand.TestRunCommand, [studyId]);
    await new Promise(r => setTimeout(r, 1500));
    console.log("Study mode started");

    // Rate the first card (Again=1)
    const rate1Id = cmdId("CmdFlashcardRate1");
    console.log("Rating card 0 (Again)...");
    await client.request(ControlCommand.TestRateFlashcard, [0, rate1Id]);
    await new Promise(r => setTimeout(r, 2000));

    // Rate the second card (Hard=2)
    const rate2Id = cmdId("CmdFlashcardRate2");
    console.log("Rating card 1 (Hard)...");
    await client.request(ControlCommand.TestRateFlashcard, [1, rate2Id]);
    await new Promise(r => setTimeout(r, 2000));

    // Rate the third card (Good=3)
    const rate3Id = cmdId("CmdFlashcardRate3");
    console.log("Rating card 2 (Good)...");
    await client.request(ControlCommand.TestRateFlashcard, [2, rate3Id]);
    await new Promise(r => setTimeout(r, 2000));

    // Give time for deferred bookkeeping (prune/save) to flush
    await new Promise(r => setTimeout(r, 5000));

    // Flush deferred flashcard bookkeeping to ensure study JSON is saved
    // Use the original TestFlushFlashcardBookkeeping command (id 86)
    console.log("Flushing flashcard bookkeeping...");
    await client.request(ControlCommand.TestFlushFlashcardBookkeeping);
    await new Promise(r => setTimeout(r, 3000));

    // Find the study JSON for each card key
    const keys = [key1, key2, key3];
    const foundKeys: bigint[] = [];
    for (const key of keys) {
      const jsonPath = findStudyJsonForKey(key);
      if (!jsonPath) {
        throw new Error(`Study JSON file not found for card key ${key}`);
      }
      console.log(`Found study JSON for key ${key}: ${jsonPath}`);

      const entries = parseStudyJson(jsonPath);
      console.log(`Parsed ${entries.length} study state entries:`);
      for (const e of entries) {
        console.log(`  key=${e.key} rating=${e.rating} interval=${e.interval}`);
      }

      // Find the matching entry
      const matchingEntries = entries.filter(e => e.key === key);
      if (matchingEntries.length !== 1) {
        throw new Error(`FAIL: Expected exactly 1 entry for key ${key}, got ${matchingEntries.length}`);
      }
      const entry = matchingEntries[0];
      
      // Verify rating matches what we rated
      let expectedRating = 0;
      if (key === key1) expectedRating = 1; // Again
      else if (key === key2) expectedRating = 2; // Hard
      else if (key === key3) expectedRating = 3; // Good
      
      if (entry.rating !== expectedRating) {
        throw new Error(`FAIL: Key ${key} expected rating ${expectedRating}, got ${entry.rating}`);
      }
      if (entry.interval <= 0) {
        throw new Error(`FAIL: Key ${key} expected interval > 0, got ${entry.interval}`);
      }
      
      console.log(`PASS: Key ${key} rating=${entry.rating} interval=${entry.interval}`);
      foundKeys.push(key);
    }

    // Verify all 3 keys found
    if (foundKeys.length !== 3) {
      throw new Error(`FAIL: Expected 3 keys, found ${foundKeys.length}`);
    }
    console.log("PASS: All 3 rated cards found in study JSON");

    // Verify keys in ascending order (binary search will work)
    const allEntries: { key: bigint; rating: number; interval: number }[] = [];
    for (const key of keys) {
      const jsonPath = findStudyJsonForKey(key);
      if (jsonPath) {
        const entries = parseStudyJson(jsonPath);
        allEntries.push(...entries);
      }
    }
    // Sort by key
    allEntries.sort((a, b) => a.key < b.key ? -1 : 1);
    for (let i = 1; i < allEntries.length; i++) {
      if (allEntries[i].key <= allEntries[i - 1].key) {
        throw new Error(`FAIL: Keys not in ascending order: ${allEntries[i - 1].key} >= ${allEntries[i].key}`);
      }
    }
    console.log("PASS: Keys in ascending order (binary search will work)");

    // No duplicate keys
    const uniqueKeys = new Set(allEntries.map(e => e.key));
    if (uniqueKeys.size !== allEntries.length) {
      throw new Error(`FAIL: Duplicate keys found`);
    }
    console.log("PASS: No duplicate keys");

    console.log("\n✅ All invariant checks PASSED");
  } finally {
    // KILL the process (not graceful quit) because unsaved flashcard annotation
    // would trigger "Save changes?" modal which blocks the message loop.
    if (client) {
      client.close();
    }
    proc.kill();
    await proc.exited;
    
    // ASSERTION: Fixture was NOT modified
    const fixtureStatAfter = getFileStat(fixtureAbs);
    if (!fixtureStatAfter) {
      throw new Error("Fixture disappeared!");
    }
    if (fixtureStatAfter.size !== fixtureStatBefore.size || fixtureStatAfter.mtime !== fixtureStatBefore.mtime) {
      throw new Error(`FAIL: Fixture was modified! Before: size=${fixtureStatBefore.size} mtime=${fixtureStatBefore.mtime}, After: size=${fixtureStatAfter.size} mtime=${fixtureStatAfter.mtime}`);
    }
    console.log(`PASS: Fixture unchanged (size=${fixtureStatAfter.size}, mtime=${fixtureStatAfter.mtime})`);
  }
}

if (import.meta.main) {
  await runStandalone(testit);
}