#include "utils/BaseUtil.h"

#include "Settings.h"
#include "SumatraPDF.h"

#include "prettysumatra/BridgeDispatcher.h"
#include "prettysumatra/CommandBridgeSpec.h"

#include "Commands.h"
#include "DisplayMode.h"
#include "wingui/UIModels.h"
#include "DocController.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "GlobalPrefs.h"
#include "MainWindow.h"
#include "ProgressUpdateUI.h"
#include "TextSelection.h"
#include "TextSearch.h"
#include "SearchAndDDE.h"
#include "WindowTab.h"
#include "Theme.h"
#include "Translations.h"
#include "AppSettings.h"
#include "DarkModeSubclass.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/WebView.h"
#include "FileHistory.h"

#include "utils/JsonParser.h"
#include "utils/Log.h"
#include "utils/StrUtil.h"
#include "utils/WinUtil.h"

#include "Notifications.h"

namespace prettysumatra {
namespace bridge {

static bool ParseBoolEnvWithDefault(const char* envName, bool defValue) {
    char buf[16] = {};
    DWORD n = GetEnvironmentVariableA(envName, buf, dimof(buf));
    if (n == 0 || n >= dimof(buf)) {
        return defValue;
    }
    if (str::EqI(buf, "1") || str::EqI(buf, "true") || str::EqI(buf, "yes") || str::EqI(buf, "on")) {
        return true;
    }
    if (str::EqI(buf, "0") || str::EqI(buf, "false") || str::EqI(buf, "no") || str::EqI(buf, "off")) {
        return false;
    }
    return defValue;
}

static TempStr ColorToCssHex(COLORREF c) {
    return str::FormatTemp("#%02x%02x%02x", GetRValue(c), GetGValue(c), GetBValue(c));
}

static bool WindowsPrefersDarkModeForHybridToolbar() {
    DWORD val = 1;
    DWORD cbData = sizeof(val);
    constexpr const wchar_t* kThemeRegPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    LONG err =
        RegGetValueW(HKEY_CURRENT_USER, kThemeRegPath, L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &val, &cbData);
    if (err != ERROR_SUCCESS) {
        return false;
    }
    return val == 0;
}

bool UseHybridToolbar() {
    return ParseBoolEnvWithDefault("PRETTYSUMATRA_WEBVIEW_TOOLBAR", true);
}

bool UseHybridSidebar() {
    return ParseBoolEnvWithDefault("PRETTYSUMATRA_WEBVIEW_SIDEBAR", false);
}

bool UseHybridShell() {
    if (ParseBoolEnvWithDefault("PRETTYSUMATRA_WEBVIEW_SHELL", false)) {
        return true;
    }
    return UseHybridToolbar() || UseHybridSidebar();
}

bool LogBridgeMessages() {
#ifdef NDEBUG
    return ParseBoolEnvWithDefault("PRETTYSUMATRA_LOG_BRIDGE", false);
#else
    return ParseBoolEnvWithDefault("PRETTYSUMATRA_LOG_BRIDGE", true);
#endif
}

static bool gHybridFollowWindowsTheme = true;
static bool gHybridFollowWindowsThemeInitialized = false;

static void EnsureHybridFollowWindowsThemeState() {
    if (gHybridFollowWindowsThemeInitialized) {
        return;
    }
    gHybridFollowWindowsTheme = IsCurrentThemeDefault();
    gHybridFollowWindowsThemeInitialized = true;
}

bool HybridThemeFollowsWindows() {
    EnsureHybridFollowWindowsThemeState();
    return gHybridFollowWindowsTheme;
}

static MainWindow* FindWindowForFrame(HWND hwndFrame) {
    if (!hwndFrame) {
        return nullptr;
    }
    return FindMainWindowByHwnd(hwndFrame);
}

static bool HybridToolbarCanAnnotate(MainWindow* win) {
    if (!win) {
        return false;
    }
    bool canAnnotate = false;
    WindowTab* tab = win->CurrentTab();
    if (tab && tab->IsDocLoaded()) {
        EngineBase* eng = tab->GetEngine();
        if (eng) {
            canAnnotate = EngineSupportsAnnotations(eng) && !(win->isFullScreen || win->presentation);
        }
    }
    return canAnnotate;
}

static void SyncHybridToolbarState(HWND hwndFrame, int currentPage, int totalPages, float zoomPercent,
                                   bool canAnnotate) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->hybridToolbar) {
        return;
    }

    if (currentPage <= 0) {
        currentPage = 1;
    }
    if (totalPages <= 0) {
        totalPages = 1;
    }
    if (zoomPercent < 1.0f) {
        zoomPercent = 1.0f;
    }
    if (zoomPercent > 6400.0f) {
        zoomPercent = 6400.0f;
    }

    if (win->hybridToolbarSyncCtrl != win->ctrl) {
        win->hybridToolbarSyncCtrl = win->ctrl;
        win->hybridToolbarSyncHasState = false;
    }
    if (win->hybridToolbarSyncHasState && win->hybridToolbarSyncPageNo == currentPage &&
        win->hybridToolbarSyncPageCount == totalPages && win->hybridToolbarSyncZoom == zoomPercent &&
        win->hybridToolbarSyncCanAnnotate == canAnnotate) {
        return;
    }

    win->hybridToolbarSyncPageNo = currentPage;
    win->hybridToolbarSyncPageCount = totalPages;
    win->hybridToolbarSyncZoom = zoomPercent;
    win->hybridToolbarSyncCanAnnotate = canAnnotate;
    win->hybridToolbarSyncHasState = true;

    TempStr js = str::FormatTemp(
        "window.hybridToolbarBatchUpdate && window.hybridToolbarBatchUpdate({page:%d,total:%d,zoom:%.4f,canAnnotate:%s});",
        currentPage, totalPages, zoomPercent, canAnnotate ? "true" : "false");
    win->hybridToolbar->Eval(js);
}

static TempStr HybridToolbarThemeJs(HWND hwndFrame) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->hybridToolbar) {
        return nullptr;
    }

    COLORREF canvas = ThemeMainWindowBackgroundColor();
    COLORREF panelOrig = ThemeWindowControlBackgroundColor();
    // Make toolbar background slightly lighter than the main window background
    COLORREF panel = AdjustLightness2(panelOrig, 2);
    COLORREF panel2 = PrettyStyleEnabled() ? PrettySurfaceColor() : ThemeWindowControlBackgroundColor();
    COLORREF stroke = PrettyBorderColor();
    COLORREF text = ThemeWindowTextColor();
    COLORREF muted = ThemeWindowTextDisabledColor();
    COLORREF btn = AdjustLightness2(panelOrig, 10);
    COLORREF accent = PrettyAccentColor();
    COLORREF brandPrimary = ThemeBrandPrimaryColor();
    COLORREF brandGlow = ThemeBrandGlowColor();
    COLORREF shadow = ThemeShadowColor();
    // Determine appDark based on the original main window background
    bool appDark = DarkMode::isColorDark(panelOrig);
    bool docInverted = gGlobalPrefs->fixedPageUI.invertColors;
    bool windowsDark = WindowsPrefersDarkModeForHybridToolbar();
    bool followWindows = HybridThemeFollowsWindows();

    return str::FormatTemp(
        "window.__hybridToolbarThemePayload={canvas:'%s',panel:'%s',panel2:'%s',stroke:'%s',"
        "text:'%s',muted:'%s',btn:'%s',accent:'%s',brandPrimary:'%s',brandGlow:'%s',shadow:'%s',appDark:%s,"
        "docInverted:%s,windowsDark:%s,followWindows:%s};"
        "if(window.hybridToolbarApplyTheme){window.hybridToolbarApplyTheme(window.__hybridToolbarThemePayload);}",
        ColorToCssHex(canvas), ColorToCssHex(panel), ColorToCssHex(panel2), ColorToCssHex(stroke), ColorToCssHex(text),
        ColorToCssHex(muted), ColorToCssHex(btn), ColorToCssHex(accent), ColorToCssHex(brandPrimary),
        ColorToCssHex(brandGlow), ColorToCssHex(shadow), appDark ? "true" : "false", docInverted ? "true" : "false",
        windowsDark ? "true" : "false", followWindows ? "true" : "false");
}

// Generate JavaScript to inject theme colors into home page
// Similar to HybridToolbarThemeJs but for home page
static TempStr HomePageThemeJs() {
    COLORREF canvas = ThemeMainWindowBackgroundColor();
    COLORREF panel = ThemeMainWindowBackgroundColor();
    COLORREF panel2 = PrettyStyleEnabled() ? PrettySurfaceColor() : ThemeWindowControlBackgroundColor();
    COLORREF stroke = PrettyBorderColor();
    COLORREF text = ThemeWindowTextColor();
    COLORREF muted = ThemeWindowTextDisabledColor();
    COLORREF btn = ThemeControlBackgroundColor();
    COLORREF accent = PrettyAccentColor();
    COLORREF brandPrimary = ThemeBrandPrimaryColor();
    COLORREF brandGlow = ThemeBrandGlowColor();
    COLORREF shadow = ThemeShadowColor();
    bool appDark = DarkMode::isColorDark(panel);

    // Build JavaScript payload with all theme colors
    return str::FormatTemp(
        "window.__homePageThemePayload={canvas:'%s',panel:'%s',panel2:'%s',stroke:'%s',"
        "text:'%s',muted:'%s',btn:'%s',accent:'%s',brandPrimary:'%s',brandGlow:'%s',shadow:'%s',appDark:%s};"
        "if(window.applyHomePageTheme){window.applyHomePageTheme(window.__homePageThemePayload);}",
        ColorToCssHex(canvas), ColorToCssHex(panel), ColorToCssHex(panel2), ColorToCssHex(stroke), ColorToCssHex(text),
        ColorToCssHex(muted), ColorToCssHex(btn), ColorToCssHex(accent), ColorToCssHex(brandPrimary),
        ColorToCssHex(brandGlow), ColorToCssHex(shadow), appDark ? "true" : "false");
}

static void AppendJsonString(StrBuilder& json, const char* src);

static TempStr JsQuoted(const char* s) {
    StrBuilder sb;
    AppendJsonString(sb, s ? s : "");

    return (TempStr)sb.StealData();
}

static TempStr HybridToolbarTextJs(HWND hwndFrame) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->hybridToolbar) {
        return nullptr;
    }

    return str::FormatTemp(
        "window.__hybridToolbarTextPayload={lang:%s,subtitle:%s,openButton:%s,pagePrevTitle:%s,pageNextTitle:%s,"
        "pageTemplate:%s,zoomOutTitle:%s,zoomInTitle:%s,viewSinglePage:%s,viewFacing:%s,viewBookView:%s,"
        "continuousTitle:%s,searchPlaceholder:%s,bookmarksTitle:%s,favoritesTitle:%s,fullscreenTitle:%s,"
        "commandPaletteText:%s,rotateLeftTitle:%s,rotateRightTitle:%s,printTitle:%s,themeLabel:%s,"
        "followWindowsTitle:%s,followingWindowsTitle:%s,darkWord:%s,lightWord:%s,toggleThemeTitle:%s,documentLabel:%s,"
        "documentInvertTitle:%s};"
        "if(window.hybridToolbarApplyText){window.hybridToolbarApplyText(window.__hybridToolbarTextPayload);}",
        JsQuoted(trans::GetCurrentLangCode()), JsQuoted(_TRA("Focused reading")), JsQuoted(_TRA("Open")),
        JsQuoted(_TRA("Previous page")), JsQuoted(_TRA("Next page")), JsQuoted(_TRA("Page {current} / {total}")),
        JsQuoted(_TRA("Zoom out")), JsQuoted(_TRA("Zoom in")), JsQuoted(_TRA("Single Page")), JsQuoted(_TRA("Facing")),
        JsQuoted(_TRA("Book View")), JsQuoted(_TRA("Show pages continuously")), JsQuoted(_TRA("Search text")),
        JsQuoted(_TRA("Sidebar")), JsQuoted(_TRA("Favorites")), JsQuoted(_TRA("Fullscreen")), JsQuoted(_TRA("Cmd")),
        JsQuoted(_TRA("Rotate left")), JsQuoted(_TRA("Rotate right")), JsQuoted(_TRA("Print")), JsQuoted(_TRA("Theme")),
        JsQuoted(_TRA("Follow Windows")), JsQuoted(_TRA("Following Windows ({mode})")), JsQuoted(_TRA("dark")),
        JsQuoted(_TRA("light")), JsQuoted(_TRA("Toggle light/dark")), JsQuoted(_TRA("Doc")),
        JsQuoted(_TRA("Invert document colors")));
}

bool HasHybridToolbar(HWND hwndFrame) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    return win && win->hybridToolbar;
}

void SyncHybridToolbarTheme(HWND hwndFrame) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->hybridToolbar) {
        return;
    }
    TempStr js = HybridToolbarThemeJs(hwndFrame);
    if (!js) {
        return;
    }
    win->hybridToolbar->Eval(js);
}

void SyncHybridToolbarText(HWND hwndFrame) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->hybridToolbar) {
        return;
    }
    TempStr js = HybridToolbarTextJs(hwndFrame);
    if (!js) {
        return;
    }
    win->hybridToolbar->Eval(js);
}

void SyncHybridToolbarSearchText(HWND hwndFrame, const char* text) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->hybridToolbar) {
        return;
    }
    TempStr js =
        str::FormatTemp("window.hybridToolbarSetSearchText && window.hybridToolbarSetSearchText(%s);", JsQuoted(text));
    win->hybridToolbar->Eval(js);
}

void FocusHybridToolbarSearch(HWND hwndFrame) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->hybridToolbar) {
        return;
    }

    win->hybridToolbar->Focus();

    // Then focus the search input element inside the webview
    win->hybridToolbar->Eval("window.hybridToolbarFocusSearch && window.hybridToolbarFocusSearch();");
}

void SyncHybridToolbarButtonVisibility(HWND hwndFrame, bool showButtons) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->hybridToolbar) {
        return;
    }

    const char* js = showButtons ? "window.showToolbarButtons && window.showToolbarButtons();"
                                 : "window.hideToolbarButtons && window.hideToolbarButtons();";
    win->hybridToolbar->Eval(js);
}

void SyncHybridToolbarEditableAllowed(HWND hwndFrame, bool allowed) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->hybridToolbar) {
        return;
    }

    const char* js = allowed
                         ? "window.hybridToolbarSetEditableAllowed && window.hybridToolbarSetEditableAllowed(true);"
                         : "window.hybridToolbarSetEditableAllowed && window.hybridToolbarSetEditableAllowed(false);";
    win->hybridToolbar->Eval(js);
}

void InitHybridToolbarTheme(HWND hwndFrame) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->hybridToolbar) {
        return;
    }
    TempStr js = HybridToolbarThemeJs(hwndFrame);
    if (!js) {
        return;
    }
    win->hybridToolbar->Init(js);
}

void InitHybridToolbarText(HWND hwndFrame) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->hybridToolbar) {
        return;
    }
    TempStr js = HybridToolbarTextJs(hwndFrame);
    if (!js) {
        return;
    }
    win->hybridToolbar->Init(js);
}

void SyncHybridToolbarPageState(HWND hwndFrame, int currentPage, int totalPages) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win) {
        return;
    }
    float zoomPercent = win->ctrl ? win->ctrl->GetZoomVirtual(true) : 100.0f;
    SyncHybridToolbarState(hwndFrame, currentPage, totalPages, zoomPercent, HybridToolbarCanAnnotate(win));
}

void SyncHybridToolbarAnnotationAvailability(HWND hwndFrame) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win) {
        return;
    }
    int currentPage = win->ctrl ? win->ctrl->CurrentPageNo() : 1;
    int totalPages = win->ctrl ? win->ctrl->PageCount() : 1;
    float zoomPercent = win->ctrl ? win->ctrl->GetZoomVirtual(true) : 100.0f;
    SyncHybridToolbarState(hwndFrame, currentPage, totalPages, zoomPercent, HybridToolbarCanAnnotate(win));
}

void SyncHybridToolbarZoomState(HWND hwndFrame, float zoomPercent) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win) {
        return;
    }
    int currentPage = win->ctrl ? win->ctrl->CurrentPageNo() : 1;
    int totalPages = win->ctrl ? win->ctrl->PageCount() : 1;
    SyncHybridToolbarState(hwndFrame, currentPage, totalPages, zoomPercent, HybridToolbarCanAnnotate(win));
}

struct BridgeMessage {
    const char* name = nullptr;
    const char* path = nullptr;
    const char* query = nullptr;
    const char* command = nullptr;
    const char* action = nullptr;
    const char* direction = nullptr;
    const char* mode = nullptr;
    const char* color = nullptr;
    int page = 0;
    int cmdId = 0;
    float value = 0.0f;
    bool hasPage = false;
    bool hasCmdId = false;
    bool hasValue = false;
};

class BridgeMessageVisitor : public json::ValueVisitor {
  public:
    explicit BridgeMessageVisitor(BridgeMessage* out) : msg(out) {}

    bool Visit(const char* path, const char* value, json::Type type) override {
        if (!msg || !path || !value) {
            return false;
        }
        if (str::Eq(path, "/name") && type == json::Type::String) {
            msg->name = str::DupTemp(value);
            return true;
        }
        if (str::Eq(path, "/payload/path") && type == json::Type::String) {
            msg->path = str::DupTemp(value);
            return true;
        }
        if (str::Eq(path, "/payload/query") && type == json::Type::String) {
            msg->query = str::DupTemp(value);
            return true;
        }
        if (str::Eq(path, "/payload/command") && type == json::Type::String) {
            msg->command = str::DupTemp(value);
            return true;
        }
        if (str::Eq(path, "/payload/action") && type == json::Type::String) {
            msg->action = str::DupTemp(value);
            return true;
        }
        if (str::Eq(path, "/payload/direction") && type == json::Type::String) {
            msg->direction = str::DupTemp(value);
            return true;
        }
        if (str::Eq(path, "/payload/mode") && type == json::Type::String) {
            msg->mode = str::DupTemp(value);
            return true;
        }
        if (str::Eq(path, "/payload/page") && type == json::Type::Number) {
            msg->page = atoi(value);
            msg->hasPage = true;
            return true;
        }
        if (str::Eq(path, "/payload/cmdId") && type == json::Type::Number) {
            msg->cmdId = atoi(value);
            msg->hasCmdId = true;
            return true;
        }
        if (str::Eq(path, "/payload/value") && type == json::Type::Number) {
            msg->value = (float)atof(value);
            msg->hasValue = true;
            return true;
        }
        // Accept string 'value' for backwards-compatible color payloads (e.g. "#facc15")
        if (str::Eq(path, "/payload/value") && type == json::Type::String) {
            msg->color = str::DupTemp(value);
            return true;
        }
        if (str::Eq(path, "/payload/color") && type == json::Type::String) {
            msg->color = str::DupTemp(value);
            return true;
        }
        return true;
    }

  private:
    BridgeMessage* msg = nullptr;
};

static bool ParseBridgeMessage(const char* rawMsg, BridgeMessage& out) {
    if (str::IsEmptyOrWhiteSpace(rawMsg)) {
        return false;
    }
    BridgeMessageVisitor visitor(&out);
    bool ok = json::Parse(rawMsg, &visitor);
    return ok && !str::IsEmpty(out.name);
}

static MainWindow* GetTargetWindow() {
    HWND hwnd = GetForegroundWindow();
    MainWindow* win = FindMainWindowByHwnd(hwnd);
    if (!win && gLastActiveFrameHwnd) {
        win = FindMainWindowByHwnd(gLastActiveFrameHwnd);
    }
    if (!win && !gWindows.IsEmpty()) {
        win = gWindows.at(0);
    }
    return win;
}

// Global variable to store the highlight color from the latest message
static char gPendingHighlightColor[16] = "#facc15";
static char gPendingUnderlineColor[16] = "#22c55e";
static char gPendingStrikeoutColor[16] = "#ef4444";

const char* GetPendingHighlightColor() {
    return gPendingHighlightColor;
}

const char* GetPendingUnderlineColor() {
    return gPendingUnderlineColor;
}

const char* GetPendingStrikeoutColor() {
    return gPendingStrikeoutColor;
}

static bool DispatchHighlightSelection(const BridgeMessage& msg) {
    MainWindow* win = GetTargetWindow();
    if (!win) {
        return false;
    }

    // Validate and store the color
    if (msg.color && str::StartsWith(msg.color, "#") && str::Len(msg.color) == 7) {
        str::BufSet(gPendingHighlightColor, dimof(gPendingHighlightColor), msg.color);
    }

    // Dispatch synchronously so the native command sees the current text
    // selection before toolbar focus changes clear it.
    SendMessageW(win->hwndFrame, WM_COMMAND, CmdCreateAnnotHighlight, 0);
    return true;
}

static bool DispatchOpenFile(const BridgeMessage& msg) {
    if (!CanAccessDisk()) {
        return false;
    }
    MainWindow* win = GetTargetWindow();
    if (!win) {
        return false;
    }
    if (str::IsEmpty(msg.path)) {
        PostMessageW(win->hwndFrame, WM_COMMAND, CmdOpenFile, 0);
        return true;
    }
    LoadArgs args(msg.path, win);
    LoadDocument(&args);
    return true;
}

static bool DispatchGoToPage(const BridgeMessage& msg) {
    MainWindow* win = GetTargetWindow();
    if (!win || !win->ctrl || !msg.hasPage) {
        return false;
    }
    if (!win->ctrl->ValidPageNo(msg.page)) {
        return false;
    }
    win->ctrl->GoToPage(msg.page, true);
    return true;
}

static bool DispatchZoom(const BridgeMessage& msg) {
    MainWindow* win = GetTargetWindow();
    if (!win || !win->ctrl || !msg.hasValue) {
        return false;
    }

    float zoom = msg.value;
    if (zoom > 0 && zoom <= 8.0f) {
        zoom *= 100.0f;
    }
    if (zoom < kZoomMin) {
        zoom = kZoomMin;
    }
    if (zoom > kZoomMax) {
        zoom = kZoomMax;
    }
    win->ctrl->SetZoomVirtual(zoom, nullptr);
    SyncHybridToolbarZoomState(win->hwndFrame, win->ctrl->GetZoomVirtual(true));
    return true;
}

static bool DispatchSetFitMode(const BridgeMessage& msg) {
    MainWindow* win = GetTargetWindow();
    if (!win || !win->ctrl || str::IsEmpty(msg.mode)) {
        return false;
    }

    float zoom = 0;
    if (str::EqI(msg.mode, "page-width")) {
        zoom = kZoomFitWidth;
    } else if (str::EqI(msg.mode, "page")) {
        zoom = kZoomFitPage;
    } else if (str::EqI(msg.mode, "actual-size")) {
        zoom = kZoomActualSize;
    } else {
        return false;
    }
    win->ctrl->SetZoomVirtual(zoom, nullptr);
    SyncHybridToolbarZoomState(win->hwndFrame, win->ctrl->GetZoomVirtual(true));
    return true;
}

static bool DispatchSearch(const BridgeMessage& msg) {
    MainWindow* win = GetTargetWindow();
    if (!win || !win->IsDocLoaded() || str::IsEmptyOrWhiteSpace(msg.query)) {
        return false;
    }

    TextSearch::Direction dir = TextSearch::Direction::Forward;
    if (str::EqI(msg.direction, "backward") || str::EqI(msg.direction, "prev") || str::EqI(msg.direction, "previous")) {
        dir = TextSearch::Direction::Backward;
    }

    bool wasModified = true;
    if (str::EqI(msg.action, "step") || str::EqI(msg.action, "next") || str::EqI(msg.action, "prev") ||
        str::EqI(msg.action, "previous")) {
        wasModified = false;
    } else if (str::IsEmpty(msg.action)) {
        TempStr currentFind = HwndGetTextTemp(win->hwndFindEdit);
        if (str::Eq(msg.query, currentFind)) {
            wasModified = false;
        }
    }
    HwndSetText(win->hwndFindEdit, msg.query);
    Edit_SetModify(win->hwndFindEdit, FALSE);
    int startPage = (dir == TextSearch::Direction::Backward) ? win->ctrl->PageCount() : 1;
    FindTextOnThread(win, dir, msg.query, wasModified, true, startPage);
    return true;
}

static bool DispatchToggleSidebar() {
    MainWindow* win = GetTargetWindow();
    if (!win) {
        return false;
    }
    bool nextTocVisible = !win->tocVisible;
    SetSidebarVisibility(win, nextTocVisible, gGlobalPrefs->showFavorites);
    return true;
}

static bool DispatchSetViewMode(const BridgeMessage& msg) {
    MainWindow* win = GetTargetWindow();
    if (!win || str::IsEmpty(msg.mode)) {
        return false;
    }

    DisplayMode mode = DisplayMode::Automatic;
    if (str::EqI(msg.mode, "continuous")) {
        mode = DisplayMode::Continuous;
    } else if (str::EqI(msg.mode, "single-page")) {
        mode = DisplayMode::SinglePage;
    } else {
        return false;
    }
    SwitchToDisplayMode(win, mode, false);
    return true;
}

static bool DispatchPrint() {
    MainWindow* win = GetTargetWindow();
    if (!win) {
        return false;
    }
    PostMessageW(win->hwndFrame, WM_COMMAND, CmdPrint, 0);
    return true;
}

static bool WindowsPrefersDarkModeForHybrid() {
    DWORD val = 1;
    DWORD cbData = sizeof(val);
    constexpr const wchar_t* kThemeRegPath = L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
    LONG err =
        RegGetValueW(HKEY_CURRENT_USER, kThemeRegPath, L"AppsUseLightTheme", RRF_RT_REG_DWORD, nullptr, &val, &cbData);
    if (err != ERROR_SUCCESS) {
        return false;
    }
    return val == 0;
}

static void ApplyThemeState(bool targetDark) {
    if (!SetThemeVariant(targetDark)) {
        SetTheme(targetDark ? "Dark" : "Light");
    }
    SaveSettings();
}

static bool DispatchToggleThemeFollowWindows() {
    EnsureHybridFollowWindowsThemeState();
    gHybridFollowWindowsTheme = !gHybridFollowWindowsTheme;

    if (gHybridFollowWindowsTheme) {
        bool windowsDark = WindowsPrefersDarkModeForHybrid();
        ApplyThemeState(windowsDark);
    } else {
        SaveSettings();
    }
    MainWindow* win = GetTargetWindow();
    if (win) {
        SyncHybridToolbarTheme(win->hwndFrame);
        SyncHomePageTheme(win->hwndFrame);
    }
    return true;
}

static bool DispatchToggleThemeLightDark() {
    EnsureHybridFollowWindowsThemeState();
    // Theme button always toggles only Light/Dark and exits follow-Windows mode.
    gHybridFollowWindowsTheme = false;

    bool appIsDark = DarkMode::isColorDark(ThemeWindowControlBackgroundColor());
    bool targetDark = !appIsDark;
    ApplyThemeState(targetDark);
    MainWindow* win = GetTargetWindow();
    if (win) {
        SyncHybridToolbarTheme(win->hwndFrame);
        SyncHomePageTheme(win->hwndFrame);
    }
    return true;
}

static bool DispatchToggleDocumentInvert() {
    MainWindow* win = GetTargetWindow();
    if (!win) {
        return false;
    }

    gGlobalPrefs->fixedPageUI.invertColors ^= true;
    UpdateDocumentColors();
    UpdateControlsColors(win);
    SaveSettings();
    SyncHybridToolbarTheme(win->hwndFrame);
    return true;
}

static bool DispatchSetDocumentInvertOn() {
    MainWindow* win = GetTargetWindow();
    if (!win) {
        return false;
    }

    if (!gGlobalPrefs->fixedPageUI.invertColors) {
        gGlobalPrefs->fixedPageUI.invertColors = true;
        UpdateDocumentColors();
        UpdateControlsColors(win);
        SaveSettings();
        SyncHybridToolbarTheme(win->hwndFrame);
    }
    return true;
}

static bool DispatchSetDocumentInvertOff() {
    MainWindow* win = GetTargetWindow();
    if (!win) {
        return false;
    }

    if (gGlobalPrefs->fixedPageUI.invertColors) {
        gGlobalPrefs->fixedPageUI.invertColors = false;
        UpdateDocumentColors();
        UpdateControlsColors(win);
        SaveSettings();
        SyncHybridToolbarTheme(win->hwndFrame);
    }
    return true;
}

static int ResolveBridgeCommandId(const char* commandName) {
    if (str::IsEmptyOrWhiteSpace(commandName)) {
        return 0;
    }
    struct CommandMapEntry {
        const char* name;
        int cmdId;
    };

    static constexpr CommandMapEntry kCommandMap[] = {
        {"commandPalette", CmdCommandPalette},       {"openFile", CmdOpenFile},
        {"properties", CmdProperties},               {"find", CmdFindFirst},
        {"newWindow", CmdNewWindow},                 {"saveAs", CmdSaveAs},
        {"reload", CmdReloadDocument},               {"reopenLastClosed", CmdReopenLastClosedFile},
        {"navigateBack", CmdNavigateBack},           {"navigateForward", CmdNavigateForward},
        {"prevPage", CmdGoToPrevPage},               {"nextPage", CmdGoToNextPage},
        {"firstPage", CmdGoToFirstPage},             {"lastPage", CmdGoToLastPage},
        {"zoomIn", CmdZoomIn},                       {"zoomOut", CmdZoomOut},
        {"fitWidth", CmdZoomFitWidth},               {"fitPage", CmdZoomFitPage},
        {"actualSize", CmdZoomActualSize},           {"singlePage", CmdSinglePageView},
        {"facing", CmdFacingView},                   {"bookView", CmdBookView},
        {"showPagesContinuously", CmdToggleContinuousView},
        {"toggleBookmarks", CmdToggleBookmarks},     {"toggleFavorites", CmdFavoriteToggle},
        {"toggleFullscreen", CmdToggleFullscreen},   {"rotateLeft", CmdRotateLeft},
        {"rotateRight", CmdRotateRight},             {"print", CmdPrint},
        {"highlightSelection", CmdCreateAnnotHighlight},
        {"addAnnotation", CmdCreateAnnotText},       {"freeDraw", CmdCreateAnnotInk},
        {"createAnnotUnderline", CmdCreateAnnotUnderline},
        {"createAnnotStrikeOut", CmdCreateAnnotStrikeOut},
    };

    for (size_t i = 0; i < dimof(kCommandMap); ++i) {
        if (str::EqI(commandName, kCommandMap[i].name)) {
            return kCommandMap[i].cmdId;
        }
    }
    return 0;
}

static bool DispatchExecCommand(const BridgeMessage& msg) {
    if (LogBridgeMessages()) {
        logf("[PrettySumatraBridge] execCommand received: command='%s' color='%s' hasCmdId=%d cmdId=%d\n",
             msg.command ? msg.command : "", msg.color ? msg.color : "", msg.hasCmdId ? 1 : 0,
             msg.hasCmdId ? msg.cmdId : 0);
    }
    MainWindow* win = GetTargetWindow();
    if (!win) {
        return false;
    }

    if (str::EqI(msg.command, "highlightSelection")) {
        return DispatchHighlightSelection(msg);
    }
    if (str::EqI(msg.command, "toggleTheme")) {
        return DispatchToggleThemeLightDark();
    }
    if (str::EqI(msg.command, "toggleThemeFollowWindows")) {
        return DispatchToggleThemeFollowWindows();
    }
    if (str::EqI(msg.command, "toggleDocumentInvert")) {
        return DispatchToggleDocumentInvert();
    }
    if (str::EqI(msg.command, "setDocumentInvertOn")) {
        return DispatchSetDocumentInvertOn();
    }
    if (str::EqI(msg.command, "setDocumentInvertOff")) {
        return DispatchSetDocumentInvertOff();
    }

    int cmdId = 0;
    if (msg.hasCmdId) {
        cmdId = msg.cmdId;
    } else {
        cmdId = ResolveBridgeCommandId(msg.command);
    }
    if (cmdId <= CmdFirst || cmdId >= CmdLast) {
        return false;
    }

    // For annotation commands that require a text selection, show a notification
    // if there's no selection (same UX as the built-in highlighter).
    if (cmdId == CmdCreateAnnotUnderline || cmdId == CmdCreateAnnotStrikeOut) {
        // If the bridge message included a color value, apply it to the pending color
        if (msg.color && str::StartsWith(msg.color, "#") && str::Len(msg.color) == 7) {
            if (cmdId == CmdCreateAnnotUnderline) {
                str::BufSet(gPendingUnderlineColor, dimof(gPendingUnderlineColor), msg.color);
            } else if (cmdId == CmdCreateAnnotStrikeOut) {
                str::BufSet(gPendingStrikeoutColor, dimof(gPendingStrikeoutColor), msg.color);
            }
        }
        SendMessageW(win->hwndFrame, WM_COMMAND, cmdId, 0);
        return true;
    }

    PostMessageW(win->hwndFrame, WM_COMMAND, cmdId, 0);
    return true;
}

static bool DispatchSetHighlightColor(const BridgeMessage& msg) {
    MainWindow* win = GetTargetWindow();
    if (!win) return false;

    // msg.color may contain a hex string like "#facc15" (from payload.value or payload.color)
    if (msg.color && str::StartsWith(msg.color, "#") && str::Len(msg.color) == 7) {
        str::BufSet(gPendingHighlightColor, dimof(gPendingHighlightColor), msg.color);
        return true;
    }

    // fallback: if payload.color contained a name, ignore for now
    return true;
}

static bool DispatchSetUnderlineColor(const BridgeMessage& msg) {
    MainWindow* win = GetTargetWindow();
    if (!win) return false;
    if (msg.color && str::StartsWith(msg.color, "#") && str::Len(msg.color) == 7) {
        str::BufSet(gPendingUnderlineColor, dimof(gPendingUnderlineColor), msg.color);
        return true;
    }
    return true;
}

static bool DispatchSetStrikeoutColor(const BridgeMessage& msg) {
    MainWindow* win = GetTargetWindow();
    if (!win) return false;
    if (msg.color && str::StartsWith(msg.color, "#") && str::Len(msg.color) == 7) {
        str::BufSet(gPendingStrikeoutColor, dimof(gPendingStrikeoutColor), msg.color);
        return true;
    }
    return true;
}

static bool DispatchToolbarReady() {
    MainWindow* win = GetTargetWindow();
    if (!win) {
        return false;
    }
    win->hybridToolbarSyncCtrl = nullptr;
    win->hybridToolbarSyncHasState = false;
    SyncHybridToolbarButtonVisibility(win->hwndFrame, !win->IsCurrentTabAbout());
    SyncHybridToolbarEditableAllowed(win->hwndFrame, !win->IsCurrentTabAbout());
    SyncHybridToolbarTheme(win->hwndFrame);
    if (win->ctrl) {
        SyncHybridToolbarState(win->hwndFrame, win->ctrl->CurrentPageNo(), win->ctrl->PageCount(),
                               win->ctrl->GetZoomVirtual(true), HybridToolbarCanAnnotate(win));
    } else {
        SyncHybridToolbarState(win->hwndFrame, 1, 1, 100.0f, HybridToolbarCanAnnotate(win));
    }
    return true;
}

static void AppendJsonEscapedChar(StrBuilder& json, unsigned char c) {
    switch (c) {
    case '"':
        json.Append("\\\"");
        return;
    case '\\':
        json.Append("\\\\");
        return;
    case '\b':
        json.Append("\\b");
        return;
    case '\f':
        json.Append("\\f");
        return;
    case '\n':
        json.Append("\\n");
        return;
    case '\r':
        json.Append("\\r");
        return;
    case '\t':
        json.Append("\\t");
        return;
    default:
        break;
    }
    if (c < 0x20) {
        static const char* kHex = "0123456789abcdef";
        json.Append("\\u00");
        json.AppendChar(kHex[(c >> 4) & 0x0f]);
        json.AppendChar(kHex[c & 0x0f]);
        return;
    }
    json.AppendChar((char)c);
}

static void AppendJsonString(StrBuilder& json, const char* src) {
    json.AppendChar('"');
    if (src) {
        const unsigned char* p = (const unsigned char*)src;
        while (*p) {
            if (*p < 0x80) {
                AppendJsonEscapedChar(json, *p++);
                continue;
            }

            int runeLen = utf8RuneLen(p);
            if (runeLen <= 0 || !isLegalUTF8Sequence(p, p + runeLen)) {
                json.Append("\\uFFFD");
                p++;
                continue;
            }

            if (runeLen == 3 && p[0] == 0xE2 && p[1] == 0x80 && p[2] == 0xA8) {
                json.Append("\\u2028");
                p += 3;
                continue;
            }
            if (runeLen == 3 && p[0] == 0xE2 && p[1] == 0x80 && p[2] == 0xA9) {
                json.Append("\\u2029");
                p += 3;
                continue;
            }
            for (int i = 0; i < runeLen; ++i) {
                AppendJsonEscapedChar(json, p[i]);
            }
            p += runeLen;
        }
    }
    json.AppendChar('"');
}

static constexpr int kHomePageMaxRecentItems = 40;

static u32 CalcRecentFilesSignature() {
    u32 sig = 0;
    for (int i = 0; i < kHomePageMaxRecentItems; i++) {
        FileState* fs = gFileHistory.Get(i);
        if (!fs || fs->isMissing) {
            break;
        }
        if (!fs->filePath) {
            continue;
        }
        sig ^= MurmurHash2(fs->filePath, str::Len(fs->filePath));
    }
    return sig;
}

// Helper function to serialize recent files to JSON for HomePage
static ::TempStr SerializeRecentFilesToJson() {
    StrBuilder json(4096);
    json.AppendChar('[');
    bool first = true;
    for (int i = 0; i < kHomePageMaxRecentItems; i++) {
        FileState* fs = gFileHistory.Get(i);
        if (!fs || fs->isMissing) {
            break;
        }
        if (!fs->filePath) {
            continue;
        }

        if (!first) {
            json.AppendChar(',');
        }
        first = false;

        // Extract filename from path
        const char* filePath = fs->filePath;
        const char* fileName = filePath;
        for (const char* p = filePath; *p; p++) {
            if (*p == '\\' || *p == '/') {
                fileName = p + 1;
            }
        }

        json.Append("{\"path\":");
        AppendJsonString(json, filePath);
        json.Append(",\"name\":");
        AppendJsonString(json, fileName);
        json.AppendChar('}');
    }

    json.AppendChar(']');
    return json.StealData();
}

// HomePage UI message handlers
static bool DispatchHomePageReady() {
    MainWindow* win = GetTargetWindow();
    if (!win || !win->homePageWebView) {
        return false;
    }

    static u32 sCachedRecentSignature = (u32)-1;
    static char* sCachedRecentFilesJson = nullptr;
    u32 recentSignature = CalcRecentFilesSignature();
    if (recentSignature != sCachedRecentSignature || !sCachedRecentFilesJson) {
        free(sCachedRecentFilesJson);
        sCachedRecentFilesJson = (char*)SerializeRecentFilesToJson();
        sCachedRecentSignature = recentSignature;
    }

    // Send recent files list to HomePage
    const char* recentFilesJson = sCachedRecentFilesJson ? sCachedRecentFilesJson : "[]";
    char* js = str::FormatTemp("window.setRecentFiles && window.setRecentFiles(%s);", recentFilesJson);
    win->homePageWebView->Eval(js);

    // Apply current theme with full color payload
    TempStr themeJs = HomePageThemeJs();
    if (themeJs) {
        win->homePageWebView->Eval(themeJs);
    }

    return true;
}

static bool DispatchOpenRecent(const BridgeMessage& msg) {
    if (!CanAccessDisk()) {
        return false;
    }
    MainWindow* win = GetTargetWindow();
    if (!win) {
        return false;
    }
    if (str::IsEmpty(msg.path)) {
        return false;
    }
    LoadArgs args(msg.path, win);
    LoadDocument(&args);
    return true;
}

static bool DispatchApplyTheme(const BridgeMessage& msg) {
    if (LogBridgeMessages()) {
        logf("[PrettySumatraBridge] theme changed by user\n");
    }
    return true;
}

static bool DispatchReopenLast() {
    char* path = PopRecentlyClosedDocument();
    if (!path) {
        return false;
    }
    MainWindow* win = GetTargetWindow();
    if (!win) {
        return false;
    }
    LoadArgs args(path, win);
    LoadDocument(&args);
    return true;
}

static bool DispatchKnownCommand(const BridgeMessage& msg) {
    if (str::Eq(msg.name, kOpenFile)) {
        return DispatchOpenFile(msg);
    }
    if (str::Eq(msg.name, kGoToPage)) {
        return DispatchGoToPage(msg);
    }
    if (str::Eq(msg.name, kZoom)) {
        return DispatchZoom(msg);
    }
    if (str::Eq(msg.name, kSetFitMode)) {
        return DispatchSetFitMode(msg);
    }
    if (str::Eq(msg.name, kSearch)) {
        return DispatchSearch(msg);
    }
    if (str::Eq(msg.name, kToggleSidebar)) {
        return DispatchToggleSidebar();
    }
    if (str::Eq(msg.name, kSetViewMode)) {
        return DispatchSetViewMode(msg);
    }
    if (str::Eq(msg.name, kExecCommand)) {
        return DispatchExecCommand(msg);
    }
    if (str::Eq(msg.name, "setHighlightColor")) {
        return DispatchSetHighlightColor(msg);
    }
    if (str::Eq(msg.name, kToolbarReady)) {
        return DispatchToolbarReady();
    }
    if (str::Eq(msg.name, "setUnderlineColor")) {
        return DispatchSetUnderlineColor(msg);
    }
    if (str::Eq(msg.name, "setStrikeoutColor") || str::Eq(msg.name, "setStrikeOutColor")) {
        return DispatchSetStrikeoutColor(msg);
    }
    if (str::EqI(msg.name, "print")) {
        return DispatchPrint();
    }
    if (str::Eq(msg.name, kHomePageReady)) {
        return DispatchHomePageReady();
    }
    if (str::Eq(msg.name, kOpenRecent)) {
        return DispatchOpenRecent(msg);
    }
    if (str::Eq(msg.name, kApplyTheme)) {
        return DispatchApplyTheme(msg);
    }
    if (str::Eq(msg.name, kReopenLast)) {
        return DispatchReopenLast();
    }

    if (str::Eq(msg.name, kAddAnnotation) || str::Eq(msg.name, kEditAnnotation) ||
        str::Eq(msg.name, kDeleteAnnotation) || str::Eq(msg.name, kExportAnnotations) ||
        str::Eq(msg.name, kImportAnnotations)) {
        if (LogBridgeMessages()) {
            logf("[PrettySumatraBridge] command '%s' not implemented yet\n", msg.name);
        }
        return true;
    }
    return false;
}

void SyncHomePageTheme(HWND hwndFrame) {
    MainWindow* win = FindWindowForFrame(hwndFrame);
    if (!win || !win->homePageWebView) {
        return;
    }
    TempStr js = HomePageThemeJs();
    if (js) {
        win->homePageWebView->Eval(js);
    }
}

DispatchResult DispatchShellMessage(const char* msg) {
    if (!UseHybridShell()) {
        return DispatchResult::Disabled;
    }
    if (LogBridgeMessages()) {
        logf("[PrettySumatraBridge] raw shell message: %s\n", msg);
    }
    if (str::IsEmptyOrWhiteSpace(msg)) {
        if (LogBridgeMessages()) {
            log("[PrettySumatraBridge] invalid bridge message: empty payload\n");
        }
        return DispatchResult::InvalidMessage;
    }

    BridgeMessage bridgeMsg;
    if (!ParseBridgeMessage(msg, bridgeMsg)) {
        if (LogBridgeMessages()) {
            logf("[PrettySumatraBridge] invalid command envelope: %s\n", msg);
        }
        return DispatchResult::InvalidMessage;
    }

    bool ok = DispatchKnownCommand(bridgeMsg);
    if (!ok) {
        if (LogBridgeMessages()) {
            logf("[PrettySumatraBridge] unknown command envelope: %s\n", msg);
        }
        return DispatchResult::UnknownCommand;
    }

    if (LogBridgeMessages()) {
        logf("[PrettySumatraBridge] accepted command '%s'\n", bridgeMsg.name);
    }
    return DispatchResult::Accepted;
}

} // namespace bridge
} // namespace prettysumatra
