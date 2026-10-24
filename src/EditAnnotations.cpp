/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

extern "C" {
#include <mupdf/fitz.h>
#include <mupdf/pdf.h>
}

#include "utils/BaseUtil.h"
#include "utils/BitManip.h"
#include "utils/FileUtil.h"
#include "utils/ScopedWin.h"
#include "utils/WinUtil.h"
#include "utils/Dpi.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"

#include "Settings.h"
#include "AppSettings.h"
#include "DocController.h"
#include "Annotation.h"
#include "EngineBase.h"
#include "EngineAll.h"
#include "EngineMupdf.h"
#include "Translations.h"
#include "SumatraConfig.h"
#include "GlobalPrefs.h"
#include "DisplayModel.h"
#include "ProgressUpdateUI.h"
#include "Notifications.h"
#include "MainWindow.h"
#include "Toolbar.h"
#include "WindowTab.h"
#include "EditAnnotations.h"
#include "wingui/LabelWithCloseWnd.h"
#include "SumatraPDF.h"
#include "Canvas.h"
#include "Commands.h"
#include "DarkModeSubclass.h"

#include "utils/Log.h"

#include "theme.h"

constexpr int borderWidthMin = 0;
constexpr int borderWidthMax = 12;
constexpr int kEditAnnotationsSidebarDefaultDx = 420;
constexpr int kEditAnnotationsSidebarMinDx = 240;

// clang-format off
static const char *gFileAttachmentUcons = "Graph\0Paperclip\0PushPin\0Tag\0";
static const char *gSoundIcons = "Speaker\0Mic\0";
static const char *gStampIcons = "Approved\0AsIs\0Confidential\0Departmental\0Draft\0Experimental\0Expired\0Final\0ForComment\0ForPublicRelease\0NotApproved\0NotForPublicRelease\0Sold\0TopSecret\0";
// those are in order of pdf_line_ending enum in annot.h
static const char *gLineEndingStyles = "None\0Square\0Circle\0Diamond\0OpenArrow\0ClosedArrow\0Butt\0ROpenArrow\0RClosedArrow\0Slash\0";
static const char* gColors = "Transparent\0Aqua\0Black\0Blue\0Fuchsia\0Gray\0Green\0Lime\0Maroon\0Navy\0Olive\0Orange\0Purple\0Red\0Silver\0Teal\0White\0Yellow\0";
static const char *gFontNames = "Cour\0Helv\0TiRo\0";
static const char *gFontReadableNames = "Courier\0Helvetica\0TimesRoman\0";
static const char* gQuaddingNames = "Left\0Center\0Right\0";

static PdfColor gColorsValues[] = {
	0x00000000, /* transparent */
	0xff00ffff, /* aqua */
	0xff000000, /* black */
	0xff0000ff, /* blue */
	0xffff00ff, /* fuchsia */
	0xff808080, /* gray */
	0xff008000, /* green */
	0xff00ff00, /* lime */
	0xff800000, /* maroon */
	0xff000080, /* navy */
	0xff808000, /* olive */
	0xffffa500, /* orange */
	0xff800080, /* purple */
	0xffff0000, /* red */
	0xffc0c0c0, /* silver */
	0xff008080, /* teal */
	0xffffffff, /* white */
	0xffffff00, /* yellow */
};

// list of annotations where GetColor() returns background color
// TODO: probably incomplete;
static AnnotationType gAnnotsIsColorBackground[] = {
    AnnotationType::FreeText,
};
// clang-format on

const char* GetKnownColorName(PdfColor c) {
    int n = (int)dimof(gColorsValues);
    for (int i = 0; i < n; i++) {
        if (c == gColorsValues[i]) {
            const char* s = seqstrings::IdxToStr(gColors, i);
            return s;
        }
    }
    return nullptr;
}

struct EditAnnotationsWindow : Wnd {
    WindowTab* tab = nullptr;
    LayoutBase* mainLayout = nullptr;
    LabelWithCloseWnd* headerLabel = nullptr;
    Vec<Static*> sectionLabels;
    HBRUSH brSurface = nullptr;
    int scrollPos = 0;
    int contentHeight = 0;
    int viewportHeight = 0;

    ListBox* listBox = nullptr;
    Static* staticRect = nullptr;
    Static* staticAuthor = nullptr;
    Static* staticModificationDate = nullptr;
    Static* staticPopup = nullptr;
    Static* staticContents = nullptr;
    Edit* editContents = nullptr;
    Static* staticTextAlignment = nullptr;
    DropDown* dropDownTextAlignment = nullptr;
    Static* staticTextFont = nullptr;
    DropDown* dropDownTextFont = nullptr;
    Static* staticTextSize = nullptr;
    Trackbar* trackbarTextSize = nullptr;
    Static* staticTextColor = nullptr;
    DropDown* dropDownTextColor = nullptr;

    Static* staticLineStart = nullptr;
    DropDown* dropDownLineStart = nullptr;
    Static* staticLineEnd = nullptr;
    DropDown* dropDownLineEnd = nullptr;

    Static* staticIcon = nullptr;
    DropDown* dropDownIcon = nullptr;

    Static* staticBorder = nullptr;
    Trackbar* trackbarBorder = nullptr;

    Static* staticColor = nullptr;
    DropDown* dropDownColor = nullptr;
    Button* buttonColorCustom = nullptr;
    Static* staticInteriorColor = nullptr;
    DropDown* dropDownInteriorColor = nullptr;
    Button* buttonInteriorColorCustom = nullptr;
    COLORREF customColors[16] = {};
    COLORREF customInteriorColors[16] = {};

    Static* staticOpacity = nullptr;
    Trackbar* trackbarOpacity = nullptr;

    Button* buttonSaveAttachment = nullptr;
    Button* buttonEmbedAttachment = nullptr;

    Button* buttonDelete = nullptr;

    Button* buttonSaveToCurrentFile = nullptr;
    Button* buttonSaveToNewFile = nullptr;

    // those are
    Vec<Annotation*> annotations;

    bool skipGoToPage = false;

    StrBuilder currTextColor;
    StrBuilder currCustomColor;
    StrBuilder currCustomInteriorColor;

    void OnSize(UINT msg, UINT type, SIZE size) override;
    void OnFocus() override;
    bool OnCommand(WPARAM wparam, LPARAM lparam) override;
    bool PreTranslateMessage(MSG&) override;
    LRESULT WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) override;

    void ListBoxSelectionChanged();

    ~EditAnnotationsWindow() override;
};

#if 0
static Annotation* PickNewSelectedAnnotation(EditAnnotationsWindow* ew, int prevIdx) {
    int nAnnots = ew->annotations.Size();
    if (nAnnots == 0) {
        return nullptr;
    }
    if (prevIdx >= nAnnots) {
        prevIdx = nAnnots - 1;
    }
    return ew->annotations.at(prevIdx);
}
#endif

void DeleteAnnotationAndUpdateUI(WindowTab* tab, Annotation* annot) {
    EditAnnotationsWindow* ew = tab->editAnnotsWindow;
    Annotation* selectNext = nullptr;
    if (annot != tab->selectedAnnotation) {
        // preserve current selection if we're not deleting it
        selectNext = tab->selectedAnnotation;
    }

    DeleteAnnotation(annot);
    if (ew != nullptr) {
        // can be null if called from Menu.cpp and annotations window is not visible
        // ew->skipGoToPage = true;
        // int currSelIdx = ew ? ew->listBox->GetCurrentSelection() : -1;
        UpdateAnnotationsList(ew);
#if 0
        if ((selectNext == nullptr) && (currSelIdx >= 0)) {
            // if we're deleting currently selected, pick
            // next to select
            annot = PickNewSelectedAnnotation(ew, currSelIdx);
        }
#endif
    }
    SetSelectedAnnotation(tab, selectNext);
}

static void DeleteSelectedAnnotation(EditAnnotationsWindow* ew) {
    int idx = ew->listBox->GetCurrentSelection();
    if (idx < 0) {
        // can get out of sync e.g. after UpdateAnnotationsList during save/reload
        ew->tab->selectedAnnotation = nullptr;
        return;
    }
    Annotation* annot = ew->annotations.at(idx);
    if (ew->tab->selectedAnnotation != annot) {
        // can get out of sync if e.g. keyboard navigation in listbox
        // hasn't triggered ListBoxSelectionChanged yet
        ew->tab->selectedAnnotation = annot;
    }
    DeleteAnnotationAndUpdateUI(ew->tab, annot);

    // Note: auto-selecting next annotation might cause page jumping
#if 0
    annot = PickNewSelectedAnnotation(this, idx);
    skipGoToPage = false;
    if (annot) {
        SetSelectedAnnotation(tab, annot);
    }
#endif
}

static NO_INLINE EngineMupdf* GetEngineMupdf(EditAnnotationsWindow* ew) {
#if 0
    // TODO: shouldn't happen but seen in crash report
    if (!ew || !ew->tab) {
        return nullptr;
    }
#endif
    DisplayModel* dm = ew->tab->AsFixed();
#if 0
    if (!dm) {
        return nullptr;
    }
#endif
    return AsEngineMupdf(dm->GetEngine());
}

static void HidePerAnnotControls(EditAnnotationsWindow* ew) {
    // Use array-based approach for robustness: easier to add new controls
    // without forgetting to hide them
    Wnd* controls[] = {
        ew->staticRect, ew->staticAuthor, ew->staticModificationDate,
        ew->staticPopup, ew->staticContents, ew->editContents,
        ew->staticTextAlignment, ew->dropDownTextAlignment,
        ew->staticTextFont, ew->dropDownTextFont,
        ew->staticTextSize, ew->trackbarTextSize,
        ew->staticTextColor, ew->dropDownTextColor,
        ew->staticLineStart, ew->dropDownLineStart,
        ew->staticLineEnd, ew->dropDownLineEnd,
        ew->staticIcon, ew->dropDownIcon,
        ew->staticBorder, ew->trackbarBorder,
        ew->staticColor, ew->dropDownColor, ew->buttonColorCustom,
        ew->staticInteriorColor, ew->dropDownInteriorColor, ew->buttonInteriorColorCustom,
        ew->staticOpacity, ew->trackbarOpacity,
        ew->buttonSaveAttachment, ew->buttonEmbedAttachment,
        ew->buttonDelete,
    };
    for (auto* control : controls) {
        if (control) {
            control->SetIsVisible(false);
        }
    }
}

HWND GetEditAnnotationsFocusHwnd(WindowTab* tab) {
    if (!tab || !tab->editAnnotsWindow) {
        return nullptr;
    }
    auto ew = tab->editAnnotsWindow;
    if (ew->listBox) {
        return ew->listBox->hwnd;
    }
    return ew->hwnd;
}

static int FindStringInArray(const char* items, const char* toFind, int valIfNotFound = -1) {
    int idx = seqstrings::StrToIdx(items, toFind);
    if (idx < 0) {
        idx = valIfNotFound;
    }
    return idx;
}

static bool IsAnnotationTypeInArray(AnnotationType* arr, size_t arrSize, AnnotationType toFind) {
    for (size_t i = 0; i < arrSize; i++) {
        if (toFind == arr[i]) {
            return true;
        }
    }
    return false;
}

// return true if closed the window, false if there was no window to close
bool CloseAndDeleteEditAnnotationsWindow(WindowTab* tab) {
    if (!tab->editAnnotsWindow) {
        return false;
    }
    auto ew = tab->editAnnotsWindow;
    MainWindow* win = tab->win;
    tab->editAnnotsWindow = nullptr;
    // this will trigger closing the window
    delete ew;
    if (win && !win->isBeingClosed) {
        RelayoutWindow(win);
    }
    return true;
}

void LayoutEditAnnotationsWindow(WindowTab* tab, Rect& rc, DeferWinPosHelper* dh) {
    if (!tab || !tab->editAnnotsWindow) {
        return;
    }
    auto ew = tab->editAnnotsWindow;
    if (!ew->IsVisible()) {
        return;
    }

    int sidebarDx = tab->lastEditAnnotsWindowPos.dx;
    if (sidebarDx <= 0) {
        sidebarDx = kEditAnnotationsSidebarDefaultDx;
    }
    int minDx = std::min(kEditAnnotationsSidebarMinDx, std::max(1, rc.dx / 2));
    int maxDx = std::max(minDx, rc.dx / 2);
    sidebarDx = limitValue(sidebarDx, minDx, maxDx);

    Rect r{rc.x + rc.dx - sidebarDx, rc.y, sidebarDx, rc.dy};
    // If caller provided a DeferWinPosHelper, use it to batch window moves
    if (dh) {
        // Ensure the annotations panel is positioned and placed above the
        // main canvas so it is visible immediately after the batch move.
        constexpr uint kFlags = SWP_NOOWNERZORDER | SWP_NOACTIVATE;
        dh->SetWindowPos(ew->hwnd, HWND_TOP, r.x, r.y, r.dx, r.dy, kFlags);
    } else {
        RECT rr = ToRECT(r);
        ew->SetPos(&rr);
        HwndRepaintNow(ew->hwnd);
    }
    rc.dx -= sidebarDx;
}

EditAnnotationsWindow::~EditAnnotationsWindow() {
    // hacky: we want the position of the main window
    // but the size of client area
    tab->lastEditAnnotsWindowPos = WindowRect(hwnd);
    auto cr = ClientRect(hwnd);
    tab->lastEditAnnotsWindowPos.dx = cr.dx;
    tab->lastEditAnnotsWindowPos.dy = cr.dy;

    if (tab->selectedAnnotation != nullptr) {
        tab->selectedAnnotation = nullptr;
        if (!tab->win->isBeingClosed) {
            MainWindowRerender(tab->win);
            ToolbarUpdateStateForWindow(tab->win, false);
        }
    }
    if (brSurface) {
        DeleteObject(brSurface);
    }
    delete mainLayout;
}

LRESULT EditAnnotationsWindow::WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_VSCROLL) {
        SCROLLINFO si = {sizeof(si), SIF_ALL};
        GetScrollInfo(hwnd, SB_VERT, &si);
        int newPos = scrollPos;
        switch (LOWORD(wparam)) {
            case SB_LINEUP:
                newPos -= DpiScale(hwnd, 24);
                break;
            case SB_LINEDOWN:
                newPos += DpiScale(hwnd, 24);
                break;
            case SB_PAGEUP:
                newPos -= viewportHeight;
                break;
            case SB_PAGEDOWN:
                newPos += viewportHeight;
                break;
            case SB_THUMBPOSITION:
            case SB_THUMBTRACK:
                newPos = si.nTrackPos;
                break;
            case SB_TOP:
                newPos = 0;
                break;
            case SB_BOTTOM:
                newPos = std::max(0, contentHeight - viewportHeight);
                break;
            default:
                return 0;
        }
        newPos = limitValue(newPos, 0, std::max(0, contentHeight - viewportHeight));
        if (newPos != scrollPos) {
            int delta = scrollPos - newPos;
            scrollPos = newPos;
            si.fMask = SIF_POS;
            si.nPos = scrollPos;
            SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
            ScrollWindowEx(hwnd, 0, delta, nullptr, nullptr, nullptr, nullptr,
                           SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
        }
        return 0;
    }
    if (msg == WM_ERASEBKGND) {
        HDC hdc = (HDC)wparam;
        RECT rc;
        GetClientRect(hwnd, &rc);

        if (brSurface) {
            FillRect(hdc, &rc, brSurface);
        } else {
            COLORREF bgColor = ThemeControlBackgroundColor();
            HBRUSH br = CreateSolidBrush(bgColor);
            FillRect(hdc, &rc, br);
            DeleteObject(br);
        }
        return 1;
    }
    return Wnd::WndProc(hwnd, msg, wparam, lparam);
}

static bool DidAnnotationsChange(EditAnnotationsWindow* ew) {
    EngineMupdf* engine = GetEngineMupdf(ew);
    if (!engine) { // maybe seen in crash report
        ReportIf(true);
        return false;
    }
    return EngineMupdfHasUnsavedAnnotations(engine);
}

static void EnableSaveIfAnnotationsChanged(EditAnnotationsWindow* ew) {
    bool didChange = DidAnnotationsChange(ew);
    ew->buttonSaveToCurrentFile->SetIsEnabled(didChange);
    ew->buttonSaveToNewFile->SetIsEnabled(didChange);
}

void NotifyAnnotationsChanged(EditAnnotationsWindow* ew) {
    if (!ew) {
        return;
    }
    EnableSaveIfAnnotationsChanged(ew);
}

void UpdateEditAnnotationsColors(WindowTab* tab) {
    if (tab && tab->editAnnotsWindow) {
        UpdateEditAnnotationsColors(tab->editAnnotsWindow);
    }
}

void UpdateEditAnnotationsColors(EditAnnotationsWindow* ew) {
    if (!ew || !ew->hwnd) {
        return;
    }

    COLORREF txtCol = ThemeWindowTextColor();
    COLORREF bgCol  = ThemeControlBackgroundColor();

    // Update parent window colors to handle child color notifications (WM_CTLCOLOR*)
    ew->SetColors(txtCol, bgCol);

    // === ACTUALIZAR EL BRUSH DE FONDO (brSurface) ===
    if (ew->brSurface) {
        DeleteObject(ew->brSurface);
    }
    ew->brSurface = CreateSolidBrush(bgCol);

    // ListBox
    if (ew->listBox) {
        ew->listBox->SetColors(txtCol, bgCol);
    }

    // Header
    if (ew->headerLabel) {
        ew->headerLabel->SetColors(txtCol, bgCol);
    }

    // Section labels
    for (auto* lbl : ew->sectionLabels) {
        if (lbl) {
            lbl->SetColors(txtCol, bgCol);
        }
    }

    // Todos los Static (labels)
    Static* statics[] = {
        ew->staticRect, ew->staticAuthor, ew->staticModificationDate,
        ew->staticPopup, ew->staticContents, ew->staticTextAlignment,
        ew->staticTextFont, ew->staticTextSize, ew->staticTextColor,
        ew->staticLineStart, ew->staticLineEnd, ew->staticIcon,
        ew->staticBorder, ew->staticColor, ew->staticInteriorColor,
        ew->staticOpacity
    };
    for (auto* s : statics) {
        if (s) s->SetColors(txtCol, bgCol);
    }

    // Edit (contenido)
    if (ew->editContents) {
        ew->editContents->SetColors(txtCol, bgCol);
    }

    // DropDowns
    DropDown* dropdowns[] = {
        ew->dropDownTextAlignment, ew->dropDownTextFont, ew->dropDownTextColor,
        ew->dropDownLineStart, ew->dropDownLineEnd, ew->dropDownIcon,
        ew->dropDownColor, ew->dropDownInteriorColor
    };
    for (auto* dd : dropdowns) {
        if (dd) dd->SetColors(txtCol, bgCol);
    }

    // Trackbars
    Trackbar* trackbars[] = {
        ew->trackbarTextSize, ew->trackbarBorder, ew->trackbarOpacity
    };
    for (auto* tb : trackbars) {
        if (tb) tb->SetColors(txtCol, bgCol);
    }

    // Buttons
    Button* buttons[] = {
        ew->buttonColorCustom, ew->buttonInteriorColorCustom,
        ew->buttonSaveAttachment, ew->buttonEmbedAttachment,
        ew->buttonDelete, ew->buttonSaveToCurrentFile,
        ew->buttonSaveToNewFile
    };
    for (auto* btn : buttons) {
        if (btn) btn->SetColors(txtCol, bgCol);
    }

    if (UseDarkModeLib()) {
        DarkMode::setDarkWndNotifySafe(ew->hwnd);
    }

    RedrawWindow(ew->hwnd, nullptr, nullptr,
                 RDW_ERASE | RDW_INVALIDATE | RDW_UPDATENOW |
                 RDW_ALLCHILDREN | RDW_FRAME);
}

static void RebuildAnnotationsListBox(EditAnnotationsWindow* ew) {

    auto model = new ListBoxModelStrings();
    int n = 0;
    n = ew->annotations.Size();

    StrBuilder s;
    for (int i = 0; i < n; i++) {
        auto annot = ew->annotations.at(i);
        s.Reset();
        s.AppendFmt(_TRA("page %d,"), annot->pageNo);
        TempStr name = AnnotationReadableNameTemp(annot->type);
        s.AppendFmt(" %s", name);
        model->strings.Append(s.Get());
    }

    auto topIdx = ListBoxGetTopIndex(ew->listBox->hwnd);
    ew->listBox->SetModel(model);
    topIdx = std::min(ew->listBox->GetCount() - 1, topIdx);
    if (topIdx >= 0) {
        ListBoxSetTopIndex(ew->listBox->hwnd, topIdx);
    }
    EnableSaveIfAnnotationsChanged(ew);
}

// TODO: this should be OnDestroy()
static void OnClose(Wnd::CloseEvent* ev) {
    auto w = (EditAnnotationsWindow*)ev->e->self;
    auto tab = w->tab;
    MainWindow* win = tab ? tab->win : nullptr;
    HWND toActivate = win ? win->hwndFrame : nullptr;
    if (tab) {
        tab->editAnnotsWindow = nullptr;
    }
    delete w; // TODO: sketchy
    if (win && !win->isBeingClosed) {
        RelayoutWindow(win);
    }
    SetActiveWindow(toActivate);
}

void EditAnnotationsWindow::OnFocus() {
    SelectTabInWindow(tab);
}

extern bool SaveAnnotationsToMaybeNewPdfFile(WindowTab*);

static void ButtonSaveToNewFileHandler(EditAnnotationsWindow* ew) {
    WindowTab* tab = ew->tab;
    bool ok = SaveAnnotationsToMaybeNewPdfFile(tab);
    if (!ok) {
        return;
    }
}

extern bool SaveAnnotationsToExistingFile(WindowTab* tab);

static void ButtonSaveToCurrentPDFHandler(EditAnnotationsWindow* ew) {
    SaveAnnotationsToExistingFile(ew->tab);
}

constexpr int kMaxControls = 18;

static bool AdvanceFocus(EditAnnotationsWindow* ew, bool forward) {
    HWND controls[kMaxControls];
    int n = 0;
    auto addIfVisible = [&](HWND h) {
        if (h && IsWindowVisible(h)) {
            ReportIf(n >= kMaxControls);
            controls[n++] = h;
        }
    };

    addIfVisible(ew->listBox->hwnd);
    addIfVisible(ew->editContents->hwnd);
    addIfVisible(ew->dropDownTextAlignment->hwnd);
    addIfVisible(ew->dropDownTextFont->hwnd);
    addIfVisible(ew->trackbarTextSize->hwnd);
    addIfVisible(ew->dropDownTextColor->hwnd);
    addIfVisible(ew->dropDownLineStart->hwnd);
    addIfVisible(ew->dropDownLineEnd->hwnd);
    addIfVisible(ew->dropDownIcon->hwnd);
    addIfVisible(ew->trackbarBorder->hwnd);
    addIfVisible(ew->dropDownColor->hwnd);
    addIfVisible(ew->dropDownInteriorColor->hwnd);
    addIfVisible(ew->trackbarOpacity->hwnd);
    addIfVisible(ew->buttonSaveAttachment->hwnd);
    addIfVisible(ew->buttonEmbedAttachment->hwnd);
    addIfVisible(ew->buttonDelete->hwnd);
    addIfVisible(ew->buttonSaveToCurrentFile->hwnd);
    addIfVisible(ew->buttonSaveToNewFile->hwnd);

    if (n == 0) {
        return false;
    }

    HWND focused = ::GetFocus();
    int idx = -1;
    for (int i = 0; i < n; i++) {
        if (controls[i] == focused || ::IsChild(controls[i], focused)) {
            idx = i;
            break;
        }
    }

    if (idx < 0) {
        HwndSetFocus(controls[forward ? 0 : n - 1]);
        return true;
    }
    if ((forward && idx == n - 1) || (!forward && idx == 0)) {
        return false;
    }
    int next = idx + (forward ? 1 : -1);
    HwndSetFocus(controls[next]);
    return true;
}

bool EditAnnotationsWindow::PreTranslateMessage(MSG& msg) {
    if (msg.message == WM_MOUSEWHEEL && msg.hwnd == hwnd) {
        int delta = GET_WHEEL_DELTA_WPARAM(msg.wParam);
        int lines = std::max(1, abs(delta) / WHEEL_DELTA) * 3;
        WPARAM command = MAKEWPARAM(delta > 0 ? SB_LINEUP : SB_LINEDOWN, 0);
        for (int i = 0; i < lines; i++) {
            SendMessageW(hwnd, WM_VSCROLL, command, 0);
        }
        return true;
    }
    if (msg.message == WM_KEYDOWN) {
        int key = (int)msg.wParam;
        if (key == VK_ESCAPE) {
            Close();
            return true;
        }
        if (key == VK_TAB) {
            bool forward = !IsShiftPressed();
            if (!AdvanceFocus(this, forward)) {
                ::AdvanceFocus(tab->win);
            }
            return true;
        }
        if (key == VK_DELETE) {
            if (IsCtrlPressed()) {
                DeleteSelectedAnnotation(this);
                return true;
            }
            // we don't want this to trigger in edit control
            HWND focused = ::GetFocus();
            TempStr cls = HwndGetClassName(focused);
            if (str::EqI(cls, "Edit")) {
                return false;
            }
            DeleteSelectedAnnotation(this);
            return true;
        }
        if (key == 'S' && IsShiftPressed() && IsCtrlPressed()) {
            // TODO: delay by posting a message?
            // TODO: the keybinding could be changed so this should
            // be more sophisticated and match the shortcut
            ButtonSaveToCurrentPDFHandler(this);
            return true;
        }
    }
    return false;
}

bool EditAnnotationsWindow::OnCommand(WPARAM wparam, LPARAM lparam) {
    if (wparam == 1) {
        Close();
        return true;
    }
    return Wnd::OnCommand(wparam, lparam);
}

static void ItemsFromSeqstrings(StrVec& items, const char* strings) {
    while (strings) {
        items.Append(strings);
        seqstrings::Next(strings);
    }
}

static void DropDownFillColors(DropDown* w, PdfColor col, StrBuilder& customColor) {
    StrVec items;
    ItemsFromSeqstrings(items, gColors);
    const char* colorName = GetKnownColorName(col);
    int idx = seqstrings::StrToIdx(gColors, colorName);
    if (idx < 0) {
        customColor.Reset();
        SerializePdfColor(col, customColor);
        items.Append(customColor.LendData());
        idx = items.Size() - 1;
    }
    w->SetItems(items);
    w->SetCurrentSelection(idx);
}

static PdfColor GetDropDownColor(const char* sv) {
    int idx = seqstrings::StrToIdx(gColors, sv);
    if (idx >= 0) {
        int nMaxColors = (int)dimof(gColorsValues);
        ReportIf(idx >= nMaxColors);
        if (idx < nMaxColors) {
            return gColorsValues[idx];
        }
        return 0;
    }
    ParsedColor col;
    ParseColor(col, sv);
    return col.pdfCol;
}

// TODO: mupdf shows it in 1.6 but not 1.7. Why?
bool gShowRect = true;

// TODO: only limit to widgets that have rect?
static void DoRect(EditAnnotationsWindow* ew, Annotation* annot) {
    if (!gShowRect) {
        return;
    }
    StrBuilder s;
    RectF rect = GetBounds(annot);
    int x = (int)rect.x;
    int y = (int)rect.y;
    int dx = (int)rect.dx;
    int dy = (int)rect.dy;
    s.AppendFmt(_TRA("Rect: x=%d y=%d dx=%d dy=%d"), x, y, dx, dy);
    ew->staticRect->SetText(s.Get());
    ew->staticRect->SetIsVisible(true);
}

static void DoAuthor(EditAnnotationsWindow* ew, Annotation* annot) {
    const char* author = Author(annot);
    bool isVisible = !str::IsEmpty(author);
    if (!isVisible) {
        return;
    }
    StrBuilder s;
    s.AppendFmt(_TRA("Author: %s"), author);
    ew->staticAuthor->SetText(s.Get());
    ew->staticAuthor->SetIsVisible(true);
}

static void AppendPdfDate(StrBuilder& s, time_t secs) {
    struct tm tm;
    gmtime_s(&tm, &secs);
    char buf[100];
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M UTC", &tm);
    s.Append(buf);
}

static void DoModificationDate(EditAnnotationsWindow* ew, Annotation* annot) {
    bool isVisible = (ModificationDate(annot) != 0);
    if (!isVisible) {
        return;
    }
    StrBuilder s;
    s.Append(_TRA("Date:"));
    s.Append(" "); // apptranslator doesn't handle spaces at the end of translated string
    AppendPdfDate(s, ModificationDate(annot));
    ew->staticModificationDate->SetText(s.Get());
    ew->staticModificationDate->SetIsVisible(true);
}

static void DoPopup(EditAnnotationsWindow* ew, Annotation* annot) {
    int popupId = PopupId(annot);
    if (popupId < 0) {
        return;
    }
    StrBuilder s;
    s.AppendFmt(_TRA("Popup: %d 0 R"), popupId);
    ew->staticPopup->SetText(s.Get());
    ew->staticPopup->SetIsVisible(true);
}

static void DoContents(EditAnnotationsWindow* ew, Annotation* annot) {
    TempStr s = Contents(annot);
    // don't replace if already is "\r\n"
    s = str::ReplaceTemp(s, "\r\n", "\n");
    s = str::ReplaceTemp(s, "\n", "\r\n");
    ew->editContents->SetText(s);
    ew->staticContents->SetIsVisible(true);
    ew->editContents->SetIsVisible(true);
}

static void DoTextAlignment(EditAnnotationsWindow* ew, Annotation* annot) {
    if (Type(annot) != AnnotationType::FreeText) {
        return;
    }
    int itemNo = Quadding(annot);
    const char* items = gQuaddingNames;
    ew->dropDownTextAlignment->SetItemsSeqStrings(items);
    ew->dropDownTextAlignment->SetCurrentSelection(itemNo);
    ew->staticTextAlignment->SetIsVisible(true);
    ew->dropDownTextAlignment->SetIsVisible(true);
}

static void TextAlignmentSelectionChanged(EditAnnotationsWindow* ew) {
    auto annot = ew->tab->selectedAnnotation;
    if (!annot || !annot->engine) {
        return;
    }
    auto idx = ew->dropDownTextAlignment->GetCurrentSelection();
    int newQuadding = idx;
    SetQuadding(annot, newQuadding);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoTextFont(EditAnnotationsWindow* ew, Annotation* annot) {
    if (Type(annot) != AnnotationType::FreeText) {
        return;
    }
    const char* fontName = DefaultAppearanceTextFont(annot);
    // TODO: might have other fonts, like "Symb" and "ZaDb"
    auto itemNo = seqstrings::StrToIdx(gFontNames, fontName);
    if (itemNo < 0) {
        return;
    }
    ew->dropDownTextFont->SetItemsSeqStrings(gFontReadableNames);
    ew->dropDownTextFont->SetCurrentSelection(itemNo);
    ew->staticTextFont->SetIsVisible(true);
    ew->dropDownTextFont->SetIsVisible(true);
}

static void TextFontSelectionChanged(EditAnnotationsWindow* ew) {
    auto annot = ew->tab->selectedAnnotation;
    if (!annot || !annot->engine) {
        return;
    }
    auto idx = ew->dropDownTextFont->GetCurrentSelection();
    const char* font = seqstrings::IdxToStr(gFontNames, idx);
    SetDefaultAppearanceTextFont(annot, font);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoTextSize(EditAnnotationsWindow* ew, Annotation* annot) {
    if (Type(annot) != AnnotationType::FreeText) {
        return;
    }
    int fontSize = DefaultAppearanceTextSize(annot);
    TempStr s = str::FormatTemp(_TRA("Text Size: %d"), fontSize);
    ew->staticTextSize->SetText(s);
    // TODO: DoTextSize() shouldn't modify the annotation but I'm not sure
    // if it's not needed to be called for free text annotations
    // at some point (i.e. when creating)
    // SetDefaultAppearanceTextSize(ew->tab->selectedAnnotation, fontSize);
    ew->trackbarTextSize->SetValue(fontSize);
    ew->staticTextSize->SetIsVisible(true);
    ew->trackbarTextSize->SetIsVisible(true);
}

static void TextFontSizeChanging(EditAnnotationsWindow* ew, Trackbar::PositionChangingEvent* ev) {
    auto annot = ew->tab->selectedAnnotation;
    if (!annot || !annot->engine) {
        return;
    }
    int fontSize = ev->pos;
    SetDefaultAppearanceTextSize(annot, fontSize);
    TempStr s = str::FormatTemp(_TRA("Text Size: %d"), fontSize);
    ew->staticTextSize->SetText(s);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoTextColor(EditAnnotationsWindow* ew, Annotation* annot) {
    if (Type(annot) != AnnotationType::FreeText) {
        return;
    }
    PdfColor col = DefaultAppearanceTextColor(annot);
    DropDownFillColors(ew->dropDownTextColor, col, ew->currTextColor);
    ew->staticTextColor->SetIsVisible(true);
    ew->dropDownTextColor->SetIsVisible(true);
}

static void TextColorSelectionChanged(EditAnnotationsWindow* ew) {
    auto annot = ew->tab->selectedAnnotation;
    if (!annot || !annot->engine) {
        return;
    }
    auto idx = ew->dropDownTextColor->GetCurrentSelection();
    char* item = ew->dropDownTextColor->items.At(idx);
    auto col = GetDropDownColor(item);
    SetDefaultAppearanceTextColor(annot, col);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoBorder(EditAnnotationsWindow* ew, Annotation* annot) {
    if (!AnnotationSupportsBorder(annot->type)) {
        return;
    }
    int borderWidth = BorderWidth(annot);
    borderWidth = std::clamp(borderWidth, borderWidthMin, borderWidthMax);
    TempStr s = str::FormatTemp(_TRA("Border: %d"), borderWidth);
    ew->staticBorder->SetText(s);
    ew->trackbarBorder->SetValue(borderWidth);
    ew->staticBorder->SetIsVisible(true);
    ew->trackbarBorder->SetIsVisible(true);
}

static void BorderWidthChanging(EditAnnotationsWindow* ew, Trackbar::PositionChangingEvent* ev) {
    auto annot = ew->tab->selectedAnnotation;
    if (!annot || !annot->engine) {
        return;
    }
    int borderWidth = ev->pos;
    SetBorderWidth(annot, borderWidth);
    TempStr s = str::FormatTemp(_TRA("Border: %d"), borderWidth);
    ew->staticBorder->SetText(s);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoLineStartEnd(EditAnnotationsWindow* ew, Annotation* annot) {
    if (Type(annot) != AnnotationType::Line) {
        return;
    }
    int start = 0;
    int end = 0;
    GetLineEndingStyles(annot, &start, &end);
    ew->dropDownLineStart->SetItemsSeqStrings(gLineEndingStyles);
    ew->dropDownLineStart->SetCurrentSelection(start);
    ew->dropDownLineEnd->SetItemsSeqStrings(gLineEndingStyles);
    ew->dropDownLineEnd->SetCurrentSelection(end);
    ew->staticLineStart->SetIsVisible(true);
    ew->dropDownLineStart->SetIsVisible(true);
    ew->staticLineEnd->SetIsVisible(true);
    ew->dropDownLineEnd->SetIsVisible(true);
}

static void LineStartSelectionChanged(EditAnnotationsWindow* ew) {
    auto annot = ew->tab->selectedAnnotation;
    if (!annot || !annot->engine) {
        return;
    }
    auto start = ew->dropDownLineStart->GetCurrentSelection();
    if (start < 0) {
        return;
    }
    SetLineStartStyles(annot, start);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void LineEndSelectionChanged(EditAnnotationsWindow* ew) {
    auto annot = ew->tab->selectedAnnotation;
    if (!annot || !annot->engine) {
        return;
    }
    auto end = ew->dropDownLineEnd->GetCurrentSelection();
    if (end < 0) {
        return;
    }
    SetLineEndStyles(annot, end);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoIcon(EditAnnotationsWindow* ew, Annotation* annot) {
    const char* itemName = IconName(annot);
    const char* items = nullptr;
    switch (Type(annot)) {
        case AnnotationType::Text:
            items = gAnnotationTextIcons;
            break;
        case AnnotationType::FileAttachment:
            items = gFileAttachmentUcons;
            break;
        case AnnotationType::Sound:
            items = gSoundIcons;
            break;
        case AnnotationType::Stamp:
            items = gStampIcons;
            break;
        default:
            // no-op
            break;
    }
    if (!items || str::IsEmpty(itemName)) {
        return;
    }
    ew->dropDownIcon->SetItemsSeqStrings(items);
    int idx = FindStringInArray(items, itemName, 0);
    ew->dropDownIcon->SetCurrentSelection(idx);
    ew->staticIcon->SetIsVisible(true);
    ew->dropDownIcon->SetIsVisible(true);
}

static void IconSelectionChanged(EditAnnotationsWindow* ew) {
    auto annot = ew->tab->selectedAnnotation;
    if (!annot || !annot->engine) {
        return;
    }
    auto idx = ew->dropDownIcon->GetCurrentSelection();
    auto item = ew->dropDownIcon->items.At(idx);
    SetIconName(annot, item);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoColor(EditAnnotationsWindow* ew, Annotation* annot) {
    if (!AnnotationSupportsColor(annot->type)) {
        return;
    }
    PdfColor col = GetColor(annot);
    DropDownFillColors(ew->dropDownColor, col, ew->currCustomColor);
    int n = dimof(gAnnotsIsColorBackground);
    bool isBgCol = IsAnnotationTypeInArray(gAnnotsIsColorBackground, n, Type(annot));
    if (isBgCol) {
        ew->staticColor->SetText(_TRA("Background Color:"));
    } else {
        ew->staticColor->SetText(_TRA("Color:"));
    }
    ew->staticColor->SetIsVisible(true);
    ew->dropDownColor->SetIsVisible(true);
}

static void ColorSelectionChanged(EditAnnotationsWindow* ew) {
    auto annot = ew->tab->selectedAnnotation;
    if (!annot || !annot->engine) {
        return;
    }
    auto idx = ew->dropDownColor->GetCurrentSelection();
    auto item = ew->dropDownColor->items.At(idx);
    auto col = GetDropDownColor(item);
    SetColor(annot, col);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoInteriorColor(EditAnnotationsWindow* ew, Annotation* annot) {
    if (!AnnotationSupportsInteriorColor(annot->type)) {
        return;
    }
    PdfColor col = InteriorColor(annot);
    DropDownFillColors(ew->dropDownInteriorColor, col, ew->currCustomInteriorColor);
    ew->staticInteriorColor->SetIsVisible(true);
    ew->dropDownInteriorColor->SetIsVisible(true);
}

static void InteriorColorSelectionChanged(EditAnnotationsWindow* ew) {
    auto annot = ew->tab->selectedAnnotation;
    if (!annot || !annot->engine) {
        return;
    }
    auto idx = ew->dropDownInteriorColor->GetCurrentSelection();
    auto item = ew->dropDownInteriorColor->items.At(idx);
    auto col = GetDropDownColor(item);
    SetInteriorColor(annot, col);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void DoOpacity(EditAnnotationsWindow* ew, Annotation* annot) {
    if (Type(annot) != AnnotationType::Highlight) {
        return;
    }
    int opacity = Opacity(ew->tab->selectedAnnotation);
    TempStr s = str::FormatTemp(_TRA("Opacity: %d"), opacity);
    ew->staticOpacity->SetText(s);
    ew->staticOpacity->SetIsVisible(true);
    ew->trackbarOpacity->SetIsVisible(true);
    ew->trackbarOpacity->SetValue(opacity);
}

static void DoSaveEmbed(EditAnnotationsWindow* ew, Annotation* annot) {
    if (Type(annot) != AnnotationType::FileAttachment) {
        return;
    }
    ew->buttonSaveAttachment->SetIsVisible(true);
    ew->buttonEmbedAttachment->SetIsVisible(true);
}

static void OpacityChanging(EditAnnotationsWindow* ew, Trackbar::PositionChangingEvent* ev) {
    auto annot = ew->tab->selectedAnnotation;
    if (!annot || !annot->engine) {
        return;
    }
    int opacity = ev->pos;
    SetOpacity(annot, opacity);
    TempStr s = str::FormatTemp(_TRA("Opacity: %d"), opacity);
    ew->staticOpacity->SetText(s);
    EnableSaveIfAnnotationsChanged(ew);
    MainWindowRerender(ew->tab->win);
}

static void LayoutAnnotationsPanel(EditAnnotationsWindow* ew) {
    Rect client = ClientRect(ew->hwnd);
    if (client.dx <= 0 || client.dy <= 0 || !ew->mainLayout) {
        return;
    }

    ew->viewportHeight = client.dy;
    ew->contentHeight = std::max(client.dy, ew->mainLayout->Layout(ExpandHeight(client.dx)).dy);
    ew->scrollPos = limitValue(ew->scrollPos, 0, ew->contentHeight - client.dy);
    LayoutToSize(ew->mainLayout, {client.dx, ew->contentHeight});

    SCROLLINFO si = {sizeof(si), SIF_RANGE | SIF_PAGE | SIF_POS};
    si.nMin = 0;
    si.nMax = ew->contentHeight - 1;
    si.nPage = (UINT)client.dy;
    si.nPos = ew->scrollPos;
    SetScrollInfo(ew->hwnd, SB_VERT, &si, TRUE);
    if (ew->scrollPos > 0) {
        ScrollWindowEx(ew->hwnd, 0, -ew->scrollPos, nullptr, nullptr, nullptr, nullptr,
                       SW_SCROLLCHILDREN | SW_INVALIDATE | SW_ERASE);
    }
}

// TODO: maybe use ew->tab->selectedAnnotation instead of annot
static void UpdateUIForSelectedAnnotation(EditAnnotationsWindow* ew, Annotation* annot, bool isNew = false,
                                          EditAnnotFocus focus = EditAnnotFocus::Default) {
    HidePerAnnotControls(ew);
    if (annot) {
        int itemNo = ew->annotations.Find(annot);
        if (itemNo < 0) {
            // can happen if annotations list is out of sync (e.g. after reload)
            return;
        }

        DoRect(ew, annot);
        DoAuthor(ew, annot);
        DoModificationDate(ew, annot);
        DoPopup(ew, annot);
        DoContents(ew, annot);

        DoTextAlignment(ew, annot);
        DoTextFont(ew, annot);
        DoTextSize(ew, annot);
        DoTextColor(ew, annot);

        DoLineStartEnd(ew, annot);

        DoIcon(ew, annot);

        DoBorder(ew, annot);
        DoColor(ew, annot);
        DoInteriorColor(ew, annot);

        DoOpacity(ew, annot);
        DoSaveEmbed(ew, annot);

        ew->listBox->SetCurrentSelection(itemNo);
        ew->buttonDelete->SetIsVisible(true);

        if (focus == EditAnnotFocus::Edit) {
            HwndSetFocus(ew->editContents->hwnd);
            ew->editContents->SelectAll();
        } else if (focus == EditAnnotFocus::List) {
            HwndSetFocus(ew->listBox->hwnd);
        } else if (isNew && annot->type == AnnotationType::FreeText) {
            HwndSetFocus(ew->editContents->hwnd);
            // ew->editContents->SetCursorPositionAtEnd();
            ew->editContents->SelectAll();
        } else {
            HwndSetFocus(ew->listBox->hwnd);
        }
    }

    ew->scrollPos = 0;
    LayoutAnnotationsPanel(ew);

    if (!annot) {
        return;
    }
    if (ew->skipGoToPage) {
        ew->skipGoToPage = false;
        return;
    }

    int annotPageNo = annot->pageNo;
    DisplayModel* dm = ew->tab->AsFixed();
    int nPages = dm->PageCount();
    if (annotPageNo > nPages) {
        // see https://github.com/sumatrapdfreader/sumatrapdf/issues/1701
        logf("UpdateUIForSelectedAnnotation: invalid annotPageNo (%d), should be <= than nPages (%d)\n", annotPageNo,
             nPages);
        ReportIf(annotPageNo > nPages);
        return;
    }

    // don't switch pages if already visible. needed for cases where
    // we show more than one page at a time and GoToPage() scrolls
    // to top page
    // TODO: this is not perfect. We should skipGoToPage if this
    // is caused by creating an annotation. by definition the page
    // was visible when user created an annotation.
    // but that requires passing down more stuff
    if (!dm->PageVisible(annotPageNo)) {
        dm->GoToPage(annotPageNo, true);
    }
}

static void ButtonSaveAttachment(EditAnnotationsWindow* ew) {
    Annotation* annot = ew->tab->selectedAnnotation;
    ReportIf(!annot);
    if (!annot || annot->type != AnnotationType::FileAttachment) {
        return;
    }
    EngineMupdf* engine = GetEngineMupdf(ew);
    if (!engine) {
        return;
    }
    fz_context* ctx = engine->Ctx();
    pdf_annot* pdfannot = annot->pdfannot;
    if (!pdfannot) {
        return;
    }

    int objNum = pdf_to_num(ctx, pdf_annot_obj(ctx, pdfannot));
    ByteSlice data = EngineMupdfLoadAnnotAttachment((EngineBase*)engine, objNum);
    if (data.empty()) {
        return;
    }

    const char* fileName = nullptr;
    pdf_obj* fs = pdf_annot_filespec(ctx, pdfannot);
    if (fs) {
        pdf_filespec_params fileParams = {};
        pdf_get_filespec_params(ctx, fs, &fileParams);
        fileName = fileParams.filename;
    }
    if (str::IsEmpty(fileName)) {
        fileName = "attachment";
    }

    TempStr dir = path::GetDirTemp(ew->tab->filePath);
    fileName = path::GetBaseNameTemp(fileName);
    TempStr dstPath = path::JoinTemp(dir, fileName);
    SaveDataToFile(ew->hwnd, dstPath, data);
    str::Free(data.data());
}

static void ButtonEmbedAttachment(EditAnnotationsWindow* ew) {
    ReportIf(!ew->tab->selectedAnnotation);
    // TODO: implement me
    MessageBoxNYI(ew->hwnd);
}

void SetSelectedAnnotation(WindowTab* tab, Annotation* annot, bool isNew, EditAnnotFocus focus) {
    // when we delete an annotation we automatically pick one to
    // set as selected and it might end up as currently selected
    // we still want to redraw to not show deleted annotation
    // but not do the rest of the logic as it triggers infinite loop
    // TODO: maybe if we already have selected annotation, do not auto-pick
    MainWindow* win = tab->win;
    auto ew = tab->editAnnotsWindow;
    if (annot == tab->selectedAnnotation) {
        if (ew && annot) {
            ew->SetIsVisible(true);
            HwndMakeVisible(ew->hwnd);
            RedrawWindow(ew->hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
        }
        MainWindowRerender(win);
        if (ew) {
            UpdateUIForSelectedAnnotation(ew, annot, isNew, focus);
        }
        ToolbarUpdateStateForWindow(win, false);
        return;
    }
    tab->selectedAnnotation = annot;
    tab->didScrollToSelectedAnnotation = false;
    if (annot && ew) {
        ew->SetIsVisible(true);
        HwndMakeVisible(ew->hwnd);
        RedrawWindow(ew->hwnd, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
    // go to page with a given annotations before triggering repaint
    if (ew) {
        UpdateUIForSelectedAnnotation(ew, annot, isNew, focus);
        HwndMakeVisible(ew->hwnd);
    }
    MainWindowRerender(win);
    ToolbarUpdateStateForWindow(win, false);
}

void UpdateAnnotationsList(EditAnnotationsWindow* ew) {
    if (!ew) {
        return;
    }
    auto engine = GetEngineMupdf(ew);
    EngineMupdfGetAnnotations(engine, ew->annotations);
    RebuildAnnotationsListBox(ew);
}

static void ButtonDeleteHandler(EditAnnotationsWindow* ew) {
    ReportIf(!ew->tab->selectedAnnotation);
    DeleteSelectedAnnotation(ew);
}

static void ListBoxSelectionChanged(EditAnnotationsWindow* ew) {
    ew->ListBoxSelectionChanged();
}

void EditAnnotationsWindow::ListBoxSelectionChanged() {
    int itemNo = listBox->GetCurrentSelection();
    if (itemNo < 0) {
        // an item has been deselected because e.g. selected annotation was deleted
        return;
    }
    if (!annotations.isValidIndex(itemNo)) {
        logfa("EditAnnotationsWindow::ListBoxSelectionChanged: invalid itemNo=%d, annotations.size()=%d\n", itemNo,
              annotations.Size());
        ReportDebugIf(true);
        return;
    }
    Annotation* annot = annotations.at(itemNo);
    SetSelectedAnnotation(tab, annot);
}

static UINT_PTR gMainWindowRerenderTimer = 0;
static MainWindow* gMainWindowForRender = nullptr;

// TODO: there seems to be a leak
static void ContentsChanged(EditAnnotationsWindow* ew) {
    auto a = ew->tab->selectedAnnotation;
    // TODO: saw a crash when this was null
    ReportDebugIf(!a);
    if (!a) {
        return;
    }
    auto txt = ew->editContents->GetTextTemp();
    txt = str::ReplaceTemp(txt, "\r\n", "\n");
    SetContents(a, txt);
    EnableSaveIfAnnotationsChanged(ew);

    MainWindow* win = ew->tab->win;
    if (gMainWindowRerenderTimer != 0) {
        // logf("ContentsChanged: killing existing timer for re-render of MainWindow\n");
        KillTimer(win->hwndCanvas, gMainWindowRerenderTimer);
        gMainWindowRerenderTimer = 0;
    }
    UINT timeoutInMs = 1000;
    gMainWindowForRender = win;
    gMainWindowRerenderTimer = SetTimer(win->hwndCanvas, 1, timeoutInMs, [](HWND, UINT, UINT_PTR, DWORD) {
        if (IsMainWindowValid(gMainWindowForRender)) {
            // logf("ContentsChanged: re-rendering MainWindow\n");
            MainWindowRerender(gMainWindowForRender);
        } else {
            // logf("ContentsChanged: NOT re-rendering MainWindow because is not valid anymore\n");
        }
        gMainWindowRerenderTimer = 0;
    });
}

void EditAnnotationsWindow::OnSize(UINT msg, UINT, SIZE size) {
    if (msg != WM_SIZE) {
        return;
    }
    if (!mainLayout) {
        return;
    }
    int dx = (int)size.cx;
    int dy = (int)size.cy;
    if (dx == 0 || dy == 0) {
        return;
    }
    // Ensure background is erased to avoid painting artifacts on resize
    InvalidateRect(hwnd, nullptr, TRUE);
    if (false && mainLayout->lastBounds.EqSize(dx, dy)) {
        // avoid un-necessary layout
        return;
    }
    LayoutAnnotationsPanel(this);
}

static Static* CreateStatic(HWND parent, const char* s = nullptr) {
    auto w = new Static();
    Static::CreateArgs args;
    args.parent = parent;
    args.text = s;
    args.isRtl = IsUIRtl();
    args.font = GetAppFont();
    HWND hwnd = w->Create(args);
    ReportIf(!hwnd);
    return w;
}

static void CreateMainLayout(EditAnnotationsWindow* ew) {
    HWND parent = ew->hwnd;
    auto vbox = new VBox();
    vbox->alignMain = MainAxisAlign::MainStart;
    vbox->alignCross = CrossAxisAlign::Stretch;
    HFONT fnt = GetAppFont();

    {
        auto* header = new LabelWithCloseWnd();
        LabelWithCloseWnd::CreateArgs args;
        args.parent = parent;
        args.font = GetDefaultGuiFont(true, false);
        args.cmdId = 1;
        args.isRtl = IsUIRtl();
        header->Create(args);
        header->SetLabel(_TRA("Annotations"));
        header->SetPaddingXY(4, 4);
        ew->headerLabel = header;
        vbox->AddChild(header);
    }

    {
        ListBox::CreateArgs args;
        args.parent = parent;
        args.idealSizeLines = 6;
        args.font = fnt;
        args.isRtl = IsUIRtl();
        auto w = new ListBox();
        w->SetInsetsPt(6, 0);
        w->Create(args);
        //styleInput(w);
        auto lbModel = new ListBoxModelStrings(); // Use a local variable for lbModel
        w->SetModel(lbModel);
        w->onSelectionChanged = MkFunc0(ListBoxSelectionChanged, ew);
        ew->listBox = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent);
        ew->staticRect = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent);
        // WindowBaseLayout* l2 = (WindowBaseLayout*)l;
        // l2->SetInsetsPt(20, 0, 0, 0);
        ew->staticAuthor = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent);
        ew->staticModificationDate = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent);
        ew->staticPopup = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRA("Contents:"));
        ew->staticContents = w;
        w->SetInsetsPt(4, 0, 0, 0);
        vbox->AddChild(w);
    }

    {
        Edit::CreateArgs args;
        args.parent = parent;
        args.isMultiLine = true;
        args.idealSizeLines = 5;
        args.font = fnt;
        args.isRtl = IsUIRtl();
        auto w = new Edit();
        HWND hwnd = w->Create(args);
        ReportIf(!hwnd);
        w->SetInsetsPt(4, 0, 0, 0);
        w->maxDx = 150;
        //styleInput(w);
        w->onTextChanged = MkFunc0(ContentsChanged, ew);
        ew->editContents = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRA("Text Alignment:"));
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticTextAlignment = w;
        vbox->AddChild(w);
    }

    {
        DropDown::CreateArgs args;
        args.parent = parent;
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);
        w->Create(args);

        w->SetItemsSeqStrings(gQuaddingNames);
        w->onSelectionChanged = MkFunc0(TextAlignmentSelectionChanged, ew);
        ew->dropDownTextAlignment = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRA("Text Font:"));
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticTextFont = w;
        vbox->AddChild(w);
    }

    {
        DropDown::CreateArgs args;
        args.parent = parent;
        args.font = fnt;
        args.isRtl = IsUIRtl();
        auto w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);

        w->Create(args);
        w->SetItemsSeqStrings(gQuaddingNames);
        w->onSelectionChanged = MkFunc0(TextFontSelectionChanged, ew);
        ew->dropDownTextFont = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRA("Text Size:"));
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticTextSize = w;
        vbox->AddChild(w);
    }

    {
        Trackbar::CreateArgs args;
        args.parent = parent;
        args.rangeMin = 8;
        args.rangeMax = 36;
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto w = new Trackbar();
        w->SetInsetsPt(4, 0, 0, 0);

        w->Create(args);

        w->onPositionChanging = MkFunc1(TextFontSizeChanging, ew);
        ew->trackbarTextSize = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRA("Text Color:"));
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticTextColor = w;
        vbox->AddChild(w);
    }

    {
        DropDown::CreateArgs args;
        args.parent = parent;
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);
        w->Create(args);

        w->SetItemsSeqStrings(gColors);
        w->onSelectionChanged = MkFunc0(TextColorSelectionChanged, ew);
        ew->dropDownTextColor = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRA("Line Start:"));
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticLineStart = w;
        vbox->AddChild(w);
    }

    {
        DropDown::CreateArgs args;
        args.parent = parent;
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);
        w->Create(args);

        w->onSelectionChanged = MkFunc0(LineStartSelectionChanged, ew);
        ew->dropDownLineStart = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRA("Line End:"));
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticLineEnd = w;
        vbox->AddChild(w);
    }

    {
        DropDown::CreateArgs args;
        args.parent = parent;
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);
        w->Create(args);

        w->onSelectionChanged = MkFunc0(LineEndSelectionChanged, ew);
        ew->dropDownLineEnd = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRA("Icon:"));
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticIcon = w;
        vbox->AddChild(w);
    }

    {
        DropDown::CreateArgs args;
        args.parent = parent;
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);
        w->Create(args);

        w->onSelectionChanged = MkFunc0(IconSelectionChanged, ew);
        ew->dropDownIcon = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, "Border:");
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticBorder = w;
        vbox->AddChild(w);
    }

    {
        Trackbar::CreateArgs args;
        args.parent = parent;
        args.rangeMin = borderWidthMin;
        args.rangeMax = borderWidthMax;
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto w = new Trackbar();
        w->SetInsetsPt(4, 0, 0, 0);
        w->Create(args);
        w->onPositionChanging = MkFunc1(BorderWidthChanging, ew);
        ew->trackbarBorder = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRA("Color:"));
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticColor = w;
        vbox->AddChild(w);
    }

    {
        DropDown::CreateArgs args;
        args.parent = parent;
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);
        w->Create(args);
        w->SetItemsSeqStrings(gColors);
        w->onSelectionChanged = MkFunc0(ColorSelectionChanged, ew);
        ew->dropDownColor = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRA("Interior Color:"));
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticInteriorColor = w;
        vbox->AddChild(w);
    }

    {
        DropDown::CreateArgs args;
        args.parent = parent;
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto w = new DropDown();
        w->SetInsetsPt(4, 0, 0, 0);
        w->Create(args);

        w->SetItemsSeqStrings(gColors);
        w->onSelectionChanged = MkFunc0(InteriorColorSelectionChanged, ew);
        ew->dropDownInteriorColor = w;
        vbox->AddChild(w);
    }

    {
        auto w = CreateStatic(parent, _TRA("Opacity:"));
        w->SetInsetsPt(8, 0, 0, 0);
        ew->staticOpacity = w;
        vbox->AddChild(w);
    }

    {
        Trackbar::CreateArgs args;
        args.parent = parent;
        args.rangeMin = 0;
        args.rangeMax = 255;
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto w = new Trackbar();
        w->Create(args);

        w->onPositionChanging = MkFunc1(OpacityChanging, ew);
        ew->trackbarOpacity = w;
        vbox->AddChild(w);
    }

    {
        Button::CreateArgs args;
        args.parent = parent;
        args.text = _TRA("Save...");
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto w = new Button();
        w->SetInsetsPt(8, 0, 0, 0);
        HWND hwnd = w->Create(args);
        ReportIf(!hwnd);

        w->onClick = MkFunc0(ButtonSaveAttachment, ew);
        ew->buttonSaveAttachment = w;
        vbox->AddChild(w);
    }

    {
        Button::CreateArgs args;
        args.parent = parent;
        args.text = _TRA("Embed...");
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto w = new Button();
        w->SetInsetsPt(8, 0, 0, 0);
        HWND hwnd = w->Create(args);
        ReportIf(!hwnd);

        w->onClick = MkFunc0(ButtonEmbedAttachment, ew);
        ew->buttonEmbedAttachment = w;
        vbox->AddChild(w);
    }

    {
        Button::CreateArgs args;
        args.parent = parent;
        args.text = _TRA("Delete");
        args.font = fnt;
        args.isRtl = IsUIRtl();

        auto w = new Button();
        w->SetInsetsPt(11, 0, 0, 0);
        HWND hwnd = w->Create(args);
        ReportIf(!hwnd);

        // TODO: doesn't work
        // w->SetTextColor(MkColor(0xff, 0, 0));

        w->onClick = MkFunc0(ButtonDeleteHandler, ew);
        ew->buttonDelete = w;
        vbox->AddChild(w);
        }

        {
            // used to take all available space between the what's above and below
            auto w = new Spacer(0, 0);
            vbox->AddChild(w, 1);
        }

        {
            Button::CreateArgs args;
            args.parent = parent;
            args.text = _TRA("Save to current PDF");
            args.font = fnt;
            args.isRtl = IsUIRtl();

            auto w = new Button();
            HWND hwnd = w->Create(args);
            ReportIf(!hwnd);

            w->SetIsEnabled(false); // only enabled if there are changes
            w->onClick = MkFunc0(ButtonSaveToCurrentPDFHandler, ew);
            ew->buttonSaveToCurrentFile = w;
            vbox->AddChild(w);
        }

        {
            Button::CreateArgs args;
            args.parent = parent;
            args.text = _TRA("Save as new PDF");
            args.font = fnt;
            args.isRtl = IsUIRtl();

            auto w = new Button();
            HWND hwnd = w->Create(args);
            ReportIf(!hwnd);

            w->SetIsEnabled(false); // only enabled if there are changes
            w->onClick = MkFunc0(ButtonSaveToNewFileHandler, ew);
            ew->buttonSaveToNewFile = w;
            vbox->AddChild(w);
        }

    auto padding = new Padding(vbox, DpiScaledInsets(parent, 4, 8));
    ew->mainLayout = padding;
    HidePerAnnotControls(ew);
}

void ShowEditAnnotationsWindow(WindowTab* tab, Annotation* annot, EditAnnotFocus focus) {
    if (!tab) {
        return;
    }

    auto engine = tab->GetEngine();
    auto canAnnotate = EngineSupportsAnnotations(engine);
    if (!canAnnotate) {
        ReportDebugIf(true);
        return;
    }
    EditAnnotationsWindow* ew = tab->editAnnotsWindow;
    if (ew) {
        bool isNew = annot != ew->tab->win->annotationUnderCursor;
        HwndMakeVisible(ew->hwnd);
        SetForegroundWindow(ew->hwnd);
        if (ew->listBox && ew->listBox->model->ItemsCount() > 0) {
            HwndSetFocus(ew->listBox->hwnd);
        }
        if (!annot) return;
        SetSelectedAnnotation(tab, annot, isNew, focus);
        return;
    }
    ew = new EditAnnotationsWindow();
    ew->onClose = MkFunc1Void(OnClose);
    CreateCustomArgs args;
    args.bgColor = ThemeWindowControlBackgroundColor();

    args.title = str::JoinTemp(_TRA("Annotations"), ": ", tab->GetTabTitle());
    args.parent = tab->win->hwndFrame;
    args.style = WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS | WS_VSCROLL;
    args.visible = false;
    args.font = GetAppFont();

    ew->CreateCustom(args);

    CreateMainLayout(ew);
    ew->tab = tab;
    tab->editAnnotsWindow = ew;

    UpdateAnnotationsList(ew);

    Rect lastPos = tab->lastEditAnnotsWindowPos;
    // size our editor window to be the same height as main window
    int minDy = lastPos.dy;
    if (minDy == 0) {
        minDy = 720;
        // TODO: this is slightly less that wanted
        HWND hwnd = tab->win->hwndCanvas;
        auto rc = ClientRect(hwnd);
        if (rc.dy > 0) {
            minDy = rc.dy;
        }
    }

    // if it's a tall window, up the number of items in list box
    // from 5 to 14
    if (minDy > 1024) {
        ew->listBox->idealSizeLines = 14;
    }

    ew->SetIsVisible(true);
    // Ensure child window is actually shown (SetIsVisible sets internal state
    // but for child windows we also need to make it visible in the z-order).
    HwndMakeVisible(ew->hwnd);
    UpdateWindow(ew->hwnd);
    RelayoutWindow(tab->win);
    if (lastPos.IsEmpty() && tab->lastEditAnnotsWindowPos.dx <= 0) {
        tab->lastEditAnnotsWindowPos.dx = kEditAnnotationsSidebarDefaultDx;
    }
    if (!annot) annot = ew->tab->selectedAnnotation;
    ew->skipGoToPage = (annot != nullptr);
    if (annot) {
        bool isNew = annot != ew->tab->win->annotationUnderCursor;
        SetSelectedAnnotation(tab, annot, isNew, focus);
    }
    if (UseDarkModeLib()) {
        DarkMode::setDarkWndNotifySafe(ew->hwnd);
        DarkMode::setWindowEraseBgSubclass(ew->hwnd);
    }

    // important to call this after hooking up onSize to ensure
    // first layout is triggered
    ew->SetIsVisible(true);
    RedrawWindow(tab->win->hwndFrame, nullptr, nullptr, RDW_ERASE | RDW_INVALIDATE | RDW_ALLCHILDREN | RDW_UPDATENOW);
}

void SetEditAnnotationsWindowVisible(WindowTab* tab, bool visible) {
    if (tab && tab->editAnnotsWindow) {
        bool wasVisible = tab->editAnnotsWindow->IsVisible();
        tab->editAnnotsWindow->SetIsVisible(visible);
        if (wasVisible != visible && tab->win) {
            RelayoutWindow(tab->win);
        }
    }
}

bool IsEditAnnotationsWindowVisible(WindowTab* tab) {
    return tab && tab->editAnnotsWindow && tab->editAnnotsWindow->IsVisible();
}

HWND GetEditAnnotationsWindowHwnd(WindowTab* tab) {
    if (tab && tab->editAnnotsWindow) {
        return tab->editAnnotsWindow->hwnd;
    }
    return nullptr;
}