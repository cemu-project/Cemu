#pragma once
#include "gui/canvas/IRenderCanvas.h"
#include <wx/window.h>

wxWindow* GLCanvas_Create(wxWindow* parent, const wxSize& size, bool is_main_window);
bool GLCanvas_MakeCurrent(bool padView);
void GLCanvas_SwapBuffers(bool swapTV, bool swapDRC);
bool GLCanvas_HasPadViewOpen();