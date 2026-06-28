#include "ota_handler.h"
#include "../config/config.h"
#include "../display/display_manager.h"
#include <ArduinoOTA.h>
#include <Arduino.h>
#include "esp_task_wdt.h"

static int16_t otaPrevFilled  = 0;
static bool    _otaInProgress = false;

bool ota_in_progress() { return _otaInProgress; }

static void drawOtaScreen(const char* typeStr) {
    int16_t cx = tft.width() / 2;
    otaPrevFilled = 0;
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(4);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString("OTA Update", cx, 55);
    tft.setTextFont(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString(typeStr, cx, 90);
    tft.drawRect(20, 120, tft.width() - 40, 22, TFT_WHITE);
}

void ota_init() {
    ArduinoOTA.setHostname(MDNS_HOSTNAME);
    ArduinoOTA.setPassword(OTA_PASSWORD);

    ArduinoOTA.onStart([]() {
        _otaInProgress      = true;
        const char* typeStr = (ArduinoOTA.getCommand() == U_FLASH)
                              ? "Firmware" : "Dateisystem";
        Serial.printf("[OTA] Start: %s – Hintergrundprozesse pausiert\n", typeStr);
        drawOtaScreen(typeStr);
    });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        esp_task_wdt_reset();   // OTA-Upload dauert ~25s, verhindert WDT-Reset
        uint8_t pct    = (uint8_t)(progress * 100 / total);
        int16_t innerW = tft.width() - 42;
        int16_t filled = (int16_t)(innerW * pct / 100);
        if (filled > otaPrevFilled) {
            tft.fillRect(21 + otaPrevFilled, 121, filled - otaPrevFilled, 20, TFT_GREEN);
            otaPrevFilled = filled;
        }
        char buf[8];
        snprintf(buf, sizeof(buf), "%3u%%", pct);
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.setTextPadding(50);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(buf, tft.width() / 2, 158);
        tft.setTextPadding(0);
        if (pct % 10 == 0) Serial.printf("[OTA] %u%%\n", pct);
    });

    ArduinoOTA.onEnd([]() {
        _otaInProgress = false;
        Serial.println("[OTA] Fertig – Neustart");
        int16_t cx = tft.width() / 2;
        tft.fillRect(21, 121, tft.width() - 42, 20, TFT_GREEN);
        tft.setTextFont(4);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("Fertig!", cx, 190);
        tft.setTextFont(2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Neustart...", cx, 218);
    });

    ArduinoOTA.onError([](ota_error_t error) {
        _otaInProgress = false;
        const char* msg;
        switch (error) {
            case OTA_AUTH_ERROR:    msg = "Auth fehlgeschlagen"; break;
            case OTA_BEGIN_ERROR:   msg = "Begin-Fehler";        break;
            case OTA_CONNECT_ERROR: msg = "Verbindungsfehler";   break;
            case OTA_RECEIVE_ERROR: msg = "Empfangsfehler";      break;
            case OTA_END_ERROR:     msg = "End-Fehler";          break;
            default:                msg = "Unbekannter Fehler";  break;
        }
        Serial.printf("[OTA] Fehler: %s\n", msg);
        int16_t cx = tft.width() / 2;
        tft.setTextFont(2);
        tft.setTextDatum(MC_DATUM);
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.drawString("OTA Fehler:", cx, 190);
        tft.drawString(msg, cx, 210);
    });

    ArduinoOTA.begin();
    Serial.printf("[OTA] Bereit – %s.local : Passwort gesetzt\n", MDNS_HOSTNAME);
}

void ota_handle() {
    ArduinoOTA.handle();
}
