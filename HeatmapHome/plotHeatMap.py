import tkinter as tk
from tkinter import filedialog, messagebox
import requests
import threading
import time
import matplotlib.pyplot as plt
import numpy as np
from PIL import Image, ImageTk
import io

running = False
data_points = []  # stores (x, y, rssi) in ORIGINAL image coords
ESP32_IP = None
latest_rssi = None
floorplan_img = None
floorplan_canvas = None
canvas_img_obj = None
canvas_w, canvas_h = 500, 400  # fixed preview size

def get_rssi_url():
    return f"http://{ESP32_IP}/rssi"

def check_connection():
    try:
        r = requests.get(get_rssi_url(), timeout=2)
        return r.status_code == 200
    except:
        return False

def collect_data():
    global running, latest_rssi
    while running:
        try:
            r = requests.get(get_rssi_url(), timeout=2)
            latest_rssi = int(r.text)
            rssi_label.config(text=f"RSSI: {latest_rssi} dBm")
        except:
            rssi_label.config(text="ESP32 not responding...")
        time.sleep(2)

def start_mapping():
    global running, data_points, ESP32_IP
    ESP32_IP = ip_entry.get().strip()
    if not ESP32_IP:
        messagebox.showerror("Error", "Please enter ESP32 IP address.")
        return

    if not check_connection():
        messagebox.showerror("Error", f"ESP32 not found at {ESP32_IP}")
        return

    running = True
    data_points = []
    threading.Thread(target=collect_data, daemon=True).start()
    status_label.config(text=f"Mapping started at {ESP32_IP}...")

def stop_mapping():
    global running
    running = False
    status_label.config(text="Stopped. Heatmap saved as wifi_heatmap.png")
    if data_points:
        save_heatmap("wifi_heatmap.png")

def save_heatmap(filename="wifi_heatmap.png"):
    """Save final heatmap to file at ORIGINAL image scale"""
    if not data_points or not floorplan_img:
        return

    x = [d[0] for d in data_points]
    y = [d[1] for d in data_points]
    z = [d[2] for d in data_points]

    w, h = floorplan_img.size
    plt.imshow(floorplan_img, extent=[0, w, h, 0])
    plt.tricontourf(x, y, z, levels=20, cmap="RdYlGn", alpha=0.6)
    plt.colorbar(label="WiFi RSSI (dBm)")
    plt.scatter(x, y, c="black", marker="x")
    plt.title("Home WiFi Heatmap")
    plt.savefig(filename, dpi=300)
    plt.close()

def update_live_heatmap():
    """Redraw live heatmap preview on resized canvas"""
    if not data_points or not floorplan_img:
        return

    x = [d[0] for d in data_points]
    y = [d[1] for d in data_points]
    z = [d[2] for d in data_points]

    w, h = floorplan_img.size
    plt.imshow(floorplan_img, extent=[0, w, h, 0])
    plt.tricontourf(x, y, z, levels=20, cmap="RdYlGn", alpha=0.6)
    plt.scatter(x, y, c="black", marker="x")
    plt.axis("off")

    # Save to memory
    buf = io.BytesIO()
    plt.savefig(buf, format="png")
    buf.seek(0)
    live_img = Image.open(buf)
    live_img = live_img.resize((canvas_w, canvas_h))
    tk_img = ImageTk.PhotoImage(live_img)

    floorplan_canvas.create_image(0, 0, anchor="nw", image=tk_img)
    floorplan_canvas.image = tk_img
    plt.close()

def load_floorplan():
    global floorplan_img, canvas_img_obj
    file_path = filedialog.askopenfilename(filetypes=[("Image Files", "*.png;*.jpg;*.jpeg")])
    if not file_path:
        return
    floorplan_img = Image.open(file_path)
    img_resized = floorplan_img.resize((canvas_w, canvas_h))
    tk_img = ImageTk.PhotoImage(img_resized)
    canvas_img_obj = floorplan_canvas.create_image(0, 0, anchor="nw", image=tk_img)
    floorplan_canvas.image = tk_img
    status_label.config(text="Floorplan loaded. Click to record points.")

def on_canvas_click(event):
    global latest_rssi
    if latest_rssi is None:
        messagebox.showwarning("Warning", "No RSSI value yet.")
        return

    if not floorplan_img:
        messagebox.showwarning("Warning", "Please load a floorplan first.")
        return

    # Scale canvas click to original image coords
    w, h = floorplan_img.size
    scale_x = w / canvas_w
    scale_y = h / canvas_h
    orig_x = int(event.x * scale_x)
    orig_y = int(event.y * scale_y)

    data_points.append((orig_x, orig_y, latest_rssi))
    print(f"Point recorded: x={orig_x}, y={orig_y}, RSSI={latest_rssi}")
    update_live_heatmap()

# GUI
root = tk.Tk()
root.title("ESP32 WiFi Heatmap")

tk.Label(root, text="ESP32 IP Address:").pack(pady=5)
ip_entry = tk.Entry(root, width=20)
ip_entry.pack(pady=5)
ip_entry.insert(0, "192.168.1.50")

start_button = tk.Button(root, text="Start Mapping", command=start_mapping)
start_button.pack(pady=5)

stop_button = tk.Button(root, text="Stop & Save", command=stop_mapping)
stop_button.pack(pady=5)

rssi_label = tk.Label(root, text="RSSI: -- dBm")
rssi_label.pack(pady=5)

status_label = tk.Label(root, text="Not started")
status_label.pack(pady=5)

load_button = tk.Button(root, text="Load Floorplan", command=load_floorplan)
load_button.pack(pady=5)

floorplan_canvas = tk.Canvas(root, width=canvas_w, height=canvas_h, bg="lightgray")
floorplan_canvas.pack(pady=5)
floorplan_canvas.bind("<Button-1>", on_canvas_click)

root.mainloop()
