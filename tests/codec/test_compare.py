#!/usr/bin/env python3
"""Prova di tools/codec_compare.c: il livello d'ingresso non deve decidere la qualita' del confronto.

Registrazioni sintetiche di parlato (raffiche con pause, rimbombo a 50 Hz e un po' di rumore) a livelli diversi: troppo basso,
medio, alto, saturo, muto. Controlli: il parlato che arriva al codec e' a circa -24 dBFS con qualunque livello non saturo, il
verdetto del livello e' quello giusto, la registrazione muta e' rifiutata. Serve build/codec_compare (make codec_compare).
"""
import math
import os
import random
import struct
import subprocess
import sys
import tempfile
import wave

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
EXE = os.path.join(ROOT, "build", "codec_compare")
failures = 0


def check(name, cond, extra=""):
    global failures
    if not cond:
        failures += 1
        print("  FAIL %s %s" % (name, extra))


def make(path, amp, secs=6, clip=False, noise=8, silence=False):
    random.seed(11)
    rate = 8000
    out = []
    for i in range(rate * secs):
        t = i / rate
        f0 = 120 + 30 * math.sin(2 * math.pi * 0.45 * t)
        gate = 1.0 if (t % 2.0) < 1.2 else 0.0
        env = gate * (0.5 + 0.5 * math.sin(2 * math.pi * 1.3 * t) ** 2)
        s = sum(math.sin(2 * math.pi * f0 * h * t) / h * (1.5 if 5 <= h <= 8 else 1) for h in range(1, 24))
        v = amp * (env * s + 0.4 * math.sin(2 * math.pi * 50 * t)) + random.randint(-noise, noise)
        if silence:
            v = 0
        if clip:
            v *= 8
        out.append(int(max(-32768, min(32767, v))))
    w = wave.open(path, "wb")
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(rate)
    w.writeframes(struct.pack("<%dh" % len(out), *out))
    w.close()


def active_db(path):
    """Livello del parlato (dBFS): i frame da 40 ms entro 15 dB dal 90 percentile, come lo strumento."""
    w = wave.open(path)
    n = w.getnframes()
    d = struct.unpack("<%dh" % n, w.readframes(n))
    frames = n // 320
    pw = [sum((x / 32768.0) ** 2 for x in d[f * 320:(f + 1) * 320]) / 320 for f in range(frames)]
    db = [10 * math.log10(p + 1e-12) for p in pw]
    srt = sorted(db)
    p90 = srt[(frames - 1) * 9 // 10]
    act = [pw[f] for f in range(frames) if db[f] >= p90 - 15]
    return 10 * math.log10(sum(act) / len(act) + 1e-12)


def run(*args):
    return subprocess.run([EXE] + list(args), capture_output=True, text=True)


if not os.path.exists(EXE):
    sys.exit("manca %s: make codec_compare" % EXE)

with tempfile.TemporaryDirectory() as tmp:
    cases = {"basso": dict(amp=25, noise=2), "medio": dict(amp=600), "alto": dict(amp=3500), "satura": dict(amp=3000, clip=True),
             "muto": dict(amp=0, silence=True)}
    for name, kw in cases.items():
        make(os.path.join(tmp, name + ".wav"), **kw)

    # Con qualunque livello non saturo il parlato arriva al codec a circa -24 dBFS.
    for name in ("basso", "medio", "alto"):
        out = os.path.join(tmp, "out_" + name)
        r = run(os.path.join(tmp, name + ".wav"), out, "--modi", "1200")
        check("%s: esce senza errori" % name, r.returncode == 0, r.stdout + r.stderr)
        for f in ("originale.wav", "ingresso-codec.wav", "1200.wav"):
            check("%s: scrive %s" % (name, f), os.path.exists(os.path.join(out, f)))
        lvl = active_db(os.path.join(out, "ingresso-codec.wav"))
        check("%s: il codec riceve il parlato a -24 dBFS (misurato %.1f)" % (name, lvl), abs(lvl + 24) <= 1.5)

    # Verdetti
    verdicts = {"basso": "BASSO", "medio": "OK", "alto": "OK", "satura": "ALTO", "muto": "SILENZIO"}
    for name, v in verdicts.items():
        r = run(os.path.join(tmp, name + ".wav"), "--livello")
        check("%s: --livello esce senza errori e non scrive file" % name, r.returncode == 0 and not os.path.exists(os.path.join(tmp, "out_" + name + "_x")))
        check("%s: verdetto %s" % (name, v), ("VERDETTO: " + v) in r.stdout, r.stdout)

    # Registrazione satura: normalizzata verso il basso e avvisa
    out = os.path.join(tmp, "out_satura")
    r = run(os.path.join(tmp, "satura.wav"), out, "--modi", "1200")
    check("satura: avvisa che satura", "satura" in r.stdout and "ALTO" in r.stdout, r.stdout)
    check("satura: il codec la riceve piu' bassa dell'originale", active_db(os.path.join(out, "ingresso-codec.wav")) < -15)

    # Muta: rifiutata
    r = run(os.path.join(tmp, "muto.wav"), os.path.join(tmp, "out_muto"), "--modi", "1200")
    check("muta: rifiutata con errore", r.returncode == 1 and "silenzios" in r.stderr, r.stdout + r.stderr)

    # --grezzo: nessuna normalizzazione, il livello resta quello della registrazione (basso)
    out = os.path.join(tmp, "out_grezzo")
    r = run(os.path.join(tmp, "basso.wav"), out, "--modi", "1200", "--grezzo")
    check("--grezzo: dichiarato", "Senza normalizzazione" in r.stdout, r.stdout)
    check("--grezzo: non scrive ingresso-codec.wav", not os.path.exists(os.path.join(out, "ingresso-codec.wav")))

    # Errori d'uso
    check("senza argomenti: errore d'uso", run().returncode == 2)
    check("bersaglio fuori limiti rifiutato", run(os.path.join(tmp, "medio.wav"), os.path.join(tmp, "x"), "--bersaglio", "0").returncode == 2)

print("%s" % ("tests/codec/test_compare.py: tutto ok" if not failures else "%d falliti" % failures))
sys.exit(1 if failures else 0)
