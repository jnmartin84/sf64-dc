#!/usr/bin/env python3
"""
Shared Audiobank parser: enumerate + dedup every sample referenced by the
soundfonts, resolving each to its sample bank. Self-contained -- needs only
aicaseq/data/audio_tables.json + the game .bin files.
"""

import json
import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent          # n64_aica_sequencer/
DATA = Path(__file__).resolve().parent.parent / "data"

CODEC_NAMES = {0: "ADPCM", 1: "S8", 2: "S16_MEM", 3: "SMALL_ADPCM", 4: "REVERB", 5: "S16"}
FRAME = {0: (16, 9), 3: (16, 5)}    # (samples, bytes) per compressed frame


def be32(b, o):
    return struct.unpack_from(">I", b, o)[0]


class Sample:
    __slots__ = ("bank", "addr", "size", "codec", "has_loop", "loop_start",
                 "loop_end", "loop_count", "nsamples", "fonts", "tunings",
                 "order", "npredictors", "book", "data")

    def __init__(self, bank, addr, size, codec, loop, book, data):
        self.bank, self.addr, self.size, self.codec = bank, addr, size, codec
        if loop is None:
            self.has_loop, self.loop_start, self.loop_end, self.loop_count = False, 0, 0, 0
        else:
            self.loop_start, self.loop_end, self.loop_count = loop
            self.has_loop = self.loop_count != 0
        if codec in FRAME:
            smp_per, byte_per = FRAME[codec]
            self.nsamples = (size // byte_per) * smp_per
        else:
            self.nsamples = None
        self.order, self.npredictors, self.book = book   # (order, npredictors, tuple coeffs)
        self.data = data                                  # raw VADPCM bytes
        self.fonts = set()
        self.tunings = set()


def _parse_font(ab, base, size, n_inst, n_drums, n_sfx):
    data = ab[base:base + size]
    refs = []   # (sample_header_offset, tuning)

    def sound_at(o):
        samp = be32(data, o)
        tuning = struct.unpack_from(">f", data, o + 4)[0]
        if samp != 0 and tuning != 0.0:
            refs.append((samp, tuning))

    # SF64 soundfont layout (per AudioLoad_RelocateFont): fontDataPtrs[0] = drum
    # list, instruments at fontDataPtrs[1..numInstruments] (offset 4 + 4*i), and
    # NO sfx array -- differs from OoT (sfx_list@4, instruments@8).
    drum_list = be32(data, 0)
    for i in range(n_inst):
        ip = be32(data, 4 + 4 * i)
        if ip == 0:
            continue
        sound_at(ip + 0x08)
        sound_at(ip + 0x10)
        sound_at(ip + 0x18)
    for i in range(n_drums):
        dp = be32(data, drum_list + 4 * i) if drum_list else 0
        if dp == 0:
            continue
        sound_at(dp + 0x04)
    return data, refs


def _read_sample_header(data, off):
    bits = be32(data, off)
    codec = (bits >> 28) & 0xF
    medium = (bits >> 26) & 0x3
    size = bits & 0xFFFFFF
    addr = be32(data, off + 4)
    loop_off = be32(data, off + 8)
    book_off = be32(data, off + 12)
    return codec, medium, size, addr, loop_off, book_off


def _read_loop(data, off):
    start, end, count, _nf = struct.unpack_from(">IIII", data, off)
    return (start, end, count)


def _read_book(data, off):
    order, npred = struct.unpack_from(">ii", data, off)
    n = 8 * order * npred
    coeffs = struct.unpack_from(f">{n}h", data, off + 8)
    return order, npred, coeffs


def load_tables(tables_json):
    with open(tables_json) as f:
        return json.load(f)


def parse_all(audiobank_path, audiotable_path, tables_json, with_data=False):
    tables = load_tables(tables_json)
    with open(audiobank_path, "rb") as f:
        ab = f.read()
    at = None
    if with_data:
        with open(audiotable_path, "rb") as f:
            at = f.read()
    bank_base = {b["index"]: b["rom_addr"] for b in tables["sample_banks"] if not b["is_ptr"]}
    samples = {}
    for sf in tables["soundfonts"]:
        bank = sf["bank_normal"]
        data, refs = _parse_font(ab, sf["rom_addr"], sf["size"],
                                 sf["num_instruments"], sf["num_drums"], sf["num_sfx"])
        for hdr_off, tuning in refs:
            codec, medium, size, addr, loop_off, book_off = _read_sample_header(data, hdr_off)
            assert medium == 0, (sf["index"], medium)
            loop = _read_loop(data, loop_off) if loop_off else None
            book = _read_book(data, book_off)
            key = (bank, addr)
            if key in samples:
                s = samples[key]
                assert s.size == size and s.codec == codec, (key, s.size, size)
            else:
                raw = None
                if with_data:
                    base = bank_base[bank] + addr
                    raw = at[base:base + size]
                s = Sample(bank, addr, size, codec, loop, book, raw)
                samples[key] = s
            s.fonts.add(sf["index"])
            s.tunings.add(tuning)
    return tables, samples
