#include "CameraSettingsWindow.h"

#include "input/camera/CameraManager.h"
#include <wx/button.h>
#include <wx/combobox.h>
#include <wx/sizer.h>
#include <wx/dcbuffer.h>
#include <wx/rawbmp.h>
#include <wx/stattext.h>

struct wxCameraDeviceUniqueId : public wxClientData
{
    std::string id;

    explicit wxCameraDeviceUniqueId(std::string id) : id(std::move(id))
    {
    }
};

CameraSettingsWindow::CameraSettingsWindow(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, _("Camera settings"), wxDefaultPosition),
      m_imageBitmap(CameraManager::CAMERA_WIDTH, CameraManager::CAMERA_HEIGHT, 24),
      m_imageBuffer(CameraManager::CAMERA_RGB_BUFFER_SIZE)
{
    CameraManager::Open();
    auto* rootSizer = new wxBoxSizer(wxVERTICAL);
    {
        auto* topSizer = new wxBoxSizer(wxHORIZONTAL);
        {
            m_cameraComboBox = new wxComboBox(this, wxID_ANY);
            m_cameraComboBox->Bind(wxEVT_COMBOBOX, &CameraSettingsWindow::OnSelectCameraChoice, this);
            m_cameraComboBox->SetToolTip(_("Cameras are only listed if they support 640x480"));

            m_refreshButton = new wxButton(this, wxID_ANY, _("Refresh"));
            m_refreshButton->Bind(wxEVT_BUTTON, &CameraSettingsWindow::OnRefreshPressed, this);
            wxQueueEvent(m_refreshButton, new wxCommandEvent{wxEVT_BUTTON});

            topSizer->Add(m_cameraComboBox);
            topSizer->Add(m_refreshButton);
        }

        m_imageWindow = new wxWindow(this, wxID_ANY, wxDefaultPosition,
                                     {CameraManager::CAMERA_WIDTH, CameraManager::CAMERA_HEIGHT});
        m_imageWindow->SetBackgroundStyle(wxBG_STYLE_PAINT);

        m_statusInfoText = new wxStaticText(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize,
                                            wxTE_READONLY);

        rootSizer->Add(topSizer);
        rootSizer->Add(m_imageWindow, wxEXPAND);
        rootSizer->Add(m_statusInfoText);
    }
    SetSizerAndFit(rootSizer);
    m_imageUpdateTimer.Bind(wxEVT_TIMER, &CameraSettingsWindow::Update, this);
    m_imageWindow->Bind(wxEVT_PAINT, &CameraSettingsWindow::DrawImage, this);
    this->Bind(wxEVT_CLOSE_WINDOW, &CameraSettingsWindow::OnClose, this);

    m_imageUpdateTimer.Start(33, wxTIMER_CONTINUOUS);
}

void CameraSettingsWindow::OnSelectCameraChoice(wxCommandEvent&)
{
    const auto selection = m_cameraComboBox->GetSelection();
    if (selection < 0)
    {
        m_cameraComboBox->Select(0);
        return;
    }
    if (selection == 0)
        CameraManager::ResetDevice();
    else
    {
        auto id = static_cast<wxCameraDeviceUniqueId*>(m_cameraComboBox->GetClientData(selection))->id;
        CameraManager::SetDevice(id);
    }
}

void CameraSettingsWindow::OnRefreshPressed(wxCommandEvent&)
{
    wxArrayString choices = {_("None")};
    const auto devices = CameraManager::EnumerateDevices();
    const auto currentDevice = CameraManager::GetCurrentDevice();
    auto currentIndex = 0;
    for (auto i = 0; i < devices.size(); ++i)
    {
        auto& device = devices[i];
        choices.push_back(fmt::format("{} ({})", device.name, device.uniqueId));
        if (currentDevice && device.uniqueId == *currentDevice)
            currentIndex = i + 1;
    }

    m_cameraComboBox->Set(choices);
    m_cameraComboBox->SetSelection(currentIndex);
    // Client data can only be set when there are entries
    for (auto i = 0; i < devices.size(); ++i)
    {
        auto& device = devices[i];
        const auto choiceIndex = i + 1;
        m_cameraComboBox->SetClientData(choiceIndex, new wxCameraDeviceUniqueId{device.uniqueId});
    }
}

void CameraSettingsWindow::Update(const wxTimerEvent&)
{
    switch (CameraManager::GetState())
    {
        using enum CameraManager::State;
    case NoSupport:
        {
            m_statusInfoText->SetLabel(_("This build does not support camera capture"));
            return;
        }
    case NoDevice:
        {
            m_statusInfoText->SetLabel(_("No camera selected"));
            return;
        }
    case UnsupportedFormat:
        {
            m_statusInfoText->SetLabel(_("Camera does not support the required format"));
        }
    case NotOpen:
        {
            m_statusInfoText->SetLabel(_("Camera is unavailable, or was closed due to an error"));
            return;
        }
    case NeedsPermission:
        {
            m_statusInfoText->SetLabel(_("Waiting for permission to access camea"));
            return;
        }
    case NoPermission:
        {
            m_statusInfoText->SetLabel(_("Cemu was denied permission to access camera"));
            return;
        }
    case Capturing:
        {
            m_statusInfoText->SetLabel(_("Capturing"));
            break;
        }
    default:
        cemu_assert(false);
    }
    CameraManager::FillRGBBuffer(std::span<uint8, CameraManager::CAMERA_RGB_BUFFER_SIZE>(m_imageBuffer));
    wxNativePixelData data{m_imageBitmap};
    if (!data)
        return;
    wxNativePixelData::Iterator p{data};
    for (auto row = 0u; row < CameraManager::CAMERA_HEIGHT; ++row)
    {
        const auto* rowPtr = m_imageBuffer.data() + row * CameraManager::CAMERA_WIDTH * 3;
        wxNativePixelData::Iterator rowStart = p;
        for (auto col = 0u; col < CameraManager::CAMERA_WIDTH; ++col, ++p)
        {
            auto* colour = rowPtr + col * 3;
            p.Red() = colour[0];
            p.Green() = colour[1];
            p.Blue() = colour[2];
        }
        p = rowStart;
        p.OffsetY(data, 1);
    }
    m_imageWindow->Refresh();
}

void CameraSettingsWindow::DrawImage(const wxPaintEvent&)
{
    wxAutoBufferedPaintDC dc{m_imageWindow};
    dc.DrawBitmap(m_imageBitmap, 0, 0);
}

void CameraSettingsWindow::OnClose(wxCloseEvent& event)
{
    m_imageUpdateTimer.Stop();
    CameraManager::SaveDevice();
    CameraManager::Close();
    event.Skip();
}
