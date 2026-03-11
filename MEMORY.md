# Projekt-Gedächtnis: TCM-Emu-Gemini

### Projektziel
Entwicklung einer Firmware für den ESP32-C6, die ein EnOcean TCM515 (ESP3-Protokoll) USB-Gateway emuliert. Die Hardware-Basis besteht aus einem ESP32-C6-Modul und einem CC1101 Transceiver für das 868-MHz-Band.

### Aktueller Stand
Die Kernfunktionalität ist implementiert. Nach tiefgreifenden Analysen und Tests wurden sowohl der Sende- als auch der Empfangspfad fundamental überarbeitet, um Kompatibilitätsprobleme mit der Referenzhardware und Stabilitätsfehler (Reboot-Schleifen) zu beheben. Die Firmware ist nun theoretisch robust für den 125-kbps-OOK-Betrieb.

### Offene Punkte / Aktuelle Probleme
*   **Validierung ausstehend**: Nach der kompletten Überarbeitung der Radio-Abstraktionsschicht muss ein finaler Differentialtest gegen die TCM515-Referenzhardware durchgeführt werden, um die Korrektheit der Sende- und Empfangslogik endgültig zu bestätigen.

### Architektur-Entscheidungen

1.  **Framework**: ESP-IDF mit PlatformIO für robustes Build-Management und Komponenten-Handling.
2.  **Kommunikations-Schnittstelle**: Der native USB-JTAG-Controller des ESP32-C6 wird für den ESP3-Datenstrom verwendet. System-Logs werden zur Laufzeit auf diesem Port deaktiviert, um einen sauberen Binär-Datenstrom zu gewährleisten.
3.  **Protokoll-Handling**: Eine dedizierte State-Machine (`esp3_proto.c`) parst den eingehenden ESP3-Stream byte-weise. Dies ist speichereffizient und fehlertolerant.
4.  **Radio Abstraction**: Eine Hardware Abstraction Layer (`radio_hal.c`) kapselt die gesamte SPI-Kommunikation und Konfiguration des CC1101 sowie die RMT-basierte Empfangslogik.
5.  **Sendestrategie (Neu implementiert)**:
    *   **Grund**: Der Hardware-Manchester-Encoder des CC1101 erzeugt Timing-Fehler bei ASK/OOK-Modulation.
    *   **Modus**: **Software-Manchester-Encoding** mittels einer schnellen Lookup-Table (LUT).
    *   **CC1101-Konfiguration**: Der CC1101 wird während des Sendens auf eine rohe Chiprate von **250 kbaud** konfiguriert, um die 125-kbps-Manchester-Daten 1:1 zu übertragen. Preamble und Sync-Word werden weiterhin von der Hardware erzeugt, passen aber durch die höhere Baudrate zum EnOcean-Timing.
    *   **LBT & Subtelegramme**: bleiben erhalten (RSSI-Prüfung und 3-faches Senden).
6.  **Empfangsstrategie (Gehärtet)**: Die RMT-basierte Software-Dekodierung wurde beibehalten und maßgeblich stabilisiert.
    *   **CC1101-Modus**: Asynchroner, transparenter serieller Modus. Die Register für Datenrate (125kbps) und AGC sind auf TI Best-Practices für OOK optimiert.
    *   **ESP32-Peripherie**: Das RMT-Modul empfängt das demodulierte Signal von **GDO0**.
    *   **Robuster RX-Task**: Ein kritischer Reboot-Bug (`rst:0xc SW_CPU`), verursacht durch `ESP_ERR_INVALID_STATE` im RMT-Treiber, wurde behoben. Die Task-Logik fängt Fehler und Timeouts nun ab und setzt den RMT-Kanal defensiv zurück (`rmt_disable`/`rmt_enable`), anstatt abzustürzen.
    *   **Hybrid-Trigger**: Der RMT-Empfang wird nicht permanent aktiv gehalten. Stattdessen wird der Task durch den **Carrier Sense (CS) Pin (GDO2)** per Interrupt getriggert, was die CPU-Last minimiert.
    *   **Buffer**: Der RMT-Symbol-Puffer wurde auf 1024 Einträge verdoppelt, um längere Pakete bei 125 kbps sicher zu erfassen.

### Abgeschlossene Aufgaben (Development Log)

*   **DONE**: Basis-Projektstruktur mit PlatformIO und ESP-IDF aufgesetzt.
*   **DONE**: SPI-Treiber und HAL für CC1101-Kommunikation implementiert (`radio_hal.c`).
*   **DONE**: Build-System-Fehler behoben (CMake/Git-Integration, `GLOB_RECURSE`).
*   **DONE**: KConfig-Konflikt (`redefinition of 'bootloader_console_init'`) gelöst.
*   **DONE**: ESP3-Protokoll-Parser für Sende-Telegramme (Typ 1) implementiert.
*   **DONE**: ESP3-Antwort-Pakete (Typ 2, `RET_OK`) implementiert.
*   **DONE**: LBT- und Subtelegramm-Logik in die Sende-Routine integriert.
*   **DONE**: Test-Skript in Python zur Validierung der USB-Kommunikation erstellt.
*   **DONE**: CC1101-Konfiguration auf asynchronen Modus für den Empfang umgestellt.
*   **DONE**: RMT-Receiver zur Erfassung des demodulierten Signals implementiert.
*   **DONE**: Carrier-Sense-Logik (GDO2) zur CPU-Entlastung des RMT-Tasks implementiert.
*   **DONE**: Jitter-toleranten Manchester-Software-Decoder ("Half-Bit-Extractor") implementiert.
*   **DONE**: Anwendungslogik zur Weiterleitung empfangener EnOcean-Pakete an den Host implementiert.
*   **DONE**: Kompilierte Binärdateien und `manifest.json` bereitgestellt.
*   **DONE**: Differentialtest-Skript (`diff_test_enocean.py`) erstellt.
*   **DONE**: Problem mit störenden System-Logs auf dem USB-Datenstrom identifiziert und durch Anpassung von `sdkconfig.defaults` nachhaltig behoben (inkl. Bootloader-Logs).
*   **DONE**: Sende-Logik von CC1101-Hardware- auf **Software-Manchester-Encoding** umgestellt, um Timing-Inkompatibilitäten zu beheben.
*   **DONE**: CC1101-Register für den RX-Pfad (Datenrate, Filter, AGC) auf 125 kbps OOK optimiert.
*   **DONE**: Manchester-Sync-Word-Suche im Decoder korrigiert.
*   **DONE**: **Kritischen Reboot-Fehler (`rst:0xc SW_CPU`) im RMT-Empfangstask behoben** durch defensive Fehlerbehandlung und manuelles Zurücksetzen des RMT-Kanals.
*   **DONE**: RMT-Empfangspuffer auf 1024 Symbole vergrößert.