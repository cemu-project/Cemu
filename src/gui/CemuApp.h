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

	std::vector<const wxLanguageInfo*> GetLanguages() const;

	static bool CheckMLCPath(const fs::path& mlc);
	static bool CreateDefaultMLCFiles(const fs::path& mlc);
	static void CreateDefaultCemuFiles();

	static void InitializeNewMLCOrFail(fs::path mlc);
	static void InitializeExistingMLCOrFail(fs::path mlc);
private:
	void LocalizeUI(wxLanguage languageToUse);

	void DeterminePaths(std::set<fs::path>& failedWriteAccess);

	void ActivateApp(wxActivateEvent& event);
	static std::vector<const wxLanguageInfo*> GetAvailableTranslationLanguages(wxTranslations* translationsMgr);

	MainWindow* m_mainFrame = nullptr;

	wxLocale m_locale;
	std::vector<const wxLanguageInfo*> m_availableTranslations;
};

wxDECLARE_APP(CemuApp);
