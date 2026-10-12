/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

#ifndef Commands_Edit_h
#define Commands_Edit_h

struct MainWindow;
struct DisplayModel;
struct RectF;

// Handle edit-mode commands (Edit toggle, +Text, +Img). Called from the main
// command dispatch. Returns true if the command was handled.
bool HandleCommandEdit(MainWindow* win, int cmd);

// Place the armed (pending) clipboard image at canvas position (x, y) as a
// Stamp annotation. Returns true if the click was consumed by edit mode.
bool EditToolsPlacePendingImage(MainWindow* win, int x, int y);

// Free the pending clipboard image (if any) and disarm placement.
void EditToolsClearPendingImage(MainWindow* win);

// --- in-page image bbox editing (edit mode) ---
// Left-click routing: start a handle/inside drag on the selected bbox (true
// if consumed), else select the image under (x, y) (true if one was found).
bool EditToolsImageMouseDown(MainWindow* win, int x, int y);

// Live drag update (mouse move while a bbox drag is active).
void EditToolsImageDragMove(MainWindow* win, int x, int y);

// Finish the bbox drag (mouse up).
void EditToolsImageDragEnd(MainWindow* win);

// 0 = no hover, 1 = inside (move), 2..9 = edge/handle id (for OnSetCursor).
int EditToolsImageHoverTest(MainWindow* win, int x, int y);

// Draw the bbox overlays + handles for pending image edits into hdc
// (viewport coords; call from OnPaintDocument's edit branch).
void EditToolsPaintImageOverlays(MainWindow* win, DisplayModel* dm, HDC hdc);

// Drop all pending image edits + selection (mode off / doc change).
void EditToolsClearImageEdits(MainWindow* win);

// Select the image identified by (pageNo, imageIdx) at pageRect, from the
// context-menu Crop/Resize path (activates in-page editing).
void EditToolsSelectImageFromMenu(MainWindow* win, int pageNo, int imageIdx, RectF imageRect, bool cropMode);

#endif
