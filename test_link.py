import serial
import time
import threading

tcm_port = '/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_58:E6:C5:E6:77:28-if00'
emu_port = '/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_98:A3:16:8E:9E:5C-if00'

tcm = serial.Serial(tcm_port, 57600, timeout=1)
emu = serial.Serial(emu_port, 115200, timeout=1)

def reset_emu():
    emu.dtr = False
    emu.rts = True
    time.sleep(0.1)
    emu.dtr = True
    emu.rts = False
    time.sleep(4)
    emu.read(emu.in_waiting)

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
                    rem = dlen + optlen + 1 # +crc8d
                    buf += tcm.read(rem)
                print(f"TCM RX: {buf.hex()}")
        else:
            time.sleep(0.01)

threading.Thread(target=read_tcm, daemon=True).start()

reset_emu()

packet = bytes.fromhex("55 00 07 07 01 7A F6 00 FF BA 5D 80 30 03 FF FF FF FF 00 00 EF")
print(f"Sending to EMU: {packet.hex()}")
emu.write(packet)
res = emu.read(10)
print(f"EMU RET: {res.hex()}")

time.sleep(2)
print("Done")
