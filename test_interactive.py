import serial
import time
import threading

def read_loop(ser):
    while True:
        try:
            s = ser.read(ser.in_waiting)
            if s:
                print("HEX: " + s.hex())
        except:
            break
        time.sleep(0.01)

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
        
        t = threading.Thread(target=read_loop, args=(emu,), daemon=True)
        t.start()
        
        print("SEND 55")
        emu.write(b'\x55')
        time.sleep(1)
        
        print("SEND header")
        emu.write(b'\x00\x07\x07\x01')
        time.sleep(1)
        
        print("SEND crch")
        emu.write(b'\x7a')
        time.sleep(1)
        
        print("SEND body")
        emu.write(bytes.fromhex('f600ffba5d803003ffffffff0000'))
        time.sleep(1)
        
        print("SEND crcd")
        emu.write(bytes.fromhex('ef'))
        time.sleep(3)
        
    except Exception as e:
        print(e)

if __name__ == "__main__":
    run_test()
