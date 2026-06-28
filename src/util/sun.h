#pragma once

// Calculates local sunrise and sunset for the given coordinates and date.
// Times are in local time (respects the active TZ / DST setting).
// Returns false on polar day/night or if lat==0 && lon==0.
bool sun_calc(float lat, float lon, int year, int month, int day,
              int* riseH, int* riseM, int* setH, int* setM);
