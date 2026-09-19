#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$ROOT"

need=()
for pkg in build-essential pkg-config libx11-dev libwayland-dev wayland-protocols; do
  if ! dpkg -s "$pkg" >/dev/null 2>&1; then
    need+=("$pkg")
  fi
done
if ((${#need[@]})); then
  echo "Installing packages: ${need[*]}"
  sudo apt-get install -y "${need[@]}"
fi

make
make install

echo "jgk_tiles installed. After the first GNOME extension install, log out and back in, then:"
echo "  gnome-extensions enable jgk-tiles@local"
echo "  test -S \"\$XDG_RUNTIME_DIR/jgk_tiles.sock\" && echo bridge ok"
