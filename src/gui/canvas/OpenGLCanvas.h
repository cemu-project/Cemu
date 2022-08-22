#pragma once
#include <wx/window.h>
#include "gui/canvas/IRenderCanvas.h"

wxWindow* GLCanvas_Create(wxWindow* parent, const wxSize& size, bool is_main_window);
bool GLCanvas_MakeCurrent(bool padView);
void GLCanvas_SwapBuffers(bool swapTV, bool swapDRC);
bool GLCanvas_HasPadViewOpen();