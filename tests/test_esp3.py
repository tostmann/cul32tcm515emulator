import serial
import time

def crc8_calc(data, crc):
    crc ^= data
    for _ in range(8):
        if crc & 0x80:
            crc = (crc << 1) ^ 0x07
        else:
            crc <<= 1
        crc &= 0xFF
    return crc

def calc_buffer_crc(buf):
    crc = 0
    for b in buf:
        crc = crc8_calc(b, crc)
    return crc

def send_esp3(ser, packet_type, data, opt_data=[]):
    data_len = len(data)
    opt_len = len(opt_data)
    header = [
        (data_len >> 8) & 0xFF,
        data_len & 0xFF,
        opt_len,
        packet_type
    ]
    crc8h = calc_buffer_crc(header)
    
    payload = data + opt_data
    crc8d = calc_buffer_crc(payload)
    
    packet = [0x55] + header + [crc8h] + payload + [crc8d]
    ser.write(bytearray(packet))
    print(f"Sent: {bytearray(packet).hex()}")

port = "/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_98:A3:16:8E:9E:5C-if00"
ser = serial.Serial(port, 115200, timeout=1)

# Send ERP1 packet (Type 1)
send_esp3(ser, 1, [0xA5])

# Wait for response
resp = ser.read(10)
if resp:
    print(f"Received: {resp.hex()}")
else:
    print("No response")

ser.close()
