import serial
import time
import struct

def run_test():
    try:
        emu = serial.Serial('/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_98:A3:16:8E:9E:5C-if00', 115200, timeout=0.1)
        tcm = serial.Serial('/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_58:E6:C5:E6:77:28-if00', 57600, timeout=0.1)
        
        emu.dtr = False
        emu.rts = True
        time.sleep(0.1)
        emu.dtr = True
        emu.rts = False
        
        time.sleep(4)
        emu.read(emu.in_waiting)
        tcm.read(tcm.in_waiting)
        
        f = bytes.fromhex('55000707017af600ffba5d803003ffffffff0000ef')
        emu.write(f)
        
        end = time.time() + 3
        while time.time() < end:
            se = emu.read(emu.in_waiting)
            st = tcm.read(tcm.in_waiting)
            if se: print("EMU: " + se.hex())
            if st: print("TCM: " + st.hex())
            time.sleep(0.1)
            
    except Exception as e:
        print(e)

if __name__ == "__main__":
    run_test()
