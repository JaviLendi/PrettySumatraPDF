#pragma once

namespace prettysumatra {
namespace bridge {

enum class DispatchResult {
    Disabled,
    InvalidMessage,
    UnknownCommand,
    Accepted,
};

bool UseHybridShell();
bool UseHybridToolbar();
bool UseHybridSidebar();
bool LogBridgeMessages();
bool HybridThemeFollowsWindows();
bool HasHybridToolbar(HWND hwndFrame);
void InitHybridToolbarTheme(HWND hwndFrame);
void SyncHybridToolbarTheme(HWND hwndFrame);
void SyncHomePageTheme(HWND hwndFrame);
void InitHybridToolbarText(HWND hwndFrame);
void SyncHybridToolbarText(HWND hwndFrame);
void SyncHybridToolbarSearchText(HWND hwndFrame, const char* text);
void FocusHybridToolbarSearch(HWND hwndFrame);
void SyncHybridToolbarButtonVisibility(HWND hwndFrame, bool showButtons);
void SyncHybridToolbarEditableAllowed(HWND hwndFrame, bool allowed);
void SyncHybridToolbarPageState(HWND hwndFrame, int currentPage, int totalPages);
void SyncHybridToolbarZoomState(HWND hwndFrame, float zoomPercent);
void SyncHybridToolbarAnnotationAvailability(HWND hwndFrame);

DispatchResult DispatchShellMessage(const char* msg);

// Get the pending highlight color from bridge (used for passing color from webui)
const char* GetPendingHighlightColor();
// Get pending colors for underline and strikeout (from webui color chips)
const char* GetPendingUnderlineColor();
const char* GetPendingStrikeoutColor();

} // namespace bridge
} // namespace prettysumatra
