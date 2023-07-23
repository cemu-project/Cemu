#include "helpers/wxHelpers.h"

#include <wx/wupdlock.h>
#include <wx/stattext.h>
#include <wx/slider.h>
#include <wx/dirdlg.h>

#include "helpers/wxControlObject.h"

#if BOOST_OS_LINUX
#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk/gdkwindow.h>
#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#ifdef HAS_WAYLAND
#include <gdk/gdkwayland.h>
#endif
#endif

void wxAutosizeColumn(wxListCtrlBase* ctrl, int col)
{
	ctrl->SetColumnWidth(col, wxLIST_AUTOSIZE_USEHEADER);
	int wh = ctrl->GetColumnWidth(col);
	ctrl->SetColumnWidth(col, wxLIST_AUTOSIZE);
	int wc = ctrl->GetColumnWidth(col);
	if (wh > wc)
		ctrl->SetColumnWidth(col, wxLIST_AUTOSIZE_USEHEADER);
}

void wxAutosizeColumns(wxListCtrlBase* ctrl, int col_start, int col_end)
{
	wxWindowUpdateLocker lock(ctrl);
	for (int i = col_start; i <= col_end; ++i)
		wxAutosizeColumn(ctrl, i);
}

void update_slider_text(wxCommandEvent& event, const wxFormatString& format /*= "%d%%"*/)
{
	const auto slider = dynamic_cast<wxSlider*>(event.GetEventObject());
	wxASSERT(slider);

	auto slider_text = dynamic_cast<wxControlObject*>(event.GetEventUserData())->GetControl<wxStaticText>();
	wxASSERT(slider_text);

	slider_text->SetLabel(wxString::Format(format, slider->GetValue()));
}

uint32 fix_raw_keycode(uint32 keycode, uint32 raw_flags)
{
#if BOOST_OS_WINDOWS
	const auto flags = (HIWORD(raw_flags) & 0xFFF);
	if(keycode == VK_SHIFT)
	{
		if(flags == 0x2A)
			return 160;
		else if (flags == 0x36)
			return 161;
	}
	else if (keycode == VK_CONTROL)
	{
		if (flags == 0x1d)
			return 162;
		else if (flags == 0x11d)
			return 163;
	}
	else if (keycode == VK_MENU)
	{
		if ((flags & 0xFF) == 0x38)
			return 164;
		else if ((flags & 0xFF) == 0x38)
			return 165;
	}
#endif

	return keycode;
}

std::string rawKeyCodeToString(uint32 keyCode)
{
#if BOOST_OS_WINDOWS
	LONG scan_code = MapVirtualKeyA((UINT)keyCode, MAPVK_VK_TO_VSC_EX);
	if (HIBYTE(scan_code))
		scan_code |= 0x100;

	// because MapVirtualKey strips the extended bit for some keys
	switch (keyCode)
	{
		case VK_LEFT:
		case VK_UP:
		case VK_RIGHT:
		case VK_DOWN:  // arrow keys
		case VK_PRIOR:
		case VK_NEXT:  // page up and page down
		case VK_END:
		case VK_HOME:
		case VK_INSERT:
		case VK_DELETE:
		case VK_DIVIDE:  // numpad slash
		case VK_NUMLOCK:
		{
			scan_code |= 0x100;  // set extended bit
			break;
		}
	}

	scan_code <<= 16;

	char key_name[128];
	if (GetKeyNameTextA(scan_code, key_name, std::size(key_name)) != 0)
		return key_name;
	else
		return fmt::format("key_{}", keyCode);
#elif BOOST_OS_LINUX
	return gdk_keyval_name(keyCode);
#else
	return fmt::format("key_{}", keyCode);
#endif
}

std::optional<GuiSystem::PlatformKeyCodes> rawKeyCodeToPlatformKeyCode(uint32 keyCode)
{
	switch (keyCode)
	{
#if BOOST_OS_WINDOWS
		case VK_LCONTROL: return GuiSystem::PlatformKeyCodes::LCONTROL;
		case VK_RCONTROL: return GuiSystem::PlatformKeyCodes::RCONTROL;
		case VK_TAB: return GuiSystem::PlatformKeyCodes::TAB;
#elif BOOST_OS_LINUX
		case GDK_KEY_Control_L: return GuiSystem::PlatformKeyCodes::LCONTROL;
		case GDK_KEY_Control_R: return GuiSystem::PlatformKeyCodes::RCONTROL;
		case GDK_KEY_Tab: return GuiSystem::PlatformKeyCodes::TAB;
#elif BOOST_OS_MACOS
		case kVK_Control: return GuiSystem::PlatformKeyCodes::LCONTROL;
		case kVK_RightControl: return GuiSystem::PlatformKeyCodes::RCONTROL;
		case kVK_Tab: return GuiSystem::PlatformKeyCodes::TAB;
#endif
		default: return std::nullopt;
	}
}

GuiSystem::WindowHandleInfo get_window_handle_info_for_wxWindow(wxWindow* wxw)
{
	GuiSystem::WindowHandleInfo handleInfo;
#if BOOST_OS_WINDOWS
	handleInfo.backend = GuiSystem::WindowHandleInfo::Backend::WINDOWS;
	handleInfo.surface = reinterpret_cast<void*>(wxw->GetHWND());
#elif BOOST_OS_LINUX
	GtkWidget* gtkWidget = (GtkWidget*)wxw->GetHandle();  // returns GtkWidget
	gtk_widget_realize(gtkWidget);
	GdkWindow* gdkWindow = gtk_widget_get_window(gtkWidget);
	GdkDisplay* gdkDisplay = gdk_window_get_display(gdkWindow);
	if (GDK_IS_X11_WINDOW(gdkWindow))
	{
		handleInfo.backend = GuiSystem::WindowHandleInfo::Backend::X11;
		handleInfo.surface = reinterpret_cast<void*>(gdk_x11_window_get_xid(gdkWindow));
		handleInfo.display = gdk_x11_display_get_xdisplay(gdkDisplay);
		if (!handleInfo.display)
		{
			cemuLog_log(LogType::Force, "Unable to get xlib display");
		}
	}
#ifdef HAS_WAYLAND
	else if (GDK_IS_WAYLAND_WINDOW(gdkWindow))
	{
		handleInfo.backend = GuiSystem::WindowHandleInfo::Backend::WAYLAND;
		handleInfo.surface = gdk_wayland_window_get_wl_surface(gdkWindow);
		handleInfo.display = gdk_wayland_display_get_wl_display(gdkDisplay);
	}
#endif
	else
	{
		cemuLog_log(LogType::Force, "Unsuported GTK backend");
	}
#elif BOOST_OS_MACOS
	handleInfo.backend = GuiSystem::WindowHandleInfo::Backend::COCOA;
	handleInfo.surface = reinterpret_cast<void*>(wxw->GetHandle());
#endif
	return handleInfo;
}
