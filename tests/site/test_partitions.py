import os
import struct
import sys
import unittest

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
import check_partitions as cp  # noqa: E402


def table(entries):
    """Una tabella come la scrive la build: voci da 32 byte, poi il vuoto."""
    out = b""
    for name, ptype, sub, off, size in entries:
        out += cp.ENTRY.pack(cp.MAGIC, ptype, sub, off, size, name.encode().ljust(16, b"\0"), 0)
    return out + b"\xff" * 32


DEFAULT = [("nvs", 1, 2, 0x9000, 0x5000), ("otadata", 1, 0, 0xE000, 0x2000), ("app0", 0, 0x10, 0x10000, 0x140000),
           ("app1", 0, 0x11, 0x150000, 0x140000), ("spiffs", 1, 0x82, 0x290000, 0x160000), ("coredump", 1, 3, 0x3F0000, 0x10000)]
MIN_SPIFFS = [("nvs", 1, 2, 0x9000, 0x5000), ("otadata", 1, 0, 0xE000, 0x2000), ("app0", 0, 0x10, 0x10000, 0x1E0000),
              ("app1", 0, 0x11, 0x1F0000, 0x1E0000), ("spiffs", 1, 0x82, 0x3D0000, 0x20000), ("coredump", 1, 3, 0x3F0000, 0x10000)]


class PartitionsTest(unittest.TestCase):
    def test_tabelle_buone(self):
        # la tabella di prima e quella con piu' spazio per il programma: le impostazioni restano allo stesso posto
        for t in (DEFAULT, MIN_SPIFFS):
            errs, parts = cp.check(table(t))
            self.assertEqual(errs, [])
            self.assertEqual(parts["nvs"][2:], (0x9000, 0x5000))
        self.assertEqual(cp.check(table(MIN_SPIFFS))[1]["app0"][3], 0x1E0000)

    def test_nvs_spostata(self):
        bad = [("nvs", 1, 2, 0x10000, 0x5000)] + MIN_SPIFFS[1:]
        errs, _ = cp.check(table(bad))
        self.assertTrue(any("nvs" in e and "impostazioni" in e for e in errs))
        bad = [("nvs", 1, 2, 0x9000, 0x6000)] + MIN_SPIFFS[1:]
        self.assertTrue(cp.check(table(bad))[0])

    def test_otadata_o_app_spostati(self):
        self.assertTrue(cp.check(table([MIN_SPIFFS[0], ("otadata", 1, 0, 0xF000, 0x2000)] + MIN_SPIFFS[2:]))[0])
        self.assertTrue(cp.check(table(MIN_SPIFFS[:2] + [("app0", 0, 0x10, 0x20000, 0x1E0000)] + MIN_SPIFFS[3:]))[0])

    def test_mancanti(self):
        self.assertTrue(any("manca" in e for e in cp.check(table(MIN_SPIFFS[1:]))[0]))   # niente nvs
        self.assertTrue(any("app0" in e for e in cp.check(table(MIN_SPIFFS[:2]))[0]))    # niente programma
        self.assertTrue(cp.check(b"")[0])


if __name__ == "__main__":
    unittest.main()
