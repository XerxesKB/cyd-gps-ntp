#pragma once
#include <Arduino.h>

extern volatile uint64_t ppsMicros;       // esp_timer_get_time() beim letzten PPS-Puls
extern volatile uint32_t ppsPulseCount;

void pps_init();

// Gibt true zurück und resetet den Flag, wenn seit dem letzten Aufruf ein neuer Puls kam
bool pps_new_pulse();
