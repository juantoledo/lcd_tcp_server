import serial
import socket
import threading
import argparse
import time
import pyudev
import os

ser = None
ser_lock = threading.Lock()
current_port = None
TCP_PORT = 9999

def find_serial_port_by_vid_pid(vid, pid):
    context = pyudev.Context()
    print(f"[DEBUG] Scanning for devices with VID={vid} and PID={pid}")
    for device in context.list_devices(subsystem='tty'):
        props = device.properties
        if 'ID_VENDOR_ID' in props and 'ID_MODEL_ID' in props:
            found_vid = props['ID_VENDOR_ID']
            found_pid = props['ID_MODEL_ID']
            dev_path = device.device_node
            print(f"[DEBUG] Found device {dev_path} with VID={found_vid}, PID={found_pid}")
            if found_vid == vid and found_pid == pid:
                print(f"[DEBUG] Matching device found: {dev_path}")
                return dev_path
    print("[DEBUG] No matching device found.")
    return None

def serial_port_exists(port):
    exists = os.path.exists(port)
    print(f"[DEBUG] Checking if port {port} exists: {exists}")
    return exists

def monitor_serial_output():
    global ser
    while True:
        with ser_lock:
            local_ser = ser
        if local_ser and local_ser.is_open and local_ser.in_waiting:
            try:
                line = local_ser.readline().decode("utf-8", errors="replace").strip()
                if line:
                    print(f"[ARDUINO] {line}")
            except Exception as e:
                print(f"[ERROR] Reading serial: {e}")
        time.sleep(0.1)

def connect_serial(vid, pid, baud):
    global ser, current_port
    while True:
        port = find_serial_port_by_vid_pid(vid, pid)
        if not port:
            print("[WAIT] Arduino not found. Retrying in 2s...")
            time.sleep(2)
            continue

        try:
            print(f"[DEBUG] >>> Attempting to open serial port {port} at {baud} baud")
            new_ser = serial.Serial(port, baud, timeout=1)
            print(f"[DEBUG] <<< Serial port {port} opened")

            new_ser.dtr = False
            time.sleep(0.1)
            new_ser.dtr = True

            print("[DEBUG] Waiting for Arduino to print 'Ready...'")
            start_time = time.time()
            while True:
                if new_ser.in_waiting:
                    line = new_ser.readline().decode("utf-8", errors="replace").strip()
                    print(f"[DEBUG] From Arduino: {line}")
                    if "Ready" in line:
                        print("[OK] Arduino is ready.")
                        break
                if time.time() - start_time > 5:
                    print("[WARN] Timeout waiting for Arduino READY message.")
                    break

            with ser_lock:
                ser = new_ser
                current_port = port
                print(f"[DEBUG] Serial object assigned.")
                print(f"[DEBUG] Serial open: {ser.is_open}")

            # Enviar mensaje al LCD
            try:
                dev_label = os.path.basename(port)
                msg1 = f"x=0&y=0&message={dev_label}\n"
                msg2 = f"x=0&y=1&message=TCP {TCP_PORT}\n"
                new_ser.write(msg1.encode())
                print(f"[DEBUG] Wrote to LCD: {msg1.strip()}")
                time.sleep(0.1)
                new_ser.write(msg2.encode())
                print(f"[DEBUG] Wrote to LCD: {msg2.strip()}")
            except Exception as e:
                print(f"[ERROR] Could not write startup message to LCD: {e}")

            print(f"[OK] Connected to {port}")
            return

        except Exception as e:
            print(f"[ERROR] Could not open serial port {port}: {e}")
            time.sleep(5)

def serial_reconnector(vid, pid, baud):
    global ser, current_port
    while True:
        should_reconnect = False

        with ser_lock:
            local_ser = ser
            local_port = current_port

            if local_ser is None or not local_ser.is_open:
                should_reconnect = True
            elif not local_port or not serial_port_exists(local_port):
                try:
                    local_ser.close()
                    print(f"[DEBUG] Closed port {local_port}")
                except Exception as e:
                    print(f"[ERROR] While closing port: {e}")
                ser = None
                should_reconnect = True

        if should_reconnect:
            print("[DEBUG] Serial not open or disconnected. Reconnecting...")
            connect_serial(vid, pid, baud)

        time.sleep(10)

def handle_client(conn, addr):
    global ser
    try:
        data = conn.recv(1024).decode("utf-8").strip()
        if data:
            print(f"[RECV from {addr}] {data}")
            with ser_lock:
                local_ser = ser
            if local_ser and local_ser.is_open:
                print(f"[DEBUG] Writing to serial: {data}")
                try:
                    local_ser.write((data + "\n").encode("utf-8"))
                except Exception as e:
                    print(f"[ERROR] Writing to serial: {e}")
            else:
                print("[ERROR] Serial port not connected. Message dropped.")
        conn.close()
    except Exception as e:
        print(f"[ERROR] Handling {addr}: {e}")

def main():
    global TCP_PORT
    parser = argparse.ArgumentParser(description="TCP to Serial LCD bridge with LCD init display and READY wait")
    parser.add_argument("--baud", type=int, default=9600, help="Baud rate")
    parser.add_argument("--tcp", type=int, default=9999, help="TCP listen port")
    parser.add_argument("--vid", default="2341", help="USB Vendor ID (hex, e.g. 2341)")
    parser.add_argument("--pid", default="0043", help="USB Product ID (hex, e.g. 0043)")
    args = parser.parse_args()

    TCP_PORT = args.tcp

    print(f"[INFO] Starting LCD bridge on TCP port {TCP_PORT}")
    print(f"[INFO] Looking for device with VID={args.vid} PID={args.pid} at {args.baud} baud")

    threading.Thread(target=serial_reconnector, args=(args.vid, args.pid, args.baud), daemon=True).start()
    threading.Thread(target=monitor_serial_output, daemon=True).start()

    print("[DEBUG] Waiting for serial connection before accepting TCP...")
    while True:
        with ser_lock:
            if ser and ser.is_open:
                print(f"[DEBUG] Serial is ready on {current_port}")
                break
        time.sleep(0.5)

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("0.0.0.0", TCP_PORT))
    server.listen(5)
    print(f"[INFO] Listening for TCP connections on port {TCP_PORT}...")

    while True:
        conn, addr = server.accept()
        print(f"[INFO] New TCP connection from {addr}")
        threading.Thread(target=handle_client, args=(conn, addr), daemon=True).start()

if __name__ == "__main__":
    main()
