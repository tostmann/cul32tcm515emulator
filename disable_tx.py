import os

def disable_radio_transmit():
    with open('src/esp3_proto.c', 'r', encoding='utf-8') as f:
        content = f.read()

    # Just comment out radio_transmit call
    old_call = """                        esp3_send_packet(0x75, &pre_tx, 1, NULL, 0); // DIAG BEFORE TX
                        radio_transmit(current_packet.data, current_packet.data_len);"""
    
    new_call = """                        esp3_send_packet(0x75, &pre_tx, 1, NULL, 0); // DIAG BEFORE TX
                        // radio_transmit(current_packet.data, current_packet.data_len);"""
    
    content = content.replace(old_call, new_call)
    
    with open('src/esp3_proto.c', 'w', encoding='utf-8') as f:
        f.write(content)
    print("Disabled radio_transmit!")

if __name__ == "__main__":
    disable_radio_transmit()
