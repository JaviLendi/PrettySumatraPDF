/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
License: GPLv3 */

void SetTheme(const char* name);
void SetCurrentThemeFromSettings();
bool SetThemeVariant(bool targetDark);
void SelectNextTheme();
void CreateThemeCommands();

COLORREF ThemeDocumentColors(COLORREF&);
COLORREF ThemePageRenderColors(COLORREF&);
COLORREF ThemeMainWindowBackgroundColor();
COLORREF ThemeControlBackgroundColor();
COLORREF ThemeWindowBackgroundColor();
COLORREF ThemeWindowTextColor();
COLORREF ThemeWindowTextDisabledColor();
COLORREF ThemeWindowControlBackgroundColor();
COLORREF ThemeWindowLinkColor();
COLORREF ThemeAccentColor();
COLORREF ThemeBrandPrimaryColor();
COLORREF ThemeBrandGlowColor();
COLORREF ThemeShadowColor();
COLORREF ThemeNotificationsBackgroundColor();
COLORREF ThemeNotificationsTextColor();
COLORREF ThemeNotificationsHighlightColor();
COLORREF ThemeNotificationsHighlightTextColor();
COLORREF ThemeNotificationsProgressColor();
bool ThemeColorizeControls();
bool IsCurrentThemeDefault();
bool PrettyStyleEnabled();
COLORREF PrettySurfaceColor();
COLORREF PrettySurfaceAltColor();
COLORREF PrettyBorderColor();
COLORREF PrettyAccentColor();
COLORREF AccentColor(COLORREF col, int light, int dark = 0);
void FreeThemes();
bool UseDarkModeLib();

extern int gFirstSetThemeCmdId;
extern int gLastSetThemeCmdId;
extern int gCurrSetThemeCmdId;
