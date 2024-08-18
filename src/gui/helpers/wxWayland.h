#pragma once

#if BOOST_OS_LINUX && HAS_WAYLAND

#include <gdk/gdk.h>
#include <gdk/gdkwayland.h>
#include <gtk/gtk.h>
#include <wayland-client.h>
#include <wx/wx.h>
#include "wayland-viewporter-client-protocol.h"

class wxWlSubsurface
{
	static void registry_add_object(void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version)
	{
		auto wlSubsurface = static_cast<wxWlSubsurface*>(data);

		if (!strcmp(interface, wl_subcompositor_interface.name))
		{
			wlSubsurface->m_subcompositor = static_cast<wl_subcompositor*>(wl_registry_bind(registry, name, &wl_subcompositor_interface, 1));
		}
		else if (!strcmp(interface, wp_viewporter_interface.name))
		{
			wlSubsurface->m_viewporter = static_cast<wp_viewporter*>(wl_registry_bind(registry, name, &wp_viewporter_interface, 1));
		}
	}
	static void registry_remove_object(void* /*data*/, struct wl_registry* /*registry*/, uint32_t /*name*/) {}

	wl_registry_listener m_registry_listener = {&registry_add_object, &registry_remove_object};
	wl_subcompositor* m_subcompositor;
	wl_surface* m_surface;
	wl_subsurface* m_subsurface;
	wp_viewporter* m_viewporter = NULL;
	wp_viewport* m_viewport = NULL;
	int32_t m_xPos = 0;
	int32_t m_yPos = 0;
	int32_t m_width = 0;
	int32_t m_height = 0;

public:
	wxWlSubsurface(wxWindow* window)
	{
		GtkWidget* widget = static_cast<GtkWidget*>(window->GetHandle());
		gtk_widget_realize(widget);
		GdkWindow* gdkWindow = gtk_widget_get_window(widget);
		GdkDisplay* gdkDisplay = gdk_window_get_display(gdkWindow);
		wl_display* display = gdk_wayland_display_get_wl_display(gdkDisplay);
		wl_surface* surface = gdk_wayland_window_get_wl_surface(gdkWindow);
		wl_compositor* compositor = gdk_wayland_display_get_wl_compositor(gdkDisplay);
		wl_registry* registry = wl_display_get_registry(display);
		wl_registry_add_listener(registry, &m_registry_listener, this);
		wl_display_roundtrip(display);
		m_surface = wl_compositor_create_surface(compositor);
		if (m_viewporter)
			m_viewport = wp_viewporter_get_viewport(m_viewporter, m_surface);
		wl_region* region = wl_compositor_create_region(compositor);
		wl_surface_set_input_region(m_surface, region);
		m_subsurface = wl_subcompositor_get_subsurface(m_subcompositor, m_surface, surface);
		wl_subsurface_set_desync(m_subsurface);
		auto sRect = window->GetScreenRect();
		setSize(sRect.x, sRect.y, sRect.width, sRect.height);
		wl_region_destroy(region);
	}

	wl_surface* getSurface() const { return m_surface; }

	void setSize(int32_t xPos, int32_t yPos, int32_t width, int32_t height)
	{
		if (xPos != m_xPos || m_yPos != yPos || m_width != width || m_height != height)
		{
			m_xPos = xPos;
			m_yPos = yPos;
			m_height = height;
			m_width = width;
			wl_subsurface_set_position(m_subsurface, m_xPos, m_yPos);
			if (m_viewport)
				wp_viewport_set_destination(m_viewport, m_width, m_height);
			wl_surface_commit(m_surface);
		}
	}

	~wxWlSubsurface()
	{
		wl_subsurface_destroy(m_subsurface);
		wl_surface_destroy(m_surface);
		wl_subcompositor_destroy(m_subcompositor);
	}
};

bool wxWlIsWaylandWindow(wxWindow* window);
void wxWlSetAppId(wxFrame* frame, const char* application_id);

#endif	// BOOST_OS_LINUX && HAS_WAYLAND
