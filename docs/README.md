# Documentazione Tecnica Attack Shark X11 Linux

Questa directory raccoglie la documentazione tecnica e i risultati dell'attività di reverse engineering condotta sul mouse da gioco **Attack Shark X11** e sul relativo software ufficiale Windows.

---

## Indice dei Documenti

1. **[REVERSE_ENGINEERING.md](file:///var/home/simone/GitHub/attack-shark-x11-linux/docs/REVERSE_ENGINEERING.md)**:
   * Relazione completa sull'analisi dei binari ufficiali (`AttackShark.exe` e `hiddriver_3.dll`).
   * Spiegazione dettagliata con frammenti di codice assembly (disassembly x86) del dispatcher dei comandi.
   * Diagnosi e risoluzione dei bug storici (errore ACK `03 10 50 01 04`, tabelle sensore PAW3311, checksum a 16 bit).
   * Spiegazione dell'ADC batteria a intervalli discreti del 10% e della sincronizzazione hardware del tasto DPI.
   * Gestione dell'architettura dual-mode (cablata `1d57:fa61` con spegnimento modulo radio vs wireless `1d57:fa60`).

2. **[PROTOCOL.md](file:///var/home/simone/GitHub/attack-shark-x11-linux/docs/PROTOCOL.md)**:
   * Riferimento rapido per sviluppatori sul protocollo USB HID a basso livello.
   * Layout esatto dei pacchetti Feature Report a 64 byte (`Report 0x04` DPI, `Report 0x05` RGB, `Report 0x06` Polling Rate).
   * Formato degli Interrupt Input Report asincroni (ACK, tasto DPI fisico, telemetria batteria).
   * Algoritmi di checksum e tabelle di conversione hardware.

