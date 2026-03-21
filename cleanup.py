import re
import os

def cleanup_source_file(filepath, is_radio_hal=False):
    if not os.path.exists(filepath):
        print(f"Datei nicht gefunden: {filepath}")
        return

    print(f"Verarbeite {filepath}...")

    # 1. Datei einlesen (mit errors='replace', um binäre/ungültige Zeichen zu handhaben)
    with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read()

    # Regex für esp3_send_packet(0x7... / 0x8...) - we also want to match single digit 0x7 if any but they are usually 0x70. 
    # Let's just remove anything with 0x7 or 0x8 as first arg
    debug_packet_regex = r'esp3_send_packet\s*\(\s*0x[78][0-9a-fA-F]?\s*,[^;]*\);'

    # Spezifisches Muster für den Hardware-Test-Block in radio_hal.c
    hw_test_block_regex = r'// --- HARDWARE VERIFICATION TEST ---[\s\S]*?// --- END VERIFICATION ---'

    new_content = content

    if is_radio_hal:
        new_content = re.sub(hw_test_block_regex, '', new_content)
        print(f"  - Hardware Verification Block entfernt (falls vorhanden).")

    count_before = len(re.findall(debug_packet_regex, new_content))
    new_content = re.sub(debug_packet_regex, '', new_content)
    
    if count_before > 0:
        print(f"  - {count_before} Debug-Paket-Aufrufe (0x7x/0x8x) entfernt.")

    # Also remove the uint8_t pre_tx = 0xAA; lines we added
    new_content = re.sub(r'uint8_t pre_tx = 0xAA;', '', new_content)
    new_content = re.sub(r'uint8_t post_tx = 0xBB;', '', new_content)
    new_content = re.sub(r'uint8_t d_enter = 0xAA;', '', new_content)
    new_content = re.sub(r'uint8_t d_exit = 0xBB;', '', new_content)
    new_content = re.sub(r'uint8_t d_pre = 0xC1;', '', new_content)
    new_content = re.sub(r'uint8_t d_mid = 0xC2;', '', new_content)
    new_content = re.sub(r'uint8_t d_post = 0xC3;', '', new_content)
    new_content = re.sub(r'uint8_t d1 = 0xD1;', '', new_content)
    new_content = re.sub(r'uint8_t d2 = 0xD2;', '', new_content)
    new_content = re.sub(r'uint8_t d3 = 0xD3;', '', new_content)
    
    # Remove variables in radio_hal_init that were part of hardware verification
    new_content = re.sub(r'uint8_t d[0-9]\[[0-9]\] = \{[^}]*\};', '', new_content)

    # 3. Bereinigten Inhalt zurückschreiben
    with open(filepath, 'w', encoding='utf-8') as f:
        f.write(new_content)
    
    print(f"Fertig: {filepath} wurde bereinigt.\n")

def main():
    proto_file = os.path.join('src', 'esp3_proto.c')
    hal_file = os.path.join('src', 'radio_hal.c')

    cleanup_source_file(proto_file, is_radio_hal=False)
    cleanup_source_file(hal_file, is_radio_hal=True)

if __name__ == "__main__":
    main()
