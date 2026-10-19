/**
 * @brief   fnweather: the screens
 * @license gpl v. 3
 * @verbose Laid out for the tether's 80 by 24 terminal.  The weather now,
 *          with the week below it, eight days in columns; the forecast,
 *          four days to a page; and what fnweather is.
 */

#include "weather.h"

#define WHITE   (TC_BRIGHT + TC_WHITE)
#define PANEL_X 52
#define ROW_WEEK 10
#define ROW_STATUS 21
#define ROW_KEYS 23

static char buf[64], val[24];

static void color(byte fg)
{
    term_color(fg, TC_DEFAULT);
}

static void put(byte fg, const char *s)
{
    color(fg);
    term_puts(s);
}

static void put_at(byte x, byte y, byte fg, const char *s)
{
    term_gotoxy(x, y);
    put(fg, s);
}

// s centered in the w columns from x.
static void center(byte x, byte w, byte y, byte fg, const char *s)
{
    byte n = (byte)strlen(s);
    put_at(n < w ? x + (w - n) / 2 : x, y, fg, s);
}

static void degree(void)
{
    term_putc((char) TG_DEGREE);
}

// A temperature, in its color, with a degree sign.
static void put_temp(const char *c)
{
    fmt_temp(val, c);
    put(temp_color(c), val);
    degree();
}

// A bar across the top: white on blue, text at the left and the right.
static void bar(byte y, const char *left, const char *right)
{
    byte x;
    term_gotoxy(0, y);
    term_color(WHITE, TC_BLUE);
    term_putc(' ');
    term_puts(left);
    for (x = (byte)strlen(left) + 1; x < TERM_COLS - 1 - strlen(right); x++)
        term_putc(' ');
    term_puts(right);
    term_putc(' ');
    color(TC_DEFAULT);
}

// A rule, with a label near its left end.
static void rule(byte y, const char *label)
{
    byte x;
    term_gotoxy(0, y);
    color(TC_BLUE);
    term_putc((char) TG_HLINE);
    term_putc((char) TG_HLINE);
    put(TC_BRIGHT + TC_CYAN, label);
    color(TC_BLUE);
    for (x = (byte)strlen(label) + 2; x < TERM_COLS; x++)
        term_putc((char) TG_HLINE);
    color(TC_DEFAULT);
}

// A percentage as a bar of ten cells.
static void meter(const char *pct, byte fg)
{
    long v = (tenths(pct) + 50) / 100;
    byte i;
    for (i = 0; i < 10; i++) {
        color(i < v ? fg : TC_BLUE);
        term_putc((char) (i < v ? TG_FULL : TG_LOWERHALF));
    }
    color(TC_DEFAULT);
}

static void place(char *out)
{
    strcpy(out, rep.loc.city);
    if (rep.loc.region[0]) {
        strcat(out, ", ");
        strcat(out, rep.loc.region);
    }
    if (rep.loc.country[0]) {
        strcat(out, ", ");
        strcat(out, rep.loc.country);
    }
    out[50] = 0;
}

// The color of the words for a kind of weather.
static byte sky_color(byte icon)
{
    switch (icon) {
    case ICON_CLEAR: return TC_BRIGHT + TC_YELLOW;
    case ICON_SHOWERS: case ICON_RAIN: return TC_BRIGHT + TC_BLUE;
    case ICON_STORM: return TC_BRIGHT + TC_MAGENTA;
    case ICON_SNOW: return TC_BRIGHT + TC_CYAN;
    case ICON_FOG: return TC_WHITE;
    case ICON_MOON: return TC_BRIGHT + TC_WHITE;
    }
    return WHITE;
}

static void label(byte y, const char *s)
{
    put_at(PANEL_X, y, TC_CYAN, s);
    term_gotoxy(PANEL_X + 12, y);
}

static void hhmm(char *out, const char *stamp)
{
    strncpy(out, strlen(stamp) >= 16 ? stamp + 11 : "--:--", 5);
    out[5] = 0;
}

void show_status(const char *msg, byte fg)
{
    term_gotoxy(0, ROW_STATUS);
    term_clreol();
    put_at(1, ROW_STATUS, fg, msg);
    color(TC_DEFAULT);
    term_flush();
}

// The time of the weather shown (Open-Meteo's, the start of its quarter
// hour, in the place's time), and how long ago it was fetched.
void show_fetched(void)
{
    dword age = now_minutes() - rep.fetched;
    const char *unit = " minute";
    char *p;

    strcpy(buf, "As of ");
    hhmm(buf + strlen(buf), rep.now.time);
    strcat(buf, " ");
    strcat(buf, rep.now.tz);
    strcat(buf, ", fetched ");
    p = buf + strlen(buf);
    if (age < 1) {
        strcpy(p, "just now");
    } else {
        if (age >= 2 * 24 * 60) {
            age /= 24 * 60;
            unit = " day";
        } else if (age >= 2 * 60) {
            age /= 60;
            unit = " hour";
        }
        put_tenths(p, (long) age * 10, 0);
        strcat(p, unit);
        strcat(p, age == 1 ? " ago" : "s ago");
    }
    show_status(buf, TC_CYAN);
    put_at(TERM_COLS - 23, ROW_STATUS, TC_BLUE, "Open-Meteo, ip-api.com");
    color(TC_DEFAULT);
}

void show_keys(byte forecast)
{
    static const char *const weather_keys[] = {
        "R", "Refresh", "F", "Forecast", "U", "Units", "L", "Place", "I", "Info", "Q", "Quit", 0
    };
    static const char *const forecast_keys[] = {
        "N", "Next", "B", "Back", "W", "Weather", "U", "Units", "Q", "Quit", 0
    };
    const char *const *k = forecast ? forecast_keys : weather_keys;

    term_gotoxy(0, ROW_KEYS);
    term_clreol();
    term_gotoxy(1, ROW_KEYS);
    for (; *k; k += 2) {
        term_color(TC_BLACK, TC_CYAN);
        term_putc(' ');
        term_puts(k[0]);
        term_putc(' ');
        put(WHITE, " ");
        term_puts(k[1]);
        term_puts("   ");
    }
    color(TC_DEFAULT);
    term_flush();
}

static void week(void)
{
    byte i, x;
    Day *d;
    long chance, amount;

    rule(ROW_WEEK, " The week ");
    for (i = 0; i < DAYS; i++) {
        d = &rep.day[i];
        x = i * 10;
        center(x, 10, 11, WHITE, i ? day_name(d->date, 0) : "Today");
        strcpy(buf, d->date + 8);
        strcat(buf, " ");
        strcat(buf, month_name(d->date, 0));
        center(x, 10, 12, TC_CYAN, buf);
        draw_icon(x + 1, 13, icon_of((byte) atoi(d->code)), 0);

        term_gotoxy(x + 2, 17);
        put_temp(d->tmax);
        put(TC_WHITE, " ");
        put_temp(d->tmin);

        // the chance of rain, and how much, if any
        chance = (tenths(d->chance) + 5) / 10;
        amount = tenths(d->precip);
        if (chance) {
            put_tenths(buf, chance * 10, 0);
            strcat(buf, "% rain");
            center(x, 10, 18, TC_BRIGHT + TC_BLUE, buf);
        } else if (!amount) {
            center(x, 10, 18, TC_WHITE, "dry");
        }
        if (amount) {
            fmt_precip(buf, d->precip);
            center(x, 10, 19, TC_BLUE, buf);
        }
    }
    color(TC_BLUE);
    term_gotoxy(0, 20);
    for (x = 0; x < TERM_COLS; x++)
        term_putc((char) TG_HLINE);
    color(TC_DEFAULT);
}

void show_weather(void)
{
    byte icon, code = (byte) atoi(rep.now.code);
    Day *d0 = &rep.day[0];
    char left[52], *p;

    term_clrscr();
    place(left);
    strcpy(buf, day_name(rep.now.time, 1));     // "Sunday 18 Oct"
    p = buf + strlen(buf);
    *p++ = ' ';
    if (rep.now.time[8] != '0') *p++ = rep.now.time[8];
    *p++ = rep.now.time[9];
    *p++ = ' ';
    *p = 0;
    strcat(buf, month_name(rep.now.time, 0));
    strcat(buf, "  ");
    hhmm(buf + strlen(buf), rep.now.time);
    strcat(buf, " ");
    strcat(buf, rep.now.tz);
    bar(0, left, buf);

    icon = icon_of(code);
    if (is_night() && (icon == ICON_CLEAR || icon == ICON_FEW)) icon = ICON_MOON;
    draw_icon(2, 2, icon, 1);

    fmt_temp(val, rep.now.temp);
    strcpy(buf, val);
    strcat(buf, rep.imperial ? "*F" : "*C");
    draw_big_text(19, 2, buf, temp_color(rep.now.temp));

    strncpy(buf, describe(code), 30);
    buf[30] = 0;
    put_at(19, 6, sky_color(icon), buf);
    put_at(19, 7, TC_WHITE, "Feels like ");
    put_temp(rep.now.feels);
    put_at(19, 8, TC_WHITE, "High ");
    put_temp(d0->tmax);
    put(TC_WHITE, "  Low ");
    put_temp(d0->tmin);

    label(2, "Humidity");
    strcpy(buf, rep.now.humidity);
    strcat(buf, "%");
    put(WHITE, buf);
    term_gotoxy(PANEL_X + 17, 2);
    meter(rep.now.humidity, TC_BRIGHT + TC_CYAN);

    label(3, "Clouds");
    strcpy(buf, rep.now.clouds);
    strcat(buf, "%");
    put(WHITE, buf);
    term_gotoxy(PANEL_X + 17, 3);
    meter(rep.now.clouds, TC_WHITE);

    label(4, "Dew point");
    put_temp(rep.now.dew);

    label(5, "Pressure");
    fmt_pressure(buf, rep.now.pressure);
    put(WHITE, buf);

    label(6, "Visibility");
    fmt_vis(buf, rep.now.vis);
    put(WHITE, buf);

    label(7, "Wind");
    fmt_speed(buf, rep.now.wind);
    strcat(buf, " from ");
    strcat(buf, compass(rep.now.wind_dir));
    put(WHITE, buf);

    label(8, "UV today");
    put_tenths(buf, tenths(d0->uv), 0);
    put(uv_color(tenths(d0->uv)), buf);
    put(uv_color(tenths(d0->uv)), " ");
    term_puts(uv_word(tenths(d0->uv)));

    label(9, "Sun");
    hhmm(buf, d0->sunrise);
    put(TC_BRIGHT + TC_YELLOW, buf);
    put(TC_WHITE, " to ");
    hhmm(buf, d0->sunset);
    put(TC_YELLOW, buf);

    week();
    show_fetched();
    show_keys(0);
}

// The words for a day's weather, in lines of up to 18, centered.
static void wrapped(byte x, byte y, const char *s, byte fg)
{
    char line[20];
    const char *sp;
    byte n, row = 0;

    while (*s && row < 2) {
        n = (byte)strlen(s);
        if (n > 18) {
            for (sp = s + 18; sp > s && *sp != ' '; sp--)
                ;
            n = sp > s ? (byte)(sp - s) : 18;
        }
        memcpy(line, s, n);
        line[n] = 0;
        center(x, 20, y + row++, fg, line);
        s += n;
        while (*s == ' ') s++;
    }
}

void show_forecast(byte page)
{
    byte j, x, i, icon;
    Day *d;
    char left[52];

    term_clrscr();
    place(left);
    strcpy(buf, page ? "Days 5 to 8" : "Days 1 to 4");
    bar(0, left, buf);

    for (j = 0; j < 4; j++) {
        i = page * 4 + j;
        d = &rep.day[i];
        x = j * 20;
        icon = icon_of((byte) atoi(d->code));
        center(x, 20, 2, WHITE, i ? day_name(d->date, 1) : "Today");
        strcpy(buf, d->date[8] == '0' ? d->date + 9 : d->date + 8);
        strcat(buf, " ");
        strcat(buf, month_name(d->date, 1));
        center(x, 20, 3, TC_CYAN, buf);
        draw_icon(x + 3, 5, icon, 1);
        wrapped(x, 12, describe((byte) atoi(d->code)), sky_color(icon));

        put_at(x + 2, 15, TC_WHITE, "High   ");
        put_temp(d->tmax);
        put_at(x + 2, 16, TC_WHITE, "Low    ");
        put_temp(d->tmin);
        put_at(x + 2, 17, TC_WHITE, "Rain   ");
        if (tenths(d->precip)) fmt_precip(buf, d->precip);
        else strcpy(buf, "none");
        put(TC_BRIGHT + TC_BLUE, buf);
        put_at(x + 2, 18, TC_WHITE, "Chance ");
        strcpy(buf, d->chance[0] ? d->chance : "0");
        strcat(buf, "%");
        put(TC_BRIGHT + TC_BLUE, buf);
        put_at(x + 2, 19, TC_WHITE, "UV     ");
        put_tenths(buf, tenths(d->uv), 0);
        strcat(buf, " ");
        strcat(buf, uv_word(tenths(d->uv)));
        put(uv_color(tenths(d->uv)), buf);
        put_at(x + 2, 20, TC_WHITE, "Wind   ");
        fmt_speed(buf, d->wind);
        put(WHITE, buf);
        put(TC_WHITE, " ");
        put(WHITE, compass(d->wind_dir));
        put_at(x + 2, 21, TC_WHITE, "Sun    ");
        hhmm(buf, d->sunrise);
        put(TC_BRIGHT + TC_YELLOW, buf);
        put(TC_WHITE, "-");
        hhmm(buf, d->sunset);
        put(TC_YELLOW, buf);
    }
    show_keys(1);
}

void show_about(void)
{
    term_clrscr();
    bar(0, "fnweather", "the weather, through the FujiNet");
    draw_icon(4, 3, ICON_PARTLY, 1);
    put_at(22, 3, WHITE, "fnweather");
    put_at(22, 4, TC_WHITE, "The weather where you are, now and for the week.");
    put_at(22, 6, TC_CYAN, "The place:    ");
    put(WHITE, "ip-api.com, from your network's address,");
    put_at(36, 7, WHITE, "or Open-Meteo's geocoding, for a city");
    put_at(22, 8, TC_CYAN, "The weather:  ");
    put(WHITE, "Open-Meteo (open-meteo.com), CC BY 4.0");
    put_at(22, 10, TC_CYAN, "Kept in:      ");
    put(WHITE, cache_path);
    put_at(22, 11, TC_WHITE, "Fetched again after 30 minutes, or with R.");
    put_at(22, 13, TC_CYAN, "After:        ");
    put(WHITE, "FujiNetWIFI/fujinet-weather, for the CoCo,");
    put_at(36, 14, WHITE, "by Shunichi Kitahara and Thomas Cherryhomes");
    put_at(22, 15, TC_CYAN, "Ported:       ");
    put(WHITE, "to NitrOS-9 in 2026 by DILLER,");
    put_at(36, 16, WHITE, "for the TFR/911H");
    center(0, TERM_COLS, 21, TC_WHITE, "Press a key");
    color(TC_DEFAULT);
    term_flush();
}

// A city to look up, typed on the status line; empty for where you are.
byte ask_city(char *out, byte size)
{
    term_gotoxy(0, ROW_STATUS);
    term_clreol();
    put_at(1, ROW_STATUS, WHITE, "City, or Enter for where you are: ");
    color(TC_BRIGHT + TC_YELLOW);
    term_getline(out, size);
    color(TC_DEFAULT);
    return out[0] != 0;
}
