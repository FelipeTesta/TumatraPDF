/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

struct RenderedBitmap;
namespace Gdiplus {
class Bitmap;
}

// Hooks the app provides to the shared image editor. Every member is optional;
// each one describes what is lost by leaving it out.
struct ImageEditHost {
    // Decodes an image file. Without it the editor can only work on a bitmap it
    // was handed (a screenshot), not one loaded from disk.
    Gdiplus::Bitmap* (*LoadImageFile)(Str path) = nullptr;
    // Writes a single-page PDF. Without it PDF isn't offered as a destination
    // format, since nothing could produce one.
    bool (*SaveBitmapAsPdf)(Gdiplus::Bitmap* bmp, Str destPath) = nullptr;
    // Called with what was just written, for a host that can display it.
    void (*OpenSavedFile)(HWND parent, Str path) = nullptr;
    // Replace the PDF image the editor was opened from with the edited result
    // (TUMATRA writeback). engine is the opaque engine pointer the context
    // was set with; it no longer matches when the document was switched or
    // closed, and the hook must do nothing then.
    bool (*WriteBackToPdf)(HWND parent, Str path, int pageNo, int imageIdx, void* engine) = nullptr;
    // Translates a UI string; without it the English source string is used.
    Str (*Translate)(Str) = nullptr;
    // Applies the host's dark mode to a window the editor created.
    void (*ApplyDarkMode)(HWND) = nullptr;
    // Font for the editor's controls; without it the default GUI font is used.
    HFONT (*GetFont)(HWND) = nullptr;
    // Window the editor falls back to as a parent (used by the headless test).
    HWND (*GetOwnerHwnd)() = nullptr;
    // Icon resource id for the editor window; 0 leaves it without one.
    int appIconId = 0;
    // whether Esc closes the editor from its save mode
    bool escToExit = false;
};

extern ImageEditHost gImageEditHost;

void InitImageEditHost();

enum class ImageEditMode {
    Save,
    Crop,
    Resize
};

// parent is the window the editor is owned by and centred on. Either filePath
// names an image to load or rbmp holds one already rendered; with neither
// there is nothing to edit and the call does nothing. Returns the editor's
// window or null when nothing was opened.
HWND ShowImageEditWindow(HWND parent, ImageEditMode mode, Str filePath = {}, RenderedBitmap* rbmp = nullptr,
                         bool selectPdf = false);

// After ShowImageEditWindow returns a window that edits an image coming from
// a PDF page, the caller registers which image it is; Save then also replaces
// that image inside the document (TUMATRA writeback). engine is the opaque
// engine pointer the host hook validates against the still-open document.
void ImageEditSetWritebackContext(HWND editHwnd, int pageNo, int imageIdx, void* engine);

TempStr ImageResizeArrowKeyResultTemp(Str imagePath, int* exitCodeOut = nullptr);
