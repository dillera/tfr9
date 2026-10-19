/**
 * @brief   fnweather: the weather where you are, through the FujiNet
 * @license gpl v. 3
 * @verbose What was fetched last is kept in a cache (cache.c) and shown
 *          at once; it is fetched again when it is half an hour old, at
 *          the start or while fnweather is showing it, or with R.
 */

#include "weather.h"

Report rep;

static byte forecast_page;      // 0 or 1, while the forecast is shown
static byte showing_forecast;
static dword next_try;          // after a failed fetch, when to try again

static void redraw(void)
{
    if (showing_forecast) show_forecast(forecast_page);
    else show_weather();
}

static void auto_units(void)
{
    const char *c = rep.loc.country;
    rep.imperial = !strcmp(c, "US") || !strcmp(c, "LR") || !strcmp(c, "MM");
}

static byte refresh(void)
{
    if (!fetch_weather()) {
        next_try = now_minutes() + 5;
        show_status(fetch_error, TC_BRIGHT + TC_RED);
        return 0;
    }
    rep.fetched = now_minutes();
    cache_save();
    redraw();
    return 1;
}

// A first screen, while there is nothing to show yet.
static void waiting(const char *msg)
{
    term_clrscr();
    draw_icon(33, 6, ICON_PARTLY, 1);
    show_status(msg, TC_BRIGHT + TC_YELLOW);
}

static byte relocate(const char *city)
{
    Location old;
    byte ok;

    memcpy(&old, &rep.loc, sizeof old);
    show_status(city ? "Looking the place up" : "Finding where you are",
                TC_BRIGHT + TC_YELLOW);
    ok = city ? locate_city(&rep.loc, city) : locate_here(&rep.loc);
    if (!ok) {
        memcpy(&rep.loc, &old, sizeof old);
        show_status(fetch_error, TC_BRIGHT + TC_RED);
        return 0;
    }
    auto_units();
    return 1;
}

int main(void)
{
    byte c, minute = 255;
    dword now;
    char city[40];
    Location old;
    byte old_units;

    term_init();
    if (!cache_load() || !rep.loc.lat[0]) {
        memset(&rep, 0, sizeof rep);
        waiting("Finding where you are");
        if (!relocate((const char *) 0)) {
            term_getc();
            term_done();
            return 1;
        }
        cache_save();
    }

    if (rep.have_weather) show_weather();
    else waiting("Fetching the weather");
    if (!rep.have_weather || now_minutes() - rep.fetched >= STALE_MINUTES)
        refresh();
    if (!rep.have_weather) {
        term_getc();
        term_done();
        return 1;
    }

    for (;;) {
        if (!term_ready()) {
            // Fetch again when half an hour old; say how old once a minute.
            term_sleep(30);
            now = now_minutes();
            if (now - rep.fetched >= STALE_MINUTES && now >= next_try) {
                refresh();
            } else if (!showing_forecast && (byte)(now % 60) != minute) {
                minute = (byte)(now % 60);
                show_fetched();
            }
            continue;
        }
        c = term_getc();
        if (c >= 'a' && c <= 'z') c -= 32;
        if (c == 'Q') break;
        if (c == 'U') {
            rep.imperial = !rep.imperial;
            cache_save();
            redraw();
            continue;
        }
        if (showing_forecast) {
            if (c == 'N' || c == KEY_RIGHT || c == KEY_DOWN || c == ' ') {
                forecast_page = 1;
                redraw();
            } else if (c == 'B' || c == KEY_LEFT || c == KEY_UP) {
                forecast_page = 0;
                redraw();
            } else if (c == 'W' || c == 'F' || c == KEY_BREAK || c == KEY_ENTER) {
                showing_forecast = 0;
                redraw();
            }
            continue;
        }
        switch (c) {
        case KEY_BREAK:
            goto done;
        case 'R':
            refresh();
            break;
        case 'F':
            showing_forecast = 1;
            forecast_page = 0;
            redraw();
            break;
        case 'L':
            // A new place; if its weather cannot be had, the old one again.
            memcpy(&old, &rep.loc, sizeof old);
            old_units = rep.imperial;
            if (relocate(ask_city(city, sizeof city) ? city : (const char *) 0) && !refresh()) {
                memcpy(&rep.loc, &old, sizeof old);
                rep.imperial = old_units;
                show_weather();
                show_status(fetch_error, TC_BRIGHT + TC_RED);
            }
            break;
        case 'I':
            show_about();
            term_getc();
            show_weather();
            break;
        }
    }
done:
    term_done();
    return 0;
}
