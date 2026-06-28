#include <Arduino.h>
#include <sys/time.h>
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "config/config.h"
#include "gps/gps_handler.h"
#include "gps/pps_handler.h"
#include "ntp/ntp_server.h"
#include "display/display_manager.h"
#include "wifi/wifi_manager.h"
#include "wifi/web_server.h"
#include "ota/ota_handler.h"
#include "ntp/ntp_client.h"
#include "led/led.h"

// Stratum: 1=GPS+PPS, 2=GPS only, 3=Holdover (Fix vorhanden, Signal weg), 16=unkalibriert
uint8_t ntp_current_stratum() {
    bool gpsFresh = gpsData.valid && (millis() - gpsData.lastUpdateMs < 3000);
    bool ppsAlive = (ppsMicros > 0) &&
                    ((esp_timer_get_time() - (int64_t)ppsMicros) < 2000000LL);
    if (gpsFresh && ppsAlive) return 1;
    if (gpsFresh)             return 2;
    if (gpsData.valid)        return 3;
    return 16;
}

// LI=0 (keine Warnung) wenn GPS frisch; LI=3 (Alarm) sonst
uint8_t ntp_current_li() {
    bool gpsFresh = gpsData.valid && (millis() - gpsData.lastUpdateMs < 3000);
    return gpsFresh ? 0 : 3;
}

static bool     ppsSynced         = false;
static bool     ntpFallbackActive = true;
static uint64_t lastSyncPpsMicros = 0;

static void syncSystemClock() {
    if (!gpsData.valid) return;

    uint64_t currentEspMicros = (uint64_t)esp_timer_get_time();
    uint64_t ppsTs            = ppsMicros;
    if (ppsTs == 0) return;

    int64_t ageMicros = (int64_t)(currentEspMicros - ppsTs);
    if (ageMicros < 0 || ageMicros > 950000LL) return;
    if (ppsTs == lastSyncPpsMicros) return;

    uint32_t gpsSec = gps_to_unix();
    if (gpsSec == 0) return;

    struct timeval tv;
    tv.tv_sec  = (time_t)(gpsSec + ageMicros / 1000000LL);
    tv.tv_usec = (suseconds_t)(ageMicros % 1000000LL);
    settimeofday(&tv, nullptr);

    lastSyncPpsMicros = ppsTs;

    if (!ppsSynced) {
        ppsSynced = true;
        Serial.printf("[SYNC] Erster GPS+PPS-Lock: %04d-%02d-%02d %02d:%02d:%02d UTC  "
                      "HDOP=%.1f  Sats=%d\n",
                      gpsData.year, gpsData.month, gpsData.day,
                      gpsData.hour, gpsData.minute, gpsData.second,
                      gpsData.hdop, gpsData.satellites);
    }

    if (ntpFallbackActive && wifi_connected()) {
        ntp_client_stop();
        ntpFallbackActive = false;
        Serial.println("[SYNC] NTP-Fallback gestoppt – GPS+PPS übernimmt");
    }
}

// Periodisches Status-Log alle 60s
static unsigned long lastStatusLogMs = 0;
static void logPeriodicStatus() {
    if (millis() - lastStatusLogMs < 60000) return;
    lastStatusLogMs = millis();

    uint32_t upSec = millis() / 1000;
    uint32_t upD   = upSec / 86400;
    uint32_t upH   = (upSec % 86400) / 3600;
    uint32_t upM   = (upSec % 3600)  / 60;
    char upStr[24];
    snprintf(upStr, sizeof(upStr), "%lud %02luh%02lum",
             (unsigned long)upD, (unsigned long)upH, (unsigned long)upM);

    struct timeval tv; gettimeofday(&tv, nullptr);
    struct tm utcTm;   gmtime_r(&tv.tv_sec, &utcTm);

    if (gpsData.valid) {
        bool ppsAlive = (ppsMicros > 0) &&
                        ((esp_timer_get_time() - (int64_t)ppsMicros) < 2000000LL);
        Serial.printf("[STATUS] %02d:%02d:%02d UTC | GPS Sats=%d HDOP=%.1f | "
                      "PPS=%s | Stratum=%u | Uptime=%s | NTP-Anfragen=%lu\n",
                      utcTm.tm_hour, utcTm.tm_min, utcTm.tm_sec,
                      gpsData.satellites, gpsData.hdop,
                      ppsAlive ? "locked" : "lost",
                      (unsigned)ntp_current_stratum(),
                      upStr,
                      (unsigned long)ntpServer.requestCount());
    } else {
        Serial.printf("[STATUS] %02d:%02d:%02d UTC | GPS=warte... | "
                      "Uptime=%s | NTP-Anfragen=%lu\n",
                      utcTm.tm_hour, utcTm.tm_min, utcTm.tm_sec,
                      upStr,
                      (unsigned long)ntpServer.requestCount());
    }
}

static unsigned long lastDisplayMs = 0;

void setup() {
    Serial.begin(115200);
    Serial.printf("\n[CYD-NTP] ========== Start v%s ==========\n", FW_VERSION);

    led_init();

    setenv("TZ", TZ_POSIX, 1);
    tzset();

    display_init();
    Serial.println("[CYD-NTP] Display initialisiert");

    pps_init();
    Serial.printf("[GPS] PPS-Pin %d konfiguriert (RISING)\n", PPS_PIN);

    gps_init();

    wifi_init();

    ntpServer.begin();
    Serial.printf("[NTP] Server gestartet auf Port %d\n", NTP_PORT);

    web_server_begin();
    Serial.println("[WEB] HTTP-Server gestartet");

    esp_task_wdt_init(60, true);   // 60s Timeout, Panic bei Ablauf
    esp_task_wdt_add(NULL);        // aktuellen Task (Loop) überwachen
    Serial.println("[WDT] Hardware-Watchdog aktiv (60s)");

    Serial.println("[CYD-NTP] Bereit – warte auf GPS-Fix...");
}

void loop() {
    esp_task_wdt_reset();

    // Während OTA: nur OTA bedienen, alles andere pausieren
    if (wifi_connected()) ota_handle();
    if (ota_in_progress()) return;

    gps_update();
    pps_new_pulse();
    syncSystemClock();

    ntpServer.handle();
    web_server_handle();
    logPeriodicStatus();

    if (millis() - lastDisplayMs >= 100) {
        lastDisplayMs = millis();
        display_update();
    }
}
