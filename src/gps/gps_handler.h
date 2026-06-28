#pragma once
#include <TinyGPSPlus.h>

struct GpsData {
    bool     valid;
    bool     fixAcquired;
    int      hour, minute, second, centisecond;
    int      day, month, year;
    double   lat, lon;
    int      satellites;
    double   hdop;
    int      fixType;           // 0=none, 1=2D, 2=3D
    unsigned long lastUpdateMs;
};

extern GpsData    gpsData;
extern TinyGPSPlus gps;
extern uint32_t   gpsRawBytes;     // Gesamtzahl empfangener Bytes vom GPS-UART
extern uint32_t   gpsBaudCurrent;  // Erkannte/verwendete Baudrate (0 = kein Signal)

void     gps_init();
void     gps_update();
uint32_t gps_to_unix();    // UTC Unix-Timestamp aus gpsData
