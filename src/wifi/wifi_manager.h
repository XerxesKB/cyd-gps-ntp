#pragma once

void        wifi_init();
bool        wifi_connected();
const char* wifi_ip_str();   // Stabiler Anzeigestring – kein live WiFi.getMode()
