#pragma once

#include <QTimer>
#include <QWindow>

#include "MobileBackend.h"

// Top-level window the emulator renders into with its own Vulkan swapchain.
// Qt only provides the native surface and the input events.
class GameWindow : public QWindow
{
	Q_OBJECT
  public:
	explicit GameWindow(QWindow* parent = nullptr);

	// native handles + current size. Only valid once the window is shown
	MobileBackend::NativeWindow nativeWindow();

  Q_SIGNALS:
	// emitted once, when the window is mapped and can be rendered to
	void ready();
	// the user (or the compositor) closed the window
	void closed();

  protected:
	bool event(QEvent* event) override;
	void exposeEvent(QExposeEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;
	void keyReleaseEvent(QKeyEvent* event) override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void touchEvent(QTouchEvent* event) override;
	void focusInEvent(QFocusEvent* event) override;
	void focusOutEvent(QFocusEvent* event) override;

  private:
	void notifyReady();
	void pushSize();
	void pushMouse(QMouseEvent* event);

	bool m_readyEmitted = false;
	QTimer m_readyFallback;
};
