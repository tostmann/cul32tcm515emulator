# EnOcean TCM Emulator Library

This project provides a 100% transparent, drop-in replacement for an EnOcean TCM515 Gateway using an ESP32-C6 and a CC1101 SPI transceiver.

## Features
- **TCM515 Emulation**: Implements the EnOcean Serial Protocol 3 (ESP3).
- **Stream Abstraction**: Swap between the Software Emulator and a real TCM515 (Hardware UART) using a unified `enocean_stream_t` interface.
- **Security Layer**: Full support for EnOcean Security (AES-128 CMAC & CTR) with NVS-based key management and anti-replay protection.
- **Asynchronous OOK**: Uses ESP32 RMT for high-precision Manchester-encoded RF transmission.

## Library API

### `enocean_stream_t` Interface
The library uses a "Vtable" pattern for polymorphism in C.

```c
#include "enocean_stream.h"

enocean_stream_t *tcm = get_emulator_stream();
STREAM_INIT(tcm, 57600);

uint8_t rx_data[256];
size_t len = STREAM_READ_BUF(tcm, rx_data, sizeof(rx_data));
if (len > 0) {
    // Process ESP3 packet
}
```

### Backends
- **Emulator**: `get_emulator_stream()` - Uses the CC1101 transceiver on the local board.
- **UART**: `get_uart_stream(uart_port, tx_pin, rx_pin)` - Bridges to a physical TCM515 module.

## Hardware Support (cul32-c6)
- **MCU**: ESP32-C6-MINI
- **Transceiver**: CC1101
- **Pins**:
  - GDO0: GPIO2 (RMT Data)
  - GDO2: GPIO3 (Carrier Sense)
  - SPI: SS=18, SCK=19, MISO=20, MOSI=21

## Build & Test
```bash
# Build
pio run -e esp32-c6-mini

# Test (Loopback)
python3 tests/test_loopback_v3.py
```

## License
Copyright (c) 2024 Dirk Tostmann tostmann@busware.de  
Licensed under the Apache License, Version 2.0.
