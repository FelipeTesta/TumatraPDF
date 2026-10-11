/* Copyright 2026 the TumatraPDF project authors. All rights reserved.
   License: GPLv3 */

// Replace / externally-edit embedded PDF images (right-click "Image" menu),
// extracted from SumatraPDF.cpp. "Edit With Paint" hands the extracted image
// to mspaint via a temp png and polls the file's mtime; saving the png in
// mspaint swaps the image back into the PDF (persist via File > Save).

#include "base/Base.h"
#include "base/Win.h"
#include <shobjidl.h>

#include "Settings.h"
#include "DocController.h"
#include "DocProperties.h"
#include "TreeModel.h"
#include "Annotation.h"
#include "EngineBase.h"
extern "C" {
#include <mupdf/pdf.h>
}
#include "EngineMupdf.h"
#include "DisplayModel.h"
#include "RenderCache.h"
#include "SumatraPDF.h"
#include "MainWindow.h"
#include "WindowTab.h"
#include "Commands_Image.h"

static constexpr int kPaintWatchDelayMs = 1200;

// watch state for "Edit With Paint": one in-flight external edit at a time
static MainWindow* gPaintWatchWin = nullptr;
static EngineMupdf* gPaintWatchEngine = nullptr;
static int gPaintWatchPageNo = -1;
static int gPaintWatchImageIdx = -1;
static WCHAR gPaintWatchTmpPath[MAX_PATH]{};
static FILETIME gPaintWatchMtime{};

static void StopPaintImageWatch() {
    if (gPaintWatchWin) {
        KillTimer(gPaintWatchWin->hwndFrame, kPaintWatchTimerId);
    }
    gPaintWatchWin = nullptr;
    gPaintWatchEngine = nullptr;
    gPaintWatchPageNo = -1;
    gPaintWatchImageIdx = -1;
    gPaintWatchTmpPath[0] = 0;
}

// very small GDI+ PNG writer (lookup the png encoder clsid once per call)
static bool SaveHbitmapAsPng(HBITMAP hbmp, WStr destPath) {
    UINT num = 0;
    UINT size = 0;
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) {
        return false;
    }
    char* buf = (char*)malloc(size);
    Gdiplus::ImageCodecInfo* codecs = (Gdiplus::ImageCodecInfo*)buf;
    Gdiplus::GetImageEncoders(num, size, codecs);
    CLSID pngClsid{};
    bool found = false;
    for (UINT i = 0; i < num; i++) {
        if (0 == lstrcmpW(codecs[i].MimeType, L"image/png")) {
            pngClsid = codecs[i].Clsid;
            found = true;
            break;
        }
    }
    free(buf);
    if (!found) {
        return false;
    }
    Gdiplus::Bitmap bmp(hbmp, nullptr);
    Gdiplus::Status st = bmp.Save(destPath.s, &pngClsid, nullptr);
    return st == Gdiplus::Ok;
}

// modern open-file picker restricted to image formats (unicode-safe)
static Str PickImageFileDup(HWND owner) {
    Str res{};
    static COMDLG_FILTERSPEC kImgFilters[] = {{L"Images", L"*.png;*.jpg;*.jpeg;*.bmp;*.gif;*.webp"},
                                              {L"All files", L"*.*"}};
    IFileOpenDialog* dlg = nullptr;
    HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_IFileOpenDialog, (void**)&dlg);
    if (FAILED(hr) || !dlg) {
        return res;
    }
    dlg->SetFileTypes(2, kImgFilters);
    dlg->SetTitle(L"Select an image to insert into the PDF");
    DWORD flags = 0;
    dlg->GetOptions(&flags);
    dlg->SetOptions(flags | FOS_FILEMUSTEXIST | FOS_FORCEFILESYSTEM);
    hr = dlg->Show(owner);
    if (SUCCEEDED(hr)) {
        IShellItem* item = nullptr;
        dlg->GetResult(&item);
        if (item) {
            WCHAR* wpath = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &wpath)) && wpath) {
                res = str::Dup(ToUtf8Temp(WStr(wpath)));
                CoTaskMemFree(wpath);
            }
            item->Release();
        }
    }
    dlg->Release();
    return res;
}

void HandleCmdReplaceImage(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    EngineMupdf* emup = nullptr;
    if (dm) {
        emup = AsEngineMupdf(dm->GetEngine());
    }
    IPageElement* pageEl = nullptr;
    if (dm) {
        pageEl = dm->GetElementAtPos(win->contextMenuPt, nullptr);
    }
    if (!emup || !pageEl || !pageEl->Is(kindPageElementImage)) {
        logf("[imgedit] ReplaceImage: no image under the right-click point\n");
        return;
    }
    Str picked = PickImageFileDup(win->hwndFrame);
    if (!picked.s) {
        logf("[imgedit] ReplaceImage: picker cancelled\n");
        return;
    }
    TempStr confirmMsg =
        fmt("Replace this image with the picked file?\nSave the PDF afterwards to keep the change.\n\n%s", picked);
    int choice = MessageBoxW(win->hwndFrame, CWStrTemp(Str(confirmMsg)), CWStrTemp(StrL("Replace Image")),
                             MB_OKCANCEL | MB_ICONQUESTION);
    if (choice != IDOK) {
        str::Free(picked);
        return;
    }
    int replaced = emup->ReplaceImageWithFile(pageEl, picked);
    str::Free(picked);
    if (replaced) {
        gRenderCache->FreeForDisplayModel(dm);
        MainWindowRerender(win);
        logf("[imgedit] ReplaceImage: done; save the PDF (File > Save) to persist\n");
    }
}

void HandleCmdEditImageWithPaint(MainWindow* win) {
    WindowTab* tab = win->CurrentTab();
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    EngineMupdf* emup = dm ? AsEngineMupdf(dm->GetEngine()) : nullptr;
    IPageElement* pageEl = dm ? dm->GetElementAtPos(win->contextMenuPt, nullptr) : nullptr;
    if (!emup || !pageEl || !pageEl->Is(kindPageElementImage)) {
        logf("[imgedit] EditWithPaint: no image under the right-click point\n");
        return;
    }
    RenderedBitmap* bmp = dm->GetEngine()->GetImageForPageElement(pageEl);
    if (!bmp) {
        logf("[imgedit] EditWithPaint: could not extract image\n");
        return;
    }
    int pageNo = 0;
    int imageIdx = 0;
    if (!emup->GetImageElementCoords(pageEl, &pageNo, &imageIdx)) {
        delete bmp;
        return;
    }

    // stop any previous watch FIRST: it zeroes gPaintWatchTmpPath, so the new
    // path must be built after, for both the ShellExecute arg and the watch timer
    StopPaintImageWatch();

    WCHAR tmpDir[MAX_PATH]{};
    GetTempPathW(MAX_PATH, tmpDir);
    swprintf_s(gPaintWatchTmpPath, L"%stumatrapdf-paintedit-%u.png", tmpDir, GetCurrentProcessId());
    if (!SaveHbitmapAsPng(bmp->GetBitmap(), WStr(gPaintWatchTmpPath))) {
        logf("[imgedit] EditWithPaint: could not write temp png\n");
        delete bmp;
        return;
    }
    delete bmp;

    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (GetFileAttributesExW(gPaintWatchTmpPath, GetFileExInfoStandard, &fad)) {
        gPaintWatchMtime = fad.ftLastWriteTime;
    }
    WCHAR params[MAX_PATH + 16]{};
    swprintf_s(params, L"\"%s\"", gPaintWatchTmpPath);
    gPaintWatchWin = win;
    gPaintWatchEngine = emup;
    gPaintWatchPageNo = pageNo;
    gPaintWatchImageIdx = imageIdx;

    HINSTANCE started = ShellExecuteW(win->hwndFrame, L"open", L"mspaint.exe", params, nullptr, SW_SHOWNORMAL);
    if ((INT_PTR)started <= 32) {
        logf("[imgedit] EditWithPaint: could not launch mspaint.exe\n");
        StopPaintImageWatch();
        return;
    }
    SetTimer(win->hwndFrame, kPaintWatchTimerId, kPaintWatchDelayMs, nullptr);
    logf("[imgedit] EditWithPaint: mspaint opened; saving the png replaces the PDF image (save the PDF after)\n");
}

void TickPaintImageWatch() {
    MainWindow* win = gPaintWatchWin;
    if (!win) {
        return;
    }
    WindowTab* tab = win->CurrentTab();
    DisplayModel* dm = tab ? tab->AsFixed() : nullptr;
    EngineMupdf* em = dm ? AsEngineMupdf(dm->GetEngine()) : nullptr;
    if (em != gPaintWatchEngine) {
        // doc swapped/closed while editing — stop without touching stale state
        logf("[imgedit] paint watch stopped (doc changed)\n");
        StopPaintImageWatch();
        return;
    }
    WIN32_FILE_ATTRIBUTE_DATA fad{};
    if (!GetFileAttributesExW(gPaintWatchTmpPath, GetFileExInfoStandard, &fad)) {
        return;
    }
    if (0 == CompareFileTime(&fad.ftLastWriteTime, &gPaintWatchMtime)) {
        return;
    }
    TempStr utf8Path = ToUtf8Temp(WStr(gPaintWatchTmpPath));
    int r = gPaintWatchEngine->ReplaceImageByIdx(gPaintWatchPageNo, gPaintWatchImageIdx, utf8Path);
    logf("[imgedit] paint edit saved; replaced=%d\n", r);
    StopPaintImageWatch();
    if (r > 0 && dm) {
        gRenderCache->FreeForDisplayModel(dm);
        MainWindowRerender(win);
        logf("[imgedit] external edit applied; save the PDF (File > Save) to persist\n");
    }
}
