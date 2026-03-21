import os

def print_esp3_resp():
    with open('src/esp3_proto.c', 'rb') as f:
        content = f.read().decode('utf-8', 'replace')
    idx = content.find("void esp3_send_response")
    print(content[idx:idx+400])

if __name__ == "__main__":
    print_esp3_resp()
