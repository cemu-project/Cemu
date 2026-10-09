#include "wxgui/GamePadSetupDialog.h"

#include <wx/button.h>
#include <wx/filename.h>
#include <wx/listctrl.h>
#include <wx/msgdlg.h>
#include <wx/process.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/stream.h>
#include <wx/textctrl.h>
#include <wx/utils.h>

namespace
{
	// The helper: $CEMU_GAMEPAD_SETUP, else ~/.local/bin/cemu-gamepad-setup (scripts/install.sh links it there),
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

	enum Column
	{
		kColAdapter,
		kColInterface,
		kColVerdict,
		kColDetails,
	};
}

GamePadSetupDialog::GamePadSetupDialog(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, _("Set up GamePad"), wxDefaultPosition, wxSize(900, 560), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
	  m_poll(this)
{
	auto* sizer = new wxBoxSizer(wxVERTICAL);

	auto* intro = new wxStaticText(this, wxID_ANY,
		_("The GamePad connects to a Wi-Fi network hosted by this PC, like a Wii U console hosts one. "
		  "Choose the Wi-Fi adapter to host it with, then test it. The adapter must be able to host a 5 GHz network "
		  "and give access to its internal clock, which the GamePad syncs to. A separate USB adapter is best, "
		  "so your internet connection stays untouched."));
	intro->Wrap(860);
	sizer->Add(intro, 0, wxALL, 10);

	m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 160), wxLC_REPORT | wxLC_SINGLE_SEL);
	m_list->InsertColumn(kColAdapter, _("Adapter"), wxLIST_FORMAT_LEFT, 280);
	m_list->InsertColumn(kColInterface, _("Interface"), wxLIST_FORMAT_LEFT, 110);
	m_list->InsertColumn(kColVerdict, _("Verdict"), wxLIST_FORMAT_LEFT, 90);
	m_list->InsertColumn(kColDetails, _("Details"), wxLIST_FORMAT_LEFT, 400);
	m_list->Bind(wxEVT_LIST_ITEM_SELECTED, &GamePadSetupDialog::OnSelectionChanged, this);
	m_list->Bind(wxEVT_LIST_ITEM_DESELECTED, &GamePadSetupDialog::OnSelectionChanged, this);
	sizer->Add(m_list, 0, wxEXPAND | wxLEFT | wxRIGHT, 10);

	auto* buttons = new wxBoxSizer(wxHORIZONTAL);
	m_refresh = new wxButton(this, wxID_ANY, _("Refresh list"));
	m_refresh->Bind(wxEVT_BUTTON, &GamePadSetupDialog::OnRefresh, this);
	buttons->Add(m_refresh, 0, wxRIGHT, 5);
	m_test = new wxButton(this, wxID_ANY, _("Test selected adapter"));
	m_test->SetToolTip(_("Starts a short test network on the adapter and reads its clock. Asks for your password: "
						 "hosting a network needs administrator rights."));
	m_test->Bind(wxEVT_BUTTON, &GamePadSetupDialog::OnTest, this);
	buttons->Add(m_test, 0, wxRIGHT, 5);
	m_result = new wxStaticText(this, wxID_ANY, wxEmptyString);
	buttons->Add(m_result, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);
	sizer->Add(buttons, 0, wxEXPAND | wxALL, 10);

	m_log = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
						   wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
	sizer->Add(m_log, 1, wxEXPAND | wxLEFT | wxRIGHT, 10);

	auto* close = new wxButton(this, wxID_CLOSE, _("Close"));
	close->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Close(); });
	sizer->Add(close, 0, wxALIGN_RIGHT | wxALL, 10);
	SetSizer(sizer);

	Bind(wxEVT_TIMER, &GamePadSetupDialog::OnPoll, this);
	Bind(wxEVT_END_PROCESS, &GamePadSetupDialog::OnProcessEnded, this);
	Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& e) {
		if (m_proc && e.CanVeto())
		{
			wxMessageBox(_("Wait for the test to finish: it puts the adapter back when it's done."), _("Set up GamePad"),
						 wxOK | wxICON_INFORMATION, this);
			e.Veto();
			return;
		}
		e.Skip();
	});

	m_helper = FindHelper();
	Refresh();
}

GamePadSetupDialog::~GamePadSetupDialog()
{
	m_poll.Stop();
	if (m_proc)
		m_proc->Detach(); // deletes itself when the helper exits
}

void GamePadSetupDialog::Refresh()
{
	m_list->DeleteAllItems();
	if (m_helper.empty())
	{
		m_log->SetValue(_("The GamePad setup helper (cemu-gamepad-setup) was not found. Install the GamePad bridge first "
						  "(scripts/install.sh), or set CEMU_GAMEPAD_SETUP to the helper's path."));
		UpdateButtons();
		return;
	}
	wxArrayString out, err;
	const wxString cmd = wxString::Format("\"%s\" list", m_helper);
	const long rv = wxExecute(cmd, out, err, wxEXEC_SYNC | wxEXEC_NODISABLE);
	long row = 0;
	for (const auto& line : out)
	{
		const auto f = wxSplit(line, '\t', 0);
		if (f.size() < 7 || f[0] != "ADAPTER")
			continue;
		// ADAPTER <iface> <driver> <bus> <name> <verdict> <reason>
		const wxString verdict = f[5] == "works" ? _("Works") : f[5] == "maybe" ? _("Test it") : _("Won't work");
		const long i = m_list->InsertItem(row++, f[4] + " (" + f[2] + ", " + f[3].Upper() + ")");
		m_list->SetItem(i, kColInterface, f[1]);
		m_list->SetItem(i, kColVerdict, verdict);
		m_list->SetItem(i, kColDetails, f[6]);
		if (f[5] == "no")
			m_list->SetItemTextColour(i, wxColour(140, 140, 140));
	}
	m_log->Clear();
	if (rv != 0 || row == 0)
	{
		m_log->AppendText(row == 0 ? _("No Wi-Fi adapters found. Plug in a USB Wi-Fi adapter and press Refresh.\n")
								   : wxString(_("The adapter list may be incomplete:\n")));
		for (const auto& line : err)
			m_log->AppendText(line + "\n");
	}
	m_result->SetLabel(wxEmptyString);
	UpdateButtons();
}

void GamePadSetupDialog::UpdateButtons()
{
	const bool busy = m_proc != nullptr;
	m_refresh->Enable(!busy && !m_helper.empty());
	m_test->Enable(!busy && m_list->GetSelectedItemCount() == 1);
}

void GamePadSetupDialog::OnRefresh(wxCommandEvent&)
{
	Refresh();
}

void GamePadSetupDialog::OnSelectionChanged(wxListEvent&)
{
	UpdateButtons();
}

void GamePadSetupDialog::OnTest(wxCommandEvent&)
{
	const long sel = m_list->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
	if (sel < 0 || m_proc)
		return;
	const wxString iface = m_list->GetItemText(sel, kColInterface);
	if (m_list->GetItemText(sel, kColDetails).Contains("internet connection") &&
		wxMessageBox(_("This adapter is your internet connection right now. The test takes it over for a few seconds "
					   "and you'll be offline meanwhile. Continue?"),
					 _("Set up GamePad"), wxYES_NO | wxICON_WARNING, this) != wxYES)
		return;

	m_log->Clear();
	m_log->AppendText(wxString::Format(_("Testing %s. Your system will ask for your password.\n"), iface));
	m_result->SetLabel(_("Testing..."));
	m_partial.clear();
	m_sawResult = false;
	m_proc = new wxProcess(this);
	m_proc->Redirect();
	const wxString cmd = wxString::Format("pkexec \"%s\" test \"%s\"", m_helper, iface);
	if (wxExecute(cmd, wxEXEC_ASYNC, m_proc) <= 0)
	{
		delete m_proc;
		m_proc = nullptr;
		m_result->SetLabel(_("Could not start the test (is pkexec installed?)"));
		UpdateButtons();
		return;
	}
	m_poll.Start(100);
	UpdateButtons();
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
	if (line.StartsWith("RESULT works", &rest))
	{
		m_sawResult = true;
		m_result->SetLabel(_("This adapter works. Next: pair the GamePad."));
		m_result->SetForegroundColour(wxColour(0, 140, 60));
	}
	else if (line.StartsWith("RESULT fails", &rest))
	{
		m_sawResult = true;
		m_result->SetLabel(_("This adapter won't work:") + rest);
		m_result->SetForegroundColour(wxColour(200, 40, 40));
	}
	else if (line.StartsWith("STEP ", &rest))
		m_log->AppendText("...  " + rest + "\n");
	else if (line.StartsWith("OK ", &rest))
		m_log->AppendText("OK   " + rest + "\n");
	else if (line.StartsWith("FAIL ", &rest))
		m_log->AppendText("FAIL " + rest + "\n");
	else if (!line.empty())
		m_log->AppendText("     " + line + "\n");
	Layout();
}

void GamePadSetupDialog::OnPoll(wxTimerEvent&)
{
	DrainOutput();
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
	if (!m_sawResult)
	{
		// pkexec: 126 = the password dialog was dismissed, 127 = not authorized
		const int code = event.GetExitCode();
		m_result->SetLabel(code == 126 || code == 127 ? _("Test cancelled: administrator rights are needed.")
													  : wxString::Format(_("The test stopped unexpectedly (exit code %d)."), code));
		m_result->SetForegroundColour(wxColour(200, 40, 40));
	}
	Layout();
	UpdateButtons();
}
