#!/usr/bin/env python3
"""Prova di tools/site_versions.py: classificazione, ordine e versione proposta."""
import os
import sys
import unittest
import json

def rb(path):
    with open(path, "rb") as f:
        return f.read()


def load(path):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
import site_versions as sv  # noqa: E402


class Classifica(unittest.TestCase):
    def test_fasi(self):
        self.assertEqual(sv.kind_of("v0.2.0"), "stable")
        self.assertEqual(sv.kind_of("v1.10.3"), "stable")
        self.assertEqual(sv.kind_of("v0.2.0-rc.1"), "rc")
        self.assertEqual(sv.kind_of("v0.1.0-beta.1"), "beta")
        self.assertEqual(sv.kind_of("main-09/11/2026"), "dev")
        self.assertEqual(sv.kind_of("dev"), "dev")

    def test_non_versioni(self):
        for t in ("", None, "v1", "v0.1", "0.1.0", "v0.1.0-alpha.1", "v0.1.0-beta", "release-1", "v0.1.0-beta.1-3-gabc1234"):
            self.assertIsNone(sv.kind_of(t), t)


class Elenco(unittest.TestCase):
    def ids(self, d):
        return [e["id"] for e in d["versions"]]

    def test_solo_beta_e_sviluppo(self):
        d = sv.build("dev", "main-09/11/2026", "abc1234", [{"tag": "v0.1.0-beta.1", "date": "2026-11-08"}])
        self.assertEqual(self.ids(d), ["v0.1.0-beta.1", "dev"])
        self.assertEqual(d["default"], "v0.1.0-beta.1")  # la beta e' proposta, non lo sviluppo
        self.assertEqual(d["versions"][0]["date"], "2026-11-08")
        self.assertEqual(d["versions"][1]["commit"], "abc1234")

    def test_ordine_stabili_rc_beta_sviluppo(self):
        rel = [{"tag": t} for t in ("v0.1.0-beta.1", "v0.2.0-rc.1", "v0.1.0", "v0.2.0", "v0.1.0-beta.2", "v0.10.0", "v0.9.5")]
        d = sv.build("dev", "main-09/11/2026", "c", rel)
        self.assertEqual(self.ids(d), ["v0.10.0", "v0.9.5", "v0.2.0", "v0.1.0", "v0.2.0-rc.1", "v0.1.0-beta.2", "v0.1.0-beta.1", "dev"])
        self.assertEqual(d["default"], "v0.10.0")  # la stabile piu' recente; 0.10.0 > 0.9.5 (non e' un ordine alfabetico)

    def test_senza_stabile_si_propone_la_rc(self):
        d = sv.build("dev", "main-09/11/2026", "c", [{"tag": "v0.1.0-beta.3"}, {"tag": "v0.1.0-rc.1"}])
        self.assertEqual(d["default"], "v0.1.0-rc.1")

    def test_senza_release_si_propone_lo_sviluppo(self):
        d = sv.build("dev", "main-09/11/2026", "c", [])
        self.assertEqual(self.ids(d), ["dev"])
        self.assertEqual(d["default"], "dev")

    def test_build_da_tag(self):
        # La pubblicazione di un tag: il build corrente e' il tag e non si ripete tra le release scaricate.
        d = sv.build("v0.1.0-beta.1", "v0.1.0-beta.1", "7009c8e", [{"tag": "v0.1.0-beta.1"}, {"tag": "v0.1.0-beta.0"}])
        self.assertEqual(self.ids(d), ["v0.1.0-beta.1", "v0.1.0-beta.0"])
        self.assertEqual(d["versions"][0]["commit"], "7009c8e")
        self.assertEqual(d["default"], "v0.1.0-beta.1")

    def test_tag_non_validi_ignorati(self):
        d = sv.build("dev", "main-09/11/2026", "c", [{"tag": "nightly"}, {"tag": ""}, {"tag": "v0.1.0-beta.1"}])
        self.assertEqual(self.ids(d), ["v0.1.0-beta.1", "dev"])


class Sito(unittest.TestCase):
    """tools/make_site.sh con artefatti e release finti."""

    def setUp(self):
        import tempfile
        self.tmp = tempfile.mkdtemp()

    def tearDown(self):
        import shutil
        shutil.rmtree(self.tmp, ignore_errors=True)

    @staticmethod
    def image(tag):
        """Un'immagine come quella di merge_bin: vuota (0xFF) con pezzi riconoscibili a 0x1000, 0x8000, 0xE000 e 0x10000."""
        d = bytearray(b"\xff" * 0x10020)
        d[0x1000:0x1000 + len(tag) + 5] = b"boot-" + tag
        d[0x8000:0x8000 + len(tag) + 6] = b"table-" + tag
        d[0xE000:0xE000 + len(tag) + 5] = b"otad-" + tag
        d[0x10000:0x10000 + len(tag) + 4] = b"app-" + tag
        d[0x9000:0x9010] = b"NVS-NON-TOCCARE!"   # dove stanno le impostazioni: nell'immagine intera e' un vuoto, qui un segnale
        return bytes(d)

    def put(self, path, data=b"x"):
        full = os.path.join(self.tmp, path)
        os.makedirs(os.path.dirname(full), exist_ok=True)
        with open(full, "wb") as f:
            f.write(data)

    def run_site(self, version, with_releases=True):
        import subprocess
        for art in ("firmware-t3-v161", "firmware-tbeam-v12", "firmware-tbeam-v12-sx1262"):
            self.put("art/%s/web_factory.bin" % art, self.image(b"corrente-" + art.encode()))
            self.put("art/%s/ota_update.bin" % art)
        args = [os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools", "make_site.sh"),
                os.path.join(self.tmp, "art"), os.path.join(self.tmp, "out"), version, "abc1234def"]
        if with_releases:
            for tag in ("v0.1.0-beta.1", "v0.1.0-beta.0"):
                self.put("rel/%s/firmware-t3-v161-web_factory.bin" % tag, self.image(b"t3-" + tag.encode()))
                self.put("rel/%s/firmware-tbeam-v12-web_factory.bin" % tag, self.image(b"tb-" + tag.encode()))
            self.put("rel/v0.0.9/senza-binari.txt")   # una release senza binari non entra nel menu
            self.put("rel/index.json", json.dumps([{"tagName": "v0.1.0-beta.1", "publishedAt": "2026-11-08T14:41:43Z"},
                                                   {"tagName": "v0.1.0-beta.0", "publishedAt": "2026-11-01T10:00:00Z"}]).encode())
            args.append(os.path.join(self.tmp, "rel"))
        r = subprocess.run(["bash"] + args, capture_output=True, text=True)
        self.assertEqual(r.returncode, 0, r.stderr)
        return os.path.join(self.tmp, "out")

    def test_sviluppo_con_release(self):
        out = self.run_site("main-09/11/2026")
        v = load(os.path.join(out, "versions.json"))
        self.assertEqual([e["id"] for e in v["versions"]], ["v0.1.0-beta.1", "v0.1.0-beta.0", "dev"])
        self.assertEqual(v["default"], "v0.1.0-beta.1")
        self.assertEqual(v["versions"][0]["date"], "2026-11-08")
        # Binari al loro posto, ognuno con il suo
        self.assertEqual(rb(os.path.join(out, "firmware/v0.1.0-beta.1/t3/web_factory.bin")), self.image(b"t3-v0.1.0-beta.1"))
        self.assertEqual(rb(os.path.join(out, "firmware/v0.1.0-beta.0/tbeam/web_factory.bin")), self.image(b"tb-v0.1.0-beta.0"))
        self.assertEqual(rb(os.path.join(out, "firmware/dev/t3/web_factory.bin")), self.image(b"corrente-firmware-t3-v161"))
        self.assertFalse(os.path.exists(os.path.join(out, "firmware/v0.0.9")))
        # T-Beam con SX1262: c'e' solo nel build corrente; le release uscite prima non lo hanno e il menu lo sa (campo "boards")
        self.assertEqual(rb(os.path.join(out, "firmware/dev/tbeam1262/web_factory.bin")), self.image(b"corrente-firmware-tbeam-v12-sx1262"))
        self.assertFalse(os.path.exists(os.path.join(out, "firmware/v0.1.0-beta.1/tbeam1262")))
        by_id = {e["id"]: e for e in v["versions"]}
        self.assertEqual(by_id["dev"]["boards"], ["t3", "tbeam", "tbeam1262"])
        self.assertEqual(by_id["v0.1.0-beta.1"]["boards"], ["t3", "tbeam"])
        m = load(os.path.join(out, "firmware/v0.1.0-beta.1/t3/manifest.json"))
        self.assertEqual(m["version"], "v0.1.0-beta.1")
        # esp-web-tools 10.1.1: con "new_install_prompt_erase" false cancella TUTTA la memoria (anche le impostazioni) senza chiedere;
        # con true mostra la casella "Erase device", lasciata vuota di default
        self.assertIs(m["new_install_prompt_erase"], True)
        # Il flasher scrive quattro pezzi, non l'immagine intera: la partizione delle impostazioni (nvs, da 0x9000 a 0xDFFF) non si tocca
        parts = m["builds"][0]["parts"]
        self.assertEqual([p["offset"] for p in parts], [0x1000, 0x8000, 0xE000, 0x10000])
        self.assertEqual([p["path"] for p in parts], ["part_boot.bin", "part_table.bin", "part_otadata.bin", "part_app.bin"])
        for p in parts:
            a, b = p["offset"], p["offset"] + len(rb(os.path.join(out, "firmware/v0.1.0-beta.1/t3", p["path"])))
            self.assertTrue(b <= 0x9000 or a >= 0xE000, "un pezzo scrive nella zona delle impostazioni: %s" % p)
        self.assertTrue(rb(os.path.join(out, "firmware/v0.1.0-beta.1/t3/part_app.bin")).startswith(b"app-t3-v0.1.0-beta.1"))
        self.assertTrue(rb(os.path.join(out, "firmware/v0.1.0-beta.1/t3/part_boot.bin")).startswith(b"boot-t3-v0.1.0-beta.1"))
        self.assertTrue(rb(os.path.join(out, "firmware/v0.1.0-beta.1/t3/part_table.bin")).startswith(b"table-t3-v0.1.0-beta.1"))
        self.assertTrue(rb(os.path.join(out, "firmware/v0.1.0-beta.1/t3/part_otadata.bin")).startswith(b"otad-t3-v0.1.0-beta.1"))
        for n in ("part_boot.bin", "part_table.bin", "part_otadata.bin", "part_app.bin"):  # nessun pezzo contiene il segnale di dove stanno le impostazioni
            self.assertNotIn(b"NVS-NON-TOCCARE!", rb(os.path.join(out, "firmware/dev/t3", n)))
        # Percorso storico: stessi pezzi del build corrente, senza una seconda copia
        old = load(os.path.join(out, "firmware/t3/manifest.json"))
        self.assertEqual(old["version"], "main-09/11/2026")
        self.assertEqual([p["offset"] for p in old["builds"][0]["parts"]], [0x1000, 0x8000, 0xE000, 0x10000])
        target = os.path.normpath(os.path.join(out, "firmware/t3", old["builds"][0]["parts"][3]["path"]))
        self.assertTrue(rb(target).startswith(b"app-corrente-firmware-t3-v161"))
        self.assertEqual(load(os.path.join(out, "version.json"))["version"], "main-09/11/2026")

    def test_build_da_tag_non_si_ripete(self):
        out = self.run_site("v0.1.0-beta.1")
        v = load(os.path.join(out, "versions.json"))
        self.assertEqual([e["id"] for e in v["versions"]], ["v0.1.0-beta.1", "v0.1.0-beta.0"])  # niente "dev" da un tag
        self.assertEqual(v["versions"][0]["commit"], "abc1234def")
        # il build corrente (da artefatto) prevale sulla release con lo stesso tag
        self.assertEqual(rb(os.path.join(out, "firmware/v0.1.0-beta.1/t3/web_factory.bin")), self.image(b"corrente-firmware-t3-v161"))

    def test_senza_release(self):
        out = self.run_site("main-09/11/2026", with_releases=False)
        v = load(os.path.join(out, "versions.json"))
        self.assertEqual([e["id"] for e in v["versions"]], ["dev"])
        self.assertEqual(v["default"], "dev")


if __name__ == "__main__":
    unittest.main(verbosity=1)
