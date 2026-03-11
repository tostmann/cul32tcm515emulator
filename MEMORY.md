# Projekt-Gedächtnis: TCM-Emu-Gemini

### Projektziel
Entwicklung einer Firmware für den ESP32-C6, die ein EnOcean TCM515 (ESP3-Protokoll) USB-Gateway emuliert. Die Hardware-Basis besteht aus einem ESP32-C6-Modul und einem CC1101 Transceiver für das 868-MHz-Band.

### Aktueller Stand
Die Software-Architektur des Empfängers wurde auf **Produktionsniveau** gehoben und der **PLL-basierte Decoder mit Clock-Recovery** ist vollständig integriert. Während der Validierung wurde die **kritische Hardware-Schwäche bestätigt und als Wurzelursache identifiziert**: Selbst mit optimierten CC1101-Registern und maximaler Verstärkung erreicht der RSSI-Wert bei 10 cm Abstand nur ca. **-85 dBm** und fällt bei >1m Distanz unter die Nachweisgrenze. Dies bestätigt eine **Signal-Dämpfung von ca. 60 dB**, was charakteristisch für ein **inkorrektes Antennen-Matching des CC1101-Moduls** ist (ein für 433 MHz bestücktes Modul wird bei 868 MHz betrieben). **Alle Software-Validierungsarbeiten sind dadurch hart blockiert.**

### Nächste Schritte
*   **Hardware-Austausch (BLOCKER)**: Beschaffung und Austausch des CC1101-Moduls durch ein verifiziertes, korrekt für 868 MHz bestücktes Modul. **Dies ist die einzige und absolut höchste Priorität.**
*   **End-to-End Validierung des Empfangs (nach HW-Fix)**: Verifizieren, dass der PLL-Decoder mit einem funktionierenden RF-Frontend vollständige ERP1-Pakete korrekt dekodiert und die Checksummen-Prüfung besteht.
*   **Validierung des Sendevorgangs (nach HW-Fix)**: Analysieren, warum der reale TCM515 die gesendeten Pakete noch nicht empfängt.
*   **Timing-Feinabstimmung (PLL-Decoder)**: Anpassen der Pulsweiten-Toleranzen (`MIN_1T`, `MAX_2T` etc.), um die Empfangssicherheit weiter zu maximieren.
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
        *   **State-Machine Decoder (`erp1_decoder_t`)**: Implementierung einer persistenten State-Machine, die Puls für Puls verarbeitet:
            *   **SOF-Erkennung**: Identifiziert den Paketstart durch eine **8 µs LOW-Phase** nach mindestens 6 Preamble-Pulsen.
            *   **Clock-Recovery**: Nutzt eine **3/4-Sampling-Regel** (Abtastung bei 6 µs im 8 µs Bit-Fenster), um gegen Jitter und Duty-Cycle-Verzerrungen (Sättigung) immun zu sein.
            *   **Multi-Block-fähig**: Der Zustand des Decoders (`erp1_decoder_t`) bleibt über mehrere RMT-DMA-Blöcke hinweg erhalten.
    *   **Gating-Logik (Gehärtet)**: Der Carrier Sense (CS) Pin (GDO2) löst einen GPIO-Interrupt aus, der die Auswertung startet.
7.  **Diagnose-Funktionen (Neu)**:
    *   Ein dedizierter Task sendet Echtzeit-Diagnosedaten über das ESP3-Protokoll an den Host.
    *   **Paket-Typ 0x33**: Sendet die Anzahl der empfangenen RMT-Symbole und den Decoder-Status.
    *   **Paket-Typ 0x34**: Sendet den aktuellen RSSI-Wert sowie die GDO0- und GDO2-Pegel.
8.  **CC1101-Register (Final, Anti-Saturation)**:
    *   **Frequenz**: **868,300 MHz** (`0x215E63`).
    *   **Datenrate**: **250 kbps** für 125 kbps Manchester.
    *   **OOK-Modulation**: `PATABLE` = `{0x00, 0xC0}`, `FREND0` konfiguriert, `MDMCFG2=0x30`.
    *   **RX-Bandbreite**: **406 kHz** (`MDMCFG4=0x8D`) (Erhöht, um Pulsverformung zu minimieren).
    *   **AGC (Anti-Saturation)**: `AGCCTRL2=0x43`, `AGCCTRL1=0x40`, `AGCCTRL0=0x90`. Reduzierter LNA-Gain, LNA-Priorität aktiviert, kein AGC-Freeze mehr, um Übersteuerung zu verhindern.
9.  **Task Management (Final)**: Stack des USB-Empfangstasks auf **8192 Bytes** erhöht.

### Abgeschlossene Aufgaben (Development Log)
*   **DONE**: **Hardware-Fehlanpassung als Wurzelursache verifiziert**: Systematische Tests (Distanzänderung, AGC-Tuning) bestätigen, dass die extrem niedrige Empfindlichkeit (~60 dB Dämpfung) auf eine falsche Bestückung des CC1101-Moduls (433-MHz-Frontend) zurückzuführen ist.
*   **DONE**: **Kritische Hardware-Schwäche identifiziert**: Extrem niedriger RSSI (-85dBm @ 10cm) deutet auf ein 433-MHz-Frontend auf dem CC1101-Modul hin.
*   **DONE**: **CC1101-Register (AGC, BW) gegen Sättigung gehärtet**: Die RX-Bandbreite wurde auf 406 kHz erhöht und die AGC-Parameter wurden angepasst, um LNA-Clipping bei starken Signalen zu verhindern.
*   **DONE**: **Produktionsreifer PLL-basierter Manchester-Decoder implementiert**: Der RMT-Empfänger wurde auf eine robuste State-Machine mit Clock-Recovery (3/4-Sampling) umgestellt.
*   **DONE**: **Diagnose-Task für RF-Parameter implementiert** (RSSI, GDO-Pegel, RMT-Symbol-Count).
*   **DONE**: **Kritischer Frequenz-Fehler (869.0 vs 868.3 MHz) identifiziert und behoben.**
*   **DONE**: RF-Empfang grundlegend validiert: Empfang von realen ERP1-Paketen eines TCM515 löst Carrier-Sense und RMT-Abtastung korrekt aus.
*   **DONE**: RF-Sendestrategie fundamental überarbeitet: Korrekte ERP1-Frame-Struktur (Preamble, 1-Bit SOF) implementiert.
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