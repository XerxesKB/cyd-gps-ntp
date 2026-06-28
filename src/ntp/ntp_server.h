#pragma once
#include <WiFiUdp.h>

class NtpServer {
public:
    void     begin();
    void     handle();
    uint32_t requestCount() const { return _requestCount; }

private:
    WiFiUDP  _udp;
    uint32_t _requestCount = 0;
    void     sendResponse(IPAddress ip, uint16_t port, const uint8_t* req);
};

extern NtpServer ntpServer;

// Dynamischer Stratum und Leap Indicator — implementiert in main.cpp
uint8_t ntp_current_stratum();
uint8_t ntp_current_li();
