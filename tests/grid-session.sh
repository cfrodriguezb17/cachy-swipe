#!/bin/sh
# Real native switcher + QML layout in a private virtual KWin session.
set -eu
QT_QPA_PLATFORM=wayland QT_QPA_PLATFORMTHEME=generic "$CACHY_TEST_FIXTURE" &
fixture_pid=$!
trap 'kill "$fixture_pid" 2>/dev/null || true' EXIT
sleep 1
[ "$(qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect cachyswipe)" = true ]
debug() { qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.debug cachyswipe "$1"; }
state() { debug state | tail -n1 > "$CACHY_TEST_RUNTIME/state.json"; }
assert_index() {
    state
    python3 - "$1" <<'PY'
import json, os, sys
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
n = int(os.environ['CACHY_TEST_WINDOWS'])
assert s['shown'] and s['visible'] and s['count'] == n, s
assert s['index'] == int(sys.argv[1]), s
assert s['columns'] == 4 and s['gridNavigation'], s
assert s.get('uiColumns') == 4 and s.get('uiRows') == (n + 3) // 4, s
assert s.get('uiIndex') == s['index'], s
assert s.get('uiIconReady') and s.get('uiIconWidth', 0) > 0, s
print('grid: native and visual selection', s['index'], ': pasado')
PY
}

debug test-begin > /dev/null
debug test-move:0.6,0 > /dev/null
sleep 0.4
assert_index 1
debug test-move:0,1 > /dev/null
if [ "$CACHY_TEST_WINDOWS" = 6 ]; then assert_index 4; else assert_index 5; fi
debug test-move:-1,0 > /dev/null
assert_index 4
debug test-move:0,-1 > /dev/null
if [ "$CACHY_TEST_WINDOWS" = 6 ]; then assert_index 1; else assert_index 0; fi
debug test-move:-50,0 > /dev/null
assert_index 0
debug test-move:50,0 > /dev/null
assert_index 3
debug test-move:50,0 > /dev/null
assert_index 3

if [ "$CACHY_TEST_WINDOWS" = 6 ]; then
    debug test-move:0,1 > /dev/null
    assert_index 5
    debug test-move:0,-1 > /dev/null
    assert_index 2
    debug test-move:0,1 > /dev/null
    assert_index 5
    debug test-move:50,0 > /dev/null
    assert_index 5
    debug test-move:-0.6,0 > /dev/null
    assert_index 4
else
    debug test-move:-0.6,0 > /dev/null
    assert_index 2
    debug test-move:0,50 > /dev/null
    assert_index 10
    debug test-move:0,50 > /dev/null
    assert_index 10
    debug test-move:0,-0.6 > /dev/null
    assert_index 6
    debug test-move:0,-50 > /dev/null
    assert_index 2
    debug test-move:0,0.6 > /dev/null
    assert_index 6
fi

sleep 0.3
debug test-screenshot > /dev/null
debug test-escape > /dev/null
debug test-move:1,0 > /dev/null
state
python3 - <<'PY'
import json, os
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
assert s['gestureActive'] and not s['shown'] and not s['visible'], s
print('grid: Escape and consumed gesture tail: pasado')
PY
debug test-end > /dev/null
debug test-begin > /dev/null
debug test-move:0,0.6 > /dev/null
assert_index 4
if [ "$CACHY_TEST_WINDOWS" = 6 ]; then
    debug test-move:0,-1 > /dev/null
    assert_index 1
    debug test-move:0,1 > /dev/null
    assert_index 4
    debug test-move:1,-1 > /dev/null
    assert_index 2
    debug test-move:0,1 > /dev/null
    assert_index 5
    debug test-move:-1,-1 > /dev/null
    assert_index 1
fi
state
selected=$(python3 -c "import json,os;print(json.load(open(os.environ['CACHY_TEST_RUNTIME']+'/state.json'))['selectedCaption'])")
debug test-end > /dev/null
sleep 0.3
state
python3 - "$selected" <<'PY'
import json, os, sys
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
assert not s['gestureActive'] and not s['shown'] and not s['visible'], s
assert s['activeCaption'] == sys.argv[1], s
print('grid: vertical start and release confirm the exact selected window: pasado')
PY
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect cachyswipe
touch "$CACHY_TEST_RUNTIME/integration-passed"
