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

## Supported versions

Only the latest release (see [Releases](../../releases)) receives fixes.
