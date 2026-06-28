#pragma once
#include <stdint.h>

void led_init();                              // Pins als OUTPUT, LED aus
void led_off();
void led_set(bool r, bool g, bool b);         // true = an
