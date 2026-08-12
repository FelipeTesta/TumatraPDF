# TumatraPDF

PDF reader based on [SumatraPDF](https://github.com/sumatrapdfreader/sumatrapdf), an open source project (GPLv3) developed by [Krzysztof Kowalczyk](https://github.com/kjk) and contributors.

## Goal

Create an enhanced version of SumatraPDF with additional features focused on reading books and long documents, while maintaining compatibility with upstream updates.

## Credits

This project is a fork of **SumatraPDF** — a lightweight, fast, open source PDF reader for Windows.

- **Original repository:** [github.com/sumatrapdfreader/sumatrapdf](https://github.com/sumatrapdfreader/sumatrapdf)
- **Main author:** [Krzysztof Kowalczyk (kjk)](https://github.com/kjk)
- **License:** [GNU General Public License v3.0](https://github.com/sumatrapdfreader/sumatrapdf/blob/master/COPYING)
- **Official site:** [sumatrapdfreader.org](https://www.sumatrapdfreader.org)
- **Base version used:** Pre-release `3.7.20958`

We thank Krzysztof and all contributors for the excellent work on SumatraPDF.

## 🆕 TumatraPDF Exclusive Features

The features below were developed on top of SumatraPDF and **do not exist in the original version**. They follow the same project philosophy — native C/C++ code, Win32 interface, no external dependencies.

> **Note for Krzysztof Kowalczyk** (creator of SumatraPDF): these features were implemented as hooks in the original code, without modifying the SumatraPDF core. The `sumatrapdf-src/` directory contains the preserved upstream code. All extensions are in `src/features/`. Feel free to incorporate any of them into the official SumatraPDF.

---

### 1. ⏱️ AutoScroll with Timer and ETA

**What it does:** Continuous automatic document scrolling with speed control, scheduled stop timer, and estimated time remaining (ETA) to finish the book.

**How it works:**
- A high-precision timer (`SetTimer`) fires `MoveDocBy(0, dy)` on each tick, moving the document vertically
- Sub-pixel accumulator ensures smooth scrolling even at low speeds
- ETA calculation uses: `remaining pages × page height / current scroll speed`
- Scrolling pauses automatically when clicking on the page (for text selection)
- Stops at the end of the document (`IsAtDocumentEnd()`)

**Controls:**
| Shortcut | Action |
|---|---|
| `F10` | Start/Stop AutoScroll |
| `F7` | Increase speed (+20%) |
| `F8` | Decrease speed (-20%) |

**Toolbar interface:**
- Play/Pause toggle button (▶/⏸ icon)
- + and - buttons for speed adjustment
- Numeric field for timer (minutes, 0 = no limit)
- Label showing ETA (e.g.: "⏱ 12min remaining")

**Persistence:** Speed and timer saved in `TumatraPDF-settings.txt` (`AutoScrollSpeed`, `AutoScrollTimerMinutes`).

---

### 2. 🎨 Contrast Filter (Overlay)

**What it does:** Applies a semi-transparent black overlay over the document, simulating brightness/contrast reduction without modifying the original PDF.

**How it works:**
- An overlay window (`WS_EX_LAYERED | WS_EX_TRANSPARENT`) is created as a sibling of `hwndCanvas`
- The `WS_EX_TRANSPARENT` attribute ensures mouse clicks pass through the overlay — text selection and navigation continue to work normally
- Opacity is controlled via `SetLayeredWindowAttributes(hwnd, 0, alpha, LWA_ALPHA)`, where alpha = 255 × (transparency / 100)
- The overlay automatically resizes with the window (`WM_SIZE`)

**Controls:**
| Shortcut | Action |
|---|---|
| `Ctrl+Shift+C` | Enable/Disable contrast filter |

**Toolbar interface:**
- Toggle button (🌓/☀ icon)
- Slider/numeric input from 0% to 100% (0% = no overlay, 100% = full black)

**Persistence:** State and value saved (`ContrastEnabled`, `ContrastOpacity`).

---

### 3. 📖 Viewport Crop with Margin Trimming

**Status:** 🟡 In Development — Zoom correction needed

**What it does:** Automatically trims margins and displays two-column pages one column at a time, maximizing screen space and optimizing auto-scroll reading flow.

**How it works:**
- The viewport is cropped by user-defined margins (left, right, top, bottom, inter-column gap) in pixels
- In two-column mode, each column is displayed sequentially: left → right → next page left
- The zoom level adjusts like "fit-width" but constrained within the trimmed area
- A quick toggle (double-click or shortcut) temporarily shows the full page — useful for tables or images — and returns to crop view without losing settings or page position

**Configuration dialog:**
| Input | Unit | Description |
|---|---|---|
| Left margin | px | Crop from left edge |
| Right margin | px | Crop from right edge |
| Top margin | px | Crop from top edge |
| Bottom margin | px | Crop from bottom edge |
| Inter-column gap | px | Gap between two text columns |

**Quick toggle:** Temporarily switch to full-page "fit width" view and back without resetting crop settings or losing the current page position.

**Interface:**
| Access | Action |
|---|---|
| Menu: New Tools → Viewport Crop | Enable/disable |
| Toolbar button | Toggle on/off |
| Config button (gear icon) | Open margin settings dialog |
| Quick toggle | Temporarily show full page |

---

### 4. 🔄 Upstream Update System

**What it does:** PowerShell script to check for new SumatraPDF versions and generate merge instructions.

**How to use:**
```powershell
.\scripts\check-updates.ps1
```

The script compares the version in `sumatrapdf-src/src/Version.h` with the pre-release version available at `sumatrapdfreader.org/updatecheck-pre-release.txt`. If there is an update, it generates a step-by-step merge guide.

**Merge architecture:** See `MERGE.md` for full documentation of the update process.

---

### 5. 🎨 Own Visual Identity

- **Binary name:** `TumraPDF.exe` (allows side-by-side installation with original SumatraPDF)
- **Window:** Title "TumatraPDF" instead of "SumatraPDF"
- **Settings file:** `TumatraPDF-settings.txt` (does not conflict with `SumatraPDF-settings.txt`)
- **Icons:** Logos with inverted colors for visual distinction
- **Registry:** Own keys in Windows Registry (do not interfere with SumatraPDF installation)

---

### 📋 Technical Summary for Developers

| Feature | Main files | Hook point |
|---|---|---|
| AutoScroll | `src/features/autoscroll/`, `src/Canvas.cpp` | `OnTimer()`, `FrameOnCommand()` |
| Contrast Filter | `src/features/contrast/`, `src/Canvas.cpp` | `WM_CREATE` (overlay window) |
| Viewport Crop | `src/features/viewport/`, `src/DisplayModel.cpp` | `GetViewPort()`, `ScrollYBy()` |
| Update | `scripts/check-updates.ps1`, `MERGE.md` | N/A (external tool) |

**How to build:** See "Build" section below.

---

### 🗺️ Roadmap

- [x] Setup do projeto e build base
- [x] Rename binário + logos invertidos
- [x] AutoScroll + ETA + toolbar
- [x] Filtro de contraste + toolbar
- [x] Temas: Light+Dark apenas, Dark padrão
- [x] Menu "New Tools" com comandos TumatraPDF
- [x] Viewport crop + margin trim (zoom bug fixed)
- [ ] Testes automatizados para novas funcionalidades

---

## Modularity Strategy

To allow SumatraPDF upstream updates without breaking our features:

```
TumatraPDF/
├── sumatrapdf-src/        # Original SumatraPDF code (untouched)
├── src/                   # Our extensions and hooks
│   ├── features/          # Modular features
│   │   ├── autoscroll/    # Automatic scrolling
│   │   ├── viewport/      # Viewport cropping
│   │   └── contrast/      # Contrast filter
│   └── hooks/             # Upstream integration points
├── docs/                  # Documentation
├── FLOW/                  # Planning diagrams
├── README.md
├── LOG.md
└── MERGE.md               # Upstream merge instructions
```

**Principles:**
- Upstream code in `sumatrapdf-src/` is NEVER modified directly
- Features implemented in `src/features/` as isolated modules
- Integration via hooks/patching in the build system (Premake5)
- `MERGE.md` documents the upstream update process

## Update Check System

- Monitors SumatraPDF pre-release versions via `updatecheck-pre-release.txt`
- Automatic check on startup (configurable)
- Notifies when a new upstream version is available
- Script `scripts/check-updates.ps1` for manual checking
- Merge process documented in `MERGE.md`

## Architecture

Based on SumatraPDF:
- **Language:** C, C++
- **Build:** Premake5 → Visual Studio 2022
- **UI:** Native Win32 API
- **PDF engine:** MuPDF
- **Formats:** PDF, EPUB, MOBI, CBZ/CBR, FB2, CHM, XPS, DjVu
- **Native auto-update:** `src/UpdateCheck.cpp` (SumatraPDF)

## Status

🟢 Complete — All core features implemented (AutoScroll, Contrast Filter, Viewport Crop & Margin Trim, Dark Theme default, New Tools menu). Binary: `TumatraPDF.exe`.
