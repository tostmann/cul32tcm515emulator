# TCM515 Emulator for ESP32-C6 & CC1101

This firmware emulates an EnOcean TCM515 Gateway using an ESP32-C6 and a CC1101 SPI transceiver. It is designed to be a 100% transparent drop-in replacement via the EnOcean Serial Protocol 3 (ESP3).

## Hardware Setup

- **Processor**: ESP32-C6-MINI
- **RF Transceiver**: TI CC1101 (868.3 MHz)
- **Connections**:
  - GDO0: GPIO2
  - GDO2: GPIO3 (Carrier Sense / Async Data Out)
  - SS: GPIO18
  - SCK: GPIO19
  - MISO: GPIO20
  - MOSI: GPIO21
  - LED: GPIO8 (low active)
  - Switch: GPIO9 (low active)

## Features

- **Transparent ESP3 Parser**: Handles EnOcean Serial Protocol 3.
- **Hardware-Timed TX**: Uses CC1101 Packet Mode with LUT-based Manchester encoding for precise 125kbps RF timing.
- **PLL-Based RX Decoder**: Asynchronous sampling of RF pulses via RMT for robust Manchester decoding.
- **SIIL (Software-in-the-Loop) Testing**:
  - **Internal Loopback**: Toggle via `COMMON_COMMAND` 0x7E. Reflects TX packets back to the host.
  - **Pulse Injection**: Inject virtual RMT symbols via `COMMON_COMMAND` 0x7F for decoder validation.

## Development & Testing

### Requirements
- PlatformIO with ESP-IDF framework.
- Python 3 with `pyserial`.

### Scripts
- `test_loopback.py`: Validates the ESP3 stack and host communication.
- `test_injection_v2.py`: Validates the Manchester decoder logic with synthetic pulses.

## License
MIT
