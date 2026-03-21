import os

def print_esp3_data():
    with open('src/esp3_proto.c', 'rb') as f:
        content = f.read().decode('utf-8', 'replace')
    idx = content.find("        case STATE_DATA:")
    print(content[idx:idx+800])

if __name__ == "__main__":
    print_esp3_data()
