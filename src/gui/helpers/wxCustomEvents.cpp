#include "gui/helpers/wxCustomEvents.h"

wxDEFINE_EVENT(wxEVT_SET_STATUS_BAR_TEXT, wxSetStatusBarTextEvent);
wxDEFINE_EVENT(wxEVT_SET_GAUGE_VALUE, wxSetGaugeValue);
wxDEFINE_EVENT(wxEVT_REMOVE_ITEM, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_SET_TEXT, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_ENABLE, wxCommandEvent);