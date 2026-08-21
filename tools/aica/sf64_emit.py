#!/usr/bin/env python3
"""
SF64 Stage 2: emit adpcm_pool.bin (runtime-loaded) + generated-C descriptor table.

Key = global audio_table offset = sample_bank.rom_addr + sample.addr (the runtime
derives it as patched sampleAddr - audiotable base), same as OoT.

Oversize policy:
  n <= 65534                      : normal hardware voice
  65534 < n <= RESAMPLE_FIT_MAX,
          one-shot                : "fit" -- anti-aliased resample down to one AICA
                                    channel (native HW voice). Tiny inaudible
                                    pitch/tempo shift; avoids the SH4 PCM-ring
                                    streamer (whose per-16-sample G2 store-queue
                                    writes cost ~12x realtime when many play at once).
  otherwise (genuinely long)      : stream=1, full quality, SH4 PCM-ring streamer
"""

import argparse
import math
import os
from multiprocessing import Pool
from pathlib import Path

from sf64_audiobank_parse import parse_all
from transcode import (transcode_sample, resample_to_fit, beam_encode_cached,
                       _HAVE_SCIPY, FORCE_PCM_KEYS, FMT_PCM16, FMT_PCM8, FMT_ADPCM)
import vadpcm
import ya2beam_c

ALIGN = 32
AICA_MAX = 65534
# Only resample-to-fit a one-shot that is *barely* over the channel cap; genuinely
# long samples still stream at full quality. 1.10 => at most ~10% compression
# (<~1.6 semitones worst case; the real SF64 offenders are 1.036x/1.047x).
RESAMPLE_FIT_MAX = int(AICA_MAX * 1.10)


def align_up(n, a):
    return (n + a - 1) & ~(a - 1)


def transcode_streamed(s):
    """Long sample: full-quality Yamaha ADPCM (beam), no decimation (SH4-streamed).
    Always ADPCM -- the SH4 ring decodes it to PCM16 at runtime, so no octave/SNR
    promotion applies (and PCM allows the full pitch range anyway)."""
    pcm = vadpcm.decode(s.data, s.codec, s.order, s.npredictors, s.book)
    adpcm, n = beam_encode_cached(pcm)
    loop_start, loop_end = (s.loop_start, s.loop_end) if s.has_loop else (0, n)
    return {"data": adpcm, "fmt": FMT_ADPCM, "nsamples": n, "loop": bool(s.has_loop),
            "loop_start": loop_start, "loop_end": min(loop_end, n),
            "downsample_shift": 0, "stream": 1}


def transcode_fit(s):
    """Barely-oversize one-shot: anti-aliased resample down to fit one native AICA
    channel (hardware ADPCM voice) instead of SH4-streaming. The runtime plays it
    like any other short ADPCM sample (stream=0). Tiny, inaudible pitch/tempo shift;
    caller guarantees this is only reached for near-cap one-shots."""
    pcm = vadpcm.decode(s.data, s.codec, s.order, s.npredictors, s.book)
    n0 = len(pcm)
    pcm = resample_to_fit(pcm, AICA_MAX)
    adpcm, n = beam_encode_cached(pcm)
    assert n <= AICA_MAX, (s.bank, s.addr, n0, n)
    return {"data": adpcm, "fmt": FMT_ADPCM, "nsamples": n, "loop": bool(s.has_loop),
            "loop_start": 0, "loop_end": n, "downsample_shift": 0, "stream": 0,
            "_fit_from": n0}


def _classify(s):
    n = s.nsamples
    if n <= AICA_MAX:
        return "normal"
    if (not s.has_loop) and n <= RESAMPLE_FIT_MAX:
        return "fit"
    return "stream"


def _transcode_one(item):
    """Pool worker: (Sample, key, mode) -> descriptor (pool_offset later).
    key = src_offset (bank base + addr) = runtime lookup key, used for force-PCM."""
    s, key, mode = item
    if mode == "stream":
        d = transcode_streamed(s)
    elif mode == "fit":
        d = transcode_fit(s)
    else:
        d = transcode_sample(s, force=(key in FORCE_PCM_KEYS))
        d["stream"] = 0
    d["key"] = key
    return d


def main(audiobank, audiotable, tables_json, outdir, incdir, pool_path):
    outdir = Path(outdir); incdir = Path(incdir); pool_path = Path(pool_path)
    tables, samples = parse_all(audiobank, audiotable, tables_json, with_data=True)
    bank_base = {b["index"]: b["rom_addr"] for b in tables["sample_banks"] if not b["is_ptr"]}

    # Beam encode is CPU-heavy + independent -> fan out across cores. Pool.map keeps
    # order; pool_offset assigned serially after, then re-sorted by key. Deterministic.
    items = [(s, bank_base[bank] + addr, _classify(s))
             for (bank, addr), s in samples.items()]
    # Build the C encoder .so once here so forked workers inherit it (no per-worker
    # compile race); prints a notice + falls back to pure-Python if no C compiler.
    if not ya2beam_c.prebuild():
        print("sf64_emit: C beam encoder unavailable; using pure-Python (slow)")
    with Pool(os.cpu_count()) as pool:
        descs = pool.map(_transcode_one, items)

    blob = bytearray()
    for d in descs:
        d["pool_offset"] = len(blob)
        blob += d["data"]
        blob += b"\x00" * (align_up(len(blob), ALIGN) - len(blob))
    n_stream = sum(d["stream"] for d in descs)

    descs.sort(key=lambda d: d["key"])
    keys = [d["key"] for d in descs]
    assert keys == sorted(keys) and len(keys) == len(set(keys)), "keys not sorted/unique"
    for d in descs:
        assert blob[d["pool_offset"]:d["pool_offset"] + len(d["data"])] == d["data"]
        if not d["stream"]:
            assert d["nsamples"] <= AICA_MAX, (d["key"], d["nsamples"])
        assert 0 <= d["loop_start"] <= d["loop_end"] <= d["nsamples"]

    outdir.mkdir(parents=True, exist_ok=True)
    incdir.mkdir(parents=True, exist_ok=True)
    pool_path.parent.mkdir(parents=True, exist_ok=True)
    pool_path.write_bytes(blob)

    h = ["/* Auto-generated by tools/aica/sf64_emit.py. Do not edit. */",
         "#ifndef AICA_SAMPLE_TABLE_H", "#define AICA_SAMPLE_TABLE_H", "#include <stdint.h>", "",
         "typedef struct {",
         "    uint32_t src_offset;       /* global audio_table offset (lookup key) */",
         "    uint32_t pool_offset;      /* byte offset into adpcm_pool.bin */",
         "    uint32_t byte_len;         /* Yamaha ADPCM byte length */",
         "    uint32_t nsamples;         /* PCM sample count (may exceed 65534 if streamed) */",
         "    uint32_t loop_start;",
         "    uint32_t loop_end;",
         "    uint8_t  loop_flag;",
         "    uint8_t  downsample_shift; /* 0, or 1 = 2x-decimated, play at freq>>1 */",
         "    uint8_t  stream;           /* 1 = too long for one channel; SH4-streamed */",
         "    uint8_t  fmt;              /* AICA_SM_* format: 0=PCM16 1=PCM8 2=ADPCM */",
         "} AicaSampleDesc;", "",
         f"#define AICA_SAMPLE_COUNT {len(descs)}",
         f"#define AICA_ADPCM_POOL_SIZE {len(blob)}u",
         "extern const AicaSampleDesc gAicaSampleTable[AICA_SAMPLE_COUNT];", "", "#endif"]
    (incdir / "aica_sample_table.h").write_text("\n".join(h) + "\n")

    c = ["/* Auto-generated by tools/aica/sf64_emit.py. Do not edit. */",
         '#include "aica_sample_table.h"', "",
         "const AicaSampleDesc gAicaSampleTable[AICA_SAMPLE_COUNT] = {"]
    for d in descs:
        c.append(f"    {{ 0x{d['key']:06X}, 0x{d['pool_offset']:06X}, {len(d['data']):>6}, "
                 f"{d['nsamples']:>6}, {d['loop_start']:>6}, {d['loop_end']:>6}, "
                 f"{1 if d['loop'] else 0}, {d['downsample_shift']}, {d['stream']}, {d['fmt']} }},")
    c.append("};")
    (outdir / "aica_sample_table.c").write_text("\n".join(c) + "\n")

    n16 = sum(1 for d in descs if d["fmt"] == FMT_PCM16)
    n8 = sum(1 for d in descs if d["fmt"] == FMT_PCM8)
    nad = sum(1 for d in descs if d["fmt"] == FMT_ADPCM)
    fits = sorted((d for d in descs if d.get("_fit_from")), key=lambda d: d["key"])
    print(f"sf64_emit: {len(descs)} descs  ADPCM={nad} PCM16={n16} PCM8={n8} "
          f"({n_stream} streamed, {len(fits)} fit-resampled), "
          f"pool {len(blob):,} B -> {pool_path}"
          f"{'' if _HAVE_SCIPY else '  [stdlib resample fallback]'}")
    for d in fits:
        n0, n = d["_fit_from"], d["nsamples"]
        cents = 1200.0 * math.log2(n0 / n) if n else 0.0
        print(f"  fit-resampled 0x{d['key']:06X}: {n0} -> {n} samples "
              f"(+{cents:.0f} cents, {100.0 * (1 - n / n0):.1f}% shorter) -> native HW voice")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("--audiobank", required=True)
    ap.add_argument("--audiotable", required=True)
    ap.add_argument("--tables", required=True)
    ap.add_argument("--outdir", required=True)
    ap.add_argument("--incdir")
    ap.add_argument("--pool", required=True)
    a = ap.parse_args()
    main(a.audiobank, a.audiotable, a.tables, a.outdir, a.incdir or a.outdir, a.pool)
