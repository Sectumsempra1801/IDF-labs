import asyncio
import threading
import tkinter as tk
from tkinter import ttk, messagebox

from bleak import BleakClient, BleakScanner

# GATT Characteristic UUIDs
SERVICE_UUID = "000000ff-0000-1000-8000-00805f9b34fb"
CHAR_UUID = "0000FF01-0000-1000-8000-00805F9B34FB"

class BLEGuiApp:
    def __init__(self, root: tk.Tk):
        self.root = root
        self.root.title("ESP BLE Wi-Fi & IP Provisioner")
        self.root.geometry("650x500")
        self.root.minsize(650, 500)
        self.root.resizable(False, False)

        self.client = None
        self.connected = False
        self.is_connecting = False
        self.manual_disconnect = False
        self.scanned_devices = {} 

        self.loop = asyncio.new_event_loop()
        self.loop_thread = threading.Thread(target=self._run_loop, daemon=True)
        self.loop_thread.start()

        self._build_ui()
        self.check_connection_status()

    def _build_ui(self):
        pad_x, pad_y = 10, 8
        main = ttk.Frame(self.root, padding=12)
        main.pack(fill="both", expand=True)

        # Register validation command
        vcmd = (self.root.register(self.validate_input), '%P')

        # --- Device Selection --- (Row 0)
        ttk.Label(main, text="Select Device     :").place(x=0, y=0)
        self.device_selector = ttk.Combobox(main, state="readonly", width=50)
        self.device_selector.place(x=100, y=0)
        self.device_selector.set("Click 'Scan' to find devices...")

        self.scan_btn = ttk.Button(main, text="Scan", command=self.scan_device)
        self.scan_btn.place(x=450, y=-2)

        self.connect_btn = ttk.Button(main, text="Connect BLE", command=self.connect_device)
        self.connect_btn.place(x=550, y=-2)

        # --- SSID --- (Row 1)
        ttk.Label(main, text="SSID (max 32)    :").place(x = 0, y = 30)
        self.ssid_var = tk.StringVar()
        self.ssid_entry = ttk.Entry(main, textvariable=self.ssid_var, width=50, validate="key", validatecommand=vcmd)
        self.ssid_entry.place(x=100, y=30)
        self.ssid_entry.insert(0,"Tam")

        # --- Password --- (Row 3)
        ttk.Label(main, text="Password (8-32):").place(x=0,y=60)
        self.pw_var = tk.StringVar()
        self.pw_entry = ttk.Entry(main, textvariable=self.pw_var, width=50, validate="key", validatecommand=vcmd) 
        self.pw_entry.place(x=100, y=60)
        self.pw_entry.insert(0,"o903903126")

        self.send_wifi_btn = ttk.Button(main, text="Send Wi-Fi", command=self.send_credentials, state="disabled")
        self.send_wifi_btn.place(x=450, y=58)

        # --- Server IP --- (Row 4) 
        ttk.Label(main, text="Server IP             :").place(x=0,y=90)
        self.ip_var = tk.StringVar()
        self.ip_entry = ttk.Entry(main, textvariable=self.ip_var, width=50) 
        self.ip_entry.place(x=100, y=90)

        self.send_ip_btn = ttk.Button(main, text="Send IP", command=self.send_server_ip, state="disabled")
        self.send_ip_btn.place(x=450,y=88)

         # --- Status --- (Row 5)
        ttk.Label(main, text="URL                     :").place(x=0,y=120)
        self.url_var = tk.StringVar()
        self.url_entry = ttk.Entry(main, textvariable=self.url_var, width=50) 
        self.url_entry.place(x=100,y=120)
        self.url_entry.insert(0, "http://httpforever.com/") 
 
        self.send_url_btn = ttk.Button(main, text="Send URL", command=self.send_url, state="disabled")
        self.send_url_btn.place(x=450,y=118)

        self.get_method_btn =  ttk.Button(main, text="GET", command=self.get_method, state="disabled")
        self.get_method_btn.place(x=550,y=118)
        # --- Status --- (Row 6)
        ttk.Label(main, text="Status:").place(x=0, y=150)
        self.status_var = tk.StringVar(value="DISCONNECTED")
        self.status_label = ttk.Label(main, textvariable=self.status_var, font=("Arial", 10, "bold"), foreground="red")
        self.status_label.place(x=100,y=150)

        self.disconnect_btn = ttk.Button(main, text="Disconnect", command=self.disconnect_device, state="disabled")
        self.disconnect_btn.place(x=450,y=150)

        # --- Log --- (Row 7)
        self.log_text = tk.Text(main, height=12, width=70)
        self.log_text.tag_config("error", foreground="red")
        self.log_text.tag_config("success", foreground="green")
        self.log_text.tag_config("info", foreground="blue")
        self.log_text.place(x=0,y=180)

        main.columnconfigure(1, weight=0)

    def validate_input(self, new_value):
        """Logic to restrict entry to 32 characters maximum."""
        return len(new_value) <= 32

    def _log(self, text: str, tag=None):
        def append():
            self.log_text.insert("end", f"> {text}\n", tag)
            self.log_text.see("end")
        self.root.after(0, append)

    def check_connection_status(self):
        if self.is_connecting:
            self.root.after(5000, self.check_connection_status)
            return

        actual_connected = bool(self.client and self.client.is_connected)
        
        if actual_connected != self.connected:
            if not actual_connected:
                if not self.manual_disconnect:
                    self._log("BLE connection lost with the device", "error")
                    self.status_var.set("CONNECTION LOST")
                else:
                    self._log("Disconnected from device", "info")
                    self.status_var.set("DISCONNECTED")

                self.device_selector.config(state="normal")
                self.status_label.config(foreground="red")
                self.send_wifi_btn.config(state="disabled")
                self.send_ip_btn.config(state="disabled")
                self.send_url_btn.config(state="disabled")
                self.get_method_btn.config(state="disabled")
                self.disconnect_btn.config(state="disabled")
                self.connect_btn.config(state="normal")
            else:
                self.status_var.set("CONNECTED")
                self.device_selector.config(state="disabled")
                self.status_label.config(foreground="green")
                self.send_wifi_btn.config(state="normal")
                self.send_url_btn.config(state="normal")
                self.get_method_btn.config(state="normal")
                self.send_ip_btn.config(state="normal")
                self.disconnect_btn.config(state="normal")
                self.connect_btn.config(state="disabled")
            
            self.connected = actual_connected
        
        self.root.after(5000, self.check_connection_status)

    def _run_loop(self):
        asyncio.set_event_loop(self.loop)
        self.loop.run_forever()

    def _run_async(self, coro):
        return asyncio.run_coroutine_threadsafe(coro, self.loop)

    def scan_device(self):
        self._log("Scanning for devices...")
        self._run_async(self._scan_device_async())

    async def _scan_device_async(self):
        try:
            devices = await BleakScanner.discover(timeout=4.0)
            self.scanned_devices = { (d.name if d.name and d.name.strip() else d.address): d for d in devices }
            display_list = list(self.scanned_devices.keys())
            self.root.after(0, lambda: self.device_selector.config(values=display_list))
            if display_list:
                self.root.after(0, lambda: self.device_selector.current(0))
                self._log(f"Found {len(display_list)} devices.", "success")
        except Exception as e:
            self._log(f"Scan error: {e}", "error")

    def connect_device(self):
        if self.is_connecting or self.connected: return
        selection = self.device_selector.get()
        if selection not in self.scanned_devices: 
            self._log("Please select a device from the list", "error")
            return

        self.is_connecting = True
        self.manual_disconnect = False
        self.connect_btn.config(state="disabled")
        self.status_var.set("CONNECTING...")
        self.status_label.config(foreground="orange")
        
        device_obj = self.scanned_devices[selection]
        self._run_async(self._connect_with_timeout(device_obj))

    async def _connect_with_timeout(self, device):
        try:
            self._log(f"Connecting to {device.address} (Timeout: 7s)...", "info")
            self.client = BleakClient(device)
            await asyncio.wait_for(self.client.connect(), timeout=7.0)
            self._log(f"Connected to {device.name if device.name else device.address}", "success")
        except asyncio.TimeoutError:
            self._log("Connection timed out after 7 seconds.", "error")
            self._reset_to_disconnected()
        except Exception as e:
            self._log(f"Connection failed: {str(e)}", "error")
            self._reset_to_disconnected()
        finally:
            self.is_connecting = False

    def _reset_to_disconnected(self):
        self.client = None
        self.root.after(0, lambda: [
            self.status_var.set("DISCONNECTED"),
            self.status_label.config(foreground="red"),
            self.connect_btn.config(state="normal")
        ])

    def disconnect_device(self):
        if not self.client: return
        self.manual_disconnect = True
        self._run_async(self._disconnect_device_async())

    async def _disconnect_device_async(self):
        try:
            if self.client: await self.client.disconnect()
        finally:
            self.client = None

    # --- HÀM GỬI WI-FI ---
    def send_credentials(self):
        ssid, pw = self.ssid_var.get().strip(), self.pw_var.get().strip()
        if not ssid:
            self._log("Error: SSID cannot be empty", "error")
            return
        if len(pw) < 8:
            self._log(f"Error: Password too short ({len(pw)}/min 8 chars)", "error")
            return
            
        payload = f'ssid="{ssid}" pw="{pw}"'
        self._log(f"Sending Wi-Fi: {payload}")
        self._run_async(self._send_payload_async(payload))


    def send_server_ip(self):
        ip_addr = self.ip_var.get().strip()
        if not ip_addr:
            self._log("Error: Server IP cannot be empty", "error")
            return
            

        payload = f'ip="{ip_addr}"'
        self._log(f"Sending IP: {payload}")
        self._run_async(self._send_payload_async(payload))

    def send_url(self):
        url = self.url_var.get().strip()
        if not url:
            self._log("Error: URL cannot be empty","error")
            return
        
        payload =f'url="{url}"'
        self._log(f"Sending URL: {payload}")
        self._run_async(self._send_payload_async(payload))

    def get_method (self):
        payload = 'method="GET"'
        self._log(f"The request's query parameters.")
        self._run_async(self._send_payload_async(payload))

    async def _send_payload_async(self, payload: str):
        if self.client and self.client.is_connected:
            try:
                await self.client.write_gatt_char(CHAR_UUID, payload.encode(), response=True)
                self._log("Transfer successful!", "success")
            except Exception as e:
                self._log(f"Transfer failed: {e}", "error")
        else:
            self._log("Not connected to device", "error")

    def on_close(self):
        self.loop.call_soon_threadsafe(self.loop.stop)
        self.root.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = BLEGuiApp(root)
    root.protocol("WM_DELETE_WINDOW", app.on_close)
    root.mainloop(),