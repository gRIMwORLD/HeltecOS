import tkinter as tk
from tkinter import ttk
import urllib.request
import json
import threading
import time

DEVICE_IP = "10.11.20.105"

class HeltecCompanionApp:
    def __init__(self, root):
        self.root = root
        self.root.title("HeltecOS | PC Companion")
        self.root.geometry("480x420")
        self.root.minsize(440, 380)
        self.root.configure(bg="#0f172a")

        self.running = True

        # Header
        header_frame = tk.Frame(root, bg="#0f172a")
        header_frame.pack(fill=tk.X, padx=20, pady=(16, 12))

        title_lbl = tk.Label(header_frame, text="HELTECOS COMPANION", font=("Segoe UI", 14, "bold"), fg="#38bdf8", bg="#0f172a")
        title_lbl.pack(side=tk.LEFT)

        self.status_lbl = tk.Label(header_frame, text="● Connecting...", font=("Segoe UI", 10), fg="#facc15", bg="#0f172a")
        self.status_lbl.pack(side=tk.RIGHT)

        # Card Grid
        grid_frame = tk.Frame(root, bg="#0f172a")
        grid_frame.pack(fill=tk.BOTH, expand=True, padx=20, pady=8)
        grid_frame.columnconfigure(0, weight=1)
        grid_frame.columnconfigure(1, weight=1)

        # Temp Card
        self.card_temp = self.create_card(grid_frame, 0, 0, "TEMPERATURE", "--.- °C", "#f97316", "BME280 Ambient Sensor")
        # Hum Card
        self.card_hum = self.create_card(grid_frame, 0, 1, "HUMIDITY", "--.- %", "#06b6d4", "Precision Hygrometer")
        # Pressure Card
        self.card_pres = self.create_card(grid_frame, 1, 0, "BAROMETER", "----.- hPa", "#8b5cf6", "Altitude: -- m")
        # Battery Card
        self.card_bat = self.create_card(grid_frame, 1, 1, "BATTERY", "-- %", "#10b981", "-- V | -- mA")

        # Controls Panel
        btn_frame = tk.Frame(root, bg="#1e293b", bd=1, relief=tk.SOLID)
        btn_frame.pack(fill=tk.X, padx=20, pady=(10, 16))

        tk.Label(btn_frame, text="REMOTE CONTROLS", font=("Segoe UI", 9, "bold"), fg="#94a3b8", bg="#1e293b").pack(anchor=tk.W, padx=12, pady=(8, 4))
        
        btns_sub = tk.Frame(btn_frame, bg="#1e293b")
        btns_sub.pack(fill=tk.X, padx=12, pady=(0, 10))

        self.btn_oled = tk.Button(btns_sub, text="Toggle OLED", command=lambda: self.send_cmd("oled_toggle"), bg="#0284c7", fg="white", font=("Segoe UI", 9, "bold"), bd=0, padx=10, pady=6, cursor="hand2")
        self.btn_oled.pack(side=tk.LEFT, padx=(0, 8))

        self.btn_led = tk.Button(btns_sub, text="Toggle LED", command=lambda: self.send_cmd("led_toggle"), bg="#334155", fg="white", font=("Segoe UI", 9, "bold"), bd=0, padx=10, pady=6, cursor="hand2")
        self.btn_led.pack(side=tk.LEFT, padx=(0, 8))

        self.btn_page = tk.Button(btns_sub, text="Next Screen", command=lambda: self.send_cmd("screen_next"), bg="#334155", fg="white", font=("Segoe UI", 9, "bold"), bd=0, padx=10, pady=6, cursor="hand2")
        self.btn_page.pack(side=tk.LEFT)

        # Start background polling thread
        self.poll_thread = threading.Thread(target=self.poll_telemetry, daemon=True)
        self.poll_thread.start()

    def create_card(self, parent, row, col, label_text, default_val, accent_color, sub_text):
        card = tk.Frame(parent, bg="#1e293b", bd=1, relief=tk.SOLID)
        card.grid(row=row, column=col, padx=6, pady=6, sticky="nsew")

        # Accent bar
        accent = tk.Frame(card, bg=accent_color, height=3)
        accent.pack(fill=tk.X, side=tk.TOP)

        content = tk.Frame(card, bg="#1e293b")
        content.pack(fill=tk.BOTH, expand=True, padx=12, pady=10)

        lbl = tk.Label(content, text=label_text, font=("Segoe UI", 9, "bold"), fg="#94a3b8", bg="#1e293b")
        lbl.pack(anchor=tk.W)

        val_lbl = tk.Label(content, text=default_val, font=("Segoe UI", 20, "bold"), fg="#f8fafc", bg="#1e293b")
        val_lbl.pack(anchor=tk.W, pady=(4, 2))

        sub_lbl = tk.Label(content, text=sub_text, font=("Segoe UI", 8), fg="#94a3b8", bg="#1e293b")
        sub_lbl.pack(anchor=tk.W)

        return {"val": val_lbl, "sub": sub_lbl}

    def poll_telemetry(self):
        while self.running:
            try:
                url = f"http://{DEVICE_IP}/api/telemetry?t={int(time.time()*1000)}"
                req = urllib.request.Request(url, headers={"User-Agent": "HeltecCompanion/1.0"})
                with urllib.request.urlopen(req, timeout=1.5) as resp:
                    data = json.loads(resp.read().decode())
                
                # Update UI via main thread
                self.root.after(0, self.update_ui, data)
            except Exception as e:
                self.root.after(0, self.update_error, str(e))
            time.sleep(1.0)

    def update_ui(self, data):
        self.status_lbl.config(text="● Connected (Live)", fg="#4ade80")

        # Temp
        self.card_temp["val"].config(text=f"{data.get('temp_c', 0.0):.1f} °C")

        # Humidity
        self.card_hum["val"].config(text=f"{data.get('humidity', 0.0):.1f} %")

        # Pressure & Altitude
        pres = data.get("pressure", 0.0)
        alt = 44330.0 * (1.0 - (pres / 1013.25) ** 0.1903)
        self.card_pres["val"].config(text=f"{pres:.1f} hPa")
        self.card_pres["sub"].config(text=f"Altitude: {alt:.0f} m")

        # Battery
        bat_v = data.get("battery_v", 0.0)
        bat_pct = data.get("battery_pct", 0)
        current_ma = data.get("current_ma", 0.0)
        is_charging = data.get("charging", False)
        is_full = data.get("full", False)

        state_str = "Full" if is_full else ("Charging" if is_charging else "Discharging")
        sign = "+" if current_ma >= 0 else ""
        self.card_bat["val"].config(text=f"{bat_pct} %")
        self.card_bat["sub"].config(text=f"{bat_v:.2f}V | {sign}{current_ma:.0f}mA ({state_str})")

    def update_error(self, err_msg):
        self.status_lbl.config(text="● Disconnected", fg="#f87171")

    def send_cmd(self, cmd):
        def _send():
            try:
                url = f"http://{DEVICE_IP}/api/action?cmd={cmd}"
                urllib.request.urlopen(url, timeout=1.5)
            except Exception as e:
                print(f"Command error: {e}")
        threading.Thread(target=_send, daemon=True).start()

if __name__ == "__main__":
    root = tk.Tk()
    app = HeltecCompanionApp(root)
    root.mainloop()
