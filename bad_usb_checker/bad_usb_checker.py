import tkinter as tk
from tkinter import ttk, messagebox
import threading, time, random, string
import serial, serial.tools.list_ports as list_ports

ESPRESSIF_VID = 0x303A
BAUD = 115200
BANNER = "READY:USB_CABLE_TESTER"
READ_TIMEOUT = 1.0

ACCENT_OK = "#16a34a"
ACCENT_FAIL = "#dc2626"
ACCENT_INFO = "#2563eb"
BG_DARK = "#0b1220"
FG_TEXT = "#e5e7eb"

LED_MODES = ("NEOPIXEL", "AUTO", "SIMPLE")

def list_all_ports():
    return list_ports.comports()

class CableTesterApp(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("USB Cable Tester (ESP32-C6)")
        self.geometry("680x380")
        self.resizable(False, False)
        self.configure(bg=BG_DARK)

        # ttk theme + styles
        self.style = ttk.Style(self)
        for theme in ("clam","alt","default"):
            try: self.style.theme_use(theme); break
            except: pass
        self.style.configure("Root.TFrame", background=BG_DARK)
        self.style.configure("TFrame", background=BG_DARK)
        self.style.configure("TLabel", background=BG_DARK, foreground=FG_TEXT, font=("Segoe UI", 11))
        self.style.configure("Header.TLabel", background=BG_DARK, foreground=FG_TEXT, font=("Segoe UI", 16, "bold"))
        self.style.configure("TButton", font=("Segoe UI", 11, "bold"))
        self.style.configure("BadgeNeutral.TLabel", background="#1f2937", foreground="#e5e7eb", padding=6)
        self.style.configure("BadgeOk.TLabel",      background=ACCENT_OK, foreground="white", padding=6)
        self.style.configure("BadgeFail.TLabel",    background=ACCENT_FAIL, foreground="white", padding=6)
        self.style.configure("BannerNeutral.TLabel", background=BG_DARK, foreground="#d1d5db", padding=4)
        self.style.configure("BannerOk.TLabel",      background=BG_DARK, foreground=ACCENT_OK, padding=4)
        self.style.configure("BannerFail.TLabel",    background=BG_DARK, foreground=ACCENT_FAIL, padding=4)
        self.style.configure("BannerInfo.TLabel",    background=BG_DARK, foreground=ACCENT_INFO, padding=4)

        # state
        self.ser = None
        self.connected_port = None
        self.status_var = tk.StringVar(value="Disconnected")
        self.port_var = tk.StringVar(value="(Auto)")
        self.banner_var = tk.StringVar(value="")
        self.running_var = tk.StringVar(value="")
        self.led_mode = tk.StringVar(value="NEOPIXEL")

        self.test_secs = tk.IntVar(value=3)
        self.payload_len = tk.IntVar(value=32)

        self.vars = {k: tk.StringVar(value="❌") for k in ["+5V","GND","TX","RX"]}

        self._build_ui()
        self._refresh_ports()
        self.after(300, self.auto_connect_bg)

    # ---------- UI ----------
    def _build_ui(self):
        root = ttk.Frame(self, padding=14, style="Root.TFrame"); root.pack(fill="both", expand=True)

        ttk.Label(root, text="USB Cable Tester", style="Header.TLabel").pack(anchor="w", pady=(0,6))

        # Top row: port select, LED mode, reconnect
        top = ttk.Frame(root, style="TFrame"); top.pack(fill="x", pady=(4,8))

        # Port chooser
        ttk.Label(top, text="Port:").pack(side="left", padx=(0,6))
        self.port_combo = ttk.Combobox(top, state="readonly", width=24, textvariable=self.port_var)
        self.port_combo.pack(side="left")
        self.port_combo.bind("<<ComboboxSelected>>", self._on_port_selected)

        ttk.Button(top, text="Refresh", command=self._refresh_ports).pack(side="left", padx=(6,10))

        # LED mode
        ttk.Label(top, text="LED Mode:").pack(side="left", padx=(0,6))
        mode_combo = ttk.Combobox(top, values=LED_MODES, state="readonly", width=12, textvariable=self.led_mode)
        mode_combo.pack(side="left")
        mode_combo.bind("<<ComboboxSelected>>", self._on_mode_change)

        ttk.Button(top, text="Reconnect", command=self.reconnect_bg).pack(side="right")

        # Check cards
        checks = ttk.Frame(root, style="TFrame"); checks.pack(fill="x", pady=(4,8))
        for k in ["+5V","GND","TX","RX"]:
            card = ttk.Frame(checks, padding=10, style="TFrame")
            card.pack(side="left", expand=True, fill="x", padx=6)
            ttk.Label(card, text=k).pack(anchor="w")
            ttk.Label(card, textvariable=self.vars[k], style="Header.TLabel").pack(anchor="center", pady=(8,2))

        # Test controls
        controls = ttk.Frame(root, style="TFrame"); controls.pack(fill="x", pady=(2,6))
        ttk.Label(controls, text="Duration (s):").pack(side="left", padx=(0,6))
        tk.Spinbox(controls, from_=1, to=300, textvariable=self.test_secs, width=6).pack(side="left", padx=(0,16))
        ttk.Label(controls, text="Payload (bytes):").pack(side="left", padx=(0,6))
        tk.Spinbox(controls, from_=1, to=1024, textvariable=self.payload_len, width=6).pack(side="left", padx=(0,16))

        # Banner + buttons
        mid = ttk.Frame(root, style="TFrame"); mid.pack(fill="x", pady=(4,2))
        self.banner = ttk.Label(mid, textvariable=self.banner_var, style="BannerNeutral.TLabel", anchor="center")
        self.banner.pack(fill="x", pady=(0,6))
        self._set_banner("Waiting for device…", "neutral")

        btns = ttk.Frame(root, style="TFrame"); btns.pack(fill="x", pady=(2,0))
        self.run_btn = ttk.Button(btns, text="▶ Run Test", command=self.run_test_bg, state="disabled")
        self.run_btn.pack(side="left")
        ttk.Button(btns, text="Quit", command=self.destroy).pack(side="right")

        foot = ttk.Frame(root, style="TFrame"); foot.pack(fill="x", pady=(10,0))
        ttk.Label(foot, text="LED: Blue=running • Green=pass • Red=fail • OFF=idle").pack(side="left")
        ttk.Label(foot, textvariable=self.running_var, foreground="#93c5fd").pack(side="right")

        # Status badges
        status_row = ttk.Frame(root, style="TFrame"); status_row.pack(fill="x", pady=(6,0))
        ttk.Label(status_row, text="Status:").pack(side="left", padx=(0,6))
        self.status_lbl = ttk.Label(status_row, textvariable=self.status_var, style="BadgeNeutral.TLabel")
        self.status_lbl.pack(side="left", padx=(0,8))

    def _set_status_style(self, mode="neutral"):
        style = {
            "neutral": "BadgeNeutral.TLabel",
            "ok": "BadgeOk.TLabel",
            "fail": "BadgeFail.TLabel",
        }[mode]
        self.status_lbl.configure(style=style)

    def _set_banner(self, text, mode="neutral"):
        self.banner_var.set(text)
        style = {
            "neutral": "BannerNeutral.TLabel",
            "ok": "BannerOk.TLabel",
            "fail": "BannerFail.TLabel",
            "info": "BannerInfo.TLabel",
        }[mode]
        self.banner.configure(style=style)

    # ---------- Ports / Connect ----------
    def _refresh_ports(self):
        ports = list_all_ports()
        items = ["(Auto)"] + [f"{p.device} — {p.description}" for p in ports]
        self.port_combo["values"] = items
        # keep selection if possible
        cur = self.port_var.get()
        if cur not in items:
            self.port_var.set("(Auto)")

    def _on_port_selected(self, _evt):
        # reconnect using selected choice
        self.reconnect_bg()

    def _pick_port(self):
        choice = self.port_var.get()
        # manual choice
        if choice and choice != "(Auto)":
            # extract device before " — "
            return choice.split(" — ")[0]
        # auto: prefer Espressif VID, else first available
        esp = [p.device for p in list_all_ports() if p.vid == ESPRESSIF_VID]
        if esp: return esp[0]
        allp = [p.device for p in list_all_ports()]
        return allp[0] if allp else None

    def auto_connect_bg(self):
        threading.Thread(target=self._connect_thread, daemon=True).start()

    def reconnect_bg(self):
        if self.ser:
            try: self.ser.close()
            except: pass
        self.ser = None
        self.status_var.set("Reconnecting…"); self._set_status_style("neutral")
        for k in self.vars: self.vars[k].set("❌")
        self._set_banner("Reconnecting…", "neutral")
        self.run_btn.config(state="disabled")
        self.auto_connect_bg()

    def _connect_thread(self):
        dev = self._pick_port()
        if not dev:
            self.status_var.set("Disconnected"); self._set_status_style("neutral")
            self._set_banner("No serial ports found", "fail")
            return
        try:
            ser = serial.Serial(dev, BAUD, timeout=READ_TIMEOUT)
        except Exception as e:
            self.status_var.set("Disconnected"); self._set_status_style("fail")
            self._set_banner(f"Open failed: {e}", "fail")
            return

        self.ser = ser
        self.connected_port = dev
        self.status_var.set("Connected"); self._set_status_style("ok")
        self._set_banner("Device ready", "ok")
        self.vars["+5V"].set("✅"); self.vars["GND"].set("✅")
        self.run_btn.config(state="normal")

        # nudge banner + set LED mode
        try: self._send("\n")
        except: pass
        self._apply_led_mode()

    # ---------- I/O ----------
    def _send(self, line):
        try:
            self.ser.write((line if line.endswith("\n") else line+"\n").encode())
            self.ser.flush()
            return True
        except Exception:
            return False

    def _read_until_contains(self, expect: bytes, timeout_s: float):
        t0=time.time(); buf=b""
        while time.time()-t0<timeout_s:
            if self.ser.in_waiting:
                buf += self.ser.read(self.ser.in_waiting)
                if expect in buf: return True, buf
            else:
                time.sleep(0.01)
        return False, buf

    # ---------- LED mode ----------
    def _apply_led_mode(self):
        mode = self.led_mode.get().upper()
        self._send(f"CONF:LED:{mode}")

    def _on_mode_change(self, _evt):
        if self.ser: self._apply_led_mode()

    # ---------- Test run (timed echo) ----------
    def run_test_bg(self):
        self.run_btn.config(state="disabled")
        threading.Thread(target=self._run_test_thread, daemon=True).start()

    def _spinner(self, stop):
        dots=""
        while not stop.is_set():
            dots = "." if dots=="..." else dots+"."
            self.running_var.set(f"Running{dots}")
            time.sleep(0.25)
        self.running_var.set("")

    def _mk_payload(self, n):
        # visible ASCII only to keep line-based protocol simple
        alphabet = string.ascii_letters + string.digits
        return ''.join(random.choice(alphabet) for _ in range(n))

    def _run_test_thread(self):
        for k in ("TX","RX"):
            self.vars[k].set("❌")
        if not self.ser:
            self.status_var.set("Disconnected"); self.run_btn.config(state="normal")
            messagebox.showerror("Not connected", "ESP32-C6 not connected."); return

        stop = threading.Event()
        threading.Thread(target=self._spinner, args=(stop,), daemon=True).start()

        # Start: set BLUE
        self._set_banner("Testing (echo round-trips)…", "info")
        self._send("LED:BLUE")
        self._send("RUN")

        secs = max(1, int(self.test_secs.get()))
        plen = max(1, int(self.payload_len.get()))

        start = time.time()
        rounds = 0
        errors = 0
        bytes_ok = 0

        # first TX sanity
        tx_sane = self._send("PING:HELLO")
        self.vars["TX"].set("✅" if tx_sane else "❌")

        while time.time() - start < secs:
            payload = self._mk_payload(plen)
            line = f"ECHO:{payload}"
            sent = self._send(line)
            if not sent:
                errors += 1
                continue
            ok, buf = self._read_until_contains(f"ECHO:{payload}".encode(), timeout_s=1.2)
            if ok:
                rounds += 1
                bytes_ok += len(payload)
                self.vars["RX"].set("✅")
            else:
                errors += 1

        stop.set()

        # Summary + LED color
        all_pass = (self.vars["+5V"].get()=="✅" and self.vars["GND"].get()=="✅" and errors==0 and rounds>0)
        self._send("LED:GREEN" if all_pass else "LED:RED")
        self.status_var.set("Passed" if all_pass else "Failed")
        self._set_status_style("ok" if all_pass else "fail")

        duration = max(0.001, time.time()-start)
        rps = rounds / duration
        bps = (bytes_ok / duration)
        self._set_banner(f"{'Passed ✅' if all_pass else 'Failed ❌'} — {rounds} rtts in {duration:.2f}s, {rps:.1f} rtt/s, {bps:.0f} B/s", "ok" if all_pass else "fail")

        self.run_btn.config(state="normal")

if __name__ == "__main__":
    app = CableTesterApp()
    app.mainloop()
