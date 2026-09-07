# Touchscreen Toggle

Piccola applicazione nativa per Windows 10 e 11 che mostra lo stato corrente del
touchscreen del notebook e consente di attivarlo o disattivarlo con un pulsante.

![Icona dell'app](assets/touchscreen-toggle-preview.png)

## Cosa fa

- Legge lo stato del touchscreen all'avvio e lo mostra nella finestra.
- Offre un pulsante per attivare o disattivare il dispositivo e un pulsante per aggiornare lo stato.
- Modifica lo stato del dispositivo tramite le API Windows SetupAPI, come l'operazione di abilitazione/disabilitazione in Gestione dispositivi.
- Mantiene la finestra sempre in primo piano; è comunque possibile minimizzarla.
- Include un'icona per la finestra, Esplora file e la barra applicazioni.

La chiusura dell'app non ripristina automaticamente lo stato precedente del touchscreen.
Per riattivarlo dopo averlo disattivato occorre usare mouse, touchpad o tastiera.

## Notebook di riferimento e compatibilità

Il programma è stato sviluppato per il notebook **HUAWEI HKD-WXX**, versione
hardware **M1010** (identificativi letti direttamente dalle informazioni di sistema
del notebook utilizzato per lo sviluppo).

Il touchscreen di riferimento è **Touch screen compatibile HID**, controller
`HID\GXTP738X&Col01`. Il touchpad è un dispositivo distinto, basato su `GXTP7863`.

Requisiti: **Windows 10/11 x64** e autorizzazione amministratore alla richiesta UAC.
L'eseguibile distribuito è x64 e non richiede Qt, Python o l'installazione della
toolchain di compilazione.

Il riconoscimento include anche touchscreen HID standard, quindi può individuare
dispositivi su altri notebook, ma **la compatibilità con altri modelli non è verificata**.
Il codice non è vincolato esclusivamente al modello HUAWEI: usa anche firma HID e
nome del dispositivo. Se trova più touchscreen, seleziona il primo corrispondente;
non offre una scelta fra più schermi.

Sono state verificate compilazione Release, risorse dell'icona e manifest Windows;
durante queste verifiche non è stato eseguito un test automatico di commutazione
del dispositivo. Queste verifiche non costituiscono una certificazione di compatibilità
per tutti i notebook dello stesso modello.

## Sicurezza del riconoscimento

L'app riconosce un vero touchscreen tramite la firma HID standard (usage page
Digitizer `0x0D`, usage Touch Screen `0x04`) e include un controllo specifico per
il controller di questo notebook (`HID\\GXTP738X&Col01`). Non interviene sul
touchpad `GXTP7863`, sulle penne o sugli altri dispositivi HID.

## Avvio

Scaricare [TouchscreenToggle.exe](dist/TouchscreenToggle.exe) (aprire il file su GitHub
e scegliere **Download raw file**) ed eseguirlo. Windows mostra la richiesta Controllo account
utente perché abilitare o disabilitare un dispositivo richiede privilegi di
amministratore. Lo stato viene letto immediatamente all'avvio.

Per la barra applicazioni, aggiungere l'eseguibile dopo averlo salvato in una cartella
stabile. Se è già fissata una vecchia versione senza icona, rimuovere il collegamento
e aggiungerlo di nuovo.

## Compilazione

Aprire un prompt per sviluppatori x64 di Visual Studio, quindi:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

L'eseguibile usa il runtime C++ statico e le sole librerie incluse in Windows.
