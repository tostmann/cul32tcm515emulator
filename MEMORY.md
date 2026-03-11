# Projekt-Gedächtnis: TCM-Emu-Gemini

### Projektziel
Entwicklung einer Firmware für den ESP32-C6, die ein EnOcean TCM515 (ESP3-Protokoll) USB-Gateway emuliert. Die Hardware-Basis besteht aus einem ESP32-C6-Modul und einem CC1101 Transceiver für das 868-MHz-Band.

### Aktueller Stand
**Die Firmware ist stabil.** Die kritischen Reboot-Schleifen (`rst:0xc SW_CPU`), die unter Last im Differentialtest auftraten, wurden **vollständig behoben**. Die Stabilität wurde durch mehrere, tiefgreifende Korrekturen auf Hardware- und Software-Ebene erreicht. Der finale Differentialtest (`diff_test_enocean.py`) wird nun erfolgreich durchlaufen, und der Emulator antwortet korrekt auf ESP3-Befehle vom Host.

### Nächste Schritte
*   **End-to-End-Validierung**: Testen der Funkkommunikation gegen reale EnOcean-Sensoren und -Aktoren.
*   **Feinabstimmung**: Ggf. Kalibrierung der RSSI-Werte und des LBT-Schwellwerts für den Praxiseinsatz.
*   **Code-Refactoring**: Aufräumen des Codes und Hinzufügen von Kommentaren vor dem finalen Release.

### Architektur-Entscheidungen

1.  **Framework**: ESP-IDF mit PlatformIO für robustes Build-Management und Komponenten-Handling.
2.  **Kommunikations-Schnittstelle**: Der native USB-JTAG-Controller des ESP32-C6 wird für den ESP3-Datenstrom verwendet. System- und Bootloader-Logs werden via `sdkconfig.defaults` zur Laufzeit vollständig deaktiviert, um einen sauberen Binär-Datenstrom zu gewährleisten.
3.  **Protokoll-Handling**:
    *   Eine dedizierte State-Machine (`esp3_proto.c`) parst den eingehenden ESP3-Stream byte-weise.
    *   **(Neu)** **Statischer Payload-Puffer**: Die dynamische Speicherallokation (`malloc`) im Parser wurde durch einen **statischen Puffer** ersetzt, um Heap-Fragmentierung unter Last vollständig zu eliminieren.
4.  **Radio Abstraction**:
    *   Eine Hardware Abstraction Layer (`radio_hal.c`) kapselt die gesamte SPI-Kommunikation und Konfiguration des CC1101.
    *   **(Neu)** **Atomare SPI-Transaktionen**: Die Burst-Funktionen (`cc1101_write/read_burst`) wurden korrigiert und verwenden nun **eine einzige, atomare SPI-Transaktion**. Dies behebt Hardware-Fehler, bei denen die CS-Leitung fälschlicherweise zwischen Adress- und Datenphase deaktiviert wurde.
5.  **Sendestrategie (Final)**:
    *   **Modus**: **Software-Manchester-Encoding** mittels einer schnellen Lookup-Table (LUT), um Timing-Fehler des CC1101-Hardware-Encoders zu umgehen.
    *   **CC1101-Konfiguration**: Der CC1101 wird während des Sendens auf eine rohe Chiprate von **250 kbaud** konfiguriert.
6.  **Empfangsstrategie (Final, gehärtet)**:
    *   **Architektur**: Ein **Carrier-Sense-Gating-Mechanismus** schützt die CPU vor Interrupt-Stürmen durch OOK-Rauschen.
    *   **Gating-Logik (Gehärtet)**:
        *   Der **Carrier Sense (CS) Pin (GDO2)** löst einen GPIO-Interrupt aus.
        *   Der Interrupt wird **sofort im ISR deaktiviert** (`gpio_intr_disable`), um Flankenstürme zu verhindern, und erst am Ende der Verarbeitungs-Schleife im Task wieder aktiviert.
        *   Das **RMT-Modul** wird erst **nach** diesem Interrupt für den Empfang aktiviert.
    *   **DMA-Puffer**: Der RMT-Empfangspuffer wurde auf **1024 Symbole** vergrößert und auf DMA-fähigen Speicher (`heap_caps_calloc`) umgestellt.
    *   **Hardware-Anpassung**: Der RMT-Parameter `mem_block_symbols` wurde auf den korrekten Wert von **48** für den ESP32-C6 korrigiert.
7.  **Concurrency Management (Final)**:
    *   Die gesamte SPI-Kommunikation mit dem CC1101 wird durch einen **Mutex** geschützt. Dies verhindert Race Conditions zwischen dem USB-Task (Senden) und dem RF-Empfangstask (RSSI lesen, Status abfragen).
8.  **Task Management (Final)**:
    *   Der Stack des USB-Empfangstasks wurde präventiv auf **8192 Bytes** erhöht, um Stack Overflows bei tiefen Funktionsaufrufen in der Radio-HAL zu vermeiden.

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
*   **DONE**: Sende-Logik von CC1101-Hardware- auf **Software-Manchester-Encoding** umgestellt.
*   **DONE**: CC1101-Register für den RX-Pfad auf 125 kbps OOK optimiert.
*   **DONE**: Manchester-Sync-Word-Suche im Decoder korrigiert.
*   **DONE**: RMT-Empfangspuffer auf **DMA-fähigen Speicher** umgestellt.
*   **DONE**: RMT-Hardware-Parameter (`mem_block_symbols`) an die Spezifikationen des ESP32-C6 (48) angepasst.
*   **DONE**: SPI-Kommunikation mit einem **Mutex** abgesichert, um Race Conditions zu verhindern.
*   **DONE**: Stack-Größe des USB-Empfangstasks präventiv auf **8192 Bytes** erhöht.
*   **DONE**: Carrier-Sense-ISR durch sofortiges Deaktivieren des Interrupts (`gpio_intr_disable`) **gehärtet**, um Interrupt-Stürme zu verhindern.
*   **DONE**: ESP3-Protokoll-Parser von `malloc` auf einen **statischen Puffer** umgestellt, um Heap-Fragmentierung zu verhindern.
*   **DONE**: SPI-Burst-Funktionen (`cc1101_write/read_burst`) auf **atomare Transaktionen** korrigiert.
*   **DONE**: **Kritische Reboot-Schleife (`rst:0xc SW_CPU`) unter Last vollständig behoben.**
*   **DONE**: `diff_test_enocean.py` erfolgreich durchlaufen; Emulator antwortet korrekt mit `RET_OK`.