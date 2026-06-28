#include "display_manager.h"
#include "display_main.h"
#include "display_detail.h"
#include "../config/config.h"
#include <Arduino.h>
#include <SPI.h>
#include <XPT2046_Touchscreen.h>

TFT_eSPI tft = TFT_eSPI();
#define TOUCH_IRQ  36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25
#define TOUCH_CS   33

SPIClass touchSPI = SPIClass(VSPI);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);

static DisplayPage   currentPage       = DisplayPage::MAIN;
static bool          pageNeedsRedraw   = true;
static unsigned long detailEnterMs     = 0;

// Touch-Entprellzeit
static unsigned long lastTouchMs = 0;
static const uint32_t TOUCH_DEBOUNCE = 800;

DisplayPage display_current_page() { return currentPage; }

void display_switch_page(DisplayPage p) {
    currentPage     = p;
    pageNeedsRedraw = true;
    if (p == DisplayPage::DETAIL) detailEnterMs = millis();
}

static void handle_touch() {
    if (!touch.touched()) return;

    TS_Point p = touch.getPoint();
    if (p.z < TOUCH_THRESHOLD) return;

    unsigned long now = millis();
    if (now - lastTouchMs < TOUCH_DEBOUNCE) return;
    lastTouchMs = now;

    if (currentPage == DisplayPage::MAIN) {
        display_switch_page(DisplayPage::DETAIL);
    } else {
        display_switch_page(DisplayPage::MAIN);
    }
}

void display_init() {
    // Backlight erst NACH Init & Bildschirm-Clear einschalten → kein Initialisierungsgemüll sichtbar
    pinMode(TFT_BL_PIN, OUTPUT);
    digitalWrite(TFT_BL_PIN, LOW);

    tft.init();
    touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
    touch.begin(touchSPI);
    touch.setRotation(2);
    tft.setRotation(TFT_ROTATION);
    tft.fillScreen(TFT_BLACK);
    Serial.printf("Display: %d x %d\n", tft.width(), tft.height());
    digitalWrite(TFT_BL_PIN, HIGH); // Jetzt erst Hintergrundbeleuchtung an

    // Splash – Mitte dynamisch anhand tatsächlicher Displaybreite
    int16_t cx = tft.width() / 2;
    tft.setTextColor(TFT_ORANGE, TFT_BLACK);
    tft.setTextDatum(MC_DATUM);
    tft.setFreeFont(nullptr);
    tft.setTextSize(2);
    tft.drawString("CYD GPS NTP Server", cx, 100);
    tft.setTextSize(1);
    tft.drawString("v" FW_VERSION, cx, 130);
    tft.drawString("Initializing...", cx, 155);

    // Touch läuft mit Library-Defaults (x0=300..3600, y0=300..3600).
    // Kalibrierung via tft.calibrateTouch() kann später interaktiv ausgelöst werden.
}

void display_update() {
    handle_touch();

    // Auto-Return von Detail nach DISPLAY_TIMEOUT_DETAIL ms
    if (currentPage == DisplayPage::DETAIL) {
        if (millis() - detailEnterMs >= DISPLAY_TIMEOUT_DETAIL) {
            display_switch_page(DisplayPage::MAIN);
        }
    }

    if (currentPage == DisplayPage::MAIN) {
        display_main_draw(pageNeedsRedraw);
    } else {
        display_detail_draw(pageNeedsRedraw);
    }
    pageNeedsRedraw = false;
}
