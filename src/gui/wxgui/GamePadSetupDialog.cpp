#include "wxgui/GamePadSetupDialog.h"

#include <wx/button.h>
#include <wx/filename.h>
#include <wx/gauge.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/process.h>
#include <wx/simplebook.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/stream.h>
#include <wx/utils.h>
#include <wx/busyinfo.h>

namespace
{
	// The helper: $CEMU_GAMEPAD_SETUP, else ~/.local/bin/cemu-gamepad-setup (the bridge's install.sh links it there),
	// else cemu-gamepad-setup on PATH. pkexec needs an absolute path, so PATH hits are resolved.
	wxString FindHelper()
	{
		wxString env;
		if (wxGetEnv("CEMU_GAMEPAD_SETUP", &env) && wxFileName::IsFileExecutable(env))
			return env;
		const wxString local = wxFileName::GetHomeDir() + "/.local/bin/cemu-gamepad-setup";
		if (wxFileName::IsFileExecutable(local))
			return local;
		wxString path;
		if (wxGetEnv("PATH", &path))
		{
			for (const auto& dir : wxSplit(path, ':'))
			{
				const wxString candidate = dir + "/cemu-gamepad-setup";
				if (!dir.empty() && wxFileName::IsFileExecutable(candidate))
					return candidate;
			}
		}
		return {};
	}

	wxArrayString RunHelper(const wxString& helper, const wxString& args)
	{
		wxArrayString out, err;
		if (!helper.empty())
			wxExecute(wxString::Format("\"%s\" %s", helper, args), out, err, wxEXEC_SYNC | wxEXEC_NODISABLE);
		return out;
	}

	wxString SymbolGlyph(const wxString& name)
	{
		if (name == "SPADE")
			return wxString::FromUTF8("\xE2\x99\xA0");
		if (name == "HEART")
			return wxString::FromUTF8("\xE2\x99\xA5");
		if (name == "DIAMOND")
			return wxString::FromUTF8("\xE2\x99\xA6");
		if (name == "CLUB")
			return wxString::FromUTF8("\xE2\x99\xA3");
		return "?";
	}

	// list columns; the interface name and the "internet" flag ride along as hidden data
	enum Column
	{
		kColAdapter,
		kColStatus,
	};
	struct AdapterRow
	{
		wxString iface;
		bool usable;
		bool internet;
	};
}

GamePadSetupStatus GamePadSetupDialog::QueryStatus()
{
	GamePadSetupStatus st;
	for (const auto& line : RunHelper(FindHelper(), "status"))
	{
		if (line.StartsWith("SETUP done"))
		{
			st.setUp = true;
			st.adapterName = line.AfterFirst('\t');
		}
		else if (line.StartsWith("PAD "))
			st.padState = line.Mid(4);
	}
	return st;
}

bool GamePadSetupDialog::RunReset(wxWindow* parent)
{
	const wxString helper = FindHelper();
	if (helper.empty())
		return false;
	wxBusyCursor busy;
	wxArrayString out, err;
	// Synchronous is fine here: reset doesn't wait on anything (it stops the network service and deletes files).
	const long rv = wxExecute(wxString::Format("pkexec \"%s\" reset", helper), out, err, wxEXEC_SYNC);
	if (rv == 126 || rv == 127)
		return false; // password dialog dismissed
	if (rv != 0)
	{
		wxMessageBox(_("The GamePad setup couldn't be reset."), _("Reset GamePad"), wxOK | wxICON_ERROR, parent);
		return false;
	}
	return true;
}

GamePadSetupDialog::GamePadSetupDialog(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, _("Set up GamePad"), wxDefaultPosition, wxSize(560, 420), wxDEFAULT_DIALOG_STYLE),
	  m_poll(this)
{
	m_helper = FindHelper();
	auto* sizer = new wxBoxSizer(wxVERTICAL);
	m_book = new wxSimplebook(this);

	wxFont titleFont = GetFont().Bold().Scaled(1.3f);

	// 1. choose the adapter
	{
		auto* page = new wxPanel(m_book);
		auto* s = new wxBoxSizer(wxVERTICAL);
		auto* title = new wxStaticText(page, wxID_ANY, _("Choose a Wi-Fi adapter"));
		title->SetFont(titleFont);
		s->Add(title, 0, wxBOTTOM, 6);
		s->Add(new wxStaticText(page, wxID_ANY, _("The GamePad connects to your PC through it.")), 0, wxBOTTOM, 10);
		m_list = new wxListCtrl(page, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
		m_list->InsertColumn(kColAdapter, _("Adapter"), wxLIST_FORMAT_LEFT, 340);
		m_list->InsertColumn(kColStatus, wxEmptyString, wxLIST_FORMAT_LEFT, 150);
		m_list->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent&) { ShowPage(kPageChoose); });
		m_list->Bind(wxEVT_LIST_ITEM_DESELECTED, [this](wxListEvent&) { ShowPage(kPageChoose); });
		s->Add(m_list, 1, wxEXPAND);
		m_chooseNote = new wxStaticText(page, wxID_ANY, wxEmptyString);
		s->Add(m_chooseNote, 0, wxTOP, 8);
		page->SetSizer(s);
		m_book->AddPage(page, wxEmptyString);
	}
	// 2. checking / syncing / finishing
	{
		auto* page = new wxPanel(m_book);
		auto* s = new wxBoxSizer(wxVERTICAL);
		m_workTitle = new wxStaticText(page, wxID_ANY, wxEmptyString);
		m_workTitle->SetFont(titleFont);
		s->Add(m_workTitle, 0, wxBOTTOM, 10);
		m_workText = new wxStaticText(page, wxID_ANY, wxEmptyString);
		s->Add(m_workText, 0, wxBOTTOM, 10);
		m_symbols = new wxStaticText(page, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxALIGN_CENTRE_HORIZONTAL);
		m_symbols->SetFont(GetFont().Scaled(4.0f));
		s->Add(m_symbols, 0, wxEXPAND | wxTOP | wxBOTTOM, 10);
		s->AddStretchSpacer();
		m_gauge = new wxGauge(page, wxID_ANY, 100);
		s->Add(m_gauge, 0, wxEXPAND);
		page->SetSizer(s);
		m_book->AddPage(page, wxEmptyString);
	}
	// 3. done (or failed)
	{
		auto* page = new wxPanel(m_book);
		auto* s = new wxBoxSizer(wxVERTICAL);
		m_doneTitle = new wxStaticText(page, wxID_ANY, wxEmptyString);
		m_doneTitle->SetFont(titleFont);
		s->Add(m_doneTitle, 0, wxBOTTOM, 10);
		m_doneText = new wxStaticText(page, wxID_ANY, wxEmptyString);
		s->Add(m_doneText, 0);
		page->SetSizer(s);
		m_book->AddPage(page, wxEmptyString);
	}
	sizer->Add(m_book, 1, wxEXPAND | wxALL, 16);

	auto* buttons = new wxBoxSizer(wxHORIZONTAL);
	m_back = new wxButton(this, wxID_BACKWARD, _("Back"));
	m_back->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
		FillAdapters();
		ShowPage(kPageChoose);
	});
	buttons->Add(m_back, 0);
	buttons->AddStretchSpacer();
	m_cancel = new wxButton(this, wxID_CANCEL, _("Cancel"));
	m_cancel->Bind(wxEVT_BUTTON, &GamePadSetupDialog::OnCancel, this);
	buttons->Add(m_cancel, 0, wxRIGHT, 6);
	m_next = new wxButton(this, wxID_FORWARD, _("Next"));
	m_next->Bind(wxEVT_BUTTON, &GamePadSetupDialog::OnNext, this);
	buttons->Add(m_next, 0);
	sizer->Add(buttons, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 16);
	SetSizer(sizer);

	Bind(wxEVT_TIMER, &GamePadSetupDialog::OnPoll, this);
	Bind(wxEVT_END_PROCESS, &GamePadSetupDialog::OnProcessEnded, this);
	Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& e) {
		if (m_proc && e.CanVeto())
		{
			wxCommandEvent dummy;
			OnCancel(dummy); // closes after the helper has put the adapter back
			e.Veto();
			return;
		}
		e.Skip();
	});

	FillAdapters();
	ShowPage(kPageChoose);
}

GamePadSetupDialog::~GamePadSetupDialog()
{
	m_poll.Stop();
	if (m_proc)
	{
		m_proc->CloseOutput(); // the helper cancels and cleans up on its own
		m_proc->Detach();	   // deletes itself when the helper exits
	}
	for (long i = 0; i < m_list->GetItemCount(); i++)
		delete reinterpret_cast<AdapterRow*>(m_list->GetItemData(i));
}

void GamePadSetupDialog::FillAdapters()
{
	for (long i = 0; i < m_list->GetItemCount(); i++)
		delete reinterpret_cast<AdapterRow*>(m_list->GetItemData(i));
	m_list->DeleteAllItems();
	long row = 0, usable = 0;
	for (const auto& line : RunHelper(m_helper, "list"))
	{
		// ADAPTER <iface> <name> <verdict> <reason> <internet>
		const auto f = wxSplit(line, '\t', 0);
		if (f.size() < 6 || f[0] != "ADAPTER")
			continue;
		auto* data = new AdapterRow{f[1], f[3] != "no", f[5] == "1"};
		const long i = m_list->InsertItem(row++, f[2]);
		m_list->SetItem(i, kColStatus, f[4]);
		m_list->SetItemPtrData(i, wxUIntPtr(data));
		if (!data->usable)
			m_list->SetItemTextColour(i, wxColour(150, 150, 150));
		else if (usable++ == 0)
			m_list->SetItemState(i, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
	}
	if (m_helper.empty())
		m_chooseNote->SetLabel(_("The GamePad bridge isn't installed."));
	else if (row == 0)
		m_chooseNote->SetLabel(_("No Wi-Fi adapter found. Plug one in, then reopen this window."));
	else if (usable == 0)
		m_chooseNote->SetLabel(_("None of these adapters work with the GamePad. A compatible USB Wi-Fi adapter is needed."));
	else
		m_chooseNote->SetLabel(wxEmptyString);
}

void GamePadSetupDialog::ShowPage(Page page)
{
	m_page = page;
	m_book->SetSelection(page);
	const long sel = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	const auto* row = sel >= 0 ? reinterpret_cast<AdapterRow*>(m_list->GetItemData(sel)) : nullptr;
	m_back->Show(page == kPageDone && !m_succeeded);
	m_cancel->Show(page != kPageDone);
	m_next->Show(page != kPageWorking);
	m_next->SetLabel(page == kPageDone ? (m_succeeded ? _("Done") : _("Close")) : _("Next"));
	m_next->Enable(page != kPageChoose || (row && row->usable));
	Layout();
}

void GamePadSetupDialog::OnNext(wxCommandEvent&)
{
	if (m_page == kPageDone)
	{
		EndModal(m_succeeded ? wxID_OK : wxID_CANCEL);
		return;
	}
	const long sel = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (sel < 0 || m_proc)
		return;
	const auto* row = reinterpret_cast<AdapterRow*>(m_list->GetItemData(sel));
	if (row->internet &&
		wxMessageBox(_("This adapter is your internet connection. The GamePad would take it over, and you'd be offline "
					   "while playing. Use it anyway?"),
					 _("Set up GamePad"), wxYES_NO | wxICON_WARNING, this) != wxYES)
		return;

	m_workTitle->SetLabel(_("Checking the adapter..."));
	m_workText->SetLabel(_("Your system will ask for your password."));
	m_symbols->SetLabel(wxEmptyString);
	m_gauge->Pulse();
	m_partial.clear();
	m_sawResult = false;
	m_cancelling = false;
	ShowPage(kPageWorking);

	m_proc = new wxProcess(this);
	m_proc->Redirect();
	if (wxExecute(wxString::Format("pkexec \"%s\" setup \"%s\"", m_helper, row->iface), wxEXEC_ASYNC, m_proc) <= 0)
	{
		delete m_proc;
		m_proc = nullptr;
		ShowFailure(_("Couldn't ask for administrator rights."));
		return;
	}
	m_poll.Start(100);
}

void GamePadSetupDialog::OnCancel(wxCommandEvent&)
{
	if (!m_proc)
	{
		EndModal(wxID_CANCEL);
		return;
	}
	// The helper runs as root, so it can't be killed from here: closing its input tells it to stop. It puts the
	// adapter back and exits; OnProcessEnded then closes the window.
	m_workTitle->SetLabel(_("Cancelling..."));
	m_workText->SetLabel(wxEmptyString);
	m_symbols->SetLabel(wxEmptyString);
	m_cancelling = true;
	m_cancel->Disable();
	m_proc->CloseOutput();
}

void GamePadSetupDialog::DrainOutput()
{
	if (!m_proc)
		return;
	for (wxInputStream* in : {m_proc->GetInputStream(), m_proc->GetErrorStream()})
	{
		while (in && in->CanRead())
		{
			char buf[512];
			in->Read(buf, sizeof(buf));
			const size_t n = in->LastRead();
			if (n == 0)
				break;
			m_partial += wxString::FromUTF8(buf, n);
		}
	}
	int nl;
	while ((nl = m_partial.Find('\n')) != wxNOT_FOUND)
	{
		HandleLine(m_partial.Left(nl));
		m_partial = m_partial.Mid(nl + 1);
	}
}

void GamePadSetupDialog::HandleLine(const wxString& line)
{
	wxString rest;
	if (line == "STAGE check")
	{
		m_workTitle->SetLabel(_("Checking the adapter..."));
		m_workText->SetLabel(wxEmptyString);
	}
	else if (line.StartsWith("STAGE sync ", &rest))
	{
		wxString glyphs;
		for (const auto& name : wxSplit(rest, ' '))
			glyphs += SymbolGlyph(name) + "  ";
		m_workTitle->SetLabel(_("Sync your GamePad"));
		m_workText->SetLabel(_("Press the SYNC button on the back of the GamePad, then enter these symbols:"));
		m_symbols->SetLabel(glyphs.Trim());
	}
	else if (line == "STAGE finish")
	{
		m_workTitle->SetLabel(_("Finishing..."));
		m_workText->SetLabel(_("GamePad synced."));
		m_symbols->SetLabel(wxEmptyString);
	}
	else if (line == "RESULT works")
	{
		m_sawResult = true;
		m_succeeded = true;
	}
	else if (line.StartsWith("RESULT fails ", &rest))
	{
		m_sawResult = true;
		if (rest != "cancelled")
			ShowFailure(rest.Left(1).Upper() + rest.Mid(1) + ".");
	}
	m_book->GetPage(kPageWorking)->Layout();
}

void GamePadSetupDialog::ShowFailure(const wxString& reason)
{
	m_succeeded = false;
	m_doneTitle->SetLabel(_("Setup didn't finish"));
	m_doneText->SetLabel(reason);
	m_doneText->Wrap(500);
	ShowPage(kPageDone);
}

void GamePadSetupDialog::OnPoll(wxTimerEvent&)
{
	DrainOutput();
	if (m_proc)
		m_gauge->Pulse();
}

void GamePadSetupDialog::OnProcessEnded(wxProcessEvent& event)
{
	m_poll.Stop();
	DrainOutput();
	if (!m_partial.empty())
		HandleLine(m_partial);
	m_partial.clear();
	delete m_proc;
	m_proc = nullptr;
	m_cancel->Enable();

	if (m_succeeded)
	{
		// The bridge's user service. Not waited for: a blocking wait here once hung Cemu for good (the service start
		// waited on the GamePad network, which wasn't up).
		wxExecute(wxString::Format("\"%s\" install-user", m_helper), wxEXEC_ASYNC);
		m_doneTitle->SetLabel(_("All set!"));
		m_doneText->SetLabel(_("Turn on the GamePad and start a game."));
		ShowPage(kPageDone);
		return;
	}
	if (m_page == kPageDone)
		return; // failure already shown
	if (m_cancelling)
	{
		EndModal(wxID_CANCEL); // cancelled
		return;
	}
	// pkexec: 126 = the password dialog was dismissed, 127 = not authorized
	const int code = event.GetExitCode();
	if (code == 126 || code == 127)
	{
		FillAdapters();
		ShowPage(kPageChoose);
		return;
	}
	ShowFailure(wxString::Format(_("Something went wrong (code %d)."), code));
}
