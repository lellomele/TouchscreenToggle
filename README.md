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

Il programma è stato sviluppato per il notebook **Huawei Matebook 14s 2021**, versione
hardware **M1010** 

Requisiti: **Windows 10/11 x64** e autorizzazione amministratore alla richiesta UAC.

Il riconoscimento include anche touchscreen HID standard, quindi può individuare
dispositivi su altri notebook, ma **la compatibilità con altri modelli non è verificata**.

## Compilazione

Aprire un prompt per sviluppatori x64 di Visual Studio, quindi:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

L'eseguibile usa il runtime C++ statico e le sole librerie incluse in Windows.
