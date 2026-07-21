#pragma once
#include <wx/dialog.h>
#include <wx/timer.h>
#include <wx/bitmap.h>

class wxButton;
class wxComboBox;
class wxStaticText;

class CameraSettingsWindow : public wxDialog
{
    wxComboBox* m_cameraComboBox;
    wxButton* m_refreshButton;
    wxWindow* m_imageWindow;
    wxBitmap m_imageBitmap;
    wxStaticText* m_statusInfoText;
    wxTimer m_imageUpdateTimer;
    std::vector<uint8> m_imageBuffer;

public:
    explicit CameraSettingsWindow(wxWindow* parent);
    void OnSelectCameraChoice(wxCommandEvent&);
    void OnRefreshPressed(wxCommandEvent&);
    void Update(const wxTimerEvent&);
    void DrawImage(const wxPaintEvent&);
    void OnClose(wxCloseEvent& event);
};
