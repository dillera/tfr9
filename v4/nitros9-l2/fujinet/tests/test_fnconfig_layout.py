import pathlib
import re
import unittest


SOURCE_PATH = pathlib.Path(__file__).parents[1] / "cmds" / "fnconfig.as"


class FnConfigTerminalContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.source = SOURCE_PATH.read_text()

    def routine(self, start, end):
        match = re.search(
            rf"^{re.escape(start)}\b(?P<body>.*?)^{re.escape(end)}\b",
            self.source,
            re.MULTILINE | re.DOTALL,
        )
        self.assertIsNotNone(match, f"missing {start}..{end} source block")
        return match.group("body")

    def test_host_rows_end_with_crlf(self):
        body = self.routine("DrawHostName", "DrawHostsDone")
        self.assertRegex(body, r"lbsr\s+NewLine")

    def test_device_rows_end_with_crlf(self):
        body = self.routine("DrawDevFile", "DrawDevsDone")
        self.assertRegex(body, r"lbsr\s+NewLine")

    def test_device_mounted_flag_reloads_the_slot_record(self):
        body = self.routine("DrawModeText", "DrawMountText")
        self.assertRegex(
            body,
            r"(?s)lbsr\s+DeviceSlotX.*?lda\s+1,x.*?bita\s+#MODE\$Mounted",
        )

    def test_device_filename_reloads_the_slot_record(self):
        body = self.routine("DrawMountText", "DrawDevFile")
        self.assertRegex(
            body,
            r"(?s)lbsr\s+DeviceSlotX.*?leax\s+2,x.*?lbsr\s+FixedToLine",
        )

    def test_newline_is_an_explicit_crlf_pair(self):
        self.assertRegex(self.source, r"ctlNewLine\s+fcb\s+\$0D,\$0A")

    def test_info_key_and_screen_are_present(self):
        common = self.routine("MainCommon", "MainDispatch")
        self.assertRegex(common, r"cmpa\s+#'I")
        self.assertIn("DrawInfo", common)
        self.assertIn("Version 1.0", self.source)
        self.assertIn("Written for FujiNet by Diller", self.source)


if __name__ == "__main__":
    unittest.main()
