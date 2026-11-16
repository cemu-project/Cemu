# Qt mobile interface

Touch-friendly Qt 6 / QML front end for small screens (Plasma Mobile and similar).
It lives next to the wxWidgets interface and is started with `Cemu --mobile-ui`
(or `CEMU_MOBILE_UI=1`). Build it with `-DENABLE_QT_MOBILE_UI=ON`.

What it does today:

- game list (grid with icons), refresh, launch
- game folder management
- full screen Vulkan render window with keyboard, mouse and touch forwarding

Everything else (input mapping, graphics/audio settings, graphic packs, title manager,
debugger, ...) is still only available in the desktop interface; both share the same
configuration files.

## Layout

| File | Role |
| --- | --- |
| `MobileBackend.*` | Bridge to the emulator core. Standard C++ types only, no Qt. |
| `QtMobileUI.*` | Entry point, creates the Qt application and the QML engine. |
| `Controller.*` | Object exposed to QML as `cemu`. |
| `GameListModel.*` | List model behind the game grid. |
| `GameWindow.*` | Native window the emulator renders into. |
| `qml/` | The pages. |

The Qt code never includes Cemu headers and the bridge never includes Qt headers:
mixing them in one translation unit clashes (X11/GTK macros, integer typedefs, `signals`/`slots`).

Leaving a game restarts the process to get back to the list, because stopping a title
and starting another one in the same process is not reliable in the core.

## Menus available in the mobile interface

- **⋮ › Settings**: console language, screen saver, intro sound, VSync, asynchronous shader compilation,
  scaling and filters, audio output device / volume / channels / latency, overlay and notifications.
- **Long press on a game**: start, graphic packs of that game (enable + presets), remove shader caches.

Everything is stored in the same `settings.xml` as the desktop interface.
