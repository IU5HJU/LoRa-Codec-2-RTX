#!/usr/bin/env python3
"""Controlla la tabella delle partizioni compilata (partitions.bin) prima di pubblicare un firmware.

Le impostazioni dell'operatore stanno nella partizione nvs (0x9000, 0x5000 byte) e il flasher della pagina scrive solo bootloader, tabella,
otadata e programma: se una tabella nuova spostasse nvs o otadata, ogni aggiornamento cancellerebbe le impostazioni (o romperebbe l'avvio).
Questo controllo fa fallire la compilazione in quel caso. Il programma (app0) parte sempre da 0x10000.

    check_partitions.py <partitions.bin>
"""
import struct
import sys

ENTRY = struct.Struct("<HBBLL16sL")  # magic, tipo, sottotipo, indirizzo, dimensione, nome, flag
MAGIC = 0x50AA
TYPE_APP, TYPE_DATA = 0x00, 0x01
SUB_OTA0, SUB_OTADATA, SUB_NVS = 0x10, 0x00, 0x02

EXPECTED = {
    "nvs": (TYPE_DATA, SUB_NVS, 0x9000, 0x5000),
    "otadata": (TYPE_DATA, SUB_OTADATA, 0xE000, 0x2000),
}


def parse(data):
    parts = {}
    for i in range(0, len(data) - ENTRY.size + 1, ENTRY.size):
        magic, ptype, sub, off, size, name, _flags = ENTRY.unpack_from(data, i)
        if magic != MAGIC:
            break
        parts[name.split(b"\0")[0].decode("ascii", "replace")] = (ptype, sub, off, size)
    return parts


def check(data):
    """Lista dei problemi (vuota = tutto a posto)."""
    parts = parse(data)
    errs = []
    for name, want in EXPECTED.items():
        got = parts.get(name)
        if got is None:
            errs.append("manca la partizione %s" % name)
        elif got != want:
            errs.append("la partizione %s e' (tipo %#x, sottotipo %#x, indirizzo %#x, dimensione %#x), deve essere (%#x, %#x, %#x, %#x): "
                        "le impostazioni salvate non resterebbero al loro posto" % ((name,) + got + want))
    app0 = parts.get("app0")
    if app0 is None or app0[0] != TYPE_APP or app0[1] != SUB_OTA0:
        errs.append("manca la partizione app0 (programma)")
    elif app0[2] != 0x10000:
        errs.append("app0 parte da %#x, deve partire da 0x10000 (e' li' che il flasher scrive il programma)" % app0[2])
    return errs, parts


def main():
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    errs, parts = check(open(sys.argv[1], "rb").read())
    for n, (t, s, o, z) in sorted(parts.items(), key=lambda kv: kv[1][2]):
        print("%-9s tipo %#04x sottotipo %#04x indirizzo %#08x dimensione %#08x (%d KB)" % (n, t, s, o, z, z // 1024))
    if errs:
        for e in errs:
            print("ERRORE:", e)
        return 1
    print("tabella delle partizioni ok: nvs e otadata al loro posto, programma da 0x10000 (%d KB)" % (parts["app0"][3] // 1024))
    return 0


if __name__ == "__main__":
    sys.exit(main())
