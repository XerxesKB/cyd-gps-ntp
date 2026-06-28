# CLAUDE.md – CYD GPS NTP Server

## Projektübersicht

Lokaler NTP-Zeitserver auf Basis eines **Cheap Yellow Display (ESP32-2432S028)** mit GPS-Zeitquelle über ein **AIR530Z Modul**. Das PPS-Signal des Moduls wird zur Präzisionsverbesserung genutzt. Zeit und Statusdaten werden auf dem integrierten Touchdisplay sowie über eine Webseite angezeigt.

## Aktuelle Debug-Notiz

- 2026-06-28: Windows-`w32tm` zeigte `time.local` stabil gegen den lokal synchronisierten PC, aber PC/`time.local` lagen ca. 0,98 s hinter PTB. Ursache im Code: PPS-Flanke markiert den Beginn der nächsten Sekunde, während der zuletzt geparste NMEA-Zeitstempel noch zur vorherigen Sekunde gehört. Fix: PPS-Sync in `main.cpp` setzt `gpsSec + 1`; NTP-Fraktion in `ntp_server.cpp` nutzt jetzt exakt `2^32`.

---

## Hardware

| Komponente | Detail |
|---|---|
| Mikrocontroller | ESP32-2432S028 ("Cheap Yellow Display", CYD) |
| Display | 2,8" TFT, 320×240px, ILI9341 (dieses Board) |
| Touch | XPT2046 resistiv |
| GPS-Modul | AIR530Z (UART + PPS), läuft auf 9600 Baud |
| GPS TX → ESP32 RX | GPIO 35 (input-only, empfängt NMEA) |
| GPS RX ← ESP32 TX | GPIO 22 (sendet optionale Konfigbefehle) |
| PPS Signal | GPIO 27 (Interrupt-fähig, RISING) |
| RGB-LED | GPIO 4 (R), 16 (G), 17 (B) – common anode, aktiv LOW |

### CYD Display-Konfiguration

Dieses Projekt verwendet ein Cheap Yellow Display mit 2,8" TFT und sichtbarer Auflösung **320×240 Pixel (Landscape)**.

**Wichtig: Der korrekte TFT_eSPI-Treiber für dieses Board ist `ILI9341_2_DRIVER`, nicht `ILI9341_DRIVER`.**

- `ILI9341_2_DRIVER` hat eine andere Initialisierungssequenz und andere MADCTL-Werte als `ILI9341_DRIVER`
- Nur mit `ILI9341_2_DRIVER` liefert `TFT_ROTATION=1` korrekt Landscape 320×240
- Zusätzlich ist `TFT_INVERSION_ON=1` für korrekte Farben notwendig
- Nach `tft.setRotation(1)`: `tft.width()==320`, `tft.height()==240` ✓

**Rotationstabelle für dieses Board (mit ILI9341_2_DRIVER):**

| Rotation | Ergebnis |
|---|---|
| 0 | Portrait (240×320) |
| 1 | **Landscape korrekt (320×240)** ← verwendet |
| 2 | Portrait 180° (240×320) |
| 3 | Landscape gespiegelt (320×240) |

**SPI-Konfiguration:**
- Display: HSPI (Pins 12/13/14), `SPI_FREQUENCY=27MHz` — 40MHz führt zu SPI-Integritätsproblemen auf diesem Board
- Touch: VSPI (separate SPIClass-Instanz, Pins 25/32/39), Library XPT2046_Touchscreen

**Was bei falscher Treiberwahl passiert:**
- `ILI9341_DRIVER` (falsch): `TFT_ROTATION=1` zeigt Portrait statt Landscape; Farben können invertiert sein
- `ST7789_DRIVER` (falsch): massive Farbfehler auf diesem Board

### CYD-Varianten – Display-Controller

Der CYD wird in verschiedenen Hardware-Revisionen verkauft:

| Variante | Display-Controller | TFT_eSPI-Treiber |
|---|---|---|
| ESP32-2432S028R (dieses Board) | **ILI9341** | `ILI9341_2_DRIVER` + `TFT_INVERSION_ON` |
| ESP32-2432S028R (ältere Revision) | **ILI9341** | `ILI9341_DRIVER` (kein Inversion) |
| ESP32-2432S028R (v2, neuere Platine) | **ST7789** | `ST7789_DRIVER` |
| ESP32-2432S028C | **ILI9341** | kapazitiver Touch (GT911), andere Lib nötig |

Erkennungsmerkmal: Bei unbekannter Platine zuerst mit `ILI9341_2_DRIVER` + `TFT_INVERSION_ON` testen.

### Touch-Konfiguration (XPT2046_Touchscreen)

Der Touch läuft über die **PaulStoffregen XPT2046_Touchscreen Library** auf einem **eigenen VSPI-Bus** (unabhängig vom Display-HSPI):

| Signal | GPIO |
|---|---|
| Touch CLK | 25 |
| Touch MOSI | 32 |
| Touch MISO | 39 |
| Touch CS | 33 |
| Touch IRQ | 36 |

```cpp
SPIClass touchSPI = SPIClass(VSPI);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
touch.begin(touchSPI);
touch.setRotation(2);   // Koordinaten passend zu TFT_ROTATION=1
```

`touch.setRotation(2)` sorgt dafür, dass Touch-Koordinaten mit der Display-Ausrichtung (TFT_ROTATION=1) übereinstimmen.

### Pinbelegung CYD (belegte / gesperrte Pins)

Nur diese drei GPIOs sind am CYD für externe Nutzung frei:
- **GPIO 35** – input-only → GPS UART RX
- **GPIO 22** – bidirektional → GPS UART TX (Konfig, optional)
- **GPIO 27** – bidirektional → PPS Interrupt

Alle anderen GPIOs sind intern durch Display, Touch, SD-Karte und RGB-LED belegt.

---

## Entwicklungsumgebung

- **Framework:** PlatformIO mit Arduino Core (ESP32)
- **Board:** `esp32dev`
- **Sprache:** C++ (Arduino-Style)
- **Upload:** standardmäßig OTA (`cyd_ota`), USB nur als Notfall-Fallback (`cyd_usb`)

### platformio.ini

```ini
[platformio]
default_envs = cyd_ota

[env:cyd_ota]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_protocol = espota
upload_port = time.local
upload_flags = --auth=cyd-ntp
lib_deps =
    bodmer/TFT_eSPI@^2.5.43
    mikalhart/TinyGPSPlus@^1.0.3
    tzapu/WiFiManager@^2.0.17
    https://github.com/PaulStoffregen/XPT2046_Touchscreen.git
build_flags =
    -DUSER_SETUP_LOADED=1
    -DILI9341_2_DRIVER=1
    -DUSE_HSPI_PORT=1
    -DTFT_INVERSION_ON=1
    -DTFT_WIDTH=240
    -DTFT_HEIGHT=320
    -DTFT_MISO=12 -DTFT_MOSI=13 -DTFT_SCLK=14
    -DTFT_CS=15 -DTFT_DC=2 -DTFT_RST=-1 -DTFT_BL=21
    -DTOUCH_CS=33
    -DSPI_FREQUENCY=27000000
    -DLOAD_GLCD=1 -DLOAD_FONT2=1 -DLOAD_FONT4=1
    -DLOAD_FONT6=1 -DLOAD_FONT7=1 -DLOAD_FONT8=1
    -DLOAD_GFXFF=1 -DSMOOTH_FONT=1

[env:cyd_usb]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
upload_speed = 921600
lib_deps = ${env:cyd_ota.lib_deps}
build_flags = ${env:cyd_ota.build_flags}
```

---

## Projektstruktur

```
timeserver/
├── platformio.ini
├── CLAUDE.md
└── src/
    ├── main.cpp                    # Setup & Loop, Zeitsynchronisation (PPS+GPS), Status-Log
    ├── config/
    │   └── config.h                # Zentrale Konstanten & Pin-Definitionen
    ├── gps/
    │   ├── gps_handler.h/.cpp      # UART, TinyGPS++ Parsing, Baud-Scan ($-Erkennung)
    │   └── pps_handler.h/.cpp      # PPS-Interrupt (IRAM_ATTR), Zeitstempel
    ├── ntp/
    │   ├── ntp_server.h/.cpp       # UDP NTP Server (Port 123), eigene Implementierung
    │   └── ntp_client.h/.cpp       # NTP-Fallback via SNTP (PTB) bis GPS+PPS lock
    ├── display/
    │   ├── display_manager.h/.cpp  # Seitenwechsel, Touch (XPT2046), Auto-Return
    │   ├── display_main.h/.cpp     # Hauptseite (Zeit, UTC, Datum, Status, HDOP, IP)
    │   └── display_detail.h/.cpp   # Detailseite (Koord, Auf/Unter, Sats, HDOP, PPS, NMEA…)
    ├── wifi/
    │   ├── wifi_manager.h/.cpp     # WiFiManager AP + STA Modus, Standalone-Fallback
    │   └── web_server.h/.cpp       # HTTP: / (HTML-Shell) + /data (JSON-API)
    ├── ota/
    │   └── ota_handler.h/.cpp      # ArduinoOTA, Fortschrittsbalken auf Display
    ├── led/
    │   └── led.h/.cpp              # RGB-LED Modul (common anode, aktiv LOW)
    └── util/
        └── sun.h/.cpp              # Sonnenaufgang/Untergang (USNO-Algorithmus)
```

---

## Zentrale Konfiguration (`config.h`)

```cpp
#define FW_VERSION       "1.0.4-rc"
#define DISPLAY_DRIVER   "ILI9341"

// GPS UART
#define GPS_RX_PIN          35
#define GPS_TX_PIN          22
#define GPS_UART_NUM        2
#define GPS_BAUD_SCAN_LIST  {4800, 9600, 38400, 57600, 115200}
#define GPS_BAUD_FINAL      115200

// PPS
#define PPS_PIN        27
#define PPS_INTERRUPT  RISING   // AIR530Z: HIGH-Flanke = Sekundenbeginn

// NTP
#define NTP_PORT   123
#define STRATUM    1

// Display
#define DISPLAY_TIMEOUT_DETAIL  10000   // ms bis Auto-Rücksprung zur Hauptseite
#define TFT_BL_PIN              21
#define TFT_ROTATION            1       // Landscape 320×240 (mit ILI9341_2_DRIVER)
#define TOUCH_THRESHOLD         1200    // WiFi-RF erhöht Rauschen → 1200 nötig

// RGB-LED (common anode, HIGH = aus, LOW = ein)
#define LED_RED_PIN    4
#define LED_GREEN_PIN  16
#define LED_BLUE_PIN   17

// WiFi AP Fallback
#define AP_SSID      "CYD-NTP-Setup"
#define AP_PASSWORD  ""
#define WIFI_STANDALONE_TIMEOUT  60     // Sekunden

// OTA & mDNS
#define MDNS_HOSTNAME  "time"           // erreichbar als time.local
#define OTA_PASSWORD   "cyd-ntp"

// Zeitzone (Europe/Berlin, automatische Sommer-/Winterzeit)
#define TZ_POSIX  "CET-1CEST,M3.5.0,M10.5.0/3"
```

---

## Funktionsbeschreibung

### Zeitsynchronisation (main.cpp)

Strategie:
- PPS-Puls = exakter Sekundenanfang (ISR schreibt `ppsMicros` via `esp_timer_get_time()`)
- NMEA-Datum/Zeit kommt ~200ms nach PPS → gibt die UTC-Sekunde
- `syncSystemClock()` kombiniert beide: `settimeofday()` mit GPS-Sekunde korrigiert um Alter des PPS-Pulses
- Danach liefert `gettimeofday()` präzise Zeit auf µs-Ebene
- Beim ersten GPS+PPS-Lock wird der NTP-Fallback (SNTP) gestoppt

### GPS & PPS

- NMEA über `HardwareSerial Serial2` (GPIO 35 RX, GPIO 22 TX)
- Baudrate-Scan beim ersten Start (4800→9600→38400→57600→115200), gespeichert via `Preferences`
- Baud-Erkennung anhand `$`-Zeichen (NMEA-Satzanfang) — robuster als roher Byte-Zähler, da UART-Noise bei falscher Baud keine `$` produziert
- PPS-ISR (`IRAM_ATTR`): schreibt nur Timestamp + Zähler, keine Logik
- `pps_new_pulse()` konsumiert den Flag sicher aus dem Loop
- Exportierte Diagnosevariablen: `gpsRawBytes`, `gpsBaudCurrent`, `gps.passedChecksum()`, `gps.failedChecksum()`

### NTP-Fallback (ntp_client.cpp)

- Aktiv solange kein GPS+PPS-Lock besteht
- `configTzTime()` + PTB-Server (`ptbtime1.ptb.de`, `ptbtime2.ptb.de`, `de.pool.ntp.org`)
- Sync-Intervall 60s (`sntp_set_sync_interval`) — verhindert Drift ohne RTC
- Nach erstem GPS+PPS-Lock: `ntp_client_stop()` deaktiviert SNTP dauerhaft

### Display – Hauptseite (display_main.cpp)

Layout (320×240 Landscape):

| Bereich | y | Inhalt |
|---|---|---|
| Lokalzeit | 5 | Font7, 8 Zeichen × 40px-Slot, Orange `0xFB80` |
| Datum | 62 | Font4, zentriert, Wochentag + Datum (z.B. „So 28.06.2026"), Weiß |
| Trennlinie | 93 | dunkelgrün |
| UTC-Zeit | 97 | Font7, 8 Zeichen × 30px-Slot, 240px zentriert, Weiß |
| UTC-Label | 97 | Font2, rechts der UTC-Uhr, dim |
| Sonnen-Icons | 157 | Halbkreis + Horizont + Pfeil (r=9), je Hälftenmitte (x=80/240), warmes Gelb `0xFEE0` |
| Sonnenzeiten | 184 | Font4, „HH:MM", je Hälftenmitte zentriert |
| Status + HDOP | 222 | Font2: Status linksbündig (TL_DATUM), HDOP rechtsbündig (TR_DATUM) in Ampelfarbe |

- Sonnen-Icons und -Zeiten vertikal zentriert im Band zwischen UTC-Uhr (y≈145) und Statuszeile (y=222)
- Inkrementelles Rendering: nur geänderte Slots/Felder neu zeichnen
- `setTextPadding(w) + setTextColor(fg, bg)`: ein SPI-Pass, kein Flackern

### Display – Detailseite (display_detail.cpp)

| Zeile | y | Inhalt |
|---|---|---|
| Header | 0–22 | „GPS Detail" + „Touch → zurück" |
| Koord. | 28 | `lat / lon` kombiniert (oder „kein Fix") |
| Auf/Unter | 48 | Sonnenaufgang / Sonnenuntergang Ortszeit |
| Satelliten | 68 | Anzahl Satelliten |
| HDOP | 88 | HDOP-Wert in Ampelfarbe (grün/orange/rot) |
| PPS Pulse | 108 | SI-Präfix-Zähler, grün wenn aktiv |
| GPS Bytes | 128 | SI-Präfix + Baudrate |
| NMEA | 148 | ok/err SI-Präfix, Farbe je Qualität |
| IP | 168 | IP-Adresse |
| WLAN | 188 | SSID (gelb im AP-Modus) |
| Uptime | 208 | `Xd HHhMMmSSs  vX.Y.Z` |
| Footer | 238 | „Touch→Haupt | Auto 10s" |

### Display-Besonderheiten (CYD-Rendering)

- **Touch**: `XPT2046_Touchscreen`-Library auf VSPI — nicht `tft.getTouch()` verwenden
- **Backlight** (GPIO 21): LOW vor `tft.init()`, HIGH erst nach `fillScreen(BLACK)` → keine Initialisierungsartefakte
- **`setTextPadding(width)` + `setTextColor(fg, bg)`**: Text inkl. Hintergrund in einem SPI-Pass → kein Flackern, kein `fillRect` nötig
- **HDOP-Ampel**: grün < 2.0 / orange < 5.0 / rot ≥ 5.0 — konsistent auf Hauptseite, Detailseite und Webseite
- **SI-Präfixe**: `fmtSI()` für große Zähler (GPS Bytes, NMEA, PPS Pulse, NTP-Anfragen) — k/M/G
- **Uptime**: immer `Xd HHhMMmSSs` — Tage werden auch bei 0 angezeigt

### NTP-Server

- Eigene UDP-Implementierung (`ntp_server.cpp`), keine externe Library
- Standard-NTP-Paket (48 Byte), VN=4, Mode=4 (Server)
- Reference ID: `GPS\0`
- Precision: -20 (~1µs, mit PPS realistisch)
- Transmit Timestamp wird unmittelbar vor dem Senden gesetzt (minimaler Jitter)
- Jede Anfrage wird per Serial geloggt: `[NTP] Anfrage #N von <IP>  Stratum X`

**Dynamischer Stratum** (`ntp_current_stratum()` in main.cpp, deklariert in ntp_server.h):

| Zustand | Stratum |
|---|---|
| GPS + PPS aktiv | 1 |
| GPS-Fix, kein PPS | 2 |
| GPS hatte Fix, Signal weg (Holdover) | 3 |
| Kein GPS (SNTP-Fallback oder standalone) | 16 |

**Leap Indicator** (`ntp_current_li()`): LI=0 wenn GPS frisch (<3s), LI=3 (Alarm) sonst — signalisiert NTP-Clients korrekt wenn die Zeitquelle unsynchronisiert ist.

### Zeitstatus-Logik

| Zustand | Display-Status (Hauptseite) | Farbe | NTP-Stratum | LI |
|---|---|---|---|---|
| Kein GPS, kein NTP | „Warte GPS/NTP..." | Orange | 16 | 3 |
| Kein GPS, NTP ok | „NTP (kein GPS)" | Orange | 16 | 3 |
| GPS OK, kein PPS | „GPS OK (kein PPS)" | Orange | 2 | 0 |
| GPS + PPS | „PPS locked  Stratum 1" | Grün | 1 | 0 |
| GPS verloren (Holdover) | „GPS signal lost" | Rot | 3 | 3 |

### WiFi & AP-Modus / Standalone-Modus

- Start: Verbindungsversuch mit gespeichertem WLAN (Timeout: 20s)
- Schlägt fehl → AP „CYD-NTP-Setup" (offen) + Captive Portal (60s)
- Portal-Timeout → Standalone-Modus: WiFi komplett aus (`WiFi.disconnect(true)` + `WiFi.mode(WIFI_OFF)`)
- Standalone: GPS-Zeit auf Display OK, kein NTP-Server, kein Web-Interface

### OTA-Updates (ota_handler.cpp)

- ArduinoOTA über mDNS (`time.local`), Passwort `cyd-ntp`
- **Während OTA**: `ota_in_progress()` Flag → `loop()` kehrt sofort zurück, GPS/NTP/Display pausiert → volle CPU für OTA (~25s statt 70s+)
- Fortschrittsbalken: Delta-Rendering (nur neuer grüner Anteil), kein Flackern
- `esp_task_wdt_reset()` im `onProgress`-Callback — verhindert WDT-Reset während des ~25s-Uploads
- Upload: `pio run -t upload` (OTA-Umgebung ist Default), USB via `pio run -e cyd_usb -t upload`

### Hardware-Watchdog (main.cpp)

- `esp_task_wdt_init(60, true)` — 60s Timeout, Panic + Reset bei Ablauf
- Wird am Ende von `setup()` gestartet (nach WiFi-Init), nicht davor — WiFi-AP-Timeout kann bis zu 80s dauern
- `esp_task_wdt_reset()` am Anfang jedes `loop()`-Durchlaufs
- Schutz vor Loop-Hänger (z.B. WiFi-Deadlock, I²C-Blockade)

### RGB-LED (led.cpp)

- Common-Anode-Verdrahtung: HIGH = aus, LOW = ein
- `led_init()`: alle Pins auf OUTPUT + HIGH (aus), als erstes in `setup()` aufgerufen
- `led_set(r, g, b)`: true = Farbe ein (Invertierung intern)
- Derzeit dauerhaft aus — für spätere Statusanzeige vorbereitet

### Web-Interface

URL: `http://<IP>/` oder `http://time.local/` (mDNS)

- `GET /` liefert einmalig die HTML-Shell (CSS + leere Platzhalter + JS)
- `GET /data` liefert JSON mit allen Live-Daten (kein Cache)
- JavaScript `fetch('/data')` alle 1s, aktualisiert nur geänderte DOM-Elemente
- Keine Seitenreloads — keine Unterbrechung der Darstellung

JSON-Felder: `localTime`, `utcTime`, `date`, `status`, `statusClass`, `fixAcquired`, `lat`, `lon`, `satellites`, `hdop`, `hdopClass`, `gpsBaud`, `nmeaOk`, `nmeaErr`, `gpsBytes`, `ppsLocked`, `ppsPulseCount`, `ntpRequests`, `stratum`, `rssi`, `uptime`, `sunrise`, `sunset`, `firmware`

**CSS-Hinweis**: Farbklassen müssen `td.v-ok` statt `.v-ok` heißen — `td:last-child` hat Spezifität 0,1,1 und würde `.v-ok` (0,1,0) sonst überschreiben.

### Sonnenaufgang/Untergang (util/sun.cpp)

- USNO-Algorithmus, Genauigkeit ~2 Minuten
- Eingabe: GPS-Koordinaten + aktuelles Datum (aus Systemzeit)
- UTC-Offset wird live aus der Systemzeit abgeleitet → DST-korrekt
- Angezeigt auf **Hauptseite** (Icons + Zeit, vertikal zentriert zwischen UTC und Statuszeile), **Detailseite** (Zeile „Auf/Unter:") und **Webseite** (GPS-Abschnitt)
- Gibt `false` zurück bei Polartag/Polarnacht oder fehlendem GPS-Fix

### Serial-Logging

Schlüsselereignisse werden auf Serial (115200 Baud) ausgegeben:

| Präfix | Inhalt |
|---|---|
| `[CYD-NTP]` | Start, Version, Modulinitialisierung |
| `[GPS]` | Baud-Scan, erster Zeit-Fix, erster Positions-Fix |
| `[SYNC]` | Erster GPS+PPS-Lock, NTP-Fallback gestoppt |
| `[NTP]` | Jede Client-Anfrage mit IP und Uhrzeit |
| `[STATUS]` | Alle 60s: UTC, GPS Sats/HDOP, PPS-Status, Uptime, NTP-Anfragen |
| `[WIFI]` | Verbindung (SSID, IP, RSSI), AP-Modus, Standalone, mDNS |
| `[OTA]` | Start, Fortschritt 10%-Schritte, Fertig/Fehler |

---

## Architekturentscheidungen

- **Kein RTOS**: Loop-basierter Ansatz mit `millis()`-Timern — ausreichend für diesen Use-Case
- **PPS im ISR**: Nur Timestamp + Zähler, kein `delay()`, kein `malloc()` in der ISR
- **NTP eigen implementiert**: Keine externe Library — NTP ist einfach genug (48 Byte, RFC 5905)
- **NTP-Fallback via SNTP**: Systemzeit auch ohne GPS nutzbar; wird bei GPS+PPS-Lock deaktiviert
- **TFT_eSPI direkt**: Kein LVGL-Overhead, kein Frame-Buffer → RAM gespart
- **Separate SPI-Busse**: Display auf HSPI (Pins 12/13/14), Touch auf VSPI (Pins 25/32/39)
- **OTA als Standard-Upload**: USB-Kabel während Betrieb nicht nötig; `ota_in_progress()` pausiert Loop
- **JSON-API statt HTML-Reload**: `/data`-Endpoint entkoppelt Daten von Darstellung; JS diffed DOM

---

## Noch offene Entscheidungen / TODOs

- [x] AIR530Z Baudrate: Scan + `$`-basierte Erkennung (implementiert, Preferences gespeichert)
- [x] PPS Flanke: RISING (AIR530Z Datenblatt)
- [x] NTP: eigene UDP-Implementierung
- [x] mDNS Hostname: `time.local`
- [x] OTA-Updates: implementiert, Passwort in config.h, Progress auf Display
- [x] Zeitzone: TZ_POSIX automatische Sommer-/Winterzeit
- [x] Serial-Logging: durchgängig mit Präfix-System
- [x] NTP-Fallback: SNTP bis GPS+PPS lock
- [x] Webseite: JSON-API + JS-Polling, kein Seitenreload
- [x] HDOP-Ampel: konsistent auf allen drei Ansichten
- [x] Sonnenaufgang/Untergang: Hauptseite (Icons) + Detailseite + Webseite
- [x] Dynamischer NTP-Stratum (1/2/3/16) + Leap Indicator (LI=0/3) je nach GPS/PPS-Zustand
- [x] Hardware-Watchdog: 60s Timeout, WDT-Reset in loop() und OTA-Progress
- [x] Wochentag im Datum (Hauptseite)
- [ ] Touch-Kalibrierung: Rohdaten, keine Kalibrierung — `touch.setRotation(2)` funktioniert praktisch
- [ ] LED-Statusanzeige: GPIO reserviert, Logik noch nicht definiert
- [ ] GPS auf 115200 Baud: PMTK251 wird gesendet, aber Preferences speichert 9600 → für später

---

## Versionierung

Das Projekt verwendet **Semantic Versioning** (`MAJOR.MINOR.PATCH`):

| Teil | Wann erhöhen |
|---|---|
| `MAJOR` | Inkompatible Änderungen (Pin-Neubelegung, Protokollwechsel, kompletter Umbau) |
| `MINOR` | Neue Funktionen, rückwärtskompatibel (neue Display-Seite, neue API-Route) |
| `PATCH` | Bugfixes, kleine Korrekturen, Dokumentationsänderungen |

**Regeln für Claude Code:**
- Die aktuelle Version steht in `config.h` als `#define FW_VERSION "x.y.z"`
- **Bei jeder Änderung am Projekt** die Version in `config.h` anpassen und einen Eintrag in der Änderungshistorie ergänzen
- Format: `JJJJ-MM-TT | vX.Y.Z | Beschreibung`

**Aktuelle Version:** `v1.0.4-rc`

---

## Änderungshistorie

| Datum | Version | Änderung |
|---|---|---|
| 2026-06-27 | v0.1.0 | Initiale Projektdefinition |
| 2026-06-27 | v0.1.1 | Baudrate-Scan, PPS RISING, mDNS, NTP-Entscheidung, kein OTA |
| 2026-06-27 | v0.1.2 | CYD-Varianten dokumentiert (ILI9341 vs ST7789) |
| 2026-06-27 | v0.1.3 | Standalone-Modus, Backlight-Fix, Touch-Schwellwert, TFT_ROTATION in config.h |
| 2026-06-27 | v0.1.4 | prevIp-Buffer-Overflow behoben; WiFi-Oszillation behoben |
| 2026-06-27 | v0.1.5 | ST7789_DRIVER probiert → Farbfehler → verworfen; Board ist ILI9341 |
| 2026-06-27 | v0.1.6 | USE_HSPI_PORT=1; SPI 40→27MHz; Layout-Overlap-Fix (y-Koordinaten) |
| 2026-06-27 | v0.1.7 | Doppelter GRAM-Clear; dynamisches tft.width() statt hardcodiert |
| 2026-06-27 | v0.1.8 | Layout neu: Artefakte (Striche) behoben; alle y-Koordinaten überschneidungsfrei |
| 2026-06-27 | v0.1.9 | TFT_ROTATION=2 stabilisiert; Touch-Debounce 800ms |
| 2026-06-27 | v0.1.11 | **Displaytreiber-Bug behoben**: ILI9341_DRIVER → ILI9341_2_DRIVER + TFT_INVERSION_ON; TFT_ROTATION=1 (Landscape 320×240); Touch auf XPT2046_Touchscreen-Library (VSPI); Farbschema Orange; TZ_POSIX |
| 2026-06-28 | v0.1.12 | Flackern Detailseite behoben: fillRect+drawString → setTextPadding+drawString (ein SPI-Pass) |
| 2026-06-28 | v0.1.13 | WIFI_STANDALONE_TIMEOUT 20s → 60s |
| 2026-06-28 | v0.1.14 | Flackern Hauptseite behoben: setTextPadding+drawString für Status, Sats, IP |
| 2026-06-28 | v0.1.15 | Phantom-Touch bei aktivem WiFi behoben: TOUCH_THRESHOLD 1200 (WiFi-RF erhöht Störpegel) |
| 2026-06-28 | v0.1.16 | Reset-Grund-Diagnose via esp_reset_reason() in Serial-Log |
| 2026-06-28 | v0.1.17 | On-Screen-Diagnose: RST/PSW-Zähler auf Detailseite (temporär) |
| 2026-06-28 | v0.1.18 | Diagnose-Code entfernt (Ursache Brownout durch schlechtes USB-Kabel behoben) |
| 2026-06-28 | v0.2.0 | OTA-Updates: ArduinoOTA (src/ota/), OTA_PASSWORD + MDNS_HOSTNAME in config.h, Fortschrittsbalken |
| 2026-06-28 | v0.2.1 | OTA-Balken-Flackern behoben: Delta-Rendering, setTextPadding für Prozenttext |
| 2026-06-28 | v0.2.2 | Hauptseite redesigned: kein Header, Uhrzeit Font7 (48px 7-Segment) zentriert, Orange 0xFB80 |
| 2026-06-28 | v0.2.3 | Uhrzeit nutzt volle Breite: HH:MM Font8 + :SS Font7, unten bündig, textWidth()-zentriert |
| 2026-06-28 | v0.2.4 | Gesamte Uhrzeit Font7; NTP-Fallback auf PTB bis GPS+PPS-Lock; NTP-Status auf Display |
| 2026-06-28 | v0.2.5 | Uhrzeit: 8 Einzelzeichen in 40px-Slots (kein Overflow); nur geänderte Slots neu zeichnen |
| 2026-06-28 | v0.2.6 | UTC-Zeit Font7 (30px-Slots, 240px zentriert); Unterbereich bottom-aligned; UTC-Label |
| 2026-06-28 | v0.2.7 | NTP-Fallback-Intervall 60s (sntp_set_sync_interval) — verhindert Drift ohne RTC |
| 2026-06-28 | v0.2.8 | IP-Zeile zeigt zusätzlich mDNS-Adresse |
| 2026-06-28 | v0.2.9 | RGB-LED deaktiviert: src/led/ (common anode, GPIO 4/16/17 HIGH) |
| 2026-06-28 | v0.2.10 | GPS-Diagnose: gpsRawBytes-Zähler; Detailseite zeigt GPS Bytes mit Farbindikator |
| 2026-06-28 | v0.2.11 | Baudrate-Scan robuster: gpsBaudCurrent exportiert; Serial-Raw-Echo |
| 2026-06-28 | v0.2.12 | Detailseite: GPS Bytes und NMEA auf separate Zeilen; Uptime+Firmware kombiniert |
| 2026-06-28 | v0.2.13 | NMEA ok/err-Zähler via gps.passedChecksum()/failedChecksum(); Farbkodierung |
| 2026-06-28 | v0.2.14 | Baud-Erkennung auf `$`-Zeichen umgestellt (NMEA-Satzanfang, verhindert Noise-Fehldetektionen) |
| 2026-06-28 | v0.2.15 | Serial-Raw-Echo entfernt (GPS bestätigt: 9600 Bd, PPS locked, HDOP 0.9) |
| 2026-06-28 | v0.2.16 | Durchgängiges Serial-Logging: [SYNC][STATUS][NTP][GPS][WIFI] mit Präfix-System |
| 2026-06-28 | v0.2.17 | OTA-Geschwindigkeit: ota_in_progress() Flag pausiert Loop während OTA (~25s statt 70s+) |
| 2026-06-28 | v0.2.18 | Webseite an Detailseite angepasst: Sektionen GPS/PPS/NTP/Netzwerk/System |
| 2026-06-28 | v0.2.19 | Web-Aktualisierung ohne Seitenreload: /data JSON-API + JS fetch() alle 1s, DOM-Diff |
| 2026-06-28 | v0.2.20 | SI-Präfixe (k/M/G) für Byte-Zähler; Uptime mit Tagen; HDOP-Ampel auf Detail+Web |
| 2026-06-28 | v0.2.21 | HDOP-Ampel auf Hauptseite: Sats/HDOP getrennt gezeichnet, HDOP in Ampelfarbe rechts |
| 2026-06-28 | v0.2.22 | CSS-Fix (td.v-ok statt .v-ok); Uptime immer mit Tagen (0d); Sonnenaufgang/Untergang (USNO, src/util/sun.cpp) auf Detail+Web; Lat/Lon auf Detailseite kombiniert |
| 2026-06-28 | v1.0.0-rc | CLAUDE.md vollständig aktualisiert; Version auf 1.0.0-rc hochgestuft |
| 2026-06-28 | v1.0.1-rc | Sonnenaufgang/Untergang als Icons auf Hauptseite: Halbkreis+Horizont+Pfeil (r=5), Font2-Zeiten |
| 2026-06-28 | v1.0.2-rc | Hauptseite redesigned: Sats+IP entfernt; Status links + HDOP rechts in Ampelfarbe auf einer Zeile; Sonnen-Icons größer (r=9), je Hälftenmitte (x=80/240), Font4 für Zeiten |
| 2026-06-28 | v1.0.3-rc | Wochentag im Datum (So–Sa); dynamischer NTP-Stratum (1/2/3/16) + LI-Bit; Hardware-Watchdog 60s (esp_task_wdt); Stratum live im Web-Interface |
| 2026-06-28 | v1.0.4-rc | Sonnen-Icons/-Zeiten vertikal zentriert zwischen UTC-Uhr und Statuszeile (y=157/184); CLAUDE.md aktualisiert |

---

## Hinweise für Claude Code

- **Immer `config.h` für Pins und Konstanten** — keine Magic Numbers im Code
- **Display-Treiber**: `ILI9341_2_DRIVER` (nicht `ILI9341_DRIVER`!) + `TFT_INVERSION_ON=1`
- **Landscape**: `TFT_ROTATION=1` → `tft.width()=320`, `tft.height()=240`
- **Touch**: `XPT2046_Touchscreen` auf VSPI — nicht `tft.getTouch()` verwenden
- **ISR**: `IRAM_ATTR` für PPS-ISR, nur atomare Operationen (kein malloc, kein delay)
- **GPS**: `HardwareSerial Serial2`, RX=GPIO35 (input-only), TX=GPIO22
- **GPIO 35 ist input-only** — niemals als Output konfigurieren
- **mDNS**: `MDNS.begin(MDNS_HOSTNAME)` → `http://time.local/`
- **OTA**: Standard-Upload via `pio run -t upload`; USB-Fallback via `-e cyd_usb`
- **Versionierung**: Bei jeder Änderung `FW_VERSION` in `config.h` erhöhen + Änderungshistorie in `CLAUDE.md` ergänzen
- **Baudrate-Scan**: Ergebnis in `Preferences` gespeichert — nur beim ersten Start oder nach Reset
- **Kein `delay()` im Loop** — ausschließlich `millis()`-basierte Zeitsteuerung
- **CSS auf Webseite**: Farbklassen als `td.v-ok` definieren, nicht `.v-ok` — `td:last-child` hat sonst Vorrang
- **Sonnenaufgang/Untergang**: nur verfügbar wenn `gpsData.fixAcquired` — liefert Ortszeit inkl. DST; auf Hauptseite als Icons (x=80/240, r=9), auf Detail/Web als Text
- **Uptime-Format**: immer `Xd HHhMMmSSs` — Tage werden auch bei 0 angezeigt (kein bedingtes Format)
- **NTP-Stratum**: dynamisch via `ntp_current_stratum()` in main.cpp — niemals `STRATUM`-Konstante direkt in ntp_server.cpp verwenden
- **Hardware-Watchdog**: `esp_task_wdt_reset()` muss in `loop()` UND in OTA-`onProgress` aufgerufen werden — sonst Reset nach 60s während OTA-Upload
