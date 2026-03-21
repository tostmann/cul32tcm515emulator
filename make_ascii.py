import sys
import string

def make_ascii(filepath):
    with open(filepath, 'rb') as f:
        data = f.read()
    
    printable = set(string.printable.encode('ascii'))
    cleaned = bytearray()
    for b in data:
        if b in printable:
            cleaned.append(b)
        else:
            # We skip non-printable chars
            pass
            
    with open(filepath, 'wb') as f:
        f.write(cleaned)

make_ascii('src/esp3_proto.c')
