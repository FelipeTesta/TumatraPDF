# TumatraPDF — Development Log

Format basé sur [Keep a Changelog](https://keepachangelog.com/fr/1.1.0/) avec
catégories : Added, Changed, Fixed, Removed, Decisions, Notes.

---

## [Unreleased]

Travail en cours sur la branche `feature/flashcard-system`.

### Added

- Flashcard Sidebar — panneau flottant avec TreeView listant les cards
- Page range filter — filtrage des cards par plage de pages
- Structured logging [FC] — 36+ appels logf dans 4 fichiers flashcard
- `CmdFlashcardFilter` — nouveau ID via gen-commands.ts
- Flashcard button on main toolbar (between Trim Config and Arch Tools)
- Card count display in flashcard toolbar (Cards: N/N/N | total/new/due)
- `ToolbarApplyThemeToRebar()` helper — applies rebar theme (subclass, dark mode, background)
- PowerShell `&&` documentation in both AGENTS.md files
- `CmdArchCleanAll` — single button clears measurements + resets scale + resets tool mode
- Arch Tools: per-page scale and measurements (independent per document page, not per-document)
- `ArchScaleState` struct + `scaleStates` Vec in ArchToolsState — per-page scale storage
- `CmdGoToPageNextKeepScroll` / `CmdGoToPagePrevKeepScroll` — Ctrl+Right/Left page nav preserving vertical scroll position

### Changed

- Card creation: FreeText → Highlight annotation (cloze system)
- `CmdFlashcardAdd`: creates Highlight overlay for text covering
- Flashcard counter auto-updates after toggle/add/rate
- Measure button in Arch Tools toolbar now uses `BTNS_CHECK` style for proper on/off highlight
- `SetToolbarButtonCheckedState` extended to search both main toolbar and Arch Tools toolbar2
- Arch Tools scale/measure persistence upgraded to per-page format with legacy fallback

### Fixed

- Build errors: `_TR` → raw `L""` strings, `engine->doc` → `EngineMupdfSaveUpdated()`
- `FlashcardToolbarUpdateCount` using `SetWindowTextW` (not `SendMessageT`)
- Flashcard toolbar rebar integration with RelayoutFrame
- `#include "DisplayModel.h"` in Commands_ArchTools.cpp caused 49 cascade errors — replaced with lightweight `#include "DocController.h"`

### Decisions

- Cards stored INSIDE PDF as Highlight annotations (cloze system, portables)
- Cloze = highlight cinza atrás do texto; study mode = retângulo opaco por cima
- Study state in external JSON `%APPDATA%\SumatraPDF\FlashcardStudy\<MD5>.json`

### Known Issues (TODO)

- BUG-FC1: LoadFromDocument only loads FREE_TEXT, cards created as Highlight are LOST on reload
- BUG-FC2: Card count label floating (not in toolbar band)
- BUG-FC3: Flashcard toolbar not respecting theme colors
- BUG-FC4: Text selection displaced with trim=on (Selection.cpp += → -=)
- Sidebar comme popup flottante (évite de modifier SetSidebarVisibility)
- Toolbar secondaire suit le pattern Arch Tools (rebar + toolbar)

### Notes

- BUG-3 (right-click image context menu) — deferido até v2
- Label/Tip system — v2
- Image Occlusion — v2

---

## [2026-09-11] — Flashcard MVP + Bug Fixes

### Added

- Flashcard system complet pour étude par répétition espacée (Anki-like)
- 12 commandes : Toggle, Study, Add, Back, Next, Reveal, Rate1-4, Lista, Filter
- Flashcard.h/cpp — data model, LoadFromDocument, SM-2 algorithm, JSON persistence
- FlashcardToolbar.cpp — barre secondaire Study/Back/Lista/Filter
- FlashcardSidebar.cpp — panneau flottant TreeView, clic navigue vers card
- Canvas.cpp — rendu highlight flashcard (gris subtil lecture / gris solide study)
- SelectionToolbar bouton Flashcard + raccourci `S`
- `FlashcardState` struct dans MainWindow.h

### Fixed

- **BUG-1** — ContrastOverlay WM_NCHITTEST→HTTRANSPARENT restaure copie clipboard images (Invert=on)
- **BUG-2** — Selection.cpp : offset trim.top dans SelectionOnPage::GetRect() corrige décalage sélection texte (Trim=on)

### Changed

- 12 nouveaux command IDs via gen-commands.ts (Commands.h/cpp régénérés)

### Decisions

- Cards comme annotations PDF = portables (partage PDF → cards incluses)
- SM-2 lite : rating 1=Again→interval=0, 2=Hard→×1.2, 3=Good×ease, 4=Easy→×ease×1.3
- PCH convention : headers ne doivent JAMAIS inclure base/Base.h (double-inclusion)

### Notes

- AGENTS.md créé à la racine avec conventions spécifiques TumatraPDF
- Build : `bun cmd/build-test.ts` → 0 errors, 0-1 warnings

---

## [2026-09-10] — ToolbarLayout + Autoscroll Persistence

### Added

- ToolbarLayout intégré — LayoutToolbarChildWindows comme point d'entrée unique
- autoscrollSpeedMultiplier persisté dans GlobalPrefs (cross-session)

### Changed

- Supprimé RepositionEtaLabel, RepositionSpeedLabel, RepositionTimerControls (redondants)
- HandleToolbarOverflow implémenté (masquage prioritaire : edit → label)

### Decisions

- LayoutToolbarChildWindows est le SEUL point d'entrée pour positionnement enfants toolbar

---

## [2026-09-09] — Autoscroll Round Steps + Toolbar Wrapping

### Added

- Autoscroll round-step lookup table : {25, 50, 75, 100, 150, 200, 300, 400, 600, 800, 1200, 1600} px/min
- Toolbar wrapping TBSTYLE_WRAPABLE — boutons s'empilent sur fenêtre étroite

### Changed

- Autoscroll utilise index de step au lieu de progression géométrique
- Bouton Crop renommé "Two Column"
- Timer beep + ∴ overlay orange 3s à l'expiration autoscroll

### Decisions

- Table de lookup remplace progression géométrique (nombres ronds, plus intuitifs)
- TBSTYLE_WRAPABLE + RelayoutFrame hauteur rebar dynamique = auto-adaptation

---

## [2026-09-08] — Design System + 16C-F6 Modularization

### Added

- Design system : centrage texte boutons (NM_CUSTOMDRAW) et inputs (ES_CENTER)
- 16C-F6 Phase A+B : extraction ~380 lignes de SumatraPDF.cpp
- MainWindowCreate.h/cpp — 11 fonctions déplacées
- SelectionState struct — 15 membres plats regroupés dans struct

### Changed

- SumatraPDF.cpp : 14,212 → ~13,830 lignes
- 4 statics promus à linkage externe (UpdateWindowRtlLayout, IsMenubarVisible, etc.)

### Decisions

- SelectionState struct suivi le pattern AutoScrollState/ArchToolsState
- Commands.h n'a PAS de include guard — ne jamais inclure depuis un header
