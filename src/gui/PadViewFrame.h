#pragma once

#define WM_CREATE_PAD	(WM_USER+1)
#define WM_DESTROY_PAD	(WM_USER+2)

wxDECLARE_EVENT(EVT_PAD_CLOSE, wxCommandEvent);
wxDECLARE_EVENT(EVT_SET_WINDOW_TITLE, wxCommandEvent);

class PadViewFrame : public wxFrame
{
public:
	PadViewFrame(wxFrame* parent);
	~PadViewFrame();

	bool Initialize();
	void InitializeRenderCanvas();
	void DestroyCanvas();

	void OnKeyUp(wxKeyEvent& event);
	void OnChar(wxKeyEvent& event);
	
	void AsyncSetTitle(std::string_view windowTitle);

private:

	void OnMouseMove(wxMouseEvent& event);
	void OnMouseLeft(wxMouseEvent& event);
	void OnMouseRight(wxMouseEvent& event);
	void OnSizeEvent(wxSizeEvent& event);
	void OnDPIChangedEvent(wxDPIChangedEvent& event);
	void OnMoveEvent(wxMoveEvent& event);
	void OnGesturePan(wxPanGestureEvent& event);
	void OnSetWindowTitle(wxCommandEvent& event);

	wxWindow* m_render_canvas = nullptr;
};
