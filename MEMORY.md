# Projekt-Gedächtnis: TCM-Emu-Gemini

### Projektziel
Entwicklung einer Firmware für den ESP32-C6, die ein EnOcean TCM515 (ESP3-Protokoll) USB-Gateway emuliert. Die Hardware-Basis besteht aus einem ESP32-C6-Modul und einem CC1101 Transceiver für das 868-MHz-Band.

### Aktueller Stand
**Ein kritischer Meilenstein wurde erreicht.** Der **Empfang und das korrekte Dekodieren** von realen EnOcean-ERP1-Paketen, die von einem TCM515 gesendet werden, ist **voll funktionsfähig**. Der Differentialtest (`diff_test_enocean.py`) läuft für die Richtung TCM515 -> Emulator erfolgreich durch; der Host empfängt das dekodierte RF-Paket korrekt. Die Firmware ist stabil, die USB-Kommunikation fehlerfrei und die Reboot-Schleifen (`rst:0xc SW_CPU`) sind vollständig behoben.

### Nächste Schritte
*   **Validierung des Sendevorgangs**: Der Emulator sendet RF-Pakete, aber der reale TCM515 empfängt diese noch nicht. Die Ursache muss analysiert werden (Timing, Power, Frame-Struktur). **Dies ist die höchste Priorität.**
*   **Timing-Feinabstimmung (PLL-Decoder)**: Anpassen der Pulsweiten-Toleranzen (`MIN_1T`, `MAX_2T` etc.) im PLL-Decoder, um die Empfangssicherheit bei unterschiedlichen Signalstärken weiter zu maximieren.
*   **Code-Refactoring**: Aufräumen des Codes, insbesondere der neuen Decoder-Logik (`erp1_decoder.c`), und Hinzufügen von Kommentaren.
*   **Feinabstimmung**: Kalibrierung der RSSI-Werte und des LBT-Schwellwerts für den Praxiseinsatz.

### Architektur-Entscheidungen

1.  **Framework**: ESP-IDF mit PlatformIO.
2.  **Kommunikations-Schnittstelle**: Nativer USB-JTAG-Controller, System-Logs zur Laufzeit deaktiviert.
3.  **Protokoll-Handling**: Dedizierte State-Machine (`esp3_proto.c`) mit statischem Puffer.
4.  **Radio Abstraction**: HAL (`radio_hal.c`) mit atomaren SPI-Transaktionen, geschützt durch einen Mutex.
5.  **Sendestrategie (Final, ERP1-konform)**:
    *   **Modus**: CC1101 dynamisch im **Packet Mode** (FIFO-basiert).
    *   **Frame-Aufbau (Software)**: Der gesamte physikalische ERP1-Frame wird in Software generiert:
        *   `0xAAAA...` (Warm-Up für CC1101 OOK-Modulator)
        *   EnOcean Preamble (Logische `1`er)
        *   EnOcean **Start-of-Frame (SOF) Bit** (Einzelne logische `0`)
        *   Manchester-codierter Payload (MSB first)
6.  **Empfangsstrategie (Final, PLL-basiert)**:
    *   **Modus**: CC1101 im **Asynchronen Seriellen Modus**.
    *   **Architektur**:
        *   Das **RMT-Modul** tastet den rohen Manchester-Chip-Stream mit **8 MHz** (1 Tick = 125 ns) ab.
        *   **PLL-basierter Manchester-Decoder**: Eine robuste State-Machine (`erp1_decoder.c`) wurde implementiert:
            *   **SOF-Erkennung**: Identifiziert den Paketstart durch die **8 µs LOW-Phase** nach einer Serie von Preamble-Pulsen.
            *   **Clock-Recovery**: Nutzt eine **3/4-Sampling-Regel** (Abtastung bei 6 µs im 8 µs Bit-Fenster), um gegen Jitter und Duty-Cycle-Verzerrungen immun zu sein.
            *   **Multi-Block-fähig**: Der Zustand des Decoders bleibt über mehrere RMT-DMA-Blöcke hinweg erhalten.
    *   **Gating-Logik (Gehärtet)**: Der Carrier Sense (CS) Pin (GDO2) löst einen GPIO-Interrupt aus, der die Auswertung startet. Der Interrupt wird sofort im ISR deaktiviert.
7.  **Diagnose-Funktionen (Neu)**:
    *   Ein dedizierter Task sendet Echtzeit-Diagnosedaten über das ESP3-Protokoll an den Host.
    *   **Paket-Typ 0x33**: Sendet die Anzahl der empfangenen RMT-Symbole zur Analyse.
    *   **Paket-Typ 0x34**: Sendet den aktuellen RSSI-Wert sowie die GDO0- und GDO2-Pegel.
8.  **CC1101-Register (Final, für 250 kchip/s OOK)**:
    *   **Frequenz**: **868,300 MHz** (`0x21656A`).
    *   **Datenrate**: **250 kbps** für 125 kbps Manchester.
    *   **OOK-Modulation**: `PATABLE` = `{0x00, 0xC0}`, `FREND0` konfiguriert, `MDMCFG2=0x30` (deaktiviert HW-Preamble/Sync).
    *   **RX-Bandbreite**: **325 kHz**.
    *   **AGC**: Konfiguriert, um die Verstärkung bei erkanntem Trägersignal einzufrieren (`AGC_FREEZE_ON_CS`).
9.  **Task Management (Final)**: Stack des USB-Empfangstasks auf **8192 Bytes** erhöht.

### Abgeschlossene Aufgaben (Development Log)

*   **DONE**: **RF-Empfang validiert**: Empfang und Dekodierung von ERP1-Paketen eines echten TCM515 ist funktional.
*   **DONE**: **PLL-basierter Manchester-Decoder implementiert**: Der RMT-Empfänger wurde auf eine robuste State-Machine mit Clock-Recovery (3/4-Sampling) umgestellt.
*   **DONE**: **Diagnose-Task für RF-Parameter implementiert** (RSSI, GDO-Pegel).
*   **DONE**: RF-Sendestrategie fundamental überarbeitet: Korrekte ERP1-Frame-Struktur (Preamble, 1-Bit SOF) implementiert.
*   **DONE**: CC1101-Register für robustes 250k-OOK Senden/Empfangen optimiert (Frequenz, PATABLE, AGC-Freeze).
*   **DONE**: Dynamische Umschaltung des CC1101 zwischen RX (async) und TX (packet) Modus implementiert.
*   **DONE**: USB-Kommunikation und Host-Antworten (`RET_OK`) im `diff_test_enocean.py` erfolgreich validiert.
*   **DONE**: **Kritische Reboot-Schleife (`rst:0xc SW_CPU`) unter Last vollständig behoben.**
*   **DONE**: SPI-Burst-Funktionen (`cc1101_write/read_burst`) auf **atomare Transaktionen** korrigiert.
*   **DONE**: ESP3-Protokoll-Parser von `malloc` auf einen **statischen Puffer** umgestellt.
*   **DONE**: Carrier-Sense-ISR durch sofortiges Deaktivieren des Interrupts (`gpio_intr_disable`) **gehärtet**.
*   **DONE**: SPI-Kommunikation mit einem **Mutex** abgesichert.
*   **DONE**: Stack-Größe des USB-Empfangstasks präventiv auf **8192 Bytes** erhöht.
*   **DONE**: Problem mit störenden System-Logs auf dem USB-Datenstrom nachhaltig behoben (inkl. Bootloader-Logs).
*   **DONE**: ESP3-Antwort-Pakete (Typ 2, `RET_OK`) implementiert.
*   **DONE**: ESP3-Protokoll-Parser für Sende-Telegramme (Typ 1) implementiert.
*   **DONE**: SPI-Treiber und HAL für CC1101-Kommunikation implementiert (`radio_hal.c`).
*   **DONE**: Basis-Projektstruktur mit PlatformIO und ESP-IDF aufgesetzt.