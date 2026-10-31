# Cachy Swipe — Linux touchpad gestures for KDE Plasma Wayland

**Cachy Swipe is a native KWin effect that lets you switch between open windows with three-finger touchpad swipes on Linux.** It uses KDE Plasma's native Alt+Tab window switcher with a four-column grid with the classic large-icon appearance, distance-based selection and hard stops. It is designed for CachyOS and Arch Linux running **KWin 6.8.x on Wayland**.

Version **0.4.0** · [MIT license](LICENSE) · [FAQ](#frequently-asked-questions) · [Documentation index](llms.txt)

## Project facts

| Property | Value |
| --- | --- |
| Project type | Native KWin desktop effect written in C++20 |
| Purpose | Switch between open windows using touchpad gestures |
| Supported desktop | KDE Plasma on Wayland, KWin 6.8.x |
| Documented distributions | CachyOS and Arch Linux |
| Default gesture | Three-finger swipes: left/right for columns, up/down for rows; four fingers optional |
| Default sensitivity | `SwipeDistance=120`; lower values require less movement |
| Window switcher | KDE's native Alt+Tab / TabBox, without simulated keyboard input |
| License | MIT |

## How to switch windows with touchpad gestures


- Swipe with **three fingers** to open the native switcher: **left/right** moves between columns, **up/down** between rows.
- The included **Cachy Swipe Grid** layout shows up to **four windows per row**, so 12 windows form a 4 × 3 grid. Continue in either direction without lifting your fingers.
- The final row is centered when incomplete. Vertical movement selects the nearest window along the actual horizontal trace, including centered and incomplete rows. After the switcher opens, diagonal movement follows both axes.
- Selection follows the distance traveled; release your fingers to activate the highlighted window.
- Every row and column has hard stops: no wrapping or accumulated overscroll.
- A small stability margin prevents selection changes from finger jitter.
- Escape or a cancelled gesture closes the switcher without changing windows.
- The initial movement waits for a clear direction. Two-finger scrolling is unaffected.
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
kwriteconfig6 --file kwinrc --group TabBox --key LayoutName cachy-swipe-grid
kwriteconfig6 --file kwinrc --group Effect-cachyswipe --key GridNavigation true
kwriteconfig6 --file kwinrc --group Plugins --key cachyswipeEnabled true
qdbus6 org.kde.KWin /KWin org.kde.KWin.reconfigure
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect cachyswipe
```

If replacing an already loaded version, log out and back in after installation.

## Automatic rebuild after system updates

On CachyOS or Arch Linux with Plasma's systemd-managed Wayland session, run
this once **as your desktop user, without sudo** (also requires Python 3):

```sh
python3 scripts/auto_rebuild.py install
```

**Log out and back in once after setup.** The installer adds a KWin service
override so Qt discovers the per-user plugin directory. It does not restart
KWin or interrupt your desktop. It enables Cachy Swipe without changing gesture
sensitivity or direction. It installs and selects **Cachy Swipe Grid** for the
normal Alt+Tab switcher as well as gestures; the previous layout name is saved
in `~/.local/share/cachy-swipe/previous-layout`.

A user timer checks installed KWin, Qt and KDE dependency versions every minute.
When dependencies or the installed source snapshot change, it builds a fresh
production effect and runs its tests. Only a successful build replaces the
plugin, atomically, under `~/.local/share/cachy-swipe/plugins`. No root access,
passwordless sudo or package-manager hook is needed. Source is copied into
`~/.local/share/cachy-swipe/source`, so the original checkout can be moved.
This rebuilds the local snapshot; it does not download newer project code.
Run the installer again from an updated checkout to refresh that snapshot.

If an upgrade leaves the old KWin process running, activation waits until the
running version matches the installed version, normally at your next login.
An effect you disable explicitly stays disabled. Incompatible KWin minor
versions are still rejected by CMake: automation does not bypass compatibility
checks. Build failures preserve the previous plugin, issue a notification if
`notify-send` is available, and retry after an hour. The previous plugin may
itself be incompatible with an updated KWin until the build issue is resolved.

Inspect or manually retry the service:

```sh
systemctl --user status cachy-swipe-rebuild.timer
journalctl --user -u cachy-swipe-rebuild.service
rm -f ~/.local/share/cachy-swipe/failure.json
systemctl --user start cachy-swipe-rebuild.service
```

To remove automatic rebuilding and the per-user installation:

```sh
systemctl --user disable --now cachy-swipe-rebuild.timer
systemctl --user stop cachy-swipe-rebuild.service
rm -f ~/.config/systemd/user/cachy-swipe-rebuild.service ~/.config/systemd/user/cachy-swipe-rebuild.timer
rm -f ~/.config/systemd/user/plasma-kwin_wayland.service.d/90-cachy-swipe.conf
kwriteconfig6 --file kwinrc --group TabBox --key LayoutName "$(cat ~/.local/share/cachy-swipe/previous-layout)"
kwriteconfig6 --file kwinrc --group Effect-cachyswipe --key GridNavigation false
rm -rf ~/.local/share/cachy-swipe
rm -rf ~/.local/share/kwin/tabbox/cachy-swipe-grid
systemctl --user daemon-reload
```

Log out and back in afterward. If using a custom `XDG_CONFIG_HOME`, substitute
that directory for `~/.config`. A previous system-wide installation is separate.

## Configure touchpad swipe sensitivity

Settings are stored in `~/.config/kwinrc`, under `[Effect-cachyswipe]`:

| Key | Default | Meaning |
| --- | --- | --- |
| `SwipeDistance` | `120` | Travel per window, in KWin gesture units. Lower is more sensitive. Range: 80–4000. |
| `FingerCount` | `3` | Use 3 or 4 fingers. |
| `ReverseDirection` | `true` | Positive horizontal motion moves right; `false` reverses horizontal movement. Up/down are unaffected. |
| `GridNavigation` | `true` | Navigate the four-column Cachy Swipe Grid in two dimensions. Set `false` for the previous horizontal list behavior. |

Select **Cachy Swipe Grid** under **System Settings → Window Management → Task Switcher** when using grid navigation. This layout also applies to keyboard Alt+Tab.
Choosing a different layout automatically restores horizontal list navigation,
so gesture columns never disagree with another layout's visual arrangement.

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

To uninstall, also remove the plugin file listed in `build/install_manifest.txt`. The default `/usr` installation uses `/usr/lib/qt6/plugins/kwin/effects/plugins/cachyswipe.so` and `/usr/share/kwin/tabbox/cachy-swipe-grid/`. Select another task-switcher layout before removing the grid package.

## Frequently asked questions

### Can I switch between open windows with a three-finger swipe on Linux?

Yes. On KDE Plasma Wayland with KWin 6.8.x, Cachy Swipe opens the native window switcher when a three-finger gesture becomes clearly horizontal or vertical. Move across up to four columns or between rows to change the highlighted window, then lift your fingers to activate it.

### Does this work like macOS or Windows touchpad gestures?

Cachy Swipe provides gesture-driven window switching with proportional travel and end stops. It uses KDE's native switcher with the included four-column layout. It switches open windows rather than virtual desktops; it does not reproduce either operating system's full gesture system.

### Is this a Touchégg replacement for KDE Wayland?

Cachy Swipe handles one specific task: three- or four-finger swipes for window switching in KWin. It is a native compositor effect, not a general-purpose gesture daemon or a replacement for every Touchégg action.

### How do I make the swipe more or less sensitive?

Change `SwipeDistance` in the `[Effect-cachyswipe]` section of `~/.config/kwinrc`, then reload the effect with `reconfigureEffect cachyswipe` as shown above. The default is **120**. A smaller value increases sensitivity; a larger value requires more movement. Supported values range from 80 to 4000 KWin gesture units.

### Does it work on GNOME, Hyprland or X11?

No. The supported environment is KDE Plasma Wayland with KWin 6.8.x. Other KWin minor versions require reviewing the private TabBox interface before support can be claimed.

### Why can a swipe stop moving the selection?

Each row has left/right stops, and the grid has top/bottom stops. At a row edge, swipe up or down to select another row. Reverse direction to leave an edge immediately; the gesture does not wrap to another row or select empty cells.

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

Test automatic upgrade handling without modifying your session:

```sh
python3 tests/auto_rebuild_test.py
```

Run integration checks with a virtual KWin session (also requires Python 3 and `dbus-run-session`):

```sh
cmake -S . -B build-integration -G Ninja -DCMAKE_BUILD_TYPE=Release -DCACHY_SWIPE_INTEGRATION_TESTING=ON
cmake --build build-integration
./tests/run-isolated.sh build-integration
./tests/run-isolated.sh build-integration grid 12
./tests/run-isolated.sh build-integration grid 6
```

Checks cover defaults, configuration overrides, selection, confirmation, cancellation, jitter, two-axis navigation, incomplete rows and hard stops. Grid integration verifies that the actual QML columns, rows and highlighted index match the native selection. Integration uses temporary configuration and a private session bus. **Do not install the integration build:** synthetic gesture commands are enabled only in that build.

## Publishing

See the [GitHub publishing guide](docs/PUBLISHING.md) for the repository description, suggested topics and search-discovery guidance.
