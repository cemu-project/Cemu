#pragma once

#include <wx/app.h>

class MainWindow;

class CemuApp : public wxApp
{
public:
	bool OnInit() override;
	int OnExit() override;

	void OnAssertFailure(const wxChar* file, int line, const wxChar* func, const wxChar* cond, const wxChar* msg) override;
	int FilterEvent(wxEvent& event) override;

	const std::vector<const wxLanguageInfo*>& GetLanguages() const { return m_languages; }
	static std::vector<const wxLanguageInfo*> GetAvailableLanguages();

	static void CreateDefaultFiles(bool first_start = false);
	static bool TrySelectMLCPath(std::wstring path);
	static bool SelectMLCPath(wxWindow* parent = nullptr);

	static wxString GetConfigPath();
	static wxString GetConfigPath(const wxString& cat);

	static wxString GetUserDataPath();
	static wxString GetUserDataPath(const wxString& cat);

	static wxString GetMLCPath();
	static wxString GetMLCPath(const wxString& cat);
private:
	void ActivateApp(wxActivateEvent& event);

	MainWindow* m_mainFrame = nullptr;

	wxLocale m_locale;
	std::vector<const wxLanguageInfo*> m_languages;
};

wxDECLARE_APP(CemuApp);
