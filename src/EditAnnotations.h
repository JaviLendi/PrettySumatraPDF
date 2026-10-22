/* Copyright 2022 the SumatraPDF project authors (see AUTHORS file).
   License: Simplified BSD (see COPYING.BSD) */

#pragma once

struct EditAnnotationsWindow;
struct Rect;

enum class EditAnnotFocus {
    Default,
    Edit,
    List,
};

void ShowEditAnnotationsWindow(WindowTab*, Annotation*, EditAnnotFocus focus = EditAnnotFocus::Default);
bool CloseAndDeleteEditAnnotationsWindow(WindowTab*);
void DeleteAnnotationAndUpdateUI(WindowTab*, Annotation*);
void SetSelectedAnnotation(WindowTab*, Annotation*, bool isNew = false, EditAnnotFocus focus = EditAnnotFocus::Default);
void UpdateAnnotationsList(EditAnnotationsWindow*);
void NotifyAnnotationsChanged(EditAnnotationsWindow*);
void UpdateEditAnnotationsColors(WindowTab*);
void UpdateEditAnnotationsColors(EditAnnotationsWindow*); 
class DeferWinPosHelper;
void LayoutEditAnnotationsWindow(WindowTab*, Rect&, DeferWinPosHelper* dh = nullptr);
HWND GetEditAnnotationsFocusHwnd(WindowTab*);
void SetEditAnnotationsWindowVisible(WindowTab* tab, bool visible);
bool IsEditAnnotationsWindowVisible(WindowTab* tab);
HWND GetEditAnnotationsWindowHwnd(WindowTab* tab);