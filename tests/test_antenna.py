import serial
import time
import threading

# IDs
TCM_ID = "/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_58:E6:C5:E6:77:28-if00"
EMU_ID = "/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_98:A3:16:8E:9E:5C-if00"

def listen_port(name, port_id, duration=10):
    try:
        ser = serial.Serial(port_id, 57600, timeout=0.1)
        print(f"--- Listening on {name} ({port_id}) ---")
        start = time.time()
        while time.time() - start < duration:
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                print(f"[{name} RECV]: {data.hex()}")
            time.sleep(0.01)
    except Exception as e:
        print(f"Error on {name}: {e}")

# Test 1: EMU sends multiple packets, TCM listens
print("\n--- TEST 1: EMU -> TCM (Antenna check) ---")
tcm_thread = threading.Thread(target=listen_port, args=("TCM", TCM_ID, 12))
tcm_thread.start()
time.sleep(1)

try:
    ser_emu = serial.Serial(EMU_ID, 57600)
    # Send 5 RPS Telegrams
    for i in range(5):
        payload = bytes.fromhex("55000707017af600ffffffff3003ffffffff00000a")
        print(f"[EMU SEND {i+1}]: {payload.hex()}")
        ser_emu.write(payload)
        time.sleep(1.5)
except Exception as e:
    print(f"Error sending from EMU: {e}")

tcm_thread.join()

# Test 2: TCM sends multiple packets, EMU listens
print("\n--- TEST 2: TCM -> EMU (Check RSSI & Data) ---")
emu_thread = threading.Thread(target=listen_port, args=("EMU", EMU_ID, 12))
emu_thread.start()
time.sleep(1)

try:
    ser_tcm = serial.Serial(TCM_ID, 57600)
    for i in range(5):
        payload = bytes.fromhex("55000707017af600ffffffff3003ffffffff00000a")
        print(f"[TCM SEND {i+1}]: {payload.hex()}")
        ser_tcm.write(payload)
        time.sleep(1.5)
except Exception as e:
    print(f"Error sending from TCM: {e}")

emu_thread.join()
