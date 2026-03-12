import serial
import time

EMU_ID = "/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_58:E6:C5:E6:90:B0-if00"

try:
    with serial.Serial(EMU_ID, 57600, timeout=1) as ser:
        # Enable loopback (if not already)
        # Assuming CO_COMMAND 0x7E 0x01
        # Sync(55) Header(0002 00 05) CRC8H(43) Data(7E 01) CRC8D(20)
        ser.write(bytes.fromhex("5500020005437e0120"))
        time.sleep(0.1)
        ser.read(ser.in_waiting) # clear buffer

        # Send RPS
        payload = bytes.fromhex("55000707017af600ffffffff3003ffffffff00000a")
        print(f"Sending: {payload.hex()}")
        ser.write(payload)
        
        start = time.time()
        while time.time() - start < 3:
            if ser.in_waiting > 0:
                resp = ser.read(ser.in_waiting)
                print(f"Received: {resp.hex()}")
                if b'\xf6\x00\xff\xff\xff\xff\x30' in resp:
                    print("SUCCESS: Loopback received!")
                    break
            time.sleep(0.1)
        else:
            print("FAILED: No loopback received.")
except Exception as e:
    print(f"Error: {e}")
