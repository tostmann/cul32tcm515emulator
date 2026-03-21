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
        
        frames = bytes.fromhex("55000707017af600ffba5d803003ffffffff0000ef")
        for i, b in enumerate(frames):
            emu.write(bytes([b]))
            time.sleep(0.01) # slow stream
            s = emu.read(emu.in_waiting)
            if s: print(f"RECV at step {i} (sent {hex(b)}): " + s.hex())
            
        time.sleep(1)
        s = emu.read(emu.in_waiting)
        if s: print("RECV FIN: " + s.hex())
            
    except Exception as e:
        print(e)

if __name__ == "__main__":
    run_test()
