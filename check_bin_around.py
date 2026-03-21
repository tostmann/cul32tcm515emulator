with open('src/esp3_proto.c', 'rb') as f:
    data = f.read()

idx = 6349
print("Before:", data[idx-20:idx].decode('utf-8', 'ignore'))
print("Byte:", hex(data[idx]))
print("After:", data[idx+1:idx+21].decode('utf-8', 'ignore'))
