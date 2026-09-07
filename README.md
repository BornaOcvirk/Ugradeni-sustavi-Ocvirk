# Autić na daljinsko upravljanje – ESP32

Samostalan ESP-IDF projekt za **ESP32-WROOM-32** razvojnu pločicu. Autić se vozi s dva
**28BYJ-48 koračna motora**, svaki na svom **ULN2003** modulu, upravlja se s
mobitela preko Wi-Fi stranice i mjeri udaljenost do prepreke iza sebe **HC-SR04P**
senzorom, koji radi kao parkirni senzor. Sve, uključujući oba motora, napaja se iz
**jednog USB power banka** priključenog na ESP32.

Sve komponente napisane su lokalno — nema vanjskih biblioteka ni paketa za
preuzimanje. Svaka komponenta ima zasebnu mapu, C datoteku, zaglavlje i CMake opis.

## Potrebne komponente

- ESP32 razvojna pločica s **ESP32-WROOM-32** modulom (ova je rađena na uPesy
  WROOM DevKit s CH340C pretvaračem; radi i na svakoj drugoj s istim modulom)
- 2× 28BYJ-48 5 V koračni motor **sa svojim ULN2003 modulom**
- **HC-SR04P** ultrazvučni senzor (verzija za 3–5,5 V; obični HC-SR04 traži
  djelitelj napona, vidi shemu)
- **USB power bank, 5 V i najmanje 2 A** — jedino napajanje cijelog autića
- elektrolitski kondenzator **1000 µF / 10 V** između VIN i GND
- podatkovni USB kabel (ne samo za punjenje)

## Shema i spajanje

Slika i puna tablica: **[docs/shema_spajanja.md](docs/shema_spajanja.md)**.

| Modul | Pin | ESP32 |
|---|---|---:|
| ULN2003 lijevi | IN1 / IN2 / IN3 / IN4 | 32 / 33 / 25 / 26 |
| ULN2003 desni | IN1 / IN2 / IN3 / IN4 | 27 / 14 / 12 / 13 |
| ULN2003 oba | VCC / GND | **VIN** / GND |
| HC-SR04P | TRIG / ECHO / VCC | 17 / 16 (**izravno**) / **3V3** |

Pinovi su odabrani tako da nijedan nije rezerviran za unutarnju flash memoriju
(GPIO 6–11) ni samo-ulazni (GPIO 34–39). **GPIO 12 je strapping pin** — pri resetu
mora biti nizak, inače se pločica ne pokrene. Spoj je siguran jer ULN2003 ulaz
struju može samo vući, nikada je davati; zašto je to tako i što na tu liniju
nikako ne smiješ spojiti, piše u [shemi spajanja](docs/shema_spajanja.md).

## Priprema i učitavanje

Treba ti **ESP-IDF v5.5 ili noviji**; projekt je izgrađen i provjeren na v5.5.5.
Ako ga još nemaš, instaliraj ga po
[službenim uputama](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/get-started/).

Aktiviraj ESP-IDF okruženje, pa iz mape projekta:

```
idf.py set-target esp32
idf.py build
idf.py -p <PORT> flash monitor
```

Okruženje se aktivira skriptom iz svoje ESP-IDF instalacije — putanju zamijeni
svojom:

| Sustav | Naredba |
|---|---|
| Linux / macOS | `. ~/esp/esp-idf/export.sh` |
| Windows, Command Prompt | `%USERPROFILE%\esp\esp-idf\export.bat` |
| Windows, PowerShell | `. $env:USERPROFILE\esp\esp-idf\export.ps1` |

Kako naći `<PORT>`:

| Sustav | Naredba | Primjer |
|---|---|---|
| Linux | `ls /dev/ttyUSB*` | `/dev/ttyUSB0` |
| macOS | `ls /dev/cu.*` | `/dev/cu.usbserial-0001` |
| Windows | `[System.IO.Ports.SerialPort]::getportnames()` | `COM3` |

Ako pločica ima CH340 pretvarač (većina jeftinijih ima), a port se ne pojavi,
treba ti CH340 upravljački program s proizvođačeve stranice.

### Konfiguracija

`sdkconfig` **nije u repozitoriju** — `idf.py` ga sam stvara iz `sdkconfig.defaults`
pri prvom `set-target`. Od tamošnjih postavki najvažnija je:

```
CONFIG_FREERTOS_HZ=1000
```

Bez nje FreeRTOS radi na 100 Hz, najkraći `vTaskDelay(1)` traje 10 ms umjesto 1 ms,
motori ne stignu dobiti korake na vrijeme i samo zuje u mjestu. Ako ikad pokreneš
`idf.py menuconfig`, tu vrijednost ostavi na miru.

## Korištenje

1. Mobitelom se spoji na Wi-Fi mrežu **ESP32-AUTIC**, lozinka **autic1234**.
2. Otvori `http://192.168.4.1`.
3. Tipke NAPRIJED / NATRAG / LIJEVO / DESNO / STOP upravljaju autićem; na
   računalu rade i strelice, a razmaknica je STOP.
4. Tablica ispod tipki osvježava se 2,5 puta u sekundi: stanje pogona, udaljenost
   straga i prijeđeni put. Kad je nešto bliže od 20 cm iza autića, iznad tablice se
   pali crveno upozorenje i tipka NATRAG prestaje djelovati.
5. Tipka NULIRAJ PUT vraća odometriju na nulu.

Mreža i lozinka su `WIFI_SSID` i `WIFI_PASSWORD` u `main/main.c`.

Senzor je montiran **na stražnjoj strani autića** i radi kao parkirni senzor.
Udaljenost se mjeri otprilike tri puta u sekundi, medijanom od tri uzorka. Kad je
prepreka bliže od 20 cm, vožnja **natrag** se blokira; naprijed i skretanje ostaju
dopušteni da se autić može izvući iz uskog mjesta. Prepreka se smatra riješenom tek
iznad 25 cm — ta histereza sprječava da se autić trza oko samog praga. Pragovi su
`OBSTACLE_CM` i `OBSTACLE_CLEAR_CM` u `main/main.c`.

Ako senzor montiraš sprijeda, u `main/main.c` zamijeni `CAR_DRIVE_BACKWARD` s
`CAR_DRIVE_FORWARD` u sigurnosnom pravilu.

## Organizacija projekta

- `main/main.c` — raspored pinova, sigurnosno zaustavljanje i spajanje komponenti
- `components/stepper_28byj48` — jedan 28BYJ-48 motor preko ULN2003, polukoračno
- `components/car_drive` — diferencijalni pogon s dva motora, vlastiti zadatak za korake
- `components/hcsr04` — mjerenje udaljenosti s medijan filtrom
- `components/web_control` — Wi-Fi pristupna točka i web sučelje
- `docs/` — shema spajanja (MD, SVG) i skripta koja je crta
