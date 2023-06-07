#pragma once
#include <wx/window.h>
#include "canvas/IRenderCanvas.h"

wxWindow* GLCanvas_Create(wxWindow* parent, const wxSize& size, bool is_main_window);
