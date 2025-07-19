#pragma once

#include <wx/wxprec.h>

// base class for all render interfaces
class IRenderCanvas
{
public:
	IRenderCanvas(bool is_main_window)
		: m_is_main_window(is_main_window) {}

protected:
	bool m_is_main_window;
};