#include "ntp_client.h"
#include "../config/config.h"
#include <esp_sntp.h>
#include <Arduino.h>

static volatile bool _synced = false;

static void onSyncCallback(struct timeval* tv) {
    _synced = true;
    struct tm utcTm;
    gmtime_r(&tv->tv_sec, &utcTm);
    Serial.printf("[NTP] Fallback-Sync OK: %02d:%02d:%02d UTC\n",
                  utcTm.tm_hour, utcTm.tm_min, utcTm.tm_sec);
}

void ntp_client_begin() {
    sntp_set_time_sync_notification_cb(onSyncCallback);
    sntp_set_sync_interval(60000);   // 60s – ohne RTC driftet der ESP32 sonst merklich
    // PTB = Physikalisch-Technische Bundesanstalt (Deutsche Zeitnormal)
    configTzTime(TZ_POSIX,
                 "ptbtime1.ptb.de",
                 "ptbtime2.ptb.de",
                 "de.pool.ntp.org");
    Serial.println("[NTP] Fallback-NTP gestartet (60s-Intervall, ptbtime1.ptb.de, de.pool.ntp.org)");
}

void ntp_client_stop() {
    sntp_stop();
    Serial.println("[NTP] Fallback-NTP gestoppt – GPS übernimmt");
}

bool ntp_client_synced() {
    return _synced;
}
