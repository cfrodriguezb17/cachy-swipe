# Cachy Swipe — Linux touchpad gestures for KDE Plasma Wayland

**Cachy Swipe is a native KWin effect that lets you switch between open windows with three-finger touchpad swipes on Linux.** It uses KDE Plasma's existing Alt+Tab window switcher, with distance-based selection and hard stops at both ends. It is designed for CachyOS and Arch Linux running **KWin 6.8.x on Wayland**.

Version **0.3.2** · [MIT license](LICENSE) · [FAQ](#frequently-asked-questions) · [Documentation index](llms.txt)

## Project facts

| Property | Value |
| --- | --- |
| Project type | Native KWin desktop effect written in C++20 |
| Purpose | Switch between open windows using touchpad gestures |
| Supported desktop | KDE Plasma on Wayland, KWin 6.8.x |
| Documented distributions | CachyOS and Arch Linux |
| Default gesture | Horizontal three-finger swipe; four fingers optional |
| Default sensitivity | `SwipeDistance=120`; lower values require less movement |
| Window switcher | KDE's native Alt+Tab / TabBox, without simulated keyboard input |
| License | MIT |

## How to switch windows with touchpad gestures


- Swipe horizontally with **three fingers** to open the switcher and move through windows.
- Selection follows the distance traveled; release your fingers to activate the highlighted window.
- The first and last windows are hard stops: no wrapping or accumulated overscroll.
- A small stability margin prevents selection changes from finger jitter.
- Escape or a cancelled gesture closes the switcher without changing windows.
- Vertical gestures do not open the switcher. Two-finger scrolling is unaffected.
- Window order comes from the native switcher. Changes to the window list, focus, desktop or activity cancel the selection.

## Linux and KWin compatibility

**KDE Plasma on Wayland, KWin 6.8.x, Qt 6, CMake 3.25+, ECM and a C++20 compiler.**

The effect uses private KWin TabBox symbols. Other KWin minor versions require a compatibility review; rebuild after upgrading KWin. This is a KWin effect, not a standalone application or an X11 gesture daemon.

## Install on CachyOS or Arch Linux

On CachyOS / Arch Linux, from the project directory:

```sh
sudo pacman -S --needed base-devel cmake ninja extra-cmake-modules kwin qt6-base qt6-declarative qt6-tools
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DCACHY_SWIPE_INTEGRATION_TESTING=OFF
cmake --build build
ctest --test-dir build --output-on-failure
sudo cmake --install build
```

Enable **Cachy Swipe** in **System Settings → Window Management → Desktop Effects**, or run:

```sh
kwriteconfig6 --file kwinrc --group Plugins --key cachyswipeEnabled true
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect cachyswipe
```

If replacing an already loaded version, log out and back in after installation.

## Configure touchpad swipe sensitivity

Settings are stored in `~/.config/kwinrc`, under `[Effect-cachyswipe]`:

| Key | Default | Meaning |
| --- | --- | --- |
| `SwipeDistance` | `120` | Travel per window, in KWin gesture units. Lower is more sensitive. Range: 80–4000. |
| `FingerCount` | `3` | Use 3 or 4 fingers. |
| `ReverseDirection` | `true` | Positive horizontal motion advances through the list; `false` reverses it. |

Existing settings override these defaults. Non-finite distances fall back to 120; finite distances outside the range are clamped.

For example, set the distance to 150 and reload the effect:

```sh
kwriteconfig6 --file kwinrc --group Effect-cachyswipe --key SwipeDistance 150
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.reconfigureEffect cachyswipe
```

Inspect the effective settings with:

```sh
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.debug cachyswipe state
```

## Disable or uninstall

```sh
kwriteconfig6 --file kwinrc --group Plugins --key cachyswipeEnabled false
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect cachyswipe
```

To uninstall, also remove the plugin file listed in `build/install_manifest.txt`. The default `/usr` installation uses `/usr/lib/qt6/plugins/kwin/effects/plugins/cachyswipe.so`.

## Frequently asked questions

### Can I switch between open windows with a three-finger swipe on Linux?

Yes. On KDE Plasma Wayland with KWin 6.8.x, Cachy Swipe opens the native window switcher when a three-finger gesture becomes clearly horizontal. Continue swiping to change the highlighted window, then lift your fingers to activate it.

### Does this work like macOS or Windows touchpad gestures?

Cachy Swipe provides gesture-driven window switching with proportional travel and end stops. It keeps KDE's existing switcher and animation. It switches open windows rather than virtual desktops; it does not reproduce either operating system's full gesture system.

### Is this a Touchégg replacement for KDE Wayland?

Cachy Swipe handles one specific task: three- or four-finger horizontal swipes for window switching in KWin. It is a native compositor effect, not a general-purpose gesture daemon or a replacement for every Touchégg action.

### How do I make the swipe more or less sensitive?

Change `SwipeDistance` in the `[Effect-cachyswipe]` section of `~/.config/kwinrc`, then reload the effect with `reconfigureEffect cachyswipe` as shown above. The default is **120**. A smaller value increases sensitivity; a larger value requires more movement. Supported values range from 80 to 4000 KWin gesture units.

### Does it work on GNOME, Hyprland or X11?

No. The supported environment is KDE Plasma Wayland with KWin 6.8.x. Other KWin minor versions require reviewing the private TabBox interface before support can be claimed.

### Why can a swipe stop moving the selection?

The first and last items are intentional hard stops. Swiping outward from either end leaves that item selected. Reverse direction to move back through the list; the effect never wraps around to the opposite end.

### Does it change two-finger scrolling?

No. Cachy Swipe handles three- or four-finger window-switching gestures and does not change two-finger scroll speed, smoothing or inertia.

## Development and tests

The effect controls the native TabBox directly, without synthesizing Alt+Tab keys. Gesture math is kept separate from KWin integration.

Run gesture tests without KDE dependencies:

```sh
cmake -S . -B build-tests -G Ninja -DBUILD_KWIN_PLUGIN=OFF
cmake --build build-tests
ctest --test-dir build-tests --output-on-failure
```

Run integration checks with a virtual KWin session (also requires Python 3 and `dbus-run-session`):

```sh
cmake -S . -B build-integration -G Ninja -DCMAKE_BUILD_TYPE=Release -DCACHY_SWIPE_INTEGRATION_TESTING=ON
cmake --build build-integration
./tests/run-isolated.sh build-integration
```

Checks cover defaults, configuration overrides, selection, confirmation, cancellation, jitter, direction changes and hard stops. Integration uses temporary configuration and a private session bus. **Do not install the integration build:** synthetic gesture commands are enabled only in that build.

## Publishing

See the [GitHub publishing guide](docs/PUBLISHING.md) for the repository description, suggested topics and search-discovery guidance.
