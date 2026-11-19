# Qt mobile interface

Touch-friendly Qt 6 / QML front end for small screens (Plasma Mobile and similar).
It lives next to the wxWidgets interface and is started with `Cemu --mobile-ui`
(or `CEMU_MOBILE_UI=1`). Build it with `-DENABLE_QT_MOBILE_UI=ON`.

What it does today:

- game list (grid with icons), refresh, launch
- game folder management
- controllers: pick the SDL controller for player 1, default mapping, live button test
- full screen Vulkan render window with keyboard, mouse and touch forwarding

Everything else (custom input mapping, graphics/audio settings, graphic packs, title manager,
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

## PGO (profile-guided optimization)

1. Run the workflow manually (Actions › Run workflow) with `pgo = generate`, install the package.
2. On the phone: `LLVM_PROFILE_FILE="$HOME/cemu-%m.profraw" Cemu --mobile-ui`, play a few minutes with several games, quit the game normally.
3. `sudo pacman -S llvm`, then `llvm-profdata merge -output=cemu.profdata ~/cemu-*.profraw`.
4. Commit `cemu.profdata` to `dist/linux/arch/`, run the workflow with `pgo = use`.

## Built-in frame profiler

`CEMU_PERF_LOG=1 Cemu --mobile-ui` prints one line per second while a game runs:

```
PERF fps=16.2 frame=61.7ms | latte: waiting-for-guest=35.1ms fence=4.2ms async=0.3ms working=22.1ms | guest cores idle%: c0=62 c1=57 c2=60 | ...
```

- `waiting-for-guest`: the GPU command thread has nothing to do (the game thread is the bottleneck)
- `fence`: it waits for a condition in memory
- `working`: it is really processing draw calls (Cemu + Vulkan driver)
- `guest cores idle%`: share of the second a PPC core had no runnable thread
