import serial
import time

try:
    ser = serial.Serial('/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_98:A3:16:8E:9E:5C-if00', 115200, timeout=0.1)
    
    # Send a quick soft-reset via DTR/RTS (standard ESP32 reset)
    ser.setDTR(False)
    ser.setRTS(True)
    time.sleep(0.1)
    ser.setDTR(True)
    ser.setRTS(False)
    
    print("Waiting for boot logs...")
    end = time.time() + 3
    while time.time() < end:
        line = ser.readline()
        if b'HW_TEST' in line:
            print(line.decode('utf-8', 'ignore').strip())
except Exception as e:
    print(e)
