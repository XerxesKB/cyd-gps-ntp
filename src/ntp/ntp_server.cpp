#include "ntp_server.h"
#include "../config/config.h"
#include <Arduino.h>
#include <sys/time.h>

NtpServer ntpServer;

static const uint32_t NTP_EPOCH_DELTA = 2208988800UL;

static void packU32(uint8_t* buf, uint32_t val) {
    buf[0] = val >> 24; buf[1] = val >> 16;
    buf[2] = val >> 8;  buf[3] = val;
}

static void packTimestamp(uint8_t* buf, uint32_t sec, uint32_t usec) {
    packU32(buf,     sec + NTP_EPOCH_DELTA);
    packU32(buf + 4, (uint32_t)(((uint64_t)usec << 32) / 1000000ULL));
}

void NtpServer::begin() {
    _udp.begin(NTP_PORT);
}

void NtpServer::handle() {
    int size = _udp.parsePacket();
    if (size < 48) return;

    uint8_t req[48];
    _udp.read(req, 48);

    IPAddress  clientIp   = _udp.remoteIP();
    uint16_t   clientPort = _udp.remotePort();

    sendResponse(clientIp, clientPort, req);

    _requestCount++;

    // Aktuelle UTC-Zeit für Log
    struct timeval tv; gettimeofday(&tv, nullptr);
    struct tm utcTm;   gmtime_r(&tv.tv_sec, &utcTm);
    Serial.printf("[NTP] Anfrage #%lu von %s  → %02d:%02d:%02d.%03lu UTC  Stratum %u\n",
                  (unsigned long)_requestCount,
                  clientIp.toString().c_str(),
                  utcTm.tm_hour, utcTm.tm_min, utcTm.tm_sec,
                  (unsigned long)(tv.tv_usec / 1000),
                  (unsigned)ntp_current_stratum());
}

void NtpServer::sendResponse(IPAddress ip, uint16_t port, const uint8_t* req) {
    struct timeval tv;
    gettimeofday(&tv, nullptr);

    uint8_t resp[48] = {};
    uint8_t li = ntp_current_li();
    resp[0] = (li << 6) | (4 << 3) | 4;   // LI(2) | VN=4(3) | Mode=4(3)
    resp[1] = ntp_current_stratum();
    resp[2] = 6;           // poll exponent
    resp[3] = 0xEC;        // precision: -20 (~1µs)

    packU32(resp + 4, 0);           // Root Delay: 0
    packU32(resp + 8, 0x00000104);  // Root Dispersion: ~1ms
    resp[12] = 'G'; resp[13] = 'P'; resp[14] = 'S'; resp[15] = 0;

    packTimestamp(resp + 16, (uint32_t)tv.tv_sec, (uint32_t)tv.tv_usec);
    memcpy(resp + 24, req + 40, 8);
    packTimestamp(resp + 32, (uint32_t)tv.tv_sec, (uint32_t)tv.tv_usec);

    gettimeofday(&tv, nullptr);
    packTimestamp(resp + 40, (uint32_t)tv.tv_sec, (uint32_t)tv.tv_usec);

    _udp.beginPacket(ip, port);
    _udp.write(resp, 48);
    _udp.endPacket();
}
