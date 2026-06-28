#include "display_main.h"
#include "display_manager.h"
#include "../config/config.h"
#include "../gps/gps_handler.h"
#include "../gps/pps_handler.h"
#include "../wifi/wifi_manager.h"
#include "../ntp/ntp_client.h"
#include "../util/sun.h"
#include <Arduino.h>
#include <sys/time.h>
#include <time.h>

#define C_BG    TFT_BLACK
#define C_TIME  0xFB80    // dunkles Orange – Lokalzeit
#define C_UTC   TFT_WHITE
#define C_TEXT  TFT_WHITE
#define C_DIM   0x9CD3
#define C_GOOD  TFT_GREEN
#define C_WARN  TFT_ORANGE
#define C_BAD   TFT_RED
#define C_DIV   0x2100
#define C_SUN   0xFEE0    // warmes Gelb

// Hauptuhr: 8 Slots à 40px = 320px (volle Breite)
#define TIME_Y      5
#define TIME_SLOT   40

// UTC-Uhr: 8 Slots à 30px = 240px, zentriert (startX = 40)
#define UTC_Y       97
#define UTC_SLOT    30
#define UTC_START_X ((320 - UTC_SLOT * 8) / 2)   // = 40

// Sonnen-Icons: je Hälfte zentriert, unterhalb UTC-Uhr
//   UTC endet ca. y=145 (97 + 48px Font7)
#define SUN_ICON_Y   157   // oberer Rand der Icons — vertikal zentriert zwischen UTC (y≈145) und Status (y=222)
#define SUN_TIME_Y   184   // Zeittext (Font4, 26px → endet y=210, Abstand unten ≈12px)
#define SUN_RISE_CX   80   // Aufgang: Mitte linke Hälfte
#define SUN_SET_CX   240   // Untergang: Mitte rechte Hälfte

// Unterste Zeile: Status links + HDOP rechts
#define BOTTOM_Y    222    // Font2 (16px) → endet y=238

static char prevLocalTime[9] = "";
static char prevDate[16]     = "";
static char prevUtcTime[9]   = "";
static char prevStatus[40]   = "";
static char prevHdop[16]     = "";
static char prevSunrise[6]   = "";
static char prevSunset[6]    = "";

// ---------------------------------------------------------------------------
// Sonnen-Icon mit Halbkreis, Horizont und Richtungspfeil
// isRise = true:  Pfeil oben  (Sonnenaufgang)
// isRise = false: Pfeil unten (Sonnenuntergang)
// topY: oberer Rand des Icon-Bereichs (~22px hoch)
// ---------------------------------------------------------------------------
static void drawSunIcon(int cx, int topY, bool isRise) {
    const int r  = 9;   // Sonnenradius
    const int aW = 6;   // Pfeil-Halbbreite
    const int aH = 6;   // Pfeil-Höhe

    if (isRise) {
        // ↑ Pfeil am oberen Rand
        tft.fillTriangle(cx, topY,
                         cx - aW, topY + aH,
                         cx + aW, topY + aH, C_SUN);

        // Halbkreis darunter (Zentrum bei topY+aH+5+r = topY+20)
        int sunY = topY + aH + 5 + r;
        tft.fillCircle(cx, sunY, r, C_SUN);
        tft.fillRect(cx - r, sunY + 1, r * 2 + 1, r + 1, C_BG);  // untere Hälfte frei

        // Schrägstrahlen (keine oberen Strahlen → würden in Pfeil ragen)
        tft.drawLine(cx - 6, sunY - 7, cx - 9, sunY - 10, C_SUN);
        tft.drawLine(cx + 6, sunY - 7, cx + 9, sunY - 10, C_SUN);
        tft.drawLine(cx - r - 1, sunY - 3, cx - r - 4, sunY - 3, C_SUN);
        tft.drawLine(cx + r + 1, sunY - 3, cx + r + 4, sunY - 3, C_SUN);

        // Horizont
        tft.drawFastHLine(cx - r - 7, sunY, (r + 7) * 2, C_SUN);

    } else {
        // Halbkreis oben (Zentrum bei topY+r = topY+9)
        int sunY = topY + r;
        tft.fillCircle(cx, sunY, r, C_SUN);
        tft.fillRect(cx - r, sunY + 1, r * 2 + 1, r + 1, C_BG);

        // Strahlen oben
        tft.drawLine(cx,     sunY - r - 2, cx,     sunY - r - 5, C_SUN);
        tft.drawLine(cx - 6, sunY - 7,     cx - 9, sunY - 10,    C_SUN);
        tft.drawLine(cx + 6, sunY - 7,     cx + 9, sunY - 10,    C_SUN);
        tft.drawLine(cx - r - 1, sunY - 3, cx - r - 4, sunY - 3, C_SUN);
        tft.drawLine(cx + r + 1, sunY - 3, cx + r + 4, sunY - 3, C_SUN);

        // Horizont
        tft.drawFastHLine(cx - r - 7, sunY, (r + 7) * 2, C_SUN);

        // ↓ Pfeil unterhalb Horizont
        int arrowBase = sunY + 5;
        tft.fillTriangle(cx, arrowBase + aH,
                         cx - aW, arrowBase,
                         cx + aW, arrowBase, C_SUN);
    }
}

void display_main_draw(bool fullRedraw) {
    struct timeval tv;
    gettimeofday(&tv, nullptr);
    time_t nowEpoch = tv.tv_sec;
    struct tm localTm, utcTm;
    localtime_r(&nowEpoch, &localTm);
    gmtime_r(&nowEpoch, &utcTm);

    static const char* wdays[] = {"So","Mo","Di","Mi","Do","Fr","Sa"};
    char localTime[9], date[16], utcTime[9];
    snprintf(localTime, sizeof(localTime), "%02d:%02d:%02d",
             localTm.tm_hour, localTm.tm_min, localTm.tm_sec);
    snprintf(date, sizeof(date), "%s %02d.%02d.%04d",
             wdays[localTm.tm_wday], localTm.tm_mday, localTm.tm_mon + 1, localTm.tm_year + 1900);
    snprintf(utcTime, sizeof(utcTime), "%02d:%02d:%02d",
             utcTm.tm_hour, utcTm.tm_min, utcTm.tm_sec);

    bool gpsFresh = gpsData.valid && (millis() - gpsData.lastUpdateMs < 3000);
    bool ppsAlive = (ppsPulseCount > 0) && (ppsMicros > 0) &&
                    ((esp_timer_get_time() - (int64_t)ppsMicros) < 2000000LL);

    // Statustext (Kurzform damit er links neben HDOP passt)
    char status[36];
    uint16_t statusColor;
    if (!gpsData.valid) {
        if (ntp_client_synced()) {
            strncpy(status, "NTP (kein GPS)", sizeof(status));
        } else {
            strncpy(status, "Warte GPS/NTP...", sizeof(status));
        }
        statusColor = C_WARN;
    } else if (!gpsFresh) {
        strncpy(status, "GPS signal lost", sizeof(status));
        statusColor = C_BAD;
    } else if (ppsAlive) {
        strncpy(status, "PPS locked  Stratum 1", sizeof(status));
        statusColor = C_GOOD;
    } else {
        strncpy(status, "GPS OK  (kein PPS)", sizeof(status));
        statusColor = C_WARN;
    }

    // HDOP
    char hdopStr[16];
    uint16_t hdopCol;
    if (gpsData.valid) {
        snprintf(hdopStr, sizeof(hdopStr), "HDOP: %.1f", gpsData.hdop);
        hdopCol = (gpsData.hdop < 2.0f) ? C_GOOD :
                  (gpsData.hdop < 5.0f) ? C_WARN : C_BAD;
    } else {
        strncpy(hdopStr, "HDOP: --", sizeof(hdopStr));
        hdopCol = C_DIM;
    }

    // Sonnenauf- / Sonnenuntergang
    char sunrise[6] = "--:--";
    char sunset[6]  = "--:--";
    if (gpsData.fixAcquired) {
        int rH, rM, sH, sM;
        if (sun_calc(gpsData.lat, gpsData.lon,
                     localTm.tm_year + 1900, localTm.tm_mon + 1, localTm.tm_mday,
                     &rH, &rM, &sH, &sM)) {
            snprintf(sunrise, sizeof(sunrise), "%02d:%02d", rH, rM);
            snprintf(sunset,  sizeof(sunset),  "%02d:%02d", sH, sM);
        }
    }

    int16_t w  = tft.width();
    int16_t cx = w / 2;

    if (fullRedraw) {
        tft.fillScreen(C_BG);

        // Trennlinie
        tft.drawFastHLine(0, 93, w, C_DIV);

        // "UTC"-Label
        tft.setTextFont(2);
        tft.setTextDatum(TL_DATUM);
        tft.setTextColor(C_DIM, C_BG);
        tft.drawString("UTC", UTC_START_X + UTC_SLOT * 8 + 6, UTC_Y + 16);

        // Sonnen-Icons (statisch, nur bei fullRedraw)
        drawSunIcon(SUN_RISE_CX, SUN_ICON_Y, true);
        drawSunIcon(SUN_SET_CX,  SUN_ICON_Y, false);

        prevLocalTime[0] = prevDate[0]    = prevUtcTime[0] = 0;
        prevStatus[0]    = prevHdop[0]    = 0;
        prevSunrise[0]   = prevSunset[0]  = 0;
    }

    // --- Hauptuhr ---
    {
        tft.setTextFont(7);
        tft.setTextSize(1);
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(C_TIME, C_BG);
        tft.setTextPadding(TIME_SLOT);
        for (int i = 0; i < 8; i++) {
            if (fullRedraw || localTime[i] != prevLocalTime[i]) {
                char ch[2] = {localTime[i], '\0'};
                tft.drawString(ch, TIME_SLOT * i + TIME_SLOT / 2, TIME_Y);
            }
        }
        tft.setTextPadding(0);
        strncpy(prevLocalTime, localTime, sizeof(prevLocalTime));
    }

    // --- Datum ---
    if (strcmp(date, prevDate) != 0) {
        tft.setTextFont(4);
        tft.setTextDatum(TC_DATUM);
        tft.setTextPadding(w - 10);
        tft.setTextColor(C_TEXT, C_BG);
        tft.drawString(date, cx, 62);
        tft.setTextPadding(0);
        strncpy(prevDate, date, sizeof(prevDate));
    }

    // --- UTC-Uhr ---
    if (strcmp(utcTime, prevUtcTime) != 0 || fullRedraw) {
        tft.setTextFont(7);
        tft.setTextSize(1);
        tft.setTextDatum(TC_DATUM);
        tft.setTextColor(C_UTC, C_BG);
        tft.setTextPadding(UTC_SLOT);
        for (int i = 0; i < 8; i++) {
            if (fullRedraw || utcTime[i] != prevUtcTime[i]) {
                char ch[2] = {utcTime[i], '\0'};
                tft.drawString(ch, UTC_START_X + UTC_SLOT * i + UTC_SLOT / 2, UTC_Y);
            }
        }
        tft.setTextPadding(0);
        strncpy(prevUtcTime, utcTime, sizeof(prevUtcTime));
    }

    // --- Sonnenzeiten: Font4, zentriert je Hälfte ---
    tft.setTextFont(4);
    tft.setTextDatum(TC_DATUM);
    tft.setTextPadding(140);
    if (strcmp(sunrise, prevSunrise) != 0 || fullRedraw) {
        tft.setTextColor(gpsData.fixAcquired ? C_SUN : C_DIM, C_BG);
        tft.drawString(sunrise, SUN_RISE_CX, SUN_TIME_Y);
        strncpy(prevSunrise, sunrise, sizeof(prevSunrise));
    }
    if (strcmp(sunset, prevSunset) != 0 || fullRedraw) {
        tft.setTextColor(gpsData.fixAcquired ? C_SUN : C_DIM, C_BG);
        tft.drawString(sunset, SUN_SET_CX, SUN_TIME_Y);
        strncpy(prevSunset, sunset, sizeof(prevSunset));
    }
    tft.setTextPadding(0);

    // --- Unterste Zeile: Status links + HDOP rechts ---
    if (strcmp(status, prevStatus) != 0 || fullRedraw) {
        tft.setTextFont(2);
        tft.setTextDatum(TL_DATUM);
        tft.setTextPadding(w - 88);
        tft.setTextColor(statusColor, C_BG);
        tft.drawString(status, 6, BOTTOM_Y);
        tft.setTextPadding(0);
        strncpy(prevStatus, status, sizeof(prevStatus));
    }
    if (strcmp(hdopStr, prevHdop) != 0 || fullRedraw) {
        tft.setTextFont(2);
        tft.setTextDatum(TR_DATUM);
        tft.setTextPadding(84);
        tft.setTextColor(hdopCol, C_BG);
        tft.drawString(hdopStr, w - 4, BOTTOM_Y);
        tft.setTextPadding(0);
        strncpy(prevHdop, hdopStr, sizeof(prevHdop));
    }
}
