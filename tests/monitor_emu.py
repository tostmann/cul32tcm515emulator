import serial
import time
import sys

EMU_PORT = "/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_58:E6:C5:E6:90:B0-if00"

def crc8_calc(data, crc):
    for b in data:
        crc ^= b
        for _ in range(8):
            if crc & 0x80:
                crc = (crc << 1) ^ 0x07
            else:
                crc <<= 1
            crc &= 0xFF
    return crc

try:
    ser = serial.Serial(EMU_PORT, 115200, timeout=0.1)
    print("Monitoring EMU RSSI...")
    
    while True:
        if ser.in_waiting >= 10:
            byte = ser.read(1)
            if byte == b'\x55':
                header = ser.read(4)
                if len(header) < 4: continue
                crc8h = ser.read(1)
                data_len = (header[0] << 8) | header[1]
                opt_len = header[2]
                pkt_type = header[3]
                
                payload = ser.read(data_len + opt_len)
                crc8d = ser.read(1)
                
                if pkt_type == 0x33:
                    rssi_raw = payload[0]
                    rssi_dbm = payload[1]
                    if rssi_dbm > 127: rssi_dbm -= 256
                    gdo2 = payload[2]
                    gdo0 = payload[3]
                    print(f"RSSI: {rssi_dbm} dBm, GDO2: {gdo2}, GDO0: {gdo0}")
                elif pkt_type == 0x34:
                    cnt = (payload[1] << 8) | payload[0]
                    sync = (payload[3] << 8) | payload[2]
                    print(f"RMT Capture: {cnt} symbols, Last Sync Pulses: {sync}")
                elif pkt_type == 0x07:
                    print(f"*** RADIO PACKET RECEIVED: {payload.hex()} ***")
except KeyboardInterrupt:
    print("Exit")
except Exception as e:
    print(f"Error: {e}")
