#pragma once
#include <TFT_eSPI.h>

extern TFT_eSPI tft;

enum class DisplayPage { MAIN, DETAIL };

void display_init();
void display_update();

DisplayPage display_current_page();
void display_switch_page(DisplayPage p);
