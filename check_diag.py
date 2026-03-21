import os

def check():
    with open('src/esp3_proto.c', 'r', encoding='utf-8') as f:
        content = f.read()
    print(content[content.find("0xAA"):content.find("0xAA")+300])

if __name__ == "__main__":
    check()
