#include "wifi_manager.h"
#include "../config/config.h"
#include "../display/display_manager.h"
#include "../ota/ota_handler.h"
#include "../ntp/ntp_client.h"
#include <WiFi.h>
#include <WiFiManager.h>
#include <ESPmDNS.h>

static bool _connected = false;
static char _ipStr[24] = "Standalone (kein WiFi)";

const char* wifi_ip_str() { return _ipStr; }
bool        wifi_connected() { return _connected; }

static void apCallback(WiFiManager* wm) {
    Serial.printf("[WIFI] Kein gespeichertes WLAN – AP '%s' gestartet\n", AP_SSID);
    int16_t cx = tft.width() / 2;
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(4);
    tft.setTextColor(TFT_YELLOW, TFT_BLACK);
    tft.drawString("WiFi Setup", cx, 60);
    tft.setTextFont(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Verbinde mit WLAN:", cx, 100);
    tft.setTextColor(TFT_CYAN, TFT_BLACK);
    tft.drawString(AP_SSID, cx, 120);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("dann http://192.168.4.1", cx, 145);
    tft.drawString("oeffnen", cx, 165);
}

void wifi_init() {
    Serial.println("[WIFI] Verbindungsversuch...");
    int16_t cx = tft.width() / 2;
    tft.fillScreen(TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setTextFont(2);
    tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Verbinde mit WiFi...", cx, 120);

    WiFiManager wm;
    wm.setAPCallback(apCallback);
    wm.setConnectTimeout(WIFI_STANDALONE_TIMEOUT);
    wm.setConfigPortalTimeout(WIFI_STANDALONE_TIMEOUT);
    wm.setTitle("CYD GPS NTP Server");
    wm.setCustomHeadElement(
        "<style>body{font-family:sans-serif;}h1{color:#0af;}"
        ".msg{background:#1a1a2e;color:#eee;padding:10px;border-radius:6px;margin:10px 0;}"
        "</style><div class='msg'><b>WLAN-Konfiguration</b><br>"
        "Netzwerk auswaehlen und Passwort eingeben.<br>"
        "Danach startet der NTP-Server automatisch.</div>"
    );

    bool ok = wm.autoConnect(AP_SSID, AP_PASSWORD[0] ? AP_PASSWORD : nullptr);

    if (ok) {
        _connected = true;
        WiFi.localIP().toString().toCharArray(_ipStr, sizeof(_ipStr));
        Serial.printf("[WIFI] Verbunden mit '%s'  IP=%s  RSSI=%ddBm\n",
                      WiFi.SSID().c_str(), _ipStr, WiFi.RSSI());

        if (MDNS.begin(MDNS_HOSTNAME)) {
            MDNS.addService("ntp", "udp", 123);
            Serial.printf("[WIFI] mDNS: http://%s.local/\n", MDNS_HOSTNAME);
        }
        ota_init();
        ntp_client_begin();

        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextFont(2);
        tft.setTextColor(TFT_GREEN, TFT_BLACK);
        tft.drawString("WiFi verbunden!", cx, 110);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString(_ipStr, cx, 135);
        delay(1500);
    } else {
        _connected = false;
        WiFi.disconnect(true);
        WiFi.mode(WIFI_OFF);
        strncpy(_ipStr, "Standalone (kein WiFi)", sizeof(_ipStr));
        Serial.println("[WIFI] Kein WLAN – Standalone-Modus (kein NTP, kein OTA)");

        tft.fillScreen(TFT_BLACK);
        tft.setTextDatum(MC_DATUM);
        tft.setTextFont(4);
        tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        tft.drawString("Standalone-Modus", cx, 100);
        tft.setTextFont(2);
        tft.setTextColor(TFT_WHITE, TFT_BLACK);
        tft.drawString("Kein WiFi - nur GPS-Zeitanzeige", cx, 135);
        tft.drawString("NTP nicht verfuegbar", cx, 155);
        delay(2000);
    }
}
