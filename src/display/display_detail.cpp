#include "display_detail.h"
#include "display_manager.h"
#include "../config/config.h"
#include "../gps/gps_handler.h"
#include "../gps/pps_handler.h"
#include "../util/sun.h"
#include <Arduino.h>
#include <WiFi.h>
#include <sys/time.h>

#define C_BG     TFT_BLACK
#define C_HDR    0x2100
#define C_KEY    0x9CD3
#define C_VAL    TFT_WHITE
#define C_GOOD   TFT_GREEN
#define C_WARN   TFT_ORANGE
#define C_BAD    TFT_RED

static void fmtSI(char* buf, size_t len, uint32_t val) {
    if      (val >= 1000000000UL) snprintf(buf, len, "%.1fG", val / 1000000000.0f);
    else if (val >= 1000000UL)    snprintf(buf, len, "%.1fM", val / 1000000.0f);
    else if (val >= 1000UL)       snprintf(buf, len, "%.1fk", val / 1000.0f);
    else                          snprintf(buf, len, "%lu",   (unsigned long)val);
}

static void fmtUptime(char* buf, size_t len, uint32_t upSec) {
    uint32_t d = upSec / 86400;
    uint32_t h = (upSec % 86400) / 3600;
    uint32_t m = (upSec % 3600)  / 60;
    uint32_t s = upSec % 60;
    snprintf(buf, len, "%lud %02luh%02lum%02lus",
             (unsigned long)d, (unsigned long)h, (unsigned long)m, (unsigned long)s);
}

static uint16_t hdopColor(float h) {
    if (h < 2.0f) return C_GOOD;
    if (h < 5.0f) return C_WARN;
    return C_BAD;
}

static void detailKey(const char* key, int y) {
    tft.setTextFont(2);
    tft.setTextDatum(TL_DATUM);
    tft.setTextColor(C_KEY, C_BG);
    tft.drawString(key, 6, y);
}

static void detailVal(const char* val, int y, uint16_t valColor = C_VAL) {
    int16_t w = tft.width();
    tft.setTextFont(2);
    tft.setTextDatum(TL_DATUM);
    tft.setTextPadding(w - 124);
    tft.setTextColor(valColor, C_BG);
    tft.drawString(val, 120, y);
    tft.setTextPadding(0);
}

void display_detail_draw(bool fullRedraw) {
    int16_t w = tft.width();
    if (fullRedraw) {
        tft.fillScreen(C_BG);
        tft.fillRect(0, 0, w, 22, C_HDR);
        tft.setTextFont(2);
        tft.setTextColor(TFT_CYAN, C_HDR);
        tft.setTextDatum(TL_DATUM);
        tft.drawString("GPS Detail", 6, 5);
        tft.setTextColor(0x7BEF, C_HDR);
        tft.setTextDatum(TR_DATUM);
        tft.drawString("Touch -> zurueck", w - 4, 5);
    }

    char buf[44];

    if (fullRedraw) {
        detailKey("Koord.:",     28);
        detailKey("Auf/Unter:",  48);
        detailKey("Satelliten:", 68);
        detailKey("HDOP:",       88);
        detailKey("PPS Pulse:",  108);
        detailKey("GPS Bytes:",  128);
        detailKey("NMEA:",       148);
        detailKey("IP:",         168);
        detailKey("WLAN:",       188);
        detailKey("Uptime:",     208);
    }

    // Koordinaten (kombiniert auf einer Zeile)
    if (gpsData.fixAcquired) {
        snprintf(buf, sizeof(buf), "%.5f / %.5f", gpsData.lat, gpsData.lon);
        detailVal(buf, 28);
    } else {
        detailVal("kein Fix", 28, C_WARN);
    }

    // Sonnenaufgang / Sonnenuntergang
    {
        struct timeval tv; gettimeofday(&tv, nullptr);
        struct tm lTm; localtime_r(&tv.tv_sec, &lTm);
        int rH, rM, sH, sM;
        if (gpsData.fixAcquired &&
            sun_calc(gpsData.lat, gpsData.lon,
                     lTm.tm_year + 1900, lTm.tm_mon + 1, lTm.tm_mday,
                     &rH, &rM, &sH, &sM)) {
            snprintf(buf, sizeof(buf), "%02d:%02d / %02d:%02d", rH, rM, sH, sM);
            detailVal(buf, 48);
        } else {
            detailVal(gpsData.fixAcquired ? "--:-- / --:--" : "kein Fix", 48, C_KEY);
        }
    }

    // Satelliten
    snprintf(buf, sizeof(buf), "%d", gpsData.satellites);
    detailVal(buf, 68);

    // HDOP mit Ampelfarbe
    if (gpsData.fixAcquired) {
        snprintf(buf, sizeof(buf), "%.2f", gpsData.hdop);
        detailVal(buf, 88, hdopColor(gpsData.hdop));
    } else {
        detailVal("---", 88);
    }

    // PPS Pulse
    {
        bool ppsAlive = (ppsMicros > 0) &&
                        ((esp_timer_get_time() - (int64_t)ppsMicros) < 2000000LL);
        fmtSI(buf, sizeof(buf), ppsPulseCount);
        detailVal(buf, 108, ppsAlive ? C_GOOD : C_BAD);
    }

    // GPS Roh-Bytes + Baudrate
    if (gpsBaudCurrent > 0) {
        char si[12]; fmtSI(si, sizeof(si), gpsRawBytes);
        snprintf(buf, sizeof(buf), "%s B  @%lu Bd", si, (unsigned long)gpsBaudCurrent);
    } else {
        strncpy(buf, "kein Signal", sizeof(buf));
    }
    detailVal(buf, 128, gpsRawBytes > 0 ? C_VAL : C_BAD);

    // NMEA ok/err
    {
        uint32_t nmOk  = gps.passedChecksum();
        uint32_t nmErr = gps.failedChecksum();
        uint16_t color = (gpsRawBytes == 0) ? C_BAD : (nmOk > 0 ? C_GOOD : C_WARN);
        char siOk[10], siErr[10];
        fmtSI(siOk,  sizeof(siOk),  nmOk);
        fmtSI(siErr, sizeof(siErr), nmErr);
        snprintf(buf, sizeof(buf), "ok:%s  err:%s", siOk, siErr);
        detailVal(buf, 148, color);
    }

    // IP
    if (WiFi.status() == WL_CONNECTED)
        WiFi.localIP().toString().toCharArray(buf, sizeof(buf));
    else
        strncpy(buf, "192.168.4.1", sizeof(buf));
    detailVal(buf, 168);

    // SSID
    if (WiFi.status() == WL_CONNECTED)
        detailVal(WiFi.SSID().c_str(), 188);
    else
        detailVal(AP_SSID, 188, TFT_YELLOW);

    // Uptime + Firmware (immer mit Tagen)
    char up[20]; fmtUptime(up, sizeof(up), millis() / 1000);
    snprintf(buf, sizeof(buf), "%s v" FW_VERSION, up);
    detailVal(buf, 208);

    if (fullRedraw) {
        tft.setTextFont(1);
        tft.setTextColor(0x4208, C_BG);
        tft.setTextDatum(BC_DATUM);
        tft.drawString("Touch->Haupt | Auto 10s", w / 2, 238);
    }
}
