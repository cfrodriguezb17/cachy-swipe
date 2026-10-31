#!/bin/sh
set -eu
build_dir=$(realpath "${1:-build-integration}")
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
session_script="$script_dir/integration-session.sh"
if [ "${2:-}" = grid ]; then
    session_script="$script_dir/grid-session.sh"
    export CACHY_TEST_WINDOWS="${3:-12}"
fi
test -f "$build_dir/bin/kwin/effects/plugins/cachyswipe.so"
test -x "$build_dir/window_fixture"
run_dir=$(mktemp -d /tmp/cachy-swipe-test.XXXXXX)
trap 'fusermount3 -u "$run_dir/runtime/gvfs" 2>/dev/null || true; rm -rf -- "$run_dir" 2>/dev/null || true' EXIT
mkdir -p "$run_dir/plugins/kwin/effects/plugins" "$run_dir/runtime" "$run_dir/config" "$run_dir/data"
chmod 700 "$run_dir/runtime"
cp "$build_dir/bin/kwin/effects/plugins/cachyswipe.so" "$run_dir/plugins/kwin/effects/plugins/"
mkdir -p "$run_dir/data/kwin/tabbox"
cp -r "$script_dir/../tabbox/cachy-swipe-grid" "$run_dir/data/kwin/tabbox/"
printf '[TabBox]\nLayoutName=cachy-swipe-grid\n' > "$run_dir/config/kwinrc"
export CACHY_TEST_FIXTURE="$build_dir/window_fixture"
export CACHY_TEST_RUNTIME="$run_dir/runtime"
if env -u DISPLAY -u WAYLAND_DISPLAY \
    XDG_RUNTIME_DIR="$run_dir/runtime" XDG_CONFIG_HOME="$run_dir/config" \
    XDG_DATA_HOME="$run_dir/data" QT_PLUGIN_PATH="$run_dir/plugins" \
    QT_QPA_PLATFORM=offscreen QT_QPA_PLATFORMTHEME=kde LIBGL_ALWAYS_SOFTWARE=1 \
    timeout 25 dbus-run-session -- kwin_wayland --virtual --no-lockscreen \
    --no-kactivities --exit-with-session "$session_script" \
    > "$run_dir/kwin.log" 2>&1; then
    grep 'pasado' "$run_dir/kwin.log" || true
else
    cat "$run_dir/kwin.log"
    exit 1
fi
test -f "$run_dir/runtime/integration-passed"
if [ -n "${CACHY_TEST_SCREENSHOT:-}" ]; then
    cp "$run_dir/runtime/grid.png" "$CACHY_TEST_SCREENSHOT"
fi
if grep -E 'main.qml:.*(Error|not defined|Cannot|Unable)|Type .* unavailable|is not a type' "$run_dir/kwin.log"; then
    exit 1
fi
