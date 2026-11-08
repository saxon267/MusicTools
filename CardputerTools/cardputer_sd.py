#!/usr/bin/env python3
"""CardputerTools - manage the microSD card of a Cardputer over USB serial.

The Cardputer has to run the companion bridge firmware (see firmware/), which
speaks a small line based protocol:

    PING                -> "PONG 1 cardputer-sd"
    LS <path>           -> "OK <n>" then n lines of  name\t size\t type
    GET <path>          -> "OK <size>" then <size> raw bytes
    PUT <path> <size>   -> "READY", <size> raw bytes, then "OK <written>"
    DEL / MKDIR / RMDIR -> "OK 0"
    FREE                -> "OK <bytes>"

Run it with a real device:

    python cardputer_sd.py --port COM5

or without hardware, against an in-memory fake card:

    python cardputer_sd.py --mock
"""
from __future__ import annotations

import argparse
import os
import queue
import sys
import threading
import tkinter as tk
from tkinter import filedialog, messagebox, simpledialog, ttk

try:
    import serial
    from serial.tools import list_ports
except ImportError:  # the mock mode still works without pyserial
    serial = None
    list_ports = None

# ---------------------------------------------------------------- GruvBox Dark

GB = {
    "bg0": "#282828",
    "bg1": "#3c3836",
    "bg2": "#504945",
    "fg": "#ebdbb2",
    "fg4": "#a89984",
    "gray": "#928374",
    "red": "#fb4934",
    "green": "#b8bb26",
    "yellow": "#fabd2f",
    "blue": "#83a598",
    "purple": "#d3869b",
    "aqua": "#8ec07c",
    "orange": "#fe8019",
}


class BridgeError(Exception):
    pass


class Bridge:
    """Thin client for the SD bridge protocol."""

    def __init__(self, port, baud=115200):
        self.port = port
        self.ser = port if not isinstance(port, str) else None

    # -- low level ------------------------------------------------------
    def open(self):
        if self.ser is None:
            self.ser = serial.Serial(self.port, 115200, timeout=1, write_timeout=10)

    def close(self):
        if self.ser is not None and hasattr(self.ser, "close"):
            try:
                self.ser.close()
            except Exception:
                pass
        self.ser = None

    def _readline(self, timeout=8.0):
        buf = bytearray()
        deadline = _now() + timeout
        while _now() < deadline:
            ch = self.ser.read(1)
            if not ch:
                continue
            if ch == b"\n":
                return buf.decode("utf-8", "replace").rstrip("\r")
            buf += ch
        raise BridgeError("timeout waiting for the device")

    def _expect_ok(self):
        line = self._readline()
        if line.startswith("ERR"):
            raise BridgeError(line)
        if not line.startswith("OK"):
            raise BridgeError("unexpected reply: " + line)
        return line

    def _read_exact(self, size, progress=None):
        data = bytearray()
        while len(data) < size:
            chunk = self.ser.read(min(4096, size - len(data)))
            if not chunk:
                raise BridgeError("transfer stalled")
            data += chunk
            if progress:
                progress(len(data), size)
        return bytes(data)

    def _cmd(self, text):
        self.ser.write((text + "\n").encode())
        self.ser.flush()

    # -- commands -------------------------------------------------------
    def ping(self):
        self._cmd("PING")
        line = self._readline(3)
        if not line.startswith("PONG"):
            raise BridgeError("not a Cardputer SD bridge: " + line)
        return line

    def ls(self, path):
        self._cmd("LS " + path)
        head = self._expect_ok()
        count = int(head.split()[1])
        entries = []
        for _ in range(count):
            line = self._readline()
            parts = line.split("\t")
            if len(parts) != 3:
                continue
            name, size, kind = parts
            entries.append((name, int(size), kind == "d"))
        entries.sort(key=lambda e: (not e[2], e[0].lower()))
        return entries

    def get(self, path, progress=None):
        self._cmd("GET " + path)
        head = self._expect_ok()
        size = int(head.split()[1])
        return self._read_exact(size, progress)

    def put(self, path, data, progress=None):
        self._cmd("PUT %s %d" % (path, len(data)))
        line = self._readline()
        if not line.startswith("READY"):
            raise BridgeError(line)

        sent = 0
        while sent < len(data):
            chunk = data[sent : sent + 4096]
            self.ser.write(chunk)
            self.ser.flush()
            sent += len(chunk)
            if progress:
                progress(sent, len(data))

        self._expect_ok()

    def delete(self, path):
        self._cmd("DEL " + path)
        self._expect_ok()

    def mkdir(self, path):
        self._cmd("MKDIR " + path)
        self._expect_ok()

    def rmdir(self, path):
        self._cmd("RMDIR " + path)
        self._expect_ok()


def _now():
    import time

    return time.monotonic()


# ---------------------------------------------------------------- fake device


class MockSerial:
    """Implements the bridge protocol in memory, so the UI can be exercised
    without a Cardputer (and so the protocol has something to test against)."""

    def __init__(self):
        self.rx = bytearray()
        self.inbuf = bytearray()
        self.dirs = {"/"}
        self.files = {
            "/firmware/MusicTools-8MB.bin": 628 * 1024,
            "/firmware/readme.txt": 212,
            "/music/Am-G-F-E.mp3": 3_400_000,
            "/music/practice-loop.wav": 8_800_000,
            "/notes/chords.txt": 980,
        }
        for f in list(self.files):
            self.dirs.add(os.path.dirname(f))
        self.pending = 0
        self.put_size = 0

    # pyserial-ish surface
    @property
    def in_waiting(self):
        return len(self.rx)

    def read(self, n=1):
        if not self.rx:
            return b""
        chunk = bytes(self.rx[:n])
        del self.rx[:n]
        return chunk

    def write(self, data):
        data = bytes(data)
        while data:
            if self.pending:
                take = min(self.pending, len(data))
                self.pending -= take
                data = data[take:]
                if self.pending == 0:
                    self.rx += b"OK %d\n" % self.put_size
                continue
            idx = data.find(b"\n")
            if idx < 0:
                self.inbuf += data
                break
            self.inbuf += data[:idx]
            line = bytes(self.inbuf).decode("utf-8", "replace").strip()
            self.inbuf = bytearray()
            data = data[idx + 1 :]
            self._handle(line)

    def flush(self):
        pass

    def close(self):
        pass

    def _payload(self, path):
        if path in self.files:
            return bytes((i * 7 + len(path)) & 0xFF for i in range(self.files[path]))
        return b""

    def _handle(self, line):
        if not line:
            return
        verb, _, args = line.partition(" ")
        if verb == "PING":
            self.rx += b"PONG 1 cardputer-sd\n"
            return

        if verb == "LS":
            path = (args or "/").rstrip("/") or "/"
            names = set()
            for f in self.files:
                if os.path.dirname(f) == path:
                    names.add(os.path.basename(f))
            for d in self.dirs:
                if d != path and os.path.dirname(d) == path:
                    names.add(os.path.basename(d))
            self.rx += b"OK %d\n" % len(names)
            for n in sorted(names):
                full = (path.rstrip("/") + "/" + n)
                if full in self.dirs:
                    self.rx += ("%s\t0\td\n" % n).encode()
                else:
                    self.rx += ("%s\t%d\tf\n" % (n, self.files[full])).encode()
            return

        if verb == "GET":
            data = self._payload(args)
            self.rx += b"OK %d\n" % len(data)
            self.rx += data
            return

        if verb == "PUT":
            path, _, size = args.rpartition(" ")
            size = int(size)
            self.files[path] = size
            self.dirs.add(os.path.dirname(path))
            self.pending = size
            self.put_size = size
            self.rx += b"READY\n"
            return

        if verb == "DEL":
            self.files.pop(args, None)
            self.rx += b"OK 0\n"
            return

        if verb == "MKDIR":
            self.dirs.add(args)
            self.rx += b"OK 0\n"
            return

        if verb == "RMDIR":
            self.dirs.discard(args)
            self.rx += b"OK 0\n"
            return

        self.rx += b"ERR 10 unknown command\n"


# ---------------------------------------------------------------- the app


class App(tk.Tk):
    def __init__(self, port=None, mock=False):
        super().__init__()
        self.title("CardputerTools · SD card over serial")
        self.configure(bg=GB["bg0"])
        self.geometry("860x560")
        self.minsize(720, 460)

        self.bridge = None
        self.path = "/"
        self.entries = []
        self.busy = False
        self.events = queue.Queue()

        self._build_style()
        self._build_ui()
        self.after(80, self._drain)
        self.update_idletasks()
        self._dark_titlebar()

        if mock:
            self.port_var.set("mock://")
            self.connect(mock=True)
        elif port:
            self.port_var.set(port)
            self.connect()

    def _dark_titlebar(self):
        """Windows draws a light title bar by default; ask DWM for the dark one."""
        if sys.platform != "win32":
            return
        try:
            import ctypes

            hwnd = ctypes.windll.user32.GetParent(self.winfo_id())
            value = ctypes.c_int(1)
            ctypes.windll.dwmapi.DwmSetWindowAttribute(hwnd, 20, ctypes.byref(value), ctypes.sizeof(value))
        except Exception:
            pass

    # -- styling --------------------------------------------------------
    def _build_style(self):
        st = ttk.Style(self)
        st.theme_use("clam")

        st.configure(".", background=GB["bg0"], foreground=GB["fg"], fieldbackground=GB["bg1"])
        st.configure("TFrame", background=GB["bg0"])
        st.configure("TLabel", background=GB["bg0"], foreground=GB["fg"])
        st.configure("Dim.TLabel", background=GB["bg0"], foreground=GB["gray"])
        st.configure("TButton", background=GB["bg1"], foreground=GB["fg"], borderwidth=0, padding=(10, 5))
        st.map("TButton", background=[("active", GB["bg2"]), ("disabled", GB["bg0"])],
               foreground=[("disabled", GB["gray"])])
        st.configure("Accent.TButton", background=GB["yellow"], foreground=GB["bg0"])
        st.map("Accent.TButton", background=[("active", GB["orange"])])
        st.configure("TEntry", fieldbackground=GB["bg1"], foreground=GB["fg"], insertcolor=GB["fg"],
                     bordercolor=GB["bg1"], lightcolor=GB["bg1"], darkcolor=GB["bg1"])
        st.configure("TCombobox", fieldbackground=GB["bg1"], background=GB["bg1"], foreground=GB["fg"],
                     arrowcolor=GB["fg"])
        st.map("TCombobox", fieldbackground=[("readonly", GB["bg1"])],
               foreground=[("readonly", GB["fg"])])
        st.configure("Treeview", background=GB["bg0"], fieldbackground=GB["bg0"], foreground=GB["fg"],
                     borderwidth=0, rowheight=22)
        st.configure("Treeview.Heading", background=GB["bg1"], foreground=GB["gray"], relief="flat")
        st.map("Treeview", background=[("selected", GB["bg2"])], foreground=[("selected", GB["yellow"])])
        st.map("Treeview.Heading", background=[("active", GB["bg2"])])
        self.option_add("*TCombobox*Listbox.background", GB["bg1"])
        self.option_add("*TCombobox*Listbox.foreground", GB["fg"])
        self.option_add("*TCombobox*Listbox.selectBackground", GB["bg2"])

    # -- layout ---------------------------------------------------------
    def _build_ui(self):
        top = ttk.Frame(self, padding=(12, 10, 12, 6))
        top.pack(fill="x")

        ttk.Label(top, text="PORT").pack(side="left")
        self.port_var = tk.StringVar()
        self.port_box = ttk.Combobox(top, textvariable=self.port_var, width=12, state="readonly")
        self.port_box.pack(side="left", padx=(8, 6))
        self.refresh_ports()

        ttk.Button(top, text="扫描端口", command=self.refresh_ports).pack(side="left", padx=3)
        self.connect_btn = ttk.Button(top, text="连接", style="Accent.TButton", command=self.connect)
        self.connect_btn.pack(side="left", padx=3)

        self.status_dot = tk.Canvas(top, width=12, height=12, bg=GB["bg0"], highlightthickness=0)
        self.status_dot.pack(side="right")
        self._dot = self.status_dot.create_oval(2, 2, 10, 10, fill=GB["red"], outline="")
        self.status_var = tk.StringVar(value="未连接")
        ttk.Label(top, textvariable=self.status_var, style="Dim.TLabel").pack(side="right", padx=(0, 8))

        bar = ttk.Frame(self, padding=(12, 0, 12, 8))
        bar.pack(fill="x")
        ttk.Button(bar, text="↑ 上一级", command=self.go_up).pack(side="left")
        self.path_var = tk.StringVar(value="/")
        entry = ttk.Entry(bar, textvariable=self.path_var, state="readonly")
        entry.pack(side="left", fill="x", expand=True, padx=8)

        ttk.Button(bar, text="上传", command=self.upload).pack(side="left", padx=2)
        ttk.Button(bar, text="下载", command=self.download).pack(side="left", padx=2)
        ttk.Button(bar, text="新建文件夹", command=self.new_folder).pack(side="left", padx=2)
        ttk.Button(bar, text="删除", command=self.delete).pack(side="left", padx=2)
        ttk.Button(bar, text="刷新", command=self.reload).pack(side="left", padx=2)

        mid = ttk.Frame(self, padding=(12, 0, 12, 0))
        mid.pack(fill="both", expand=True)

        self.tree = ttk.Treeview(mid, columns=("name", "size", "kind"), show="headings", selectmode="browse")
        self.tree.heading("name", text="名称")
        self.tree.heading("size", text="大小")
        self.tree.heading("kind", text="类型")
        self.tree.column("name", width=420, anchor="w")
        self.tree.column("size", width=110, anchor="e")
        self.tree.column("kind", width=70, anchor="w")
        self.tree.pack(side="left", fill="both", expand=True)
        self.tree.bind("<Double-1>", self._on_double)

        sb = ttk.Scrollbar(mid, orient="vertical", command=self.tree.yview)
        sb.pack(side="left", fill="y")
        self.tree.configure(yscrollcommand=sb.set)

        bottom = ttk.Frame(self, padding=(12, 8, 12, 12))
        bottom.pack(fill="x")
        self.log = tk.Text(bottom, height=6, bg=GB["bg1"], fg=GB["fg4"], insertbackground=GB["fg"],
                           relief="flat", borderwidth=0, font=("Consolas", 9), wrap="none")
        self.log.pack(fill="x")
        self.log.configure(state="disabled")
        self._log("CardputerTools ready. Flash CardputerTools/firmware, then connect.")

    # -- helpers --------------------------------------------------------
    def _log(self, text, color=None):
        self.log.configure(state="normal")
        self.log.insert("end", text + "\n")
        self.log.see("end")
        self.log.configure(state="disabled")

    def _set_status(self, text, ok=None):
        self.status_var.set(text)
        color = GB["green"] if ok else (GB["red"] if ok is False else GB["gray"])
        self.status_dot.itemconfigure(self._dot, fill=color)

    def _drain(self):
        try:
            while True:
                fn = self.events.get_nowait()
                fn()
        except queue.Empty:
            pass
        self.after(80, self._drain)

    def _post(self, fn):
        self.events.put(fn)

    def _run(self, work, done=None):
        if self.busy:
            return
        self.busy = True

        def runner():
            try:
                result = work()
            except Exception as exc:  # noqa: BLE001
                msg = str(exc)
                self._post(self._unbusy)
                self._post(lambda: self._log("错误: " + msg))
                self._post(lambda: self._set_status("错误", False))
                return
            # release the flag first, otherwise a follow-up call from done()
            # would be dropped by the busy check
            self._post(self._unbusy)
            if done:
                self._post(lambda: done(result))

        threading.Thread(target=runner, daemon=True).start()

    def _unbusy(self):
        self.busy = False

    # -- connection -----------------------------------------------------
    def refresh_ports(self):
        values = []
        if list_ports is not None:
            for p in list_ports.comports():
                label = "%s" % p.device
                values.append(label)
        if "mock://" not in values:
            values.append("mock://")
        self.port_box["values"] = values
        if not self.port_var.get() and values:
            self.port_var.set(values[0])

    def connect(self, mock=False):
        if self.bridge:
            self.bridge.close()
            self.bridge = None
            self.connect_btn.configure(text="连接")
            self._set_status("未连接")
            return

        name = "mock://" if mock else self.port_var.get()
        if not name:
            return

        def work():
            if name == "mock://":
                bridge = Bridge(MockSerial())
            else:
                bridge = Bridge(name)
                bridge.open()
            bridge.ping()
            return bridge

        def done(bridge):
            self.bridge = bridge
            self.connect_btn.configure(text="断开")
            self._set_status("已连接 " + name, True)
            self._log("已连接: " + name)
            self.path = "/"
            self.reload()

        self._run(work, done)

    # -- navigation -----------------------------------------------------
    def reload(self):
        if not self.bridge:
            return
        path = self.path

        def work():
            return self.bridge.ls(path)

        def done(entries):
            self.entries = entries
            self.tree.delete(*self.tree.get_children())
            for name, size, isdir in entries:
                label = name + "/" if isdir else name
                size_txt = "—" if isdir else _human(size)
                self.tree.insert("", "end", values=(label, size_txt, "文件夹" if isdir else "文件"))
            self.path_var.set(path)
            self._log("%s  %d 项" % (path, len(entries)))

        self._run(work, done)

    def go_up(self):
        if self.path in ("/", ""):
            return
        self.path = os.path.dirname(self.path) or "/"
        self.reload()

    def _on_double(self, _event):
        sel = self.tree.selection()
        if not sel:
            return
        name = self.tree.item(sel[0], "values")[0]
        if not name.endswith("/"):
            return
        self.path = (self.path.rstrip("/") + "/" + name.rstrip("/")) or "/"
        self.reload()

    def _selected(self):
        sel = self.tree.selection()
        if not sel:
            return None
        return self.tree.item(sel[0], "values")[0]

    def _join(self, name):
        return self.path.rstrip("/") + "/" + name.rstrip("/")

    # -- actions --------------------------------------------------------
    def upload(self):
        if not self.bridge:
            return
        local = filedialog.askopenfilename(title="选择要上传的文件")
        if not local:
            return
        with open(local, "rb") as f:
            data = f.read()
        target = self._join(os.path.basename(local))
        self._log("上传 %s (%s) ..." % (target, _human(len(data))))

        def work():
            self.bridge.put(target, data)

        self._run(work, lambda _: (self._log("上传完成: " + target), self.reload()))

    def download(self):
        if not self.bridge:
            return
        name = self._selected()
        if not name or name.endswith("/"):
            return
        remote = self._join(name)
        local = filedialog.asksaveasfilename(title="保存到", initialfile=name.rstrip("/"))
        if not local:
            return

        def work():
            return self.bridge.get(remote)

        def done(data):
            with open(local, "wb") as f:
                f.write(data)
            self._log("已保存 %s (%s)" % (local, _human(len(data))))

        self._run(work, done)

    def new_folder(self):
        if not self.bridge:
            return
        name = simpledialog.askstring("新建文件夹", "名称:", parent=self)
        if not name:
            return
        target = self._join(name)

        def work():
            self.bridge.mkdir(target)

        self._run(work, lambda _: (self._log("已创建 " + target), self.reload()))

    def delete(self):
        if not self.bridge:
            return
        name = self._selected()
        if not name:
            return
        target = self._join(name)
        if not messagebox.askyesno("删除", "确定删除 %s ?" % target):
            return

        def work():
            if name.endswith("/"):
                self.bridge.rmdir(target)
            else:
                self.bridge.delete(target)

        self._run(work, lambda _: (self._log("已删除 " + target), self.reload()))


def _human(n):
    for unit in ("B", "KB", "MB", "GB"):
        if n < 1024 or unit == "GB":
            return "%.1f %s" % (n, unit) if unit != "B" else "%d B" % n
        n /= 1024.0
    return str(n)


def main():
    ap = argparse.ArgumentParser(description="Manage the Cardputer microSD over serial")
    ap.add_argument("--port", help="serial port, e.g. COM5")
    ap.add_argument("--mock", action="store_true", help="run against an in-memory fake card")
    args = ap.parse_args()

    if not args.mock and serial is None:
        print("pyserial is required:  pip install pyserial", file=sys.stderr)
        sys.exit(1)

    App(port=args.port, mock=args.mock).mainloop()


if __name__ == "__main__":
    main()
