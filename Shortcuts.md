# TumatraPDF — Keyboard Shortcuts

Full key list. Fork features first; upstream SumatraPDF basics below. All
flashcard actions also work from the flashcard toolbar buttons.

## Flashcards (fork feature)

| Key                | Action                                                   |
| ------------------ | -------------------------------------------------------- |
| `S` (or `Shift+S`) | Create cloze flashcard from the current text selection   |
| `Space` / `Enter`  | Reveal the answer (study mode)                           |
| `1` (also numpad)  | Rate Again — fails the card, requeues it in this session |
| `2` (also numpad)  | Rate Hard                                                |
| `3` (also numpad)  | Rate Good                                                |
| `4` (also numpad)  | Rate Easy                                                |

Toolbar buttons (no default keys): study start/stop, Back (previous card,
arrives revealed), Next (skip without rating), Order, Filter, List, Config.
In dialogs: `Enter` = apply typed text, `Esc` / `X` = cancel/close.

## Viewport / reading (fork features)

| Key            | Action                  |
| -------------- | ----------------------- |
| `F9`           | Toggle autoscroll       |
| `F8`           | Autoscroll speed up     |
| `F7`           | Autoscroll speed down   |
| `Shift+I`      | Invert colors           |
| `Ctrl+Shift+C` | Toggle contrast overlay |

Scan Mode (ex Viewport Crop), Two Columns and Margin Trim are toggled from the
toolbar or the `New Tools` menu (no default keys).

## Basics (upstream SumatraPDF)

| Key                           | Action                        |
| ----------------------------- | ----------------------------- |
| `Ctrl+O`                      | Open document                 |
| `Ctrl+W` / `Ctrl+Shift+W`     | Close tab / close window      |
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | Next / previous tab           |
| `Ctrl+F` / `F3` / `Shift+F3`  | Find / next / previous result |
| `Ctrl+G`                      | Go to page                    |
| `+` / `-` / `0`               | Zoom in / out / fit           |
| `Ctrl+L`                      | Presentation mode             |
| `F11`                         | Fullscreen / windowed         |
| `F12`                         | Favorites                     |
| `Ctrl+P`                      | Print                         |
| `Ctrl+Q`                      | Quit                          |
| `←` / `→` / `↑` / `↓`         | Scroll / previous / next page |
| `Shift+←` / `Shift+→`         | Previous / next page          |

Full upstream list: `sumatrapdf-src/docs/md/Keyboard-shortcuts.md`.
