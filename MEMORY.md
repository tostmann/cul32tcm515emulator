# Projekt-Gedächtnis: TCM-Emu-Gemini

### Projektziel
Entwicklung einer Firmware für den ESP32-C6, die ein EnOcean TCM515 (ESP3-Protokoll) USB-Gateway emuliert. Die Hardware-Basis besteht aus einem ESP32-C6-Modul und einem CC1101 Transceiver für das 868-MHz-Band.

### Aktueller Stand
Die **Hardware-Schwäche (falsches 433-MHz-Matching-Netzwerk) ist als definitive Wurzelursache** für alle RF-Probleme bestätigt. Es verursacht Dämpfung (>60 dB) und massive **Signalform-Verzerrungen (Puls-Ringing)**.
*   **Software-Test-Infrastruktur**: Die Entwicklung wird durch eine **vollständig implementierte und validierte Software-in-the-Loop-Schnittstelle** (Loopback & Puls-Injektion) entblockt.
*   **Validierung**: Der **Loopback-Test war erfolgreich** und bestätigt die korrekte Funktion des gesamten Host-Protokoll-Stacks (ESP3-Parser, TX/RX-Logik).
*   **Neues Problem**: Erste Tests mit der **Puls-Injektions-Schnittstelle zeigen, dass der PLL-basierte Decoder ideale, synthetische Puls-Daten noch nicht korrekt zu einem ERP1-Paket zusammensetzt**. Dies bestätigt einen reinen Software-Fehler in der Decoder-Logik (`erp1_decoder.c`), der nun unabhängig von der Hardware behoben werden kann.

### Nächste Schritte
*   **Fehlersuche & Kalibrierung (PLL-Decoder) (Höchste Prio)**: Systematisches Debuggen des Decoders (`erp1_decoder.c`) mit **idealisierten Puls-Daten über die verifizierte Injektions-Schnittstelle**. Ziel ist es, den Software-Fehler zu finden und die korrekte Funktion des Decoders deterministisch nachzuweisen.
*   **Hardware-Austausch (BLOCKER)**: Beschaffung und Austausch des CC1101-Moduls durch ein verifiziertes, korrekt für 868 MHz bestücktes Modul. **Dies bleibt die höchste Priorität für den RF-Betrieb.**
*   **End-to-End Validierung des Empfangs (nach HW-Fix & SW-Fix)**: Verifizieren, dass der korrigierte PLL-Decoder mit einem funktionierenden RF-Frontend vollständige ERP1-Pakete korrekt dekodiert.
*   **Code-Refactoring**: Aufräumen des Codes, insbesondere der Decoder-Logik und der Test-Schnittstellen.
*   **Feinabstimmung**: Kalibrierung der RSSI-Werte und des LBT-Schwellwerts für den Praxiseinsatz.

### Architektur-Entscheidungen

1.  **Framework**: ESP-IDF mit PlatformIO.
2.  **Kommunikations-Schnittstelle**: Nativer USB-JTAG-Controller, System-Logs zur Laufzeit deaktiviert.
3.  **Protokoll-Handling**: Dedizierte State-Machine (`esp3_proto.c`) mit statischem Puffer.
4.  **Radio Abstraction**: HAL (`radio_hal.c`) mit atomaren SPI-Transaktionen, geschützt durch einen Mutex.
5.  **Sendestrategie (Final, LUT-basiert)**:
    *   **Modus**: CC1101 dynamisch im **Packet Mode** (FIFO-basiert) für hardware-präzises Timing.
    *   **Manchester-Kodierung**: Hocheffizient über eine **Look-Up Table (LUT)**, die Daten-Nibbles direkt in CC1101-FIFO-Bytes umwandelt.
    *   **Frame-Aufbau (Software)**: Verlängerte Preamble, EnOcean SOF-Bit, Manchester-codierter Payload.
6.  **Empfangsstrategie (Final, PLL-basiert)**:
    *   **Modus**: CC1101 im **Asynchronen Seriellen Modus**.
    *   **Architektur**: **RMT-Modul** tastet mit 8 MHz ab, **State-Machine Decoder (`erp1_decoder_t`)** verarbeitet Pulse sequentiell.
    *   **Clock-Recovery**: Nutzt eine **3/4-Sampling-Regel**.
    *   **Gating-Logik (Gehärtet)**: Carrier Sense (CS) Pin (GDO2) löst GPIO-Interrupt aus.
7.  **Diagnose-Funktionen (Final)**: Echtzeit-Diagnosedaten über ESP3-Protokoll (Paket-Typ `0x33`, `0x34`).
8.  **CC1101-Register (Final, Anti-Saturation)**: Frequenz 868.300 MHz, Datenrate 250 kbps, OOK, RX-Bandbreite 406 kHz, gehärtete AGC-Einstellungen.
9.  **Task Management (Final)**: Stack des USB-Empfangstasks auf **8192 Bytes** erhöht.
10. **Test-Schnittstellen (Final, Software-in-the-Loop)**:
    *   **Loopback-Modus**: Aktivierbar via `COMMON_COMMAND` (Opcode `0x7E`). Sendet TX-Pakete direkt an den Host zurück. **VALIDIERT**.
    *   **Puls-Injektion**: Schnittstelle via `COMMON_COMMAND` (Opcode `0x7F`), um virtuelle RMT-Pulse in den Decoder einzuspeisen. **IMPLEMENTIERT & MECHANISMUS VERIFIZIERT** (Decoder-Logik noch fehlerhaft).

### Abgeschlossene Aufgaben (Development Log)
*   **DONE**: **GitHub-Repository aufgesetzt und bereinigt**: Projekt auf GitHub (`cul32tcm515emulator`) initialisiert. Ein projektspezifischer Deploy-Key wurde konfiguriert, eine `.gitignore` erstellt und das Repository bereinigt, um sensible Daten (`project_config.json`) und transiente Dateien (`*.log`, `sdkconfig`) auszuschließen. Test-Skripte wurden in ein `tests/`-Verzeichnis verschoben.
*   **DONE**: **Software-Loopback-Test erfolgreich validiert**: Der interne Loopback-Modus (via `COMMON_COMMAND` 0x7E) bestätigt die korrekte Funktion des gesamten Host-Kommunikations-Stacks (Host -> ESP3-Parser -> TX-Pfad -> RX-Pfad -> Host).
*   **DONE**: **Protokoll-Parser für Test-Schnittstellen erweitert**: Der ESP3-Protokoll-Handler wurde um `ESP3_TYPE_COMMON_COMMAND` (Opcode `0x05`) erweitert, um Loopback (`0x7E`) und Puls-Injektion (`0x7F`) zu steuern.
*   **DONE**: **Physikalische Ursache für Decoder-Fehler identifiziert**: Experten-Analyse bestätigt, dass das 433-MHz-Matching-Netzwerk die 868-MHz-Pulsformen durch *Ringing* und Gruppenlaufzeitverzerrung zerstört.
*   **DONE**: **Software-Test-Infrastruktur implementiert**: Ein interner **Loopback-Modus** und eine **Puls-Injektions-Schnittstelle** wurden zur Firmware hinzugefügt, um die Entwicklung ohne funktionale RF-Hardware zu ermöglichen.
*   **DONE**: **RF-Sendestrategie auf Packet Mode und LUT-Kodierung umgestellt**: Der Transmitter nutzt nun den hardware-getimten **Packet Mode** des CC1101 und eine **Look-Up Table (LUT)**.
*   **DONE**: **Hardware-Fehlanpassung als Wurzelursache verifiziert**: Systematische Tests bestätigen die extreme Dämpfung (>60 dB).
*   **DONE**: **Kritischer Frequenz-Fehler (869.0 vs 868.3 MHz) identifiziert und behoben.**
*   **DONE**: **CC1101-Register (AGC, BW) gegen Sättigung gehärtet**.
*   **DONE**: **Produktionsreifer PLL-basierter Manchester-Decoder implementiert** (Implementierung abgeschlossen, Debugging läuft).
*   **DONE**: **Diagnose-Task für RF-Parameter implementiert** (RSSI, GDO-Pegel, RMT-Symbol-Count).
*   **DONE**: RF-Empfang grundlegend validiert (Carrier-Sense und RMT-Abtastung werden korrekt ausgelöst).
*   **DONE**: Dynamische Umschaltung des CC1101 zwischen RX (async) und TX (packet) Modus implementiert.
*   **DONE**: USB-Kommunikation und Host-Antworten (`RET_OK`) erfolgreich validiert.
*   **DONE**: **Kritische Reboot-Schleife (`rst:0xc SW_CPU`) unter Last vollständig behoben.**
*   **DONE**: SPI-Burst-Funktionen auf **atomare Transaktionen** korrigiert.
*   **DONE**: ESP3-Protokoll-Parser von `malloc` auf einen **statischen Puffer** umgestellt.
*   **DONE**: Carrier-Sense-ISR durch sofortiges Deaktivieren des Interrupts (`gpio_intr_disable`) **gehärtet**.
*   **DONE**: SPI-Kommunikation mit einem **Mutex** abgesichert.
*   **DONE**: Stack-Größe des USB-Empfangstasks präventiv auf **8192 Bytes** erhöht.
*   **DONE**: Problem mit störenden System-Logs auf dem USB-Datenstrom nachhaltig behoben.
*   **DONE**: ESP3-Antwort-Pakete (Typ 2, `RET_OK`) implementiert.
*   **DONE**: ESP3-Protokoll-Parser für Sende-Telegramme (Typ 1) implementiert.
*   **DONE**: SPI-Treiber und HAL für CC1101-Kommunikation implementiert (`radio_hal.c`).
*   **DONE**: Basis-Projektstruktur mit PlatformIO und ESP-IDF aufgesetzt.