#include "CameraSettingsWindow.h"

#include "camera/CameraManager.h"

#include <wx/sizer.h>
#include <wx/dcclient.h>
#include <wx/dcbuffer.h>
#include <wx/rawbmp.h>

constexpr unsigned CAMERA_WIDTH = 640;
constexpr unsigned CAMERA_HEIGHT = 480;

CameraSettingsWindow::CameraSettingsWindow(wxWindow* parent)
	: wxDialog(parent, wxID_ANY, _("Camera settings"), wxDefaultPosition),
	  m_imageBitmap(CAMERA_WIDTH, CAMERA_HEIGHT, 24), m_imageBuffer(CAMERA_WIDTH * CAMERA_HEIGHT * 3)
{

	CameraManager::Init();
	CameraManager::Open();
	auto* rootSizer = new wxBoxSizer(wxVERTICAL);
	{
		auto* topSizer = new wxBoxSizer(wxHORIZONTAL);
		{
			m_cameraChoice = new wxChoice(this, wxID_ANY, wxDefaultPosition, {300, -1});
			m_cameraChoice->Bind(wxEVT_CHOICE, &CameraSettingsWindow::OnSelectCameraChoice, this);

			m_refreshButton = new wxButton(this, wxID_ANY, wxString::FromUTF8("⟳"));
			m_refreshButton->Fit();
			m_refreshButton->Bind(wxEVT_BUTTON, &CameraSettingsWindow::OnRefreshPressed, this);
			wxQueueEvent(m_refreshButton, new wxCommandEvent{wxEVT_BUTTON});

			topSizer->Add(m_cameraChoice);
			topSizer->Add(m_refreshButton);
		}

		m_imageWindow = new wxWindow(this, wxID_ANY, wxDefaultPosition, {CAMERA_WIDTH, CAMERA_HEIGHT});
		rootSizer->Add(topSizer);
		rootSizer->Add(m_imageWindow, wxEXPAND);
	}
	SetSizerAndFit(rootSizer);
	m_imageUpdateTimer.Bind(wxEVT_TIMER, &CameraSettingsWindow::UpdateImage, this);
	m_imageUpdateTimer.Start(33, wxTIMER_CONTINUOUS);
	this->Bind(wxEVT_CLOSE_WINDOW, &CameraSettingsWindow::OnClose, this);
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
	CameraManager::FillRGBBuffer(m_imageBuffer.data());

	wxNativePixelData data{m_imageBitmap};
	if (!data)
		return;
	wxNativePixelData::Iterator p{data};
	for (auto row = 0u; row < CAMERA_HEIGHT; ++row)
	{
		const auto* rowPtr = m_imageBuffer.data() + row * CAMERA_WIDTH * 3;
		wxNativePixelData::Iterator rowStart = p;
		for (auto col = 0u; col < CAMERA_WIDTH; ++col, ++p)
		{
			auto* colour = rowPtr + col * 3;
			p.Red() = colour[0];
			p.Green() = colour[1];
			p.Blue() = colour[2];
		}
		p = rowStart;
		p.OffsetY(data, 1);
	}

	wxClientDC dc{m_imageWindow};
	dc.DrawBitmap(m_imageBitmap, 0, 0);
}
void CameraSettingsWindow::OnClose(wxCloseEvent& event)
{
	CameraManager::Close();
	CameraManager::SaveDevice();
	event.Skip();
}