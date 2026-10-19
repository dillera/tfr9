/**
 * @brief   fnweather: fetching the location and the weather
 * @license gpl v. 3
 * @verbose After fujinet-weather's coco/src/ipapi.c and openmeteo.c.
 *          Three Open-Meteo requests, not the CoCo's seven: a device
 *          spec is at most 255 characters, and these are the fewest that
 *          carry the conditions now and eight days' forecast within it.
 *          Units are left metric (units.c converts).
 */

#include "weather.h"
#include "fujinet-network.h"

char fetch_error[40];

static char url[256];
static char scratch[128];   // a value as the FujiNet sends it
static char path[48];

static const char ip_url[] =
    "N:http://ip-api.com/json/?fields=status,city,regionName,countryCode,lon,lat";
static const char om_head[] = "N:https://api.open-meteo.com/v1/forecast?latitude=";
static const char *const om_tail[3] = {
    "&timezone=auto&current=temperature_2m,apparent_temperature,"
    "relative_humidity_2m,weather_code,cloud_cover,surface_pressure,wind_speed_10m",
    "&timezone=auto&current=wind_direction_10m,dew_point_2m,visibility"
    "&forecast_days=8&daily=weather_code,temperature_2m_max,temperature_2m_min,"
    "sunrise,sunset",
    "&timezone=auto&forecast_days=8&daily=precipitation_sum,"
    "precipitation_probability_max,uv_index_max,wind_speed_10m_max,"
    "wind_direction_10m_dominant"
};

static void query(const char *q, char *out, byte size)
{
    int16_t n = network_json_query(url, q, scratch);
    if (n < 0) n = 0;
    scratch[n] = 0;
    strncpy(out, scratch, size - 1);
    out[size - 1] = 0;
}

// One element of a daily array: /daily/<field>/<i>.
static void query_day(const char *field, byte i, char *out, byte size)
{
    byte n;
    strcpy(path, "/daily/");
    strcat(path, field);
    n = (byte)strlen(path);
    path[n] = '/';
    path[n + 1] = (char)('0' + i);
    path[n + 2] = 0;
    query(path, out, size);
}

static byte failed(const char *what)
{
    strcpy(fetch_error, what);
    network_close(url);
    return 0;
}

static byte open_parse(void)
{
    if (network_open(url, OPEN_MODE_READ, OPEN_TRANS_NONE) != FN_ERR_OK)
        return failed("Could not reach the server");
    if (network_json_parse(url) != FN_ERR_OK)
        return failed("The server's reply was not JSON");
    return 1;
}

byte locate_here(Location *loc)
{
    strcpy(url, ip_url);
    if (!open_parse()) return 0;
    query("/status", scratch, sizeof scratch);
    if (strcmp(scratch, "success")) return failed("ip-api.com could not place you");
    query("/city", loc->city, sizeof loc->city);
    query("/regionName", loc->region, sizeof loc->region);
    query("/countryCode", loc->country, sizeof loc->country);
    query("/lat", loc->lat, sizeof loc->lat);
    query("/lon", loc->lon, sizeof loc->lon);
    network_close(url);
    return 1;
}

byte locate_city(Location *loc, const char *city)
{
    char *p;
    Location found;

    strcpy(url, "N:https://geocoding-api.open-meteo.com/v1/search?name=");
    p = url + strlen(url);
    for (; *city && p < url + 160; city++) {
        if (*city == ' ') {
            *p++ = '%';
            *p++ = '2';
            *p++ = '0';
        } else {
            *p++ = *city;
        }
    }
    strcpy(p, "&count=1&language=en&format=json");
    if (!open_parse()) return 0;
    query("/results/0/name", found.city, sizeof found.city);
    if (!found.city[0]) return failed("No place of that name");
    query("/results/0/admin1", found.region, sizeof found.region);
    query("/results/0/country_code", found.country, sizeof found.country);
    query("/results/0/latitude", found.lat, sizeof found.lat);
    query("/results/0/longitude", found.lon, sizeof found.lon);
    network_close(url);
    if (!strcmp(found.region, found.city)) found.region[0] = 0;
    memcpy(loc, &found, sizeof found);
    return 1;
}

static void om_url(byte which)
{
    strcpy(url, om_head);
    strcat(url, rep.loc.lat);
    strcat(url, "&longitude=");
    strcat(url, rep.loc.lon);
    strcat(url, om_tail[which]);
}

// Into rep.now and rep.day; a failure leaves what was there.  Each of
// the three requests is shown on the status line.
byte fetch_weather(void)
{
    static Current now;
    static Day day[DAYS];
    byte i;
    Day *d;

    show_status("Fetching the weather: now", TC_BRIGHT + TC_YELLOW);
    om_url(0);
    if (!open_parse()) return 0;
    query("/current/time", now.time, sizeof now.time);
    query("/timezone_abbreviation", now.tz, sizeof now.tz);
    query("/current/temperature_2m", now.temp, VAL);
    query("/current/apparent_temperature", now.feels, VAL);
    query("/current/relative_humidity_2m", now.humidity, VAL);
    query("/current/weather_code", now.code, VAL);
    query("/current/cloud_cover", now.clouds, VAL);
    query("/current/surface_pressure", now.pressure, VAL);
    query("/current/wind_speed_10m", now.wind, VAL);
    network_close(url);
    if (!now.time[0]) return failed("No weather in the reply");

    show_status("Fetching the weather: the week", TC_BRIGHT + TC_YELLOW);
    om_url(1);
    if (!open_parse()) return 0;
    query("/current/wind_direction_10m", now.wind_dir, VAL);
    query("/current/dew_point_2m", now.dew, VAL);
    query("/current/visibility", now.vis, VAL);
    for (i = 0; i < DAYS; i++) {
        d = &day[i];
        query_day("time", i, d->date, VAL);
        query_day("weather_code", i, d->code, VAL);
        query_day("temperature_2m_max", i, d->tmax, VAL);
        query_day("temperature_2m_min", i, d->tmin, VAL);
        query_day("sunrise", i, d->sunrise, STAMP);
        query_day("sunset", i, d->sunset, STAMP);
    }
    network_close(url);

    show_status("Fetching the weather: rain, sun and wind", TC_BRIGHT + TC_YELLOW);
    om_url(2);
    if (!open_parse()) return 0;
    for (i = 0; i < DAYS; i++) {
        d = &day[i];
        query_day("precipitation_sum", i, d->precip, VAL);
        query_day("precipitation_probability_max", i, d->chance, VAL);
        query_day("uv_index_max", i, d->uv, VAL);
        query_day("wind_speed_10m_max", i, d->wind, VAL);
        query_day("wind_direction_10m_dominant", i, d->wind_dir, VAL);
    }
    network_close(url);

    memcpy(&rep.now, &now, sizeof now);
    memcpy(rep.day, day, sizeof day);
    rep.have_weather = 1;
    return 1;
}
