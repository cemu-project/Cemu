#include "gui/canvas/OpenGLCanvas.h"
#include "Cafe/HW/Latte/Renderer/OpenGL/OpenGLRenderer.h"

#include "config/CemuConfig.h"

#include "Common/GLInclude/GLInclude.h"
#include <wx/glcanvas.h> // this includes GL/gl.h, avoid using this in a header because it would contaminate our own OpenGL definitions (GLInclude)

static const int g_gl_attribute_list[] =
{
	WX_GL_RGBA,
	WX_GL_DOUBLEBUFFER,
	WX_GL_DEPTH_SIZE, 16,

	WX_GL_MIN_RED, 8,
	WX_GL_MIN_GREEN, 8,
	WX_GL_MIN_BLUE, 8,
	WX_GL_MIN_ALPHA, 8,

	WX_GL_STENCIL_SIZE, 8,

	//WX_GL_MAJOR_VERSION, 4,
	//WX_GL_MINOR_VERSION, 1,
	//wx_GL_COMPAT_PROFILE,

	0, // end of list
};

wxGLContext* sGLContext = nullptr;
class OpenGLCanvas* sGLTVView = nullptr;
class OpenGLCanvas* sGLPadView = nullptr;

class OpenGLCanvas : public IRenderCanvas, public wxGLCanvas
{
public:
	OpenGLCanvas(wxWindow* parent, const wxSize& size, bool is_main_window)
		: IRenderCanvas(is_main_window), wxGLCanvas(parent, wxID_ANY, g_gl_attribute_list, wxDefaultPosition, size, wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS)
	{
		if (m_is_main_window)
		{
			sGLTVView = this;
			sGLContext = new wxGLContext(this);

			g_renderer = std::make_unique<OpenGLRenderer>();
		}
		else
			sGLPadView = this;

		wxWindow::EnableTouchEvents(wxTOUCH_PAN_GESTURES);
	}

	~OpenGLCanvas() override
	{
		// todo - if this is the main window, make sure the renderer has been shut down

		if (m_is_main_window)
			sGLTVView = nullptr;
		else
			sGLPadView = nullptr;

		if (sGLTVView == nullptr && sGLPadView == nullptr && sGLContext)
			delete sGLContext;
	}

	void UpdateVSyncState()
	{
		int configValue = GetConfig().vsync.GetValue();
		if(m_activeVSyncState != configValue)
		{
#if BOOST_OS_WINDOWS
			if(wglSwapIntervalEXT)
				wglSwapIntervalEXT(configValue); // 1 = enabled, 0 = disabled
#elif BOOST_OS_LINUX
			if (eglSwapInterval)
			{
				if (eglSwapInterval(eglGetCurrentDisplay(), configValue) == EGL_FALSE)
				{
					cemuLog_log(LogType::Force, "Failed to set vsync using EGL");
				}
			}
#else
			cemuLog_log(LogType::Force, "OpenGL vsync not implemented");
#endif
			m_activeVSyncState = configValue;
		}
	}

private:
	int m_activeVSyncState = -1;
	//wxGLContext* m_context = nullptr;
};

wxWindow* GLCanvas_Create(wxWindow* parent, const wxSize& size, bool is_main_window)
{
	return new OpenGLCanvas(parent, size, is_main_window);
}

bool GLCanvas_HasPadViewOpen()
{
	return sGLPadView != nullptr;
}

bool GLCanvas_MakeCurrent(bool padView)
{
	OpenGLCanvas* canvas = padView ? sGLPadView : sGLTVView;
	if (!canvas)
		return false;
	sGLContext->SetCurrent(*canvas);
	return true;
}

void GLCanvas_SwapBuffers(bool swapTV, bool swapDRC)
{
	if (swapTV && sGLTVView)
	{
		GLCanvas_MakeCurrent(false);
		sGLTVView->SwapBuffers();
		sGLTVView->UpdateVSyncState();
	}
	if (swapDRC && sGLPadView)
	{
		GLCanvas_MakeCurrent(true);
		sGLPadView->SwapBuffers();
		sGLPadView->UpdateVSyncState();
	}

	GLCanvas_MakeCurrent(false);
}
