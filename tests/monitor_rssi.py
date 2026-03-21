import serial
import time

EMU_ID = "/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_98:A3:16:8E:9E:5C-if00"

print("Monitoring RSSI and GDO status...")
try:
    with serial.Serial(EMU_ID, 57600, timeout=1) as ser:
        while True:
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                # Look for Type 0x33: 55 00 04 00 33 CRC8H [4 bytes] CRC8D
                idx = data.find(b'\x33')
                if idx >= 4 and data[idx-4] == 0x55:
                    payload = data[idx+2 : idx+6]
                    if len(payload) == 4:
                        rssi_raw = payload[0]
                        rssi_dbm = payload[1]
                        if rssi_dbm > 127: rssi_dbm -= 256
                        gdo2 = payload[2]
                        gdo0 = payload[3]
                        print(f"RSSI: {rssi_dbm} dBm (Raw: {rssi_raw:02X}), GDO2(CS): {gdo2}, GDO0(Data): {gdo0}")
            time.sleep(0.1)
except KeyboardInterrupt:
    pass
except Exception as e:
    print(f"Error: {e}")
