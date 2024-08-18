#include "gui/helpers/wxWayland.h"

#if BOOST_OS_LINUX && HAS_WAYLAND

#include <dlfcn.h>

bool wxWlIsWaylandWindow(wxWindow* window)
{
	GtkWidget* gtkWindow = static_cast<GtkWidget*>(window->GetHandle());
	GdkWindow* gdkWindow = gtk_widget_get_window(gtkWindow);
	return GDK_IS_WAYLAND_WINDOW(gdkWindow);
}

void wxWlSetAppId(wxFrame* frame, const char* applicationId)
{
	GtkWidget* gtkWindow = static_cast<GtkWidget*>(frame->GetHandle());
	gtk_widget_realize(gtkWindow);
	GdkWindow* gdkWindow = gtk_widget_get_window(gtkWindow);
	static auto gdk_wl_set_app_id = reinterpret_cast<void (*) (GdkWindow*, const char*)>(dlsym(nullptr, "gdk_wayland_window_set_application_id"));
	if (gdk_wl_set_app_id)
		gdk_wl_set_app_id(gdkWindow, applicationId);
}

#endif	// BOOST_OS_LINUX && HAS_WAYLAND
