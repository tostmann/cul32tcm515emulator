import os

def print_esp3_process():
    with open('src/esp3_proto.c', 'rb') as f:
        content = f.read().decode('utf-8', 'replace')
    idx = content.find("void esp3_process_byte(uint8_t byte)")
    print(content[idx:idx+800])

if __name__ == "__main__":
    print_esp3_process()
