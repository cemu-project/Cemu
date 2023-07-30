#include "wxgui.h"
#include "PadViewFrame.h"

#include <wx/display.h>

#include "config/ActiveSettings.h"
#include "Cafe/OS/libs/swkbd/swkbd.h"
#include "canvas/OpenGLCanvas.h"
#include "canvas/VulkanCanvas.h"
#include "config/CemuConfig.h"
#include "MainWindow.h"
#include "helpers/wxHelpers.h"
#include "input/InputManager.h"
#include "Cemu/GuiSystem/GuiSystem.h"

#if BOOST_OS_LINUX || BOOST_OS_MACOS
#include "resource/embedded/resources.h"
#endif
#include "wxHelper.h"

PadViewFrame::PadViewFrame(wxFrame* parent)
	: wxFrame(nullptr, wxID_ANY, _("GamePad View"), wxDefaultPosition, wxSize(854, 480), wxMINIMIZE_BOX | wxMAXIMIZE_BOX | wxSYSTEM_MENU | wxCAPTION | wxCLIP_CHILDREN | wxRESIZE_BORDER | wxCLOSE_BOX | wxWANTS_CHARS)
{
	auto& windowInfo = GuiSystem::getWindowInfo();
	windowInfo.window_pad = get_window_handle_info_for_wxWindow(this);
	SetIcon(wxICON(M_WND_ICON128));
	wxWindow::EnableTouchEvents(wxTOUCH_PAN_GESTURES);

	SetMinClientSize({ 320, 180 });

	SetPosition({ windowInfo.restored_pad_x, windowInfo.restored_pad_y });
	SetSize({ windowInfo.restored_pad_width, windowInfo.restored_pad_height });

	if (windowInfo.pad_maximized)
		Maximize();

	Bind(wxEVT_SIZE, &PadViewFrame::OnSizeEvent, this);
	Bind(wxEVT_DPI_CHANGED, &PadViewFrame::OnDPIChangedEvent, this);
	Bind(wxEVT_MOVE, &PadViewFrame::OnMoveEvent, this);
	Bind(wxEVT_MOTION, &PadViewFrame::OnMouseMove, this);

	Bind(wxEVT_SET_WINDOW_TITLE, &PadViewFrame::OnSetWindowTitle, this);

	windowInfo.pad_open = true;
}

PadViewFrame::~PadViewFrame()
{
	GuiSystem::getWindowInfo().pad_open = false;
}

bool PadViewFrame::Initialize()
{
	auto& windowInfo = GuiSystem::getWindowInfo();
	const wxSize client_size = GetClientSize();
	windowInfo.pad_width = client_size.GetWidth();
	windowInfo.pad_height = client_size.GetHeight();
	windowInfo.phys_pad_width = ToPhys(client_size.GetWidth());
	windowInfo.phys_pad_height = ToPhys(client_size.GetHeight());

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
	auto& windowInfo = GuiSystem::getWindowInfo();
	if (!IsMaximized() && !IsFullScreen())
	{
		windowInfo.restored_pad_width = GetSize().x;
		windowInfo.restored_pad_height = GetSize().y;
	}
	windowInfo.pad_maximized = IsMaximized() && !IsFullScreen();

	const wxSize client_size = GetClientSize();
	windowInfo.pad_width = client_size.GetWidth();
	windowInfo.pad_height = client_size.GetHeight();
	windowInfo.phys_pad_width = ToPhys(client_size.GetWidth());
	windowInfo.phys_pad_height = ToPhys(client_size.GetHeight());
	windowInfo.pad_dpi_scale = GetDPIScaleFactor();

	event.Skip();
}

void PadViewFrame::OnDPIChangedEvent(wxDPIChangedEvent& event)
{
	event.Skip();
	auto& windowInfo = GuiSystem::getWindowInfo();
	const wxSize client_size = GetClientSize();
	windowInfo.pad_width = client_size.GetWidth();
	windowInfo.pad_height = client_size.GetHeight();
	windowInfo.phys_pad_width = ToPhys(client_size.GetWidth());
	windowInfo.phys_pad_height = ToPhys(client_size.GetHeight());
	windowInfo.pad_dpi_scale = GetDPIScaleFactor();
}

void PadViewFrame::OnMoveEvent(wxMoveEvent& event)
{
	if (!IsMaximized() && !IsFullScreen())
	{
		auto& windowInfo = GuiSystem::getWindowInfo();
		windowInfo.restored_pad_x = GetPosition().x;
		windowInfo.restored_pad_y = GetPosition().y;
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
