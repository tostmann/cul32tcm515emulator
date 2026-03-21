import serial
import time
import threading

tcm_port = '/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_58:E6:C5:E6:77:28-if00'
emu_port = '/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_98:A3:16:8E:9E:5C-if00'

tcm = serial.Serial(tcm_port, 57600, timeout=1)
emu = serial.Serial(emu_port, 115200, timeout=1)

def read_tcm():
    print("TCM RX task started")
    while True:
        b = tcm.read(1)
        if b:
            if b == b'\x55':
                buf = b + tcm.read(4)
                if len(buf) == 5:
                    dlen = (buf[1] << 8) | buf[2]
                    optlen = buf[3]
                    rem = dlen + optlen + 1
                    buf += tcm.read(rem)
                print(f"TCM RX: {buf.hex()}")
        else:
            time.sleep(0.01)

threading.Thread(target=read_tcm, daemon=True).start()

# 4BS Telegram: R-ORG=A5, Data=00 00 00 08, ID=FF BA 5D 80, Status=00
# Calculate CRC8 for header and data to make ESP3 packet valid
def crc8(data):
    crc = 0
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = (crc << 1) ^ 0x07
            else:
                crc <<= 1
            crc &= 0xFF
    return crc

data = bytes.fromhex("A5 00 00 00 08 FF BA 5D 80 00")
opt = bytes.fromhex("03 FF FF FF FF 00 00")
dlen = len(data)
oplen = len(opt)
header = bytes([0x55, (dlen >> 8) & 0xFF, dlen & 0xFF, oplen, 0x01])
hcrc = crc8(header[1:])
header += bytes([hcrc])
packet = header + data + opt
dcrc = crc8(data + opt)
packet += bytes([dcrc])

print(f"Sending to EMU: {packet.hex()}")
emu.write(packet)
res = emu.read(10)
print(f"EMU RET: {res.hex()}")

time.sleep(5)
print("Done")
