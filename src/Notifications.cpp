/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"
#include "utils/ScopedWin.h"
#include "utils/Dpi.h"
#include "utils/UITask.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "AppSettings.h"
#include "Annotation.h"
#include "SumatraPdf.h"
#include "AppTools.h"

#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "Theme.h"

#include "utils/Log.h"

using Gdiplus::Graphics;
using Gdiplus::GraphicsPath;
using Gdiplus::Pen;
using Gdiplus::SolidBrush;

// defined in MainWindow.cpp
HWND GetHwndForNotification();

struct StrNode {
    StrNode* next;
    const char* s;
};

static StrNode* gDelayedNotifications = nullptr;

Kind kNotifCursorPos = "cursorPosHelper";
Kind kNotifActionResponse = "responseToAction";
Kind kNotifPageInfo = "pageInfoHelper";
// can have multiple of those
Kind kNotifAdHoc = "notifAdHoc";

constexpr int kPadding = 6;
constexpr int kTopLeftMargin = 8;
constexpr int kNotifRadius = 10;
constexpr int kNotifBorder = 1;
constexpr int kNotifAccentHeight = 4;
constexpr int kNotifCloseInset = 4;
constexpr int kNotifCloseSize = 18;
constexpr int kNotifProgressHeight = 4;
constexpr int kNotifTextPadX = 14;
constexpr int kNotifTextPadY = 11;
constexpr int kNotifGapY = 8;

static void AddRoundedRect(GraphicsPath& path, const Rect& rc, int radius) {
    int d = radius * 2;
    if (radius <= 0 || rc.dx <= d || rc.dy <= d) {
        path.AddRectangle(Gdiplus::Rect(rc.x, rc.y, rc.dx, rc.dy));
        return;
    }
    path.StartFigure();
    path.AddArc(rc.x, rc.y, d, d, 180, 90);
    path.AddLine(rc.x + radius, rc.y, rc.x + rc.dx - radius, rc.y);
    path.AddArc(rc.x + rc.dx - d, rc.y, d, d, 270, 90);
    path.AddLine(rc.x + rc.dx, rc.y + radius, rc.x + rc.dx, rc.y + rc.dy - radius);
    path.AddArc(rc.x + rc.dx - d, rc.y + rc.dy - d, d, d, 0, 90);
    path.AddLine(rc.x + rc.dx - radius, rc.y + rc.dy, rc.x + radius, rc.y + rc.dy);
    path.AddArc(rc.x, rc.y + rc.dy - d, d, d, 90, 90);
    path.AddLine(rc.x, rc.y + rc.dy - radius, rc.x, rc.y + radius);
    path.CloseFigure();
}

constexpr UINT_PTR kNotifTimerTimeoutId = 1;
constexpr UINT_PTR kNotifTimerDelayId = 2;

struct NotificationWnd : Wnd {
    NotificationWnd() = default;
    ~NotificationWnd() override;

    HWND Create(const NotificationCreateArgs&);

    void OnPaint(HDC hdc, PAINTSTRUCT* ps) override;
    void OnTimer(UINT_PTR event_id) override;
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;

    void UpdateMessage(const char* msg, int timeoutMs = 0, bool highlight = false);

    bool HasProgress() const { return progressPerc >= 0; }
    void Layout(const char* message);

    int timeoutMs = kNotifDefaultTimeOut; // 0 means no timeout

    bool highlight = false; // TODO: should really be a color
    bool noClose = false;

    NotificationWndRemoved wndRemovedCb;

    // there can only be a single notification of a given group
    Kind groupId = nullptr;

    // to reduce flicker, we might ask the window to shrink the size less often
    // (notifcation windows are only shrunken if by less than factor shrinkLimit)
    float shrinkLimit = 1.0f;

    int progressPerc = -1;
    int delayInMs = 0;
    UINT_PTR delayTimerId = 0;

    Rect rTxt;
    Rect rClose;
    Rect rProgress;
};

constexpr int kMaxNotifs = 128;
static NotificationWnd* gNotifs[kMaxNotifs];
static int gNotifsCount = 0;

static int GetForHwnd(HWND hwnd, NotificationWnd* wnds[kMaxNotifs]) {
    int n = 0;
    for (int i = 0; i < gNotifsCount; i++) {
        auto* wnd = gNotifs[i];
        HWND parent = HwndGetParent(wnd->hwnd);
        if (parent == hwnd) {
            wnds[n++] = wnd;
        }
    }
    return n;
}

static int NotificationIndexOf(NotificationWnd* wnd) {
    for (int i = 0; i < gNotifsCount; i++) {
        if (gNotifs[i] == wnd) {
            return i;
        }
    }
    return -1;
}

static int GetForSameHwnd(NotificationWnd* wnd, NotificationWnd* wnds[kMaxNotifs]) {
    HWND parent = GetParent(wnd->hwnd);
    return GetForHwnd(parent, wnds);
}

void RelayoutNotifications(HWND hwnd) {
    NotificationWnd* wnds[kMaxNotifs];
    HWND parent = HwndGetParent(hwnd);
    int nWnds = GetForHwnd(parent, wnds);
    if (nWnds == 0) {
        return;
    }

    auto* first = wnds[0];
    HWND hwndCanvas = GetParent(first->hwnd);
    Rect frame = ClientRect(hwndCanvas);
    int topLeftMargin = DpiScale(hwndCanvas, kTopLeftMargin);
    int dyPadding = DpiScale(hwndCanvas, kPadding);
    int y = topLeftMargin;
    for (int i = 0; i < nWnds; i++) {
        NotificationWnd* wnd = wnds[i];
        if (wnd->delayTimerId != 0) {
            // still in delay period, not yet visible
            continue;
        }
        Rect rect = WindowRect(wnd->hwnd);
        rect = MapRectToWindow(rect, HWND_DESKTOP, hwndCanvas);
        if (IsUIRtl()) {
            int cxVScroll = GetSystemMetrics(SM_CXVSCROLL);
            rect.x = frame.dx - rect.dx - topLeftMargin - cxVScroll;
        } else {
            rect.x = topLeftMargin;
        }
        uint flags = SWP_NOSIZE | SWP_NOZORDER;
        SetWindowPos(wnd->hwnd, nullptr, rect.x, y, 0, 0, flags);
        y += rect.dy + dyPadding;
    }
}

static void NotifsRemoveNotification(NotificationWnd* wnd) {
    int pos = NotificationIndexOf(wnd);
    if (pos < 0) {
        return;
    }
    gNotifsCount--;
    if (pos < gNotifsCount) {
        gNotifs[pos] = gNotifs[gNotifsCount];
    }
    gNotifs[gNotifsCount] = nullptr;
    RelayoutNotifications(wnd->hwnd);
    delete wnd;
}

int GetWndX(NotificationWnd* wnd) {
    Rect rect = WindowRect(wnd->hwnd);
    rect = MapRectToWindow(rect, HWND_DESKTOP, GetParent(wnd->hwnd));
    return rect.x;
}

NotificationWnd::~NotificationWnd() {
    if (delayTimerId != 0) {
        KillTimer(hwnd, delayTimerId);
        delayTimerId = 0;
    }
}

HWND NotificationWnd::Create(const NotificationCreateArgs& args) {
    highlight = args.warning;
    noClose = args.noClose;
    ReportIf(noClose && args.timeoutMs <= 0);
    shrinkLimit = args.shrinkLimit;
    if (shrinkLimit < 0.2f) {
        ReportIf(shrinkLimit < 0.2f);
        shrinkLimit = 1.f;
    }
    if (args.onRemoved.IsValid()) {
        wndRemovedCb = args.onRemoved;
    } else {
        wndRemovedCb = MkFunc1Void(NotifsRemoveNotification);
    }
    timeoutMs = args.timeoutMs;

    CreateCustomArgs cargs;
    cargs.parent = args.hwndParent;
    cargs.font = args.font;
    // TODO: was this important?
    // wcex.hCursor = LoadCursor(nullptr, IDC_APPSTARTING);
    cargs.exStyle = WS_EX_TOPMOST;
    cargs.style = WS_CHILD | SS_CENTER;
    cargs.title = args.msg;
    if (cargs.font == nullptr) {
        cargs.font = GetAppBiggerFont();
    }
    cargs.pos = Rect(0, 0, 0, 0);
    cargs.visible = args.delayInMs == 0;
    cargs.isRtl = IsUIRtl();

    CreateCustom(cargs);
    if (!hwnd) {
        return nullptr;
    }

    Layout(args.msg);
    delayInMs = args.delayInMs;
    if (delayInMs > 0) {
        // create hidden, will show after delay
        delayTimerId = SetTimer(hwnd, kNotifTimerDelayId, delayInMs, nullptr);
    } else {
        ShowWindow(hwnd, SW_SHOW);
        if (timeoutMs != 0) {
            SetTimer(hwnd, kNotifTimerTimeoutId, timeoutMs, nullptr);
        }
    }
    return hwnd;
}

// returns 0% - 100%
int CalcPerc(int current, int total) {
    ReportIf(total <= 0 || current < 0);
    ReportIf(total < current);
    if (total <= 0) {
        total = 1;
    }
    int perc = limitValue(100 * current / total, 0, 100);
    return perc;
}

constexpr int kCloseLeftMargin = 16;

void NotificationWnd::Layout(const char* message) {
    Size szText;
    {
        HDC hdc = GetDC(hwnd);
        uint fmt = DT_SINGLELINE | DT_NOPREFIX;
        szText = HdcMeasureText(hdc, message, fmt, font);
        ReleaseDC(hwnd, hdc);
    }

    int padX = DpiScale(hwnd, kNotifTextPadX);
    int padY = DpiScale(hwnd, kNotifTextPadY);
    int accentWidth = DpiScale(hwnd, 5);
    int closeDx = DpiScale(hwnd, kNotifCloseSize);
    int closeInset = DpiScale(hwnd, kNotifCloseInset);
    int dx = padX + accentWidth + padX + szText.dx + padX;
    int dy = padY + szText.dy + padY;
    rTxt = {padX + accentWidth + padX, padY, szText.dx, szText.dy};
    if (!noClose) {
        int closeSpace = DpiScale(hwnd, kCloseLeftMargin);
        rClose = {dx + closeSpace - closeDx, padY + closeInset, closeDx, closeDx};
        dx += closeSpace + closeDx + padX;
    } else {
        rClose = {};
        dx += padX;
    }
    int progressDy = DpiScale(hwnd, kNotifProgressHeight);
    rProgress = {rTxt.x, dy, szText.dx, progressDy};
    if (HasProgress()) {
        dy += DpiScale(hwnd, kNotifGapY) + progressDy + padY;
    }

    Rect rCurr = WindowRect(hwnd);
    // for less flicker we don't want to shrink the window when the text shrinks
    if (dx < rCurr.dx) {
        int diff = rCurr.dx - dx;
        rClose.x += diff;
        dx = rCurr.dx;
    }
#if 0
    if (dy < rCurr.dy) {
        dy = rCurr.dy;
    }
#endif
#if 0
    if (wnd->shrinkLimit < 1.0f) {
        Rect rcOrig = ClientRect(wnd->hwnd);
        if (rMsg.dx < rcOrig.dx && rMsg.dx > rcOrig.dx * wnd->shrinkLimit) {
            rMsg.dx = rcOrig.dx;
        }
    }
#endif

    // y-center close
    if (!noClose) {
        rClose.y = ((dy - rClose.dy) / 2);
    }

    if (dx == rCurr.dx && dy == rCurr.dy) {
        return;
    }

    // adjust the window to fit the message (only shrink the window when there's no progress bar)
    uint flags = SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE;
    SetWindowPos(hwnd, nullptr, 0, 0, dx, dy, flags);

    // move the window to the right for a right-to-left layout
    if (IsUIRtl()) {
        HWND parent = GetParent(hwnd);
        Rect r = MapRectToWindow(WindowRect(hwnd), HWND_DESKTOP, parent);
        int cxVScroll = GetSystemMetrics(SM_CXVSCROLL);
        r.x = WindowRect(parent).dx - r.dx - DpiScale(hwnd, kTopLeftMargin) - cxVScroll;
        flags = SWP_NOSIZE | SWP_NOZORDER | SWP_NOREDRAW | SWP_NOACTIVATE | SWP_DEFERERASE;
        SetWindowPos(hwnd, nullptr, r.x, r.y, 0, 0, flags);
    }
}

// TODO: figure out why it flickers
void NotificationWnd::OnPaint(HDC hdcIn, PAINTSTRUCT* ps) {
    Rect rc = ClientRect(hwnd);
    DoubleBuffer buffer(hwnd, rc);
    HDC hdc = buffer.GetDC();
    // HDC hdc = hdcIn;

    ScopedSelectObject fontPrev(hdc, font);

    COLORREF colBg = ThemeNotificationsBackgroundColor();
    COLORREF colBorder = AccentColor(colBg, 4);
    COLORREF colAccent = ThemeNotificationsProgressColor();
    COLORREF colTxt = ThemeNotificationsTextColor();

    Graphics graphics(hdc);
    graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    graphics.SetPixelOffsetMode(Gdiplus::PixelOffsetModeHalf);

    {
        SolidBrush clearBr(GdiRgbFromCOLORREF(colBg));
        graphics.FillRectangle(&clearBr, 0, 0, rc.dx, rc.dy);
    }

    GraphicsPath cardPath;
    int radius = DpiScale(hwnd, kNotifRadius);
    AddRoundedRect(cardPath, rc, radius);

    SolidBrush br(GdiRgbFromCOLORREF(colBg));
    Pen borderPen(GdiRgbFromCOLORREF(colBorder));
    borderPen.SetWidth(kNotifBorder);
    borderPen.SetAlignment(Gdiplus::PenAlignmentInset);
    graphics.FillPath(&br, &cardPath);
    graphics.DrawPath(&borderPen, &cardPath);

    int accentHeight = DpiScale(hwnd, kNotifAccentHeight);
    int accentInset = radius;
    int accentWidth = rc.dx - 2 * accentInset;
    if (accentWidth < 0) {
        accentWidth = 0;
    }
    if (accentHeight > 0 && accentWidth > 0) {
        Rect accent = {accentInset, 0, accentWidth, accentHeight};
        SolidBrush accentBr(GdiRgbFromCOLORREF(colAccent));
        graphics.FillRectangle(&accentBr, accent.x, accent.y, accent.dx, accent.dy);
    }

    SetBkMode(hdc, TRANSPARENT);
    SetTextColor(hdc, colTxt);
    char* text = HwndGetTextTemp(hwnd);
    uint format = DT_SINGLELINE | DT_NOPREFIX;
    RECT rTmp = ToRECT(rTxt);
    HdcDrawText(hdc, text, &rTmp, format);

    if (!noClose) {
        Point curPos = HwndGetCursorPos(hwnd);
        DrawCloseButtonArgs args;
        args.hdc = hdc;
        args.r = rClose;
        args.isHover = rClose.Contains(curPos);
        args.colX = kColCloseX;
        args.colXHover = kColCloseXHover;
        args.colHoverBg = kColCloseXHoverBg;
        DrawCloseButton(args);
    }
#if 0
    DrawCloseButtonArgs args;
    args.hdc = hdc;
    args.r = rClose;
    args.r.Inflate(-5, -5);
    args.isHover = isHover;
    DrawCloseButton2(args);
#endif

    if (HasProgress()) {
        rc = rProgress;
        int progressWidth = rc.dx;
        COLORREF col = ThemeNotificationsProgressColor();
        Rect track = {rc.x, rc.y, rc.dx, rc.dy};
        SolidBrush trackBr(GdiRgbFromCOLORREF(AccentColor(colBg, 18)));
        GraphicsPath trackPath;
        AddRoundedRect(trackPath, track, track.dy / 2);
        graphics.FillPath(&trackBr, &trackPath);

        rc.x += 1;
        rc.y += 1;
        rc.dx = (progressWidth - 2) * progressPerc / 100;
        if (rc.dx < 0) {
            rc.dx = 0;
        }
        rc.dy -= 2;
        if (rc.dy < 1) {
            rc.dy = 1;
        }

        br.SetColor(GdiRgbFromCOLORREF(col));
        GraphicsPath fillPath;
        AddRoundedRect(fillPath, rc, rc.dy / 2);
        graphics.FillPath(&br, &fillPath);
    }

    buffer.Flush(hdcIn);
}

void NotificationWnd::UpdateMessage(const char* msg, int timeoutMs, bool highlight) {
    HwndSetText(hwnd, msg);
    this->highlight = highlight;
    this->timeoutMs = timeoutMs;
    HwndSetRtl(hwnd, IsUIRtl());
    Layout(msg);
    HwndRepaintNow(hwnd);
    if (timeoutMs != 0) {
        SetTimer(hwnd, kNotifTimerTimeoutId, timeoutMs, nullptr);
    }
}

bool UpdateNotificationProgress(NotificationWnd* wnd, const char* msg, int perc) {
    if (NotificationIndexOf(wnd) < 0) {
        return false;
    }
    ReportIf(perc < 0 || perc > 100);
    wnd->progressPerc = perc;
    wnd->UpdateMessage(msg);
    return true;
}

static void NotifRemove(NotificationWnd* wnd) {
    wnd->wndRemovedCb.Call(wnd);
}

static void NotifDelete(NotificationWnd* wnd) {
    delete wnd;
}

void NotificationWnd::OnTimer(UINT_PTR timerId) {
    if (timerId == kNotifTimerDelayId) {
        // delay elapsed, now show the notification
        KillTimer(hwnd, delayTimerId);
        delayTimerId = 0;
        BringWindowToTop(hwnd);
        ShowWindow(hwnd, SW_SHOW);
        RelayoutNotifications(hwnd);
        if (timeoutMs != 0) {
            SetTimer(hwnd, kNotifTimerTimeoutId, timeoutMs, nullptr);
        }
        return;
    }
    ReportIf(kNotifTimerTimeoutId != timerId);
    // TODO a better way to delete myself
    if (wndRemovedCb.IsValid()) {
        auto fn = MkFunc0<NotificationWnd>(NotifRemove, this);
        uitask::Post(fn, "TaskNotifOnTimerRemove");
    } else {
        auto fn = MkFunc0<NotificationWnd>(NotifDelete, this);
        uitask::Post(fn, "TaskNotifOnTimerDelete");
    }
}

LRESULT NotificationWnd::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (WM_SETCURSOR == msg && !noClose) {
        Point pt = HwndGetCursorPos(hwnd);
        if (!pt.IsEmpty() && rClose.Contains(pt)) {
            SetCursorCached(IDC_HAND);
            return TRUE;
        }
    }

    if (WM_ERASEBKGND == msg) {
        // avoid flicker by telling we took care of erasing background
        return TRUE;
    }

    if (WM_MOUSEMOVE == msg) {
        HwndScheduleRepaint(hwnd);

        if (!noClose && IsMouseOverRect(hwnd, rClose)) {
            TrackMouseLeave(hwnd);
        }
        goto DoDefault;
    }

    if (WM_MOUSELEAVE == msg) {
        HwndScheduleRepaint(hwnd);
        return 0;
    }

    if (WM_LBUTTONUP == msg) {
        Point pt = Point(GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        if (!noClose && rClose.Contains(pt)) {
            // TODO a better way to delete myself
            if (wndRemovedCb.IsValid()) {
                auto fn = MkFunc0<NotificationWnd>(NotifRemove, this);
                uitask::Post(fn, "TaskNotifWndProcRemove");
            } else {
                auto fn = MkFunc0<NotificationWnd>(NotifDelete, this);
                uitask::Post(fn, "TaskNotifWndProcDelete");
            }
            return 0;
        }
    }

DoDefault:
    return WndProcDefault(hwnd, msg, wp, lp);
}

static int NotifsRemoveForGroup(NotificationWnd** wnds, int nWnds, Kind groupId) {
    ReportIf(groupId == nullptr);
    NotificationWnd* toRemove[kMaxNotifs];
    int nRemove = 0;
    for (int i = 0; i < nWnds; i++) {
        if (wnds[i]->groupId == groupId) {
            toRemove[nRemove++] = wnds[i];
        }
    }
    for (int i = 0; i < nRemove; i++) {
        NotifsRemoveNotification(toRemove[i]);
    }
    return nRemove;
}

static bool NotifsAdd(NotificationWnd** wnds, int nWnds, NotificationWnd* wnd, Kind groupId) {
    bool skipRemove = (groupId == nullptr) || (groupId == kNotifAdHoc);
    if (!skipRemove) {
        NotifsRemoveForGroup(wnds, nWnds, groupId);
    }
    if (gNotifsCount >= kMaxNotifs) {
        return false;
    }
    wnd->groupId = groupId;
    gNotifs[gNotifsCount] = wnd;
    gNotifsCount++;
    RelayoutNotifications(wnd->hwnd);
    return true;
}

static bool NotifsAdd(NotificationWnd* wnd, Kind groupId) {
    NotificationWnd* wnds[kMaxNotifs];
    int nWnds = GetForSameHwnd(wnd, wnds);
    return NotifsAdd(wnds, nWnds, wnd, groupId);
}

NotificationWnd* NotifsGetForGroup(NotificationWnd** wnds, int nWnds, Kind groupId) {
    ReportIf(!groupId);
    for (int i = 0; i < nWnds; i++) {
        if (wnds[i]->groupId == groupId) {
            return wnds[i];
        }
    }
    return nullptr;
}

NotificationWnd* ShowNotification(const NotificationCreateArgs& args) {
    ReportIf(!args.hwndParent);

    NotificationWnd* wnd = new NotificationWnd();
    wnd->Create(args);
    if (!wnd->hwnd) {
        delete wnd;
        return nullptr;
    }
    if (wnd->delayTimerId == 0) {
        BringWindowToTop(wnd->hwnd);
    }
    bool ok = NotifsAdd(wnd, args.groupId);
    if (!ok) {
        delete wnd;
        return nullptr;
    }
    return wnd;
}

// show a temporary notification that will go away after a timeout
NotificationWnd* ShowTemporaryNotification(HWND hwnd, const char* msg, int timeoutMs) {
    if (timeoutMs <= 0) {
        timeoutMs = kNotifDefaultTimeOut;
    }
    NotificationCreateArgs args;
    args.hwndParent = hwnd;
    args.msg = msg;
    args.timeoutMs = timeoutMs;
    return ShowNotification(args);
}

NotificationWnd* ShowWarningNotification(HWND hwndParent, const char* msg, int timeoutMs) {
    if (timeoutMs < 0) {
        timeoutMs = kNotifDefaultTimeOut;
    }
    NotificationCreateArgs args;
    args.hwndParent = hwndParent;
    args.msg = msg;
    args.warning = true;
    args.timeoutMs = timeoutMs;
    return ShowNotification(args);
}

void NotificationUpdateMessage(NotificationWnd* wnd, const char* msg, int timeoutMs, bool highlight) {
    if (!wnd) {
        return;
    }
    wnd->UpdateMessage(msg, timeoutMs, highlight);
}

void RemoveNotification(NotificationWnd* wnd) {
    NotifsRemoveNotification(wnd);
}

bool RemoveNotificationsForGroup(HWND hwnd, Kind kind) {
    NotificationWnd* wnds[kMaxNotifs];
    int nWnds = GetForHwnd(hwnd, wnds);
    int n = NotifsRemoveForGroup(wnds, nWnds, kind);
    return n > 0;
}

NotificationWnd* GetNotificationForGroup(HWND hwnd, Kind kind) {
    NotificationWnd* wnds[kMaxNotifs];
    int nWnds = GetForHwnd(hwnd, wnds);
    return NotifsGetForGroup(wnds, nWnds, kind);
}

static StrNode* AllocStrNode(const char* s) {
    size_t n = str::Len(s) + 1;
    size_t cbAlloc = sizeof(StrNode) + n;
    auto* node = (StrNode*)malloc(cbAlloc);
    char* dst = (char*)node + sizeof(StrNode);
    memcpy(dst, s, n);
    node->next = nullptr;
    node->s = dst;
    return node;
}

void MaybeDelayedWarningNotification(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char* msg = str::FmtV(fmt, args);
    va_end(args);

    log(msg);

    HWND hwnd = GetHwndForNotification();
    if (hwnd) {
        ShowWarningNotification(hwnd, msg, kNotifNoTimeout);
    } else {
        StrNode* node = AllocStrNode(msg);
        ListInsertFront(&gDelayedNotifications, node);
    }
    str::Free(msg);
}

void ShowMaybeDelayedNotifications(HWND hwndParent) {
    // reverse so notifications show in the order they were added
    ListReverse(&gDelayedNotifications);
    StrNode* curr = gDelayedNotifications;
    while (curr) {
        ShowWarningNotification(hwndParent, curr->s, kNotifNoTimeout);
        curr = curr->next;
    }
    ListDelete(gDelayedNotifications);
    gDelayedNotifications = nullptr;
}
