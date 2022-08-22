
#ifdef USE_CEMUHOOK

#include <wx/app.h>
#include <wx/event.h>
#include <wx/process.h>
#include <wx/timer.h>
#include <wx/window.h>
#include <wx/dialog.h>
#include <wx/dcclient.h>
#include <wx/frame.h>
#include <wx/menu.h>
#include <wx/sizer.h>
#include <wx/checklst.h>
#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/stattext.h>
#include <wx/listctrl.h>
#include <wx/dataview.h>
#include <wx/filedlg.h>
#include <wx/mstream.h>
#include <wx/hyperlink.h>
#include <wx/clipbrd.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
#include <wx/collpane.h>
#include <wx/collheaderctrl.h>
#include <wx/power.h>
#include <wx/gauge.h>

#define CHECK_FOR_WX_EVT_STRING(strVar, strConst) if (strcmp(strVar, #strConst) == 0){ return static_cast<int>(strConst); }


__declspec(dllexport) wxEvtHandler* wxEvtHandler_Initialize(uint8_t* allocMemory)
{
	wxEvtHandler* handler = new (allocMemory) wxEvtHandler();
	return handler;
}

__declspec(dllexport) void wxEvtHandler_Connect(wxEvtHandler* eventSource, int id, int lastId, int eventType, wxObjectEventFunction func, wxObject* userData, wxEvtHandler* eventSink)
{
	eventSource->Connect(id, lastId, eventType, func, userData, eventSink);
}

__declspec(dllexport) void wxEvtHandler_Disconnect(wxEvtHandler* eventSource, int id, int lastId, int eventType, wxObjectEventFunction func, wxObject* userData, wxEvtHandler* eventSink)
{
	eventSource->Disconnect(id, lastId, eventType, func, userData, eventSink);
}

__declspec(dllexport) const wchar_t* GetTranslationWChar(const wchar_t* text)
{
	return wxGetTranslation(text).wc_str();
}

__declspec(dllexport) int wxGetEventByName(const char* eventName)
{
#define PROCESS_OWN_WXEVT(EventVarName,EventHookId) if (!strcmp(eventName,#EventVarName)){ return static_cast<int>(EventVarName); }
#include "wxEvtHook.inl"
#undef PROCESS_OWN_WXEVT

	return -1;
}

#pragma optimize( "", off )

static bool FixupWxEvtId(const wxEventType& outObj, const int newId)
{
	const int oldVal = static_cast<int>(outObj);
	if (oldVal == newId)
		return false;

	wxEventType* dstObj = const_cast<wxEventType*>(&outObj);
	memcpy(dstObj, &newId, sizeof(newId));

	// check value again
	if (static_cast<int>(outObj) != newId)
		assert_dbg();

	return true;
}

void FixupWxEvtIdsToMatchCemuHook()
{
	// instantiate all the events
#define PROCESS_OWN_WXEVT(EventVarName,EventHookId) static_cast<int>(EventVarName);
#include "cemuhook/wxEvtHook.inl"
#undef PROCESS_OWN_WXEVT
	// fix them
#define PROCESS_OWN_WXEVT(EventVarName,EventHookId) FixupWxEvtId(EventVarName,EventHookId)
#include "cemuhook/wxEvtHook.inl"
#undef PROCESS_OWN_WXEVT
}

#pragma optimize( "", on )

// these I added on my own since they might be useful

__declspec(dllexport) void coreinitAPI_OSYieldThread()
{
	PPCCore_switchToScheduler();
}

#define xstr(a) str(a)
#define str(a) #a

#define PRINT_EVENT(__evtName) printf(xstr(__evtName) " = %d\n", (int)(wxEventType)__evtName);

void PrintEvents()
{
	PRINT_EVENT(wxEVT_IDLE)
	PRINT_EVENT(wxEVT_THREAD)
	PRINT_EVENT(wxEVT_ASYNC_METHOD_CALL)

	PRINT_EVENT(wxEVT_BUTTON)
	PRINT_EVENT(wxEVT_CHECKBOX)
	PRINT_EVENT(wxEVT_CHOICE)
	PRINT_EVENT(wxEVT_LISTBOX)
	PRINT_EVENT(wxEVT_LISTBOX_DCLICK)
	PRINT_EVENT(wxEVT_CHECKLISTBOX)
	PRINT_EVENT(wxEVT_MENU)
	PRINT_EVENT(wxEVT_SLIDER)
	PRINT_EVENT(wxEVT_RADIOBOX)
	PRINT_EVENT(wxEVT_RADIOBUTTON)
	PRINT_EVENT(wxEVT_SCROLLBAR)

	PRINT_EVENT(wxEVT_LEFT_DOWN)
	PRINT_EVENT(wxEVT_LEFT_UP)
	PRINT_EVENT(wxEVT_MIDDLE_DOWN)
	PRINT_EVENT(wxEVT_MIDDLE_UP)
	PRINT_EVENT(wxEVT_RIGHT_DOWN)
	PRINT_EVENT(wxEVT_RIGHT_UP)
	PRINT_EVENT(wxEVT_MOTION)
	PRINT_EVENT(wxEVT_ENTER_WINDOW)
	PRINT_EVENT(wxEVT_LEAVE_WINDOW)

	PRINT_EVENT(wxEVT_CHAR)
	PRINT_EVENT(wxEVT_SET_CURSOR)
	PRINT_EVENT(wxEVT_SCROLL_TOP)
	PRINT_EVENT(wxEVT_SCROLL_BOTTOM)

	PRINT_EVENT(wxEVT_SIZE)
	PRINT_EVENT(wxEVT_MOVE)
	PRINT_EVENT(wxEVT_CLOSE_WINDOW)
	PRINT_EVENT(wxEVT_END_SESSION)
	PRINT_EVENT(wxEVT_ACTIVATE_APP)
	PRINT_EVENT(wxEVT_ACTIVATE)
	PRINT_EVENT(wxEVT_CREATE)
	PRINT_EVENT(wxEVT_DESTROY)
	PRINT_EVENT(wxEVT_SHOW)
	PRINT_EVENT(wxEVT_ICONIZE)
	PRINT_EVENT(wxEVT_MAXIMIZE)

	PRINT_EVENT(wxEVT_PAINT)
	PRINT_EVENT(wxEVT_MENU_OPEN)
	PRINT_EVENT(wxEVT_MENU_CLOSE)
	PRINT_EVENT(wxEVT_MENU_HIGHLIGHT)
	PRINT_EVENT(wxEVT_CONTEXT_MENU)

	PRINT_EVENT(wxEVT_UPDATE_UI)
	PRINT_EVENT(wxEVT_SIZING)
	PRINT_EVENT(wxEVT_MOVING)

	PRINT_EVENT(wxEVT_TEXT_COPY)
	PRINT_EVENT(wxEVT_TEXT_CUT)
	PRINT_EVENT(wxEVT_TEXT_PASTE)

}

void wxMatchCemuhookEventIds()
{

	FixupWxEvtIdsToMatchCemuHook();

	//PrintEvents();
	// check if key eventIds match with Cemuhook
	cemu_assert((wxEventType)wxEVT_SIZE == 10078);
	cemu_assert((wxEventType)wxEVT_HYPERLINK == 10156);
	cemu_assert((wxEventType)wxEVT_IDLE == 10001);
	cemu_assert((wxEventType)wxEVT_UPDATE_UI == 10116);
}

/* This code reserves the first 300 wxWidgets event ids early, so that they cant be grabbed by wxWidgets constructors for the regular events. We then assign fixed IDs that match Cemuhook's later */
#pragma init_seg(lib)

int wxNewEventType();

bool wxReserveEventIds()
{
	for (int i = 0; i < 300; i++)
		wxNewEventType();
	return true;
}

bool s_placeholderResult = wxReserveEventIds();

#endif