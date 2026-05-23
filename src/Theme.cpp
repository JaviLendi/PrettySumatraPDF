/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "Settings.h"
#include "AppSettings.h"
#include "Commands.h"
#include "DisplayMode.h"
#include "Theme.h"
#include "GlobalPrefs.h"
#include "Translations.h"
#include "Toolbar.h"
#include "DarkModeSubclass.h"

#include "utils/Log.h"

// allow only x64 and arm64 for compatibility for older OS
#if !defined(_DARKMODELIB_NOT_USED) && \
    (defined(__x86_64__) || defined(_M_X64) || defined(__arm64__) || defined(__arm64) || defined(_M_ARM64))
bool gUseDarkModeLib = true;
#else
bool gUseDarkModeLib = false;
#endif

bool UseDarkModeLib() {
    return gUseDarkModeLib;
}

/*
preserve those translations:
_TRN("Dark")
_TRN("Darker")
_TRN("Light")
*/

constexpr COLORREF kColBlack = 0x000000;
constexpr COLORREF kColWhite = 0xFFFFFF;
constexpr COLORREF kRedColor = RgbToCOLORREF(0xff0000);

static const char* themesTxt = R"(Themes [
    [
        Name = Sumatra Light
        TextColor = #1f2937
        BackgroundColor = #f4f6f8
        ControlBackgroundColor = #ffffff
        LinkColor = #eab308
        AccentColor = #eab308
        BrandPrimaryColor = #facc15
        BrandGlowColor = #fde68a
        ShadowColor = #5b4210
        ColorizeControls = true
    ]
    [
        Name = Sumatra Dark
        TextColor = #f8fafc
        BackgroundColor = #17130a
        ControlBackgroundColor = #3a2901
        LinkColor = #eab308
        AccentColor = #eab308
        BrandPrimaryColor = #facc15
        BrandGlowColor = #fde68a
        ShadowColor = #120b02
        ColorizeControls = true
    ]
    [
        Name = Modern Slate Light
        TextColor = #243041
        BackgroundColor = #f4f6f8
        ControlBackgroundColor = #e0f0ee
        LinkColor = #0f766e
        AccentColor = #0f766e
        BrandPrimaryColor = #14b8a6
        BrandGlowColor = #99f6e4
        ShadowColor = #1e293b
        ColorizeControls = true
    ]
    [
        Name = Modern Slate Dark
        TextColor = #dbe4ee
        BackgroundColor = #0f172a
        ControlBackgroundColor = #05557a
        LinkColor = #38bdf8
        AccentColor = #38bdf8
        BrandPrimaryColor = #0ea5e9
        BrandGlowColor = #7dd3fc
        ShadowColor = #020617
        ColorizeControls = true
    ]
    [
        Name = Modern Blue Light
        TextColor = #1e293b
        BackgroundColor = #f8fafc
        ControlBackgroundColor = #daebff
        LinkColor = #0f62fe
        AccentColor = #0f62fe
        BrandPrimaryColor = #2563eb
        BrandGlowColor = #93c5fd
        ShadowColor = #1e293b
        ColorizeControls = true
    ]
    [
        Name = Modern Blue Dark
        TextColor = #e2e8f0
        BackgroundColor = #0b1120
        ControlBackgroundColor = #1c3d74
        LinkColor = #60a5fa
        AccentColor = #60a5fa
        BrandPrimaryColor = #3b82f6
        BrandGlowColor = #93c5fd
        ShadowColor = #020617
        ColorizeControls = true
    ]
    [
        Name = Modern Green Light
        TextColor = #1f2937
        BackgroundColor = #f4f6f8
        ControlBackgroundColor = #d1fae5
        LinkColor = #16a34a
        AccentColor = #16a34a
        BrandPrimaryColor = #22c55e
        BrandGlowColor = #86efac
        ShadowColor = #1f2937
        ColorizeControls = true
    ]
    [
        Name = Modern Green Dark
        TextColor = #e5f3ea
        BackgroundColor = #10251b
        ControlBackgroundColor = #0f5529
        LinkColor = #4ade80
        AccentColor = #4ade80
        BrandPrimaryColor = #22c55e
        BrandGlowColor = #86efac
        ShadowColor = #052e16
        ColorizeControls = true
    ]
    [
        Name = Modern Purple Light
        TextColor = #29223a
        BackgroundColor = #f4f6f8
        ControlBackgroundColor = #f3e8ff
        LinkColor = #7c3aed
        AccentColor = #7c3aed
        BrandPrimaryColor = #8b5cf6
        BrandGlowColor = #c4b5fd
        ShadowColor = #29223a
        ColorizeControls = true
    ]
    [
        Name = Modern Purple Dark
        TextColor = #ede9fe
        BackgroundColor = #1c122b
        ControlBackgroundColor = rgb(78, 69, 88)
        LinkColor = #c084fc
        AccentColor = #c084fc
        BrandPrimaryColor = #a855f7
        BrandGlowColor = #ddd6fe
        ShadowColor = #15051f
        ColorizeControls = true
    ]
    [
        Name = Modern Amber Light
        TextColor = #31251a
        BackgroundColor = #f4f6f8
        ControlBackgroundColor = #fff7ed
        LinkColor = #d97706
        AccentColor = #d97706
        BrandPrimaryColor = #f59e0b
        BrandGlowColor = #fde68a
        ShadowColor = #31251a
        ColorizeControls = true
    ]
    [
        Name = Modern Amber Dark
        TextColor = #f9ede1
        BackgroundColor = #23160d
        ControlBackgroundColor = #634106
        LinkColor = #fbbf24
        AccentColor = #fbbf24
        BrandPrimaryColor = #f59e0b
        BrandGlowColor = #fde68a
        ShadowColor = #120b02
        ColorizeControls = true
    ]
]
)";

extern void UpdateAfterThemeChange();

int gFirstSetThemeCmdId;
int gLastSetThemeCmdId;
int gCurrSetThemeCmdId;

static Vec<Theme*>* gThemes = nullptr;
static int gThemeCount;
static int gCurrThemeIndex = 0;
static Theme* gCurrentTheme = nullptr;
static Theme* gThemeLight = nullptr;
static Themes* gParsedThemes = nullptr;

static bool IsLightThemeVariantName(const char* name) {
    if (str::IsEmpty(name)) {
        return false;
    }
    return str::EqI(name, "Light") || str::EndsWithI(name, " Light");
}

static int GetThemeIndexByName(const char* name) {
    for (int i = 0; i < gThemeCount; i++) {
        Theme* theme = gThemes->At(i);
        if (str::EqI(theme->name, name)) {
            return i;
        }
    }
    return -1;
}

static int GetThemeVariantIndex(const char* currentName, bool targetDark) {
    if (str::IsEmpty(currentName)) {
        return -1;
    }

    if (str::EqI(currentName, "Light")) {
        return targetDark ? GetThemeIndexByName("Dark") : GetThemeIndexByName("Light");
    }
    if (str::EqI(currentName, "Dark")) {
        return targetDark ? GetThemeIndexByName("Dark") : GetThemeIndexByName("Light");
    }

    const char* lightSuffix = " Light";
    const char* darkSuffix = " Dark";
    if (!str::EndsWithI(currentName, lightSuffix) && !str::EndsWithI(currentName, darkSuffix)) {
        return GetThemeIndexByName(targetDark ? "Dark" : "Light");
    }

    const int suffixLen = (int)str::Len(str::EndsWithI(currentName, lightSuffix) ? lightSuffix : darkSuffix);
    const int nameLen = (int)str::Len(currentName);
    const int baseLen = nameLen - suffixLen;
    if (baseLen <= 0 || baseLen >= 256) {
        return GetThemeIndexByName(targetDark ? "Dark" : "Light");
    }

    char base[256];
    memcpy(base, currentName, baseLen);
    base[baseLen] = '\0';

    char targetName[256];
    if (!str::BufFmt(targetName, dimof(targetName), "%s %s", base, targetDark ? "Dark" : "Light")) {
        return GetThemeIndexByName(targetDark ? "Dark" : "Light");
    }
    return GetThemeIndexByName(targetName);
}

bool IsCurrentThemeDefault() {
    return gCurrThemeIndex == 0;
}

bool PrettyStyleEnabled() {
    return true;
}

COLORREF PrettySurfaceColor() {
    return ThemeWindowBackgroundColor();
}

COLORREF PrettySurfaceAltColor() {
    return ThemeWindowControlBackgroundColor();
}

COLORREF PrettyBorderColor() {
    // Make the border slightly darker than the window background so it stays subtle
    // but still separates the toolbar from the canvas in both light and dark themes.
    return AdjustLightness2(ThemeWindowControlBackgroundColor(), -10);
}

COLORREF PrettyAccentColor() {
    return ThemeAccentColor();
}

void FreeThemes() {
    delete gThemes; // no need to free members, they are owned by gParsedThemes
    gThemes = nullptr;
    FreeParsedThemes(gParsedThemes);
    gParsedThemes = nullptr;
}

void CreateThemeCommands() {
    FreeThemes();

    gThemes = new Vec<Theme*>();
    gParsedThemes = ParseThemes(themesTxt);
    for (Theme* theme : *gParsedThemes->themes) {
        gThemes->Append(theme);
    }

    for (Theme* theme : *gGlobalPrefs->themes) {
        gThemes->Append(theme);
    }

    gThemeCount = gThemes->Size();
    if (gCurrThemeIndex >= gThemeCount) {
        gCurrThemeIndex = 0;
    }
    gCurrentTheme = gThemes->At(gCurrThemeIndex);
    gThemeLight = gThemes->At(0);

    CustomCommand* cmd;
    for (int i = 0; i < gThemeCount; i++) {
        Theme* theme = gThemes->At(i);
        const char* themeName = theme->name;
        auto args = NewStringArg(kCmdArgTheme, themeName);
        cmd = CreateCustomCommand(themeName, CmdSetTheme, args);
        cmd->name = str::Format(_TRA("Set theme '%s'"), themeName);
        if (i == 0) {
            gFirstSetThemeCmdId = cmd->id;
        } else if (i == gThemeCount - 1) {
            gLastSetThemeCmdId = cmd->id;
        }
    }
    gCurrSetThemeCmdId = gFirstSetThemeCmdId + gCurrThemeIndex;
}

void SetThemeByIndex(int themeIdx) {
    ReportIf((themeIdx < 0) || (themeIdx >= gThemeCount));
    if (themeIdx < 0) {
        themeIdx = 0;
    }
    if (themeIdx >= gThemeCount) {
        themeIdx = 0;
    }
    bool themeChanged = (gCurrThemeIndex != themeIdx);
    gCurrThemeIndex = themeIdx;
    gCurrSetThemeCmdId = gFirstSetThemeCmdId + themeIdx;
    gCurrentTheme = gThemes->At(gCurrThemeIndex);
    str::ReplaceWithCopy(&gGlobalPrefs->theme, gCurrentTheme->name);
    if (UseDarkModeLib()) {
        // TODO: we should apply themes to every theme other than 0
        // but in Solarized Light in Find dialog's input field text is invisible i.e. black
        // UINT mode = themeIdx == 0 ? kModeClassic : kModeDark;
        const bool isDarkCol = DarkMode::isColorDark(ThemeWindowControlBackgroundColor());
        const UINT mode = static_cast<UINT>(isDarkCol         ? DarkMode::DarkModeType::dark
                                            : (themeIdx == 0) ? DarkMode::DarkModeType::classic
                                                              : DarkMode::DarkModeType::light);
        DarkMode::setDarkModeConfigEx(mode);
        DarkMode::setDefaultColors(false);

        DarkMode::setBackgroundColor(ThemeWindowBackgroundColor());
        DarkMode::setCtrlBackgroundColor(ThemeWindowControlBackgroundColor());
        COLORREF ctrlBg = ThemeWindowControlBackgroundColor();
        COLORREF hotBg = AccentColor(ctrlBg, 20);
        COLORREF edgeCol = AccentColor(ctrlBg, 40);
        DarkMode::setHotBackgroundColor(hotBg);
        DarkMode::setTextColor(ThemeWindowTextColor());
        DarkMode::setDisabledTextColor(ThemeWindowTextDisabledColor());
        DarkMode::setDlgBackgroundColor(ctrlBg);
        DarkMode::setLinkTextColor(ThemeWindowLinkColor());
        DarkMode::setEdgeColor(edgeCol);
        DarkMode::updateThemeBrushesAndPens();

        DarkMode::setViewTextColor(ThemeWindowTextColor());
        DarkMode::setViewBackgroundColor(ThemeWindowControlBackgroundColor());
        DarkMode::calculateTreeViewStyle();

        if (themeChanged) {
            UpdateAfterThemeChange();
        }

        DarkMode::setPrevTreeViewStyle();
    } else {
        if (themeChanged) {
            UpdateAfterThemeChange();
        }
    }
};

void SelectNextTheme() {
    int newIdx = (gCurrThemeIndex + 1) % gThemeCount;
    SetThemeByIndex(newIdx);
}

bool SetThemeVariant(bool targetDark) {
    if (gThemeCount <= 0 || gCurrentTheme == nullptr) {
        return false;
    }
    int idx = GetThemeVariantIndex(gCurrentTheme->name, targetDark);
    if (idx < 0) {
        return false;
    }
    SetThemeByIndex(idx);
    return true;
}

// not case sensitive
static int GetThemeByName(const char* name) {
    return GetThemeIndexByName(name);
}

// this is the default aggressive yellow that we suppress
constexpr COLORREF kMainWinBgColDefault = (RGB(0xff, 0xf2, 0) - 0x80000000);

static bool IsDefaultMainWinColor(ParsedColor* col) {
    return col->parsedOk && col->col == kMainWinBgColDefault;
}

void SetTheme(const char* name) {
    int idx = GetThemeByName(name);
    if (idx < 0) {
        // invalid name, reset to light theme
        str::ReplaceWithCopy(&gGlobalPrefs->theme, gThemeLight->name);
        idx = 0;
    }
    SetThemeByIndex(idx);
}

// call after loading settings
void SetCurrentThemeFromSettings() {
    SetTheme(gGlobalPrefs->theme);
    ParsedColor* bgParsed = GetPrefsColor(gGlobalPrefs->mainWindowBackground);
    bool isDefault = IsDefaultMainWinColor(bgParsed);
    if (isDefault) {
        gThemeLight->colorizeControls = false;
        gThemeLight->controlBackgroundColorParsed.col = kColWhite;
    } else {
        gThemeLight->colorizeControls = true;
        gThemeLight->controlBackgroundColorParsed.col = bgParsed->col;
    }
}

COLORREF AccentColor(COLORREF col, int light, int dark) {
    if (dark == 0) {
        dark = light;
    }
    if (IsLightColor(col)) {
        return AdjustLightness2(col, -light);
    }
    return AdjustLightness2(col, dark);
}

#define GetThemeCol(name, def) GetParsedCOLORREF(name, name##Parsed, def)

// canvas/window background color around the document pages
// not affected by FixedPageUI.TextColor/BackgroundColor (those affect page rendering)
COLORREF ThemeDocumentColors(COLORREF& bg) {
    bg = ThemeMainWindowBackgroundColor();

    if (!gGlobalPrefs->fixedPageUI.invertColors) {
        return ThemeWindowTextColor();
    }

    COLORREF text = ThemeWindowTextColor();
    bg = ThemeMainWindowBackgroundColor();

    if (IsLightThemeVariantName(gCurrentTheme->name)) {
        bg = AccentColor(bg, 8);
    }
    return text;
}

// colors for page bitmap recoloring (render cache)
// TextColor substitutes black, BackgroundColor substitutes white in rendered pages
COLORREF ThemePageRenderColors(COLORREF& bg) {
    COLORREF text = kColBlack;
    bg = kColWhite;

    ParsedColor* parsedCol;
    parsedCol = GetPrefsColor(gGlobalPrefs->fixedPageUI.textColor);
    if (parsedCol->parsedOk) {
        text = parsedCol->col;
    }

    parsedCol = GetPrefsColor(gGlobalPrefs->fixedPageUI.backgroundColor);
    if (parsedCol->parsedOk) {
        bg = parsedCol->col;
    }

    if (!gGlobalPrefs->fixedPageUI.invertColors) {
        return text;
    }

    // if user did change those colors in advanced settings, respect them
    bool userDidChange = text != kColBlack || bg != kColWhite;
    if (userDidChange) {
        std::swap(text, bg);
        return text;
    }

    // default colors
    if (IsLightThemeVariantName(gCurrentTheme->name)) {
        std::swap(text, bg);
        return text;
    }

    // if we're inverting in non-default themes, the colors
    // should match the colors of the window
    text = ThemeWindowTextColor();
    bg = ThemeMainWindowBackgroundColor();

    if (IsLightThemeVariantName(gCurrentTheme->name)) {
        bg = AccentColor(bg, 8);
    }
    return text;
}

COLORREF ThemeControlBackgroundColor() {
    // note: we can change it in ThemeUpdateAfterLoadSettings()
    auto col = GetThemeCol(gCurrentTheme->controlBackgroundColor, kRedColor);
    return col;
}

COLORREF ThemeMainWindowBackgroundColor() {
    COLORREF bgColor = GetThemeCol(gCurrentTheme->backgroundColor, kRedColor);
    if (gCurrThemeIndex == 0) {
        // Special behavior for light theme.
        ParsedColor* bgParsed = GetPrefsColor(gGlobalPrefs->mainWindowBackground);
        if (!IsDefaultMainWinColor(bgParsed)) {
            bgColor = bgParsed->col;
        }
    }
    return bgColor;
}

COLORREF ThemeWindowBackgroundColor() {
    auto col = GetThemeCol(gCurrentTheme->backgroundColor, kRedColor);
    return col;
}

COLORREF ThemeWindowTextColor() {
    auto col = GetThemeCol(gCurrentTheme->textColor, kRedColor);
    return col;
}

COLORREF ThemeWindowTextDisabledColor() {
    // blend text color halfway toward background so disabled text
    // is visible but clearly muted on both light and dark themes
    COLORREF txt = ThemeWindowTextColor();
    COLORREF bg = ThemeMainWindowBackgroundColor();
    u8 r = (u8)((GetRValue(txt) + GetRValue(bg)) / 2);
    u8 g = (u8)((GetGValue(txt) + GetGValue(bg)) / 2);
    u8 b = (u8)((GetBValue(txt) + GetBValue(bg)) / 2);
    return RGB(r, g, b);
}

COLORREF ThemeWindowControlBackgroundColor() {
    auto col = GetThemeCol(gCurrentTheme->controlBackgroundColor, kRedColor);
    return col;
}

COLORREF ThemeWindowLinkColor() {
    auto col = GetThemeCol(gCurrentTheme->linkColor, kRedColor);
    return col;
}

COLORREF ThemeAccentColor() {
    auto col = GetThemeCol(gCurrentTheme->accentColor, ThemeWindowLinkColor());
    return col;
}

COLORREF ThemeBrandPrimaryColor() {
    auto col = GetThemeCol(gCurrentTheme->brandPrimaryColor, ThemeAccentColor());
    return col;
}

COLORREF ThemeBrandGlowColor() {
    auto col = GetThemeCol(gCurrentTheme->brandGlowColor, ThemeBrandPrimaryColor());
    return col;
}

COLORREF ThemeShadowColor() {
    auto col = GetThemeCol(gCurrentTheme->shadowColor, kColBlack);
    return col;
}

COLORREF ThemeNotificationsBackgroundColor() {
    auto col = ThemeWindowBackgroundColor();
    return AdjustLightness2(col, 10);
}

COLORREF ThemeNotificationsTextColor() {
    return ThemeWindowTextColor();
}

COLORREF ThemeNotificationsProgressColor() {
    return ThemeAccentColor();
}

// Highlight color used for notifications (background)
COLORREF ThemeNotificationsHighlightColor() {
    // Use accent color brightened a bit for highlight background
    COLORREF base = ThemeAccentColor();
    return AdjustLightness2(base, 24);
}

// Text color to use on top of notification highlight background
COLORREF ThemeNotificationsHighlightTextColor() {
    COLORREF bg = ThemeNotificationsHighlightColor();
    return IsLightColor(bg) ? kColBlack : kColWhite;
}

bool ThemeColorizeControls() {
    if (gCurrentTheme->colorizeControls) {
        return true;
    }
    return !IsMenuFontSizeDefault();
}

#if 0
void dumpThemes() {
    logf("Themes [\n");
    for (ThemeOld* theme : gThemes) {
        auto w = *theme;
        logf("    [\n");
        logf("        Name = %s\n", w.name);
        logf("        TextColor = %s\n", SerializeColorTemp(w.textColor));
        logf("        BackgroundColor = %s\n", SerializeColorTemp(w.backgroundColor));
        logf("        ControlBackgroundColor = %s\n", SerializeColorTemp(w.controlBackgroundColor));
        logf("        LinkColor = %s\n", SerializeColorTemp(w.linkColor));
        logf("        ColorizeControls = %s\n", w.colorizeControls ? "true" : "false");
        logf("    ]\n");
    }
    logf("]\n");
}
#endif
