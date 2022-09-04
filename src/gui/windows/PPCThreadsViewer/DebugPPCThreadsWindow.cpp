#include "gui/wxgui.h"
#include "Cafe/OS/libs/coreinit/coreinit_Thread.h"
#include "Cafe/OS/libs/coreinit/coreinit_Scheduler.h"
#include "DebugPPCThreadsWindow.h"
#include "Cafe/OS/RPL/rpl.h"
#include "Cafe/OS/RPL/rpl_symbol_storage.h"

#include <cinttypes>

enum
{
	// options
	REFRESH_ID = wxID_HIGHEST + 1,
	AUTO_REFRESH_ID,
	CLOSE_ID,
	GPLIST_ID,

	// list context menu options
	THREADLIST_MENU_BOOST_PRIO_1,
	THREADLIST_MENU_BOOST_PRIO_5,
	THREADLIST_MENU_DECREASE_PRIO_1,
	THREADLIST_MENU_DECREASE_PRIO_5,
	THREADLIST_MENU_SUSPEND,
	THREADLIST_MENU_RESUME,
	THREADLIST_MENU_DUMP_STACK_TRACE,
};

wxBEGIN_EVENT_TABLE(DebugPPCThreadsWindow, wxFrame)
		EVT_BUTTON(CLOSE_ID,DebugPPCThreadsWindow::OnCloseButton)
		EVT_BUTTON(REFRESH_ID,DebugPPCThreadsWindow::OnRefreshButton)

		EVT_CLOSE(DebugPPCThreadsWindow::OnClose)
wxEND_EVENT_TABLE()

DebugPPCThreadsWindow::DebugPPCThreadsWindow(wxFrame& parent)
	: wxFrame(&parent, wxID_ANY, _("PPC threads"), wxDefaultPosition, wxSize(930, 280), wxCLOSE_BOX | wxCLIP_CHILDREN | wxCAPTION | wxRESIZE_BORDER)
{
	wxFrame::SetBackgroundColour(*wxWHITE);

	auto* sizer = new wxBoxSizer(wxVERTICAL);
	m_thread_list = new wxListCtrl(this, GPLIST_ID, wxPoint(0, 0), wxSize(930, 240), wxLC_REPORT);

	m_thread_list->SetFont(wxFont(8, wxFONTFAMILY_MODERN, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "Courier New")); //wxSystemSettings::GetFont(wxSYS_OEM_FIXED_FONT));

	// add columns
	wxListItem col0;
	col0.SetId(0);
	col0.SetText("Address");
	col0.SetWidth(75);
	m_thread_list->InsertColumn(0, col0);
	wxListItem col1;
	col1.SetId(1);
	col1.SetText("Entry");
	col1.SetWidth(75);
	m_thread_list->InsertColumn(1, col1);
	wxListItem col2;
	col2.SetId(2);
	col2.SetText("Stack");
	col2.SetWidth(145);
	m_thread_list->InsertColumn(2, col2);
	wxListItem col3;
	col3.SetId(3);
	col3.SetText("PC");
	col3.SetWidth(120);
	m_thread_list->InsertColumn(3, col3);
	wxListItem colLR;
	colLR.SetId(4);
	colLR.SetText("LR");
	colLR.SetWidth(75);
	m_thread_list->InsertColumn(4, colLR);
	wxListItem col4;
	col4.SetId(5);
	col4.SetText("State");
	col4.SetWidth(90);
	m_thread_list->InsertColumn(5, col4);
	wxListItem col5;
	col5.SetId(6);
	col5.SetText("Affinity");
	col5.SetWidth(70);
	m_thread_list->InsertColumn(6, col5);
	wxListItem colPriority;
	colPriority.SetId(7);
	colPriority.SetText("Priority");
	colPriority.SetWidth(80);
	m_thread_list->InsertColumn(7, colPriority);
	wxListItem col6;
	col6.SetId(8);
	col6.SetText("SliceStart");
	col6.SetWidth(110);
	m_thread_list->InsertColumn(8, col6);
	wxListItem col7;
	col7.SetId(9);
	col7.SetText("SumWakeTime");
	col7.SetWidth(110);
	m_thread_list->InsertColumn(9, col7);
	wxListItem col8;
	col8.SetId(10);
	col8.SetText("ThreadName");
	col8.SetWidth(180);
	m_thread_list->InsertColumn(10, col8);
	wxListItem col9;
	col9.SetId(11);
	col9.SetText("GPR");
	col9.SetWidth(180);
	m_thread_list->InsertColumn(11, col9);
	wxListItem col10;
	col10.SetId(12);
	col10.SetText("Extra info");
	col10.SetWidth(180);
	m_thread_list->InsertColumn(12, col10);

	sizer->Add(m_thread_list, 1, wxEXPAND | wxALL, 5);

	auto* row = new wxBoxSizer(wxHORIZONTAL);
	wxButton* button = new wxButton(this, REFRESH_ID, _("Refresh"), wxPoint(0, 0), wxSize(80, 26));
	row->Add(button, 0, wxALL, 5);

	m_auto_refresh = new wxCheckBox(this, AUTO_REFRESH_ID, _("Auto refresh"));
	m_auto_refresh->SetValue(true);
	row->Add(m_auto_refresh, 0, wxEXPAND | wxALL, 5);

	sizer->Add(row, 0, wxEXPAND | wxALL, 5);

	m_thread_list->Connect(wxEVT_RIGHT_DOWN, wxMouseEventHandler(DebugPPCThreadsWindow::OnThreadListRightClick), nullptr, this);

	SetSizer(sizer);

	RefreshThreadList();

	m_timer = new wxTimer(this);
	this->Bind(wxEVT_TIMER, &DebugPPCThreadsWindow::OnTimer, this);
	m_timer->Start(250);
}

DebugPPCThreadsWindow::~DebugPPCThreadsWindow()
{
	m_timer->Stop();
}

void DebugPPCThreadsWindow::OnCloseButton(wxCommandEvent& event)
{
	Close();
}

void DebugPPCThreadsWindow::OnRefreshButton(wxCommandEvent& event)
{
	RefreshThreadList();
}

void DebugPPCThreadsWindow::OnClose(wxCloseEvent& event)
{
	Close();
}

void DebugPPCThreadsWindow::OnTimer(wxTimerEvent& event)
{
	if (m_auto_refresh->IsChecked())
		RefreshThreadList();
}


#define _r(__idx) _swapEndianU32(cafeThread->context.gpr[__idx])

void DebugPPCThreadsWindow::RefreshThreadList()
{
	wxWindowUpdateLocker lock(m_thread_list);

	long selected_thread = 0;
	const int selection = m_thread_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (selection != wxNOT_FOUND)
		selected_thread = m_thread_list->GetItemData(selection);

	const int scrollPos = m_thread_list->GetScrollPos(0);
	m_thread_list->DeleteAllItems();

	__OSLockScheduler();
	srwlock_activeThreadList.LockWrite();
	for (sint32 i = 0; i < activeThreadCount; i++)
	{
		MPTR threadItrMPTR = activeThread[i];
		OSThread_t* cafeThread = (OSThread_t*)memory_getPointerFromVirtualOffset(threadItrMPTR);

		char tempStr[512];
		sprintf(tempStr, "%08X", threadItrMPTR);


		wxListItem item;
		item.SetId(i);
		item.SetText(tempStr);
		m_thread_list->InsertItem(item);
		m_thread_list->SetItemData(item, (long)threadItrMPTR);
		// entry point
		sprintf(tempStr, "%08X", _swapEndianU32(cafeThread->entrypoint));
		m_thread_list->SetItem(i, 1, tempStr);
		// stack base (low)
		sprintf(tempStr, "%08X - %08X", _swapEndianU32(cafeThread->stackEnd), _swapEndianU32(cafeThread->stackBase));
		m_thread_list->SetItem(i, 2, tempStr);
		// pc
		RPLStoredSymbol* symbol = rplSymbolStorage_getByAddress(cafeThread->context.srr0);
		if (symbol)
			sprintf(tempStr, "%s (0x%08x)", (const char*)symbol->symbolName, cafeThread->context.srr0);
		else
			sprintf(tempStr, "%08X", cafeThread->context.srr0);
		m_thread_list->SetItem(i, 3, tempStr);
		// lr
		sprintf(tempStr, "%08X", _swapEndianU32(cafeThread->context.lr));
		m_thread_list->SetItem(i, 4, tempStr);
		// state
		OSThread_t::THREAD_STATE threadState = cafeThread->state;
		wxString threadStateStr = "UNDEFINED";
		if (cafeThread->suspendCounter != 0)
			threadStateStr = "SUSPENDED";
		else if (threadState == OSThread_t::THREAD_STATE::STATE_NONE)
			threadStateStr = "NONE";
		else if (threadState == OSThread_t::THREAD_STATE::STATE_READY)
			threadStateStr = "READY";
		else if (threadState == OSThread_t::THREAD_STATE::STATE_RUNNING)
			threadStateStr = "RUNNING";
		else if (threadState == OSThread_t::THREAD_STATE::STATE_WAITING)
			threadStateStr = "WAITING";
		else if (threadState == OSThread_t::THREAD_STATE::STATE_MORIBUND)
			threadStateStr = "MORIBUND";
		m_thread_list->SetItem(i, 5, threadStateStr);
		// affinity
		uint8 affinity = cafeThread->attr&7;
		uint8 affinityReal = cafeThread->context.affinity;
		if(affinity != affinityReal)
			sprintf(tempStr, "(!) %d%d%d real: %d%d%d", (affinity >> 0) & 1, (affinity >> 1) & 1, (affinity >> 2) & 1, (affinityReal >> 0) & 1, (affinityReal >> 1) & 1, (affinityReal >> 2) & 1);
		else
			sprintf(tempStr, "%d%d%d", (affinity >> 0) & 1, (affinity >> 1) & 1, (affinity >> 2) & 1);
		m_thread_list->SetItem(i, 6, tempStr);
		// priority
		sint32 effectivePriority = cafeThread->effectivePriority;
		sprintf(tempStr, "%d", effectivePriority);
		m_thread_list->SetItem(i, 7, tempStr);
		// last awake in cycles
		uint64 lastWakeUpTime = cafeThread->wakeUpTime;
		sprintf(tempStr, "%" PRIu64, lastWakeUpTime);
		m_thread_list->SetItem(i, 8, tempStr);
		// awake time in cycles
		uint64 awakeTime = cafeThread->totalCycles;
		sprintf(tempStr, "%" PRIu64, awakeTime);
		m_thread_list->SetItem(i, 9, tempStr);
		// thread name
		const char* threadName = "NULL";
		if (!cafeThread->threadName.IsNull())
			threadName = cafeThread->threadName.GetPtr();
		m_thread_list->SetItem(i, 10, threadName);
		// GPR
		sprintf(tempStr, "r3 %08x r4 %08x r5 %08x r6 %08x r7 %08x", _r(3), _r(4), _r(5), _r(6), _r(7));
		m_thread_list->SetItem(i, 11, tempStr);
		// waiting condition / extra info
		coreinit::OSMutex* mutex = cafeThread->waitingForMutex;
		if (mutex)
			sprintf(tempStr, "Mutex 0x%08x (Held by thread 0x%08X Lock-Count: %d)", memory_getVirtualOffsetFromPointer(mutex), mutex->owner.GetMPTR(), (uint32)mutex->lockCount);
		else
			sprintf(tempStr, "");

		// OSSetThreadCancelState
		if (cafeThread->requestFlags & OSThread_t::REQUEST_FLAG_CANCEL)
			strcat(tempStr, "[Cancel requested]");

		m_thread_list->SetItem(i, 12, tempStr);

		if(selected_thread != 0 && selected_thread == (long)threadItrMPTR)
			m_thread_list->SetItemState(i, wxLIST_STATE_FOCUSED | wxLIST_STATE_SELECTED, wxLIST_STATE_FOCUSED | wxLIST_STATE_SELECTED);
	}
	srwlock_activeThreadList.UnlockWrite();
	__OSUnlockScheduler();

	m_thread_list->SetScrollPos(0, scrollPos, true);
}

void DebugLogStackTrace(OSThread_t* thread, MPTR sp);

void DebugPPCThreadsWindow::DumpStackTrace(OSThread_t* thread)
{
	cemuLog_log(LogType::Force, fmt::format("Dumping stack trace for thread {0:08x} LR: {1:08x}", memory_getVirtualOffsetFromPointer(thread), _swapEndianU32(thread->context.lr)));
	DebugLogStackTrace(thread, _swapEndianU32(thread->context.gpr[1]));
}

void DebugPPCThreadsWindow::OnThreadListPopupClick(wxCommandEvent& evt)
{
	MPTR threadMPTR = (MPTR)(size_t)static_cast<wxMenu *>(evt.GetEventObject())->GetClientData();
	// check if thread is still active
	bool threadIsActive = false;
	srwlock_activeThreadList.LockWrite();
	for (sint32 i = 0; i < activeThreadCount; i++)
	{
		MPTR threadItrMPTR = activeThread[i];
		if (threadItrMPTR == threadMPTR)
		{
			threadIsActive = true;
			break;
		}
	}
	srwlock_activeThreadList.UnlockWrite();
	if (threadIsActive == false)
		return;
	// handle command
	OSThread_t* osThread = (OSThread_t*)memory_getPointerFromVirtualOffset(threadMPTR);
	switch (evt.GetId())
	{
	case THREADLIST_MENU_BOOST_PRIO_5:
		osThread->basePriority = osThread->basePriority - 5;
		break;
	case THREADLIST_MENU_BOOST_PRIO_1:
		osThread->basePriority = osThread->basePriority - 1;
		break;
	case THREADLIST_MENU_DECREASE_PRIO_5:
		osThread->basePriority = osThread->basePriority + 5;
		break;
	case THREADLIST_MENU_DECREASE_PRIO_1:
		osThread->basePriority = osThread->basePriority + 1;
		break;
	case THREADLIST_MENU_SUSPEND:
		coreinit::OSSuspendThread(osThread);
		break;
	case THREADLIST_MENU_RESUME:
		coreinit::OSResumeThread(osThread);
		break;
	case THREADLIST_MENU_DUMP_STACK_TRACE:
		DumpStackTrace(osThread);
		break;
	}
	coreinit::__OSUpdateThreadEffectivePriority(osThread);
	// update thread list
	RefreshThreadList();
}

void DebugPPCThreadsWindow::OnThreadListRightClick(wxMouseEvent& event)
{
	// Get the item index
	int hitTestFlag;
	int itemIndex = m_thread_list->HitTest(event.GetPosition(), hitTestFlag);
	if (itemIndex == wxNOT_FOUND)
		return;
	// select item
	m_thread_list->SetItemState(itemIndex, wxLIST_STATE_FOCUSED, wxLIST_STATE_FOCUSED);
	long sel = m_thread_list->GetNextItem(-1, wxLIST_NEXT_ALL,
	                                   wxLIST_STATE_SELECTED);
	if (sel != -1)
		m_thread_list->SetItemState(sel, 0, wxLIST_STATE_SELECTED);
	m_thread_list->SetItemState(itemIndex, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
	// check if thread is still on the list of active threads
	MPTR threadMPTR = (MPTR)m_thread_list->GetItemData(itemIndex);
	bool threadIsActive = false;
	srwlock_activeThreadList.LockWrite();
	for (sint32 i = 0; i < activeThreadCount; i++)
	{
		MPTR threadItrMPTR = activeThread[i];
		if (threadItrMPTR == threadMPTR)
		{
			threadIsActive = true;
			break;
		}
	}
	srwlock_activeThreadList.UnlockWrite();
	if (threadIsActive == false)
		return;
	// create menu entry
	wxMenu menu;
	menu.SetClientData((void*)(size_t)threadMPTR);
	menu.Append(THREADLIST_MENU_BOOST_PRIO_5, _("Boost priority (-5)"));
	menu.Append(THREADLIST_MENU_BOOST_PRIO_1, _("Boost priority (-1)"));
	menu.AppendSeparator();
	menu.Append(THREADLIST_MENU_DECREASE_PRIO_5, _("Decrease priority (+5)"));
	menu.Append(THREADLIST_MENU_DECREASE_PRIO_1, _("Decrease priority (+1)"));
	menu.AppendSeparator();
	menu.Append(THREADLIST_MENU_RESUME, _("Resume"));
	menu.Append(THREADLIST_MENU_SUSPEND, _("Suspend"));
	menu.AppendSeparator();
	menu.Append(THREADLIST_MENU_DUMP_STACK_TRACE, _("Write stack trace to log"));
	menu.Connect(wxEVT_COMMAND_MENU_SELECTED, wxCommandEventHandler(DebugPPCThreadsWindow::OnThreadListPopupClick), nullptr, this);
	PopupMenu(&menu);
}

void DebugPPCThreadsWindow::Close()
{
	this->Destroy();
}
