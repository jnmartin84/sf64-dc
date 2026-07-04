#!/usr/bin/env python3
"""Strip N64 pipeline-sync commands from Torch-generated display lists (Dreamcast port).

gsDPPipeSync / gsDPTileSync / gsDPLoadSync are pipeline hazards that only matter on
real RDP hardware. The raw-PVR interpreter (src/gfx/gfx_retro_dc.c) has no opcode case
for any of them, so they execute as pure no-ops -- every one is a wasted interpreter
dispatch (and a few bytes) every time its display list is walked. Torch re-dumps ~5k of
them from the baserom into src/assets on every `make assets`, so this pass runs as part
of asset extraction to comment them out of the generated C.

SAFETY -- offset-patched lists are left untouched:
Some display lists are mutated in game code by HARDCODED array index, e.g.
    Gfx *cmd = SEGMENTED_TO_VIRTUAL((void *)((Gfx*)(aBoBaseShieldDL + 2)));
    cmd->words.w0 = (G_SETTILESIZE << 24) | ult;   // texture scroll
Commenting a command out removes that array element at compile time, shifting every
later index -- which would make the patch clobber the wrong opcode and corrupt the
effect (this bites the texture-scroll/animation lists whose gsDPTileSync sits at the
head). We do NOT keep a hand-maintained blocklist; instead we scan the whole source
tree for `(Gfx*)(SYM + N)` references and never strip inside any list so referenced.
The set stays correct automatically as scroll lists are added or removed.

Idempotent: already-commented syncs are skipped. Only bare `gsDP*Sync(),` statements
are touched; anything else on a line is left alone.

Usage: strip_dc_syncs.py [REPO_ROOT]   (default: current directory)
"""
import os
import re
import sys

SYNC_RE = re.compile(r'^(\s*)(gsDP(Pipe|Tile|Load)Sync\(\),)\s*$')
ARRAY_START_RE = re.compile(r'^\s*(?:static\s+)?Gfx\s+([A-Za-z_]\w*)\s*\[\s*\]\s*=')
ARRAY_END_RE = re.compile(r'^\s*\}\s*;')
# `(Gfx*)(SYM + N)` and `(Gfx *) SYM + N` (optional inner paren, any spacing)
OFFSET_RE = re.compile(r'\(\s*Gfx\s*\*\s*\)\s*\(?\s*([A-Za-z_]\w*)\s*\+\s*\d+')


def build_exclusions(src_dir):
    excl = set()
    for dirpath, _, files in os.walk(src_dir):
        for fn in files:
            if not fn.endswith(('.c', '.h', '.inc.c')):
                continue
            path = os.path.join(dirpath, fn)
            with open(path, encoding='utf-8', errors='replace') as f:
                for m in OFFSET_RE.finditer(f.read()):
                    excl.add(m.group(1))
    return excl


def strip_file(path, excl, counts):
    with open(path, encoding='utf-8', errors='replace') as f:
        lines = f.readlines()

    cur = None
    changed = False
    out = []
    for line in lines:
        start = ARRAY_START_RE.match(line)
        if start:
            cur = start.group(1)

        sync = SYNC_RE.match(line)
        if sync and cur is not None and cur not in excl:
            out.append('%s//%s\n' % (sync.group(1), sync.group(2)))
            counts[sync.group(3)] = counts.get(sync.group(3), 0) + 1
            changed = True
            continue

        if ARRAY_END_RE.match(line):
            cur = None
        out.append(line)

    if changed:
        with open(path, 'w', encoding='utf-8') as f:
            f.writelines(out)
    return changed


def main():
    root = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else '.')
    src_dir = os.path.join(root, 'src')
    assets_dir = os.path.join(src_dir, 'assets')
    if not os.path.isdir(assets_dir):
        print('strip_dc_syncs: no %s, nothing to do' % assets_dir)
        return 0

    excl = build_exclusions(src_dir)

    counts = {}
    files_changed = 0
    for dirpath, _, files in os.walk(assets_dir):
        for fn in files:
            if not (fn.endswith('.c') or fn.endswith('.inc.c')):
                continue
            if strip_file(os.path.join(dirpath, fn), excl, counts):
                files_changed += 1

    total = sum(counts.values())
    print('strip_dc_syncs: commented %d syncs (Pipe=%d Tile=%d Load=%d) in %d files; '
          'kept %d offset-patched list(s): %s'
          % (total, counts.get('Pipe', 0), counts.get('Tile', 0), counts.get('Load', 0),
             files_changed, len(excl), ', '.join(sorted(excl)) if excl else '(none)'))
    return 0


if __name__ == '__main__':
    sys.exit(main())
