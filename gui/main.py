import asyncio
import threading
import tkinter as tk
from tkinter import ttk, messagebox

from bleak import BleakClient, BleakScanner

DEVICE_NAME = "ESP_DEVICE"

SERVICE_UUID = "000000ff-0000-1000-8000-00805f9b34fb"
CHAR_UUID = "0000FF01-0000-1000-8000-00805F9B34FB"


class BLEGuiApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("ESP BLE Wi-Fi Sender")
        self.root.geometry("760x480")
        self.root.minsize(760, 480)
        self.root.resizable(False, False)

        self.client = None
        self.device = None
        self.connected = False

        self.loop = asyncio.new_event_loop()
        self.loop_thread = threading.Thread(target=self._run_loop, daemon=True)
        self.loop_thread.start()

        self._build_ui()
        self._set_status("Disconnected")

    def _build_ui(self):
        pad_x = 10
        pad_y = 8

        main = ttk.Frame(self.root, padding=12)
        main.pack(fill="both", expand=True)

        ttk.Label(main, text="Device:").grid(row=0, column=0, sticky="w", padx=pad_x, pady=pad_y)
        self.device_var = tk.StringVar(value=DEVICE_NAME)
        self.device_entry = ttk.Entry(main, textvariable=self.device_var, width=30)
        self.device_entry.grid(row=0, column=1, sticky="ew", padx=pad_x, pady=pad_y)

        self.scan_btn = ttk.Button(main, text="Scan", command=self.scan_device)
        self.scan_btn.grid(row=0, column=2, padx=pad_x, pady=pad_y)

        self.connect_btn = ttk.Button(main, text="Connect", command=self.connect_device)
        self.connect_btn.grid(row=1, column=2, padx=pad_x, pady=pad_y)

        ttk.Label(main, text="SSID:").grid(row=1, column=0, sticky="w", padx=pad_x, pady=pad_y)
        self.ssid_var = tk.StringVar()
        self.ssid_entry = ttk.Entry(main, textvariable=self.ssid_var, width=30)
        self.ssid_entry.grid(row=1, column=1, sticky="ew", padx=pad_x, pady=pad_y)

        ttk.Label(main, text="Password:").grid(row=2, column=0, sticky="w", padx=pad_x, pady=pad_y)
        self.pw_var = tk.StringVar()
        self.pw_entry = ttk.Entry(main, textvariable=self.pw_var, width=30)
        self.pw_entry.grid(row=2, column=1, sticky="ew", padx=pad_x, pady=pad_y)

        self.send_btn = ttk.Button(main, text="Send", command=self.send_credentials, state="disabled")
        self.send_btn.grid(row=2, column=2, padx=pad_x, pady=pad_y)

        self.disconnect_btn = ttk.Button(main, text="Disconnect", command=self.disconnect_device)
        self.disconnect_btn.grid(row=3, column=2, padx=pad_x, pady=pad_y)

        ttk.Label(main, text="Status:").grid(row=3, column=0, sticky="nw", padx=pad_x, pady=pad_y)
        self.status_var = tk.StringVar(value="Idle")
        self.status_label = ttk.Label(main, textvariable=self.status_var, wraplength=260)
        self.status_label.grid(row=3, column=1, sticky="w", padx=pad_x, pady=pad_y)

        ttk.Label(main, text="Log:").grid(row=4, column=0, sticky="nw", padx=pad_x, pady=pad_y)
        self.log_text = tk.Text(main, height=7, width=45)
        self.log_text.grid(row=4, column=1, columnspan=2, sticky="nsew", padx=pad_x, pady=pad_y)

        main.columnconfigure(1, weight=1)

    def _run_loop(self):
        asyncio.set_event_loop(self.loop)
        self.loop.run_forever()

    def _log(self, text: str):
        def update():
            self.log_text.insert("end", text + "\n")
            self.log_text.see("end")
        self.root.after(0, update)

    def _set_status(self, text: str):
        self.root.after(0, lambda: self.status_var.set(text))

    def _set_send_enabled(self, enabled: bool):
        state = "normal" if enabled else "disabled"
        self.root.after(0, lambda: self.send_btn.config(state=state))

    def _set_buttons_busy(self, busy: bool):
        scan_state = "disabled" if busy else "normal"
        connect_state = "disabled" if busy else "normal"
        self.root.after(0, lambda: self.scan_btn.config(state=scan_state))
        self.root.after(0, lambda: self.connect_btn.config(state=connect_state))

    def _run_async(self, coro):
        return asyncio.run_coroutine_threadsafe(coro, self.loop)

    def scan_device(self):
        self._set_buttons_busy(True)
        self._set_status("Scanning...")
        self._log("Scanning for BLE devices...")

        future = self._run_async(self._scan_device_async())

        def done_callback(fut):
            try:
                fut.result()
            except Exception as e:
                self._log(f"Scan error: {e}")
                self._set_status(f"Scan error: {e}")
            finally:
                self._set_buttons_busy(False)

        future.add_done_callback(done_callback)

    async def _scan_device_async(self):
        target_name = self.device_var.get().strip()
        devices = await BleakScanner.discover(timeout=5.0)

        self.device = None
        for d in devices:
            name = d.name or ""
            self._log(f"Found: {name} | {d.address}")
            if name == target_name:
                self.device = d

        if self.device:
            self._set_status(f"Found {self.device.name} ({self.device.address})")
            self._log(f"Target found: {self.device.name} | {self.device.address}")
        else:
            self._set_status(f"Device '{target_name}' not found")
            self._log(f"Device '{target_name}' not found")

    def connect_device(self):
        self._set_buttons_busy(True)
        self._set_status("Connecting...")
        self._log("Connecting...")

        future = self._run_async(self._connect_device_async())

        def done_callback(fut):
            try:
                fut.result()
            except Exception as e:
                self._log(f"Connect error: {e}")
                self._set_status(f"Connect error: {e}")
                self._set_send_enabled(False)
            finally:
                self._set_buttons_busy(False)

        future.add_done_callback(done_callback)

    async def _connect_device_async(self):
        target_name = self.device_var.get().strip()

        if self.connected and self.client and self.client.is_connected:
            self._set_status("Already connected")
            self._set_send_enabled(True)
            return

        if self.device is None:
            devices = await BleakScanner.discover(timeout=5.0)
            for d in devices:
                if (d.name or "") == target_name:
                    self.device = d
                    break

        if self.device is None:
            self._set_status(f"Device '{target_name}' not found")
            self._log(f"Cannot connect: device '{target_name}' not found")
            self._set_send_enabled(False)
            return

        self.client = BleakClient(self.device)

        await self.client.connect()

        if self.client.is_connected:
            self.connected = True
            self._set_status(f"Connected to {self.device.name}")
            self._log(f"Connected to {self.device.name} | {self.device.address}")
            self._set_send_enabled(True)
        else:
            self.connected = False
            self._set_status("Connection failed")
            self._log("Connection failed")
            self._set_send_enabled(False)

    def send_credentials(self):
        ssid = self.ssid_var.get().strip()
        password = self.pw_var.get().strip()

        if not ssid:
            messagebox.showwarning("Missing SSID", "Please enter SSID.")
            return

        payload = f'{{ssid="{ssid}",pw="{password}"}}'
        self._log(f"Sending: {payload}")
        self._set_status("Sending...")

        future = self._run_async(self._send_credentials_async(payload))

        def done_callback(fut):
            try:
                fut.result()
            except Exception as e:
                self._log(f"Send error: {e}")
                self._set_status(f"Send error: {e}")

        future.add_done_callback(done_callback)

    async def _send_credentials_async(self, payload: str):
        if not self.client or not self.client.is_connected:
            self.connected = False
            self._set_send_enabled(False)
            self._set_status("Not connected")
            self._log("Cannot send: not connected")
            return

        data = payload.encode("utf-8")
        await self.client.write_gatt_char(CHAR_UUID, data, response=True)
        self._set_status("Send successful")
        self._log("Send successful")

    def disconnect_device(self):
        future = self._run_async(self._disconnect_device_async())

        def done_callback(fut):
            try:
                fut.result()
            except Exception as e:
                self._log(f"Disconnect error: {e}")

        future.add_done_callback(done_callback)

    async def _disconnect_device_async(self):
        if self.client and self.client.is_connected:
            await self.client.disconnect()

        self.client = None
        self.device = None
        self.connected = False
        self._set_send_enabled(False)
        self._set_status("Disconnected")
        self._log("Disconnected")

    def on_close(self):
        try:
            future = self._run_async(self._disconnect_device_async())
            future.result(timeout=3)
        except Exception:
            pass

        self.loop.call_soon_threadsafe(self.loop.stop)
        self.root.destroy()


if __name__ == "__main__":
    root = tk.Tk()
    app = BLEGuiApp(root)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop()