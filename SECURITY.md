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

## Supported versions

Only the latest release (see [Releases](../../releases)) receives fixes.
