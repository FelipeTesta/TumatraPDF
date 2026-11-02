# NEXT STEPS — Detailed Action Plan (2026-11-01)

Based on FLOW analysis (tree-sitter + Serena) and critical findings in TODO.md.

---

## 🔴 PRIORITY 1 — BLOCKERS (Fix This Week)

### 1. ~~Fix ASan Build~~ — ALREADY FIXED (no action)
**File**: `sumatrapdf-src/cmd/build-asan.ts`
Verified 2026-11-01: `cmd/build-asan.ts` lines 72-73 already use `vs2022\TumatraPDF.sln` + `/t:TumatraPDF`.

---

### 2. Settings Migration — One-Time Fix for `FlashcardStudyTimeSec`
**Files**: 
- `sumatrapdf-src/cmd/gen-settings.ts` (add missing field at END)
- `sumatrapdf-src/src/Settings.cpp` (migration logic)
- `sumatrapdf-src/src/Settings.h` (generated — DO NOT hand-edit)

**Problem**: `gFileStateInfo` names blob missing `FlashcardStudyTimeSec` → fields ≥24 shifted by one
**Current state**: Round-trip works (index-consistent) but names lie; pre-s36-at files read corrupted

**Fix Steps**:
1. Add `FlashcardStudyTimeSec` at END of FileState table in `gen-settings.ts`
2. Run `bun cmd/gen-code.ts` → regenerates `Settings.h/.cpp`
3. Add migration in `Settings.cpp`:
   ```cpp
   void MigrateFileState(FileState* fs) {
       if (fs->version < 37) {
           // Shift fields 24+ back by one position
           // fs->flashcardStudyTimeSec = fs->field_that_was_shifted;
       }
   }
   ```
4. Call migration in `LoadSettings()` before deserializing FileState

**Verify**: Load pre-s36-at settings file → check AdvancedSettingsDialog shows correct names

---

### 3. WebView2 Adapter Pattern — Document & Enforce
**File**: `sumatrapdf-src/src/MarkdownModel.cpp:179` (`MarkdownHtmlWindowHandler`)

**Problem**: Every new `HtmlWindowCallback` method MUST be forwarded in adapter, or feature silently dies in Markdown

**Action**:
1. Create `docs/md/WebView2-Adapter-Pattern.md` documenting:
   - Adapter location and purpose
   - Required methods to forward (list current: `appKey`, `autoscrollProgress`, `autoscrollBottom`, `scrollPos`, `linkClick`, `imageClick`)
   - Pattern: `virtual void OnXxx() { callback->OnXxx(); }`
   - Test: Add `TestMarkdownAdapter` to `cmd/control.ts`
2. Add compile-time check: static_assert or unit test that verifies all `HtmlWindowCallback` methods have adapter impl

**Current missing**: Any future feature adding JS bridge needs adapter update

---

## 🟠 PRIORITY 2 — SYSTEMIC FIXES (This Sprint)

### 4. Coordinate Space Audit — Eliminate Silent Bugs
**Tool**: `tree-sitter` + grep
**Target**: All `engine->` calls in render path

**Command**:
```bash
# Find all engine calls in render-related files
grep -rn "engine->" sumatrapdf-src/src/RenderCache.cpp sumatrapdf-src/src/DisplayModel.cpp sumatrapdf-src/src/Canvas.cpp sumatrapdf-src/src/Commands_Edit.cpp sumatrapdf-src/src/TrimConfigDialog.cpp
```

**Check each call**:
- `engine->PageMediaBox(pageNo)` → must be `dm->PageMediaBox(virtualPageNo)`
- `engine->Transform(pageNo, ...)` → must route virtual→physical
- `engine->HasClipOptimizations(pageNo)` → must route
- `engine->RenderPage(pageNo, ...)` → must route
- `engine->GetPageImage(pageNo)` → must route

**Fix pattern**:
```cpp
// WRONG:
engine->PageMediaBox(pageNo);

// CORRECT:
int physPage = dm->VirtualToPhysical(pageNo);
engine->PageMediaBox(physPage);
```

**Files known to need fix** (from LOG.md):
- `TrimConfigDialog.cpp:92,113` — FIXED in R1 but verify
- `RenderCache.cpp:GetTileRes` — FIXED but verify all paths
- Non-cache `Paint` path in `RenderCache.cpp`

---

### 5. Toolbar Critical Ordering — Verify All 4 Toolbars
**Files**:
- `sumatrapdf-src/src/Toolbar.cpp` — `CreateToolbar2()` (Arch), main toolbar
- `sumatrapdf-src/src/Commands_Edit.cpp` — `EditToolbarCreate()`
- `sumatrapdf-src/src/FlashcardToolbar.cpp` — `FlashcardToolbarCreate()` (reference)

**Pattern to verify** (Flashcard = gold standard):
```cpp
// 1. ToolbarSetupSecondaryRebar(hwndRebar)  // subclass + RB_SETBARINFO + RB_SETBKCOLOR
// 2. Create toolbar window
// 3. SetWindowTheme(hwndToolbar, L"", L"")  // STRIP BEFORE buttons
// 4. TbAddButtons(iBitmap = TbIcon::Text (-2))
// 5. TbSetBitmapSize / TbSetButtonSize
// 6. TbGetItemRect
// 7. ShowWindow(hwndToolbar, SW_SHOW)
// 8. RB_INSERTBAND
// 9. SetWindowPos(rebar, frameWidth)
// 10. ToolbarSubclassPaint(hwndToolbar) (+ PostAddTextButtonSizing after TbAddButtons AND after re-wrap)  // secondary toolbars — WndProcToolbar BANNED here (s36-be stack-overflow fix; ToolbarSubclassSecondaryToolbar DELETED)
```

**Check each toolbar**:
- [ ] Main toolbar (`CreateToolbar`)
- [ ] Arch Tools (`CreateToolbar2`)
- [ ] Flashcard (`FlashcardToolbarCreate`) ✓
- [ ] Edit (`EditToolbarCreate`)

NOTE (s36-bo audit): step 10 previously referenced ToolbarSubclassSecondaryToolbar — deleted in s36-be crash fix; current ordering per FLOW/toolbar-system.dot is authoritative.

---

### 6. ReloadDocument — Automate Fork Field Preservation
**File**: `sumatrapdf-src/src/SumatraPDF.cpp` — `ReloadDocument()` and `UpdateFileStateForTab()`

**Problem**: Manual sync — every new per-doc field must be added to BOTH functions

**STATUS**: Verified 2026-11-01: field loss risk is LOW today — ReadingStats/FlashcardStats write reading/study seconds in-place on gFileHistory (ReadingStats.cpp:124 / FlashcardStats.cpp:172), ReloadDocument does NOT replace the gFileHistory entry (temp FileState created+deleted; SumatraPDF.cpp:2681/2746). Function referenced as UpdateFileStateForTab actually = UpdateTabFileDisplayStateForTab (SumatraPDF.cpp:929). Keep FORK_FIELD automation as tech-debt item.

**Fix**: Create a single source of truth:
```cpp
// In FileState struct, mark fork fields with attribute or macro
struct FileState {
    // ...
    FORK_FIELD(bool, invertColors);
    FORK_FIELD(bool, contrastEnabled);
    FORK_FIELD(int, contrastOpacity);
    FORK_FIELD(bool, marginTrimEnabled);
    FORK_FIELD(int, trimTop);
    FORK_FIELD(int, trimBottom);
    FORK_FIELD(bool, viewportCropV2Enabled);
    FORK_FIELD(float, autoScrollSpeedMultiplier);
    // ...
};

// Generate save/restore code from single definition
// Or use a static array of field pointers for iteration
```

**Immediate**: Add static assertion or test that verifies all `FORK_FIELD` are saved/restored

---

## 🟡 PRIORITY 3 — TECH DEBT (Next Sprint)

### 7. TrimConfigDialog — Virtual→Physical Routing (R1)
**File**: `sumatrapdf-src/src/TrimConfigDialog.cpp:92,113`
**Status**: LOG.md says FIXED (now uses `dm->PageMediaBox()`) but verify:
```cpp
// OnEditTopChanged / OnEditBottomChanged:
Rect mb = dm->PageMediaBox();  // NOT engine->PageMediaBox(CurrentPageNo())
```

---

### 8. R3-Left — Unify TrimRect Math
**Files**: `DisplayModel.cpp`, `RenderCache.cpp`, `Canvas.cpp`, `TrimConfigDialog.cpp`
**Problem**: 3 copies of trim rect math with different semantics
**Decision**: Only unify if rendering regression test exists
**Action**: Write test first (`tests/issue-trim-rect-unification.ts`), then refactor

---

### 9. R5-Left — Remove V1 Crop Stack
**Files**: `DisplayModel.h` (viewportCrop v1 fields), `DisplayModel.cpp` (`QuickToggleViewportCrop()`)
**Blocked**: V1 button (Scan View) still live
**Action**: Deprecate V1 button → remove when unused

---

### 10. R7/16C — God File Extraction
**Target**: `SumatraPDF.cpp` (~14.2k lines → target ~10k)
**Approach**: Extract by feature domain:
- `Commands_File.cpp` → already done
- `Commands_ArchTools.cpp` → already done  
- `Commands_Flashcard.cpp` → already done
- `Commands_Edit.cpp` → already done
- Remaining: UI layout, DDE, thumbnail, window management, settings sync

**Next extraction candidates**:
- `WindowManagement.cpp` — `RelayoutFrame`, `CreateAndShowMainWindow`, tab management
- `DdeHandler.cpp` — `HandleOpenCmd`, `gIsStartup` queue
- `ThumbnailManager.cpp` — async thumbnail generation

---

## 🟢 PRIORITY 4 — QUALITY & TOOLING

### 11. Clang-Tidy — Clear Remaining 8 Diagnostic Errors
**File**: `out/lint/clang-tidy-summary.txt`
**Action**: Fix the 8 `clang-diagnostic-error` TUs (flag/std gap vs MSVC)

### 12. Bun Upgrade — `1.3.14 → 1.4.3`
**Files**: `package.json`, all `cmd/*.ts`, `tests/*.ts`
**Blocker**: Re-run full test suite first

### 13. Test Infrastructure — Add Regression Tests
**Missing tests**:
- Coordinate space mixing (TC2 + Trim + Autoscroll)
- Settings migration (pre-s36-at → post-s36-at)
- WebView2 adapter completeness
- ReloadDocument fork field preservation
- Toolbar critical ordering (all 4 toolbars)

---

## 🎨 PRIORITY 2.5 — THEME COLORS COMPLIANCE (This Sprint)

### 14. Fix Non-Compliant Dialogs/Windows — Dark Mode White Controls

**Problem**: Several dialogs use `dmlib::setDarkWndSafe()` for the dialog frame but **don't handle `WM_CTLCOLOR`** for child controls (edit, static, listbox, buttons). Result: dialog frame is dark but controls paint white in dark mode.

**Reference Implementation** (ArchScaleDialog.cpp:278-288):
```cpp
LRESULT ArchScaleDialogWnd::OnMessageReflect(UINT msg, WPARAM wparam, LPARAM) {
    if (msg == WM_CTLCOLORDLG || msg == WM_CTLCOLORSTATIC) {
        HDC hdc = (HDC)wparam;
        COLORREF bgColor = ThemeWindowControlBackgroundColor();
        COLORREF textColor = ThemeWindowTextColor();
        SetBkColor(hdc, bgColor);
        SetTextColor(hdc, textColor);
        return (LRESULT)BackgroundBrush();
    }
    return 0;
}
```

**Must Fix (Priority Order)**:

| Priority | Dialog/Window | File | Fix Pattern |
|----------|---------------|------|-------------|
| 1 | **Password Dialog** | `SumatraDialogs.cpp:178` (`Dialog_GetPassword_Proc`) | Add `WM_CTLCOLOREDIT/STATIC/BTN` handling |
| 2 | **Go to Page Dialog** | `SumatraDialogs.cpp:282` (`Dialog_GoToPage_Proc`) | Add `WM_CTLCOLOREDIT/STATIC` handling |
| 3 | **Find Window** | `FindWindow.cpp` | Add `WM_CTLCOLOREDIT/STATIC/LISTBOX` handling |
| 4 | **Find Bar** | `FindBar.cpp` | Add `WM_CTLCOLOREDIT` handling |
| 5 | **Advanced Settings Dialog** | `AdvancedSettingsDialog.cpp` | Verify wingui controls use theme colors |
| 6 | **Trim Config Dialog** | `TrimConfigDialog.cpp` | Verify wingui Edit controls use theme colors |
| 7 | **Print/Page Setup Dialogs** | `PrintDialog.cpp` | `dmlib::setDarkWndSafe` + `WM_CTLCOLOR` |

**Theme Colors API** (from `Theme.h`):
- `ThemeWindowControlBackgroundColor()` — dialog/control background
- `ThemeWindowTextColor()` — text color
- `ThemeHighlightColor()` — selection highlight
- `BackgroundBrush()` — cached brush for `WM_CTLCOLORDLG`

**Verify**: Launch in dark mode, open each dialog, verify all controls (edit, static, buttons, listbox) have dark background + readable text.

---

## 📋 EXECUTION ORDER

| Week | Tasks |
|------|-------|
| **Week 1** | 1. Fix ASan build, 2. Settings migration, 3. WebView2 adapter doc |
| **Week 2** | 4. Coordinate space audit, 5. Toolbar ordering verify, 6. ReloadDocument automation |
| **Week 3** | 7. TrimConfigDialog verify, 8. R3 test + refactor, 9. R5 deprecation plan |
| **Week 4** | 10. God file extraction (WindowManagement), 11. Clang-tidy, 12. Bun upgrade prep |
| **Ongoing** | 13. Add regression tests for each fix |

---

## 🔗 CROSS-REFERENCES

| Finding | FLOW Diagram | LOG Entry | Code Location |
|---------|-------------|-----------|---------------|
| Toolbar ordering | `toolbar-system.dot`, `toolbar-critical-ordering.dot` | s36-ax | `Toolbar.cpp:2893/3062/3191` |
| Coordinate spaces | `render-pipeline.dot`, `two-columns-v2.dot` | AGENTS.md §12 | `DisplayModel.cpp:2236` |
| Settings migration | `settings-system.dot`, `document-load-flow.dot` | s36-at/az | `Settings.cpp`, `gen-settings.ts` |
| WebView2 adapter | `webview-markdown.dot` | s36-az (R2) | `MarkdownModel.cpp:179` |
| TC2 routing | `two-columns-v2.dot`, `render-pipeline.dot` | s36-am2 | `RenderCache.cpp`, `DisplayModel.cpp` |
| ReloadDocument | `document-load-flow.dot` | s36-az | `SumatraPDF.cpp:2612/870` |

---

## ✅ DONE (No Action Needed)

- [x] Flashcard toolbar pattern (reference for others)
- [x] Arch/Edit toolbar crash fix (s36-ax)
- [x] TC2 render cache re-enabled (LOG.md:2161)
- [x] Contrast single source (R2, MarkdownContrastJs)
- [x] Trim cache integration (ShouldCacheRendering=true)
- [x] Dark mode epoch invalidation
- [x] Per-thread MuPDF contexts
- [x] Predictive rendering LIFO queue

---

**Next Review**: 2026-11-08 (1 week) — verify Priority 1 complete