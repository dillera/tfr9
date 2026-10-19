# @license gpl v. 3, see fujinet/LICENSE for details.
import pathlib
import re
import unittest


ROOT = pathlib.Path(__file__).parents[1]
APP_PATH = ROOT / "cmds" / "fnwx.as"
NET_PATH = ROOT / "lib" / "fnnet.as"
MAKEFILE_PATH = ROOT.parent / "Makefile"


def fcn(source, label):
    match = re.search(
        rf'^\s*{re.escape(label)}\s+fcn\s+"(?P<value>[^"]*)"',
        source,
        re.MULTILINE,
    )
    if match is None:
        raise AssertionError(f"missing fcn string {label}")
    return match.group("value")


class FnWeatherSourceContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.app = APP_PATH.read_text()
        cls.net = NET_PATH.read_text()
        cls.makefile = MAKEFILE_PATH.read_text()

    def test_uses_reference_location_and_weather_services(self):
        self.assertEqual(
            fcn(self.app, "ipURL"),
            "N:http://ip-api.com/json/?fields=status,city,regionName,countryCode,lon,lat",
        )
        self.assertIn("api.open-meteo.com/v1/forecast", fcn(self.app, "meteoHead"))
        self.assertIn("forecast_days=1", fcn(self.app, "currentTail1"))

    def test_current_weather_uses_two_compact_requests(self):
        weather = re.search(
            r"^FetchWeather\b(?P<body>.*?)^FetchCurrentOne\b",
            self.app,
            re.MULTILINE | re.DOTALL,
        )
        self.assertIsNotNone(weather)
        body = weather.group("body")
        self.assertIn("FetchCurrentOne", body)
        self.assertIn("FetchCurrentTwo", body)
        self.assertNotIn("FetchForecast", body)

    def test_weather_urls_fit_the_fujinet_256_byte_device_spec(self):
        # The destination fields allow 19 coordinate characters plus NUL.
        coordinate = "-" * 19
        common = (
            fcn(self.app, "meteoHead")
            + coordinate
            + fcn(self.app, "meteoLongitude")
            + coordinate
        )
        variants = (
            common + fcn(self.app, "currentTail1") + fcn(self.app, "imperialTemp"),
            common + fcn(self.app, "currentTail2") + fcn(self.app, "imperialWind"),
        )
        for url in variants:
            self.assertLessEqual(len(url), 255, url)

    def test_required_json_queries_are_present(self):
        for query in (
            "/status",
            "/city",
            "/regionName",
            "/countryCode",
            "/lat",
            "/lon",
            "/current/time",
            "/current/temperature_2m",
            "/current/weather_code",
            "/current/surface_pressure",
            "/current/wind_speed_10m",
            "/current/wind_direction_10m",
        ):
            self.assertIn(f'"{query}"', self.app)

    def test_network_client_uses_drivewire_net_json_workflow(self):
        for declaration in (
            "FN$Open             equ       'O",
            "FN$Read             equ       'R",
            "FN$Status           equ       'S",
            "FN$Parse            equ       'P",
            "FN$Query            equ       'Q",
            "FN$ChannelMode      equ       $FC",
        ):
            self.assertIn(declaration, self.net)
        self.assertRegex(
            self.net,
            r"(?s)FNXfer.*?ldb\s+#SS\.Fuji.*?os9\s+I\$SetStt",
        )

    def test_json_copy_keeps_a_16_bit_remaining_count(self):
        block = re.search(
            r"^FNCopyValue\b(?P<body>.*?)^FNCopyValueDone\b",
            self.net,
            re.MULTILINE | re.DOTALL,
        )
        self.assertIsNotNone(block)
        self.assertRegex(
            block.group("body"),
            r"(?s)sta\s+,y\+.*?ldd\s+fnavail,u.*?subd\s+#1.*?std\s+fnavail,u",
        )

    def test_json_query_reads_one_caller_bounded_buffer_without_status(self):
        query = re.search(
            r"^FNNetQuery\b(?P<body>.*?)^FNTooLarge\b",
            self.net,
            re.MULTILINE | re.DOTALL,
        )
        self.assertIsNotNone(query)
        body = query.group("body")
        self.assertIn("fnreadlen           rmb       2", self.net)
        self.assertRegex(
            body,
            r"(?s)lda\s+#FN\$Read.*?clra.*?ldb\s+fncapacity,u.*?"
            r"std\s+fnreadlen,u.*?"
            r"std\s+fnreq\+5,u.*?"
            r"ldd\s+fnreadlen,u.*?leay\s+fnresp,u.*?lbsr\s+FNGetResponse",
        )
        self.assertNotRegex(body, r"ldd\s+#256")
        self.assertNotIn("#FN$Status", body)
        self.assertNotIn("fnstatus,u", body)

    def test_json_query_sends_query_then_reads_the_value_directly(self):
        query = re.search(
            r"^FNNetQuery\b(?P<body>.*?)^FNTooLarge\b",
            self.net,
            re.MULTILINE | re.DOTALL,
        )
        self.assertIsNotNone(query)
        body = query.group("body")
        self.assertRegex(
            body,
            r"(?s)ldy\s+#FN\$ReqSize.*?lbsr\s+FNSend\b.*?"
            r"lda\s+#FN\$Read.*?lbsr\s+FNSend\b.*?lbsr\s+FNGetResponse",
        )
        self.assertNotIn("FNSendCheck", body)

    def test_json_copy_stops_at_nul_cr_or_lf(self):
        block = re.search(
            r"^FNCopyValue\b(?P<body>.*?)^FNCopyValueDone\b",
            self.net,
            re.MULTILINE | re.DOTALL,
        )
        self.assertIsNotNone(block)
        body = block.group("body")
        self.assertRegex(body, r"(?s)lda\s+,x\+.*?beq\s+FNCopyValueDone")
        self.assertRegex(body, r"(?s)cmpa\s+#C\$CR.*?beq\s+FNCopyValueDone")
        self.assertRegex(body, r"(?s)cmpa\s+#C\$LF.*?beq\s+FNCopyValueDone")

    def test_weather_fields_are_cleared_before_each_fetch(self):
        weather = re.search(
            r"^FetchWeather\b(?P<body>.*?)^FetchCurrentOne\b",
            self.app,
            re.MULTILINE | re.DOTALL,
        )
        self.assertIsNotNone(weather)
        self.assertIn("lbsr      ClearWeatherData", weather.group("body"))

        clear = re.search(
            r"^ClearWeatherData\b(?P<body>.*?)^ClearWeatherLoop\b"
            r"(?P<loop>.*?)^ClearWeatherDone\b",
            self.app,
            re.MULTILINE | re.DOTALL,
        )
        self.assertIsNotNone(clear)
        self.assertRegex(clear.group("body"), r"leax\s+datetime,u")
        self.assertRegex(clear.group("body"), r"ldy\s+#urlbuf-datetime")
        self.assertRegex(clear.group("loop"), r"(?s)clr\s+,x\+.*?leay\s+-1,y.*?bne\s+ClearWeatherLoop")

    def test_send_keeps_the_request_pointer_for_ss_fuji_without_a_global_ready_reply(self):
        block = re.search(
            r"^FNSend\b(?P<body>.*?)^FNGetError\b",
            self.net,
            re.MULTILINE | re.DOTALL,
        )
        self.assertIsNotNone(block)
        self.assertRegex(
            block.group("body"),
            r"(?s)pshs\s+x.*?leax\s+fnresp,u.*?stx\s+fnout,u.*?"
            r"puls\s+x.*?lbsr\s+FNXfer.*?tstb.*?andcc\s+#\^Carry",
        )
        self.assertNotIn("FBReady", block.group("body"))

    def test_network_response_uses_its_own_e3_stream_without_a_ready_reply(self):
        """Do not put an E2 Ready reply in front of the raw E3 response.

        The hardware trace showed that this causes a one-byte shift:
        ``01 00 11 01`` instead of the four-byte E3 status ``00 11 01 01``.
        """
        block = re.search(
            r"^FNGetResponse\b(?P<body>.*?)^FNXfer\b",
            self.net,
            re.MULTILINE | re.DOTALL,
        )
        self.assertIsNotNone(block)
        body = block.group("body")
        self.assertRegex(
            body,
            r"(?s)lda\s+#FN\$Response.*?ldd\s+fnavail,u.*?"
            r"std\s+fnreq\+5,u.*?ldy\s+#7.*?lbra\s+FNXfer",
        )
        self.assertNotIn("FBReady", body)

    def test_network_error_uses_its_own_e3_stream_without_a_global_ready_reply(self):
        block = re.search(
            r"^FNGetError\b(?P<body>.*?)^FNGetResponse\b",
            self.net,
            re.MULTILINE | re.DOTALL,
        )
        self.assertIsNotNone(block)
        self.assertRegex(
            block.group("body"),
            r"(?s)lda\s+#FN\$Error.*?ldy\s+#7.*?lbsr\s+FNXfer",
        )
        self.assertNotIn("FBReady", block.group("body"))

    def test_network_init_settles_without_putting_a_ready_reply_on_the_wire(self):
        block = re.search(
            r"^FNNetInit\b(?P<body>.*?)^FNNetOpen\b",
            self.net,
            re.MULTILINE | re.DOTALL,
        )
        self.assertIsNotNone(block)
        self.assertIn("NOpen", block.group("body"))
        self.assertIn("F$Sleep", block.group("body"))
        self.assertRegex(block.group("body"), r"(?s)pshs\s+u.*?F\$Sleep.*?puls\s+u")
        self.assertNotIn("FBReady", block.group("body"))

    def test_parse_waits_for_slow_http_then_defers_validation_to_first_query(self):
        self.assertIn("FN$ParseWait        equ       600", self.net)
        block = re.search(
            r"^FNNetParse\b(?P<body>.*?)^FNNetQuery\b",
            self.net,
            re.MULTILINE | re.DOTALL,
        )
        self.assertIsNotNone(block)
        self.assertRegex(
            block.group("body"),
            r"(?s)lda\s+#FN\$Parse.*?lbsr\s+FNSend\b.*?"
            r"pshs\s+u.*?os9\s+F\$Sleep.*?puls\s+u.*?"
            r"clrb.*?andcc\s+#\^Carry.*?rts",
        )
        self.assertNotIn("FNGetError", block.group("body"))

    def test_transport_diagnostics_capture_ss_fuji_arguments_and_errors(self):
        self.assertIn("fnxferlen           rmb       2", self.net)
        self.assertIn("fnxfererr           rmb       1", self.net)
        block = re.search(
            r"^FNXfer\b(?P<body>.*?)^FNXferError\b(?P<error>.*?)^\s*endsect",
            self.net,
            re.MULTILINE | re.DOTALL,
        )
        self.assertIsNotNone(block)
        self.assertIn("sty       fnxferlen,u", block.group("body"))
        self.assertIn("stb       fnxfererr,u", block.group("error"))
        self.assertIn("fnxferlen,u", self.app)
        self.assertIn("fnxfererr,u", self.app)

    def test_weather_error_path_does_not_close_an_endpoint_twice(self):
        block = re.search(
            r"^FetchWeather\b(?P<body>.*?)^FetchCurrentOne\b",
            self.app,
            re.MULTILINE | re.DOTALL,
        )
        self.assertIsNotNone(block)
        self.assertNotIn("FetchAbort", block.group("body"))

    def test_weather_error_screen_keeps_transport_diagnostics(self):
        error = re.search(
            r"^DrawError\b(?P<body>.*?)^DrawInfo\b",
            self.app,
            re.MULTILINE | re.DOTALL,
        )
        self.assertIsNotNone(error)
        body = error.group("body")
        for field in ("debugStep,u", "fnstatus,u", "fnreadlen,u", "fnerror,u"):
            self.assertIn(field, body)
        self.assertIn('debugCurrentTime    fcn       "reading /current/time"', self.app)

    def test_compass_comparisons_remain_16_bit(self):
        block = re.search(
            r"^WindDirection\b(?P<body>.*?)^WeatherDescription\b",
            self.app,
            re.MULTILINE | re.DOTALL,
        )
        self.assertIsNotNone(block)
        body = block.group("body")
        self.assertIn("cmpd      #293", body)
        self.assertIn("cmpd      #338", body)
        self.assertNotRegex(body, r"cmpb\s+#293")

    def test_tui_controls_units_location_refresh_and_help(self):
        for text in (
            "[R] Refresh",
            "[U] Units",
            "[L] Locate",
            "[I] Info",
            "[Q] Quit",
            "Written for FujiNet and NitrOS-9 by Diller.",
        ):
            self.assertIn(text, self.app)
        self.assertRegex(self.app, r"fcb\s+C\$CR,C\$LF")

    def test_every_hardware_test_page_shows_a_build_stamp(self):
        self.assertRegex(
            self.app,
            r'(?m)^buildStamp\s+fcn\s+"Build: \d{4}-\d{2}-\d{2} '
            r'\d{2}:\d{2} EDT"',
        )
        for page in ("DrawWeather", "DrawLoading", "DrawError", "DrawInfo"):
            block = re.search(
                rf"^{page}\b(?P<body>.*?)(?=^[A-Za-z][A-Za-z0-9]*\s|\Z)",
                self.app,
                re.MULTILINE | re.DOTALL,
            )
            self.assertIsNotNone(block)
            self.assertIn("DrawBuildStamp", block.group("body"))

    def test_data_inspector_exposes_raw_received_weather_fields(self):
        self.assertIn("cmpa      #'D", self.app)
        block = re.search(
            r"^DrawData\b(?P<body>.*?)^DrawBuildStamp\b",
            self.app,
            re.MULTILINE | re.DOTALL,
        )
        self.assertIsNotNone(block)
        for field in ("city,u", "datetime,u", "timezone,u", "temperature,u",
                      "feels,u", "humidity,u", "weathercode,u"):
            self.assertIn(field, block.group("body"))
        # Distinguish a transport/copy failure from a formatting failure when
        # exercising the app on real FujiNet hardware.
        self.assertIn("fnresp,u", block.group("body"))

    def test_tfr911_disk_includes_every_fujinet_command_linked_with_fnnet(self):
        self.assertIn("$(wildcard fujinet/cmds/*.as)", self.makefile)
        self.assertRegex(self.makefile, r"build/fn/%: build/fn/%\.o build/fn/fnnet\.o")
        self.assertIn("-DANSI=1", self.makefile)


if __name__ == "__main__":
    unittest.main()
