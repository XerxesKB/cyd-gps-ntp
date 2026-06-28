#include "led.h"
#include "../config/config.h"
#include <Arduino.h>

// Common-Anode RGB: HIGH = aus, LOW = an
void led_init() {
    pinMode(LED_RED_PIN,   OUTPUT);
    pinMode(LED_GREEN_PIN, OUTPUT);
    pinMode(LED_BLUE_PIN,  OUTPUT);
    led_off();
}

void led_off() {
    digitalWrite(LED_RED_PIN,   HIGH);
    digitalWrite(LED_GREEN_PIN, HIGH);
    digitalWrite(LED_BLUE_PIN,  HIGH);
}

void led_set(bool r, bool g, bool b) {
    digitalWrite(LED_RED_PIN,   r ? LOW : HIGH);
    digitalWrite(LED_GREEN_PIN, g ? LOW : HIGH);
    digitalWrite(LED_BLUE_PIN,  b ? LOW : HIGH);
}
