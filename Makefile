CC          := cc
CFLAGS      := -O3 -flto -Wall -Wextra -std=c11
LDFLAGS     := -flto -s
PKG_CFLAGS  := $(shell pkg-config --cflags x11 wayland-client)
PKG_LIBS    := $(shell pkg-config --libs x11 wayland-client)
WL_PROTOCOL := /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml
DECO_PROTOCOL := /usr/share/wayland-protocols/unstable/xdg-decoration/xdg-decoration-unstable-v1.xml
GEN_DIR     := generated
XOBJS := main.o grid.o window.o overlay.o overlay_dispatch.o hotkey.o evdev.o wayland.o ipc.o log.o bridge.o
EXT_DIR := $(HOME)/.local/share/gnome-shell/extensions/jgk-tiles@local
GEN   := $(GEN_DIR)/xdg-shell-protocol.c $(GEN_DIR)/xdg-shell-client-protocol.h \
         $(GEN_DIR)/xdg-decoration-protocol.c $(GEN_DIR)/xdg-decoration-unstable-v1-client-protocol.h

CFLAGS += $(PKG_CFLAGS) -I$(GEN_DIR)

all: $(GEN_DIR) jgk_tiles

$(GEN_DIR):
	mkdir -p $(GEN_DIR)

$(GEN_DIR)/xdg-shell-client-protocol.h: $(WL_PROTOCOL) | $(GEN_DIR)
	wayland-scanner client-header $< $@

$(GEN_DIR)/xdg-shell-protocol.c: $(WL_PROTOCOL) | $(GEN_DIR)
	wayland-scanner private-code $< $@

$(GEN_DIR)/xdg-decoration-unstable-v1-client-protocol.h: $(DECO_PROTOCOL) | $(GEN_DIR)
	wayland-scanner client-header $< $@

$(GEN_DIR)/xdg-decoration-protocol.c: $(DECO_PROTOCOL) | $(GEN_DIR)
	wayland-scanner private-code $< $@

wayland.o: wayland.c $(GEN_DIR)/xdg-shell-client-protocol.h $(GEN_DIR)/xdg-decoration-unstable-v1-client-protocol.h
	$(CC) $(CFLAGS) -c -o $@ wayland.c

jgk_tiles: $(XOBJS) $(GEN_DIR)/xdg-shell-protocol.c $(GEN_DIR)/xdg-decoration-protocol.c
	$(CC) $(LDFLAGS) -o $@ $(XOBJS) $(GEN_DIR)/xdg-shell-protocol.c $(GEN_DIR)/xdg-decoration-protocol.c $(PKG_LIBS)

install: install-bin install-ext install-keys

install-bin:
	install -D -m 755 jgk_tiles $(HOME)/.local/bin/jgk_tiles
	mkdir -p $(HOME)/.config/autostart
	sed 's|^Exec=.*|Exec=$(HOME)/.local/bin/jgk_tiles|' jgk-tiles.desktop \
		> $(HOME)/.config/autostart/jgk-tiles.desktop

install-ext:
	install -D -m 644 extension/metadata.json $(EXT_DIR)/metadata.json
	install -D -m 644 extension/extension.js $(EXT_DIR)/extension.js
	-gnome-extensions disable jgk-tiles@local >/dev/null 2>&1 || true
	-gnome-extensions enable jgk-tiles@local >/dev/null 2>&1 || true
	@echo "Extension installed. If $(XDG_RUNTIME_DIR)/jgk_tiles.sock is missing:"
	@echo "  log out and back in, then: gnome-extensions enable jgk-tiles@local"

install-keys:
	install -D -m 755 install-keys.sh $(HOME)/.local/bin/jgk-tiles-install-keys
	$(HOME)/.local/bin/jgk-tiles-install-keys

clean:
	rm -rf jgk_tiles $(XOBJS) $(GEN_DIR) test_or

.PHONY: all install install-bin install-ext install-keys clean
