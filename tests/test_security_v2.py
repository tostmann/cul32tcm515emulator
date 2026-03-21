import struct
import serial
import time
from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
from cryptography.hazmat.primitives import cmac
from cryptography.hazmat.backends import default_backend

# --- Parameter ---
KEY = bytes(bytearray([0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 
                       0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F]))
SENDER_ID = 0xDEADBEEF
RLC = 123 # Use a higher RLC to be safe
PAYLOAD_CLEAR = 0x42
R_ORG = 0x31
STATUS = 0x00
SLF = 0x8B 

EMU_ID = "/dev/serial/by-id/usb-Espressif_USB_JTAG_serial_debug_unit_98:A3:16:8E:9E:5C-if00"

def calc_crc8(data: bytes) -> int:
    crc = 0x00
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80: crc = (crc << 1) ^ 0x07
            else: crc = crc << 1
            crc &= 0xFF
    return crc

def build_esp3_frame(packet_type: int, data: bytes, opt_data: bytes = b'') -> bytes:
    header = struct.pack(">HBB", len(data), len(opt_data), packet_type)
    crc8_hdr = calc_crc8(header)
    body = data + opt_data
    crc8_data = calc_crc8(body)
    return b'\x55' + header + bytes([crc8_hdr]) + body + bytes([crc8_data])

def encrypt_and_mac(payload_clear: int, rlc: int, sender_id: int, key: bytes):
    backend = default_backend()
    sid_bytes = struct.pack(">I", sender_id)
    rlc_bytes = struct.pack(">I", rlc)
    # Nonce: SenderID(4) + RLC(4) + 8 bytes 0x00
    nonce = sid_bytes + rlc_bytes + (b'\x00' * 8)
    cipher = Cipher(algorithms.AES(key), modes.CTR(nonce), backend=backend)
    encryptor = cipher.encryptor()
    enc_payload = encryptor.update(bytes([payload_clear])) + encryptor.finalize()
    
    # MAC Input: [Full RLC (4)] [R-ORG (1)] [Payload_Clear (1)] [SenderID (4)] [Status (1)]
    mac_input = rlc_bytes + bytes([R_ORG, payload_clear]) + sid_bytes + bytes([STATUS])
    c = cmac.CMAC(algorithms.AES(key), backend=backend)
    c.update(mac_input)
    trunc_mac = c.finalize()[:4]
    
    return enc_payload, trunc_mac

# --- Pulse Generation ---
def generate_pulses(telegram: bytes):
    raw_pulses = []
    def add_m1(): raw_pulses.extend([(1, 4), (0, 4)])
    def add_m0(): raw_pulses.extend([(0, 4), (1, 4)])

    for _ in range(10): add_m1() # Preamble
    add_m0() # SOF

    for b in telegram:
        for i in range(7, -1, -1):
            if (b >> i) & 1: add_m1()
            else: add_m0()

    merged = []
    curr_l, curr_d = raw_pulses[0]
    for next_l, next_d in raw_pulses[1:]:
        if next_l == curr_l: curr_d += next_d
        else:
            merged.append((curr_l, curr_d))
            curr_l, curr_d = next_l, next_d
    merged.append((curr_l, curr_d))

    hex_pulses = ""
    for l, d in merged:
        ticks = int(d * 8)
        val = (ticks & 0x7FFF) | (0x8000 if l else 0)
        hex_pulses += f"{val:04X}"
    return hex_pulses

try:
    with serial.Serial(EMU_ID, 57600, timeout=2) as ser:
        # 1. Add Secure Device
        add_dev_data = struct.pack(">BB", 0x19, SLF) + struct.pack(">I", SENDER_ID) + KEY + struct.pack(">I", RLC-1)
        ser.write(build_esp3_frame(0x05, add_dev_data))
        time.sleep(0.1)
        resp = ser.read(ser.in_waiting)
        print(f"Add Device Response: {resp.hex()}")

        # 2. Generate and Inject Secure Telegram
        enc_payload, mac = encrypt_and_mac(PAYLOAD_CLEAR, RLC, SENDER_ID, KEY)
        print(f"Payload Clear: {PAYLOAD_CLEAR:02X}, Enc: {enc_payload[0]:02X}")
        erp1_data = bytes([R_ORG]) + enc_payload + struct.pack(">I", RLC)[1:] + mac + struct.pack(">I", SENDER_ID) + bytes([STATUS])
        crc = sum(erp1_data) % 256
        telegram = erp1_data + bytes([crc])
        
        hex_pulses = generate_pulses(telegram)
        cmd_data = b'\x7F' + hex_pulses.encode('ascii')
        ser.write(build_esp3_frame(0x05, cmd_data))
        print(f"Injected Secure Telegram (RLC={RLC})")

        # 3. Listen for result
        start = time.time()
        found = False
        while time.time() - start < 3:
            if ser.in_waiting > 0:
                resp = ser.read(ser.in_waiting)
                if b'\x55\x00\x0f' in resp: 
                    print("!!! ERFOLG !!! Sicheres Telegramm wurde empfangen!")
                    found = True
                    break
            time.sleep(0.1)
        
        if found:
            print("\nVersuche Replay Attacke (selber RLC)...")
            ser.write(build_esp3_frame(0x05, cmd_data))
            start = time.time()
            found_replay = False
            while time.time() - start < 2:
                if ser.in_waiting > 0:
                    resp = ser.read(ser.in_waiting)
                    if b'\x55\x00\x0f' in resp:
                        print("FEHLER: Replay Telegramm wurde akzeptiert!")
                        found_replay = True
                time.sleep(0.1)
            if not found_replay:
                print("ERFOLG: Replay Attacke wurde abgeblockt.")

except Exception as e:
    print(f"Error: {e}")
