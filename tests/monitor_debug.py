import serial
import time

EMU_ID = "/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_58:E6:C5:E6:90:B0-if00"

print("Monitoring EMU for debug packets...")
try:
    with serial.Serial(EMU_ID, 57600, timeout=1) as ser:
        start = time.time()
        while time.time() - start < 10:
            if ser.in_waiting > 0:
                data = ser.read(ser.in_waiting)
                # Look for Sync 55
                for i in range(len(data)):
                    if data[i] == 0x55:
                        # Print packet snippet
                        print(f"Packet: {data[i:i+12].hex()}")
            time.sleep(0.1)
except Exception as e:
    print(f"Error: {e}")
