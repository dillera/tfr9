/**
 * @brief   fnweather: numbers, units, words and dates
 * @license gpl v. 3
 * @verbose The values arrive as metric text ("12.3"); here they become
 *          tenths, in integers (CMOC has no floating point under OS-9),
 *          and text again in the units chosen.  The descriptions and the
 *          choice of icon are fujinet-weather's (coco/src/weather.c), the
 *          dates its strutil.c's.
 */

#include "weather.h"

#ifndef _CMOC_VERSION_
#include <time.h>
#endif

long tenths(const char *s)
{
    long v = 0;
    byte neg = 0, frac = 0, round = 0;

    while (*s == ' ') s++;
    if (*s == '-') {
        neg = 1;
        s++;
    }
    for (; *s; s++) {
        if (*s == '.') {
            if (frac) break;
            frac = 1;
            continue;
        }
        if (*s < '0' || *s > '9') break;
        if (frac < 2) {
            v = v * 10 + (*s - '0');
            if (frac) frac = 2;
        } else if (frac == 2) {
            round = *s >= '5';
            frac = 3;
        }
    }
    if (frac < 2) v *= 10;
    v += round;
    return neg ? -v : v;
}

static char *put_num(char *p, unsigned long u)
{
    char d[12];
    byte n = 0;
    do {
        d[n++] = (char)('0' + u % 10);
        u /= 10;
    } while (u);
    while (n) *p++ = d[--n];
    return p;
}

// v in tenths, with no decimals (rounded) or one; or, with decimals 2,
// v in hundredths.
void put_tenths(char *buf, long v, byte decimals)
{
    char *p = buf;
    unsigned long u = v < 0 ? (unsigned long)-v : (unsigned long)v;
    unsigned long scale = decimals == 2 ? 100 : 10;

    if (!decimals) u = (u + 5) / 10;
    if (v < 0 && u) *p++ = '-';
    if (!decimals) {
        p = put_num(p, u);
    } else {
        p = put_num(p, u / scale);
        *p++ = '.';
        if (decimals == 2 && u % 100 < 10) *p++ = '0';
        p = put_num(p, u % scale);
    }
    *p = 0;
}

void fmt_temp(char *buf, const char *c)
{
    long v = tenths(c);
    if (rep.imperial) v = v * 9 / 5 + 320;
    put_tenths(buf, v, 0);
}

// The color of a temperature, by degrees Celsius.
byte temp_color(const char *c)
{
    long v = tenths(c);
    if (v < -100) return TC_BRIGHT + TC_MAGENTA;
    if (v <= 0) return TC_BRIGHT + TC_CYAN;
    if (v < 100) return TC_CYAN;
    if (v < 180) return TC_BRIGHT + TC_GREEN;
    if (v < 250) return TC_BRIGHT + TC_YELLOW;
    if (v < 320) return TC_YELLOW;
    return TC_BRIGHT + TC_RED;
}

void fmt_speed(char *buf, const char *kmh)
{
    long v = tenths(kmh);
    if (rep.imperial) {
        put_tenths(buf, v * 621 / 1000, 0);
        strcat(buf, " mph");
    } else {
        put_tenths(buf, v, 0);
        strcat(buf, " km/h");
    }
}

void fmt_precip(char *buf, const char *mm)
{
    long v = tenths(mm);
    if (rep.imperial) {
        put_tenths(buf, (v * 100 + 127) / 254, 2);
        strcat(buf, " in");
    } else {
        put_tenths(buf, v, 1);
        strcat(buf, " mm");
    }
}

void fmt_vis(char *buf, const char *m)
{
    long metres = tenths(m) / 10, v;
    if (rep.imperial) {
        v = metres * 10 / 1609;
        put_tenths(buf, v, v < 100);
        strcat(buf, " mi");
    } else {
        v = metres / 100;
        put_tenths(buf, v, v < 100);
        strcat(buf, " km");
    }
}

void fmt_pressure(char *buf, const char *hpa)
{
    long v = tenths(hpa);
    if (rep.imperial) {
        put_tenths(buf, v * 2953 / 10000, 2);
        strcat(buf, " inHg");
    } else {
        put_tenths(buf, v, 0);
        strcat(buf, " hPa");
    }
}

const char *compass(const char *deg)
{
    static const char *const points[8] = { "N", "NE", "E", "SE", "S", "SW", "W", "NW" };
    long d = (tenths(deg) + 5) / 10;
    return points[(int) (((d % 360 + 382) % 360) / 45)];
}

const char *describe(byte code)
{
    switch (code) {
    case 0: return "Clear sky";
    case 1: return "Mainly clear";
    case 2: return "Partly cloudy";
    case 3: return "Cloudy";
    case 45: return "Fog";
    case 48: return "Depositing rime fog";
    case 51: return "Light drizzle";
    case 53: return "Drizzle";
    case 55: return "Dense drizzle";
    case 56: return "Light freezing drizzle";
    case 57: return "Freezing drizzle";
    case 61: return "Light rain";
    case 63: return "Rain";
    case 65: return "Heavy rain";
    case 66: return "Light freezing rain";
    case 67: return "Freezing rain";
    case 71: return "Light snow";
    case 73: return "Snow";
    case 75: return "Heavy snow";
    case 77: return "Snow grains";
    case 80: return "Light showers";
    case 81: return "Showers";
    case 82: return "Violent showers";
    case 85: return "Light snow showers";
    case 86: return "Snow showers";
    case 95: return "Thunderstorm";
    case 96: case 99: return "Thunderstorm, hail";
    }
    return "Unknown";
}

byte icon_of(byte code)
{
    switch (code) {
    case 0: return ICON_CLEAR;
    case 1: return ICON_FEW;
    case 2: return ICON_PARTLY;
    case 3: return ICON_CLOUDY;
    case 56: case 57: case 80: case 81: case 82: return ICON_SHOWERS;
    case 51: case 53: case 55: case 61: case 63: case 65: case 66: case 67:
        return ICON_RAIN;
    case 95: case 96: case 99: return ICON_STORM;
    case 71: case 73: case 75: case 77: case 85: case 86: return ICON_SNOW;
    case 45: case 48: return ICON_FOG;
    }
    return ICON_FEW;
}

// The UV index, in tenths, by the WHO's scale.
byte uv_color(long uv)
{
    uv = (uv + 5) / 10;
    if (uv <= 2) return TC_BRIGHT + TC_GREEN;
    if (uv <= 5) return TC_BRIGHT + TC_YELLOW;
    if (uv <= 7) return TC_YELLOW;
    if (uv <= 10) return TC_BRIGHT + TC_RED;
    return TC_BRIGHT + TC_MAGENTA;
}

const char *uv_word(long uv)
{
    uv = (uv + 5) / 10;
    if (uv <= 2) return "low";
    if (uv <= 5) return "moderate";
    if (uv <= 7) return "high";
    if (uv <= 10) return "very high";
    return "extreme";
}

static int digits(const char *s, byte n)
{
    int v = 0;
    while (n--) v = v * 10 + (*s++ - '0');
    return v;
}

// "YYYY-MM-DD": Sakamoto's method.
int day_of_week(const char *date)
{
    static const byte t[12] = { 0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4 };
    int y = digits(date, 4), m = digits(date + 5, 2), d = digits(date + 8, 2);
    if (strlen(date) < 10 || m < 1 || m > 12) return 0;
    if (m < 3) y--;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}

const char *day_name(const char *date, byte longform)
{
    static const char *const names[7] = {
        "Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"
    };
    static char shortname[4];
    const char *n = names[day_of_week(date)];
    if (longform) return n;
    memcpy(shortname, n, 3);
    shortname[3] = 0;
    return shortname;
}

const char *month_name(const char *date, byte longform)
{
    static const char *const names[12] = {
        "January", "February", "March", "April", "May", "June", "July",
        "August", "September", "October", "November", "December"
    };
    static char shortname[4];
    int m = strlen(date) >= 7 ? digits(date + 5, 2) : 0;
    const char *n = m >= 1 && m <= 12 ? names[m - 1] : "?";
    if (longform) return n;
    strncpy(shortname, n, 3);
    shortname[3] = 0;
    return shortname;
}

// Days from 1970-01-01 to a date (Howard Hinnant's days_from_civil).
static long days_from_civil(int y, int m, int d)
{
    long era, yoe, doy, doe;
    if (m <= 2) y--;
    era = (y >= 0 ? y : y - 399) / 400;
    yoe = y - era * 400;
    doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}

// The time now, in minutes, for how old the cache is.
dword now_minutes(void)
{
    int y, mo, d, h, mi;
#ifdef _CMOC_VERSION_
    byte t[6];
    asm {
        leax    :t
        os9     $15             ; F$Time: y m d h m s
    }
    y = 1900 + t[0];
    mo = t[1];
    d = t[2];
    h = t[3];
    mi = t[4];
#else
    time_t now = time(0);
    struct tm *tm = localtime(&now);
    y = tm->tm_year + 1900;
    mo = tm->tm_mon + 1;
    d = tm->tm_mday;
    h = tm->tm_hour;
    mi = tm->tm_min;
#endif
    return (dword)days_from_civil(y, mo, d) * 1440 + h * 60 + mi;
}

// Whether the time of the report is before today's sunrise or after its
// sunset (all "YYYY-MM-DDTHH:MM", local to the place).
byte is_night(void)
{
    const char *t = rep.now.time + 11;
    if (strlen(rep.now.time) < 16 || strlen(rep.day[0].sunrise) < 16) return 0;
    return strncmp(t, rep.day[0].sunrise + 11, 5) < 0
        || strncmp(t, rep.day[0].sunset + 11, 5) >= 0;
}
