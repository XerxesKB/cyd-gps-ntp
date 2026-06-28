#include "web_server.h"
#include "../config/config.h"
#include "../gps/gps_handler.h"
#include "../gps/pps_handler.h"
#include "../ntp/ntp_server.h"
#include "../ntp/ntp_client.h"
#include "../util/sun.h"
#include <WebServer.h>
#include <WiFi.h>
#include <sys/time.h>
#include <time.h>

static WebServer server(80);

static void buildTimeStrings(char* localTime, char* utcTime, char* date) {
    struct timeval tv; gettimeofday(&tv, nullptr);
    time_t epoch = tv.tv_sec;
    struct tm lTm, uTm;
    localtime_r(&epoch, &lTm);
    gmtime_r(&epoch,    &uTm);
    snprintf(localTime, 9,  "%02d:%02d:%02d", lTm.tm_hour, lTm.tm_min, lTm.tm_sec);
    snprintf(utcTime,   9,  "%02d:%02d:%02d", uTm.tm_hour, uTm.tm_min, uTm.tm_sec);
    snprintf(date,      11, "%02d.%02d.%04d", lTm.tm_mday, lTm.tm_mon + 1, lTm.tm_year + 1900);
}

static const char* getStatusText(bool ppsAlive, bool gpsFresh) {
    if (!gpsData.valid) return ntp_client_synced() ? "NTP Zeit (warte auf GPS)" : "Warte auf GPS / NTP...";
    if (!gpsFresh)      return "GPS signal lost";
    if (ppsAlive)       return "PPS locked \xe2\x80\x93 Stratum 1";
    return "GPS OK (no PPS)";
}
static const char* getStatusClass(bool ppsAlive, bool gpsFresh) {
    if (!gpsData.valid) return "warn";
    if (!gpsFresh)      return "err";
    return ppsAlive ? "ok" : "warn";
}
static const char* hdopClass(float h) {
    return h < 2.0f ? "v-ok" : (h < 5.0f ? "v-warn" : "v-err");
}

// ---------------------------------------------------------------------------
// GET /data  ->  JSON
// ---------------------------------------------------------------------------
static void handleData() {
    char localTime[9], utcTime[9], date[11];
    buildTimeStrings(localTime, utcTime, date);

    bool ppsAlive = (ppsMicros > 0) &&
                    ((esp_timer_get_time() - (int64_t)ppsMicros) < 2000000LL);
    bool gpsFresh = gpsData.valid && (millis() - gpsData.lastUpdateMs < 3000);

    // Uptime -- always include days
    uint32_t upSec = millis() / 1000;
    uint32_t upD   = upSec / 86400;
    uint32_t upH   = (upSec % 86400) / 3600;
    uint32_t upM   = (upSec % 3600)  / 60;
    uint32_t upS   = upSec % 60;
    char upStr[24];
    snprintf(upStr, sizeof(upStr), "%lud %02luh %02lum %02lus",
             (unsigned long)upD, (unsigned long)upH, (unsigned long)upM, (unsigned long)upS);

    // Sunrise / sunset
    char sunriseStr[6] = "--:--";
    char sunsetStr[6]  = "--:--";
    if (gpsData.fixAcquired) {
        struct timeval tv; gettimeofday(&tv, nullptr);
        struct tm lTm; localtime_r(&tv.tv_sec, &lTm);
        int rH, rM, sH, sM;
        if (sun_calc(gpsData.lat, gpsData.lon,
                     lTm.tm_year + 1900, lTm.tm_mon + 1, lTm.tm_mday,
                     &rH, &rM, &sH, &sM)) {
            snprintf(sunriseStr, sizeof(sunriseStr), "%02d:%02d", rH, rM);
            snprintf(sunsetStr,  sizeof(sunsetStr),  "%02d:%02d", sH, sM);
        }
    }

    char buf[750];
    snprintf(buf, sizeof(buf),
        "{"
        "\"localTime\":\"%s\","
        "\"utcTime\":\"%s\","
        "\"date\":\"%s\","
        "\"status\":\"%s\","
        "\"statusClass\":\"%s\","
        "\"fixAcquired\":%s,"
        "\"lat\":%.5f,"
        "\"lon\":%.5f,"
        "\"satellites\":%d,"
        "\"hdop\":%.2f,"
        "\"hdopClass\":\"%s\","
        "\"gpsBaud\":\"%lu Bd\","
        "\"nmeaOk\":%lu,"
        "\"nmeaErr\":%lu,"
        "\"gpsBytes\":%lu,"
        "\"ppsLocked\":%s,"
        "\"ppsPulseCount\":%lu,"
        "\"ntpRequests\":%lu,"
        "\"stratum\":%u,"
        "\"rssi\":%d,"
        "\"uptime\":\"%s\","
        "\"sunrise\":\"%s\","
        "\"sunset\":\"%s\","
        "\"firmware\":\"v" FW_VERSION "\""
        "}",
        localTime, utcTime, date,
        getStatusText(ppsAlive, gpsFresh),
        getStatusClass(ppsAlive, gpsFresh),
        gpsData.fixAcquired ? "true" : "false",
        gpsData.lat, gpsData.lon,
        gpsData.satellites,
        gpsData.hdop, hdopClass(gpsData.hdop),
        (unsigned long)gpsBaudCurrent,
        (unsigned long)gps.passedChecksum(),
        (unsigned long)gps.failedChecksum(),
        (unsigned long)gpsRawBytes,
        ppsAlive ? "true" : "false",
        (unsigned long)ppsPulseCount,
        (unsigned long)ntpServer.requestCount(),
        (unsigned)ntp_current_stratum(),
        WiFi.RSSI(),
        upStr,
        sunriseStr,
        sunsetStr
    );

    server.sendHeader("Cache-Control", "no-cache");
    server.send(200, "application/json", buf);
}

// ---------------------------------------------------------------------------
// GET /  ->  HTML-Shell (once; JS fetches /data every second)
// ---------------------------------------------------------------------------
static const char PAGE_CSS[] =
    "<!DOCTYPE html><html lang='de'><head>"
    "<meta charset='utf-8'>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>CYD GPS NTP</title>"
    "<style>"
    "*{box-sizing:border-box;margin:0;padding:0}"
    "body{font-family:monospace;background:#000;color:#eee;"
         "display:flex;flex-direction:column;align-items:center;"
         "padding:20px;gap:16px}"
    ".clock{text-align:center}"
    ".time-local{font-size:72px;font-weight:bold;color:#f87000;"
                "letter-spacing:4px;line-height:1}"
    ".time-utc{font-size:36px;color:#fff;letter-spacing:2px;margin-top:4px}"
    ".utc-label{font-size:14px;color:#555;margin-left:6px;vertical-align:middle}"
    ".date{font-size:20px;color:#ccc;margin-top:6px}"
    "hr{width:100%;max-width:520px;border:none;border-top:1px solid #1a1a1a}"
    ".status{font-size:15px;padding:6px 16px;border-radius:4px;font-weight:bold}"
    ".ok  {color:#0f0;border:1px solid #0f0}"
    ".warn{color:#f87000;border:1px solid #f87000}"
    ".err {color:#f00;border:1px solid #f00}"
    "table{width:100%;max-width:520px;border-collapse:collapse}"
    "tr{border-bottom:1px solid #1a1a1a}"
    "td{padding:7px 10px;font-size:14px}"
    "td:first-child{color:#666;width:45%}"
    "td:last-child{color:#eee}"
    "td.v-ok{color:#0f0}td.v-warn{color:#f87000}td.v-err{color:#f00}"
    ".section{color:#555;font-size:11px;text-transform:uppercase;"
             "letter-spacing:2px;padding-top:10px}"
    "footer{font-size:11px;color:#333;margin-top:8px}"
    "</style></head><body>";

static const char PAGE_JS[] =
    "<script>"
    "function set(id,txt,cls){"
    "var e=document.getElementById(id);if(!e)return;"
    "if(e.textContent!==String(txt))e.textContent=txt;"
    "if(cls!==undefined&&e.className!==cls)e.className=cls;}"
    "function fmtSI(n){"
    "if(n>=1e12)return(n/1e12).toFixed(1)+'T';"
    "if(n>=1e9) return(n/1e9).toFixed(1)+'G';"
    "if(n>=1e6) return(n/1e6).toFixed(1)+'M';"
    "if(n>=1e3) return(n/1e3).toFixed(1)+'k';"
    "return String(n);}"
    "function update(){"
    "fetch('/data').then(r=>r.json()).then(d=>{"
    "set('lt',d.localTime);"
    "set('ut',d.utcTime);"
    "set('dt',d.date);"
    "var s=document.getElementById('st');"
    "if(s){s.textContent=d.status;s.className='status '+d.statusClass;}"
    "if(d.fixAcquired){"
    "set('coord',d.lat.toFixed(5)+'\xc2\xb0,  '+d.lon.toFixed(5)+'\xc2\xb0','');}"
    "else{set('coord','kein Fix','v-warn');}"
    "set('sr',d.sunrise);"
    "set('ss',d.sunset);"
    "set('sats',d.satellites);"
    "set('hdop',d.hdop.toFixed(2),d.hdopClass);"
    "set('baud',d.gpsBaud);"
    "set('nmea','ok: '+fmtSI(d.nmeaOk)+'  err: '+fmtSI(d.nmeaErr));"
    "var gb=document.getElementById('gbytes');"
    "if(gb){gb.textContent=fmtSI(d.gpsBytes)+' B';"
    "gb.className=d.gpsBytes>0?'v-ok':'v-err';}"
    "set('ppss',d.ppsLocked?'locked':'kein Signal',d.ppsLocked?'v-ok':'v-err');"
    "set('ppsc',fmtSI(d.ppsPulseCount));"
    "set('str',d.stratum);"
    "set('ntpr',fmtSI(d.ntpRequests));"
    "set('rssi',d.rssi+' dBm');"
    "set('up',d.uptime);"
    "set('fw',d.firmware);"
    "set('foot','Letzte Aktualisierung: '+d.utcTime+' UTC');"
    "}).catch(()=>set('foot','Verbindung unterbrochen\xe2\x80\xa6'));}"
    "update();setInterval(update,1000);"
    "</script>";

static void handleRoot() {
    String html;
    html.reserve(3600);
    html += PAGE_CSS;

    html += "<div class='clock'>"
            "<div class='time-local' id='lt'>--:--:--</div>"
            "<div class='time-utc'><span id='ut'>--:--:--</span>"
            "<span class='utc-label'>UTC</span></div>"
            "<div class='date' id='dt'>--.--.----</div>"
            "</div>"
            "<div id='st' class='status'>\xe2\x80\xa6</div>"
            "<hr><table>"
            "<tr><td class='section' colspan='2'>GPS</td></tr>"
            "<tr><td>Koordinaten</td><td id='coord'>\xe2\x80\x93</td></tr>"
            "<tr><td>Sonnenaufgang</td><td id='sr'>\xe2\x80\x93</td></tr>"
            "<tr><td>Sonnenuntergang</td><td id='ss'>\xe2\x80\x93</td></tr>"
            "<tr><td>Satelliten</td><td id='sats'>\xe2\x80\x93</td></tr>"
            "<tr><td>HDOP</td><td id='hdop'>\xe2\x80\x93</td></tr>"
            "<tr><td>Baudrate</td><td id='baud'>\xe2\x80\x93</td></tr>"
            "<tr><td>NMEA S\xc3\xa4tze</td><td id='nmea'>\xe2\x80\x93</td></tr>"
            "<tr><td>GPS Bytes</td><td id='gbytes'>\xe2\x80\x93</td></tr>"
            "<tr><td class='section' colspan='2'>PPS</td></tr>"
            "<tr><td>Status</td><td id='ppss'>\xe2\x80\x93</td></tr>"
            "<tr><td>Pulse gesamt</td><td id='ppsc'>\xe2\x80\x93</td></tr>"
            "<tr><td class='section' colspan='2'>NTP</td></tr>";
    html += "<tr><td>Stratum</td><td id='str'>\xe2\x80\x93</td></tr>"
            "<tr><td>Anfragen</td><td id='ntpr'>\xe2\x80\x93</td></tr>"
            "<tr><td class='section' colspan='2'>Netzwerk</td></tr>";
    html += "<tr><td>IP-Adresse</td><td>";
    html += WiFi.localIP().toString();
    html += "</td></tr><tr><td>mDNS</td><td>" MDNS_HOSTNAME ".local</td></tr>"
            "<tr><td>WLAN</td><td>";
    html += WiFi.SSID();
    html += "</td></tr>"
            "<tr><td>RSSI</td><td id='rssi'>\xe2\x80\x93</td></tr>"
            "<tr><td class='section' colspan='2'>System</td></tr>"
            "<tr><td>Uptime</td><td id='up'>\xe2\x80\x93</td></tr>"
            "<tr><td>Firmware</td><td id='fw'>\xe2\x80\x93</td></tr>"
            "</table>"
            "<footer id='foot'>Verbinde\xe2\x80\xa6</footer>";

    html += PAGE_JS;
    html += "</body></html>";

    server.send(200, "text/html; charset=utf-8", html);
}

// ---------------------------------------------------------------------------
void web_server_begin() {
    server.on("/",     handleRoot);
    server.on("/data", handleData);
    server.begin();
}

void web_server_handle() {
    server.handleClient();
}
