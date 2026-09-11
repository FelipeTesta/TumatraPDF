/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

#ifndef Commands_View_h
#define Commands_View_h

struct MainWindow;
struct DocController;
struct Point;
struct Rect;
enum class DisplayMode;

// Forward declarations for static functions in SumatraPDF.cpp used by view handlers
void ChangeZoomLevel(MainWindow* win, float newZoom, bool pagesContinuously);
void SmartZoom(MainWindow* win, float factor, Point* pt, bool smartZoom);
void OnMenuZoom(MainWindow* win, int menuId);
void OnMenuCustomZoom(MainWindow* win);
void ZoomToSelection(MainWindow* win);
void SwitchToDisplayMode(MainWindow* win, DisplayMode displayMode, bool keepContinuous);
void ShowViewModeNotification(MainWindow* win, int cmdId);
bool IsContinuous(DisplayMode mode);
void ToggleContinuousView(MainWindow* win);
void ToggleMangaMode(MainWindow* win);
void TogglePresentationMode(MainWindow* win);
void ToggleFullScreen(MainWindow* win, bool presentation);
Point HwndGetCursorPos(HWND hwnd);

// View command wrappers in SumatraPDF.cpp (avoid DisplayModel.h/WindowTab.h in Commands_View.cpp)
void RotateDocument(MainWindow* win, int degrees);
void ToggleMangaModeInternal(MainWindow* win);

// View/Zoom/Rotate command handlers (extracted from SumatraPDF.cpp)
void HandleCmdZoomFitWidthAndContinuous(MainWindow* win);
void HandleCmdZoomFitPageAndSinglePage(MainWindow* win);
void HandleCmdZoomOut(MainWindow* win);
void HandleCmdZoomIn(MainWindow* win);
void HandleCmdZoom6400(MainWindow* win);
void HandleCmdZoom3200(MainWindow* win);
void HandleCmdZoom1600(MainWindow* win);
void HandleCmdZoom800(MainWindow* win);
void HandleCmdZoom400(MainWindow* win);
void HandleCmdZoom200(MainWindow* win);
void HandleCmdZoom150(MainWindow* win);
void HandleCmdZoom100(MainWindow* win);
void HandleCmdZoom75(MainWindow* win);
void HandleCmdZoom50(MainWindow* win);
void HandleCmdZoom25(MainWindow* win);
void HandleCmdZoom12_5(MainWindow* win);
void HandleCmdZoom8_33(MainWindow* win);
void HandleCmdZoomFitPage(MainWindow* win);
void HandleCmdZoomFitWidth(MainWindow* win);
void HandleCmdZoomFitHeight(MainWindow* win);
void HandleCmdZoomFitByOrientation(MainWindow* win);
void HandleCmdZoomFitContent(MainWindow* win);
void HandleCmdZoomShrinkToFit(MainWindow* win);
void HandleCmdZoomActualSize(MainWindow* win);
void HandleCmdZoomCustom(MainWindow* win);
void HandleCmdZoomToSelection(MainWindow* win);
void HandleCmdSinglePageView(MainWindow* win);
void HandleCmdFacingView(MainWindow* win);
void HandleCmdBookView(MainWindow* win);
void HandleCmdToggleContinuousView(MainWindow* win);
void HandleCmdToggleMangaMode(MainWindow* win);
void HandleCmdTogglePresentationMode(MainWindow* win);
void HandleCmdToggleFullscreen(MainWindow* win);
void HandleCmdRotateLeft(MainWindow* win);
void HandleCmdRotateRight(MainWindow* win);

#endif