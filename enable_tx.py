import os

def enable_tx():
    with open('src/esp3_proto.c', 'r', encoding='utf-8') as f:
        content = f.read()

    # Re-enable radio_transmit
    content = content.replace("// radio_transmit(current_packet.data, current_packet.data_len);", "radio_transmit(current_packet.data, current_packet.data_len);")
    
    with open('src/esp3_proto.c', 'w', encoding='utf-8') as f:
        f.write(content)

if __name__ == "__main__":
    enable_tx()
