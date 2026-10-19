/**
 * @brief   fnweather: the weather where you are, through the FujiNet
 * @license gpl v. 3
 * @verbose After FujiNetWIFI/fujinet-weather's CoCo client (Shunichi
 *          Kitahara, Thomas Cherryhomes): its data, from ip-api.com and
 *          Open-Meteo, drawn on the tether's terminal with term.c.
 *          Everything is fetched in metric units and kept as the text
 *          the JSON has; units.c converts it for the screen.
 */

#ifndef WEATHER_H
#define WEATHER_H

#include "term.h"

#define VAL 12              // a number, as text
#define STAMP 20            // "2026-10-18T17:15"

typedef struct {
    char lat[VAL], lon[VAL];
    char city[40], region[40], country[4];
} Location;

typedef struct {
    char time[STAMP], tz[VAL];
    char temp[VAL], feels[VAL], humidity[VAL], code[VAL], clouds[VAL];
    char pressure[VAL], wind[VAL], wind_dir[VAL], dew[VAL], vis[VAL];
} Current;

typedef struct {
    char date[VAL], code[VAL], tmax[VAL], tmin[VAL];
    char sunrise[STAMP], sunset[STAMP];
    char precip[VAL], chance[VAL], uv[VAL], wind[VAL], wind_dir[VAL];
} Day;

#define DAYS 8

// What fnweather knows, and what it keeps in its cache file.
typedef struct {
    char magic[6];          // CACHE_MAGIC
    byte imperial;          // the units to show
    byte have_weather;      // now and day[] are filled
    dword fetched;          // when, in minutes (now_minutes())
    Location loc;
    Current now;
    Day day[DAYS];
} Report;

#define CACHE_MAGIC "FNWX1"
#define STALE_MINUTES 30

extern Report rep;

// fetch.c
byte locate_here(Location *loc);                    // from ip-api.com
byte locate_city(Location *loc, const char *city);  // Open-Meteo's geocoding
byte fetch_weather(void);                           // into rep.now, rep.day
extern char fetch_error[40];

// cache.c
byte cache_load(void);
void cache_save(void);
extern const char cache_path[];

// units.c
long tenths(const char *s);         // "12.3" -> 123, "-4" -> -40
void put_tenths(char *buf, long v, byte decimals);
void fmt_temp(char *buf, const char *c);        // "12.3" (C) -> "54" or "12"
byte temp_color(const char *c);
void fmt_speed(char *buf, const char *kmh);     // "12 mph" or "19 km/h"
void fmt_precip(char *buf, const char *mm);     // "0.12 in" or "3.1 mm"
void fmt_vis(char *buf, const char *m);
void fmt_pressure(char *buf, const char *hpa);
const char *compass(const char *deg);
const char *describe(byte code);
byte icon_of(byte code);
byte uv_color(long uv);
const char *uv_word(long uv);
int day_of_week(const char *date);              // 0 = Sunday
const char *day_name(const char *date, byte longform);
const char *month_name(const char *date, byte longform);
dword now_minutes(void);
byte is_night(void);

// icons.c
#define ICON_CLEAR   0
#define ICON_FEW     1
#define ICON_PARTLY  2
#define ICON_CLOUDY  3
#define ICON_SHOWERS 4
#define ICON_RAIN    5
#define ICON_STORM   6
#define ICON_SNOW    7
#define ICON_FOG     8
#define ICON_MOON    9
void draw_icon(byte x, byte y, byte icon, byte big);
void draw_big_text(byte x, byte y, const char *s, byte fg);  // 3 rows tall
byte big_text_width(const char *s);

// screen.c
void show_weather(void);
void show_forecast(byte page);
void show_about(void);
void show_status(const char *msg, byte fg);
void show_fetched(void);
void show_keys(byte forecast);
byte ask_city(char *buf, byte size);

#endif // WEATHER_H
