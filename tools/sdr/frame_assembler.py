"""Riassembla i pacchetti LoRa dal flusso di byte e dai tag di gr-lora_sdr (senza dipendere da GNU Radio).

Il blocco `crc_verif` di gr-lora_sdr emette, per ogni pacchetto, esattamente `pay_len` byte consecutivi e mette sul
primo byte un tag `frame_info` con `pay_len` e `crc_valid`. FrameAssembler riceve i byte (con il loro offset assoluto nel
flusso) e i tag, e restituisce i pacchetti completi anche quando arrivano a pezzi, in chiamate diverse.
"""


class FrameAssembler:
    def __init__(self):
        self._buf = bytearray()
        self._base = 0          # offset assoluto del primo byte di _buf
        self._frames = []       # (offset, lunghezza, crc_valido), in ordine
        self.lost_bytes = 0     # byte scartati per buchi nel flusso

    def add_tag(self, offset, pay_len, crc_valid):
        if pay_len > 0:
            self._frames.append((int(offset), int(pay_len), bool(crc_valid)))

    def add_bytes(self, offset, data):
        offset = int(offset)
        end = self._base + len(self._buf)
        if not self._buf and not self._frames:
            self._base = offset
        elif offset != end:
            # buco o sovrapposizione nel flusso: si riparte da qui
            self.lost_bytes += abs(offset - end)
            self._buf = bytearray()
            self._base = offset
            self._frames = [f for f in self._frames if f[0] >= offset]
        self._buf += bytes(data)

    def pop_ready(self):
        """Pacchetti completi: lista di (crc_valido, bytes)."""
        out = []
        while self._frames:
            off, n, ok = self._frames[0]
            if off < self._base:                      # tag di un pacchetto di cui abbiamo perso l'inizio
                self._frames.pop(0)
                continue
            start = off - self._base
            if start + n > len(self._buf):
                break
            out.append((ok, bytes(self._buf[start:start + n])))
            self._frames.pop(0)
            del self._buf[:start + n]
            self._base = off + n
        return out


def describe_packet(data):
    """Riga leggibile di un pacchetto LoRa-Codec-2-RTX (formato in docs/protocollo.md)."""
    if len(data) < 5:
        return "pacchetto troppo corto (%d byte)" % len(data)
    version, mode = data[0] >> 4, data[0] & 0x0F
    session = (data[1] << 8) | data[2]
    seq, flags = data[3], data[4]
    if version != 2:
        return "non e' un pacchetto del progetto (versione %d)" % version
    names = [n for bit, n in ((1, "START"), (2, "END"), (4, "PARITA'"), (8, "NOMINATIVO"), (16, "NOFEC"), (32, "POSIZIONE"), (64, "TOTALE")) if flags & bit]
    modes = {1: "voce Codec2 1200", 2: "prova di collegamento", 3: "risposta alla prova"}
    pos, call = 5, ""
    if flags & 8 and len(data) > 5:
        n = data[5]
        call = data[6:6 + n].decode("ascii", "replace")
        pos = 6 + n
    if flags & 32 and len(data) >= pos + 7:
        la = (data[pos + 1] << 16) | (data[pos + 2] << 8) | data[pos + 3]
        lo = (data[pos + 4] << 16) | (data[pos + 5] << 8) | data[pos + 6]
        position = " (%.4f, %.4f%s)" % (la / 2**24 * 180 - 90, lo / 2**24 * 360 - 180, " GPS" if data[pos] & 1 else "")
        pos += 7
    else:
        position = ""
    total = ""
    if flags & 64 and len(data) > pos:
        total = ", %d pacchetti in tutto" % data[pos]
        pos += 1
    payload = max(0, len(data) - pos)
    txt = "%s, sessione %d, seq %d, flag %s" % (modes.get(mode, "modo %d" % mode), session, seq, "+".join(names) or "-")
    if call:
        txt += ", da %s%s" % (call, position)
    txt += total
    return txt + ", payload %d byte" % payload
