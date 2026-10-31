"""Upgrade scenarios without touching the desktop, packages or real services."""
import importlib.util
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
spec = importlib.util.spec_from_file_location(
    "auto_rebuild", Path(__file__).resolve().parents[1] / "scripts/auto_rebuild.py")
automation = importlib.util.module_from_spec(spec)
spec.loader.exec_module(automation)


class UpgradeTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name)
        self.plugin = self.base / "plugins/kwin/effects/plugins/cachyswipe.so"
        self.plugin.parent.mkdir(parents=True)
        self.plugin.write_bytes(b"previous plugin")
        self.commands = []
        layout = patch.object(automation, "install_layout")
        self.install_layout = layout.start()
        self.addCleanup(layout.stop)

    def command(self, *args, **kwargs):
        self.commands.append(args)
        if args[:2] == ("cmake", "--build"):
            output = Path(args[2]) / "bin/kwin/effects/plugins/cachyswipe.so"
            output.parent.mkdir(parents=True)
            output.write_bytes(b"new plugin")
        return ""

    def test_upgrade_installs_only_after_tests_and_skips_unchanged(self):
        with patch.object(automation, "fingerprint", return_value=("new", "6.8.1")), \
                patch.object(automation, "run", side_effect=self.command), \
                patch.object(automation, "activate") as activate:
            automation.rebuild(self.base)
            self.assertEqual(self.plugin.read_bytes(), b"new plugin")
            self.install_layout.assert_called_once()
            self.assertTrue(any(cmd[0] == "ctest" for cmd in self.commands))
            self.commands.clear()
            automation.rebuild(self.base)
            self.assertEqual(self.commands, [])
            self.assertEqual(activate.call_count, 2)

    def test_failed_tests_preserve_previous_plugin_and_back_off(self):
        def fail(*args, **kwargs):
            if args[0] == "ctest":
                raise subprocess.CalledProcessError(1, args)
            return self.command(*args, **kwargs)
        with patch.object(automation, "fingerprint", return_value=("new", "6.8.1")), \
                patch.object(automation, "run", side_effect=fail), \
                patch.object(automation, "notify") as notify:
            with self.assertRaises(subprocess.CalledProcessError):
                automation.rebuild(self.base)
            self.assertEqual(self.plugin.read_bytes(), b"previous plugin")
            self.assertFalse((self.base / "state.json").exists())
            self.commands.clear()
            automation.rebuild(self.base)
            self.assertEqual(self.commands, [])
            notify.assert_called_once()

    def test_packages_changing_during_build_discard_result(self):
        with patch.object(automation, "fingerprint", side_effect=[("old", "6.8.1"), ("new", "6.8.2")]), \
                patch.object(automation, "run", side_effect=self.command), \
                patch.object(automation, "activate") as activate:
            automation.rebuild(self.base)
            self.assertEqual(self.plugin.read_bytes(), b"previous plugin")
            activate.assert_not_called()

    def test_old_running_kwin_never_loads_new_plugin(self):
        with patch.object(automation, "run", return_value="KWin version: 6.8.0"), \
                patch.object(automation, "effect") as effect:
            automation.activate("6.8.1")
            effect.assert_not_called()

    def test_matching_kwin_loads_plugin(self):
        with patch.object(automation, "run", side_effect=["KWin version: 6.8.1", "true"]), \
                patch.object(automation, "effect", side_effect=["blur\nscale", "true"]) as effect:
            automation.activate("6.8.1")
            self.assertEqual(effect.call_args.args, ("loadEffect", "cachyswipe"))

    def test_disabled_plugin_stays_disabled(self):
        with patch.object(automation, "run", side_effect=["KWin version: 6.8.1", "false"]), \
                patch.object(automation, "effect") as effect:
            automation.activate("6.8.1")
            effect.assert_not_called()

    def test_deleted_plugin_is_rebuilt_even_with_matching_state(self):
        (self.base / "state.json").write_text(json.dumps({"fingerprint": "new"}))
        self.plugin.unlink()
        with patch.object(automation, "fingerprint", return_value=("new", "6.8.1")), \
                patch.object(automation, "run", side_effect=self.command), \
                patch.object(automation, "activate"):
            automation.rebuild(self.base)
            self.assertEqual(self.plugin.read_bytes(), b"new plugin")


class LayoutInstallationTests(unittest.TestCase):
    def test_install_and_update_use_the_user_data_directory(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            source = root / "source"
            layout = source / "tabbox/cachy-swipe-grid"
            layout.mkdir(parents=True)
            (layout / "metadata.json").write_text("first")
            data = root / "user-data"
            with patch.dict(os.environ, {"XDG_DATA_HOME": str(data)}):
                automation.install_layout(source)
                installed = data / "kwin/tabbox/cachy-swipe-grid/metadata.json"
                self.assertEqual(installed.read_text(), "first")
                (layout / "metadata.json").write_text("updated")
                automation.install_layout(source)
                self.assertEqual(installed.read_text(), "updated")
                self.assertEqual(len(list(installed.parent.parent.iterdir())), 1)


if __name__ == "__main__":
    unittest.main()
