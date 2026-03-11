import serial
import time
import threading

# IDs
TCM_ID = "/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_58:E6:C5:E6:77:28-if00"
EMU_ID = "/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_58:E6:C5:E6:90:B0-if00"

def listen_port(name, port_id):
    try:
        ser = serial.Serial(port_id, 57600, timeout=1)
        print(f"--- Listening on {name} ({port_id}) ---")
        start = time.time()
        while time.time() - start < 10:
            data = ser.read(ser.in_waiting or 1)
            if data:
                print(f"[{name} RECV]: {data.hex()}")
    except Exception as e:
        print(f"Error on {name}: {e}")

# Step 1: EMU sends RF -> TCM receives
print("\nSTAGE 1: EMU -> RF -> TCM")
tcm_thread = threading.Thread(target=listen_port, args=("TCM", TCM_ID))
tcm_thread.start()

time.sleep(1)
ser_emu = serial.Serial(EMU_ID, 57600)
# Send RPS Telegram (0xF6) from EMU
# ESP3: Sync(55) Header(0007 07 01) CRC8H(7A) Data(F6 00 FF FF FF FF 30) Opt(03 FF FF FF FF 00 00) CRC8D(0A)
payload = bytes.fromhex("55000707017af600ffffffff3003ffffffff00000a")
print(f"[EMU SEND]: {payload.hex()}")
ser_emu.write(payload)

tcm_thread.join()

# Step 2: TCM sends RF -> EMU receives (Check RSSI)
print("\nSTAGE 2: TCM -> RF -> EMU")
emu_thread = threading.Thread(target=listen_port, args=("EMU", EMU_ID))
emu_thread.start()

time.sleep(1)
ser_tcm = serial.Serial(TCM_ID, 57600)
# Send RPS Telegram (0xF6) from TCM
print(f"[TCM SEND]: {payload.hex()}")
ser_tcm.write(payload)

emu_thread.join()
