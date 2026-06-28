#include "gps_handler.h"
#include "../config/config.h"
#include <HardwareSerial.h>
#include <Preferences.h>

GpsData     gpsData        = {};
TinyGPSPlus gps;
uint32_t    gpsRawBytes    = 0;
uint32_t    gpsBaudCurrent = 0;

static HardwareSerial gpsSerial(GPS_UART_NUM);

// --- Hilfsfunktionen UTC-Timestamp ---
static bool isLeapYear(int y) {
    return (y % 4 == 0 && y % 100 != 0) || (y % 400 == 0);
}
static int daysInMonth(int m, int y) {
    const int dom[] = {31,28,31,30,31,30,31,31,30,31,30,31};
    return (m == 2 && isLeapYear(y)) ? 29 : dom[m-1];
}

uint32_t gps_to_unix() {
    if (!gpsData.valid) return 0;
    uint32_t days = 0;
    for (int y = 1970; y < gpsData.year; y++)
        days += isLeapYear(y) ? 366 : 365;
    for (int m = 1; m < gpsData.month; m++)
        days += daysInMonth(m, gpsData.year);
    days += gpsData.day - 1;
    return days * 86400UL
         + (uint32_t)gpsData.hour   * 3600UL
         + (uint32_t)gpsData.minute * 60UL
         + (uint32_t)gpsData.second;
}

// --- Baudrate-Test: prüft auf '$'-Zeichen (NMEA-Satzanfang) ---
// Noise-Bytes bei falscher Baudrate enthalten nie '$' → zuverlässige Erkennung
static bool tryBaud(uint32_t baud) {
    gpsSerial.begin(baud, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    unsigned long start = millis();
    uint32_t dollars = 0, bytes = 0;
    while (millis() - start < 1500) {
        while (gpsSerial.available()) {
            char c = gpsSerial.read();
            gps.encode(c);
            bytes++;
            if (c == '$') dollars++;
        }
        delay(1);
    }
    Serial.printf("[GPS]   %lu Bd: %lu Bytes, %lu '$'\n",
                  (unsigned long)baud, (unsigned long)bytes, (unsigned long)dollars);
    if (dollars == 0) {
        gpsSerial.end();
        return false;
    }
    return true;
}

static void sendPmtk(const char* body) {
    uint8_t cs = 0;
    for (const char* p = body; *p; p++) cs ^= (uint8_t)*p;
    char buf[80];
    snprintf(buf, sizeof(buf), "$%s*%02X\r\n", body, cs);
    gpsSerial.print(buf);
}

void gps_init() {
    Preferences prefs;
    prefs.begin("gps", false);
    uint32_t savedBaud = prefs.getUInt("baud", 0);

    // Gespeicherte Baudrate testen — jetzt mit '$'-Prüfung
    if (savedBaud > 0) {
        Serial.printf("[GPS] Teste gespeicherte Baudrate %lu...\n", (unsigned long)savedBaud);
        if (tryBaud(savedBaud)) {
            gpsBaudCurrent = savedBaud;
            Serial.printf("[GPS] Baudrate %lu bestätigt (NMEA '$' gefunden)\n", (unsigned long)savedBaud);
            prefs.end();
            return;
        }
        Serial.println("[GPS] Keine NMEA-Daten bei gespeicherter Baudrate – lösche Preferences");
        prefs.remove("baud");
    }

    // Vollständiger Scan über alle Baudraten
    Serial.println("[GPS] Starte Baudrate-Scan...");
    const uint32_t bauds[] = GPS_BAUD_SCAN_LIST;
    for (uint32_t b : bauds) {
        if (tryBaud(b)) {
            gpsBaudCurrent = b;
            Serial.printf("[GPS] NMEA bei %lu Baud gefunden\n", (unsigned long)b);

            if (b != GPS_BAUD_FINAL) {
                Serial.printf("[GPS] Konfiguriere Modul auf %lu Baud (PMTK251)...\n", (unsigned long)GPS_BAUD_FINAL);
                char cmd[40];
                snprintf(cmd, sizeof(cmd), "PMTK251,%lu", (unsigned long)GPS_BAUD_FINAL);
                sendPmtk(cmd);
                delay(300);
                gpsSerial.end();
                delay(100);

                if (tryBaud(GPS_BAUD_FINAL)) {
                    gpsBaudCurrent = GPS_BAUD_FINAL;
                    prefs.putUInt("baud", GPS_BAUD_FINAL);
                    Serial.printf("[GPS] Modul läuft jetzt auf %lu Baud\n", (unsigned long)GPS_BAUD_FINAL);
                } else {
                    // Baudwechsel nicht übernommen — bleib bei gefundener Rate
                    gpsBaudCurrent = b;
                    prefs.putUInt("baud", b);
                    gpsSerial.begin(b, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
                    Serial.printf("[GPS] Baudwechsel fehlgeschlagen, bleibe bei %lu\n", (unsigned long)b);
                }
            } else {
                prefs.putUInt("baud", b);
            }
            prefs.end();
            return;
        }
    }

    Serial.println("[GPS] KEIN NMEA-Signal bei keiner Baudrate");
    gpsSerial.begin(9600, SERIAL_8N1, GPS_RX_PIN, GPS_TX_PIN);
    gpsBaudCurrent = 0;
    prefs.end();
}

void gps_update() {
    while (gpsSerial.available()) {
        char c = gpsSerial.read();
        gps.encode(c);
        gpsRawBytes++;
    }

    if (gps.time.isUpdated() && gps.date.isValid() && gps.time.isValid()) {
        bool wasValid = gpsData.valid;
        gpsData.valid        = true;
        gpsData.hour         = gps.time.hour();
        gpsData.minute       = gps.time.minute();
        gpsData.second       = gps.time.second();
        gpsData.centisecond  = gps.time.centisecond();
        gpsData.day          = gps.date.day();
        gpsData.month        = gps.date.month();
        gpsData.year         = gps.date.year();
        gpsData.lastUpdateMs = millis();

        if (!wasValid) {
            Serial.printf("[GPS] Erster Zeitfix: %04d-%02d-%02d %02d:%02d:%02d UTC\n",
                          gpsData.year, gpsData.month, gpsData.day,
                          gpsData.hour, gpsData.minute, gpsData.second);
        }
    }

    if (gps.location.isValid()) {
        bool wasFixed = gpsData.fixAcquired;
        gpsData.lat         = gps.location.lat();
        gpsData.lon         = gps.location.lng();
        gpsData.fixAcquired = true;
        if (!wasFixed) {
            Serial.printf("[GPS] Erster Positionsfix: %.5f, %.5f  Sats=%d  HDOP=%.1f\n",
                          gpsData.lat, gpsData.lon,
                          gpsData.satellites, gpsData.hdop);
        }
    }

    if (gps.satellites.isValid()) gpsData.satellites = gps.satellites.value();
    if (gps.hdop.isValid())       gpsData.hdop        = gps.hdop.hdop();
}
