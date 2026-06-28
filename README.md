# CYD GPS NTP Server

Lokaler NTP-Zeitserver auf Basis eines **ESP32 Cheap Yellow Display (CYD)** mit **AIR530Z GPS-Modul** und PPS-Signal. Erreichbar per mDNS unter `time.local`, konfigurierbar über ein Captive-Portal-WLAN-Setup, aktualisierbar per OTA.

---

## Features

- **Stratum-1-NTP-Server** mit GPS+PPS-Zeitquelle (RFC 5905 konform)
- **Dynamischer Stratum** (1 = GPS+PPS / 2 = GPS / 3 = Holdover / 16 = kein GPS) und korrektes **Leap-Indicator-Bit**
- **PPS-Präzision**: Systemzeit per `settimeofday()` auf µs-Ebene synchronisiert
- **NTP-Fallback** via SNTP (PTB-Server) bis zum ersten GPS+PPS-Lock
- **Touch-Display** mit Hauptseite und Detailseite (Auto-Return nach 10 s)
- **Web-Interface** unter `http://time.local/` — Live-Updates ohne Seitenreload via JSON-API
- **Sonnenaufgang / Sonnenuntergang** (USNO-Algorithmus, DST-korrekt) auf Display und Webseite
- **OTA-Updates** via PlatformIO (`pio run -t upload`)
- **Hardware-Watchdog** (60 s) gegen Loop-Hänger
- **Baudrate-Scan** beim Erststart, danach in `Preferences` gespeichert
- **mDNS** unter `time.local`

---

## Hardware

| Komponente | Details |
|---|---|
| Mikrocontroller | ESP32-2432S028 („Cheap Yellow Display", CYD) |
| Display | 2,8" TFT 320×240 px, ILI9341 |
| Touch | XPT2046 resistiv |
| GPS-Modul | AIR530Z (UART + PPS-Ausgang) |

### Verdrahtung

| Signal | GPIO | Richtung |
|---|---|---|
| GPS NMEA TX → ESP32 RX | 35 | Input-only |
| GPS NMEA RX ← ESP32 TX | 22 | Output (optional) |
| GPS PPS | 27 | Input, RISING-Flanke |
| RGB-LED R / G / B | 4 / 16 / 17 | Common Anode, aktiv LOW |
| Display (HSPI) MISO/MOSI/CLK | 12 / 13 / 14 | — |
| Display CS / DC / BL | 15 / 2 / 21 | — |
| Touch (VSPI) CLK/MOSI/MISO/CS | 25 / 32 / 39 / 33 | — |

> **Hinweis:** GPIO 35 ist auf dem CYD **input-only** und darf nicht als Output konfiguriert werden.

---

## Anzeigeseiten

### Hauptseite

```
┌─────────────────────────────────────┐
│   1  4  :  3  5  :  2  2           │  Lokalzeit (Font7, Orange)
│         So 28.06.2026               │  Wochentag + Datum
├─────────────────────────────────────┤
│       14:35:22              UTC     │  UTC-Zeit (Font7, Weiß)
│                                     │
│    ↑            ○────────           │  Sonnenaufgang-  Sonnenuntergang-
│   (○)        ─────────              │  Icon (Halbkreis + Pfeil)
│  ──────           ↓                 │
│   06:23         20:45               │  Zeiten in Font4
│                                     │
│ PPS locked  Stratum 1   HDOP: 0.9  │  Status + HDOP-Ampel
└─────────────────────────────────────┘
```

**HDOP-Ampel:** grün < 2,0 / orange < 5,0 / rot ≥ 5,0 — konsistent auf Haupt- und Detailseite sowie im Web-Interface.

### Detailseite (Touch → wechseln)

Koordinaten, Sonnenauf-/-untergang, Satelliten, HDOP, PPS-Pulse, GPS-Bytes, NMEA-Statistik, IP, SSID, Uptime, Firmware-Version.

---

## Web-Interface

Erreichbar unter `http://time.local/` oder direkt per IP.

```
http://time.local/       HTML-Shell (einmalig geladen)
http://time.local/data   JSON-API (Live-Daten, kein Cache)
```

Das JavaScript aktualisiert alle Daten **ohne Seitenreload** jede Sekunde per `fetch('/data')`.

**JSON-Felder:** `localTime`, `utcTime`, `date`, `status`, `statusClass`, `fixAcquired`, `lat`, `lon`, `satellites`, `hdop`, `hdopClass`, `gpsBaud`, `nmeaOk`, `nmeaErr`, `gpsBytes`, `ppsLocked`, `ppsPulseCount`, `ntpRequests`, `stratum`, `rssi`, `uptime`, `sunrise`, `sunset`, `firmware`

---

## NTP-Genauigkeit

| Zustand | Stratum | Leap Ind. | Typische Genauigkeit |
|---|---|---|---|
| GPS + PPS aktiv | 1 | LI=0 | < 10 µs |
| GPS-Fix, kein PPS | 2 | LI=0 | ~200 ms |
| GPS-Holdover | 3 | LI=3 | steigend |
| Kein GPS (SNTP) | 16 | LI=3 | ~50 ms (PTB) |

Der Reference-Identifier im NTP-Paket ist `GPS\0`.

---

## Software & Abhängigkeiten

- [PlatformIO](https://platformio.org/) mit Arduino-Framework für ESP32
- [TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) — Display-Treiber (`ILI9341_2_DRIVER`)
- [TinyGPS++](https://github.com/mikalhart/TinyGPSPlus) — NMEA-Parser
- [WiFiManager](https://github.com/tzapu/WiFiManager) — Captive-Portal-Setup
- [XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen) — Touch-Controller

---

## Installation

### 1. Repository klonen

```bash
git clone https://github.com/XerxesKB/cyd-gps-ntp.git
cd cyd-gps-ntp
```

### 2. Erstmalig per USB flashen

```bash
pio run -e cyd_usb -t upload
```

### 3. WLAN konfigurieren

Beim ersten Start öffnet das CYD einen WLAN-Access-Point **„CYD-NTP-Setup"** (offen). Verbinde dich damit und gib deine WLAN-Zugangsdaten ein. Nach dem Verbinden erscheint die IP-Adresse auf dem Display.

Alternativ: Timeout nach 60 s → **Standalone-Modus** (nur GPS-Display, kein NTP-Server).

### 4. Ab sofort OTA updaten

```bash
pio run -t upload          # Sendet an time.local (OTA, Passwort: cyd-ntp)
```

---

## Konfiguration

Alle zentralen Einstellungen in `src/config/config.h`:

```cpp
#define MDNS_HOSTNAME  "time"          // → http://time.local/
#define OTA_PASSWORD   "cyd-ntp"
#define TZ_POSIX       "CET-1CEST,M3.5.0,M10.5.0/3"   // Europe/Berlin
#define NTP_PORT       123
#define STRATUM        1               // Referenzwert; tatsächlich dynamisch
```

---

## Projektstruktur

```
src/
├── main.cpp                  Setup, Loop, GPS+PPS-Sync, Watchdog, NTP-Stratum
├── config/config.h           Zentrale Konstanten & Pin-Definitionen
├── gps/                      UART-Handler, Baudrate-Scan, PPS-ISR
├── ntp/                      NTP-Server (UDP, eigen) + SNTP-Fallback
├── display/                  Hauptseite, Detailseite, Touch-Manager
├── wifi/                     WiFiManager, Web-Server (/data JSON-API)
├── ota/                      ArduinoOTA, Fortschrittsbalken
├── led/                      RGB-LED (common anode, reserviert)
└── util/sun.cpp              Sonnenaufgang/-untergang (USNO-Algorithmus)
```

---

## Versionshistorie (Auszug)

| Version | Highlights |
|---|---|
| v1.0.4-rc | Sonnen-Icons vertikal zentriert; aktuelle Hauptversion |
| v1.0.3-rc | Dynamischer Stratum/LI, Hardware-Watchdog, Wochentag |
| v1.0.2-rc | Hauptseite-Redesign: Sonnen-Icons, Status+HDOP-Zeile |
| v0.2.0 | OTA-Updates eingeführt |
| v0.1.11 | Displaytreiber-Bug behoben (ILI9341_2_DRIVER) |

Vollständige Historie in [CLAUDE.md](CLAUDE.md).

---

## Lizenz

MIT
