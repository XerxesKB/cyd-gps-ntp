#pragma once

#define FW_VERSION       "1.0.4-rc"
#define DISPLAY_DRIVER   "ILI9341"   // "ILI9341" oder "ST7789" – je nach CYD-Platine

// --- GPS UART ---
#define GPS_RX_PIN          35
#define GPS_TX_PIN          22
#define GPS_UART_NUM        2
#define GPS_BAUD_SCAN_LIST  {4800, 9600, 38400, 57600, 115200}
#define GPS_BAUD_FINAL      115200

// --- PPS ---
#define PPS_PIN        27
// AIR530Z: HIGH-Flanke = Sekunden-Beginn. Bei Problemen auf FALLING wechseln.
#define PPS_INTERRUPT  RISING

// --- NTP ---
#define NTP_PORT   123
#define STRATUM    1

// --- Display ---
#define DISPLAY_TIMEOUT_DETAIL  10000   // ms bis Auto-Rücksprung zur Hauptseite
#define TFT_BL_PIN              21
// ILI9341: Landscape normalerweise 320x240 bei Rotation 1 oder 3.
// Falls Bild auf dem Kopf steht: 3 statt 1 testen.
#define TFT_ROTATION            1
// Touch-Schwellwert: höher = weniger Falschdetektionen (Standard ~600, Rauschunterdrückung ~1200)
#define TOUCH_THRESHOLD         1200

// --- WiFi AP Fallback ---
#define AP_SSID      "CYD-NTP-Setup"
#define AP_PASSWORD  ""
// Zeit in Sekunden bis automatischer Wechsel in Standalone-Modus (kein WiFi)
#define WIFI_STANDALONE_TIMEOUT  60

// --- RGB LED (common anode, active LOW) ---
#define LED_RED_PIN    4
#define LED_GREEN_PIN  16
#define LED_BLUE_PIN   17

// --- OTA ---
#define MDNS_HOSTNAME  "time"        // erreichbar als time.local
#define OTA_PASSWORD   "cyd-ntp"    // Passwort für Arduino IDE / PlatformIO OTA

// --- Zeitzone (Europe/Berlin) ---
#define TZ_POSIX  "CET-1CEST,M3.5.0,M10.5.0/3"
