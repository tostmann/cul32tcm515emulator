import serial
import time

def read_boot():
    try:
        emu = serial.Serial('/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_98:A3:16:8E:9E:5C-if00', 115200, timeout=1)
        emu.dtr = False
        emu.rts = True
        time.sleep(0.1)
        emu.dtr = True
        emu.rts = False
        
        end = time.time() + 5
        while time.time() < end:
            s = emu.read(emu.in_waiting)
            if s:
                print(s.hex())
            time.sleep(0.05)
    except Exception as e:
        print(e)

if __name__ == "__main__":
    read_boot()
