/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#include "utils/BaseUtil.h"
#include "utils/WinUtil.h"

#include "wingui/UIModels.h"
#include "wingui/Layout.h"
#include "wingui/WinGui.h"
#include "wingui/LabelWithCloseWnd.h"

#include "Settings.h"
#include "DocController.h"
#include "EngineBase.h"
#include "GlobalPrefs.h"
#include "AppSettings.h"
#include "MainWindow.h"
#include "SearchResults.h"
#include "SumatraPDF.h"
#include "Favorites.h"
#include "Translations.h"
#include "resource.h"

bool HighlightSearchResultsOnPage(MainWindow* win, const char* text, int pageNo);

static void SearchResultsTreeItemClicked(TreeView::ClickEvent* ev) {
    if (ev->treeItem != ev->treeView->GetSelection()) {
        return;
    }
    MainWindow* win = FindMainWindowByHwnd(ev->treeView->hwnd);
    if (!win) {
        return;
    }
    GoToSearchResultForTreeItem(win, ev->treeItem);
}

static void SearchResultsTreeSelectionChanged(TreeView::SelectionChangedEvent* ev) {
    MainWindow* win = FindMainWindowByHwnd(ev->treeView->hwnd);
    if (!win) {
        return;
    }
    if (!ev->byKeyboard && !ev->byMouse) {
        return;
    }
    GoToSearchResultForTreeItem(win, ev->selectedItem);
}

static void SearchResultsTreeContextMenu(ContextMenuEvent*) {}

static WNDPROC gWndProcSearchResultsBox = nullptr;
static LRESULT CALLBACK WndProcSearchResultsBox(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    MainWindow* win = FindMainWindowByHwnd(hwnd);
    if (!win) {
        return CallWindowProc(gWndProcSearchResultsBox, hwnd, msg, wp, lp);
    }

    LRESULT res = TryReflectMessages(hwnd, msg, wp, lp);
    if (res) {
        return res;
    }

    TreeView* treeView = win->searchResultsTreeView;
    switch (msg) {
        case WM_SIZE:
            if (treeView && win->searchResultsLabelWithClose) {
                LayoutTreeContainer(win->searchResultsLabelWithClose, treeView->hwnd);
            }
            break;

        case WM_COMMAND:
            if (LOWORD(wp) == IDC_SEARCH_RESULTS_LABEL_WITH_CLOSE) {
                ClearSearchResultsSidebar(win);
                SetSidebarVisibility(win, win->tocVisible, gGlobalPrefs->showFavorites);
            }
            break;
    }
    return CallWindowProc(gWndProcSearchResultsBox, hwnd, msg, wp, lp);
}

void CreateSearchResults(MainWindow* win) {
    HMODULE h = GetModuleHandleW(nullptr);
    int dx = gGlobalPrefs->sidebarDx;
    DWORD dwStyle = WS_CHILD | WS_CLIPCHILDREN | WS_CLIPSIBLINGS;
    win->hwndSearchResultsBox =
        CreateWindowW(WC_STATIC, L"", dwStyle, 0, 0, dx, 0, win->hwndFrame, (HMENU) nullptr, h, nullptr);

    auto* l = new LabelWithCloseWnd();
    {
        LabelWithCloseWnd::CreateArgs args;
        args.parent = win->hwndSearchResultsBox;
        args.cmdId = IDC_SEARCH_RESULTS_LABEL_WITH_CLOSE;
        args.font = GetDefaultGuiFont(true, false);
        args.isRtl = IsUIRtl();
        l->Create(args);
    }
    win->searchResultsLabelWithClose = l;
    l->SetPaddingXY(2, 2);
    l->SetLabel(_TRA("Results"));

    auto* treeView = new TreeView();
    TreeView::CreateArgs args;
    args.parent = win->hwndSearchResultsBox;
    args.font = GetAppTreeFont();
    args.fullRowSelect = true;
    args.exStyle = 0;
    args.isRtl = IsUIRtl();

    treeView->onContextMenu = MkFunc1Void(SearchResultsTreeContextMenu);
    treeView->onSelectionChanged = MkFunc1Void(SearchResultsTreeSelectionChanged);
    treeView->onClick = MkFunc1Void(SearchResultsTreeItemClicked);

    treeView->Create(args);
    win->searchResultsTreeView = treeView;

    if (nullptr == gWndProcSearchResultsBox) {
        gWndProcSearchResultsBox = (WNDPROC)GetWindowLongPtr(win->hwndSearchResultsBox, GWLP_WNDPROC);
    }
    SetWindowLongPtr(win->hwndSearchResultsBox, GWLP_WNDPROC, (LONG_PTR)WndProcSearchResultsBox);
}

struct SearchResultsTreeItem {
    ~SearchResultsTreeItem();

    HTREEITEM hItem = nullptr;
    SearchResultsTreeItem* parent = nullptr;
    char* text = nullptr;
    int pageNo = 0;

    Vec<SearchResultsTreeItem*> children;
};

SearchResultsTreeItem::~SearchResultsTreeItem() {
    str::Free(text);
    DeleteVecMembers(children);
}

struct SearchResultsTreeModel : TreeModel {
    ~SearchResultsTreeModel() override;

    TreeItem Root() override;
    char* Text(TreeItem) override;
    TreeItem Parent(TreeItem) override;
    int ChildCount(TreeItem) override;
    TreeItem ChildAt(TreeItem, int index) override;
    bool IsExpanded(TreeItem) override;
    bool IsChecked(TreeItem) override;
    void SetHandle(TreeItem, HTREEITEM) override;
    HTREEITEM GetHandle(TreeItem) override;

    SearchResultsTreeItem* root = nullptr;
};

SearchResultsTreeModel::~SearchResultsTreeModel() {
    delete root;
}

TreeItem SearchResultsTreeModel::Root() {
    return (TreeItem)root;
}

char* SearchResultsTreeModel::Text(TreeItem ti) {
    auto* item = (SearchResultsTreeItem*)ti;
    return item->text;
}

TreeItem SearchResultsTreeModel::Parent(TreeItem ti) {
    auto* item = (SearchResultsTreeItem*)ti;
    return (TreeItem)item->parent;
}

int SearchResultsTreeModel::ChildCount(TreeItem ti) {
    auto* item = (SearchResultsTreeItem*)ti;
    if (!item) {
        return 0;
    }
    return (int)item->children.size();
}

TreeItem SearchResultsTreeModel::ChildAt(TreeItem ti, int idx) {
    auto* item = (SearchResultsTreeItem*)ti;
    return (TreeItem)item->children[idx];
}

bool SearchResultsTreeModel::IsExpanded(TreeItem) {
    return false;
}

bool SearchResultsTreeModel::IsChecked(TreeItem) {
    return false;
}

void SearchResultsTreeModel::SetHandle(TreeItem ti, HTREEITEM hItem) {
    auto* item = (SearchResultsTreeItem*)ti;
    item->hItem = hItem;
}

HTREEITEM SearchResultsTreeModel::GetHandle(TreeItem ti) {
    auto* item = (SearchResultsTreeItem*)ti;
    return item->hItem;
}

bool IsSearchResultsSidebarActive(MainWindow* win) {
    return win && win->sidebarContentMode == SidebarContentMode::SearchResults;
}

static SearchResultsTreeModel* BuildSearchResultsTreeModel(MainWindow* win) {
    auto* model = new SearchResultsTreeModel();
    model->root = new SearchResultsTreeItem();

    int totalHits = win->sidebarSearchTotalHits;
    if (str::IsEmpty(win->sidebarSearchQuery.Get()) || totalHits <= 0) {
        auto* empty = new SearchResultsTreeItem();
        empty->text = str::Dup(_TRA("No matches"));
        empty->parent = model->root;
        model->root->children.Append(empty);
        return model;
    }

    size_t n = win->sidebarSearchPageHits.size();
    for (size_t i = 0; i < n; i++) {
        int hits = win->sidebarSearchPageHits.at(i);
        if (hits <= 0) {
            continue;
        }

        int pageNo = (int)i + 1;
        TempStr pageLabel = str::FormatTemp("%d", pageNo);
        if (win->ctrl && win->ctrl->ValidPageNo(pageNo)) {
            pageLabel = win->ctrl->GetPageLabeTemp(pageNo);
        }

        auto* row = new SearchResultsTreeItem();
        row->parent = model->root;
        row->pageNo = pageNo;
        row->text = str::Dup(str::FormatTemp("Page %s (%d)", pageLabel, hits));
        model->root->children.Append(row);
    }

    if (model->root->children.size() == 0) {
        auto* empty = new SearchResultsTreeItem();
        empty->text = str::Dup(_TRA("No matches"));
        empty->parent = model->root;
        model->root->children.Append(empty);
    }

    return model;
}

void PopulateSearchResultsTreeIfNeeded(MainWindow* win) {
    if (!IsSearchResultsSidebarActive(win) || !win->searchResultsTreeView) {
        return;
    }
    if (win->searchResultsTreeView->treeModel) {
        return;
    }

    TreeModel* tm = BuildSearchResultsTreeModel(win);
    win->searchResultsTreeView->SetTreeModel(tm);
}

void UpdateSearchResultsTree(MainWindow* win) {
    if (!IsSearchResultsSidebarActive(win) || !win->searchResultsTreeView) {
        return;
    }

    auto* prevModel = win->searchResultsTreeView->treeModel;
    TreeModel* newModel = BuildSearchResultsTreeModel(win);
    win->searchResultsTreeView->SetTreeModel(newModel);
    delete prevModel;

    SetSidebarVisibility(win, win->tocVisible, gGlobalPrefs->showFavorites);
    if (win->searchResultsLabelWithClose) {
        win->searchResultsLabelWithClose->SetLabel(_TRA("Results"));
    }
}

bool GoToSearchResultForTreeItem(MainWindow* win, UINT_PTR ti) {
    if (!IsSearchResultsSidebarActive(win) || !ti) {
        return false;
    }

    auto* item = (SearchResultsTreeItem*)ti;
    if (item->pageNo <= 0 || !win->IsDocLoaded() || !win->ctrl || !win->ctrl->ValidPageNo(item->pageNo)) {
        return false;
    }

    if (!str::IsEmpty(win->sidebarSearchQuery.Get())) {
        HighlightSearchResultsOnPage(win, win->sidebarSearchQuery, item->pageNo);
    } else {
        win->ctrl->GoToPage(item->pageNo, true);
    }
    win->Focus();
    return true;
}

void ShowSearchResultsSidebar(MainWindow* win, const char* query, const Vec<int>& pageHits, int totalHits,
                              bool autoOpen) {
    if (!win) {
        return;
    }

    win->sidebarContentMode = SidebarContentMode::SearchResults;
    win->sidebarSearchQuery.SetCopy(query);
    win->sidebarSearchTotalHits = totalHits;
    win->sidebarSearchPageHits.Reset();

    int n = (int)pageHits.size();
    win->sidebarSearchPageHits.SetSize(n);
    for (int i = 0; i < n; i++) {
        win->sidebarSearchPageHits[i] = pageHits.at(i);
    }

    if (autoOpen && totalHits > 0) {
        gGlobalPrefs->showFavorites = true;
    }

    UpdateSearchResultsTree(win);
    if (win->searchResultsLabelWithClose) {
        win->searchResultsLabelWithClose->SetLabel(_TRA("Results"));
    }
}

void ClearSearchResultsSidebar(MainWindow* win) {
    if (!win || win->sidebarContentMode != SidebarContentMode::SearchResults) {
        return;
    }

    win->sidebarContentMode = SidebarContentMode::Favorites;
    win->sidebarSearchQuery.SetCopy(nullptr);
    win->sidebarSearchPageHits.Reset();
    win->sidebarSearchTotalHits = 0;
    UpdateFavoritesTree(win);
}

const char* GetFavoritesSidebarTitle(MainWindow* win) {
    if (IsSearchResultsSidebarActive(win)) {
        return _TRA("Results");
    }
    return _TRA("Favorites");
}
