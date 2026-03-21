import serial
import time
import threading

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

def make_esp3_packet(packet_type, data, opt_data=[]):
    data_len = len(data)
    opt_len = len(opt_data)
    header = [
        (data_len >> 8) & 0xFF,
        data_len & 0xFF,
        opt_len,
        packet_type
    ]
    crc8h = crc8_calc(header, 0)
    
    payload = data + opt_data
    crc8d = crc8_calc(payload, 0)
    
    return bytearray([0x55] + header + [crc8h] + payload + [crc8d])

# Simple EnOcean 4BS Telegram (RORG 0xA5)
# RORG(1), Data(4), ID(4), Status(1), Checksum(1) = 11 Bytes
def make_erp1_payload(sender_id=[0xDE, 0xAD, 0xBE, 0xEF]):
    data = [0x01, 0x02, 0x03, 0x04]
    status = 0x00
    rorg = 0xA5
    packet = [rorg] + data + sender_id + [status]
    checksum = sum(packet) & 0xFF
    packet.append(checksum)
    return packet

TCM_PORT = "/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_58:E6:C5:E6:77:28-if00"
EMU_PORT = "/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_98:A3:16:8E:9E:5C-if00"

def listen(name, ser, stop_event):
    print(f"[{name}] Listening...")
    while not stop_event.is_set():
        if ser.in_waiting:
            data = ser.read(ser.in_waiting)
            print(f"[{name}] Received: {data.hex()}")
        time.sleep(0.1)

try:
    ser_tcm = serial.Serial(TCM_PORT, 57600, timeout=0.1)
    ser_emu = serial.Serial(EMU_PORT, 115200, timeout=0.1)
    
    stop_event = threading.Event()
    t1 = threading.Thread(target=listen, args=("TCM515", ser_tcm, stop_event))
    t2 = threading.Thread(target=listen, args=("EMULATOR", ser_emu, stop_event))
    t1.start()
    t2.start()

    time.sleep(1)

    print("\n--- Test 1: TCM515 Transmits -> Emulator Receives ---")
    erp1 = make_erp1_payload(sender_id=[0x01, 0x02, 0x03, 0x04])
    esp3 = make_esp3_packet(1, erp1, [0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00]) # Type 1 (Radio), OptData(7)
    print(f"[HOST -> TCM515] Sending ESP3: {esp3.hex()}")
    ser_tcm.write(esp3)
    
    time.sleep(2)

    print("\n--- Test 2: Emulator Transmits -> TCM515 Receives ---")
    erp1 = make_erp1_payload(sender_id=[0x05, 0x06, 0x07, 0x08])
    esp3 = make_esp3_packet(1, erp1, [0x01, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00])
    print(f"[HOST -> EMULATOR] Sending ESP3: {esp3.hex()}")
    ser_emu.write(esp3)
    
    time.sleep(2)

    stop_event.set()
    t1.join()
    t2.join()
    ser_tcm.close()
    ser_emu.close()

except Exception as e:
    print(f"Error: {e}")
