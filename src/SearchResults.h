/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: GPLv3 */

#pragma once

#include "utils/BaseUtil.h"

template <typename T>
class Vec;

struct MainWindow;

void CreateSearchResults(MainWindow* win);
bool IsSearchResultsSidebarActive(MainWindow* win);
void PopulateSearchResultsTreeIfNeeded(MainWindow* win);
void UpdateSearchResultsTree(MainWindow* win);
bool GoToSearchResultForTreeItem(MainWindow* win, UINT_PTR ti);

void ShowSearchResultsSidebar(MainWindow* win, const char* query, const Vec<int>& pageHits, int totalHits,
                              bool autoOpen);
void ClearSearchResultsSidebar(MainWindow* win);
const char* GetFavoritesSidebarTitle(MainWindow* win);
