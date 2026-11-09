"""Prova di FrameAssembler e describe_packet (python3 tests/sdr/test_assembler.py). Non serve GNU Radio."""
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "tools", "sdr"))
from frame_assembler import FrameAssembler, describe_packet  # noqa: E402

failures = 0


def check(name, cond):
    global failures
    if not cond:
        failures += 1
        print("FAIL", name)


def feed(asm, stream, tags, chunk):
    """Consegna 'stream' a pezzi di 'chunk' byte, con i tag (offset, lunghezza, crc) nella finestra giusta."""
    out = []
    pos = 0
    while pos < len(stream):
        part = stream[pos:pos + chunk]
        for off, n, ok in tags:
            if pos <= off < pos + len(part):
                asm.add_tag(off, n, ok)
        asm.add_bytes(pos, part)
        out += asm.pop_ready()
        pos += len(part)
    return out


pk = [bytes(range(i, i + 12)) for i in (1, 50, 100)]
stream = b"".join(pk)
tags, off = [], 0
for i, p in enumerate(pk):
    tags.append((off, len(p), i != 1))   # il secondo ha il CRC errato
    off += len(p)

for chunk in (1, 2, 5, 7, 12, 13, 36, 100):
    got = feed(FrameAssembler(), stream, tags, chunk)
    check("tre pacchetti a pezzi di %d byte" % chunk, [g[1] for g in got] == pk)
    check("flag CRC conservati (pezzi da %d)" % chunk, [g[0] for g in got] == [True, False, True])

# nessun tag: nessun pacchetto
check("senza tag non esce nulla", feed(FrameAssembler(), stream, [], 5) == [])

# buco nel flusso: il pacchetto interrotto si perde, i successivi arrivano
asm = FrameAssembler()
asm.add_tag(0, 12, True)
asm.add_bytes(0, pk[0][:6])
check("pacchetto incompleto non esce", asm.pop_ready() == [])
asm.add_tag(100, 12, True)
asm.add_bytes(100, pk[2])
got = asm.pop_ready()
check("dopo il buco esce il pacchetto nuovo", len(got) == 1 and got[0][1] == pk[2])
check("buco contato", asm.lost_bytes > 0)

# descrizione di un pacchetto: START con nominativo, sessione 1234 = 0x04d2
pkt = bytes([0x21, 0x04, 0xD2, 0x00, 0x01 | 0x08, 6]) + b"IU5HJU"
d = describe_packet(pkt)
check("descrizione", "sessione 1234" in d and "START" in d and "IU5HJU" in d and "voce" in d)
check("pacchetto corto", "corto" in describe_packet(b"\x21\x00"))
check("versione sconosciuta", "non e'" in describe_packet(bytes([0x11, 0, 0, 0, 0])))
# START con posizione: 43.7696, 11.2558 (24 bit: lat (43.7696+90)/180*2^24, lon (11.2558+180)/360*2^24)
la, lo = round((43.7696 + 90) / 180 * 2**24), round((11.2558 + 180) / 360 * 2**24)
pos = bytes([1, la >> 16, (la >> 8) & 255, la & 255, lo >> 16, (lo >> 8) & 255, lo & 255])
d = describe_packet(bytes([0x21, 0x04, 0xD2, 0x00, 1 | 8 | 32, 6]) + b"IU5HJU" + pos)
check("posizione descritta", "POSIZIONE" in d and "43.7696" in d and "11.2558" in d and "GPS" in d and "payload 0" in d)
# START di un messaggio con posizione e totale: 80 pacchetti
d = describe_packet(bytes([0x21, 0x04, 0xD2, 0x00, 1 | 8 | 32 | 64, 6]) + b"IU5HJU" + pos + bytes([80]))
check("totale descritto", "TOTALE" in d and "80 pacchetti" in d and "payload 0" in d)
check("prova di collegamento", "prova" in describe_packet(bytes([0x22, 0, 1, 0, 0])))

print("%d falliti" % failures if failures else "tests/sdr/test_assembler.py: tutto ok")
sys.exit(1 if failures else 0)
