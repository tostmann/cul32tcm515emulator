# Projekt-Gedächtnis: TCM-Emu-Gemini

### Projektziel
Entwicklung einer Firmware für den ESP32-C6, die ein EnOcean TCM515 (ESP3-Protokoll) USB-Gateway emuliert. Die Hardware-Basis besteht aus einem ESP32-C6-Modul und einem CC1101 Transceiver für das 868-MHz-Band.

### Aktueller Stand
**Die Firmware ist stabil.** Die Reboot-Schleifen (`rst:0xc SW_CPU`) sind **vollständig behoben**. Der Differentialtest (`diff_test_enocean.py`) läuft stabil ab, die USB-Kommunikation ist fehlerfrei und der Emulator antwortet korrekt auf ESP3-Befehle vom Host. Die fundamentalen Konzepte des physikalischen EnOcean-Funkprotokolls (ERP1) wurden analysiert und die Firmware-Architektur (CC1101-Register, RMT-Decoder, Sendestrategie) wurde tiefgreifend überarbeitet, um Kompatibilität zu gewährleisten.

### Nächste Schritte
*   **Validierung der RF-Kommunikation**: Der Differentialtest (`diff_test_enocean.py`) muss in beide Richtungen (Senden und Empfangen) erfolgreich durchlaufen werden.
*   **Timing-Feinabstimmung (RMT)**: Anpassen der Pulsweiten-Schwellwerte im 8-MHz-RMT-Decoder, um die Toleranz gegenüber dem realen Signal des TCM515 zu maximieren.
*   **Code-Refactoring**: Aufräumen des Codes und Hinzufügen von Kommentaren vor dem finalen Release.
*   **Feinabstimmung**: Kalibrierung der RSSI-Werte und des LBT-Schwellwerts für den Praxiseinsatz.

### Architektur-Entscheidungen

1.  **Framework**: ESP-IDF mit PlatformIO für robustes Build-Management und Komponenten-Handling.
2.  **Kommunikations-Schnittstelle**: Der native USB-JTAG-Controller des ESP32-C6 wird für den ESP3-Datenstrom verwendet. System- und Bootloader-Logs werden via `sdkconfig.defaults` zur Laufzeit vollständig deaktiviert.
3.  **Protokoll-Handling**:
    *   Eine dedizierte State-Machine (`esp3_proto.c`) parst den eingehenden ESP3-Stream byte-weise.
    *   **Statischer Payload-Puffer**: Die dynamische Speicherallokation (`malloc`) im Parser wurde durch einen **statischen Puffer** ersetzt, um Heap-Fragmentierung zu eliminieren.
4.  **Radio Abstraction**:
    *   Eine Hardware Abstraction Layer (`radio_hal.c`) kapselt die gesamte SPI-Kommunikation und Konfiguration des CC1101.
    *   **Atomare SPI-Transaktionen**: Die Burst-Funktionen (`cc1101_write/read_burst`) verwenden nun **eine einzige, atomare SPI-Transaktion**, um Hardware-Fehler mit der CS-Leitung zu beheben.
5.  **Sendestrategie (Final, ERP1-konform)**:
    *   **Modus**: Der CC1101 wird dynamisch in den **Packet Mode** (FIFO-basiert) geschaltet, um ein exaktes Timing zu gewährleisten.
    *   **Frame-Aufbau (Software)**: Der gesamte physikalische ERP1-Frame wird in Software generiert und in den CC1101-FIFO geschrieben:
        *   `0xAAAA` (Warm-Up für den CC1101 OOK-Modulator)
        *   EnOcean Preamble (Logische `1`er)
        *   EnOcean **Start-of-Frame (SOF) Bit** (Eine einzelne logische `0`)
        *   Manchester-codierter Payload (MSB first)
6.  **Empfangsstrategie (Final, gehärtet)**:
    *   **Modus**: Der CC1101 wird dynamisch in den **Asynchronen Seriellen Modus** geschaltet; der rohe Datenstrom wird am GDO0-Pin ausgegeben.
    *   **Architektur**:
        *   Das **RMT-Modul** des ESP32 tastet den rohen Manchester-Chip-Stream mit **8 MHz** (1 Tick = 125 ns) hochauflösend ab.
        *   Ein **Carrier-Sense-Gating-Mechanismus** schützt die CPU vor Interrupt-Stürmen.
    *   **SOF-Erkennung**: Der Decoder identifiziert den Paketstart robust durch die Erkennung der physikalischen **8 µs LOW-Phase** ("Manchester Violation"), die am Übergang von Preamble zu SOF entsteht.
    *   **Gating-Logik (Gehärtet)**:
        *   Der **Carrier Sense (CS) Pin (GDO2)** löst einen GPIO-Interrupt aus.
        *   Der Interrupt wird **sofort im ISR deaktiviert** (`gpio_intr_disable`) und erst nach Verarbeitung des Pakets wieder aktiviert.
7.  **CC1101-Register (Final, für 250 kchip/s OOK)**:
    *   **Frequenz**: Präzise auf **868,300 MHz** kalibriert (`0x21656A` für 26 MHz Quarz).
    *   **Datenrate**: Auf **250 kbps** konfiguriert, um die 125 kbps Manchester-Datenrate abzubilden.
    *   **OOK-Modulation**: `PATABLE` ist korrekt auf `{0x00, 0xC0}` gesetzt und `FREND0` konfiguriert, um echte On-Off-Keying zu erzeugen. `MDMCFG2=0x30` deaktiviert die Hardware-Preamble/-Sync.
    *   **RX-Bandbreite**: Auf **325 kHz** eingestellt.
    *   **AGC**: Konfiguriert, um die Verstärkung bei erkanntem Trägersignal einzufrieren (`AGC_FREEZE_ON_CS`), was den Empfang stabilisiert.
8.  **Concurrency Management (Final)**:
    *   Die gesamte SPI-Kommunikation mit dem CC1101 wird durch einen **Mutex** geschützt.
9.  **Task Management (Final)**:
    *   Der Stack des USB-Empfangstasks wurde präventiv auf **8192 Bytes** erhöht.

### Abgeschlossene Aufgaben (Development Log)

*   **DONE**: Basis-Projektstruktur mit PlatformIO und ESP-IDF aufgesetzt.
*   **DONE**: SPI-Treiber und HAL für CC1101-Kommunikation implementiert (`radio_hal.c`).
*   **DONE**: ESP3-Protokoll-Parser für Sende-Telegramme (Typ 1) implementiert.
*   **DONE**: ESP3-Antwort-Pakete (Typ 2, `RET_OK`) implementiert.
*   **DONE**: Problem mit störenden System-Logs auf dem USB-Datenstrom identifiziert und nachhaltig behoben (inkl. Bootloader-Logs).
*   **DONE**: SPI-Kommunikation mit einem **Mutex** abgesichert.
*   **DONE**: Stack-Größe des USB-Empfangstasks präventiv auf **8192 Bytes** erhöht.
*   **DONE**: Carrier-Sense-ISR durch sofortiges Deaktivieren des Interrupts (`gpio_intr_disable`) **gehärtet**.
*   **DONE**: ESP3-Protokoll-Parser von `malloc` auf einen **statischen Puffer** umgestellt.
*   **DONE**: SPI-Burst-Funktionen (`cc1101_write/read_burst`) auf **atomare Transaktionen** korrigiert.
*   **DONE**: **Kritische Reboot-Schleife (`rst:0xc SW_CPU`) unter Last vollständig behoben.**
*   **DONE**: RF-Sendestrategie fundamental überarbeitet: Korrekte ERP1-Frame-Struktur (Preamble, 1-Bit SOF) implementiert.
*   **DONE**: Empfangs-Decoder auf hochauflösendes RMT (8 MHz) umgestellt.
*   **DONE**: SOF-Erkennung im RMT-Decoder auf physikalische 8µs-Pulsweite (Manchester Violation) gehärtet.
*   **DONE**: CC1101-Register für robustes 250k-OOK Senden/Empfangen optimiert (Frequenz, PATABLE, AGC-Freeze).
*   **DONE**: Dynamische Umschaltung des CC1101 zwischen RX (async) und TX (packet) Modus implementiert.
*   **DONE**: USB-Kommunikation im `diff_test_enocean.py` erfolgreich validiert.