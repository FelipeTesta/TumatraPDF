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
    CmdContributeTranslation = 302,
    CmdOpenWithKnownExternalViewerFirst = 303,
    CmdOpenWithExplorer = 304,
    CmdOpenWithDirectoryOpus = 305,
    CmdOpenWithTotalCommander = 306,
    CmdOpenWithDoubleCommander = 307,
    CmdOpenWithAcrobat = 308,
    CmdOpenWithFoxIt = 309,
    CmdOpenWithFoxItPhantom = 310,
    CmdOpenWithPdfXchange = 311,
    CmdOpenWithXpsViewer = 312,
    CmdOpenWithHtmlHelp = 313,
    CmdOpenWithPdfDjvuBookmarker = 314,
    CmdOpenWithKnownExternalViewerLast = 315,
    CmdOpenSelectedDocument = 316,
    CmdPinSelectedDocument = 317,
    CmdForgetSelectedDocument = 318,
    CmdExpandAll = 319,
    CmdCollapseAll = 320,
    CmdSaveEmbeddedFile = 321,
    CmdOpenEmbeddedPDF = 322,
    CmdSaveAttachment = 323,
    CmdOpenAttachment = 324,
    CmdOptions = 325,
    CmdAdvancedOptions = 326,
    CmdAdvancedSettings = 327,
    CmdChangeLanguage = 328,
    CmdCheckUpdate = 329,
    CmdInstallPrereleaseUpdate = 330,
    CmdTogglePdfPreviewLogging = 331,
    CmdHelpOpenManual = 332,
    CmdHelpOpenManualOnWebsite = 333,
    CmdHelpOpenKeyboardShortcuts = 334,
    CmdToggleKeyboardHelp = 335,
    CmdHelpVisitWebsite = 336,
    CmdHelpAbout = 337,
    CmdMoveFrameFocus = 338,
    CmdFavoriteAdd = 339,
    CmdFavoriteDel = 340,
    CmdFavoriteToggle = 341,
    CmdToggleLinks = 342,
    CmdToggleShowAnnotations = 343,
    CmdShowAnnotations = 344,
    CmdHideAnnotations = 345,
    CmdCreateAnnotText = 346,
    CmdCreateAnnotLink = 347,
    CmdCreateAnnotFreeText = 348,
    CmdCreateAnnotLine = 349,
    CmdCreateAnnotSquare = 350,
    CmdCreateAnnotCircle = 351,
    CmdCreateAnnotPolygon = 352,
    CmdCreateAnnotPolyLine = 353,
    CmdCreateAnnotHighlight = 354,
    CmdCreateAnnotUnderline = 355,
    CmdCreateAnnotSquiggly = 356,
    CmdCreateAnnotStrikeOut = 357,
    CmdCreateAnnotRedact = 358,
    CmdCreateAnnotStamp = 359,
    CmdCreateAnnotCaret = 360,
    CmdCreateAnnotInk = 361,
    CmdCreateAnnotPopup = 362,
    CmdCreateAnnotFileAttachment = 363,
    CmdInvertColors = 364,
    CmdTogglePageInfo = 365,
    CmdToggleZoom = 366,
    CmdNavigateBack = 367,
    CmdNavigateForward = 368,
    CmdToggleCursorPosition = 369,
    CmdOpenNextFileInFolder = 370,
    CmdOpenPrevFileInFolder = 371,
    CmdCommandPalette = 372,
    CmdShowLog = 373,
    CmdShowErrors = 374,
    CmdClearHistory = 375,
    CmdReopenLastClosedFile = 376,
    CmdNextTab = 377,
    CmdPrevTab = 378,
    CmdNextTabSmart = 379,
    CmdPrevTabSmart = 380,
    CmdMoveTabLeft = 381,
    CmdMoveTabRight = 382,
    CmdInvokeInverseSearch = 383,
    CmdExec = 384,
    CmdViewWithExternalViewer = 385,
    CmdSelectionHandler = 386,
    CmdSetTheme = 387,
    CmdToggleInverseSearch = 388,
    CmdDebugCorruptMemory = 389,
    CmdDebugCrashMe = 390,
    CmdDebugDownloadSymbols = 391,
    CmdDebugTestApp = 392,
    CmdDebugShowNotif = 393,
    CmdDebugStartStressTest = 394,
    CmdDebugTogglePredictiveRender = 395,
    CmdDebugToggleRtl = 396,
    CmdListPrinters = 397,
    CmdToggleWindowsPreviewer = 398,
    CmdToggleWindowsSearchFilter = 399,
    CmdScreenshot = 400,
    CmdCropImage = 401,
    CmdResizeImage = 402,
    CmdSaveImage = 403,
    CmdPasteClipboardImage = 404,
    CmdTabGroupSave = 405,
    CmdTabGroupRestore = 406,
    CmdChangeBackgroundColor = 407,
    CmdSetTabColor = 408,
    CmdPdfCompress = 409,
    CmdPdfDecompress = 410,
    CmdPdfDeletePages = 411,
    CmdPdfExtractPages = 412,
    CmdPdfEncrypt = 413,
    CmdPdfDecrypt = 414,
    CmdPdfBake = 415,
    CmdPdShowInfo = 416,
    CmdDocumentExtractText = 417,
    CmdDocumentShowOutline = 418,
    CmdSetScreenshotHotkey = 419,
    CmdReadAloud = 420,
    CmdPauseReadAloud = 421,
    CmdContinueReadAloud = 422,
    CmdStopReadAloud = 423,
    CmdReadAloudFromTopPage = 424,
    CmdReadAloudSelection = 425,
    CmdToggleToolbarShowReadAloud = 426,
    CmdRemoveDeletedFilesFromHistory = 427,
    CmdCommandPaletteTOC = 428,
    CmdDebugToggleRenderInfo = 429,
    CmdConvertImageToPdf = 430,
    CmdExpandToCurrentPage = 431,
    CmdStartAutoScroll = 432,
    CmdAIChatWithClaudeCode = 433,
    CmdAIChatWithGrokBuild = 434,
    CmdAIChatWithOpenAICodex = 435,
    CmdTranslateSelectionWithGrokBuild = 436,
    CmdTranslateSelectionWithClaudeCode = 437,
    CmdTranslateSelectionWithOpenAICodex = 438,
    CmdFindToggleMatchWholeWord = 439,
    CmdGoToNextFavorite = 440,
    CmdGoToPrevFavorite = 441,
    CmdCreateAnnotImageFromClipboard = 442,
    CmdSetInverseSearch = 443,
    CmdCommandPaletteFavorites = 444,
    CmdNavigateFilesInFolder = 445,
    CmdDebugToggleCacheInfo = 446,
    CmdToggleEngineeringDrawingEnhance = 447,
    CmdSetDocumentColorsFollowTheme = 448,
    CmdTogglePreservePdfImages = 449,
    CmdToggleLightDarkTheme = 450,
    CmdChangeTheme = 451,
    CmdTranslateSelection = 452,
    CmdFavoriteShowInTab = 453,
    CmdTocExpandToLevel1 = 454,
    CmdTocExpandToLevel2 = 455,
    CmdTocExpandToLevel3 = 456,
    CmdTocCollapseSameLevel = 457,
    CmdToggleFavoritesSort = 458,
    CmdZoomFitHeight = 459,
    CmdDeleteFileAndOpenNext = 460,
    CmdShowGeneratedHTML = 461,
    CmdDeleteCachedFiles = 462,
    CmdToggleKeyboardLinkFollowing = 463,
    CmdDebugToggleDpiOverride = 464,
    CmdToggleImages = 465,
    CmdSelectTextViaKeyboard = 466,
    CmdOpenFileWithOSFilePicker = 467,
    CmdToggleFilePicker = 468,
    CmdToggleBoolSetting = 469,
    CmdFixDefaultApp = 470,
    CmdAIChatWithAntiGravity = 471,
    CmdTranslateSelectionWithAntiGravity = 472,
    CmdConvertToPDF = 473,
    CmdDebugShowFitContentArea = 474,
    CmdExtendSelectionCharLeft = 475,
    CmdExtendSelectionCharRight = 476,
    CmdExtendSelectionWordLeft = 477,
    CmdExtendSelectionWordRight = 478,
    CmdToggleLaserPointer = 479,
    CmdZoomToSelection = 480,
    CmdAutoScrollToggle = 481,
    CmdAutoScrollSpeedUp = 482,
    CmdAutoScrollSpeedDown = 483,
    CmdContrastToggle = 484,
    CmdContrastIncrease = 485,
    CmdContrastDecrease = 486,
    CmdViewportCropToggle = 487,
    CmdMarginTrimToggle = 488,
    CmdTrimConfig = 489,
    CmdArchScale = 490,
    CmdArchMeasure = 491,
    CmdArchScaleApply = 492,
    CmdArchClear = 493,
    CmdArchToolsToggle = 494,
    CmdArchResetScale = 495,
    CmdArchCleanAll = 496,
    CmdAutoScrollTimerToggle = 497,
    CmdAutoScrollTimerEdit = 498,
    CmdFlashcardToggle = 499,
    CmdFlashcardStudy = 500,
    CmdFlashcardAdd = 501,
    CmdFlashcardBack = 502,
    CmdFlashcardFilter = 503,
    CmdFlashcardReveal = 504,
    CmdFlashcardRate1 = 505,
    CmdFlashcardRate2 = 506,
    CmdFlashcardRate3 = 507,
    CmdFlashcardRate4 = 508,
    CmdFlashcardLista = 509,
    CmdFlashcardNext = 510,
    CmdFlashcardOrderOptions = 511,
    CmdFlashcardCleanHistory = 512,
    CmdViewportCropV2Toggle = 513,
    CmdNone = 514,

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
