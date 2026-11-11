#!/usr/bin/env python3
"""Ricevitore SDR per VoLoRa: NESDR / RTL-SDR -> pacchetti LoRa -> righe di testo per lcr_decode.

Si esegue nell'ambiente conda con GNU Radio 3.10 e gr-lora_sdr (vedi docs/sdr.md):

    conda activate lora
    python3 tools/sdr/lora_rx.py --freq 433.9e6 --out cattura.txt

I parametri radio predefiniti sono quelli del firmware (src/user_config.h): SF8, BW 250 kHz, CR 4/5, sync word 0x4C,
preambolo 12 simboli, intestazione esplicita, CRC attivo. Per ogni pacchetto scrive una riga
    t=<secondi unix> crc=<0|1> len=<byte> hex=<byte in esadecimale>
che `build/lcr_decode cattura.txt` trasforma in sessioni vocali e prove di collegamento.
"""
import argparse
import os
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from frame_assembler import FrameAssembler, describe_packet  # noqa: E402

try:
    import numpy as np
    import pmt
    from gnuradio import gr
    import gnuradio.lora_sdr as lora_sdr
except ImportError as e:  # pragma: no cover
    sys.exit("Manca GNU Radio o gr-lora_sdr (%s). Attiva l'ambiente: conda activate lora (vedi docs/sdr.md)." % e)


class PacketSink(gr.sync_block):
    """Riceve da crc_verif i byte dei pacchetti con i loro tag e li stampa/salva."""

    def __init__(self, out_path, quiet=False):
        gr.sync_block.__init__(self, name="lcr_packet_sink", in_sig=[np.uint8], out_sig=None)
        self.asm = FrameAssembler()
        self.count = 0
        self.bad_crc = 0
        self.quiet = quiet
        self.f = open(out_path, "a", buffering=1) if out_path else None
        self.key = pmt.intern("frame_info")

    def work(self, input_items, output_items):
        data = input_items[0]
        n = len(data)
        base = self.nitems_read(0)
        for tag in self.get_tags_in_window(0, 0, n, self.key):
            val = tag.value
            nil = pmt.PMT_NIL
            pay_len = pmt.to_long(pmt.dict_ref(val, pmt.intern("pay_len"), nil))
            valid = pmt.to_bool(pmt.dict_ref(val, pmt.intern("crc_valid"), pmt.PMT_F))
            self.asm.add_tag(tag.offset, pay_len, valid)
        self.asm.add_bytes(base, bytes(data))
        for ok, pkt in self.asm.pop_ready():
            self.count += 1
            if not ok:
                self.bad_crc += 1
            line = "t=%.3f crc=%d len=%d hex=%s" % (time.time(), 1 if ok else 0, len(pkt), pkt.hex())
            if self.f:
                self.f.write(line + "\n")
            if not self.quiet:
                print("[%d] %s %s" % (self.count, "CRC ok " if ok else "CRC ERR", describe_packet(pkt)), flush=True)
        return n


class LoraRx(gr.top_block):
    def __init__(self, a):
        gr.top_block.__init__(self, "VoLoRa rx", catch_exceptions=True)
        import osmosdr  # dopo gr.top_block: puo' essere lento

        bw = int(a.bw)
        if a.samp_rate % bw:
            sys.exit("--samp-rate deve essere un multiplo intero della banda (%d Hz)" % bw)
        os_factor = a.samp_rate // bw
        soft = True

        # Sorgente: chiavetta RTL-SDR (NESDR)
        src = osmosdr.source(args="numchan=1 " + a.device)
        src.set_sample_rate(a.samp_rate)
        src.set_center_freq(a.freq, 0)
        src.set_freq_corr(a.ppm, 0)
        src.set_dc_offset_mode(0, 0)
        src.set_iq_balance_mode(0, 0)
        src.set_gain_mode(False, 0)
        src.set_gain(a.gain, 0)
        src.set_if_gain(a.if_gain, 0)
        src.set_bb_gain(a.bb_gain, 0)
        src.set_antenna("", 0)
        src.set_bandwidth(0, 0)
        src.set_min_output_buffer(int(np.ceil(a.samp_rate / bw * (2 ** a.sf + 2))))

        # Ricevitore LoRa (come examples/lora_RX.py di gr-lora_sdr)
        sync = [a.sync_word]
        fsync = lora_sdr.frame_sync(int(a.freq), bw, a.sf, False, sync, os_factor, a.preamble)
        fft = lora_sdr.fft_demod(soft, True)
        gray = lora_sdr.gray_mapping(soft)
        deint = lora_sdr.deinterleaver(soft)
        hamm = lora_sdr.hamming_dec(soft)
        head = lora_sdr.header_decoder(False, a.cr, 255, True, 2, a.print_header)
        dewh = lora_sdr.dewhitening()
        crc = lora_sdr.crc_verif(0, False)
        self.sink = PacketSink(a.out, a.quiet)

        self.msg_connect((head, "frame_info"), (fsync, "frame_info"))
        self.connect(src, fsync)
        self.connect(fsync, fft)
        self.connect(fft, gray)
        self.connect(gray, deint)
        self.connect(deint, hamm)
        self.connect(hamm, head)
        self.connect(head, dewh)
        self.connect(dewh, crc)
        self.connect(crc, self.sink)
        self._keep = (src, fsync, fft, gray, deint, hamm, head, dewh, crc)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--freq", type=float, default=433.9e6, help="frequenza in Hz (predefinita 433.9e6)")
    ap.add_argument("--sf", type=int, default=8, help="spreading factor (predefinito 8)")
    ap.add_argument("--bw", type=float, default=250e3, help="banda in Hz (predefinita 250000)")
    ap.add_argument("--cr", type=int, default=1, help="coding rate 1..4 = 4/5..4/8 (predefinito 1 = 4/5)")
    ap.add_argument("--sync-word", type=lambda s: int(s, 0), default=0x4C, help="sync word (predefinita 0x4C)")
    ap.add_argument("--preamble", type=int, default=12, help="simboli di preambolo (predefinito 12)")
    ap.add_argument("--samp-rate", type=int, default=1000000, help="campionamento in Hz, multiplo della banda (predefinito 1000000)")
    ap.add_argument("--gain", type=float, default=30.0, help="guadagno RF in dB (predefinito 30; riducilo se la scheda e' vicina)")
    ap.add_argument("--if-gain", type=float, default=20.0)
    ap.add_argument("--bb-gain", type=float, default=20.0)
    ap.add_argument("--ppm", type=float, default=0.0, help="correzione di frequenza della chiavetta in ppm")
    ap.add_argument("--device", default="rtl=0", help='argomenti osmosdr, per esempio "rtl=0"')
    ap.add_argument("--out", default="cattura.txt", help="file in cui accodare i pacchetti (\"\" = non salvare)")
    ap.add_argument("--duration", type=float, default=0.0, help="secondi di ascolto (0 = fino a Ctrl+C)")
    ap.add_argument("--print-header", action="store_true", help="stampa l'intestazione LoRa di ogni pacchetto")
    ap.add_argument("--quiet", action="store_true", help="non stampare i pacchetti ricevuti")
    a = ap.parse_args()

    tb = LoraRx(a)
    print("In ascolto su %.4f MHz (SF%d, BW %.0f kHz, CR 4/%d, sync word 0x%02X). Ctrl+C per fermare." %
          (a.freq / 1e6, a.sf, a.bw / 1e3, a.cr + 4, a.sync_word), flush=True)
    tb.start()
    try:
        if a.duration > 0:
            time.sleep(a.duration)
        else:
            while True:
                time.sleep(1)
    except KeyboardInterrupt:
        pass
    tb.stop()
    tb.wait()
    print("\nPacchetti: %d (CRC errato: %d)%s" % (tb.sink.count, tb.sink.bad_crc, "; salvati in " + a.out if a.out else ""))


if __name__ == "__main__":
    main()
