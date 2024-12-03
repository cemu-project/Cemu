#include "gui/wxgui.h"
#include "PairingDialog.h"

#if BOOST_OS_WINDOWS
#include <bluetoothapis.h>
#endif
#if BOOST_OS_LINUX
#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <input/api/Wiimote/l2cap/L2CapWiimote.h>
#endif

wxDECLARE_EVENT(wxEVT_PROGRESS_PAIR, wxCommandEvent);
wxDEFINE_EVENT(wxEVT_PROGRESS_PAIR, wxCommandEvent);

PairingDialog::PairingDialog(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, _("Pairing..."), wxDefaultPosition, wxDefaultSize, wxCAPTION | wxMINIMIZE_BOX | wxSYSTEM_MENU | wxTAB_TRAVERSAL | wxCLOSE_BOX)
{
	auto* sizer = new wxBoxSizer(wxVERTICAL);
	m_gauge = new wxGauge(this, wxID_ANY, 100, wxDefaultPosition, wxSize(350, 20), wxGA_HORIZONTAL);
	m_gauge->SetValue(0);
	sizer->Add(m_gauge, 0, wxALL | wxEXPAND, 5);

	auto* rows = new wxFlexGridSizer(0, 2, 0, 0);
	rows->AddGrowableCol(1);

	m_text = new wxStaticText(this, wxID_ANY, _("Searching for controllers..."));
	rows->Add(m_text, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

	{
		auto* right_side = new wxBoxSizer(wxHORIZONTAL);

		m_cancelButton = new wxButton(this, wxID_ANY, _("Cancel"));
		m_cancelButton->Bind(wxEVT_BUTTON, &PairingDialog::OnCancelButton, this);
		right_side->Add(m_cancelButton, 0, wxALL, 5);

		rows->Add(right_side, 1, wxALIGN_RIGHT, 5);
	}

	sizer->Add(rows, 0, wxALL | wxEXPAND, 5);

	SetSizerAndFit(sizer);
	Centre(wxBOTH);

	Bind(wxEVT_CLOSE_WINDOW, &PairingDialog::OnClose, this);
	Bind(wxEVT_PROGRESS_PAIR, &PairingDialog::OnGaugeUpdate, this);

	m_thread = std::thread(&PairingDialog::WorkerThread, this);
}

PairingDialog::~PairingDialog()
{
	Unbind(wxEVT_CLOSE_WINDOW, &PairingDialog::OnClose, this);
}

void PairingDialog::OnClose(wxCloseEvent& event)
{
	event.Skip();

	m_threadShouldQuit = true;
	if (m_thread.joinable())
		m_thread.join();
}

void PairingDialog::OnCancelButton(const wxCommandEvent& event)
{
	Close();
}

void PairingDialog::OnGaugeUpdate(wxCommandEvent& event)
{
	PairingState state = (PairingState)event.GetInt();

	switch (state)
	{
	case PairingState::Pairing:
	{
		m_text->SetLabel(_("Found controller. Pairing..."));
		m_gauge->SetValue(50);
		break;
	}

	case PairingState::Finished:
	{
		m_text->SetLabel(_("Successfully paired the controller."));
		m_gauge->SetValue(100);
		m_cancelButton->SetLabel(_("Close"));
		break;
	}

	case PairingState::NoBluetoothAvailable:
	{
		m_text->SetLabel(_("Failed to find a suitable Bluetooth radio."));
		m_gauge->SetValue(0);
		m_cancelButton->SetLabel(_("Close"));
		break;
	}

	case PairingState::SearchFailed:
	{
		m_text->SetLabel(_("Failed to find controllers."));
		m_gauge->SetValue(0);
		m_cancelButton->SetLabel(_("Close"));
		break;
	}

	case PairingState::PairingFailed:
	{
		m_text->SetLabel(_("Failed to pair with the found controller."));
		m_gauge->SetValue(0);
		m_cancelButton->SetLabel(_("Close"));
		break;
	}

	case PairingState::BluetoothUnusable:
	{
		m_text->SetLabel(_("Please use your system's Bluetooth manager instead."));
		m_gauge->SetValue(0);
		m_cancelButton->SetLabel(_("Close"));
		break;
	}

	default:
	{
		break;
	}
	}
}

#if BOOST_OS_WINDOWS
void PairingDialog::WorkerThread()
{
	const std::wstring wiimoteName = L"Nintendo RVL-CNT-01";
	const std::wstring wiiUProControllerName = L"Nintendo RVL-CNT-01-UC";

	const GUID bthHidGuid = {0x00001124, 0x0000, 0x1000, {0x80, 0x00, 0x00, 0x80, 0x5F, 0x9B, 0x34, 0xFB}};

	const BLUETOOTH_FIND_RADIO_PARAMS radioFindParams =
		{
			.dwSize = sizeof(BLUETOOTH_FIND_RADIO_PARAMS)};

	HANDLE radio = INVALID_HANDLE_VALUE;
	HBLUETOOTH_RADIO_FIND radioFind = BluetoothFindFirstRadio(&radioFindParams, &radio);
	if (radioFind == nullptr)
	{
		UpdateCallback(PairingState::NoBluetoothAvailable);
		return;
	}

	BluetoothFindRadioClose(radioFind);

	BLUETOOTH_RADIO_INFO radioInfo =
		{
			.dwSize = sizeof(BLUETOOTH_RADIO_INFO)};

	DWORD result = BluetoothGetRadioInfo(radio, &radioInfo);
	if (result != ERROR_SUCCESS)
	{
		UpdateCallback(PairingState::NoBluetoothAvailable);
		return;
	}

	const BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams =
		{
			.dwSize = sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS),

			.fReturnAuthenticated = FALSE,
			.fReturnRemembered = FALSE,
			.fReturnUnknown = TRUE,
			.fReturnConnected = FALSE,

			.fIssueInquiry = TRUE,
			.cTimeoutMultiplier = 5,

			.hRadio = radio};

	BLUETOOTH_DEVICE_INFO info =
		{
			.dwSize = sizeof(BLUETOOTH_DEVICE_INFO)};

	while (!m_threadShouldQuit)
	{
		HBLUETOOTH_DEVICE_FIND deviceFind = BluetoothFindFirstDevice(&searchParams, &info);
		if (deviceFind == nullptr)
		{
			UpdateCallback(PairingState::SearchFailed);
			return;
		}

		while (!m_threadShouldQuit)
		{
			if (info.szName == wiimoteName || info.szName == wiiUProControllerName)
			{
				BluetoothFindDeviceClose(deviceFind);

				UpdateCallback(PairingState::Pairing);

				wchar_t passwd[6] = {radioInfo.address.rgBytes[0], radioInfo.address.rgBytes[1], radioInfo.address.rgBytes[2], radioInfo.address.rgBytes[3], radioInfo.address.rgBytes[4], radioInfo.address.rgBytes[5]};
				DWORD bthResult = BluetoothAuthenticateDevice(nullptr, radio, &info, passwd, 6);
				if (bthResult != ERROR_SUCCESS)
				{
					UpdateCallback(PairingState::PairingFailed);
					return;
				}

				bthResult = BluetoothSetServiceState(radio, &info, &bthHidGuid, BLUETOOTH_SERVICE_ENABLE);
				if (bthResult != ERROR_SUCCESS)
				{
					UpdateCallback(PairingState::PairingFailed);
					return;
				}

				UpdateCallback(PairingState::Finished);
				return;
			}

			BOOL nextDevResult = BluetoothFindNextDevice(deviceFind, &info);
			if (nextDevResult == FALSE)
			{
				break;
			}
		}

		BluetoothFindDeviceClose(deviceFind);
	}
}
#elif BOOST_OS_LINUX
void PairingDialog::WorkerThread()
{
	constexpr static uint8_t LIAC_LAP[] = {0x00, 0x8b, 0x9e};

	constexpr static auto isWiimoteName = [](std::string_view name) {
		return name == "Nintendo RVL-CNT-01" || name == "Nintendo RVL-CNT-01-TR";
	};

	// Get default BT device
	const auto hostId = hci_get_route(nullptr);
	if (hostId < 0)
	{
		UpdateCallback(PairingState::NoBluetoothAvailable);
		return;
	}

	// Search for device
	inquiry_info* infos = nullptr;
	m_cancelButton->Disable();
	const auto respCount = hci_inquiry(hostId, 7, 4, LIAC_LAP, &infos, IREQ_CACHE_FLUSH);
	m_cancelButton->Enable();
	if (respCount <= 0)
	{
		UpdateCallback(PairingState::SearchFailed);
		return;
	}
	stdx::scope_exit infoFree([&]() { bt_free(infos);});

	if (m_threadShouldQuit)
		return;

	// Open dev to read name
	const auto hostDev = hci_open_dev(hostId);
	stdx::scope_exit devClose([&]() { hci_close_dev(hostDev);});

	char nameBuffer[HCI_MAX_NAME_LENGTH] = {};

	bool foundADevice = false;
	// Get device name and compare. Would use product and vendor id from SDP, but many third-party Wiimotes don't store them
	for (const auto& devInfo : std::span(infos, respCount))
	{
		const auto& addr = devInfo.bdaddr;
		const auto err =  hci_read_remote_name(hostDev, &addr, HCI_MAX_NAME_LENGTH, nameBuffer,
								 2000);
		if (m_threadShouldQuit)
			return;
		if (err || !isWiimoteName(nameBuffer))
			continue;

		L2CapWiimote::AddCandidateAddress(addr);
		foundADevice = true;
		const auto& b = addr.b;
		cemuLog_log(LogType::Force, "Pairing Dialog: Found '{}' with address '{:02x}:{:02x}:{:02x}:{:02x}:{:02x}:{:02x}'",
					nameBuffer, b[5], b[4], b[3], b[2], b[1], b[0]);
	}
	if (foundADevice)
		UpdateCallback(PairingState::Finished);
	else
		UpdateCallback(PairingState::SearchFailed);
}
#else
void PairingDialog::WorkerThread()
{
	UpdateCallback(PairingState::BluetoothUnusable);
}
#endif
void PairingDialog::UpdateCallback(PairingState state)
{
	auto* event = new wxCommandEvent(wxEVT_PROGRESS_PAIR);
	event->SetInt((int)state);
	wxQueueEvent(this, event);
}