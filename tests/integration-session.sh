#!/bin/sh
# Prueba de integración en una sesión KWin aislada (virtual + dbus privado).
# Valida que el gesto maneja el conmutador nativo sin sintetizar teclas:
# mostrarlo, avanzar/retroceder proporcionalmente, topes en los extremos,
# confirmar al soltar y restaurar al cancelar.
set -eu

QT_QPA_PLATFORM=wayland QT_QPA_PLATFORMTHEME=generic "$CACHY_TEST_FIXTURE" &
fixture_pid=$!
trap 'kill "$fixture_pid" 2>/dev/null || true' EXIT
sleep 1

[ "$(qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect cachyswipe)" = true ]
debug() { qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.debug cachyswipe "$1"; }
state() { debug state | tail -n1 > "$CACHY_TEST_RUNTIME/state.json"; }
read_state() { python3 -c "import json,os;print(json.load(open(os.environ['CACHY_TEST_RUNTIME']+'/state.json'))['$1'])"; }

# --- Estado inicial -------------------------------------------------------
state
python3 - <<'PY'
import json, os
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
assert not s['gestureActive'] and not s['shown'] and not s['visible'], s
assert s['count'] == 0 and not s['attached'], s
assert s['distance'] == 120 and s['fingers'] == 3 and s['reverse'] is True, s
print('idle: sin conmutador ni gesto: pasado')
print('configuración limpia: distancia 120, tres dedos y dirección invertida: pasado')
PY
origin=$(read_state activeCaption)

# --- Un temblor pequeño no muestra nada -----------------------------------
debug test-begin > /dev/null
debug test-wobble > /dev/null
state
python3 - <<'PY'
import json, os
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
assert s['gestureActive'] and not s['shown'] and not s['visible'] and not s['horizontal'], s
print('temblor: nada visible: pasado')
PY

# --- Un gesto vertical no muestra nada ------------------------------------
debug test-vertical > /dev/null
state
python3 - <<'PY'
import json, os
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
assert s['gestureActive'] and not s['shown'] and not s['horizontal'], s
print('vertical: descartado sin UI: pasado')
PY
debug test-cancel > /dev/null

# --- Deslizamiento horizontal: aparece el conmutador nativo ---------------
debug test-begin > /dev/null
debug test-next > /dev/null
state
python3 - <<'PY'
import json, os
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
assert s['gestureActive'] and s['shown'] and s['visible'] and s['attached'], s
expected = 1 if s['base'] < s['count'] - 1 else 0
assert s['steps'] == expected, (s, expected)
open(os.environ['CACHY_TEST_RUNTIME'] + '/forward-step.txt', 'w').write(str(expected))
print('swipe: conmutador nativo visible, paso =', expected, ': pasado')
PY
forward=$(cat "$CACHY_TEST_RUNTIME/forward-step.txt")
selected=$(read_state selectedCaption)

# --- Soltar confirma la selección -----------------------------------------
debug test-end > /dev/null
sleep 0.5
state
python3 - "$origin" "$forward" "$selected" <<'PY'
import json, os, sys
origin, forward = sys.argv[1], int(sys.argv[2])
selected = sys.argv[3]
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
assert not s['gestureActive'] and not s['shown'] and not s['visible'], s
assert s['activeCaption'] == selected, (s, selected)
if forward:
    assert s['activeCaption'] != origin, (s, origin)
    print('soltar: confirmó la ventana resaltada:', s['activeCaption'], ': pasado')
else:
    print('soltar: sin paso posible (borde): pasado')
PY

# --- Cancelar restaura la ventana original --------------------------------
origin2=$(read_state activeCaption)
debug test-begin > /dev/null
debug test-next > /dev/null
state
python3 - <<'PY'
import json, os
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
assert s['shown'] and s['base'] == 0, s
print('nuevo gesto: lista MRU reconstruida tras el cambio de foco: pasado')
PY
debug test-cancel > /dev/null
sleep 0.5
state
python3 - "$origin2" <<'PY'
import json, os, sys
origin = sys.argv[1]
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
assert not s['gestureActive'] and not s['shown'] and not s['visible'], s
assert s['activeCaption'] == origin, (s, origin)
print('cancelar: ventana original intacta: pasado')
PY

# --- Topes duros en ambos extremos ----------------------------------------
debug test-begin > /dev/null
i=0
while [ "$i" -lt 15 ]; do debug test-next > /dev/null; i=$((i + 1)); done
state
python3 - <<'PY'
import json, os
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
assert s['steps'] == s['count'] - 1 - s['base'], s
print('tope final: la última ventana es parada dura: pasado')
PY
debug test-prev > /dev/null
state
python3 - <<'PY'
import json, os
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
assert s['index'] == s['count'] - 2, s
print('inversión desde el tope final sin recorrido oculto: pasado')
PY
i=0
while [ "$i" -lt 30 ]; do debug test-prev > /dev/null; i=$((i + 1)); done
state
python3 - <<'PY'
import json, os
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
assert s['steps'] == -s['base'], s
print('tope inicial: la primera ventana es parada dura: pasado')
PY
debug test-next > /dev/null
state
python3 - <<'PY'
import json, os
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
assert s['index'] == 1, s
print('inversión desde el tope inicial sin recorrido oculto: pasado')
PY
debug test-cancel > /dev/null
sleep 0.5
state
python3 - "$origin2" <<'PY'
import json, os, sys
origin = sys.argv[1]
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
assert s['activeCaption'] == origin, (s, origin)
print('topes cancelados sin cambiar de ventana: pasado')
PY

# --- Escape recorre el filtro nativo de teclado, no cierra por un atajo -----
debug test-begin > /dev/null
debug test-next > /dev/null
debug test-escape > /dev/null
state
python3 - "$origin2" <<'PY'
import json, os, sys
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
assert s['gestureActive'] and not s['shown'] and not s['visible'], s
assert s['activeCaption'] == sys.argv[1], s
print('Escape capturado por KWin cancela sin cambiar de ventana: pasado')
PY
debug test-next > /dev/null
state
python3 - <<'PY'
import json, os
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
assert s['gestureActive'] and not s['shown'], s
print('cola del gesto cancelado consumida sin reabrir el selector: pasado')
PY
debug test-end > /dev/null
state
python3 - <<'PY'
import json, os
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
assert not s['gestureActive'] and not s['shown'], s
print('estado limpio tras terminar el gesto: pasado')
PY

# --- Los ajustes explícitos prevalecen sobre los predeterminados -----------
kwriteconfig6 --file kwinrc --group Effect-cachyswipe --key SwipeDistance 240
kwriteconfig6 --file kwinrc --group Effect-cachyswipe --key FingerCount 4
kwriteconfig6 --file kwinrc --group Effect-cachyswipe --key ReverseDirection false
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.reconfigureEffect cachyswipe
state
python3 - <<'PY'
import json, os
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
assert s['distance'] == 240 and s['fingers'] == 4 and s['reverse'] is False, s
print('configuración explícita: distancia 240, cuatro dedos y dirección contraria: pasado')
PY
debug test-begin > /dev/null
debug test-next > /dev/null
state
python3 - <<'PY'
import json, os
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
assert s['shown'] and s['steps'] == 1, s
print('avance sintético con dirección contraria: pasado')
PY
debug test-prev > /dev/null
state
python3 - <<'PY'
import json, os
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
assert s['shown'] and s['steps'] == 0, s
print('retroceso sintético con dirección contraria: pasado')
PY
debug test-cancel > /dev/null

# --- Recuperación ante distancias no finitas y límites del rango -----------
for distance in nan 1 9999; do
    kwriteconfig6 --file kwinrc --group Effect-cachyswipe --key SwipeDistance "$distance"
    qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.reconfigureEffect cachyswipe
    state
    python3 - "$distance" <<'PY'
import json, os, sys
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
expected = {'nan': 120, '1': 80, '9999': 4000}[sys.argv[1]]
assert s['distance'] == expected, s
print('distancia', sys.argv[1], 'normalizada a', expected, ': pasado')
PY
done

for key in SwipeDistance FingerCount ReverseDirection; do
    kwriteconfig6 --file kwinrc --group Effect-cachyswipe --key "$key" --delete
done
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.reconfigureEffect cachyswipe
state
python3 - <<'PY'
import json, os
s = json.load(open(os.environ['CACHY_TEST_RUNTIME'] + '/state.json'))
assert s['distance'] == 120 and s['fingers'] == 3 and s['reverse'] is True, s
assert not s['gestureActive'] and not s['shown'] and not s['visible'], s
print('sin ajustes explícitos: predeterminados restaurados: pasado')
PY

qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect cachyswipe
touch "$CACHY_TEST_RUNTIME/integration-passed"
