# Relazione Tecnica di Reverse Engineering: Attack Shark X11

**Target Hardware**: Gaming Mouse Attack Shark X11 (Sensore PixArt PAW3311 / Controller Beken BK2535/BK52832)  
**Target Software Originale**: `ATTACK SHARK X11 SOFT.exe` (Versione Windows ufficiale)  
**Ambiente di Analisi**: Linux (Distrobox Ubuntu 26.04 LTS su host Bazzite / Fedora Atomic)  
**Autore**: Progetto `attack-shark-x11-linux`

---

## 1. Obiettivo dell'Analisi

Il software ufficiale fornito dal produttore per il mouse gaming **Attack Shark X11** è disponibile esclusivamente come applicazione Win32. Il tentativo iniziale di implementare un driver Linux nativo basato sul reverse engineering amatoriale (`hook.cpp`) presentava gravi problemi:
1. **Mancata applicazione dei profili e dei DPI**: Il mouse rifiutava sistematicamente i comandi, lasciando la sensibilità inalterata anche dopo aver premuto "Apply".
2. **Lettura errata della batteria**: L'interfaccia segnalava valori incoerenti (es. 10% fisso, oppure 100% senza alcuna variazione percepibile) e non riconosceva lo stato di carica/scarica.
3. **Mancata sincronizzazione del pulsante DPI fisico**: Premendo il pulsante hardware sul mouse per ciclare tra i profili di sensibilità, il software Linux non ne era a conoscenza e non aggiornava lo stato visivo della GUI.
4. **Mancata gestione della modalità mista Cavo / Wireless 2.4G**: Il mouse quando collegato via cavo USB per la ricarica cambia Product ID USB e disattiva la radio 2.4G, bloccando la comunicazione con il dongle.

Per risolvere definitivamente queste criticità, è stata condotta un'analisi approfondita di reverse engineering statico e dinamico sul pacchetto ufficiale Windows fornito da Attack Shark.

---

## 2. Metodologia e Strumenti Utilizzati

### 2.1 Estrazione del Pacchetto di Installazione
L'eseguibile ufficiale `ATTACK SHARK X11 SOFT.exe` è un archivio compresso generato tramite **Inno Setup**. È stato estratto utilizzando l'utility `innoextract`:
```bash
innoextract -d /tmp/attack_shark_official "ATTACK SHARK X11 SOFT.exe"
```
L'estrazione ha rivelato l'alberatura completa dell'applicazione:
* `AttackShark.exe`: Binario principale Win32 (sviluppato in C++ con framework grafico DirectUI / DuiLib).
* `hiddriver_*.dll`: Suite di driver di interfaccia HID a basso livello (`hiddriver_1.dll` ... `hiddriver_18.dll`).
* `res/MS/`: Definizioni grafiche e vincoli funzionali XML suddivisi per modello (`MS_1` .. `MS_18`).
* `res/MS/MS_3/`: Directory associata nello specifico all'Attack Shark X11.

### 2.2 Strumenti di Analisi dei Binari
* `objdump -M intel -D`: Disassemblaggio x86 a 32 bit delle sezioni di codice (`.text`) e delle tabelle di importazione/esportazione (`.rdata`, `.edata`, `.idata`).
* `readelf` / `hexdump`: Analisi dell'header PE, calcolo degli RVA (Relative Virtual Address) e mappatura dei file offset.
* Script Python di introspezione binaria per la localizzazione di costanti hardware, firme di pacchetti HID e chiamate API Windows (`HidD_SetFeature`, `HidD_GetFeature`, `ReadFile`, `WriteFile`, `SetTimer`, `CProgressUI::SetValue`).
* Tracciamento live dei report USB HID tramite `ioctl(HIDIOCSFEATURE)` e lettura non bloccante su nodi `/dev/hidraw*`.

---

## 3. Identificazione del Dispositivo e Architettura USB HID

### 3.1 Identificativi USB (VID / PID)
Dall'analisi di `AttackShark.exe` e dal riscontro sul bus USB tramite `sysfs`:
* **Vendor ID**: `0x1D57` (Beken Corporation).
* **Product ID Modalità Wireless (Dongle 2.4G)**: `0xFA60` (`Beken 2.4G Wireless Device`).
* **Product ID Modalità Cablata (Cavo USB / Ricarica)**: `0xFA61` (`Beken USB Gaming Mouse`).

> [!NOTE]
> Il vecchio codice cercava erroneamente `0xfa55` per la modalità cablata. In realtà l'Attack Shark X11 quando viene inserito il cavo USB espone l'ID **`0xfa61`**.

### 3.2 Mappatura delle Interfacce USB
Il ricevitore USB e il mouse cablato espongono 4 interfacce HID:
* `Interface 0` (`input0`): Cursore mouse standard (report X/Y, click tasti sinistro/destro/centrale/laterali, rotella).
* `Interface 1` (`input1`): Tasti consumer/multimediali e scorciatoie tastiera.
* **`Interface 2` (`input2`, config)**: **Interfaccia proprietaria vendor**. È l'unico endpoint abilitato alla ricezione di Feature Report di configurazione e alla trasmissione di report asincroni di telemetria e notifiche hardware.
* `Interface 3` (`input3`): Interfaccia diagnostica/firmware update.

Il driver deve quindi aprire esclusivamente il nodo `/dev/hidraw*` associato a **`bInterfaceNumber == 2`**.

---

## 4. Reverse Engineering del Protocollo DPI (Report `0x04`)

### 4.1 La Causa del Rifiuto dei Pacchetti (`03 10 50 01 04`)
Nel codice legacy `hook.cpp`, qualsiasi modifica DPI veniva ignorata dal cursore.
Monitorando le risposte hardware, il mouse rispondeva con:
```
03 10 50 01 04
```
Nel protocollo Beken:
* `buf[0] == 0x03`: Report ID 3 (ACK/Response).
* `buf[1] == 0x10`: Vendor Command.
* `buf[2] == 0x50`: Tipo pacchetto = Acknowledgement.
* `buf[3] == 0x01`: **STATUS = ERRORE / RIFIUTATO**.
* `buf[4] == 0x04`: Report ID a cui si riferisce l'ACK (Report DPI `0x04`).

Dall'analisi del disassembly di `AttackShark.exe` (routine costruttrice del pacchetto DPI), sono emersi **tre errori strutturali** nel pacchetto inviato dal codice originale:

### 4.2 Struttura Reale del Pacchetto DPI (`Report 0x04`, 64 Byte)

| Byte Offset | Valore / Funzione | Descrizione |
|---|---|---|
| `0` | `0x04` | Feature Report ID (Configurazione DPI) |
| `1` | `0x09` o `0x10` | Sotto-comando / Vendor prefix |
| `2` | `0x01` | Modalità configurazione sensore |
| `3` | `0x05` | Numero totale di stage DPI configurati (default: 5) |
| `4..5` | `0x00, 0x00` | Riservato / Allineamento |
| `6..7` | Bitmask | Flag per DPI estesi (> 12.000 DPI) |
| **`8..13`** | **Valori codificati DPI** | **Byte di codifica dei 5 stage DPI (`stage 0..4`)** |
| `14..15` | `0x00, 0x00` | Padding |
| `16..21` | Flag High-Stage | Flag specifici per intervalli 10.100–12.000 e 20.100–22.000 DPI |
| `22..23` | `0x00, 0x00` | Padding |
| **`24`** | **Stage Attivo (`1..5`)** | **Indice (1-based) dello stage DPI attualmente selezionato** |
| `25..49` | `0x00` ... | Parametri avanzati sensore (LOD, ripple control, angle snapping) |
| **`50..51`** | **Checksum Big-Endian (16 bit)** | **Somma a 16 bit calcolata sui byte `3..49` inclusi** |
| `52..63` | `0x00` ... | Padding a 64 byte |

#### Errori corretti rispetto al codice precedente:
1. **Posizione errata dei byte DPI**: Il vecchio codice inseriva lo stage attivo al byte `8` e i valori DPI ai byte `9..13`. Il firmware del mouse esige invece i valori DPI a partire dall'**offset `8`** e lo stage attivo all'**offset `24`**.
2. **Formula lineare inesistente**: Il sensore PixArt PAW3311 non usa una formula matematica `(DPI / 50) + 1`. Utilizza una tabella di calibrazione hardware non-lineare (`kDpiStepTable`), dove ad esempio:
   * 400 DPI = `0x09`
   * 800 DPI = `0x12`
   * 1600 DPI = `0x25`
   * 10000 DPI = `0xEB`
3. **Checksum Big-Endian a 16 bit**: Il firmware convalida l'integrità del pacchetto calcolando la somma aritmetica a 16 bit dei byte da `3` a `49`. Se la somma nei byte `50` e `51` non corrisponde esattamente a `(sum >> 8) & 0xFF` e `sum & 0xFF`, il comando viene scartato con status `0x01`.

Applicando la struttura corretta e il checksum dinamico, il mouse risponde immediatamente:
```
03 10 50 00 04
```
dove `0x00` indica **SUCCESSO / COMANDO ACCETTATO**. La sensibilità del sensore cambia istantaneamente.

---

## 5. Reverse Engineering delle Notifiche Hardware Asincrone (Interrupt In)

Aprendo il file descriptor di `/dev/hidraw*` in modalità non bloccante con un loop basato su `QSocketNotifier` o `poll()`, il mouse trasmette autonomamente pacchetti asincroni senza bisogno di polling attivo.

### 5.1 Notifica Pressione Tasto DPI Fisico (`0x1010`)
Quando l'utente preme il pulsante DPI fisico posizionato dietro la rotella o sul fondo del mouse, il dispositivo trasmette sul canale di interrupt un pacchetto con la seguente firma:
```
[0x03, 0x10, 0x10, <stage_1_based>, 0x00]
```
Nel binario `AttackShark.exe`, la routine del dispatcher principale al VMA `0x00496710` gestisce questo evento:
```x86asm
496719: cmp    al, 0x10                ; buf[1] == 0x10 (Vendor Prefix)
496721: and    eax, 0xff00             ; Maschera secondo byte di comando
496727: cmp    eax, 0x1000             ; buf[2] == 0x10 (Evento DPI switch)
49672c: jne    0x496782
49672e: mov    esi, DWORD PTR [ebp+0xc] ; esi = stage ricevuto (1-based: 1..8)
496731: lea    eax, [esi-1]
496734: cmp    eax, 7                  ; Limite massimo 8 stage
496737: ja     0x496859
496743: dec    esi                     ; esi = stage - 1 (conversione in 0-based)
496744: push   0
496746: push   1                       ; SetSelected(true)
49674e: mov    ecx, [edi+esi*4+0x2e4]  ; Puntatore al CControlUI del radio button stage
496755: mov    eax, [ecx]
496757: call   [eax+0x1c0]             ; Invoca OptionUI::Selected(true)
496763: push   [eax+esi*4+0x8c4]       ; Valore numerico DPI corrispondente allo stage
49676a: push   2
49676c: push   0x50b                   ; Messaggio interno WM_APP: Notifica toast DPI
496771: push   [edi+0x168]
496777: call   PostMessageW            ; Aggiorna l'interfaccia utente
```

#### Replicazione nel driver Linux:
In [`AttackSharkDevice::parseIncomingReport`](file:///var/home/simone/GitHub/attack-shark-x11-linux/device/attackshark_device.cpp#L332-L345), quando arrivano i byte `0x03, 0x10, 0x10, stage`:
1. Viene calcolato l'indice 0-based `stageIndex = stage1Based - 1`.
2. Viene emesso il segnale Qt `activeDpiStageChanged(stageIndex)`.
3. In [`atsx11::onHardwareDpiStageChanged`](file:///var/home/simone/GitHub/attack-shark-x11-linux/atsx11.cpp#L195-L215), il radio button dello stage corrispondente viene spuntato via software e viene mostrata una notifica nella status bar con il valore DPI effettivo.

---

### 5.2 Telemetria Batteria e Logica di Carica (`0x1040`)
Il report di stato della batteria trasmesso dal chip Beken ha la struttura:
```
[0x03, 0x10, 0x40, <state>, <level>]
```
Analizzando il disassemblato di `AttackShark.exe` alle funzioni `0x00496800` (gestore del report) e `0x00486210` (aggiornamento interfaccia percentuale):

```x86asm
; Analisi stato (state = buf[3])
49681b: cmp    ebx, 0x2                ; ebx == 0x02 ? (Stato: In Carica)
49681e: jne    0x49685f
496820: call   KillTimer(0x223)        ; Riavvia timer animazione
496832: push   0xa                     ; Forza visualizzazione al 100% durante l'animazione
496836: call   0x486210
49687c: call   SetTimer(0x223, 30ms)   ; Avvia timer a 30ms per far scorrere la barra verde

; Analisi livello normale in scarica (level = buf[4])
49690d: cmp    esi, 0xa                ; esi = livello (0x00..0x0a)
496910: ja     0x496920                ; Se > 10 scarta pacchetto errato
496912: push   esi                     ; Passa il livello alla funzione di rendering
49691b: call   0x00486210

; Routine di conversione livello -> percentuale (0x00486210):
48623f: mov    eax, [ebp+0x8]          ; eax = level (1..10)
486242: mov    ecx, [edi+0x39c]        ; ecx = Puntatore a CProgressUI
486248: lea    esi, [eax+eax*4]        ; esi = level * 5
48624b: add    esi, esi                ; esi = (level * 5) * 2 = level * 10
48624d: push   esi                     ; push percentuale (es. 10 * 10 = 100)
48624e: call   CProgressUI::SetValue   ; Imposta valore progress bar
48627b: push   0x4f3090                ; Stringa di formato L"%d%%"
486281: call   CDuiString::Format      ; Genera il testo "%d%"
```

#### Risoluzione del comportamento "Segna 100% e non scende":
1. **Granularità hardware al 10%**: Il convertitore ADC interno del controller wireless Beken non invia letture continue all'1%. Suddivide l'intervallo di scarica della batteria Li-Po (4.2V $\rightarrow$ 3.5V) in **10 intervalli discreti da 10% ciascuno** (`0x01` = 10%, `0x02` = 20%, ..., `0x0a` = 100%).
2. **Conferma dal vivo**: Interrogando l'hardware reale collegato alla macchina:
   ```
   Dati ricevuti da /dev/hidraw7: 03 10 40 01 0a
   ```
   * `buf[3] == 0x01`: Discharging (il mouse non è collegato al cavo né sulla dock).
   * `buf[4] == 0x0a`: Livello 10 $\rightarrow$ $10 \times 10\% = 100\%$.
   La batteria era effettivamente carica al 100%. Rimane a tale valore finché l'uso prolungato non riduce la tensione della cella al di sotto della soglia dei 90%, momento in cui il pacchetto conterrà `0x09` (90%).

---

## 6. Gestione Dual-Mode e Hot-Swap Cavo/Wireless

Dall'analisi del comportamento USB emerge una caratteristica peculiare dell'Attack Shark X11:
* Quando il mouse viene collegato tramite cavo USB Type-C per essere utilizzato o ricaricato, il firmware del mouse **spegne il modulo ricetrasmettitore radio 2.4G**.
* Il dongle wireless rimane collegato al PC (`1d57:fa60`), ma il mouse cessa di comunicare con esso e attiva il controller cablato con identificativo **`1d57:fa61`**.

### Architettura di Hotplugging nel Driver Linux:
In [`AttackSharkDevice::checkHotplugTransition`](file:///var/home/simone/GitHub/attack-shark-x11-linux/device/attackshark_device.cpp#L291-L310):
1. Un timer periodico (`kHotplugCheckIntervalMs = 2000`) scansiona `/sys/class/hidraw`.
2. Se viene rilevato il dispositivo cablato `1d57:fa61`, la priorità viene assegnata immediatamente al cavo, chiudendo il descrittore del dongle e agganciandosi a quello cablato.
3. Lo stato di carica viene forzato a `isCharging = true` e l'interfaccia Qt aggiorna l'indicatore in `Battery: %1% (Charging via USB ⚡)`.
4. Scollegando il cavo USB, il driver riapre istantaneamente l'interfaccia del ricevitore 2.4G (`1d57:fa60`) in totale trasparenza.

---

## 7. Auto-Apply On-the-Fly

Nel software Windows originale, per alcune sezioni era necessario premere esplicitamente il tasto "Apply" (`ms_3_btn_apply_dpi`), introducendo frizione d'uso. Nel driver Linux rifattorizzato:
* Tutte le spinbox e i selettori numerici sono collegati a un timer a 350ms (`m_autoApplyTimer`). Non appena l'utente smette di digitare o scorrere, la configurazione completa viene compilata e inviata al dispositivo.
* La selezione dei radio button degli stage DPI o il caricamento di un profilo preimpostato in *Settings* attivano l'applicazione **immediata** a 0ms di latenza.
* Viene preservata la piena sincronizzazione bidirezionale: qualsiasi modifica via UI aggiorna il mouse, e qualsiasi pressione del tasto fisico sul mouse aggiorna la UI.

---

---

## 8. Illuminazione RGB e Protocollo LED (Report 0x05)

### 8.1 Diagnosi del Bug "LED bloccato su breathing che cambia colori"
Il comportamento anomalo segnalato dall'utente (LED che rimaneva permanentemente su *breathing* arcobaleno, ignorando la disattivazione o i cambi modalità) è stato ricondotto a tre cause distinte:
1. **Opcode di spegnimento errato**: Nella vecchia tabella `kBaseColorModes`, l'indice 0 (*Disabled*) inviava l'opcode `0x10` (Static) con RGB `0, 0, 0`, invece del valore ufficiale `0x00` (*Off*). L'hardware interpretava `0x10` come luce fissa.
2. **Modalità Static completamente assente**: Nel software originale Windows (`res/lan/EN.xml` e `ms_3_lightpage.xml`), le 5 modalità base per il profilo `MS_3` (Attack Shark X11) sono:
   * Modo 0 (`0x00`): **LED Off / Disattivato**
   * Modo 1 (`0x10`): **Static / Luce Fissa** (colore personalizzato RGB)
   * Modo 2 (`0x20`): **Breathing / Respirazione** (colore personalizzato RGB)
   * Modo 3 (`0x30`): **Neon** (ciclo spettro continuo / arcobaleno)
   * Modo 4 (`0x40`): **Color Breathing** (respirazione multicromatica continua)
   Nel codice precedente, la modalità 1 era mappata su *Breathing*, saltando la modalità *Static*, e l'opzione di default salvata nei file di configurazione era *Neon* (`0x30`), che produce esattamente un ciclo continuo di colori pulsanti.
3. **Troncamento del Checksum a 8 bit**: Il checksum di Report `0x05` nel firmware originale (`0x496e93..0x496f8d` in `AttackShark.exe`) è una somma a 16 bit Big-Endian distribuita sui byte 11 e 12 (`sum >> 8` e `sum & 0xFF`). Il vecchio driver scriveva solo il byte basso a `out[12]` lasciando `out[11]` invariato. Di conseguenza, per qualsiasi configurazione con somma $\ge 256$, il pacchetto veniva rigettato dal mouse.

### 8.2 Struttura Disassemblata della Routine di Costruzione del Report
```asm
; AttackShark.exe @ 0x496eeb - 0x496f8d (Costruzione Report 0x05 LED & Sleep)
496eeb: mov WORD PTR [ebp-0x68], 0x0f05  ; [0] = 0x05, [1] = 0x0F
496ef1: mov BYTE PTR [ebp-0x66], 0x01    ; [2] = 0x01
496ef8: mov BYTE PTR [ebp-0x65], dl      ; [3] = mode << 4 (0x00, 0x10, 0x20, 0x30, 0x40)
496efb: mov BYTE PTR [ebp-0x64], dh      ; [4] = (bucket << 4) | (speed & 0x0F)
496f19: mov BYTE PTR [ebp-0x63], cl      ; [5] = (base & 0xF0) | (brightness & 0x0F)
496f25: mov BYTE PTR [ebp-0x62], ch      ; [6] = Red
496f28: mov BYTE PTR [ebp-0x61], bl      ; [7] = Green
496f3a: mov BYTE PTR [ebp-0x60], dl      ; [8] = Blue
496f37: mov BYTE PTR [ebp-0x5f], al      ; [9] = sleepTimeMin * 2
496f4c: mov BYTE PTR [ebp-0x5e], cl      ; [10] = ((keyResponseTime - 4) / 2) + 2
; Calcolo Checksum a 16 bit sui byte 3..10
496f7c: mov BYTE PTR [ebp-0x5c], cl      ; [12] = LSB checksum
496f7f: shr eax, 0x8
496f84: mov BYTE PTR [ebp-0x5d], al      ; [11] = MSB checksum
496f8a: push 0x0f                        ; Lunghezza totale = 15 byte
496f8c: push eax
496f8d: call 0x4969f0                    ; Invia Feature Report via ioctl
```

### 8.3 Validazione dal Vivo su `/dev/hidraw7`
Interrogando l'Attack Shark X11 con i pacchetti a 15 byte generati dal driver:
* **Modalità Spento (`0x00`)**: Spegnimento immediato del LED e ricezione ACK `03 10 50 00 05`.
* **Modalità Statico (`0x10`)**: Colore fisso solido (testato con Rosso, Verde, Blu, Bianco e confermato dall'utente).
* **Modalità Respirazione (`0x20`)**: Respirazione fluida a colore fisso impostato dall'utente.
* **Modalità Neon (`0x30`) & Color Breathing (`0x40`)**: Transizioni cicliche spettro completo con ACK `00`.
* **Modalità Static DPI (`0x50`) & Breathing DPI (`0x60`)**: L'illuminazione riflette direttamente il colore dello stage DPI attivo (Rosso per S1, Verde per S2, Blu per S3, Giallo per S4, Ciano per S5), sia a luce fissa che in respirazione pulsata.

### 8.4 Mappatura Colori Stage in Report 0x04 (Byte 25..48)
Dall'analisi della routine a `0x00497350` di `AttackShark.exe`:
Nel pacchetto di configurazione del sensore ottico (Report `0x04`, 56 byte), i byte 25..48 contengono 8 terne RGB che definiscono i colori hardware associati a ciascun livello DPI:
* **Byte 25..27**: Stage 1 = Rosso (`0xFF, 0x00, 0x00`)
* **Byte 28..30**: Stage 2 = Verde (`0x00, 0xFF, 0x00`)
* **Byte 31..33**: Stage 3 = Blu (`0x00, 0x00, 0xFF`)
* **Byte 34..36**: Stage 4 = Giallo (`0xFF, 0xFF, 0x00`)
* **Byte 37..39**: Stage 5 = Ciano (`0x00, 0xFF, 0xFF`)
* **Byte 40..48**: Stage 6..8 = Default padding (Viola, Arancione, Bianco)
* **Byte 49**: Lift-Off Distance (LOD = `0x02` per 2mm)

### 8.5 Ripristino Automatico dell'Illuminazione su Cambio DPI Hardware
Quando l'utente preme il pulsante fisico per cambiare livello DPI durante l'uso, il firmware del mouse invia il report di notifica `0x03 0x10 0x10 <stage> 0x00` e attiva autonomamente il LED sul colore fisso del nuovo livello per fornire feedback visivo.
Tuttavia, se l'utente aveva impostato una modalità personalizzata diversa (es. *Breathing*, *Neon* o *Disabled*), il mouse rimarrebbe bloccato su quel colore fisso.
Il driver Linux risolve questo comportamento:
1. All'arrivo della notifica hardware, aggiorna lo stage nella UI.
2. Se la modalità attiva non è *Static DPI*, avvia un timer a 1000ms (`m_dpiLedRestoreTimer`).
3. L'utente visualizza chiaramente il colore del nuovo livello per 1 secondo, dopodiché il driver riapplica Report `0x05`, ripristinando in modo fluido e trasparente la modalità desiderata.

---

## 9. Sintesi dei Risultati Ottenuti

| Funzionalità | Stato Originale (`hook.cpp`) | Stato Post-Reverse Engineering |
|---|---|---|
| **Applicazione DPI** | Fallita (`03 10 50 01 04`, errore hardware) | Funzionante (`03 10 50 00 04`, ACK positivo, PAW3311 configurato) |
| **Pulsante DPI Fisico** | Non rilevato | Sincronizzazione in tempo reale (`0x1010`) con ripristino automatico LED |
| **Lettura Batteria** | Errata (falso 10% / formato ignoto) | Corretta (scala a decimi, rilevamento carica/scarica `0x1040`) |
| **Modalità Cablata USB** | Non riconosciuta (cercava PID `fa55`) | Riconosciuta con priorità (`fa61`) e stato in carica automatico |
| **Controllo LED RGB** | Bloccato su breathing continuo / spegnimento rotto | 7 modalità complete (Off, Statico, Breathing, Neon, Color Breathing, Static DPI, Breathing DPI), color picker RGB e colori stage dedicati |
| **Applicazione Parametri** | Obbligo di click su tasto Apply | Auto-Apply istantaneo e debounced |
| **Architettura Software** | File monolitico procedurale | C++17 / Qt6 modulare con tipi forti, testabile e documentato |

