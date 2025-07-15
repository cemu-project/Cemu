#pragma once

#define wxNO_UNSAFE_WXSTRING_CONV 1

#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif

#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/panel.h>
#include <wx/timer.h>
#include <wx/slider.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/spinctrl.h>
#include <wx/listbase.h>
#include <wx/display.h>
#include <wx/aboutdlg.h>
#include <wx/dialog.h>
#include <wx/timer.h>
#include <wx/gauge.h>
#include <wx/dataview.h>
#include <wx/statline.h>
#include <wx/wrapsizer.h>
#include <wx/gbsizer.h>
#include <wx/radiobox.h>
#include <wx/combobox.h>
#include <wx/sizer.h>
#include <wx/scrolbar.h>
#include <wx/richtext/richtextctrl.h>
#include <wx/dcbuffer.h>
#include <wx/combo.h>
#include <wx/wupdlock.h>
#include <wx/infobar.h>
#include <wx/splitter.h>

extern bool g_inputConfigWindowHasFocus;

inline bool SendSliderEvent(wxSlider* slider, int new_value)
{
	wxCommandEvent cevent(wxEVT_SLIDER, slider->GetId());
	cevent.SetInt(new_value);
	cevent.SetEventObject(slider);
	return slider->HandleWindowEvent(cevent);
}
