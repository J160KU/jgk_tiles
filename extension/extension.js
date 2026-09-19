import GLib from 'gi://GLib';
import Gio from 'gi://Gio';
import Meta from 'gi://Meta';
import Clutter from 'gi://Clutter';
import St from 'gi://St';
import * as Main from 'resource:///org/gnome/shell/ui/main.js';
import {Extension} from 'resource:///org/gnome/shell/extensions/extension.js';

const COLS = 16;
const ROWS = 8;

export default class JgkTilesBridge extends Extension {
    enable() {
        this._stored = null;
        this._service = null;
        this._overlay = null;
        this._hl = null;
        this._grab = null;
        this._work = null;
        this._first = null;
        this._hover = null;
        this._socketPath = GLib.build_filenamev([
            GLib.get_user_runtime_dir(),
            'jgk_tiles.sock',
        ]);
        this._startServer();
    }

    disable() {
        this._hideOverlay();
        this._stopServer();
        this._stored = null;
    }

    _currentWorkArea() {
        const i = global.display.get_current_monitor();
        const ws = global.workspace_manager.get_active_workspace();
        return ws.get_work_area_for_monitor(i);
    }

    _applyResize(x, y, w, h) {
        const win = this._stored ?? global.display.get_focus_window();
        if (!win)
            return false;
        const work = this._work ?? this._currentWorkArea();
        const maxX = work.x + work.width;
        const maxY = work.y + work.height;
        if (x < work.x) x = work.x;
        if (y < work.y) y = work.y;
        if (x + w > maxX) w = maxX - x;
        if (y + h > maxY) h = maxY - y;
        if (w < 1 || h < 1)
            return false;
        try {
            try {
                win.unmaximize();
            } catch (_e) {
                try {
                    win.unmaximize(Meta.MaximizeFlags.BOTH);
                } catch (_e2) {}
            }
            win.move_frame(true, x, y);
            win.move_resize_frame(true, x, y, w, h);
            return true;
        } catch (e) {
            logError(e, 'jgk_tiles RESIZE');
            return false;
        }
    }

    _tileRect(c1, r1, c2, r2) {
        const colMin = Math.min(c1, c2);
        const colMax = Math.max(c1, c2);
        const rowMin = Math.min(r1, r2);
        const rowMax = Math.max(r1, r2);
        const w = this._work.width;
        const h = this._work.height;
        const x = Math.round(w * colMin / COLS);
        const y = Math.round(h * rowMin / ROWS);
        const tw = Math.round(w * (colMax - colMin + 1) / COLS);
        const th = Math.round(h * (rowMax - rowMin + 1) / ROWS);
        return [x, y, tw, th];
    }

    _hit(px, py) {
        const col = Math.min(COLS - 1, Math.max(0,
            Math.floor(px / (this._work.width / COLS))));
        const row = Math.min(ROWS - 1, Math.max(0,
            Math.floor(py / (this._work.height / ROWS))));
        return [col, row];
    }

    _setHighlight(c1, r1, c2, r2, selected) {
        if (!this._hl || !this._work)
            return;
        const [x, y, w, h] = this._tileRect(c1, r1, c2, r2);
        this._hl.set({x, y, width: w, height: h, visible: true});
        this._hl.set_style(selected
            ? 'background-color: rgba(64, 224, 208, 0.45); border: 3px solid rgba(64, 224, 208, 0.95);'
            : 'background-color: rgba(255, 0, 255, 0.40); border: 2px solid rgba(255, 64, 255, 0.90);');
    }

    _eventLocal(event) {
        const [sx, sy] = event.get_coords();
        return [sx - this._work.x, sy - this._work.y];
    }

    _onMotion(_actor, event) {
        if (!this._overlay)
            return Clutter.EVENT_STOP;
        const [px, py] = this._eventLocal(event);
        const [col, row] = this._hit(px, py);
        if (this._hover && this._hover.col === col && this._hover.row === row)
            return Clutter.EVENT_STOP;
        this._hover = {col, row};
        if (this._first)
            this._setHighlight(this._first.col, this._first.row, col, row, true);
        else
            this._setHighlight(col, row, col, row, false);
        return Clutter.EVENT_STOP;
    }

    _onPress(_actor, event) {
        if (!this._overlay || event.get_button() !== 1)
            return Clutter.EVENT_STOP;
        const [px, py] = this._eventLocal(event);
        const [col, row] = this._hit(px, py);
        if (!this._first) {
            this._first = {col, row};
            this._hover = {col, row};
            this._setHighlight(col, row, col, row, true);
            return Clutter.EVENT_STOP;
        }
        const [x, y, w, h] = this._tileRect(
            this._first.col, this._first.row, col, row);
        this._applyResize(this._work.x + x, this._work.y + y, w, h);
        this._hideOverlay();
        return Clutter.EVENT_STOP;
    }

    _onKey(_actor, event) {
        if (event.get_key_symbol() === Clutter.KEY_Escape)
            this._hideOverlay();
        return Clutter.EVENT_STOP;
    }

    _showOverlay() {
        if (this._overlay)
            return;
        this._stored = global.display.get_focus_window();
        this._work = this._currentWorkArea();
        this._first = null;
        this._hover = null;

        this._overlay = new St.Widget({
            reactive: true,
            can_focus: true,
            x: this._work.x,
            y: this._work.y,
            width: this._work.width,
            height: this._work.height,
            style: 'background-color: rgba(0, 0, 0, 0.5);',
        });

        this._hl = new St.Widget({visible: false});
        this._overlay.add_child(this._hl);

        for (let i = 1; i < COLS; i++) {
            const x = Math.round(this._work.width * i / COLS);
            this._overlay.add_child(new St.Widget({
                style: 'background-color: rgba(255, 255, 255, 0.33);',
                x: x - 1,
                y: 0,
                width: 2,
                height: this._work.height,
            }));
        }
        for (let i = 1; i < ROWS; i++) {
            const y = Math.round(this._work.height * i / ROWS);
            this._overlay.add_child(new St.Widget({
                style: 'background-color: rgba(255, 255, 255, 0.33);',
                x: 0,
                y: y - 1,
                width: this._work.width,
                height: 2,
            }));
        }

        this._overlay.connect('button-press-event', this._onPress.bind(this));
        this._overlay.connect('motion-event', this._onMotion.bind(this));
        this._overlay.connect('key-press-event', this._onKey.bind(this));

        Main.layoutManager.addTopChrome(this._overlay);
        try {
            this._grab = Main.pushModal(this._overlay);
        } catch (e) {
            logError(e, 'jgk_tiles pushModal');
            this._grab = null;
        }
        global.stage.set_key_focus(this._overlay);
    }

    _hideOverlay() {
        if (this._grab) {
            try {
                Main.popModal(this._grab);
            } catch (_e) {
                try {
                    this._grab.dismiss();
                } catch (_e2) {}
            }
            this._grab = null;
        }
        if (this._overlay) {
            Main.layoutManager.removeChrome(this._overlay);
            this._overlay.destroy();
            this._overlay = null;
        }
        this._hl = null;
        this._work = null;
        this._first = null;
        this._hover = null;
        if (this._stored) {
            try {
                this._stored.activate(global.get_current_time());
            } catch (_e) {}
        }
    }

    _toggleOverlay() {
        if (this._overlay)
            this._hideOverlay();
        else
            this._showOverlay();
        return true;
    }

    _handleLine(line) {
        const parts = (line ?? '').trim().split(/\s+/);
        if (parts[0] === 'PING')
            return 'OK';
        if (parts[0] === 'STORE_FOCUS') {
            this._stored = global.display.get_focus_window();
            return 'OK';
        }
        if (parts[0] === 'WORKAREA') {
            const r = this._currentWorkArea();
            return `OK ${r.x} ${r.y} ${r.width} ${r.height}`;
        }
        if (parts[0] === 'OVERLAY_TOGGLE')
            return this._toggleOverlay() ? 'OK' : 'ERR';
        if (parts[0] === 'OVERLAY_SHOW') {
            this._showOverlay();
            return 'OK';
        }
        if (parts[0] === 'OVERLAY_HIDE') {
            this._hideOverlay();
            return 'OK';
        }
        if (parts[0] === 'RESIZE' && parts.length === 5) {
            const x = parseInt(parts[1], 10);
            const y = parseInt(parts[2], 10);
            const w = parseInt(parts[3], 10);
            const h = parseInt(parts[4], 10);
            return this._applyResize(x, y, w, h) ? 'OK' : 'ERR';
        }
        return 'ERR';
    }

    _onIncoming(_svc, connection, _src) {
        const inStream = new Gio.DataInputStream({
            base_stream: connection.get_input_stream(),
        });
        inStream.read_line_async(GLib.PRIORITY_DEFAULT, null, (_obj, res) => {
            try {
                const [line] = inStream.read_line_finish_utf8(res);
                const reply = this._handleLine(line ?? '');
                connection.get_output_stream().write_all(
                    new TextEncoder().encode(reply), null);
            } catch (e) {
                logError(e, 'jgk_tiles bridge incoming');
            } finally {
                try {
                    connection.close(null);
                } catch (_e) {}
            }
        });
        return true;
    }

    _startServer() {
        if (this._service)
            return;
        try {
            GLib.unlink(this._socketPath);
        } catch (_e) {}

        try {
            const addr = Gio.UnixSocketAddress.new(this._socketPath);
            this._service = new Gio.SocketService();
            this._service.add_address(
                addr,
                Gio.SocketType.STREAM,
                Gio.SocketProtocol.DEFAULT,
                null);
            this._service.connect('incoming', this._onIncoming.bind(this));
            this._service.start();
            console.log(`jgk_tiles bridge listening on ${this._socketPath}`);
        } catch (e) {
            logError(e, 'jgk_tiles bridge start');
            this._service = null;
        }
    }

    _stopServer() {
        if (this._service) {
            try {
                this._service.stop();
                this._service.close();
            } catch (_e) {}
            this._service = null;
        }
        try {
            GLib.unlink(this._socketPath);
        } catch (_e) {}
    }
}
