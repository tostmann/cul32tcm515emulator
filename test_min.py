import serial
import time

def run_test():
    try:
        emu = serial.Serial('/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_98:A3:16:8E:9E:5C-if00', 115200, timeout=0.1)
        emu.dtr = False
        emu.rts = True
        time.sleep(0.1)
        emu.dtr = True
        emu.rts = False
        time.sleep(4)
        emu.read(emu.in_waiting)
        
        print("SEND 55")
        emu.write(b'\x55')
        time.sleep(1)
        s = emu.read(emu.in_waiting)
        if s: print("RECV: " + s.hex())
        else: print("NO RESPONSE TO 55")
            
    except Exception as e:
        print(e)

if __name__ == "__main__":
    run_test()
