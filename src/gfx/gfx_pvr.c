// gfx_pvr.c — raw-KOS-PVR backend for the Star Fox 64 DC renderer.
//
// The renderer backend: implements struct GfxRenderingAPI and submits geometry
// straight to the PVR via the direct-render (store-queue) path. The gfx_retro_dc.c
// front-end (command interpreter, matrix lighting, texgen, palette-id CI4 cache,
// SW clip/cull/scissor) is shared; this file owns everything below the vtable.
//
// sys_main.c wires gfx_pvr_api as the rendering API; the PVR flips the frame inside
// pvr_scene_finish (gfx_pvr_finish_render), not via glKosSwapBuffers.
//
// SINGLE-TILE: SF64's combiner only ever uses texture tile 0 (the two-tile shape was
// vestigial inheritance from the 2020 ancestor — see memory project_sf64_raw_pvr_port).
// The rapi vtable was stripped to single-tile (select_texture(id), shader_get_info returns
// the used-texture flag, set_sampler_parameters(linear,cms,cmt)); this backend matches.
//
// STAGE 1 (current): full backend machinery + build-wired, so the PVR scene lifecycle runs
// and the screen clears. The draw paths are present but the SHARED front-end still emits
// object-space verts through GLdc-era code until the S2/S3 seams (screen-bake + combiner
// eval + OP/PT/TR classify) land; only this backend changes here.

// This is the sole renderer backend; the Makefile compiles it directly into every build.

#include <PR/gbi.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <kos.h>
#include <dc/video.h>

#include "gfx_cc.h"
#include "gfx_rendering_api.h"
#include "macros.h"

// The front-end bakes screen-space vertices straight into KOS pvr_vertex_t, so the DR/bucket
// submit path is a plain struct copy — no reinterpret, no layout assert. The poly header is
// written into a single 32-byte store-queue slot; fail at build time if that ever changes.
_Static_assert(sizeof(pvr_poly_hdr_t) == 32, "pvr_poly_hdr_t must be one 32-byte SQ slot");

// ---------------------------------------------------------------------------
// Shader bookkeeping — identical bookkeeping to the GLdc backend. The front-end
// treats ShaderProgram opaquely (only via shader_get_info), so this layout is
// private to the backend. The PVR poly config is derived per-draw from the CC
// features + texenv (set by the front-end), not cached here.
// ---------------------------------------------------------------------------
enum MixType {
    SH_MT_NONE,
    SH_MT_TEXTURE,
    SH_MT_COLOR,
    SH_MT_TEXTURE_COLOR,
    SH_MT_COLOR_COLOR,
};

struct ShaderProgram {
    uint8_t enabled;
    uint32_t shader_id;
    struct CCFeatures cc;
    enum MixType mix;
    uint8_t texture_used;   // SF64 single-tile: tile 0 only
    int num_inputs;
};

static struct ShaderProgram shader_program_pool[64];
static uint8_t shader_program_pool_size = 0;
static struct ShaderProgram *cur_shader = NULL;

// ---------------------------------------------------------------------------
// PVR state
// ---------------------------------------------------------------------------
static pvr_dr_state_t sDrState;
static int sCurrentList = PVR_LIST_OP_POLY;

// Per-primitive depth state, driven by the front-end (set_depth_test/mask) straight from the
// RDP other_mode bits — NOT magic shader ids. PVR bakes depth state into the compiled poly
// header, so when any of these change we mark the header dirty and lazily recompile+submit a
// fresh one before the next vertices.
static uint8_t sDepthTest  = 1;   // PVR_DEPTHCMP_GEQUAL vs ALWAYS
static uint8_t sDepthWrite = 1;   // PVR_DEPTHWRITE_ENABLE vs DISABLE
// (zmode decal is NOT header state — handled as a baked-z nudge in the front-end; see
//  gfx_pvr_set_zmode_decal + PVR_DECAL_ZBIAS in gfx_retro_dc.c)
static uint8_t sTexEnv     = PVR_TXRENV_MODULATE;  // texel<->vtx combine, derived from N64 combiner
static uint8_t sFogEnabled = 0;   // PVR_FOG_VERTEX vs PVR_FOG_DISABLE (G_FOG geometry mode)

// --- Texture table ---------------------------------------------------------
// The front-end hands upload_texture data ALREADY in PVR 16-bit format (ARGB1555 or
// ARGB4444 — SF64 decodes straight to rgba16_buf). There is no pixel conversion: we POT-PAD
// (never resample — see memory), copy into VRAM, and the front-end's get_*_scale UV
// correction addresses the used sub-region. Ids are 1-based table indices; 0 == none.
#define PVR_TEX_MAX 1024
struct PvrTex {
    pvr_ptr_t addr;        // VRAM, NULL if unallocated
    uint32_t  alloc_size;  // bytes currently allocated (reused if a re-upload fits)
    uint16_t  w, h;        // padded POT dims
    int       fmt;         // PVR_TXRFMT_ARGB1555/4444 | NONTWIDDLED
    uint8_t   filter;      // 0 = point, 1 = bilinear
    uint8_t   clampu, clampv;   // G_TX_CLAMP -> PVR_UVCLAMP
    uint8_t   flipu, flipv;     // G_TX_MIRROR -> PVR_UVFLIP (mirror-repeat)
    uint8_t   opaque;           // 1 = every texel fully opaque (computed inline at convert); lets the
                                // 2D-backdrop path route an effectively-opaque XLU quad to the OP list
    float     u_scale, v_scale; // real/padded, consumed by gfx_pvr_get_*_scale
};
static struct PvrTex sTextures[PVR_TEX_MAX];
// SIX base poly headers — {colour, textured} x {OP, PT, TR} — each compiled ONCE by the official
// pvr_poly_cxt+pvr_poly_compile, so every PER-LIST field (list_type/alpha/txralpha/blend defaults)
// is correct. pvr_compile_header copies the matching base, then bit-patches the texture fields
// (addr/dims/fmt/filter/clamp/flip/env) + per-draw state (depth/fog/TR-blend). No pvr_poly_compile
// per draw.
static pvr_poly_hdr_t __attribute__((aligned(32))) sBaseHdr[2][3];   // [textured][PVR_KIND_*]
static uint8_t sBaseHdrValid[2][3];
static uint32_t sTexCount = 0;   // high-water id allocator
static uint32_t sBoundTex = 0;   // currently bound texture (SF64 single-tile)
static uint32_t sCurBound = 0;   // last select_texture id == upload target
// Per-draw texturing INTENT, set by the front-end (gfx_pvr_set_textured) = its real use_texture
// decision. The header MUST follow this, not cur_shader->texture_used + a stale sBoundTex: the 2D
// fill path (do_ext_fill) sets use_texture=0 while the shader still reports textured and a valid
// texture is still bound, which would otherwise draw an untextured fill wearing the prior texture.
static uint8_t sDrawTextured = 0;

// --- OP live + PT/TR deferred -----------------------------------------------
// Only ONE list can be open on the DR path at a time, so the OP (opaque) list stays open and
// LIVE the whole frame — geometry the front-end can GUARANTEE is fully opaque (pvr_submit_op)
// streams straight out as it arrives. The other two kinds are buffered into per-kind buckets and
// replayed at end_frame, each into its own list opened once:
//   * PT (sPunch): alpha-TEST cutouts (hard-edged glyphs/foliage). Opaque pixels, depth-tested
//     AND depth-written, alpha-tested by the hardware. Flushed FIRST.
//   * TR (sTR): real alpha-BLEND surfaces. Depth-tested, autosorted per-pixel (pvr_init),
//     composited over OP+PT. Flushed SECOND, so it blends over the cutouts.
// The front-end classifies each primitive (gfx_pvr_set_blend -> sListKind) into OP/PT/TR.
// Caps raised for SF64's transparent-heavy scenes (the faked "transparent colored surface"
// workarounds flood the TR list). PVR_DROP batch/verts logs fire when these are exceeded — geometry
// past the cap is silently dropped, so keep them ahead of the worst frame. Cost is static RAM:
// verts = TR_MAX_VERTS*32B *2 buckets; batch = TR_MAX_BATCH*~64B *2 buckets. (16384/2048 ≈ 1.3MB.)
#define TR_MAX_VERTS    8192
#define TR_MAX_BATCH     1024

typedef struct { pvr_poly_hdr_t __attribute__((aligned(32))) hdr; uint32_t start, count; } PvrBatch;
typedef struct {
    pvr_vertex_t *verts; PvrBatch *batch;
    uint32_t nverts, max_verts;
    int nbatch, max_batch, cur;
    uint8_t dirty;   // state changed since this bucket's current batch was compiled
} PvrBucket;

static pvr_vertex_t sTrVerts[TR_MAX_VERTS] __attribute__((aligned(32)));
static PvrBatch     sTrBatch[TR_MAX_BATCH];
static PvrBucket sTR = { sTrVerts, sTrBatch, 0, TR_MAX_VERTS, 0, TR_MAX_BATCH, -1, 1 };

// Deferred PUNCH-THROUGH bucket: alpha-test cutouts (hard-edged, opaque pixels, write
// depth). Flushed at end_frame BEFORE TR.
static pvr_vertex_t sPunchVerts[TR_MAX_VERTS] __attribute__((aligned(32)));
static PvrBatch     sPunchBatch[TR_MAX_BATCH];
static PvrBucket sPunch = { sPunchVerts, sPunchBatch, 0, TR_MAX_VERTS, 0, TR_MAX_BATCH, -1, 1 };

// Routing for the current primitive: 0 = OP (live), 1 = PT (sPunch bucket), 2 = TR (sTR).
#define PVR_KIND_OP 0
#define PVR_KIND_PT 1
#define PVR_KIND_TR 2
static uint8_t sListKind = PVR_KIND_OP;
static uint8_t sOpDirty = 1;   // live OP header needs re-emit (depth/texture changed)

// TR blend factors, derived by the front-end from the N64 blender and pushed via
// gfx_pvr_set_blend_factors. Default = standard alpha-over (SRC_ALPHA / INV_SRC_ALPHA).
static int sBlendSrc = PVR_BLEND_SRCALPHA;
static int sBlendDst = PVR_BLEND_INVSRCALPHA;

// Capped, greppable overflow logging — never drop silently.
static int sDropV = 0, sDropB = 0;

// A depth/texture/shader state change affects the live OP header and the next PT and TR
// batches. (gfx_pvr_set_blend only routes OP/PT/TR, so it does NOT dirty headers.)
static void pvr_mark_dirty(void) { sOpDirty = 1; sTR.dirty = 1; sPunch.dirty = 1; }

// Compile one of the SIX base headers ONCE via the official pvr_poly_cxt+pvr_poly_compile, so
// every PER-LIST field (list_type/alpha/txralpha/blend defaults) is correct for this list. The
// textured bases seed from whatever texture is bound on first use; their texture-specific fields
// get overwritten per draw, so the seed texture is irrelevant.
static void pvr_ensure_base(int textured, int kind, int list) {
    if (sBaseHdrValid[textured][kind]) return;
    pvr_poly_cxt_t cxt;
    if (textured) {
        struct PvrTex *t = &sTextures[sBoundTex];
        pvr_poly_cxt_txr(&cxt, list, t->fmt, t->w, t->h, t->addr,
                         t->filter ? PVR_FILTER_BILINEAR : PVR_FILTER_NONE);
    } else {
        pvr_poly_cxt_col(&cxt, list);
    }
    // SMALL (not NONE): the front-end already does N64 backface culling (G_CULL_* in gfx_sp_tri1),
    // so HW culling isn't needed for winding — but SMALL also discards degenerate / near-zero-area
    // triangles the front-end lets through, which otherwise produce visible degen artifacts.
    cxt.gen.culling  = PVR_CULLING_SMALL;
    cxt.gen.specular = PVR_SPECULAR_ENABLE;  // combiner routes additive offsets into oargb
    pvr_poly_compile(&sBaseHdr[textured][kind], &cxt);
    sBaseHdrValid[textured][kind] = 1;
}

// Build a poly header for the current state. pvr_poly_compile runs only ONCE per base (6 total);
// per draw this copies the matching base and bit-patches the texture fields + per-draw state in
// place (OoT technique) — no per-draw recompile. Per-list state stays from the base.
static void pvr_compile_header(pvr_poly_hdr_t *out, int kind) {
    int list = (kind == PVR_KIND_TR) ? PVR_LIST_TR_POLY
             : (kind == PVR_KIND_PT) ? PVR_LIST_PT_POLY
             :                         PVR_LIST_OP_POLY;
    // Follow the front-end's per-draw intent (sDrawTextured), NOT cur_shader->texture_used — they
    // diverge on 2D fills (do_ext_fill). Still require a valid bound texture as a safety net.
    int textured = sDrawTextured && sBoundTex && sTextures[sBoundTex].addr;
    pvr_ensure_base(textured, kind, list);
    *out = sBaseHdr[textured][kind];

    if (textured) {
        struct PvrTex *t = &sTextures[sBoundTex];
        // Texture fields, encoded exactly as KOS pvr_poly_compile (pvr_prim.c): mode3 =
        // format | ((addr & 0x00fffff8) >> 3); mode2 USIZE/VSIZE = ctz(dim)-3; filter/clamp/flip.
        out->mode3 = (uint32_t) t->fmt |
                     ((((uint32_t)(uintptr_t) t->addr) & 0x00fffff8u) >> 3);
        out->m2.u_size      = (pvr_uv_size_t)(__builtin_ctz(t->w) - 3);
        out->m2.v_size      = (pvr_uv_size_t)(__builtin_ctz(t->h) - 3);
        out->m2.filter_mode = t->filter ? PVR_FILTER_BILINEAR : PVR_FILTER_NONE;
        out->m2.u_clamp     = t->clampu;
        out->m2.v_clamp     = t->clampv;
        out->m2.u_flip      = t->flipu;
        out->m2.v_flip      = t->flipv;
        // texenv DERIVED from the N64 combiner (REPLACE/MODULATE/DECAL/MODULATEALPHA).
        out->m2.shading     = (pvr_txr_shading_mode_t) sTexEnv;
    }
    // Per-draw state (per-list alpha/txralpha/blend defaults stay from the base):
    // z = 1/w (larger == nearer): GEQUAL keeps the nearer fragment; ALWAYS == test off.
    out->m1.depth_cmp       = sDepthTest  ? PVR_DEPTHCMP_GEQUAL : PVR_DEPTHCMP_ALWAYS;
    out->m1.depth_write_dis = sDepthWrite ? 0 : 1;
    // PVR vertex fog reads density from per-vertex oargb ALPHA (front-end packs it); off clears.
    out->m2.fog_type        = sFogEnabled ? PVR_FOG_VERTEX : PVR_FOG_DISABLE;
    if (kind == PVR_KIND_TR) {
        // Only TR overrides blend (factors DERIVED from the N64 blender). OP/PT keep the base's
        // per-list defaults (ONE/ZERO and SRCALPHA/INVSRCALPHA).
        out->m2.blend_src = (pvr_blend_mode_t) sBlendSrc;
        out->m2.blend_dst = (pvr_blend_mode_t) sBlendDst;
    }
}

// Append n vertices (n/3 tris) of already-screen-baked pvr_vertex_t to a bucket,
// starting a new batch when the bucket's header state has changed.
static void pvr_append(PvrBucket *b, int kind, const pvr_vertex_t *tris, size_t n) {
    if (b->dirty || b->cur < 0) {
        if (b->nbatch >= b->max_batch) { if (sDropB++ < 8) printf("PVR_DROP batch %d\n", kind); return; }
        b->cur = b->nbatch++;
        pvr_compile_header(&b->batch[b->cur].hdr, kind);
        b->batch[b->cur].start = b->nverts;
        b->batch[b->cur].count = 0;
        b->dirty = 0;
    }
    if (b->nverts + n > b->max_verts) { if (sDropV++ < 8) printf("PVR_DROP verts %d\n", kind); return; }
    for (size_t i = 0; i < n; i++) {
        pvr_vertex_t *v = &b->verts[b->nverts++];
        *v = tris[i];
        v->flags = ((i % 3) == 2) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        // oargb (additive offset) is carried through in the copy — do not zero.
    }
    b->batch[b->cur].count += n;
}

// Reserve n verts in the deferred bucket for `kind` (1=PT/sPunch, 2=TR/sTR) and return a write
// pointer into b->verts so the FRONT-END bakes vertices straight into the bucket — no buf_vbo, no
// copy. Starts a new batch (header compiled from current state) when the bucket is dirty, exactly
// like pvr_append. These are TRIANGLES (n == n_tris*3), so EOL on every 3rd vertex. NULL on overflow.
pvr_vertex_t *pvr_reserve(int kind, size_t n) {
    PvrBucket *b = (kind == PVR_KIND_TR) ? &sTR : &sPunch;
    if (b->dirty || b->cur < 0) {
        if (b->nbatch >= b->max_batch) { if (sDropB++ < 8) printf("PVR_DROP batch %d\n", kind); return NULL; }
        b->cur = b->nbatch++;
        pvr_compile_header(&b->batch[b->cur].hdr, kind);
        b->batch[b->cur].start = b->nverts;
        b->batch[b->cur].count = 0;
        b->dirty = 0;
    }
    if (b->nverts + n > b->max_verts) { if (sDropV++ < 8) printf("PVR_DROP verts %d\n", kind); return NULL; }
    pvr_vertex_t *out = &b->verts[b->nverts];
    b->nverts += n;
    b->batch[b->cur].count += n;
    for (size_t i = 0; i < n; i++)
        out[i].flags = ((i % 3) == 2) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
    return out;
}

// Replay a bucket's batches (header + its verts) to the open list via the DR path.
static void pvr_flush_bucket(PvrBucket *b) {
    for (int i = 0; i < b->nbatch; i++) {
        PvrBatch *bt = &b->batch[i];
        if (!bt->count) continue;
        pvr_poly_hdr_t *hp = (pvr_poly_hdr_t *) pvr_dr_target(sDrState);
        *hp = bt->hdr;
        pvr_dr_commit(hp);
        // Verts are already contiguous in the bucket with flags stamped at build time (3-vert tris
        // by pvr_reserve, 4-vert quads by gfx_pvr_draw_triangles_2d), so ship the whole run to the
        // TA FIFO in one store-queue copy instead of a per-vertex dr_target/dr_commit dance.
        sq_fast_cpy(SQ_MASK_DEST(PVR_TA_INPUT), &b->verts[bt->start], bt->count);
    }
}

// Scratch for POT padding (max 256x256 @ 16bpp = 128KB).
static uint16_t sPadScratch[256 * 256] __attribute__((aligned(32)));

static inline uint32_t pvr_next_pot(uint32_t v) {
    v--; v |= v>>1; v |= v>>2; v |= v>>4; v |= v>>8; v |= v>>16; return v+1;
}
static inline int pvr_is_pot(uint32_t v) { return (v & (v - 1)) == 0; }

// Pad-copy with clamp-to-edge (mirrors GLdc's resample_tex): real image at top-left,
// pad columns/rows replicate the edge so bilinear at the border samples real texels.
static void pvr_pad16(const uint16_t *in, int iw, int ih, uint16_t *out, int ow, int oh) {
    int y;
    (void)oh;
    for (y = 0; y < ih; y++) {
        uint16_t *o = out + (y * ow);
        memcpy(o, in + (y * iw), iw * 2);
        uint16_t edge = (iw > 0) ? in[(y * iw) + iw - 1] : 0;
        for (int x = iw; x < ow; x++) o[x] = edge;
    }
    if (ih > 0) {
        const uint16_t *last = out + ((ih - 1) * ow);
        for (; y < oh; y++) memcpy(out + (y * ow), last, ow * 2);
    }
}

// 640x480, matches gfx_dc.c / gfx_screen_config.
#define DC_FB_W 640
#define DC_FB_H 480

// Mirror of OoT's known-good params for this toolchain:
//   {OP, OP_MOD, TR, TR_MOD, PT} bin sizes, vtxbuf, dma, fsaa, autosort_disabled, overflow
// Live list is OP. Alpha-TEST CUTOUTS go to the deferred PT bucket; real alpha-BLEND surfaces go
// to the deferred TR bucket. At end_frame PT is flushed first (writes depth, alpha-tested opaque),
// then TR composites over it. All three lists' bins are live -> {OP, OP_MOD, TR, TR_MOD, PT}.
static pvr_init_params_t sPvrParams = {
    { PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_32, PVR_BINSIZE_0, PVR_BINSIZE_32 },
    768 * 1024,
    1,  // dma
    0,  // fsaa
    0,  // autosort_disabled (0 = autosort on; needed for TR)
    3,  // overflow buffers
    0,  // vbuf_doublebuf_disabled (0 = double-buffering on)
};

// ===========================================================================
// GfxRenderingAPI implementation
// ===========================================================================

static uint8_t gfx_pvr_z_is_from_0_to_1(void) {
    // Matches the GLdc backend's value. PVR uses 1/w depth regardless.
    return 0;
}

static void gfx_pvr_unload_shader(UNUSED struct ShaderProgram *old_prg) {
    cur_shader = NULL;
}

static void gfx_pvr_load_shader(struct ShaderProgram *new_prg) {
    cur_shader = new_prg;
    pvr_mark_dirty();   // textured-vs-colour header depends on the combiner
}

static struct ShaderProgram *gfx_pvr_create_and_load_new_shader(uint32_t shader_id) {
    struct CCFeatures ccf;
    gfx_cc_get_features(shader_id, &ccf);

    struct ShaderProgram *prg = &shader_program_pool[shader_program_pool_size++];
    prg->shader_id = shader_id;
    prg->cc = ccf;
    prg->num_inputs = ccf.num_inputs;
    prg->texture_used = ccf.used_textures[0];   // SF64 single-tile: tile 0 only

    if (ccf.used_textures[0] && ccf.num_inputs) {
        prg->mix = SH_MT_TEXTURE_COLOR;
    } else if (ccf.used_textures[0]) {
        prg->mix = SH_MT_TEXTURE;
    } else if (ccf.num_inputs > 1) {
        prg->mix = SH_MT_COLOR_COLOR;
    } else if (ccf.num_inputs) {
        prg->mix = SH_MT_COLOR;
    } else {
        prg->mix = SH_MT_NONE;
    }

    prg->enabled = 0;
    gfx_pvr_load_shader(prg);
    return prg;
}

static struct ShaderProgram *gfx_pvr_lookup_shader(uint32_t shader_id) {
    for (size_t i = 0; i < shader_program_pool_size; i++)
        if (shader_program_pool[i].shader_id == shader_id)
            return &shader_program_pool[i];
    return NULL;
}

// Single-tile: return the used-texture flag (no used_textures[2] out-param).
static uint8_t gfx_pvr_shader_get_info(struct ShaderProgram *prg, uint8_t *num_inputs) {
    *num_inputs = prg->num_inputs;
    return prg->texture_used;
}

static uint32_t gfx_pvr_new_texture(void) {
    uint32_t id = ++sTexCount;
    // Clamp guards the array, but a clamp means the id space is exhausted THIS reset cycle:
    // every further texture aliases slot 1023 -> "same texture everywhere". sTexCount resets in
    // gfx_pvr_clear_all_textures, so this should not fire; if it does, the scene needs
    // >PVR_TEX_MAX live textures. Don't fail silently.
    if (id >= PVR_TEX_MAX) {
        static int sTexIdClampLog = 0;
        if (sTexIdClampLog++ < 8) printf("PVR_TEXID_CLAMP sTexCount=%u >= PVR_TEX_MAX=%u\n", (unsigned)id, (unsigned)PVR_TEX_MAX);
        id = PVR_TEX_MAX - 1;
    }
    memset(&sTextures[id], 0, sizeof(sTextures[id]));
    return id;
}

static void gfx_pvr_select_texture(uint32_t texture_id) {
    sBoundTex = texture_id;
    sCurBound = texture_id;   // upload target follows the most recent bind (à la GLdc)
    pvr_mark_dirty();
}

extern int gfx_pvr_next_twiddled;
// Set by the front-end convert loops (inline AND-accumulate): 1 iff every texel is fully opaque.
// Consumed (and reset to 0) by gfx_pvr_upload_texture below. Mirrors gfx_pvr_next_twiddled.
extern int gfx_pvr_next_opaque;
// `pvrfmt` is a native PVR_TXRFMT_* pixel format straight from the front-end (no GL laundering).
static void gfx_pvr_upload_texture(const uint16_t *buf16, int width, int height, unsigned int pvrfmt) {
    struct PvrTex *t = &sTextures[sCurBound];
    int twiddled = gfx_pvr_next_twiddled;
    // RGB565 is only the per-frame framebuffer-capture (blur) here: it re-uploads every frame, so the
    // Morton reorder of twiddling is pure cost with no cache-locality payoff -> always upload linear.
    if (pvrfmt == PVR_TXRFMT_RGB565) twiddled = 0;
    int fmt = pvrfmt | (twiddled ? 0 : PVR_TXRFMT_NONTWIDDLED);

    const uint16_t *src = buf16;
    uint32_t fw = width, fh = height;
    float us = 1.0f, vs = 1.0f;

    if (!pvr_is_pot(width) || !pvr_is_pot(height) || width < 8 || height < 8) {
        fw = (width  < 8) ? 8 : (pvr_is_pot(width)  ? (uint32_t) width  : pvr_next_pot(width));
        fh = (height < 8) ? 8 : (pvr_is_pot(height) ? (uint32_t) height : pvr_next_pot(height));
        // Gate on TOTAL texels vs the scratch capacity, not per-dimension: short wide strips fit
        // fine by area but a width cap would wrongly reject them -> NPOT upload -> static noise.
        if ((uint32_t) fw * (uint32_t) fh <= 256u * 256u) {
            pvr_pad16(src, width, height, sPadScratch, fw, fh);
            src = sPadScratch;
            us = (float) width  / (float) fw;
            vs = (float) height / (float) fh;
        } else {
            fw = width; fh = height;   // genuinely too big for scratch: upload as-is (rare)
        }
    }

    uint32_t size = fw * fh * 2;
    // Mirror glTexImage2D: reuse the existing VRAM allocation when the new upload fits, only
    // (re)allocate when it's bigger. The whole texture VRAM set is freed at the glDeleteTextures
    // point (gfx_pvr_clear_all_textures, via nuke_everything).
    if (t->addr && size > t->alloc_size) { pvr_mem_free(t->addr); t->addr = NULL; }
    if (!t->addr) { t->addr = pvr_mem_malloc(size); t->alloc_size = t->addr ? size : 0; }
    if (t->addr) {
        if (twiddled) pvr_txr_load_ex((void *)src, t->addr, fw, fh, PVR_TXRLOAD_16BPP);  // always twiddles
        else          pvr_txr_load((void *)src, t->addr, size);                          // linear
    }
    t->w = fw; t->h = fh; t->fmt = fmt;
    t->u_scale = us; t->v_scale = vs;
    // RGB565 has no alpha -> always opaque; else take what the convert loop computed inline (0 for
    // paths that don't compute it). Reset after consuming so a non-setting path can't inherit a stale
    // "opaque" from the previous upload.
    t->opaque = (pvrfmt == PVR_TXRFMT_RGB565) ? 1 : (uint8_t) gfx_pvr_next_opaque;
    gfx_pvr_next_opaque = 0;
    pvr_mark_dirty();
}

// Is the currently-bound texture fully opaque? Used by the 2D-backdrop OP-routing gate in the
// front-end (an effectively-opaque XLU quad can go on the OP list even when its combine reads texel
// alpha, as long as the texture itself has no transparent texels).
int gfx_pvr_bound_texture_opaque(void) { return sTextures[sBoundTex].opaque; }

static void gfx_pvr_set_sampler_parameters(uint8_t linear_filter, uint32_t cms, uint32_t cmt) {
    struct PvrTex *t = &sTextures[sBoundTex];
    // Match GLdc gfx_cm_to_opengl precedence: CLAMP wins, else MIRROR (mirror-repeat),
    // else plain repeat. PVR clamp == GL_CLAMP, PVR flip == GL_MIRRORED_REPEAT.
    t->filter = linear_filter ? 1 : 0;
    t->clampu = (cms & G_TX_CLAMP) ? 1 : 0;
    t->clampv = (cmt & G_TX_CLAMP) ? 1 : 0;
    t->flipu  = (!(cms & G_TX_CLAMP) && (cms & G_TX_MIRROR)) ? 1 : 0;
    t->flipv  = (!(cmt & G_TX_CLAMP) && (cmt & G_TX_MIRROR)) ? 1 : 0;
    pvr_mark_dirty();
}

// Free ALL cached texture VRAM. Mirrors GLdc's glDeleteTextures sweep in gfx_clear_all_textures
// (called from nuke_everything at memory resets) — the point where the texture cache's VRAM
// (including reuse bloat) is reclaimed.
//
// CRITICAL: also RESET the id allocator. nuke_everything calls this and then reset_texcache()
// (front-end cache wipe), after which the front-end re-requests a fresh id (new_texture ->
// ++sTexCount) for every texture it re-encounters. If sTexCount is NOT reset here, it climbs
// monotonically across resets; once cumulative unique textures exceed PVR_TEX_MAX, new_texture
// clamps EVERY further id to slot 1023 and the whole scene draws with the last-uploaded texture
// (the "same texture everywhere" wedge). GLdc never hit this — its id space isn't capped.
void gfx_pvr_clear_all_textures(void) {
    for (uint32_t i = 0; i <= sTexCount && i < PVR_TEX_MAX; i++) {
        if (sTextures[i].addr) {
            pvr_mem_free(sTextures[i].addr);
            sTextures[i].addr = NULL;
            sTextures[i].alloc_size = 0;
        }
    }
    sTexCount = 0;   // realign with reset_texcache() (front-end), which runs right after
    sBoundTex = 0;
    sCurBound = 0;
}

// PoT-padding UV correction (real/padded), consumed by the front-end's recip_tex_*
// in place of GLdc's get_current_*_scale (PVR build routes to these).
float gfx_pvr_get_u_scale(void) { return sTextures[sBoundTex].u_scale; }
float gfx_pvr_get_v_scale(void) { return sTextures[sBoundTex].v_scale; }

// SF64 framebuffer-capture-for-effects (blur overlay). Mirrors gfx_gldc.c's capture_framebuffer:
// downsample the displayed 16-bit framebuffer (KOS vram_s, RGB565) by 4x into scaled2 (a 256x128
// buffer, gfx_buf.c), which the front-end then uploads as the blur texture. Called from
// fox_game.c. (HW NOTE: on the raw-PVR path vram_s points at VRAM base; whether the just-flipped
// scene lands there at capture time vs GLdc's glKosSwapBuffers may need a timing/offset tweak —
// audition on HW. Logic kept identical to GLdc for now.)
extern uint16_t scaled2[256 * 128];
void capture_framebuffer(void) {
#if LOWRES
    for (int y = 0; y < 240; y += 2) {
        for (int x = 0; x < 320; x += 2) {
            scaled2[(y << 7) + (x >> 1)] = vram_s[((y << 8) + (y << 6)) + x];
        }
    }
#else
    for (int y = 0; y < 480; y += 4) {
        for (int x = 0; x < 640; x += 4) {
            scaled2[(y << 6) + (x >> 2)] = vram_s[((y << 9) + (y << 7)) + x];
        }
    }
#endif
}
 
// Full reset at scene/memory load (fox_load.c Load_SceneFiles). GLdc's nuke_everything cleared GL
// textures + the front-end cache; the PVR equivalent frees all PVR texture VRAM (and resets the id
// allocator) then wipes the front-end hashmap. Without it, scene loads leak VRAM and eventually
// clamp every texture id -> "same texture everywhere".
extern void reset_texcache(void);
extern void AicaSynth_ClearSampleCache(void);   // drop the prior scene's unreferenced ARAM samples
void nuke_everything(void) {
    gfx_pvr_clear_all_textures();
    reset_texcache();
    AicaSynth_ClearSampleCache();
}

static void gfx_pvr_set_depth_test(uint8_t depth_test) {
    if (sDepthTest != depth_test) { sDepthTest = depth_test; pvr_mark_dirty(); }
}
static void gfx_pvr_set_depth_mask(uint8_t z_upd) {
    if (sDepthWrite != z_upd) { sDepthWrite = z_upd; pvr_mark_dirty(); }
}
static void gfx_pvr_set_tex_env(uint32_t mode) {
    // enum gfx_tex_env (front-end) == pvr_txr_shading_mode order, so store straight through.
    if (sTexEnv != (uint8_t) mode) { sTexEnv = (uint8_t) mode; pvr_mark_dirty(); }
}

static void gfx_pvr_set_zmode_decal(UNUSED uint8_t zmode_decal) {
    // N64 ZMODE_DEC decal z-fighting is handled entirely in the front-end bake (gfx_sp_tri1): a
    // decal vert's z (1/w) is nudged nearer by PVR_DECAL_ZBIAS so it wins the GEQUAL depth test
    // against its coplanar base. No PVR poly-header change is needed, so this is a no-op — which
    // also avoids recompiling the header every time decal mode toggles (frequent).
}

// PVR vertex fog enable/disable — baked into the poly header, so a change dirties all lists.
void gfx_pvr_set_fog(uint8_t enabled) {
    enabled = enabled ? 1 : 0;
    if (sFogEnabled != enabled) { sFogEnabled = enabled; pvr_mark_dirty(); }
}

// Vertex fog colour register (0x00b4). Global, read at render time — write it directly
// (KOS's pvr_fog_vertex_color is a stub). Not part of the poly header, so no dirty needed.
void gfx_pvr_set_fog_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    PVR_SET(PVR_FOG_VERTEX_COLOR,
            PVR_PACK_COLOR(a * (1.0f / 255.0f), r * (1.0f / 255.0f),
                           g * (1.0f / 255.0f), b * (1.0f / 255.0f)));
}
static void gfx_pvr_set_viewport(UNUSED int x, UNUSED int y, UNUSED int w, UNUSED int h) { /* front-end SW viewport */ }
static void gfx_pvr_set_scissor(UNUSED int x, UNUSED int y, UNUSED int w, UNUSED int h)  { /* front-end SW scissor */ }
static void gfx_pvr_set_use_alpha(UNUSED uint8_t use_alpha)        { /* OP/PT/TR routing comes via gfx_pvr_set_blend */ }

// Front-end declares whether THIS draw is textured (its real use_texture/usetex decision). The
// poly header follows this — see pvr_compile_header. A change dirties all lists so a textured->
// untextured transition (e.g. a fill quad inheriting the prior textured combiner) gets a fresh
// colour header instead of riding the previous textured batch.
void gfx_pvr_set_textured(uint8_t textured) {
    textured = textured ? 1 : 0;
    if (sDrawTextured != textured) { sDrawTextured = textured; pvr_mark_dirty(); }
}

// Front-end list classifier, called directly (not via rapi) from gfx_sp_tri1/gfx_sp_quad_2d:
// kind 0 = OP (fully opaque -> live list), 1 = PT (alpha-test cutout -> sPunch bucket),
// 2 = TR (real alpha-blend -> sTR bucket). Routing only — does NOT dirty headers. Persists
// across frames in lockstep with the front-end's tracker, so start_frame does not reset it.
void gfx_pvr_set_blend(uint8_t kind) {
    sListKind = kind;   // 0=OP (live), 1=PT (sPunch bucket), 2=TR (sTR bucket)
}

// Map the front-end's abstract gfx_blend_factor codes to PVR blend modes for the TR list.
// Only TR blends, so a change dirties only the TR header (not OP/PT). No shader-id keying.
void gfx_pvr_set_blend_factors(uint8_t src_code, uint8_t dst_code) {
    static const int kMap[] = {
        [GFX_BLENDF_ZERO]        = PVR_BLEND_ZERO,
        [GFX_BLENDF_ONE]         = PVR_BLEND_ONE,
        [GFX_BLENDF_SRCALPHA]    = PVR_BLEND_SRCALPHA,
        [GFX_BLENDF_INVSRCALPHA] = PVR_BLEND_INVSRCALPHA,
        [GFX_BLENDF_DSTALPHA]    = PVR_BLEND_DESTALPHA,
        [GFX_BLENDF_INVDSTALPHA] = PVR_BLEND_INVDESTALPHA,
    };
    int s = kMap[src_code], d = kMap[dst_code];
    if (s != sBlendSrc || d != sBlendDst) {
        sBlendSrc = s; sBlendDst = d;
        sTR.dirty = 1;   // blend state lives only in the TR header
    }
}

// (Re)emit the live OP header to the open OP list when depth/texture state changed.
static inline void pvr_emit_op_header(void) {
    if (!sOpDirty) return;
    pvr_poly_hdr_t __attribute__((aligned(32))) hdr;
    pvr_compile_header(&hdr, PVR_KIND_OP);
    pvr_poly_hdr_t *hp = (pvr_poly_hdr_t *) pvr_dr_target(sDrState);
    *hp = hdr;
    pvr_dr_commit(hp);
    sOpDirty = 0;
}

// Stream OP geometry straight to the live OP list via DR. Called per source-triangle from the
// front-end (no buf_vbo) AND internally. Header re-emitted only on state change (sOpDirty), so a
// same-state run streams headerless after the first. External — no wrapper needed. (oargb carried
// in the copied vertex — do NOT zero it here.)
void pvr_submit_op(const pvr_vertex_t *tris, size_t n) {
    // Verts arrive fully built WITH strip flags set at creation (3D bake loop / 2D quad / edge mask).
    // Header on the DR path, then bulk-ship the whole run to the TA FIFO in one store-queue copy.
    pvr_emit_op_header();
    sq_fast_cpy(SQ_MASK_DEST(PVR_TA_INPUT), tris, n);
}

// 2D quad depth counter owned by the front-end (gfx_draw_rectangle increments it).
// GLdc's start_frame reset it each frame; the PVR backend must too, or it grows unbounded.
extern float screen_2d_z;
static void gfx_pvr_draw_triangles(float buf_vbo[], UNUSED size_t buf_vbo_len,
                                   size_t buf_vbo_num_tris) {
    // The front-end bakes screen-space pvr_vertex_t (gfx_sp_tri1 PVR path, S2/S3), so each vert is a
    // direct SQ submit. Routing via sListKind.
    const pvr_vertex_t *tris = (const pvr_vertex_t *) buf_vbo;
    const size_t n = buf_vbo_num_tris * 3;
    if (sListKind == PVR_KIND_TR)
        pvr_append(&sTR, PVR_KIND_TR, tris, n);
    else if (sListKind == PVR_KIND_PT)
        pvr_append(&sPunch, PVR_KIND_PT, tris, n);
    else
        pvr_submit_op(tris, n);   // OP, live
}

// 2D path (vtable slot draw_triangles_2d; the front-end currently also calls a 2D draw directly).
// SF64 passes 4 screen-space quad verts (rsp.loaded_vertices_2D). They are emitted as one native
// 4-vertex strip (v3 = EOL -> 2 tris). Routing via gfx_pvr_set_blend: opaque HUD -> live OP;
// alpha-test glyphs -> PT; real alpha-blend -> TR. The 3rd arg is SF64's use_texture flag, ignored
// here (texturing is decided by the bound shader/texture state). Vertex order/winding to be
// confirmed when the front-end 2D seam is routed (S4).
// One native 4-vertex PVR strip (v3 = EOL -> 2 tris). The front-end builds loaded_vertices_2D in
// PVR STRIP order (ul, ll, ur, lr) — see gfx_draw_rectangle et al. — so the strip tessellates
// correctly (diagonal ll-ur) with no extra verts. Routing via gfx_pvr_set_blend: opaque HUD -> OP;
// alpha-test glyphs -> PT; blend -> TR.
void gfx_pvr_draw_triangles_2d(void *buf_vbo, UNUSED size_t buf_vbo_len, UNUSED size_t buf_vbo_num_tris) {
    pvr_vertex_t *q = (pvr_vertex_t *) buf_vbo;
    // 4-vertex strip: EOL on the last vert. Set once here (the 2D choke point; the verts arrive
    // pre-built from the front-end rect builders) so every emit below just copies — matching the
    // 3D bake / bucket paths, no per-slot flag stamping.
    q[0].flags = q[1].flags = q[2].flags = PVR_CMD_VERTEX;
    q[3].flags = PVR_CMD_VERTEX_EOL;

    if (sListKind == PVR_KIND_OP) {
        pvr_emit_op_header();
        sq_fast_cpy(SQ_MASK_DEST(PVR_TA_INPUT), q, 4);
        return;
    }

    PvrBucket *b = (sListKind == PVR_KIND_TR) ? &sTR : &sPunch;
    int kind     = (sListKind == PVR_KIND_TR) ? PVR_KIND_TR : PVR_KIND_PT;
    if (b->dirty || b->cur < 0) {
        if (b->nbatch >= b->max_batch) { if (sDropB++ < 8) printf("PVR_DROP batch %d\n", kind); return; }
        b->cur = b->nbatch++;
        pvr_compile_header(&b->batch[b->cur].hdr, kind);
        b->batch[b->cur].start = b->nverts;
        b->batch[b->cur].count = 0;
        b->dirty = 0;
    }
    if (b->nverts + 4 > b->max_verts) { if (sDropV++ < 8) printf("PVR_DROP verts %d\n", kind); return; }
    for (int i = 0; i < 4; i++)
        b->verts[b->nverts++] = q[i];   // flags already set above; flushed later via sq_fast_cpy
    b->batch[b->cur].count += 4;
}

static void gfx_pvr_init(void) {
    if (vid_check_cable() != CT_VGA)
        vid_set_mode(DM_640x480_NTSC_IL, PM_RGB565);
    else
        vid_set_mode(DM_640x480_VGA, PM_RGB565);
    pvr_init(&sPvrParams);

    // PT alpha-test reference: punch-through discards texels with alpha <= this, giving N64
    // cutout/texture-edge transparency. 0x80 matches OoT. (ARGB1555 alpha is 0/255, so this
    // cleanly drops the transparent bit; ARGB4444 keeps alpha >= ~8.)
    *(volatile uint32_t *) 0xA05F811C = 0x80 + 0x40;

    // SF64's GLdc backend clears the framebuffer to BLACK every frame (it draws its own skyboxes
    // as geometry — there is no per-frame N64 clear_color global). Match that.
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);

    // Headers are compiled lazily per depth/texture/list state at draw time.
}

static void gfx_pvr_on_resize(void) { }

static void gfx_pvr_start_frame(void) {
    pvr_wait_ready();
    pvr_scene_begin();
    // SF64 clears to black (see gfx_pvr_init); nothing per-frame to recompute.
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);

    // OP list open + live for the whole frame (guaranteed-opaque geometry only).
    // Anything with alpha (blend or alpha-test cutout) is deferred to the PT/TR buckets.
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_dr_init(&sDrState);
    sCurrentList = PVR_LIST_OP_POLY;
    // 2D depth base: PVR z is 1/w, larger == nearer. The per-rect increment (gfx_draw_rectangle)
    // encodes 2D paint order into z. Base 1.0 keeps those increments within the depth buffer's
    // resolution. (Exact base/increment tuning happens when the 2D seam is routed — S4.)
    screen_2d_z = 1.0f;

    // Disable the PVR near/far z-clip. The front-end already near-clips in software, and the
    // far-plane background (ortho skybox baked to 1/w≈0) would otherwise be culled.
    pvr_set_zclip(0.0f);

    // Reset the deferred PT + TR buckets and force the live OP header to re-emit for the new
    // scene. sDepth*/sListKind persist in lockstep with the front-end's trackers.
    sTR.nverts    = 0; sTR.nbatch    = 0; sTR.cur    = -1; sTR.dirty    = 1;
    sPunch.nverts = 0; sPunch.nbatch = 0; sPunch.cur = -1; sPunch.dirty = 1;
    sOpDirty = 1;

    // Latch "did the last frame have a 3D (perspective) scene" so gfx_sp_tri1 can tell a Z-off
    // ortho BACKDROP (kept far) from a Z-off ortho OVERLAY (foreground 2D/text). Prev-frame value
    // avoids draw-order races within the frame.
    {
        extern int cur_frame_persp, prev_frame_had_persp, has_done_3d, has_done_3d_pending;
        extern float backdrop_far_z;
        prev_frame_had_persp = cur_frame_persp;
        cur_frame_persp = 0;
        has_done_3d = 0;           // reset: no 3D drawn yet this frame
        has_done_3d_pending = 0;   // clear any un-promoted arm from last frame
        backdrop_far_z = 0.00001f; // restart the backdrop far-slab stagger
    }
}

// OUT-OF-BAND overscan mask: 4 opaque-black quads over the screen edges, width 8 px * (screenW/320)
// = 16 px at 640 wide, at a super-near 1/w so they sit in front of everything. Emitted as OPAQUE OP
// with depth-write at the nearest z, so the mask also OCCLUDES any PT/TR that bleeds into the border
// (they depth-test against the mask's near z and lose). Injected while the OP list is still open.
static void gfx_pvr_draw_edge_mask(void) {
    const float W = 640.0f, H = 480.0f;
    const float b = 8.0f * (W / 320.0f);      // 16 px border at 640 wide
    const float Z = 1000000.0f;               // z = 1/w, huge == nearest -> in front of all geometry

    // Force untextured / opaque-OP / depth-writing state; sOpDirty=1 makes pvr_submit_op re-emit the
    // header even if the last draw already matched. Frame is ending, so no need to restore.
    sDrawTextured = 0; sDepthTest = 1; sDepthWrite = 1; sOpDirty = 1;

    static pvr_vertex_t v[24] __attribute__((aligned(32)));   // 4 rects * (2 tris * 3 verts)
    const float rects[4][4] = {
        { 0.0f,   0.0f,   b,   H   },   // left
        { W - b,  0.0f,   W,   H   },   // right
        { 0.0f,   0.0f,   W,   b   },   // top
        { 0.0f,   H - b,  W,   H   },   // bottom
    };
    const int idx[6] = { 0, 1, 2, 1, 2, 3 };   // strip ul,ll,ur,lr -> tris (ul,ll,ur)+(ll,ur,lr)
    int vi = 0;
    for (int r = 0; r < 4; r++) {
        const float x0 = rects[r][0], y0 = rects[r][1], x1 = rects[r][2], y1 = rects[r][3];
        const float xy[4][2] = { { x0, y0 }, { x0, y1 }, { x1, y0 }, { x1, y1 } };
        for (int k = 0; k < 6; k++) {
            pvr_vertex_t *p = &v[vi++];
            p->flags = ((k % 3) == 2) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;   // 2 tris/rect, EOL every 3rd
            p->x = xy[idx[k]][0];
            p->y = xy[idx[k]][1];
            p->z = Z;
            p->u = 0.0f; p->v = 0.0f;
            p->argb = 0xFF000000u;     // opaque black (ARGB)
            p->oargb = 0;             // no additive offset / fog
        }
    }
    pvr_submit_op(v, 24);
}

static void gfx_pvr_end_frame(void) {
    gfx_pvr_draw_edge_mask();   // out-of-band overscan mask, into the still-open OP list
    pvr_list_finish();   // close the OP list (opened once this frame)

    // Flush the deferred buckets, each into its own list opened once. PT FIRST: alpha-test
    // cutouts are opaque pixels that write depth, so they must be down before TR composites.
    // NOTE (experiment): open+finish EVERY registered list each frame even when empty. KOS/PVR
    // wants all binsize!=0 lists submitted per frame; skipping an empty list leaves the TA with
    // stale bin pointers and corrupts the OPAQUE pass (glitchy ground that clears when a bomb's
    // TR explosion is on screen). pvr_flush_bucket() no-ops on an empty bucket.
    pvr_list_begin(PVR_LIST_PT_POLY);
    pvr_dr_init(&sDrState);
    pvr_flush_bucket(&sPunch);
    pvr_list_finish();
    // TR SECOND: real alpha-blend surfaces, autosorted, composited over OP + PT (depth-tested).
    pvr_list_begin(PVR_LIST_TR_POLY);
    pvr_dr_init(&sDrState);
    pvr_flush_bucket(&sTR);
    pvr_list_finish();
}

static void gfx_pvr_finish_render(void) {
    // Submits the scene to the GPU and flips. (This is where the frame flips — gfx_dc.c does not.)
    pvr_scene_finish();
}

struct GfxRenderingAPI gfx_pvr_api = {
    gfx_pvr_z_is_from_0_to_1,
    gfx_pvr_unload_shader,
    gfx_pvr_load_shader,
    gfx_pvr_create_and_load_new_shader,
    gfx_pvr_lookup_shader,
    gfx_pvr_shader_get_info,
    gfx_pvr_new_texture,
    gfx_pvr_select_texture,
    gfx_pvr_upload_texture,
    gfx_pvr_set_sampler_parameters,
    gfx_pvr_set_depth_test,
    gfx_pvr_set_depth_mask,
    gfx_pvr_set_zmode_decal,
    gfx_pvr_set_tex_env,
    gfx_pvr_set_viewport,
    gfx_pvr_set_scissor,
    gfx_pvr_set_use_alpha,
    gfx_pvr_draw_triangles,
    gfx_pvr_draw_triangles_2d,
    gfx_pvr_init,
    gfx_pvr_on_resize,
    gfx_pvr_start_frame,
    gfx_pvr_end_frame,
    gfx_pvr_finish_render,
};
