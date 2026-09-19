#!/bin/sh
set -e

CMD="$HOME/.local/bin/jgk_tiles --toggle"

python3 - "$CMD" <<'PY'
import ast
import subprocess
import sys

cmd = sys.argv[1]
schema = "org.gnome.settings-daemon.plugins.media-keys"
path_key = "/org/gnome/settings-daemon/plugins/media-keys/custom-keybindings/jgk-tiles/"
bind_key = schema + ".custom-keybinding:" + path_key

def gget(*args):
    return subprocess.check_output(["gsettings", "get", *args], text=True).strip()

def gset(*args):
    subprocess.run(["gsettings", "set", *args], check=True)

raw = gget(schema, "custom-keybindings")
body = raw.replace("@as ", "")
paths = ast.literal_eval(body) if body.startswith("[") else []
if path_key not in paths:
    paths.append(path_key)
    gset(schema, "custom-keybindings", str(paths))

gset(bind_key, "name", "jgk_tiles")
gset(bind_key, "command", cmd)
gset(bind_key, "binding", "<Control>Escape")
print("GNOME shortcut: Ctrl+Esc -> jgk_tiles --toggle")
PY
