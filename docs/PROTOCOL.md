# Specifiche del Protocollo HID: Attack Shark X11

Documento di riferimento per i pacchetti USB HID dell'**Attack Shark X11** (Chipset Beken BK2535 / PixArt PAW3311).

---

## 1. Identificativi del Dispositivo

* **Vendor ID (VID)**: `0x1D57`
* **Product ID Wireless (PID)**: `0xFA60`
* **Product ID Wired/Charging (PID)**: `0xFA61`
* **Target Interface**: `bInterfaceNumber == 2` (Vendor Configuration Interface)
* **Canale di Trasporto**:
  * Scrittura: USB Feature Report via `ioctl(fd, HIDIOCSFEATURE(len), data)`
  * Lettura notifiche: Interrupt Input Report via `read(fd, buf, sizeof(buf))`

---

## 2. Feature Reports (Host $\rightarrow$ Mouse)

### 2.1 Report `0x04`: Configurazione Sensore e Profili DPI (64 Byte)

Inviato per configurare gli stage DPI, lo stage attivo e i registri del sensore ottico PAW3311.

```
Byte 00: 0x04              (Report ID)
Byte 01: 0x09 o 0x10       (Vendor command)
Byte 02: 0x01              (Modalità sensore)
Byte 03: 0x05              (Numero totale di stage, default = 5)
Byte 04..05: 0x00, 0x00    (Riservato)
Byte 06..07: Bitmask       (Maschera per DPI > 12.000: es. 0x01 per stage 0 se >12000)
Byte 08..13: Valori DPI    (5 byte di step calcolati tramite tabella kDpiStepTable)
Byte 14..15: 0x00, 0x00    (Padding)
Byte 16..21: Flag Range    (Flag per range 10.100–12.000 e 20.100–22.000 DPI)
Byte 22..23: 0x00, 0x00    (Padding)
Byte 24:     Active Stage  (Indice 1-based dello stage attivo: 0x01..0x05)
Byte 25..48: Stage Colors  (8 terne RGB per i colori associati agli stage DPI 1..8)
             - Stage 1: 0xFF, 0x00, 0x00 (Rosso)
             - Stage 2: 0x00, 0xFF, 0x00 (Verde)
             - Stage 3: 0x00, 0x00, 0xFF (Blu)
             - Stage 4: 0xFF, 0xFF, 0x00 (Giallo)
             - Stage 5: 0x00, 0xFF, 0xFF (Ciano)
             - Stage 6..8: 0xFF/0x00 padding
Byte 49:     LOD           (Lift-Off Distance: 0x02 = 2mm, 0x01 = 1mm)
Byte 50..51: Checksum 16   (Big-Endian 16-bit sum dei byte 3..49 inclusi)
Byte 52..63: 0x00 ...      (Zero padding fino a 64 byte)
```

#### Calcolo Checksum a 16-bit Big-Endian:
```c
uint16_t sum = 0;
for (int i = 3; i <= 49; ++i) {
    sum += packet[i];
}
packet[50] = (sum >> 8) & 0xFF; // MSB
packet[51] = sum & 0xFF;        // LSB
```

#### Tabella Codifica DPI del Sensore PAW3311 (`kDpiStepTable`):
I valori non sono lineari. Esempi di valori comuni:
* `400 DPI` $\rightarrow$ `0x09`
* `800 DPI` $\rightarrow$ `0x12`
* `1200 DPI` $\rightarrow$ `0x1B`
* `1600 DPI` $\rightarrow$ `0x25`
* `2400 DPI` $\rightarrow$ `0x38`
* `3200 DPI` $\rightarrow$ `0x4B`
* `6400 DPI` $\rightarrow$ `0x97`
* `10000 DPI` $\rightarrow$ `0xEB`

---

### 2.2 Report `0x05`: Illuminazione RGB, Timer di Sleep e Debounce (15 Byte)

Inviato per impostare la modalità dei LED RGB, il colore statico/breathing, i parametri di risparmio energetico e il tempo di risposta dei tasti.

```
Byte 00: 0x05              (Report ID)
Byte 01: 0x0F              (Comando Vendor LED)
Byte 02: 0x01              (ID zona)
Byte 03: Modalità LED      (0x00=Spento, 0x10=Statico, 0x20=Breathing, 0x30=Neon, 0x40=Color Breathing, 0x50=Static DPI, 0x60=Breathing DPI)
Byte 04: Velocità e Bucket ((bucket << 4) | (speed & 0x0F), speed = 1..8)
Byte 05: Luminosità/Sleep  (((0x08 + deepSleep * 0x10) & 0xF0) | (brightness & 0x0F), brightness = 1..8)
Byte 06: Colore R          (0x00..0xFF)
Byte 07: Colore G          (0x00..0xFF)
Byte 08: Colore B          (0x00..0xFF)
Byte 09: Timer Sleep       (sleepTimeMin * 2, da 1 a 30 minuti)
Byte 10: Tempo Debounce    (((keyResponseTimeMs - 4) / 2) + 2, da 4 a 50 ms)
Byte 11: Checksum High     ((sum >> 8) & 0xFF, somma a 16 bit Big-Endian dei byte 3..10)
Byte 12: Checksum Low      (sum & 0xFF)
Byte 13..14: 0x00, 0x00    (Padding)
```

#### Modalità LED Supportate:
1. `0x00` - **Disabled / Off**: LED completamente spento.
2. `0x10` - **Static**: Colore fisso solido personalizzato RGB (Byte 6..8).
3. `0x20` - **Breathing**: Effetto respirazione a colore singolo personalizzato RGB (Byte 6..8).
4. `0x30` - **Neon**: Ciclo continuo dello spettro cromatico / arcobaleno automatico.
5. `0x40` - **Color Breathing**: Respirazione multicromatica ciclica con transizione graduale di colori.
6. `0x50` - **Static DPI**: Luce fissa con il colore associato al livello DPI attivo (S1=Rosso, S2=Verde, S3=Blu, S4=Giallo, S5=Ciano).
7. `0x60` - **Breathing DPI**: Effetto respirazione con il colore associato al livello DPI attivo.

#### Calcolo Checksum a 16-bit Big-Endian:
```c
uint16_t sum = 0;
for (int i = 3; i <= 10; ++i) {
    sum += packet[i];
}
packet[11] = (sum >> 8) & 0xFF; // MSB
packet[12] = sum & 0xFF;        // LSB
```

---

### 2.3 Report `0x06`: Polling Rate (64 Byte)

Inviato per impostare la frequenza di campionamento USB.

```
Byte 00..02: 0x06, 0x09, 0x01
Byte 03..04: Payload frequenza
  - 125 Hz:  0x08, 0xF7
  - 250 Hz:  0x04, 0xFB
  - 500 Hz:  0x02, 0xFD
  - 1000 Hz: 0x01, 0xFE
Byte 05..63: 0x00 ... (Zero padding)
```

---

## 3. Input Reports Asincroni (Mouse $\rightarrow$ Host)

Ricevuti eseguendo una `read()` asincrona sul descrittore di `/dev/hidrawX`.

### 3.1 Risposta ACK ai Comandi Host
```
[0x03, 0x10, 0x50, <status>, <report_id>]
```
* `<status>`:
  * `0x00`: Successo (comando accettato e configurazione applicata).
  * `0x01`: Errore / Checksum non valido / Valore fuori scala.
* `<report_id>`: ID del report a cui si riferisce la risposta (es. `0x04` per DPI).

---

### 3.2 Notifica Pressione Tasto DPI Fisico
Inviato dal mouse ogni volta che l'utente clicca il pulsante DPI hardware:
```
[0x03, 0x10, 0x10, <stage_1_based>, 0x00]
```
* `<stage_1_based>`: Indice dello stage selezionato (da `1` a `5`).
* Formula conversione indice software: `stageIndex = stage_1_based - 1` (da `0` a `4`).

---

### 3.3 Telemetria Batteria e Stato di Carica
Inviato periodicamente dal mouse o su variazione dello stato elettrico:
```
[0x03, 0x10, 0x40, <state>, <level>]
```
* `<state>`:
  * `0x01`: **Discharging** (in uso normale a batteria).
  * `0x02`: **Charging** (collegato al cavo di ricarica o su dock).
  * `0x03`: **Fully Charged** (batteria al 100% con carica completata).
* `<level>`:
  * Scala da `0x00` a `0x0a` (`0`..`10`), rappresentante intervalli discreti del 10%.
  * Formula percentuale: `percentage = level * 10` (es. `0x0a` $\rightarrow$ `100%`, `0x08` $\rightarrow$ `80%`).

