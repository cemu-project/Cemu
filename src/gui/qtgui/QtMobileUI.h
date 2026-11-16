#pragma once

// Entry point of the touch-friendly Qt/QML interface ("mobile UI").
// This header is intentionally free of Qt and Cemu types so it can be included from anywhere.
namespace QtMobileUI
{
	// Runs the Qt event loop. Blocks until the user quits. Core init (config, title list, Vulkan) must be done before.
	int Run();
}
