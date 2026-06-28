#include "pps_handler.h"
#include "../config/config.h"
#include "esp_timer.h"

volatile uint64_t ppsMicros     = 0;
volatile uint32_t ppsPulseCount = 0;
static volatile bool ppsFlag    = false;

void IRAM_ATTR pps_isr() {
    ppsMicros = (uint64_t)esp_timer_get_time();
    ppsPulseCount++;
    ppsFlag = true;
}

void pps_init() {
    pinMode(PPS_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(PPS_PIN), pps_isr, PPS_INTERRUPT);
}

bool pps_new_pulse() {
    if (!ppsFlag) return false;
    ppsFlag = false;
    return true;
}
