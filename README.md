# Touchscreen Toggle

Piccola applicazione nativa per Windows 10 e 11 che mostra lo stato corrente del
touchscreen del notebook e consente di attivarlo o disattivarlo con un pulsante.

## Sicurezza del riconoscimento

L'app riconosce un vero touchscreen tramite la firma HID standard (usage page
Digitizer `0x0D`, usage Touch Screen `0x04`) e include un controllo specifico per
il controller di questo notebook (`HID\\GXTP738X&Col01`). Non interviene sul
touchpad `GXTP7863`, sulle penne o sugli altri dispositivi HID.

## Avvio

Eseguire `TouchscreenToggle.exe`. Windows mostra la richiesta Controllo account
utente perché abilitare o disabilitare un dispositivo richiede privilegi di
amministratore. Lo stato viene letto immediatamente all'avvio.

## Compilazione

Aprire un prompt per sviluppatori x64 di Visual Studio, quindi:

```bat
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

L'eseguibile usa il runtime C++ statico e le sole librerie incluse in Windows.
