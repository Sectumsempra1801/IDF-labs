import tkinter as tk
from tkinter import ttk, messagebox, scrolledtext
import asyncio, threading
from bleak import BleakClient, BleakScanner
from datetime import datetime
from functools import reduce

# Cấu hình UUID và CRC8
UART_TX_UUID = "0000FF01-0000-1000-8000-00805F9B34FB"
CMD_HEADER = bytearray.fromhex('aa')

def crc8(payload: bytearray):
    POLYNOMIAL, MSBIT = 0x85, 0x80
    def crc1(crc, step=0):
        if step >= 8: return crc & 0xff
        return crc1(crc << 1 ^ POLYNOMIAL, step+1) if crc & MSBIT else crc1(crc << 1, step+1)
    return reduce(lambda crc, b: crc1(b ^ crc), payload, 0x00)

class ProvisioningGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("ESP32 BLE Config - Stable Mode")
        self.root.geometry("600x650")
        
        self.client = None
        self.devices = {}
        
        self.ble_loop = asyncio.new_event_loop()
        self.t = threading.Thread(target=self._run_ble_loop, daemon=True)
        self.t.start()
        
        self.setup_ui()
        
        # Bắt đầu vòng lặp kiểm tra kết nối mỗi 3 giây
        self.check_connection_status()
        
        self.root.protocol("WM_DELETE_WINDOW", self.on_close)

    def _run_ble_loop(self):
        asyncio.set_event_loop(self.ble_loop)
        self.ble_loop.run_forever()

    def setup_ui(self):
        # 1. Device Scan
        f1 = ttk.LabelFrame(self.root, text="1. Device Scan", padding=10); f1.pack(fill="x", padx=10, pady=5)
        ttk.Button(f1, text="Scan Devices", command=self.scan).pack(side="left")
        self.l_scan = ttk.Label(f1, text="Ready", foreground="blue"); self.l_scan.pack(side="left", padx=10)

        # 2. Connectivity - Đã sửa để Combobox và Button bằng nhau
        f2 = ttk.LabelFrame(self.root, text="2. Connectivity", padding=10); f2.pack(fill="x", padx=10, pady=5)
        self.cb = ttk.Combobox(f2, state="readonly"); 
        self.cb.pack(fill="x", pady=5) # Thêm fill="x" để giãn bằng nút bấm
        
        self.btn_conn = ttk.Button(f2, text="Connect", command=self.toggle_connection)
        self.btn_conn.pack(fill="x")
        
        self.l_conn = ttk.Label(f2, text="STATE: DISCONNECTED", foreground="red", font=("Arial", 10, "bold")); 
        self.l_conn.pack(pady=5)

        # 3. Credentials
        f3 = ttk.LabelFrame(self.root, text="3. Credentials", padding=10); f3.pack(fill="x", padx=10, pady=5)
        ttk.Label(f3, text="SSID:").pack(anchor="w")
        self.e_ssid = ttk.Entry(f3); self.e_ssid.pack(fill="x", pady=2); self.e_ssid.insert(0, "MyWiFi")
        ttk.Label(f3, text="Password:").pack(anchor="w")
        self.e_pw = ttk.Entry(f3, show="*"); self.e_pw.pack(fill="x", pady=2); self.e_pw.insert(0, "12345678")
        ttk.Button(f3, text="Send Config via BLE", command=self.send).pack(fill="x", pady=10)

        self.txt = scrolledtext.ScrolledText(self.root, height=12); self.txt.pack(fill="both", padx=10, pady=5)

    def check_connection_status(self):
        """Kiểm tra trạng thái kết nối thực tế mỗi 3 giây"""
        is_connected = False
        if self.client:
            is_connected = self.client.is_connected
        
        self.update_conn_ui(is_connected)
        
        # Đệ quy sau 3000ms (3 giây)
        if self.root.winfo_exists():
            self.root.after(3000, self.check_connection_status)

    def log(self, msg):
        t = datetime.now().strftime("%H:%M:%S")
        if self.root.winfo_exists():
            self.root.after(0, lambda: (self.txt.insert("end", f"[{t}] {msg}\n"), self.txt.see("end")))

    def scan(self):
        self.l_scan.config(text="Scanning...", foreground="orange")
        asyncio.run_coroutine_threadsafe(self._async_scan(), self.ble_loop)

    async def _async_scan(self):
        try:
            devs = await BleakScanner.discover(timeout=5.0)
            self.devices = { (d.name if d.name else d.address): d.address for d in devs }
            self.root.after(0, self._update_scan_results, devs)
        except Exception as e: self.log(f"Scan error: {e}")

    def _update_scan_results(self, devs):
        self.cb.config(values=list(self.devices.keys()))
        self.l_scan.config(text=f"Found {len(devs)} devices", foreground="green")

    def toggle_connection(self):
        if self.client and self.client.is_connected:
            asyncio.run_coroutine_threadsafe(self._async_disconnect(), self.ble_loop)
        else:
            addr = self.devices.get(self.cb.get())
            if addr:
                asyncio.run_coroutine_threadsafe(self._async_connect(addr), self.ble_loop)
            else:
                messagebox.showwarning("Warning", "Please select a device!")

    async def _async_connect(self, addr):
        try:
            self.log(f"Connecting to {addr}...")
            # 1. Giảm timeout xuống khoảng 5-7s là đủ cho BLE 4.2
            self.client = BleakClient(addr, disconnected_callback=self.on_disconnect, timeout=7.0)
            
            await self.client.connect()
            
            # 2. (Tùy chọn) Chỉ tập trung vào Characteristic bạn cần 
            # thay vì đợi nó discover toàn bộ mạch
            self.log("✓ Connected.")
            self.root.after(0, lambda: self.update_conn_ui(True))
        except Exception as e: 
            self.log(f"Failed: {e}")
            self.root.after(0, lambda: self.update_conn_ui(False))

    async def _async_disconnect(self):
        if self.client:
            await self.client.disconnect()

    def on_disconnect(self, client):
        self.log("⚠ Disconnected from device.")
        self.root.after(0, lambda: self.update_conn_ui(False))

    def update_conn_ui(self, connected: bool):
        """Cập nhật giao diện dựa trên trạng thái kết nối"""
        if connected:
            if self.btn_conn['text'] != "Disconnect":
                self.btn_conn.config(text="Disconnect")
                self.l_conn.config(text="STATE: CONNECTED", foreground="green")
        else:
            if self.btn_conn['text'] != "Connect":
                self.btn_conn.config(text="Connect")
                self.l_conn.config(text="STATE: DISCONNECTED", foreground="red")

    def send(self):
        ssid, pw = self.e_ssid.get(), self.e_pw.get()
        if any(ord(c) > 127 for c in ssid) or any(ord(c) > 127 for c in pw):
            messagebox.showerror("Error", "English characters only (No Unikey)!")
            return
            
        if self.client and self.client.is_connected:
            asyncio.run_coroutine_threadsafe(self._async_send(ssid, pw), self.ble_loop)
        else:
            messagebox.showerror("Error", "Not connected!")

    async def _async_send(self, s, p):
        try:
            payload = b'\xa8' + f"{s}\x1f{p}".encode()
            pkt = CMD_HEADER + len(payload).to_bytes(2, 'big') + payload + crc8(payload).to_bytes(1, 'big')
            await self.client.write_gatt_char(UART_TX_UUID, pkt, response=False)
            self.log(f"Sent: {pkt.hex().upper()}")
            self.root.after(0, lambda: messagebox.showinfo("Sent", "Config delivered!"))
        except Exception as e: self.log(f"Send Error: {e}")

    def on_close(self):
        if self.client and self.client.is_connected:
            asyncio.run_coroutine_threadsafe(self.client.disconnect(), self.ble_loop)
        self.ble_loop.call_soon_threadsafe(self.ble_loop.stop)
        self.root.destroy()

if __name__ == "__main__":
    root = tk.Tk()
    app = ProvisioningGUI(root)
    root.mainloop()