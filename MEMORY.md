# Projekt-Gedächtnis: TCM-Emu-Gemini

### Projektziel
Entwicklung einer Firmware für den ESP32-C6, die ein EnOcean TCM515 (ESP3-Protokoll) USB-Gateway emuliert. Die Hardware-Basis besteht aus einem ESP32-C6-Modul und einem CC1101 Transceiver für das 868-MHz-Band.

### Aktueller Stand
Die Kernfunktionalität ist vollständig implementiert und getestet. Die Firmware kann ESP3-Pakete (Typ 1, RADIO_ERP1) über die native USB-Schnittstelle empfangen, eine "Listen-Before-Talk"-Prüfung durchführen, das EnOcean-Telegramm als 3 Sub-Telegramme über den CC1101 aussenden und eingehende Host-Telegramme mit einer ESP3-Erfolgsmeldung (Typ 2, RET_OK) quittieren.

Der Empfang von EnOcean-Telegrammen ist ebenfalls implementiert und setzt auf eine robuste Software-Dekodierung, um Jitter-anfällige Sensoren (z.B. PTM-Schalter) zuverlässig zu empfangen.

Ein Python-Testskript hat die Sende-Funktionalität erfolgreich validiert.

### Architektur-Entscheidungen

1.  **Framework**: ESP-IDF mit PlatformIO für robustes Build-Management und Komponenten-Handling.
2.  **Kommunikations-Schnittstelle**: Der native USB-JTAG-Controller des ESP32-C6 wird für den ESP3-Datenstrom verwendet. System-Logs werden zur Laufzeit auf diesem Port deaktiviert, um einen sauberen Binär-Datenstrom zu gewährleisten.
3.  **Protokoll-Handling**: Eine dedizierte State-Machine (`esp3_proto.c`) parst den eingehenden ESP3-Stream byte-weise. Dies ist speichereffizient und fehlertolerant.
4.  **Radio Abstraction**: Eine Hardware Abstraction Layer (`radio_hal.c`) kapselt die gesamte SPI-Kommunikation und Konfiguration des CC1101.
5.  **Sendestrategie (Implementiert)**:
    *   **Modus**: CC1101 im FIFO-Paketmodus mit Hardware-Manchester-Codierung.
    *   **Listen-Before-Talk (LBT)**: Vor dem Senden wird der RSSI-Wert des CC1101 ausgelesen, um Kollisionen zu vermeiden.
    *   **Subtelegramme**: Jedes Telegramm wird 3-mal mit einem pseudo-zufälligen Abstand (10-25 ms) gesendet, um die Übertragungssicherheit zu erhöhen.
6.  **Empfangsstrategie (Implementiert)**: Die anfängliche Idee, den CC1101-Hardware-Paketmodus zu nutzen, wurde verworfen. Stattdessen wurde eine robustere Software-basierte Architektur umgesetzt:
    *   **CC1101-Modus**: Asynchroner, transparenter serieller Modus. Der CC1101 agiert als reiner OOK-Demodulator und gibt das Basisband-Signal an einem GDO-Pin aus.
    *   **ESP32-Peripherie**: Das **RMT (Remote Control)** Modul des ESP32-C6 empfängt das demodulierte Signal und vermisst die Pulsdauern exakt (1 Tick = 0.25 µs). Ein Hardware-Glitch-Filter (<2 µs) unterdrückt Rauschen.
    *   **Carrier Sense (Squelch)**: Ein zweiter GDO-Pin wird als **Carrier Sense (CS)** konfiguriert. Der RMT-Empfang wird nur bei einem aktiven Signal gestartet, was die CPU-Last drastisch reduziert.
    *   **Dekodierung**: Eine "Half-Bit-Extractor"-Software-Routine dekodiert den Manchester-Code aus den RMT-Daten. Diese Methode ist extrem tolerant gegenüber Timing-Schwankungen (Jitter).

### Abgeschlossene Aufgaben (Development Log)

*   **DONE**: Basis-Projektstruktur mit PlatformIO und ESP-IDF aufgesetzt.
*   **DONE**: SPI-Treiber und HAL für CC1101-Kommunikation implementiert (`radio_hal.c`).
*   **DONE**: Build-System-Fehler behoben (CMake/Git-Integration, `GLOB_RECURSE`).
*   **DONE**: KConfig-Konflikt (`redefinition of 'bootloader_console_init'`) durch Umstellung von `build_flags` auf `sdkconfig.defaults` für die USB-Konsolen-Konfiguration gelöst.
*   **DONE**: ESP3-Protokoll-Parser für Sende-Telegramme (Typ 1) implementiert.
*   **DONE**: ESP3-Antwort-Pakete (Typ 2, `RET_OK`) implementiert.
*   **DONE**: LBT- und Subtelegramm-Logik in die Sende-Routine integriert.
*   **DONE**: Test-Skript in Python zur Validierung der USB-Kommunikation erstellt und erfolgreich ausgeführt.
*   **DONE**: CC1101-Konfiguration auf asynchronen Modus für den Empfang umgestellt.
*   **DONE**: RMT-Receiver zur Erfassung des demodulierten Signals implementiert.
*   **DONE**: Carrier-Sense-Logik (GDO2) zur CPU-Entlastung des RMT-Tasks implementiert.
*   **DONE**: Jitter-toleranten Manchester-Software-Decoder ("Half-Bit-Extractor") implementiert.
*   **DONE**: Kompilierte Binärdateien (`factory.bin`) und ein `manifest.json` für die Veröffentlichung bereitgestellt.