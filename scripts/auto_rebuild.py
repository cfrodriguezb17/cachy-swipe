#!/usr/bin/python3
"""Opt-in, unprivileged rebuilds for Arch/CachyOS Plasma systemd sessions."""
import fcntl
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time

BASE = Path.home() / ".local/share/cachy-swipe"
PACKAGES = ("kwin", "qt6-base", "qt6-declarative", "kcoreaddons", "kconfig", "kwindowsystem")
EFFECTS = ("qdbus6", "org.kde.KWin", "/Effects", "org.kde.kwin.Effects.")


def run(*args, capture=False, **kwargs):
    result = subprocess.run(args, check=True, text=True,
                            stdout=subprocess.PIPE if capture else None, **kwargs)
    return result.stdout.strip() if capture else ""


def effect(method, *args):
    return run(*EFFECTS[:3], EFFECTS[3] + method, *args, capture=True)


def atomic_write(path, data):
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(path.name + ".new")
    temporary.write_text(data)
    temporary.replace(path)


def read_state(path):
    return json.loads(path.read_text()) if path.exists() else {}


def notify(message):
    print(message, flush=True)
    if shutil.which("notify-send"):
        subprocess.run(["notify-send", "Cachy Swipe", message], check=False)


def fingerprint(source):
    packages = run("pacman", "-Q", *PACKAGES, capture=True)
    digest = hashlib.sha256(packages.encode())
    for path in sorted(source.rglob("*")):
        if path.is_file():
            digest.update(str(path.relative_to(source)).encode() + b"\0")
            digest.update(path.read_bytes())
    version = packages.splitlines()[0].split()[1].split("-")[0]
    return digest.hexdigest(), version


def install_layout(source):
    data = Path(os.environ.get("XDG_DATA_HOME", str(Path.home() / ".local/share")))
    destination = data / "kwin/tabbox/cachy-swipe-grid"
    destination.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".cachy-grid-", dir=destination.parent) as temporary:
        staging = Path(temporary) / "layout"
        backup = Path(temporary) / "previous"
        shutil.copytree(source / "tabbox/cachy-swipe-grid", staging)
        if destination.exists():
            destination.rename(backup)
        try:
            staging.rename(destination)
        except OSError:
            if backup.exists():
                backup.rename(destination)
            raise


def activate(version):
    # During an upgrade the running compositor can still be the OLD executable.
    info = run("qdbus6", "org.kde.KWin", "/KWin",
               "org.kde.KWin.supportInformation", capture=True)
    if f"KWin version: {version}" not in info.splitlines():
        return
    enabled = run("kreadconfig6", "--file", "kwinrc", "--group", "Plugins",
                  "--key", "cachyswipeEnabled", "--default", "false", capture=True)
    if enabled != "true" or "cachyswipe" in effect("loadedEffects").splitlines():
        return
    if effect("loadEffect", "cachyswipe") != "true":
        print("Waiting for the next Plasma login to discover the user plugin.", flush=True)


def rebuild(base=BASE):
    base.mkdir(parents=True, exist_ok=True)
    with (base / "lock").open("w") as lock:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except BlockingIOError:
            return
        if Path("/var/lib/pacman/db.lck").exists():
            return
        source = base / "source"
        key, version = fingerprint(source)
        state_file = base / "state.json"
        failure_file = base / "failure.json"
        plugin = base / "plugins/kwin/effects/plugins/cachyswipe.so"
        if read_state(state_file).get("fingerprint") != key or not plugin.exists():
            failure = read_state(failure_file)
            if failure.get("fingerprint") == key and time.time() - failure["time"] < 3600:
                return
            try:
                with tempfile.TemporaryDirectory(prefix="build-", dir=base) as temporary:
                    build = Path(temporary)
                    run("cmake", "-S", str(source), "-B", str(build), "-G", "Ninja",
                        "-DCMAKE_BUILD_TYPE=Release", "-DCMAKE_INSTALL_PREFIX=/usr",
                        "-DBUILD_TESTING=ON", "-DCACHY_SWIPE_INTEGRATION_TESTING=OFF")
                    run("cmake", "--build", str(build), "--parallel", "2")
                    run("ctest", "--test-dir", str(build), "--output-on-failure")
                    # Discard results if packages changed while compilation ran.
                    if Path("/var/lib/pacman/db.lck").exists() or fingerprint(source)[0] != key:
                        return
                    plugin.parent.mkdir(parents=True, exist_ok=True)
                    temporary_plugin = plugin.with_suffix(".so.new")
                    shutil.copy2(build / "bin/kwin/effects/plugins/cachyswipe.so", temporary_plugin)
                    install_layout(source)
                    temporary_plugin.replace(plugin)
                    atomic_write(state_file, json.dumps({"fingerprint": key, "kwin": version}))
                    failure_file.unlink(missing_ok=True)
                    print(f"Installed Cachy Swipe for KWin {version}.", flush=True)
            except (subprocess.CalledProcessError, OSError) as error:
                atomic_write(failure_file, json.dumps({"fingerprint": key, "time": time.time()}))
                notify("Automatic rebuild failed. Previous plugin preserved. "
                       "See: journalctl --user -u cachy-swipe-rebuild.service")
                raise error
        try:
            activate(version)
        except subprocess.CalledProcessError:
            # No graphical bus during login/logout; retry on the next timer tick.
            print("KWin is not ready; activation will be retried.", flush=True)


def install():
    if os.geteuid() == 0:
        raise SystemExit("Run this installer as your desktop user, without sudo.")
    run("systemctl", "--user", "cat", "plasma-kwin_wayland.service", capture=True)
    repo = Path(__file__).resolve().parent.parent
    config = Path(os.environ.get("XDG_CONFIG_HOME", str(Path.home() / ".config")))
    units = config / "systemd/user"
    # Stop an existing rebuild before replacing its source snapshot.
    if (units / "cachy-swipe-rebuild.service").exists():
        run("systemctl", "--user", "stop", "cachy-swipe-rebuild.timer", "cachy-swipe-rebuild.service")
    BASE.mkdir(parents=True, exist_ok=True)
    source = BASE / "source"
    with tempfile.TemporaryDirectory(prefix="source-", dir=BASE) as temporary:
        snapshot = Path(temporary) / "source"
        snapshot.mkdir()
        for name in ("CMakeLists.txt", "LICENSE", "src", "tests", "scripts", "tabbox"):
            item = repo / name
            if item.is_dir():
                shutil.copytree(item, snapshot / name, ignore=shutil.ignore_patterns("__pycache__"))
            else:
                shutil.copy2(item, snapshot / name)
        if source.exists():
            shutil.rmtree(source)
        snapshot.rename(source)
    unit = """[Unit]
Description=Rebuild Cachy Swipe when KWin dependencies change
After=graphical-session.target

[Service]
Type=oneshot
ExecStart=/usr/bin/python3 "%h/.local/share/cachy-swipe/source/scripts/auto_rebuild.py" run
Environment=PATH=/usr/bin:/bin
Nice=10
TimeoutStartSec=10min
"""
    timer = """[Unit]
Description=Check Cachy Swipe compatibility after updates

[Timer]
OnStartupSec=20s
OnUnitInactiveSec=60s
AccuracySec=5s

[Install]
WantedBy=timers.target
"""
    atomic_write(units / "cachy-swipe-rebuild.service", unit)
    atomic_write(units / "cachy-swipe-rebuild.timer", timer)
    environment = run("systemctl", "--user", "show-environment", capture=True)
    previous = next((line.partition("=")[2] for line in environment.splitlines()
                     if line.startswith("QT_PLUGIN_PATH=")), "")
    # Qt retains its built-in paths. Prepend our directory; preserve custom paths.
    own_path = str(BASE / "plugins")
    paths = [own_path] + [p for p in previous.split(":") if p and p != own_path]
    value = ":".join(paths).replace("%", "%%").replace("\\", "\\\\").replace('"', '\\"')
    atomic_write(units / "plasma-kwin_wayland.service.d/90-cachy-swipe.conf",
                 '[Service]\nEnvironment="QT_PLUGIN_PATH=' + value + '"\n')
    previous_layout = run("kreadconfig6", "--file", "kwinrc", "--group", "TabBox",
                          "--key", "LayoutName", "--default", "breeze", capture=True)
    if previous_layout != "cachy-swipe-grid" and not (BASE / "previous-layout").exists():
        atomic_write(BASE / "previous-layout", previous_layout + "\n")
    run("systemctl", "--user", "daemon-reload")
    run("systemctl", "--user", "enable", "--now", "cachy-swipe-rebuild.timer")
    run("systemctl", "--user", "start", "cachy-swipe-rebuild.service")
    if read_state(BASE / "state.json").get("fingerprint") != fingerprint(source)[0]:
        raise SystemExit("Build deferred or failed; layout settings were not changed. Check the service journal.")
    # Select the matching visual geometry only after a successful build.
    run("kwriteconfig6", "--file", "kwinrc", "--group", "TabBox", "--key", "LayoutName", "cachy-swipe-grid")
    run("kwriteconfig6", "--file", "kwinrc", "--group", "Effect-cachyswipe", "--key", "GridNavigation", "true")
    run("kwriteconfig6", "--file", "kwinrc", "--group", "Plugins", "--key", "cachyswipeEnabled", "true")
    subprocess.run(["qdbus6", "org.kde.KWin", "/KWin", "org.kde.KWin.reconfigure"], check=False)
    print("Automatic rebuild enabled. Log out and back in once to enable the user plugin path.")


if __name__ == "__main__":
    if sys.argv[1:] == ["install"]:
        install()
    elif sys.argv[1:] == ["run"]:
        rebuild()
    else:
        raise SystemExit("Usage: auto_rebuild.py install|run")
