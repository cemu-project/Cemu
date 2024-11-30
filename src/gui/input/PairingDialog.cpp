#include "gui/wxgui.h"
#include "PairingDialog.h"

#if BOOST_OS_WINDOWS
#include <bluetoothapis.h>
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

    case PairingState::BluetoothFailed:
    {
        m_text->SetLabel(_("Failed to search for controllers."));
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

void PairingDialog::WorkerThread()
{
    const std::wstring wiimoteName = L"Nintendo RVL-CNT-01";
    const std::wstring wiiUProControllerName = L"Nintendo RVL-CNT-01-UC";

#if BOOST_OS_WINDOWS
    const GUID bthHidGuid = {0x00001124,0x0000,0x1000,{0x80,0x00,0x00,0x80,0x5F,0x9B,0x34,0xFB}};

    const BLUETOOTH_FIND_RADIO_PARAMS radioFindParams =
    {
        .dwSize = sizeof(BLUETOOTH_FIND_RADIO_PARAMS)
    };

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
        .dwSize = sizeof(BLUETOOTH_RADIO_INFO)
    };

    DWORD result = BluetoothGetRadioInfo(radio, &radioInfo);
    if (result != ERROR_SUCCESS)
    {
        UpdateCallback(PairingState::NoBluetoothAvailable);
        return;
    }

    const BLUETOOTH_DEVICE_SEARCH_PARAMS searchParams =
    {
        .dwSize               = sizeof(BLUETOOTH_DEVICE_SEARCH_PARAMS),

        .fReturnAuthenticated = FALSE,
        .fReturnRemembered    = FALSE,
        .fReturnUnknown       = TRUE,
        .fReturnConnected     = FALSE,

        .fIssueInquiry        = TRUE,
        .cTimeoutMultiplier   = 5,

        .hRadio               = radio
    };

    BLUETOOTH_DEVICE_INFO info =
    {
        .dwSize = sizeof(BLUETOOTH_DEVICE_INFO)
    };

    while (!m_threadShouldQuit)
    {
        HBLUETOOTH_DEVICE_FIND deviceFind = BluetoothFindFirstDevice(&searchParams, &info);
        if (deviceFind == nullptr)
        {
            UpdateCallback(PairingState::BluetoothFailed);
            return;
        }

        while (!m_threadShouldQuit)
        {
            if (info.szName == wiimoteName || info.szName == wiiUProControllerName)
            {
                BluetoothFindDeviceClose(deviceFind);

                UpdateCallback(PairingState::Pairing);

                wchar_t passwd[6] = { radioInfo.address.rgBytes[0], radioInfo.address.rgBytes[1], radioInfo.address.rgBytes[2], radioInfo.address.rgBytes[3], radioInfo.address.rgBytes[4], radioInfo.address.rgBytes[5] };
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
#else
    UpdateCallback(PairingState::BluetoothUnusable);
#endif
}

void PairingDialog::UpdateCallback(PairingState state)
{
    auto* event = new wxCommandEvent(wxEVT_PROGRESS_PAIR);
    event->SetInt((int)state);
    wxQueueEvent(this, event);
}