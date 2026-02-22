#include "CameraSettingsWindow.h"

#include "camera/CameraManager.h"

#include <wx/sizer.h>
#include <wx/dcbuffer.h>
#include <wx/rawbmp.h>

CameraSettingsWindow::CameraSettingsWindow(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, _("Camera settings"), wxDefaultPosition),
      m_imageBitmap(CameraManager::CAMERA_WIDTH, CameraManager::CAMERA_HEIGHT, 24),
      m_imageBuffer(CameraManager::CAMERA_RGB_BUFFER_SIZE)
{
    CameraManager::Init();
    CameraManager::Open();
    auto* rootSizer = new wxBoxSizer(wxVERTICAL);
    {
        auto* topSizer = new wxBoxSizer(wxHORIZONTAL);
        {
            m_cameraChoice = new wxChoice(this, wxID_ANY, wxDefaultPosition, {300, -1});
            m_cameraChoice->Bind(wxEVT_CHOICE, &CameraSettingsWindow::OnSelectCameraChoice, this);
            m_cameraChoice->SetToolTip(_("Cameras are only listed if they support 640x480"));

            m_refreshButton = new wxButton(this, wxID_ANY, wxString::FromUTF8("⟳"));
            m_refreshButton->Fit();
            m_refreshButton->Bind(wxEVT_BUTTON, &CameraSettingsWindow::OnRefreshPressed, this);
            wxQueueEvent(m_refreshButton, new wxCommandEvent{wxEVT_BUTTON});

            topSizer->Add(m_cameraChoice);
            topSizer->Add(m_refreshButton);
        }

        m_imageWindow = new wxWindow(this, wxID_ANY, wxDefaultPosition,
                                     {CameraManager::CAMERA_WIDTH, CameraManager::CAMERA_HEIGHT});
        m_imageWindow->SetBackgroundStyle(wxBG_STYLE_PAINT);

        rootSizer->Add(topSizer);
        rootSizer->Add(m_imageWindow, wxEXPAND);
    }
    SetSizerAndFit(rootSizer);
    m_imageUpdateTimer.Bind(wxEVT_TIMER, &CameraSettingsWindow::UpdateImage, this);
    m_imageWindow->Bind(wxEVT_PAINT, &CameraSettingsWindow::DrawImage, this);
    this->Bind(wxEVT_CLOSE_WINDOW, &CameraSettingsWindow::OnClose, this);

    m_imageUpdateTimer.Start(33, wxTIMER_CONTINUOUS);
}

void CameraSettingsWindow::OnSelectCameraChoice(wxCommandEvent&)
{
    const auto selection = m_cameraChoice->GetSelection();
    if (selection < 0)
        return;
    if (selection == 0)
        CameraManager::SetDevice(CameraManager::DEVICE_NONE);
    else
        CameraManager::SetDevice(selection - 1);
}

void CameraSettingsWindow::OnRefreshPressed(wxCommandEvent&)
{
    wxArrayString choices = {_("None")};
    for (const auto& entry : CameraManager::EnumerateDevices())
    {
        choices.push_back(entry.name);
    }
    m_cameraChoice->Set(choices);
    if (auto currentDevice = CameraManager::GetCurrentDevice())
        m_cameraChoice->SetSelection(*currentDevice + 1);
}

void CameraSettingsWindow::UpdateImage(const wxTimerEvent&)
{
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
    CameraManager::Close();
    CameraManager::SaveDevice();
    event.Skip();
}
