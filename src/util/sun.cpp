#include "sun.h"
#include <math.h>
#include <time.h>

static double toRad(double d) { return d * M_PI / 180.0; }
static double toDeg(double r) { return r * 180.0 / M_PI; }

// USNO simplified sunrise/sunset algorithm (accuracy ~2 min)
// Returns UTC fractional hours, or -1 = never rises, -2 = never sets
static double sunEvent(float lat, float lon, int doy, bool sunset) {
    double lngHour = lon / 15.0;
    double t = doy + ((sunset ? 18.0 : 6.0) - lngHour) / 24.0;

    double M = 0.9856 * t - 3.289;
    double L = M + 1.916 * sin(toRad(M)) + 0.020 * sin(toRad(2.0 * M)) + 282.634;
    while (L >= 360.0) L -= 360.0;
    while (L <    0.0) L += 360.0;

    double RA = toDeg(atan(0.91764 * tan(toRad(L))));
    while (RA >= 360.0) RA -= 360.0;
    while (RA <    0.0) RA += 360.0;
    RA += floor(L / 90.0) * 90.0 - floor(RA / 90.0) * 90.0;
    RA /= 15.0;

    double sinDec = 0.39782 * sin(toRad(L));
    double cosDec = cos(asin(sinDec));

    // zenith = 90.833° accounts for atmospheric refraction + solar disc radius
    double cosH = (cos(toRad(90.833)) - sinDec * sin(toRad((double)lat)))
                / (cosDec * cos(toRad((double)lat)));
    if (cosH >  1.0) return -1.0;  // sun never rises
    if (cosH < -1.0) return -2.0;  // sun never sets

    double H = sunset ? toDeg(acos(cosH)) : 360.0 - toDeg(acos(cosH));
    H /= 15.0;

    double T  = H + RA - 0.06571 * t - 6.622;
    double UT = T - lngHour;
    while (UT >= 24.0) UT -= 24.0;
    while (UT <   0.0) UT += 24.0;
    return UT;
}

static int calcDoy(int year, int month, int day) {
    int N1 = 275 * month / 9;
    int N2 = (month + 9) / 12;
    int N3 = 1 + (year - 4 * (year / 4) + 2) / 3;
    return N1 - N2 * N3 + day - 30;
}

bool sun_calc(float lat, float lon, int year, int month, int day,
              int* riseH, int* riseM, int* setH, int* setM) {
    if (lat == 0.0f && lon == 0.0f) return false;

    int doy = calcDoy(year, month, day);
    double riseUTC = sunEvent(lat, lon, doy, false);
    double setUTC  = sunEvent(lat, lon, doy, true);
    if (riseUTC < 0 || setUTC < 0) return false;

    // Derive current UTC offset in minutes — handles DST automatically
    time_t now = time(nullptr);
    struct tm utcTm, locTm;
    gmtime_r(&now,   &utcTm);
    localtime_r(&now, &locTm);
    int offsetMin = (locTm.tm_hour * 60 + locTm.tm_min)
                  - (utcTm.tm_hour * 60 + utcTm.tm_min);
    if (offsetMin < -720) offsetMin += 1440;
    if (offsetMin >  720) offsetMin -= 1440;

    int riseMin = (int)(riseUTC * 60.0 + 0.5) + offsetMin;
    int setMin  = (int)(setUTC  * 60.0 + 0.5) + offsetMin;
    riseMin = ((riseMin % 1440) + 1440) % 1440;
    setMin  = ((setMin  % 1440) + 1440) % 1440;

    *riseH = riseMin / 60;  *riseM = riseMin % 60;
    *setH  = setMin  / 60;  *setM  = setMin  % 60;
    return true;
}
