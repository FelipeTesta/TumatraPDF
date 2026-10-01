/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

// @gen-start cmd-enum
// clang-format off
enum {
    // commands are integers sent with WM_COMMAND so start them
    // at some number higher than 0
    CmdFirst = 200,
    CmdSeparator = CmdFirst,

    CmdOpenFile = 201,
    CmdClose = 202,
    CmdCloseCurrentDocument = 203,
    CmdCloseOtherTabs = 204,
    CmdCloseTabsToTheRight = 205,
    CmdCloseTabsToTheLeft = 206,
    CmdCloseAllTabs = 207,
    CmdSaveAs = 208,
    CmdPrint = 209,
    CmdShowInFolder = 210,
    CmdRenameFile = 211,
    CmdDeleteFile = 212,
    CmdExit = 213,
    CmdReloadDocument = 214,
    CmdCreateShortcutToFile = 215,
    CmdSendByEmail = 216,
    CmdProperties = 217,
    CmdSinglePageView = 218,
    CmdFacingView = 219,
    CmdBookView = 220,
    CmdToggleContinuousView = 221,
    CmdToggleMangaMode = 222,
    CmdRotateLeft = 223,
    CmdRotateRight = 224,
    CmdToggleBookmarks = 225,
    CmdToggleTableOfContents = 226,
    CmdToggleFullscreen = 227,
    CmdPresentationWhiteBackground = 228,
    CmdPresentationBlackBackground = 229,
    CmdTogglePresentationMode = 230,
    CmdToggleToolbar = 231,
    CmdChangeScrollbar = 232,
    CmdToggleMenuBar = 233,
    CmdCopySelection = 234,
    CmdTranslateSelectionWithGoogle = 235,
    CmdTranslateSelectionWithDeepL = 236,
    CmdSearchSelectionWithGoogle = 237,
    CmdSearchSelectionWithBing = 238,
    CmdSearchSelectionWithWikipedia = 239,
    CmdSearchSelectionWithGoogleScholar = 240,
    CmdSelectAll = 241,
    CmdNewWindow = 242,
    CmdDuplicateInNewWindow = 243,
    CmdDuplicateInNewTab = 244,
    CmdCopyImage = 245,
    CmdCopyLinkTarget = 246,
    CmdCopyComment = 247,
    CmdCopyFilePath = 248,
    CmdScrollUp = 249,
    CmdScrollDown = 250,
    CmdScrollLeft = 251,
    CmdScrollRight = 252,
    CmdScrollLeftPage = 253,
    CmdScrollRightPage = 254,
    CmdScrollUpPage = 255,
    CmdScrollDownPage = 256,
    CmdScrollDownHalfPage = 257,
    CmdScrollUpHalfPage = 258,
    CmdGoToNextPage = 259,
    CmdGoToPrevPage = 260,
    CmdGoToFirstPage = 261,
    CmdGoToLastPage = 262,
    CmdGoToPage = 263,
    CmdGoToPageNextKeepScroll = 264,
    CmdGoToPagePrevKeepScroll = 265,
    CmdFindFirst = 266,
    CmdFindNext = 267,
    CmdFindPrev = 268,
    CmdFindNextSel = 269,
    CmdFindPrevSel = 270,
    CmdFindToggleMatchCase = 271,
    CmdSaveAnnotations = 272,
    CmdSaveAnnotationsNewFile = 273,
    CmdDiscardChanges = 274,
    CmdEditAnnotations = 275,
    CmdDeleteAnnotation = 276,
    CmdZoomFitPage = 277,
    CmdZoomActualSize = 278,
    CmdZoomFitWidth = 279,
    CmdZoomFitByOrientation = 280,
    CmdZoom6400 = 281,
    CmdZoom3200 = 282,
    CmdZoom1600 = 283,
    CmdZoom800 = 284,
    CmdZoom400 = 285,
    CmdZoom200 = 286,
    CmdZoom150 = 287,
    CmdZoom125 = 288,
    CmdZoom100 = 289,
    CmdZoom75 = 290,
    CmdZoom50 = 291,
    CmdZoom25 = 292,
    CmdZoom12_5 = 293,
    CmdZoom8_33 = 294,
    CmdZoomFitContent = 295,
    CmdZoomShrinkToFit = 296,
    CmdZoomCustom = 297,
    CmdZoomIn = 298,
    CmdZoomOut = 299,
    CmdZoomFitWidthAndContinuous = 300,
    CmdZoomFitPageAndSinglePage = 301,
    CmdOpenWithKnownExternalViewerFirst = 302,
    CmdOpenWithExplorer = 303,
    CmdOpenWithDirectoryOpus = 304,
    CmdOpenWithTotalCommander = 305,
    CmdOpenWithDoubleCommander = 306,
    CmdOpenWithAcrobat = 307,
    CmdOpenWithFoxIt = 308,
    CmdOpenWithFoxItPhantom = 309,
    CmdOpenWithPdfXchange = 310,
    CmdOpenWithXpsViewer = 311,
    CmdOpenWithHtmlHelp = 312,
    CmdOpenWithPdfDjvuBookmarker = 313,
    CmdOpenWithKnownExternalViewerLast = 314,
    CmdOpenSelectedDocument = 315,
    CmdPinSelectedDocument = 316,
    CmdForgetSelectedDocument = 317,
    CmdExpandAll = 318,
    CmdCollapseAll = 319,
    CmdSaveEmbeddedFile = 320,
    CmdOpenEmbeddedPDF = 321,
    CmdSaveAttachment = 322,
    CmdOpenAttachment = 323,
    CmdOptions = 324,
    CmdAdvancedOptions = 325,
    CmdAdvancedSettings = 326,
    CmdChangeLanguage = 327,
    CmdCheckUpdate = 328,
    CmdInstallPrereleaseUpdate = 329,
    CmdTogglePdfPreviewLogging = 330,
    CmdHelpOpenManual = 331,
    CmdHelpOpenManualOnWebsite = 332,
    CmdHelpOpenKeyboardShortcuts = 333,
    CmdToggleKeyboardHelp = 334,
    CmdHelpVisitWebsite = 335,
    CmdHelpAbout = 336,
    CmdMoveFrameFocus = 337,
    CmdFavoriteAdd = 338,
    CmdFavoriteDel = 339,
    CmdFavoriteToggle = 340,
    CmdToggleLinks = 341,
    CmdToggleShowAnnotations = 342,
    CmdShowAnnotations = 343,
    CmdHideAnnotations = 344,
    CmdCreateAnnotText = 345,
    CmdCreateAnnotLink = 346,
    CmdCreateAnnotFreeText = 347,
    CmdCreateAnnotLine = 348,
    CmdCreateAnnotSquare = 349,
    CmdCreateAnnotCircle = 350,
    CmdCreateAnnotPolygon = 351,
    CmdCreateAnnotPolyLine = 352,
    CmdCreateAnnotHighlight = 353,
    CmdCreateAnnotUnderline = 354,
    CmdCreateAnnotSquiggly = 355,
    CmdCreateAnnotStrikeOut = 356,
    CmdCreateAnnotRedact = 357,
    CmdCreateAnnotStamp = 358,
    CmdCreateAnnotCaret = 359,
    CmdCreateAnnotInk = 360,
    CmdCreateAnnotPopup = 361,
    CmdCreateAnnotFileAttachment = 362,
    CmdInvertColors = 363,
    CmdTogglePageInfo = 364,
    CmdToggleZoom = 365,
    CmdNavigateBack = 366,
    CmdNavigateForward = 367,
    CmdToggleCursorPosition = 368,
    CmdOpenNextFileInFolder = 369,
    CmdOpenPrevFileInFolder = 370,
    CmdCommandPalette = 371,
    CmdShowLog = 372,
    CmdShowErrors = 373,
    CmdClearHistory = 374,
    CmdReopenLastClosedFile = 375,
    CmdNextTab = 376,
    CmdPrevTab = 377,
    CmdNextTabSmart = 378,
    CmdPrevTabSmart = 379,
    CmdMoveTabLeft = 380,
    CmdMoveTabRight = 381,
    CmdInvokeInverseSearch = 382,
    CmdExec = 383,
    CmdViewWithExternalViewer = 384,
    CmdSelectionHandler = 385,
    CmdSetTheme = 386,
    CmdToggleInverseSearch = 387,
    CmdDebugCorruptMemory = 388,
    CmdDebugCrashMe = 389,
    CmdDebugDownloadSymbols = 390,
    CmdDebugTestApp = 391,
    CmdDebugShowNotif = 392,
    CmdDebugStartStressTest = 393,
    CmdDebugTogglePredictiveRender = 394,
    CmdDebugToggleRtl = 395,
    CmdListPrinters = 396,
    CmdToggleWindowsPreviewer = 397,
    CmdToggleWindowsSearchFilter = 398,
    CmdScreenshot = 399,
    CmdCropImage = 400,
    CmdResizeImage = 401,
    CmdSaveImage = 402,
    CmdPasteClipboardImage = 403,
    CmdTabGroupSave = 404,
    CmdTabGroupRestore = 405,
    CmdChangeBackgroundColor = 406,
    CmdSetTabColor = 407,
    CmdPdfCompress = 408,
    CmdPdfDecompress = 409,
    CmdPdfDeletePages = 410,
    CmdPdfExtractPages = 411,
    CmdPdfEncrypt = 412,
    CmdPdfDecrypt = 413,
    CmdPdfBake = 414,
    CmdPdShowInfo = 415,
    CmdDocumentExtractText = 416,
    CmdDocumentShowOutline = 417,
    CmdSetScreenshotHotkey = 418,
    CmdReadAloud = 419,
    CmdPauseReadAloud = 420,
    CmdContinueReadAloud = 421,
    CmdStopReadAloud = 422,
    CmdReadAloudFromTopPage = 423,
    CmdReadAloudSelection = 424,
    CmdToggleToolbarShowReadAloud = 425,
    CmdRemoveDeletedFilesFromHistory = 426,
    CmdCommandPaletteTOC = 427,
    CmdDebugToggleRenderInfo = 428,
    CmdConvertImageToPdf = 429,
    CmdExpandToCurrentPage = 430,
    CmdStartAutoScroll = 431,
    CmdAIChatWithClaudeCode = 432,
    CmdAIChatWithGrokBuild = 433,
    CmdAIChatWithOpenAICodex = 434,
    CmdTranslateSelectionWithGrokBuild = 435,
    CmdTranslateSelectionWithClaudeCode = 436,
    CmdTranslateSelectionWithOpenAICodex = 437,
    CmdFindToggleMatchWholeWord = 438,
    CmdGoToNextFavorite = 439,
    CmdGoToPrevFavorite = 440,
    CmdCreateAnnotImageFromClipboard = 441,
    CmdSetInverseSearch = 442,
    CmdCommandPaletteFavorites = 443,
    CmdNavigateFilesInFolder = 444,
    CmdDebugToggleCacheInfo = 445,
    CmdToggleEngineeringDrawingEnhance = 446,
    CmdSetDocumentColorsFollowTheme = 447,
    CmdTogglePreservePdfImages = 448,
    CmdToggleLightDarkTheme = 449,
    CmdChangeTheme = 450,
    CmdTranslateSelection = 451,
    CmdFavoriteShowInTab = 452,
    CmdTocExpandToLevel1 = 453,
    CmdTocExpandToLevel2 = 454,
    CmdTocExpandToLevel3 = 455,
    CmdTocCollapseSameLevel = 456,
    CmdToggleFavoritesSort = 457,
    CmdZoomFitHeight = 458,
    CmdDeleteFileAndOpenNext = 459,
    CmdShowGeneratedHTML = 460,
    CmdDeleteCachedFiles = 461,
    CmdToggleKeyboardLinkFollowing = 462,
    CmdDebugToggleDpiOverride = 463,
    CmdToggleImages = 464,
    CmdSelectTextViaKeyboard = 465,
    CmdOpenFileWithOSFilePicker = 466,
    CmdToggleFilePicker = 467,
    CmdToggleBoolSetting = 468,
    CmdFixDefaultApp = 469,
    CmdAIChatWithAntiGravity = 470,
    CmdTranslateSelectionWithAntiGravity = 471,
    CmdConvertToPDF = 472,
    CmdDebugShowFitContentArea = 473,
    CmdExtendSelectionCharLeft = 474,
    CmdExtendSelectionCharRight = 475,
    CmdExtendSelectionWordLeft = 476,
    CmdExtendSelectionWordRight = 477,
    CmdToggleLaserPointer = 478,
    CmdZoomToSelection = 479,
    CmdAutoScrollToggle = 480,
    CmdAutoScrollSpeedUp = 481,
    CmdAutoScrollSpeedDown = 482,
    CmdContrastToggle = 483,
    CmdContrastIncrease = 484,
    CmdContrastDecrease = 485,
    CmdViewportCropToggle = 486,
    CmdMarginTrimToggle = 487,
    CmdTrimConfig = 488,
    CmdArchScale = 489,
    CmdArchMeasure = 490,
    CmdArchScaleApply = 491,
    CmdArchClear = 492,
    CmdArchToolsToggle = 493,
    CmdArchResetScale = 494,
    CmdArchCleanAll = 495,
    CmdAutoScrollTimerToggle = 496,
    CmdAutoScrollTimerEdit = 497,
    CmdFlashcardToggle = 498,
    CmdFlashcardStudy = 499,
    CmdFlashcardAdd = 500,
    CmdFlashcardBack = 501,
    CmdFlashcardFilter = 502,
    CmdFlashcardReveal = 503,
    CmdFlashcardRate1 = 504,
    CmdFlashcardRate2 = 505,
    CmdFlashcardRate3 = 506,
    CmdFlashcardRate4 = 507,
    CmdFlashcardLista = 508,
    CmdFlashcardNext = 509,
    CmdFlashcardOrderOptions = 510,
    CmdFlashcardConfig = 511,
    CmdViewportCropV2Toggle = 512,
    CmdNone = 513,

    /* range for file history */
    CmdFileHistoryFirst,
    CmdFileHistoryLast = CmdFileHistoryFirst + 32,

    /* range for favorites */
    CmdFavoriteFirst,
    CmdFavoriteLast = CmdFavoriteFirst + 256,

    CmdLast = CmdFavoriteLast,
    CmdFirstCustom = CmdLast + 100,

    // aliases, at the end to not mess ordering
    CmdViewLayoutFirst = CmdSinglePageView,
    CmdViewLayoutLast = CmdToggleMangaMode,

    CmdZoomFirst = CmdZoomFitPage,
    CmdZoomLast = CmdZoomCustom,

    CmdCreateAnnotFirst = CmdCreateAnnotText,
    CmdCreateAnnotLast = CmdCreateAnnotFileAttachment,
};
// clang-format on
// @gen-end cmd-enum

// order of CreateAnnot* must be the same as enum AnnotationType
/*
TOOD: maybe add commands for those annotations
Sound,
Movie,
Widget,
Screen,
PrinterMark,
TrapNet,
Watermark,
ThreeD,
*/

struct CommandArg {
    enum class Type : u16 {
        None,
        Bool,
        Int,
        Float,
        String,
        Color,
    };

    // arguments are a linked list for simplicity
    struct CommandArg* next = nullptr;

    Type type = Type::None;

    // TODO: we have a fixed number of argument names
    // we could use SeqStrings and use u16 for arg name id
    Str name;

    // TODO: could be a union
    Str strVal;
    bool boolVal = false;
    int intVal = 0;
    float floatVal = 0.0;
    ParsedColor colorVal;
};

CommandArg* AllocCommandArg(Str name, Str strVal);
void FreeCommandArgs(CommandArg* first);

struct CustomCommand {
    // all commands are stored as linked list
    struct CustomCommand* next = nullptr;

    // the command id like CmdOpenFile
    int origId = 0;

    // for debugging, the full definition of the command
    // as given by the user
    Str definition;

    // optional name, if given this shows up in command palette
    Str name;

    // optional keyboard shortcut
    Str key;

    // a unique command id generated by us, starting with CmdFirstCustom
    // it identifies a command with their fixed set of arguments
    int id = 0;

    CommandArg* firstArg = nullptr;
};

CustomCommand* AllocCustomCommand(Str definition, Str name, Str key);
void FreeCustomCommand(CustomCommand* cmd);

extern CustomCommand* gFirstCustomCommand;
extern SeqStrings gCommandDescriptions;

int GetCommandIdByName(Str);
int GetCommandIdByDesc(Str);

CustomCommand* CreateCustomCommand(Str definition, int origCmdId, CommandArg* args, Str name = {}, Str key = {});
CustomCommand* CloneCustomCommand(CustomCommand* cmd, Str name = {}, Str key = {});
CustomCommand* FindCustomCommand(int cmdId);
void FreeCustomCommands();
CommandArg* NewStringArg(Str name, Str val);
CommandArg* NewFloatArg(Str name, float val);
void InsertArg(CommandArg** firstPtr, CommandArg* arg);

CustomCommand* CreateCommandFromDefinition(Str definition);
CommandArg* GetCommandArg(CustomCommand*, Str argName);
int GetCommandIntArg(CustomCommand* cmd, Str name, int defValue);
bool GetCommandBoolArg(CustomCommand* cmd, Str name, bool defValue);
Str GetCommandStringArg(CustomCommand* cmd, Str name, Str defValue);
void GetCommandsWithOrigId(Vec<CustomCommand*>& commands, int origId);

#define kCmdArgColor StrL("color")
#define kCmdArgBgColor StrL("bgcolor")
#define kCmdArgOpacity StrL("opacity")
#define kCmdArgOpenEdit StrL("openedit")
#define kCmdArgTextSize StrL("textsize")
#define kCmdArgBorderWidth StrL("borderwidth")
#define kCmdArgAlignment StrL("alignment")
#define kCmdArgInteriorColor StrL("interiorcolor")

#define kCmdArgCopyToClipboard StrL("copytoclipboard")
#define kCmdArgSetContent StrL("setcontent")
#define kCmdArgExe StrL("exe")
#define kCmdArgURL StrL("url")
// SelectionHandlers: how and what to send (see Customize-search-translation-services.md)
#define kCmdArgMethod StrL("method")
#define kCmdArgBody StrL("body")
#define kCmdArgContentType StrL("contenttype")
#define kCmdArgHeaders StrL("headers")
#define kCmdArgLevel StrL("level")
#define kCmdArgFilter StrL("filter")
#define kCmdArgN StrL("n")
#define kCmdArgMode StrL("mode")
#define kCmdArgTheme StrL("theme")
#define kCmdArgCommandLine StrL("cmdline")
#define kCmdArgToolbarText StrL("toolbartext")
#define kCmdArgToolbarSvgIcon StrL("toolbarsvgicon")
#define kCmdArgFocusEdit StrL("focusedit")
#define kCmdArgFocusList StrL("focuslist")
// optional bool to force a state on a toggle command instead of flipping it (#5067)
#define kCmdArgState StrL("state")
#define kCmdArgName StrL("name")
#define kCmdArgExt StrL("ext")
