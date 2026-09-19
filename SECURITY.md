# SECURITY

scope:local user window tiling; no network; no root for normal use
trust:unix socket in XDG_RUNTIME_DIR; gnome-shell extension in user session

## surface
- socket `$XDG_RUNTIME_DIR/jgk_tiles.sock` line protocol PING STORE_FOCUS WORKAREA OVERLAY_* RESIZE
- resize only the stored Meta window (focus at overlay show)
- writes: `~/.local/bin/jgk_tiles`, `~/.config/autostart`, user GNOME extension dir, gsettings custom keybinding
- log `~/.cache/jgk_tiles.log`

## not in repo
secrets credentials none

## abuse
same-uid can talk to the socket and resize the stored window; no privesc
install.sh may sudo apt for build deps

## privacy
no telemetry; overlay is local compositor chrome
