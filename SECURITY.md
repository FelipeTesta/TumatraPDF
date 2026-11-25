# Security

## Reporting a vulnerability

TumatraPDF is a fork of [SumatraPDF](https://github.com/sumatrapdfreader/sumatrapdf) (GPLv3, by Krzysztof Kowalczyk and contributors).

- **Bugs (including non-security crashes):** open an issue at
  https://github.com/FelipeTesta/TumatraPDF/issues — include crash dumps from
  `%LOCALAPPDATA%\SumatraPDF-data\<hash>\` (they stay local; nothing is uploaded).
- **Security issues affecting end users** (e.g. code execution from a crafted
  PDF): prefer a **private** report first — use GitHub private vulnerability
  reporting at https://github.com/FelipeTesta/TumatraPDF/security/advisories/new
  or open an issue marked clearly as security-sensitive.
- If the bug also exists in **upstream** SumatraPDF, please also report it at
  https://github.com/sumatrapdfreader/sumatrapdf/issues — the fork tracks
  upstream closely.

## Data collection stance

TumatraPDF makes exactly **one** network request on its own: the update check
(`raw.githubusercontent.com/FelipeTesta/TumatraPDF/main/update.txt`, daily by
default; disable via the `CheckForUpdates` advanced setting). There is **no**
crash upload, no analytics, no error reporting, no symbol download from any
server — all crash dumps stay in `%LOCALAPPDATA%` on your machine.

## Windows warnings and unsigned builds

The builds are not code-signed and there is no code-signing certificate. The source is public (GPLv3 fork of SumatraPDF) and every release publishes the SHA-256 of both assets in the release notes, so a download can be verified against those hashes.

The Windows warning comes from the Mark of the Web that browsers attach to downloaded files: SmartScreen shows "Windows protected your PC" and blocks the launch until you click "More info -> Run anyway". To proceed: click "More info" -> "Run anyway"; or right-click the file -> Properties -> tick "Unblock" at the bottom -> Apply; or in PowerShell `Unblock-File .\TumatraPDF-portable-win64.exe`. The downloaded installer carries the Mark of the Web (warns once when launched); the app it installs is extracted locally from the embedded payload and never carries the mark, so the installed app does not warn. The portable is the downloaded file itself — it keeps the mark and must be unblocked once ("More info -> Run anyway" / Properties -> Unblock); we removed the startup self-unblock because it is an ineffective SmartScreen workaround for an unsigned binary AND a classic AV false-positive trigger. Windows remembers the approval for that file.

This warning is an unsigned-build notice, not a malware verdict. If Defender itself quarantines the file (a Windows Security alert saying the file was removed), compare the file's SHA-256 with the one published in the release notes and report a mismatch in an issue — if the hashes match, the file is intact.

### Antivirus false positives on unsigned builds

If an antivirus flags or quarantines the portable or installer: compare the file's SHA-256 with the hashes published in the release notes. If the hashes match, the file is exactly what we published and the detection is a false positive. Add an exclusion for the file in your AV (per-AV steps vary) rather than disabling protection. If the detection persists, open an issue with the AV product name and the detection ID.

### Bitdefender ATC incident (2026-11)

In November 2026, Bitdefender's Advanced Threat Control (ATC) quarantined the published portable build. The RCA (`C:\ProgramData\Bitdefender\Bitdefender Security App\ctc\rca\423567944.dat`) reported:

- **Detection:** `ATC.Malicious` (class: Advanced Threat Control)
- **ATC detection ID:** `SuspiciousBehavior.54B279F15D24AA5B`
- **Actions taken:** kill + quarantine (action code 3)
- **MITRE techniques attributed:** T1553.005 (Mark-of-the-Web bypass), T1070.004 (file deletion), T1057 (process discovery), T1059.001 (command shell)

The triggers were **our own runtime behaviour**, not the binary contents:

1. **Runtime deletion of `Zone.Identifier` ADS** — five call sites stripped the Mark of the Web at startup. This does not stop SmartScreen (the mark is re-evaluated on each launch) and is a known AV false-positive trigger.
2. **Self-update via helper batch file** — the portable updater wrote a `.bat` into `%TEMP%` and executed it through `cmd.exe /c` (running `tasklist`, `move`, `start`, `del`). This shell activity tripped the ATC heuristics.

Both behaviours have been removed:

- All runtime Mark-of-the-Web deletion removed; the app now only reads the zone (read-only check) and never strips it.
- `SelfUpdateViaBatch` deleted. The portable self-update now launches the downloaded executable directly with `-sleep-ms 5000 -exit-when-done -update-self-to "<self>"`, and the relaunch stays inside `UpdateSelfTo`. The wait window increased from 7 s to ~35 s so large documents have time to close.

After the fixes, the new bytes ran 3× from a neutral folder (simulating a fresh download) with **0 quarantines, 0 WER events, and no new Bitdefender detection records**.

Upstream SumatraPDF also ships unsigned and documents false positives as common — see `sumatrapdf-src/docs/md/Is-there-a-virus-in-Sumatra-sources.md` (use VirusTotal; one or two engines reporting = likely false positive) and `sumatrapdf-src/docs/md/Failed-to-load-libmupdf.md` ("security policies blocking unsigned or newly written modules").

An unsigned build can still trigger heuristics on other engines. The guidance remains: compare SHA-256 with the release notes, add an AV exclusion rather than disabling protection, and report the AV product name + detection ID if it persists.

## Windows release signing

The project does not currently sign releases. The following options were researched:

| Option | Cost | Authenticode / SmartScreen benefit | Notes |
|--------|------|-----------------------------------|-------|
| SignPath Community (sponsored) | Free for open source | Yes — EXE signing, timestamping, AV scan, SLSA provenance, signed SBOM | Key stays in their HSM; policy/CI-driven. Requires program application (approval wait). First signed release planned as v1.0.23. |
| Sigstore / cosign keyless | Free | **No** — not Authenticode; gives verifiable provenance/attestation only | Quick to adopt; no SmartScreen or AV benefit. |
| Azure Artifact Signing | Paid (Basic 5,000 / Premium 100,000 sigs/mo) | Yes | Enterprise-grade. |
| Certum "Open Source Developer" OV cert | Individual OV cert | Yes | Terms not independently verified. |
| EV certificate | Most expensive | **Yes — only way to get instant SmartScreen reputation** | Highest trust tier. |

**Decision (owner):** signing is deferred for now; the roadmap is documented only. Two rules apply:

1. **Do not block v1.0.22 on the SignPath application** — apply in parallel; release proceeds unsigned.
2. **The updater must never require a signature until every published version is signed** — v1.0.21 is unsigned, so requiring it would lock users out of updating.

During SignPath onboarding, confirm which publisher name will appear on the binary.

## Build hygiene

Always produce the release binary with `bun cmd/build-release.ts` (clean Release build). A stale incremental build can produce an `.exe` whose settings layout disagrees with `libsumatrapdf.dll` and crash at startup with `0xC0000409` / `BEX64`. The Debug script (`bun cmd/build.ts`) stays incremental, so after changing `Settings.h` or any settings-struct layout, a clean build is mandatory.

## Supported versions

Only the latest release (see [Releases](../../releases)) receives fixes.
