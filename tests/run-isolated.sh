#!/bin/sh
set -eu
build_dir=$(realpath "${1:-build-integration}")
script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
test -f "$build_dir/bin/kwin/effects/plugins/cachyswipe.so"
test -x "$build_dir/window_fixture"
run_dir=$(mktemp -d /tmp/cachy-swipe-test.XXXXXX)
trap 'fusermount3 -u "$run_dir/runtime/gvfs" 2>/dev/null || true; rm -rf -- "$run_dir" 2>/dev/null || true' EXIT
mkdir -p "$run_dir/plugins/kwin/effects/plugins" "$run_dir/runtime" "$run_dir/config" "$run_dir/data"
chmod 700 "$run_dir/runtime"
cp "$build_dir/bin/kwin/effects/plugins/cachyswipe.so" "$run_dir/plugins/kwin/effects/plugins/"
export CACHY_TEST_FIXTURE="$build_dir/window_fixture"
export CACHY_TEST_RUNTIME="$run_dir/runtime"
if env -u DISPLAY -u WAYLAND_DISPLAY \
    XDG_RUNTIME_DIR="$run_dir/runtime" XDG_CONFIG_HOME="$run_dir/config" \
    XDG_DATA_HOME="$run_dir/data" QT_PLUGIN_PATH="$run_dir/plugins" \
    QT_QPA_PLATFORM=offscreen LIBGL_ALWAYS_SOFTWARE=1 \
    timeout 25 dbus-run-session -- kwin_wayland --virtual --no-lockscreen \
    --no-kactivities --exit-with-session "$script_dir/integration-session.sh" \
    > "$run_dir/kwin.log" 2>&1; then
    grep 'pasado' "$run_dir/kwin.log" || true
else
    cat "$run_dir/kwin.log"
    exit 1
fi
test -f "$run_dir/runtime/integration-passed"
