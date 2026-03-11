# Projekt-Gedächtnis: TCM-Emu-Gemini

### Projektziel
Entwicklung einer Firmware für den ESP32-C6, die ein EnOcean TCM515 (ESP3-Protokoll) USB-Gateway emuliert. Die Hardware-Basis besteht aus einem ESP32-C6-Modul und einem CC1101 Transceiver für das 868-MHz-Band.

### Aktueller Stand
Die Firmware ist architektonisch fertiggestellt und stabilisiert. Nach einer tiefgreifenden Fehleranalyse wurden kritische Probleme wie Reboot-Schleifen (`rst:0xc SW_CPU`) und Watchdog-Timeouts (verursacht durch Interrupt-Stürme) behoben. Sowohl der Sende- als auch der Empfangspfad sind nun auf die spezifischen Anforderungen des 125-kbps-OOK-Betriebs optimiert und gehärtet. Die Basis für einen erfolgreichen Differentialtest ist gelegt.

### Offene Punkte / Aktuelle Probleme
*   **Validierung ausstehend**: Nach der kompletten Überarbeitung der Radio-Abstraktionsschicht muss ein finaler Differentialtest gegen die TCM515-Referenzhardware durchgeführt werden, um die Korrektheit der Sende- und Empfangslogik endgültig zu bestätigen.

### Architektur-Entscheidungen

1.  **Framework**: ESP-IDF mit PlatformIO für robustes Build-Management und Komponenten-Handling.
2.  **Kommunikations-Schnittstelle**: Der native USB-JTAG-Controller des ESP32-C6 wird für den ESP3-Datenstrom verwendet. System- und Bootloader-Logs werden via `sdkconfig.defaults` zur Laufzeit vollständig deaktiviert, um einen sauberen Binär-Datenstrom zu gewährleisten.
3.  **Protokoll-Handling**: Eine dedizierte State-Machine (`esp3_proto.c`) parst den eingehenden ESP3-Stream byte-weise. Dies ist speichereffizient und fehlertolerant.
4.  **Radio Abstraction**: Eine Hardware Abstraction Layer (`radio_hal.c`) kapselt die gesamte SPI-Kommunikation und Konfiguration des CC1101 sowie die RMT-basierte Empfangslogik.
5.  **Sendestrategie (Neu implementiert)**:
    *   **Grund**: Der Hardware-Manchester-Encoder des CC1101 erzeugt Timing-Fehler bei ASK/OOK-Modulation.
    *   **Modus**: **Software-Manchester-Encoding** mittels einer schnellen Lookup-Table (LUT).
    *   **CC1101-Konfiguration**: Der CC1101 wird während des Sendens auf eine rohe Chiprate von **250 kbaud** konfiguriert, um die 125-kbps-Manchester-Daten 1:1 zu übertragen. Preamble und Sync-Word werden weiterhin von der Hardware erzeugt und haben durch die höhere Baudrate das korrekte EnOcean-Timing.
    *   **LBT & Subtelegramme**: bleiben erhalten (RSSI-Prüfung und 3-faches Senden).
6.  **Empfangsstrategie (Final, gehärtet)**:
    *   **Architektur**: Ein **Carrier-Sense-Gating-Mechanismus** wurde implementiert, um die CPU vor Interrupt-Stürmen durch OOK-Rauschen zu schützen.
    *   **CC1101-Modus**: Asynchroner, transparenter serieller Modus. Die Register für Datenrate (`MDMCFG3/4`), Filter und AGC sind auf TI Best-Practices für 125 kbps OOK optimiert.
    *   **ESP32-Peripherie & Gating-Logik**:
        *   Der **Carrier Sense (CS) Pin (GDO2)** löst einen GPIO-Interrupt aus und signalisiert den Beginn eines potenziellen Pakets.
        *   Das **RMT-Modul** wird erst **nach** diesem Interrupt aktiviert und lauscht am Datenpin **GDO0**.
        *   Sobald der Carrier Sense abfällt oder der RMT einen Timeout hat, wird der RMT-Kanal sofort deaktiviert, was Watchdog-Resets verhindert.
    *   **Robuster RX-Task**: Der kritische Reboot-Bug (`rst:0xc SW_CPU`), verursacht durch `ESP_ERR_INVALID_STATE` im RMT-Treiber, wurde durch defensive Fehlerbehandlung behoben. Die Task-Logik fängt Fehler und Timeouts nun ab und setzt den RMT-Kanal (`rmt_disable`/`rmt_enable`) zurück, anstatt abzustürzen.
    *   **DMA-Puffer**: Der RMT-Empfangspuffer wurde auf **1024 Symbole** vergrößert und auf DMA-fähigen Speicher (`heap_caps_calloc`) umgestellt, um die hohe Datenrate stabil zu verarbeiten.
    *   **Hardware-Anpassung**: Der RMT-Parameter `mem_block_symbols` wurde auf den korrekten Wert von **48** für den ESP32-C6 korrigiert.

### Abgeschlossene Aufgaben (Development Log)

*   **DONE**: Basis-Projektstruktur mit PlatformIO und ESP-IDF aufgesetzt.
*   **DONE**: SPI-Treiber und HAL für CC1101-Kommunikation implementiert (`radio_hal.c`).
*   **DONE**: Build-System-Fehler behoben (CMake/Git-Integration, `GLOB_RECURSE`).
*   **DONE**: KConfig-Konflikt (`redefinition of 'bootloader_console_init'`) gelöst.
*   **DONE**: ESP3-Protokoll-Parser für Sende-Telegramme (Typ 1) implementiert.
*   **DONE**: ESP3-Antwort-Pakete (Typ 2, `RET_OK`) implementiert.
*   **DONE**: LBT- und Subtelegramm-Logik in die Sende-Routine integriert.
*   **DONE**: Test-Skript in Python zur Validierung der USB-Kommunikation erstellt.
*   **DONE**: Differentialtest-Skript (`diff_test_enocean.py`) erstellt.
*   **DONE**: Problem mit störenden System-Logs auf dem USB-Datenstrom identifiziert und durch Anpassung von `sdkconfig.defaults` nachhaltig behoben (inkl. Bootloader-Logs).
*   **DONE**: Sende-Logik von CC1101-Hardware- auf **Software-Manchester-Encoding** umgestellt, um Timing-Inkompatibilitäten zu beheben.
*   **DONE**: CC1101-Register für den RX-Pfad (Datenrate, Filter, AGC) auf 125 kbps OOK optimiert.
*   **DONE**: Manchester-Sync-Word-Suche im Decoder korrigiert.
*   **DONE**: Architektur auf **Carrier-Sense-Gating** umgestellt, um Interrupt-Stürme durch OOK-Rauschen zu unterbinden und Watchdog-Resets zu verhindern.
*   **DONE**: **Kritischen Reboot-Fehler (`rst:0xc SW_CPU`) im RMT-Empfangstask behoben** durch defensive Fehlerbehandlung und manuelles Zurücksetzen des RMT-Kanals.
*   **DONE**: RMT-Empfangspuffer auf 1024 Symbole vergrößert und auf **DMA-fähigen Speicher** umgestellt.
*   **DONE**: RMT-Hardware-Parameter (`mem_block_symbols`) an die Spezifikationen des ESP32-C6 (48) angepasst.