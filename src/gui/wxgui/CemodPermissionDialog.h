#pragma once

#include "Cafe/OS/libs/cemuextend/cemuextend.h"

#include <wx/dialog.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

class wxCheckBox;

struct CemodPermissionSelection
{
	std::string principal;
	std::uint32_t requestedPermissions{};
	std::uint32_t grantedPermissions{};
};

class CemodPermissionDialog final : public wxDialog
{
public:
	CemodPermissionDialog(wxWindow* parent, const wxString& gameName,
		std::vector<CemodPermissionRequest> requests);

	[[nodiscard]] const std::vector<CemodPermissionSelection>& GetSelections() const
	{
		return m_selections;
	}

private:
	struct ModRow
	{
		CemodPermissionRequest request;
		std::array<wxCheckBox*, 5> permissions{};
	};

	void AllowAll();
	void SaveAndClose();

	std::vector<ModRow> m_rows;
	std::vector<CemodPermissionSelection> m_selections;
};
