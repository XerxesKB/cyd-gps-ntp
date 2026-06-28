#pragma once

void ntp_client_begin();    // nach WiFi-Connect aufrufen
void ntp_client_stop();     // aufrufen wenn GPS+PPS gesperrt
bool ntp_client_synced();   // true sobald erste NTP-Antwort verarbeitet
