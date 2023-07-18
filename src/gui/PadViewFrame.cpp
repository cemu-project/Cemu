#include "gui/wxgui.h"
#include "gui/guiWrapper.h"
#include "gui/PadViewFrame.h"

#include <wx/display.h>

#include "config/ActiveSettings.h"
#include "Cafe/OS/libs/swkbd/swkbd.h"
#include "gui/canvas/OpenGLCanvas.h"
#include "gui/canvas/VulkanCanvas.h"
#include "config/CemuConfig.h"
#include "gui/MainWindow.h"
#include "gui/helpers/wxHelpers.h"
#include "input/InputManager.h"

#if BOOST_OS_LINUX || BOOST_OS_MACOS
#include "resource/embedded/resources.h"
#endif
#include "wxHelper.h"

extern WindowInfo g_window_info;

PadViewFrame::PadViewFrame(wxFrame* parent)
	: wxFrame(nullptr, wxID_ANY, _("GamePad View"), wxDefaultPosition, wxSize(854, 480), wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN | wxRESIZE_BORDER | wxCLOSE_BOX | wxWANTS_CHARS)
{
	gui_initHandleContextFromWxWidgetsWindow(g_window_info.window_pad, this);
	
	SetIcon(wxICON(M_WND_ICON128));
	wxWindow::EnableTouchEvents(wxTOUCH_PAN_GESTURES);

	SetMinClientSize({ 320, 180 });

	SetPosition({ g_window_info.restored_pad_x, g_window_info.restored_pad_y });
	SetSize({ g_window_info.restored_pad_width, g_window_info.restored_pad_height });

	if (g_window_info.pad_maximized)
		Maximize();

	Bind(wxEVT_SIZE, &PadViewFrame::OnSizeEvent, this);
	Bind(wxEVT_DPI_CHANGED, &PadViewFrame::OnDPIChangedEvent, this);
	Bind(wxEVT_MOVE, &PadViewFrame::OnMoveEvent, this);
	Bind(wxEVT_MOTION, &PadViewFrame::OnMouseMove, this);

	Bind(wxEVT_SET_WINDOW_TITLE, &PadViewFrame::OnSetWindowTitle, this);

	g_window_info.pad_open = true;
}

PadViewFrame::~PadViewFrame()
{
	g_window_info.pad_open = false;
}

bool PadViewFrame::Initialize()
{
	const wxSize client_size = GetClientSize();
	g_window_info.pad_width = client_size.GetWidth();
	g_window_info.pad_height = client_size.GetHeight();
	g_window_info.phys_pad_width = ToPhys(client_size.GetWidth());
	g_window_info.phys_pad_height = ToPhys(client_size.GetHeight());

	return true;
}

void PadViewFrame::InitializeRenderCanvas()
{
	auto sizer = new wxBoxSizer(wxVERTICAL);
	{
		if (ActiveSettings::GetGraphicsAPI() == kVulkan)
			m_render_canvas = new VulkanCanvas(this, wxSize(854, 480), false);
		else
			m_render_canvas = GLCanvas_Create(this, wxSize(854, 480), false);
		sizer->Add(m_render_canvas, 1, wxEXPAND, 0, nullptr);
	}
	SetSizer(sizer);
	Layout();

	m_render_canvas->Bind(wxEVT_KEY_UP, &PadViewFrame::OnKeyUp, this);
	m_render_canvas->Bind(wxEVT_CHAR, &PadViewFrame::OnChar, this);

	m_render_canvas->Bind(wxEVT_MOTION, &PadViewFrame::OnMouseMove, this);
	m_render_canvas->Bind(wxEVT_LEFT_DOWN, &PadViewFrame::OnMouseLeft, this);
	m_render_canvas->Bind(wxEVT_LEFT_UP, &PadViewFrame::OnMouseLeft, this);
	m_render_canvas->Bind(wxEVT_RIGHT_DOWN, &PadViewFrame::OnMouseRight, this);
	m_render_canvas->Bind(wxEVT_RIGHT_UP, &PadViewFrame::OnMouseRight, this);

	m_render_canvas->Bind(wxEVT_GESTURE_PAN, &PadViewFrame::OnGesturePan, this);

	m_render_canvas->SetFocus();
	SendSizeEvent();
}

void PadViewFrame::DestroyCanvas()
{
	if(!m_render_canvas)
		return;
	m_render_canvas->Destroy();
	m_render_canvas = nullptr;
}

void PadViewFrame::OnSizeEvent(wxSizeEvent& event)
{
	if (!IsMaximized() && !IsFullScreen())
	{
		g_window_info.restored_pad_width = GetSize().x;
		g_window_info.restored_pad_height = GetSize().y;
	}
	g_window_info.pad_maximized = IsMaximized() && !IsFullScreen();

	const wxSize client_size = GetClientSize();
	g_window_info.pad_width = client_size.GetWidth();
	g_window_info.pad_height = client_size.GetHeight();
	g_window_info.phys_pad_width = ToPhys(client_size.GetWidth());
	g_window_info.phys_pad_height = ToPhys(client_size.GetHeight());
	g_window_info.pad_dpi_scale = GetDPIScaleFactor();

	event.Skip();
}

void PadViewFrame::OnDPIChangedEvent(wxDPIChangedEvent& event)
{
	event.Skip();
	const wxSize client_size = GetClientSize();
	g_window_info.pad_width = client_size.GetWidth();
	g_window_info.pad_height = client_size.GetHeight();
	g_window_info.phys_pad_width = ToPhys(client_size.GetWidth());
	g_window_info.phys_pad_height = ToPhys(client_size.GetHeight());
	g_window_info.pad_dpi_scale = GetDPIScaleFactor();
}

void PadViewFrame::OnMoveEvent(wxMoveEvent& event)
{
	if (!IsMaximized() && !IsFullScreen())
	{
		g_window_info.restored_pad_x = GetPosition().x;
		g_window_info.restored_pad_y = GetPosition().y;
	}
}

void PadViewFrame::OnKeyUp(wxKeyEvent& event)
{
	event.Skip();

	if (swkbd_hasKeyboardInputHook())
		return;

	const auto code = event.GetKeyCode();
	if (code == WXK_ESCAPE)
		ShowFullScreen(false);
	else if (code == WXK_RETURN && event.AltDown() || code == WXK_F11)
		ShowFullScreen(!IsFullScreen());
}

void PadViewFrame::OnGesturePan(wxPanGestureEvent& event)
{
	auto& instance = InputManager::instance();

	std::scoped_lock lock(instance.m_pad_touch.m_mutex);
	auto physPos = ToPhys(event.GetPosition());
	instance.m_pad_touch.position = { physPos.x, physPos.y };
	instance.m_pad_touch.left_down = event.IsGestureStart() || !event.IsGestureEnd();
	if (event.IsGestureStart() || !event.IsGestureEnd())
		instance.m_pad_touch.left_down_toggle = true;
}

void PadViewFrame::OnChar(wxKeyEvent& event)
{
	if (swkbd_hasKeyboardInputHook())
		swkbd_keyInput(event.GetUnicodeKey());
	
	event.Skip();
}

void PadViewFrame::OnMouseMove(wxMouseEvent& event)
{
	auto& instance = InputManager::instance();

	std::scoped_lock lock(instance.m_pad_touch.m_mutex);
	auto physPos = ToPhys(event.GetPosition());
	instance.m_pad_mouse.position = { physPos.x, physPos.y };

	event.Skip();
}

void PadViewFrame::OnMouseLeft(wxMouseEvent& event)
{
	auto& instance = InputManager::instance();

	std::scoped_lock lock(instance.m_pad_mouse.m_mutex);
	instance.m_pad_mouse.left_down = event.ButtonDown(wxMOUSE_BTN_LEFT);
	auto physPos = ToPhys(event.GetPosition());
	instance.m_pad_mouse.position = { physPos.x, physPos.y };
	if (event.ButtonDown(wxMOUSE_BTN_LEFT))
		instance.m_pad_mouse.left_down_toggle = true;
	
}

void PadViewFrame::OnMouseRight(wxMouseEvent& event)
{
	auto& instance = InputManager::instance();

	std::scoped_lock lock(instance.m_pad_mouse.m_mutex);
	instance.m_pad_mouse.right_down = event.ButtonDown(wxMOUSE_BTN_LEFT);
	auto physPos = ToPhys(event.GetPosition());
	instance.m_pad_mouse.position = { physPos.x, physPos.y };
	if (event.ButtonDown(wxMOUSE_BTN_RIGHT))
		instance.m_pad_mouse.right_down_toggle = true;
}

void PadViewFrame::OnSetWindowTitle(wxCommandEvent& event)
{
	this->SetTitle(event.GetString());
}

void PadViewFrame::AsyncSetTitle(std::string_view windowTitle)
{
	wxCommandEvent set_title_event(wxEVT_SET_WINDOW_TITLE);
	set_title_event.SetString(wxHelper::FromUtf8(windowTitle));
	QueueEvent(set_title_event.Clone());
}
