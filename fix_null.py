with open('src/esp3_proto.c', 'rb') as f:
    data = f.read()

data = data.replace(b"'\x00'", b"'\\0'")

with open('src/esp3_proto.c', 'wb') as f:
    f.write(data)
