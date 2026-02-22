#pragma once
#include <wx/dialog.h>
#include <wx/timer.h>
#include <wx/choice.h>
#include <wx/bmpbuttn.h>

class CameraSettingsWindow : public wxDialog
{
	wxChoice* m_cameraChoice;
	wxButton* m_refreshButton;
	wxWindow* m_imageWindow;
	wxBitmap m_imageBitmap;
	wxTimer m_imageUpdateTimer;
	std::vector<uint8> m_imageBuffer;
  public:
	explicit CameraSettingsWindow(wxWindow* parent);
	void OnSelectCameraChoice(wxCommandEvent&);
	void OnRefreshPressed(wxCommandEvent&);
	void UpdateImage(const wxTimerEvent&);
	void DrawImage(const wxPaintEvent&);
	void OnClose(wxCloseEvent& event);
};