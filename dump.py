import serial
import time

ser = serial.Serial('/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_98:A3:16:8E:9E:5C-if00', 115200)
ser.setDTR(False)
ser.setRTS(True)
time.sleep(0.1)
ser.setDTR(True)
ser.setRTS(False)

print("Reading raw bytes...")
end = time.time() + 3
while time.time() < end:
    data = ser.read(ser.in_waiting)
    if data:
        print(data.hex())
    time.sleep(0.1)
