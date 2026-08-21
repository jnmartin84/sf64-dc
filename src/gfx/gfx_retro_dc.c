#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#define recip255 0.00392157f
#define recip127 0.00787402f
#define recip2pi 0.159155f
#define recip31 0.03225806f
#define recip15 0.06666667f
#define recip64k 0.00001526f
#define recip_4timeshalfscrwid 0.0015625f
#define recip_4timeshalfscrhgt 0.00208333f

#define u32 uint32_t
#define s32 int32_t
#define u16 uint16_t
#define s16 int16_t

#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif
#include <PR/gbi.h>

#define G_TX_LOADTILE 7
#define G_TX_RENDERTILE 0

#define G_TX_NOMIRROR 0
#define G_TX_WRAP 0
#define G_TX_MIRROR 0x1
#define G_TX_CLAMP 0x2
#define G_TX_NOMASK 0
#define G_TX_NOLOD 0

// I-cache layout control. The SH4 I-cache is 8KB DIRECT-MAPPED and this file is linked with
// -fno-toplevel-reorder (source order), so any code-size change anywhere shifts every later
// function and reshuffles which hot functions alias each other (PROF 2026-08-16: adding a 1.4KB
// function before gfx_run_dl moved icache stall 3.8 -> 5.3 ms/frame with identical workload).
// The innermost per-primitive set (vertex, tri, fill-rect, DL walk: ~6.4KB total) is placed in ONE
// named section so it stays contiguous (< 8KB -> never self-aliasing) wherever the linker puts it.
// (KOS's shlelf.xc gathers `.text .text.*` in input order, so this block lands right after this
// object's .text - contiguous is all that matters.) Keep the members' total under 8KB.
#define GFX_HOT __attribute__((section(".text.hot.gfx")))

#include "gfx_pc.h"
#include "gfx_cc.h"
#include "gfx_window_manager_api.h"
#include "gfx_rendering_api.h"
#include "gfx_screen_config.h"
#include "macros.h"

#include <kos.h>
#include "sh4zam.h"

// Pack 8-bit R,G,B,A into a PVR ARGB8888 word (a<<24 | r<<16 | g<<8 | b). (KOS's PVR_PACK_COLOR
// takes normalized floats and would just re-multiply our byte values, so keep the byte version.)
#define PACK_ARGB8888(r, g, b, a) \
    ((uint32_t)((uint8_t)(a) << 24) | ((uint8_t)(r) << 16) | ((uint8_t)(g) << 8) | (uint8_t)(b))

uint32_t last_set_texture_image_width;
int draw_rect;
int do_rectdepfix = 0;
int do_space_bg = 0;
volatile int do_floorscroll = 0;
volatile int do_zfight = 0;
volatile int do_zflip = 0;

// SCALE_M_N: upscale/downscale M-bit integer to N-bit
#define SCALE_3_5(VAL_) ((VAL_) << 2)

#define SCALE_5_8(VAL_) ((VAL_) << 3)
#define SCALE_8_5(VAL_) ((VAL_) >> 3)

#define SCALE_4_8(VAL_) ((VAL_) << 4)
#define SCALE_8_4(VAL_) ((VAL_) >> 4)

#define SCALE_3_8(VAL_) ((VAL_) << 5)
#define SCALE_8_3(VAL_) ((VAL_) >> 5)

float screen_2d_z;

#define HALF_SCREEN_WIDTH (SCREEN_WIDTH / 2)
#define HALF_SCREEN_HEIGHT (SCREEN_HEIGHT / 2)

#define RATIO_X (gfx_current_dimensions.width / (2.0f * HALF_SCREEN_WIDTH))
#define RATIO_Y (gfx_current_dimensions.height / (2.0f * HALF_SCREEN_HEIGHT))

uint8_t pa;

extern int use_gorgon_alpha;

int do_andross = 0;

// 128 previously
// 96 based on actual observed max vbo tri count during a flush
// 64 is almost never actually hit during normal gameplay, so lets pick that
#define MAX_BUFFERED 64
#define MAX_LIGHTS 8
#define MAX_VERTICES 32

int alpha_noise = 0;

int do_starfield = 0;

int do_menucard = 0;

// Opaque to the front-end: the interpreter only holds/compares ShaderProgram* and hands them to the
// backend vtable (create/load/unload/lookup, shader_get_info) - it never touches the fields. The
// layout is private to the backend (gfx_pvr.c). Keeping a second full definition here would give two
// TUs conflicting `struct ShaderProgram` bodies, which LTO flags as a type mismatch on gfx_pvr_api.
struct ShaderProgram;

struct RGBA {
    uint8_t r, g, b, a;
};

struct XYWidthHeight {
    uint16_t x, y, width, height;
};

// PVR: _x/_y hold RAW CLIP x,y (no perspective divide), plus raw clip _z,_w
// (homogeneous near-clip, depth=1/w, fog=z/w) and a per-vertex fog coefficient.
// Overflows one cache line.
struct __attribute__((aligned(32))) LoadedVertex {
    // 0, 4 - PVR: raw clip x, y.
    float _x, _y;
    float _z, _w;   // raw clip z, w (PVR software near-clip + screen-bake)
    float x, y, z /* , w */;
    float u, v;
    struct RGBA color;
    uint8_t fog;    // N64 per-vertex fog coefficient (0..255) -> PVR oargb.alpha (HW vertex fog)
};

struct __attribute__((aligned(16))) LoadedNormal {
    float x, y, z, w;
};

// bits 0 - 5 -> clip_rej
// bit 6 - wlt0
// bit 7 - lit
uint8_t __attribute__((aligned(32))) clip_rej[MAX_VERTICES];

// TextureHashmapNode::dirty flag bits (packed into one byte to keep the node cache-line friendly).
#define TEX_DIRTY   0x01u   // needs (re)upload
#define TEX_DYNAMIC 0x02u   // sticky: address has been invalidated -> upload non-twiddled

// exactly half of a cache-line in size
struct TextureHashmapNode {
    // 0
    struct TextureHashmapNode* next;
    // 4
    uint32_t texture_id;
    // 8
    uint32_t key;
    // 12 - flags byte (kept 1 byte so two nodes still fit one 32-byte cache line):
    //   bit0 TEX_DIRTY   : needs re-upload (content changed / first load)
    //   bit1 TEX_DYNAMIC : sticky "content churns" - set by gfx_texture_cache_invalidate(), NOT
    //                      cleared on re-upload. Drives PVR upload layout (0=twiddled/static,
    //                      set=non-twiddled/dynamic). Both bits 0 on a freshly created node.
    uint8_t dirty;
    // 13
    uint8_t linear_filter;
    // 14
    uint8_t cms;
    // 15
    uint8_t cmt;
};

static struct {
    struct TextureHashmapNode* hashmap[1024];
    struct TextureHashmapNode pool[1024];
    uint32_t pool_pos;
} gfx_texture_cache;

struct ColorCombiner {
    uint32_t cc_id;
    struct ShaderProgram* prg;
    uint8_t shader_input_mapping[2][4];
};

static struct ColorCombiner color_combiner_pool[64];
static uint8_t color_combiner_pool_size;

static struct RSP {
    struct LoadedVertex __attribute__((aligned(32))) loaded_vertices[MAX_VERTICES + 2];
    struct LoadedNormal __attribute__((aligned(32))) loaded_normals[MAX_VERTICES + 4];
    pvr_vertex_t __attribute__((aligned(32))) loaded_vertices_2D[4];

    float modelview_matrix_stack[/* 11 */4][4][4] __attribute__((aligned(32)));

    float MP_matrix[4][4] __attribute__((aligned(32)));

    float P_matrix[4][4] __attribute__((aligned(32)));

    Light_t __attribute__((aligned(16))) current_lights[MAX_LIGHTS + 1];
    Light_t __attribute__((aligned(16))) lookat[2];

    float __attribute__((aligned(32))) current_lights_coeffs[MAX_LIGHTS][3];
    float __attribute__((aligned(32))) current_lookat_coeffs[2][3]; // lookat_x, lookat_y

    uint32_t __attribute__((aligned(32))) geometry_mode;
    struct {
        // U0.16
        float s, t;
    } texture_scaling_factor;
    uint8_t use_fog;
    uint8_t modelview_matrix_stack_size;
    // 866
    uint8_t current_num_lights; // includes ambient light
    // 867
    uint8_t lights_changed;
} rsp __attribute__((aligned(32)));

static struct RDP {
    const uint8_t* palette;
    const uint8_t* loaded_palette;
    uint32_t last_palette;
    uint8_t palette_dirty;
    struct {
        const uint8_t* addr;
        uint8_t siz;
        uint8_t tile_number;
        uint32_t tmem;
    } texture_to_load;
    struct {
        const uint8_t* addr;
        uint32_t size_bytes;
    } loaded_texture[8];
    struct {
        uint8_t fmt;
        uint8_t siz;
        uint8_t cms, cmt;
        uint16_t uls, ult, lrs, lrt; // U10.2
        uint32_t line_size_bytes;
        uint16_t masks, maskt;
        uint8_t shifts, shiftt;
        //		uint8_t w, h;
    } texture_tile;
    uint8_t textures_changed[2];

    uint32_t other_mode_l, other_mode_h;
    uint32_t combine_mode;
    // Raw G_SETCOMBINE words - the PVR combiner evaluator reads the N64 mux bit-fields directly
    // (sf64's compact combine_mode loses them). Captured at the G_SETCOMBINE dispatch.
    uint32_t combine_w0, combine_w1;

    struct RGBA env_color, prim_color, fog_color, fill_color;
    struct XYWidthHeight viewport, scissor;
    uint8_t viewport_or_scissor_changed;
    void* z_buf_address;
    void* color_image_address;
} rdp __attribute__((aligned(32)));

struct RenderingState {
    // 0
    struct ShaderProgram* shader_program;
    // 4
    struct TextureHashmapNode* textures[2];
    // 8
    uint8_t depth_test;
    uint8_t depth_mask;
    uint8_t decal_mode;
    uint8_t alpha_blend;
    // 12
    struct XYWidthHeight viewport;
    // 20
    struct XYWidthHeight scissor;
    // 28
    uint8_t fog_change;
    // 29
    uint8_t fog_col_change;
    // 30
    uint8_t tex_env;       // last gfx_tex_env pushed (PVR batch-flush gate)
    // 31
    uint8_t fog_enabled;   // last G_FOG state pushed (PVR batch-flush gate)
};

struct RenderingState  __attribute__((aligned(32))) rendering_state;

struct GfxDimensions gfx_current_dimensions;

static uint8_t dropped_frame;

//static pvr_vertex_t __attribute__((aligned(32))) buf_vbo[MAX_BUFFERED * 3]; // 3 vertices in a triangle
static pvr_vertex_t __attribute__((aligned(32))) quad_vbo[4]; // 4 verts make a quad
static size_t buf_vbo_len = 0;
static size_t buf_num_vert = 0;
static size_t buf_vbo_num_tris = 0;

// ===== Raw-PVR front-end seam state (S2/S3) ================================================
// The PVR backend does NO vertex transform; the front-end owns the full object->screen path.
// (screen_2d_z is already defined near the top of the file.) These frame trackers are
// latched/reset by the PVR backend's start_frame (gfx_pvr.c) and written by the draw paths here.
int cur_frame_persp = 0;       // a perspective (3D) projection was used this frame
int prev_frame_had_persp = 0;  // cur_frame_persp from the previous frame
int has_done_3d = 0;           // a 3D primitive has drawn this frame (backdrop-vs-overlay ortho)
int has_done_3d_pending = 0;   // a 3D tri drew, but promote has_done_3d only at G_ENDDL so a
                               // multi-triangle backdrop DL doesn't cross its own boundary mid-stream
int proj_is_ortho = 0;         // current projection is orthographic (set in gfx_sp_matrix)
int has_drawn_persp_tri = 0;   // ANY perspective triangle (depth-tested or Z-off backdrop) has been
                               // emitted this frame; set immediately (unlike has_done_3d). A 2D fill
                               // drawn after this is an OVERLAY (fade), never a pre-scene backdrop.
// Coplanar backdrop tiles (planet) all land at one far depth; autosort TR has no stable order at
// equal depth -> seam shimmer. Stagger successive backdrop prims within the far slab (draw order).
// Far-pin depths (PVR z = 1/w, larger == nearer). PVR_Z_FARPIN is behind everything the game
// projects (gProjectFar tops out at 30000 -> 1/w 3.3e-5) but NOT the old 1e-5: at that tiny 1/w
// the PVR's triangle setup dropped marginal triangles (the ending strips, and one of the two
// triangles of a 2x2 starfield fillrect -> triangle-shaped stars). The backdrop slab starts just
// IN FRONT of the far-pin so nebula/planet tiles still cover the star fills.
#define PVR_Z_FARPIN        0.00003f
#define PVR_Z_FARPIN_FILL2  0.000035f   // 2nd+ pre-scene fill (starfield pixels) - NEARER than the screen
                                        // clear so an OP z-tie can't drop them, still behind the backdrop slab
#define PVR_Z_BACKDROP0     0.00004f
int far_fill_count = 0;   // pre-scene (far-pinned) fills this frame; reset in start_frame
#define PVR_Z_BACKDROP_MAX  0.0001f
const float pvr_backdrop_z0 = PVR_Z_BACKDROP0;   // read by gfx_pvr.c start_frame reset
float backdrop_far_z = PVR_Z_BACKDROP0;
// Opaque-promoted 2D backdrop quads (gfx_sp_quad_2d XLU->OP reroute) sit NEARER than the whole
// persp backdrop slab, and must ALSO stagger per quad: two promoted quads at one z (e.g. a planet
// backdrop + a full-white fade that just reached alpha 255) tie on the depth-resolved OP list and
// the tie is decided arbitrarily - later-drawn must win to keep N64 paint order.
#define PVR_Z_QUAD_BD0      0.001f      // base: precision-safe for the razor-thin ending strips
#define PVR_Z_QUAD_BD_MAX   0.0011f
const float pvr_quad_backdrop_z0 = PVR_Z_QUAD_BD0;   // read by gfx_pvr.c start_frame reset
float quad_backdrop_z = PVR_Z_QUAD_BD0;
// Decal z-fight bias (PVR z = 1/w, GEQUAL): scale a decal vert's z slightly LARGER so it wins.
#define PVR_DECAL_ZBIAS 1.003f

// Framebuffer half-extents in pixels (gfx_current_dimensions/2, set once in gfx_init). The 2D
// rect path (gfx_draw_rectangle) maps NDC -1..1 -> 0..fb via these; they used to be the N64
// logical SCREEN_WIDTH/HEIGHT (320/240), which is only right for the 640x480 framebuffer and
// baked every 2D fill/texrect in 640-space under LOWRES (top-left quarter shown at 240p).
static float fb_half_w = 320.0f, fb_half_h = 240.0f;


// Viewport (pixel space) -> screen map: screen_x = sm_xscale*(_x/_w)+sm_xbias, y likewise
// (yscale<0 flips N64 y-up to PVR y-down), z = 1/w.
static float vpf_x = 0.0f, vpf_y = 0.0f, vpf_w = 640.0f, vpf_h = 480.0f;
static float sm_xscale = 320.0f, sm_xbias = 320.0f;
static float sm_yscale = -240.0f, sm_ybias = 240.0f;
static void gfx_recompute_screen_map(void) {
    float vw = vpf_w <= 0.0f ? 1.0f : vpf_w;
    float vh = vpf_h <= 0.0f ? 1.0f : vpf_h;
    float fb_h = (float) gfx_current_dimensions.height;
    sm_xscale = vw * 0.5f;
    sm_xbias  = vpf_x + vw * 0.5f;
    sm_yscale = -(vh * 0.5f);
    sm_ybias  = fb_h - vpf_y - vh * 0.5f;
}

// ---- Software scissor state (split-screen pane clip; mk64-dc's HW-validated scheme) -----------
// The raw-PVR backend has no per-draw scissor (the KOS userclip is one global register clipping to
// the full screen), so with multiple viewports per frame (VS split-screen) a pane's frustum
// overhang bakes to screen pixels inside the NEIGHBOURING pane and depth-stomps it. Pane clipping
// is therefore done in software in gfx_sp_tri1: the scissor rect becomes NDC bounds relative to
// the active viewport's window mapping
//   win = v.xy + (ndc+1)/2 * v.wh   =>   ndc = 2*(scissor - v.xy)/v.wh - 1
// and pane-crossing triangles are Sutherland-Hodgman clipped in clip space before the bake.
static float scf_x = 0.0f, scf_y = 0.0f, scf_w = 640.0f, scf_h = 480.0f;   // float scissor rect (rdp.scissor is uint16 -> wraps negatives)
static float sc_ndc_xmin = -1.0f, sc_ndc_xmax = 1.0f;
static float sc_ndc_ymin = -1.0f, sc_ndc_ymax = 1.0f;
static int sc_is_fullscreen = 1;
// Which scissor edges actually need the CLIP pass: an edge whose boundary sits on the framebuffer
// border has nothing to bleed into (overhang lands off-screen, the PVR userclip eats it); only
// edges on an interior split line stay active. Trivial-reject still uses all four edges.
static uint8_t sc_active_mask = 0x0F;
#define SCISSOR_W_EPS 0.00001f
// Per-vertex scissor outcode bits: each set bit = vertex OUTSIDE that pane edge. SC_FORCE marks a
// vertex whose NDC is not (yet) valid - at/behind the eye (w<=eps) or behind the near plane - so
// any triangle touching it must take the full clip path (near plane is clipped first there).
#define SC_LEFT   0x01
#define SC_RIGHT  0x02
#define SC_BOTTOM 0x04
#define SC_TOP    0x08
#define SC_FORCE  0x10
#define SC_EDGE_MASK 0x0F
// Outcodes are cached per loaded vertex (parallel to clip_rej), lazily refreshed per scissor
// generation: sc_gen_v[i] != cur_scissor_gen -> recompute. Loaders reset sc_gen_v to 0 ("never").
static uint8_t cur_scissor_gen = 1;
static uint8_t sc_oc_v[MAX_VERTICES + 2];
static uint8_t sc_gen_v[MAX_VERTICES + 2];

// Recompute the scissor NDC bounds from the current float viewport + scissor rects.
// Called from gfx_calc_and_set_viewport and gfx_dp_set_scissor (same window space).
static void gfx_recompute_scissor_planes(void) {
    float vx = vpf_x, vy = vpf_y;
    float vw = vpf_w <= 0.0f ? 1.0f : vpf_w;
    float vh = vpf_h <= 0.0f ? 1.0f : vpf_h;

    sc_ndc_xmin = 2.0f * (scf_x - vx) / vw - 1.0f;
    sc_ndc_xmax = 2.0f * ((scf_x + scf_w) - vx) / vw - 1.0f;
    sc_ndc_ymin = 2.0f * (scf_y - vy) / vh - 1.0f;
    sc_ndc_ymax = 2.0f * ((scf_y + scf_h) - vy) / vh - 1.0f;

    // Clip region is scissor INTERSECT viewport. The RSP confines geometry to the viewport
    // frustum (NDC +/-1), so a scissor LOOSER than the viewport (race-start style transitions)
    // must not widen the clip past the viewport edge - that's exactly the cross-pane depth-bleed
    // hole. Clamp every bound to [-1,1]; non-overlapping rects collapse to an empty interval
    // (nothing draws - also the correct N64 result).
    sc_ndc_xmin = sc_ndc_xmin < -1.0f ? -1.0f : (sc_ndc_xmin > 1.0f ? 1.0f : sc_ndc_xmin);
    sc_ndc_xmax = sc_ndc_xmax < -1.0f ? -1.0f : (sc_ndc_xmax > 1.0f ? 1.0f : sc_ndc_xmax);
    sc_ndc_ymin = sc_ndc_ymin < -1.0f ? -1.0f : (sc_ndc_ymin > 1.0f ? 1.0f : sc_ndc_ymin);
    sc_ndc_ymax = sc_ndc_ymax < -1.0f ? -1.0f : (sc_ndc_ymax > 1.0f ? 1.0f : sc_ndc_ymax);

    // Skip the software clip whenever the viewport fills the whole framebuffer, REGARDLESS of the
    // scissor (deliberate deviation from mk64-dc's scissor-covers-viewport condition): pane
    // clipping only protects a NEIGHBOURING sub-viewport from overhang/depth-stomp, and a
    // full-screen viewport has no neighbour. SF64 single-player always pairs the full viewport
    // with an 8px-margin scissor (Game_SetGameFrame) - the whole game is HW-validated rendering
    // that un-scissored (this port never had a scissor), and the 16px overscan edge mask covers
    // exactly that band - so honouring it here would only push every single-player triangle
    // through the classify path for nothing. Sub-viewports (VS split) always clip.
    float fbw = (float) gfx_current_dimensions.width;
    float fbh = (float) gfx_current_dimensions.height;
    int vp_is_full = (vx <= 0.5f && vy <= 0.5f &&
                      (vx + vw) >= fbw - 0.5f && (vy + vh) >= fbh - 0.5f);
    sc_is_fullscreen = vp_is_full;

    // Per-edge clip skip: map each bound back to framebuffer space and drop edges that sit on
    // the framebuffer border (half-pixel slop, matching vp_is_full).
    {
        float bx_min = vx + (sc_ndc_xmin + 1.0f) * 0.5f * vw;
        float bx_max = vx + (sc_ndc_xmax + 1.0f) * 0.5f * vw;
        float by_min = vy + (sc_ndc_ymin + 1.0f) * 0.5f * vh;
        float by_max = vy + (sc_ndc_ymax + 1.0f) * 0.5f * vh;
        uint8_t m = 0;
        if (bx_min >  0.5f)       m |= SC_LEFT;
        if (bx_max <  fbw - 0.5f) m |= SC_RIGHT;
        if (by_min >  0.5f)       m |= SC_BOTTOM;
        if (by_max <  fbh - 0.5f) m |= SC_TOP;
        sc_active_mask = m;
    }

    // Invalidate cached per-vertex outcodes (0 is reserved for "never computed").
    if (++cur_scissor_gen == 0)
        cur_scissor_gen = 1;
}

// Vertex scissor outcode from its homogeneous clip coords. One fast reciprocal + two muls beats
// four muls (SH4 fdiv is slow); the outcode is only a classifier - gfx_build_clipped_fan still
// clips with exact math.
static inline uint8_t compute_scissor_outcode(const struct LoadedVertex *v) {
    float w = v->_w;
    if (w <= SCISSOR_W_EPS || v->_z + w < 0.0f)
        return SC_FORCE;
    float rw = shz_fast_invf(w);
    float nx = v->_x * rw;
    float ny = v->_y * rw;
    uint8_t oc = 0;
    if (nx < sc_ndc_xmin) oc |= SC_LEFT;
    if (nx > sc_ndc_xmax) oc |= SC_RIGHT;
    if (ny < sc_ndc_ymin) oc |= SC_BOTTOM;
    if (ny > sc_ndc_ymax) oc |= SC_TOP;
    return oc;
}

// Backend seam (all defined in gfx_pvr.c): OP/PT/TR routing + TR blend factors + vertex fog +
// POT-pad UV correction + the no-buf_vbo submit path.
extern void gfx_pvr_set_blend(uint8_t kind);                     // 0=OP, 1=PT, 2=TR
extern void gfx_pvr_set_blend_factors(uint8_t src, uint8_t dst); // gfx_blend_factor codes
extern void gfx_pvr_set_textured(uint8_t textured);             // per-draw texturing intent (header)
extern void gfx_pvr_set_fog(uint8_t enabled);
extern void gfx_pvr_set_fog_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
extern float gfx_pvr_get_u_scale(void);
extern float gfx_pvr_get_v_scale(void);
// PVR streams 3D with NO buf_vbo: OP bakes into op_emit then DR-submits per source-tri
// (pvr_submit_op); PT/TR bake directly into their bucket (pvr_reserve). op_emit holds one
// source triangle's worst-case clipped fan: 3 verts + 5 clip planes (near + 4 pane edges)
// = 8-vert polygon = 6 tris = 18 verts, never more.
static pvr_vertex_t __attribute__((aligned(32))) op_emit[6 * 3];
extern void pvr_submit_op(const pvr_vertex_t *tris, size_t n);
extern pvr_vertex_t *pvr_reserve(int kind, size_t n);
// Inline OP submit for the per-triangle path (the 2D fill path still uses pvr_submit_op): the
// backend call + KOS sq_fast_cpy are two out-of-object hops per triangle whose addresses alias the
// hot front-end code at the linker's whim (8KB direct-mapped I-cache). Same semantics as
// pvr_submit_op: header re-emit only when dirty (gfx_pvr_op_dirty), then one SQ ship of n verts.
extern uint8_t gfx_pvr_op_dirty;
extern void pvr_emit_op_header_slow(void);
// KOS sq_fast_cpy's loop (pair-single fmov.d, one SQ line per iteration), inlined. dst = SQ area
// address, src 32B-aligned, n = number of 32-byte lines (> 0). fschg is balanced inside the asm.
static inline void pvr_sq_ship(void *dst, const void *src, size_t n) {
    void *t;
    __asm__ __volatile__(
        "fschg\n"
        "1:\n\t"
        "fmov.d @%[s]+, dr0\n\t"
        "mov    %[d], %[t]\n\t"
        "fmov.d @%[s]+, dr2\n\t"
        "add    #32, %[t]\n\t"
        "fmov.d @%[s]+, dr4\n\t"
        "fmov.d @%[s]+, dr6\n\t"
        "pref   @%[s]\n\t"
        "dt     %[n]\n\t"
        "fmov.d dr6, @-%[t]\n\t"
        "fmov.d dr4, @-%[t]\n\t"
        "fmov.d dr2, @-%[t]\n\t"
        "fmov.d dr0, @-%[t]\n\t"
        "add    #32, %[d]\n\t"
        "bf.s   1b\n\t"
        "pref   @%[t]\n\t"
        "fschg"
        : [d] "+r" (dst), [s] "+r" (src), [n] "+r" (n), [t] "=&r" (t)
        :
        : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "memory", "t");
}
static inline void pvr_submit_op_inline(const pvr_vertex_t *tris, size_t n) {
    if (gfx_pvr_op_dirty) pvr_emit_op_header_slow();
    pvr_sq_ship(SQ_MASK_DEST(PVR_TA_INPUT), tris, n);
}

// N64 fog mul/offset (G_MW_FOG), sign-extended; the direct per-vertex coefficient in gfx_calc_fog.
static int16_t pvr_fog_mul = 0, pvr_fog_ofs = 0;

static uint8_t pvr_cur_kind = 0;
static uint8_t pvr_cur_bsrc = GFX_BLENDF_SRCALPHA, pvr_cur_bdst = GFX_BLENDF_INVSRCALPHA;

// Derive PVR blend factors from the N64 blender (final cycle in other_mode_l). Standard
// "incoming over framebuffer" maps A->src, B->dst; anything else falls back to alpha-over.
static void pvr_derive_blend(uint32_t oml, uint32_t omh, uint8_t *src, uint8_t *dst) {
    int base = ((omh & (3U << G_MDSFT_CYCLETYPE)) == G_CYC_2CYCLE) ? 16 : 24;
    int p = (oml >> (base + 6)) & 3, m = (oml >> (base + 4)) & 3;
    int a = (oml >> (base + 2)) & 3, b = (oml >> (base + 0)) & 3;
    if (p != G_BL_CLR_IN || m != G_BL_CLR_MEM) {
        *src = GFX_BLENDF_SRCALPHA; *dst = GFX_BLENDF_INVSRCALPHA;
        return;
    }
    *src = (a == G_BL_0) ? GFX_BLENDF_ZERO : GFX_BLENDF_SRCALPHA;
    *dst = (b == G_BL_1MA)   ? GFX_BLENDF_INVSRCALPHA
         : (b == G_BL_A_MEM) ? GFX_BLENDF_DSTALPHA
         : (b == G_BL_1)     ? GFX_BLENDF_ONE
         :                     GFX_BLENDF_ZERO;
}

static struct GfxWindowManagerAPI* gfx_wapi;
static struct GfxRenderingAPI* gfx_rapi;

static uint16_t __attribute__((aligned(32))) tlut[256];

static struct ShaderProgram* gfx_lookup_or_create_shader_program(uint32_t shader_id) {
    struct ShaderProgram* prg = gfx_rapi->lookup_shader(shader_id);
    if (prg == NULL) {
        gfx_rapi->unload_shader(rendering_state.shader_program);
        prg = gfx_rapi->create_and_load_new_shader(shader_id);
        rendering_state.shader_program = prg;
    }
    return prg;
}

void n64_memcpy(void* dst, const void* src, size_t size);

static __attribute__((noinline)) void gfx_generate_cc(struct ColorCombiner* comb, uint32_t cc_id) {
    uint8_t c[2][4];
    uint32_t shader_id = cc_id & 0xff000000;
    uint8_t shader_input_mapping[2][4] = { { 0 } };
    int i, j;

    for (i = 0; i < 4; i++) {
        // (cc_id >> (i * 3)) & 7
        // (cc_id >> (12 + i * 3)) & 7ß
        c[0][i] = (cc_id >> ((i << 1) + i)) & 7;
        c[1][i] = (cc_id >> (12 + ((i << 1) + i))) & 7;
    }
    for (i = 0; i < 2; i++) {
        if (c[i][0] == c[i][1] || c[i][2] == CC_0) {
            c[i][0] = c[i][1] = c[i][2] = 0;
        }
        uint8_t input_number[8] = { 0 };
        int next_input_number = SHADER_INPUT_1;
        for (j = 0; j < 4; j++) {
            int val = 0;
            switch (c[i][j]) {
                case CC_0:
                    break;
                case CC_TEXEL0:
                    val = SHADER_TEXEL0;
                    break;
                case CC_TEXEL1:
                    val = SHADER_TEXEL1;
                    break;
                case CC_TEXEL0A:
                    val = SHADER_TEXEL0A;
                    break;
                case CC_PRIM:
                case CC_SHADE:
                case CC_ENV:
                case CC_LOD:
                    if (input_number[c[i][j]] == 0) {
                        shader_input_mapping[i][next_input_number - 1] = c[i][j];
                        input_number[c[i][j]] = next_input_number++;
                    }
                    val = input_number[c[i][j]];
                    break;
            }
            // (i * 12 + j * 3)
            shader_id |= val << (((i << 3) + (i << 2)) + ((j << 1) + j));
        }
    }
    comb->cc_id = cc_id;
    comb->prg = gfx_lookup_or_create_shader_program(shader_id);
    n64_memcpy(comb->shader_input_mapping, shader_input_mapping, sizeof(shader_input_mapping));
}

static __attribute__((noinline)) struct ColorCombiner* gfx_lookup_or_create_color_combiner(uint32_t cc_id) {
    size_t i;

    static struct ColorCombiner* prev_combiner;
    if (prev_combiner != NULL && prev_combiner->cc_id == cc_id) {
        return prev_combiner;
    }

    for (i = 0; i < color_combiner_pool_size; i++) {
        if (color_combiner_pool[i].cc_id == cc_id) {
            return prev_combiner = &color_combiner_pool[i];
        }
    }
    struct ColorCombiner* comb = &color_combiner_pool[color_combiner_pool_size++];
    gfx_generate_cc(comb, cc_id);
    return prev_combiner = comb;
}

void gfx_clear_texidx(unsigned int texidx);

void reset_texcache(void) {
    gfx_texture_cache.pool_pos = 0;
    memset(&gfx_texture_cache, 0, sizeof(gfx_texture_cache));
}

// Per-channel texel INVERT variant (bits r,g,b) - see pvr_tex_invert_mask. Address bits 27..31 are
// constant for all of RAM (0x8Cxxxxxx), so dropping them keeps the key unique.
static uint8_t pvr_tex_invert_mask = 0;   // set per draw in gfx_sp_tri1 before import_texture

// ---- Per-loaded-vertex combiner-eval cache ----------------------------------------------------
// pvr_eval_combiner (esp. the full 2-cycle path) is the dominant per-triangle cost, and a mesh vertex
// is shared by ~4-6 triangles, so evaluate it ONCE per loaded vertex per combiner STATE: the result
// (argb/oargb, without the per-vertex fog byte) is cached by vertex slot and stamped with the current
// combiner-state stamp; gfx_sp_vertex invalidates the slots it reloads; a state-key change (combine
// words, prim/env, texenv, textured, cycle type, decal, invert mask) bumps the stamp. NOISE materials
// (per-vertex random) and near-clip fan temporaries bypass the cache.
static struct { uint32_t stamp, argb, oargb; } cc_cache[MAX_VERTICES + 2];
static uint32_t cc_state_stamp = 1;
static uint32_t cc_last_key[6];
static inline void cc_cache_invalidate(int first, int n) {
    for (int i = 0; i < n && first + i < MAX_VERTICES + 2; i++) cc_cache[first + i].stamp = 0;
}
static inline uint32_t pack_key(uint32_t a, uint8_t pal) {
    uint32_t key = 0;
    key |= ((uint32_t) (a >> 5)) << 5;             // address (32-byte granular)
    key |= ((uint32_t) (pvr_tex_invert_mask & 7)) << 2;   // 3 bits: r,g,b inverted
    key |= ((uint32_t) (pal & 0x3));               // 2 bits
    return key;
}

static inline uint32_t unpack_A(uint32_t key) {
    // Inverse of pack_key's address field (bits 5.. of the key = addr >> 5, minus the constant top
    // bits). Compared against (segaddr >> 5) with the same truncation in gfx_texture_cache_invalidate.
    uint32_t addr = (uint32_t) ((key >> 5));
    return addr;
}

#if 1
static inline uint16_t hash10(uint32_t x) {
    x ^= x >> 16;                   // mix high 16 into low 16
    x ^= x >> 8;                    // mix 8-bit chunks together
    x ^= x >> 4;                    // more intra-byte mixing
    x ^= x >> 2;                    // tighten it a bit more
    return (uint16_t) (x & 0x3FFu); // keep 10 bits
}
#else
#if 0
static inline uint16_t hash10(uint32_t x) {
    // Knuth / golden-ratio-style multiplicative hash
    x *= 0x9E3779B1u; // 2654435761
    return (uint16_t)(x >> 22); / top 10 bits -> 0..1023
}
#else
static inline uint16_t hash10(uint32_t addr) {
    uint32_t x = addr >> 5;
    x ^= x >> 16;
    x *= 0x85ebca6bu;
    x ^= x >> 13;
    x *= 0xc2b2ae35u;
    x ^= x >> 16;
    return (uint16_t) ((x >> 22) & 0x3ff);
}
#endif
#endif

void gfx_texture_cache_invalidate(void* orig_addr) {
    void* segaddr = SEGMENTED_TO_VIRTUAL(orig_addr);
    int dirtied = 0;
    size_t hash = hash10((uintptr_t) (segaddr));
    uintptr_t addrcomp = (((uintptr_t) segaddr >> 5) & 0x07FFFFFFu);   // match pack_key truncation

    struct TextureHashmapNode** node = &gfx_texture_cache.hashmap[hash];
    uintptr_t last_node = &gfx_texture_cache.pool[gfx_texture_cache.pool_pos];
    while (*node != NULL && ((uintptr_t) *node < last_node)) {
        struct TextureHashmapNode* cur_node = (*node);
        // not convinced there is enough work to do to make this prefetch useful
        //__builtin_prefetch(cur_node->next);
        uintptr_t unpaddr = (uintptr_t) unpack_A((*node)->key);
        if (unpaddr == addrcomp) {
            cur_node->dirty |= (TEX_DIRTY | TEX_DYNAMIC);   // re-upload, and mark churny (sticky)
            return;
        }
        node = &cur_node->next;
    }
}

#define MEM_BARRIER_PREF(ptr) asm volatile("pref @%0" : : "r"((ptr)) : "memory")

static __attribute__((noinline)) uint8_t gfx_texture_cache_lookup(int tile, struct TextureHashmapNode** n,
                                                                  const uint8_t* orig_addr, uint32_t tmem, uint32_t siz,
                                                                  uint8_t pal) {
    (void) tile;  // single-tile rapi: tile selects loaded_texture[] at the caller, not the binding
    void* segaddr = SEGMENTED_TO_VIRTUAL((void*) orig_addr);
    size_t hash = hash10((uintptr_t) (segaddr));
    struct TextureHashmapNode** node = &gfx_texture_cache.hashmap[hash];
    MEM_BARRIER_PREF(*node);

    uint32_t newkey = pack_key(segaddr, pal);

    uintptr_t last_node = &gfx_texture_cache.pool[gfx_texture_cache.pool_pos];
    while (*node != NULL && ((uintptr_t) *node < last_node)) {
        __builtin_prefetch((*node)->next);

        if ((*node)->key == newkey) {
            *n = *node;
            gfx_rapi->select_texture((*node)->texture_id);

            if ((*node)->dirty & TEX_DIRTY) {
                (*node)->dirty &= ~TEX_DIRTY;   // clear dirty; KEEP the sticky TEX_DYNAMIC bit
                return 0;
            } else {
                return 1;
            }
        }
        node = &(*node)->next;
    }

    *node = &gfx_texture_cache.pool[gfx_texture_cache.pool_pos++];

    (*node)->texture_id = gfx_rapi->new_texture();
    gfx_rapi->select_texture((*node)->texture_id);
    gfx_rapi->set_sampler_parameters(0, 0, 0);

    (*node)->key = newkey;
    (*node)->dirty = 0;
    (*node)->cms = 0;
    (*node)->cmt = 0;
    (*node)->linear_filter = 0;
    (*node)->next = NULL;

    *n = *node;
    return 0;
}

extern uint16_t __attribute__((aligned(16384))) rgba16_buf[64 * 64];

static void __attribute__((noinline)) import_texture(int tile);

uint8_t __attribute__((aligned(32))) table256[256] = { 0 };

uint8_t __attribute__((aligned(32))) table32[32] = { 0,  6,  8,  10, 11, 12, 14, 15, 16, 17, 18, 18, 19, 20, 21, 22,
                                                     22, 23, 24, 24, 25, 26, 26, 27, 27, 28, 28, 29, 29, 30, 30, 31 };

uint8_t __attribute__((aligned(32))) table16[16] = { 0, 4, 5, 7, 8, 9, 9, 10, 11, 12, 12, 13, 13, 14, 14, 15 };

static uint16_t brightit_argb1555(uint16_t c) {
    uint8_t a = (c >> 15) & 1;
    uint8_t r = (c >> 10) & 0x1f;
    uint8_t g = (c >> 5) & 0x1f;
    uint8_t b = (c) & 0x1f;

    return (a << 15) | (table32[r] << 10) | (table32[g] << 5) | (table32[b]);
}

typedef enum LevelId {
    /* -1 */ LEVEL_UNK_M1 = -1,
    /*  0 */ LEVEL_CORNERIA,
    /*  1 */ LEVEL_METEO,
    /*  2 */ LEVEL_SECTOR_X,
    /*  3 */ LEVEL_AREA_6,
    /*  4 */ LEVEL_UNK_4,
    /*  5 */ LEVEL_SECTOR_Y,
    /*  6 */ LEVEL_VENOM_1,
    /*  7 */ LEVEL_SOLAR,
    /*  8 */ LEVEL_ZONESS,
    /*  9 */ LEVEL_VENOM_ANDROSS,
    /* 10 */ LEVEL_TRAINING,
    /* 11 */ LEVEL_MACBETH,
    /* 12 */ LEVEL_TITANIA,
    /* 13 */ LEVEL_AQUAS,
    /* 14 */ LEVEL_FORTUNA,
    /* 15 */ LEVEL_UNK_15,
    /* 16 */ LEVEL_KATINA,
    /* 17 */ LEVEL_BOLSE,
    /* 18 */ LEVEL_SECTOR_Z,
    /* 19 */ LEVEL_VENOM_2,
    /* 20 */ LEVEL_VERSUS,
    /* 77 */ LEVEL_WARP_ZONE = 77,
} LevelId;

extern LevelId gCurrentLevel;

extern uint16_t scaled2[];
int do_the_blur = 0;
// ---- GFXPROF: per-frame renderer profile to serial (edit->build->read-serial workflow) ------------
// Prints one greppable line per second (every 60 frames) plus any frame whose interpreter walk exceeds
// GFX_PROF_SLOW_US: walk = DL interpretation incl. TA submission, flush = PT/TR bucket replay,
// finish = pvr_scene_finish (blocks if the GPU is still on the previous frame -> GPU-bound signal).
#define GFX_PROF 0
#define GFX_PROF_SLOW_US 22000
#if GFX_PROF
static uint32_t prof_tris = 0, prof_verts = 0, prof_mtx = 0, prof_texup = 0, prof_frame = 0;
static uint64_t prof_t_walk = 0, prof_t_flush = 0, prof_t_finish = 0;
static uint64_t prof_t_vtx = 0, prof_t_tri = 0, prof_t_tri_setup = 0, prof_t_tri_mark = 0;
static uint64_t prof_t_clip = 0, prof_t_bake = 0, prof_t_submit = 0;
static uint32_t prof_evals = 0, prof_hits = 0, prof_stamps = 0, prof_setups = 0;
static uint32_t prof_op_bumps[256];
static uint64_t prof_su[4];   // setup sections: 0 depth/viewport, 1 combiner/shader, 2 texture, 3 blend/texenv/key
static uint64_t prof_t_quad = 0, prof_t_mtx = 0; static uint32_t prof_quads = 0, prof_quads_fast = 0;
// PRFC1 stall sampling: rotate through pipeline-freeze modes, one per second; delta measured over walk.
static const struct { int mode; const char* name; } prof_stall_modes[] = {
    { PMCR_PIPELINE_FREEZE_BY_ICACHE_MISS_MODE, "icache" }, { PMCR_PIPELINE_FREEZE_BY_DCACHE_MISS_MODE, "dcache" },
    { PMCR_PIPELINE_FREEZE_BY_FPU_MODE, "fpu" },            { PMCR_PIPELINE_FREEZE_BY_BRANCH_MODE, "branch" },
    { PMCR_PIPELINE_FREEZE_BY_CPU_REGISTER_MODE, "reg" },
};
static int prof_stall_idx = -1; static uint64_t prof_stall_walk = 0;
// SH4 performance counter in elapsed-cycle mode (perf_cntr_timer_enable in gfx_init): a couple of
// register reads per sample vs the TMU2 verify-loop + 64-bit maths of timer_us_gettime64. Values are
// CPU cycles; converted to us (/200) at print.
#include <dc/perfctr.h>
#define PROF_NOW() perf_cntr_count(PRFC0)
#define PROF_US(c) ((unsigned long) ((c) / 200u))
extern uint32_t gfx_pvr_prof_op, gfx_pvr_prof_pt, gfx_pvr_prof_tr;
#define PROF_INC(x) ((x)++)
#define PROF_ADD(x, n) ((x) += (n))
#else
#define PROF_INC(x) ((void) 0)
#define PROF_ADD(x, n) ((void) 0)
#endif
// Custom BLND sub-flag 'OVLY' (gSPPaintOverlay in fox_map.c): force the wrapped DL onto the near
// paint-order overlay path. For XLU (no-Z-write) geometry the N64 composites in DRAW ORDER, but PVR
// autosort orders TR polys per-pixel by depth, so an XLU surface drawn AFTER a nearer XLU surface
// (briefing TV glow after the map planet/nebula) loses on PVR. Toggle; reset each frame.
int force_paint_overlay = 0;
// Upload-layout hint: set by import_texture() from the bound node's TEX_DYNAMIC bit right before the
// upload, consumed by gfx_pvr_upload_texture(). 1 = twiddled (pvr_txr_load_ex; static, faster to
// sample), 0 = non-twiddled (pvr_txr_load; dynamic/churny, avoids the per-reload twiddle CPU cost).
int gfx_pvr_next_twiddled = 1;
// Set inline by the convert loops (AND-accumulate over texel alpha), consumed+reset by
// gfx_pvr_upload_texture(). 1 iff every texel is fully opaque -> lets the 2D-backdrop path route an
// effectively-opaque XLU quad to the OP list even when its combine reads texel alpha. Default 0
// (safe: convert paths that don't compute it just don't get the OP-routing optimization).
int gfx_pvr_next_opaque = 0;
extern u16 aTiBackdropTex[];

extern float Rand_ZeroOne(void);

static void import_texture_rgba16_alphanoise(int tile) {
    uint32_t i;
    uint32_t width = rdp.texture_tile.line_size_bytes >> 1;
    uint32_t height =
        (uint32_t) ((float) rdp.loaded_texture[tile].size_bytes / (float) rdp.texture_tile.line_size_bytes);

    uint32_t loopcount = rdp.loaded_texture[tile].size_bytes >> 1;
    uint16_t* start = (uint16_t*) rdp.loaded_texture[tile].addr;
    for (i = 0; i < loopcount; i++) {
        SHZ_PREFETCH(start + (i*16));
        uint16_t col16 = start[i];
        col16 = brightit_argb1555(((col16 & 1) << 15) | (col16 >> 1));
        if (col16 == 0x8000) {
            rgba16_buf[i] = 0;
        } else {
            uint8_t rb1 = Rand_ZeroOne() > 0.5f;
            uint8_t rb2 = Rand_ZeroOne() < 0.5f;
            uint8_t rb3 = Rand_ZeroOne() > 0.5f;
            uint8_t new_alpha = (rb1 << 2) | (rb2 << 1) | (rb3);
            uint8_t r = (col16 >> 11) & 0xf;
            uint8_t g = (col16 >> 6) & 0xf;
            uint8_t b = (col16 >> 1) & 0xf;
            rgba16_buf[i] = (new_alpha << 12) | (r << 8) | (g << 4) | (b);
        }
    }

    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, PVR_TXRFMT_ARGB4444);
}

static void import_texture_rgba16_alphablur(int tile) {
    // Captured framebuffer is native RGB565 (PM_RGB565) -> upload as RGB565, no per-pixel conversion.
    gfx_rapi->upload_texture((uint8_t*) scaled2, 256, 128, PVR_TXRFMT_RGB565);
}

static void import_texture_rgba16(int tile) {
    MEM_BARRIER_PREF(table32);
    uint32_t i;
    uint32_t width = rdp.texture_tile.line_size_bytes >> 1;
    uint32_t height =
        (uint32_t) ((float) rdp.loaded_texture[tile].size_bytes / (float) rdp.texture_tile.line_size_bytes);

    uint32_t loopcount = rdp.loaded_texture[tile].size_bytes >> 1;
    uint16_t* start = (uint16_t*) rdp.loaded_texture[tile].addr;
    uint16_t alpha_acc = 0xFFFF;   // 5551 alpha is bit 0; AND stays set iff every texel is opaque
    for (i = 0; i < loopcount; i++) {
        SHZ_PREFETCH(start + (i*16));
        uint16_t col16 = start[i];
        alpha_acc &= col16;
        rgba16_buf[i] = brightit_argb1555(((col16 & 1) << 15) | (col16 >> 1));
    }
    gfx_pvr_next_opaque = (alpha_acc & 1);
    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, PVR_TXRFMT_ARGB1555);
}

static void import_texture_rgba32(int tile) {
    MEM_BARRIER_PREF(table16);
    uint32_t width = rdp.texture_tile.line_size_bytes >> 1;
    uint32_t height =
        (uint32_t) ((float) (rdp.loaded_texture[tile].size_bytes >> 1) / (float) rdp.texture_tile.line_size_bytes);
    uint32_t* startaddr = (uint8_t*) ((uintptr_t) rdp.loaded_texture[tile].addr & ~3);
    uint32_t wxh = width * height;
    uint8_t alpha_and = 0xF;   // 4444 alpha nibble; == 0xF after the loop iff every texel is opaque
    for (uint32_t i = 0; i < wxh; i++) {
        SHZ_PREFETCH(startaddr + (i*8));
        uint32_t p = startaddr[i];
        uint8_t r = table16[(p >> 28) & 0x0f];
        uint8_t g = table16[(p >> 20) & 0x0f];
        uint8_t b = table16[(p >> 12) & 0x0f];
        uint8_t a = (p >> 4) & 0x0f;

        alpha_and &= a;
        rgba16_buf[i] = (a << 12) | (r << 8) | (g << 4) | (b);
    }
    gfx_pvr_next_opaque = (alpha_and == 0xF);
    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, PVR_TXRFMT_ARGB4444);
}

static void import_texture_ia8(int tile) {
    uint32_t width = rdp.texture_tile.line_size_bytes;
    uint32_t height =
        (uint32_t) ((float) rdp.loaded_texture[tile].size_bytes / (float) rdp.texture_tile.line_size_bytes);
    __builtin_prefetch(table16);
    uint8_t* start = (uint8_t*) rdp.loaded_texture[tile].addr;

    for (uint32_t i = 0; i < rdp.loaded_texture[tile].size_bytes; i++) {
        uint8_t val = start[i];
        uint8_t in = table16[((val >> 4) & 0xf)];
        uint8_t al = (val & 0xf);
        rgba16_buf[i] = (al << 12) | (in << 8) | (in << 4) | in;
    }
    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, PVR_TXRFMT_ARGB4444);
}

static void import_texture_ia16(int tile) {
    uint32_t i;
    uint32_t width = rdp.texture_tile.line_size_bytes >> 1;
    __builtin_prefetch(table16);
    uint32_t height =
        (uint32_t) ((float) rdp.loaded_texture[tile].size_bytes / (float) rdp.texture_tile.line_size_bytes);

    uint16_t* start = (uint16_t*) rdp.loaded_texture[tile].addr;
    for (i = 0; i < rdp.loaded_texture[tile].size_bytes / 2; i++) {
        uint16_t p = start[i];
        uint8_t al = (p >> 4) & 0xF;
        uint8_t in = table16[(p >> 12) & 0xF];
        rgba16_buf[i] = (al << 12) | (in << 8) | (in << 4) | in;
    }

    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, PVR_TXRFMT_ARGB4444);
}

static void DO_LOAD_TLUT(void);

static void import_texture_ci4(int tile) {
    uint8_t offs = rdp.last_palette << 4;
    uint16_t* offs_tlut = (uint16_t*) &tlut[offs];
    __builtin_prefetch(offs_tlut);

    uint32_t width = rdp.texture_tile.line_size_bytes << 1;
    uint32_t height =
        (uint32_t) ((float) rdp.loaded_texture[tile].size_bytes / (float) rdp.texture_tile.line_size_bytes);
    uint32_t* rgba32_buf = (uint32_t*) rgba16_buf;

    uint32_t i;
    uint8_t part1, part2;
    uint8_t byte;

    for (i = 0; i < rdp.loaded_texture[tile].size_bytes; i++) {
        uint8_t byte = rdp.loaded_texture[tile].addr[i];
        part1 = (byte >> 4) & 0xf;
        part2 = byte & 0xf;
        rgba32_buf[i] = (offs_tlut[part2] << 16) | offs_tlut[part1];
    }

    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, PVR_TXRFMT_ARGB1555);
}

static __attribute__((noinline)) void import_texture_ci8(int tile) {
    uint32_t* intex32 = (uint32_t*) rdp.loaded_texture[tile].addr;
    __builtin_prefetch((uintptr_t) intex32 & ~31);
    uint32_t width = rdp.texture_tile.line_size_bytes;
    uint32_t height =
        (uint32_t) ((float) rdp.loaded_texture[tile].size_bytes / (float) rdp.texture_tile.line_size_bytes);

    uint32_t count = rdp.loaded_texture[tile].size_bytes;
    uint32_t* tex32 = (uint32_t) rgba16_buf;
    for (uint32_t i = 0; i < count; i += 4) {
        uint32_t fourpix1 = *intex32++;

        MEM_BARRIER_PREF(tex32);

        uint16_t t1, t2, t3, t4;

        t4 = tlut[(fourpix1 >> 24) & 0xff];
        t3 = tlut[(fourpix1 >> 16) & 0xff];
        t2 = tlut[(fourpix1 >> 8) & 0xff];
        t1 = tlut[(fourpix1) & 0xff];

        MEM_BARRIER_PREF(((uintptr_t) intex32 & ~31) + 32);

        *tex32++ = (t2 << 16) | t1;
        *tex32++ = (t4 << 16) | t3;
    }

    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, PVR_TXRFMT_ARGB1555);
}

#include <kos.h>

static void __attribute__((noinline)) import_texture(int tile) {
    int cache_lookup_rv;

    uint8_t fmt = rdp.texture_tile.fmt;
    uint8_t siz = rdp.texture_tile.siz;
    uint32_t tmem = rdp.texture_to_load.tmem;

    if (alpha_noise && (siz == G_IM_SIZ_16b))
        gfx_texture_cache_invalidate((void*) rdp.loaded_texture[tile].addr);

    cache_lookup_rv = gfx_texture_cache_lookup(tile, &rendering_state.textures[tile], rdp.loaded_texture[tile].addr,
                                               tmem, siz, rdp.last_palette);

    __builtin_prefetch(SEGMENTED_TO_VIRTUAL(rdp.loaded_texture[tile].addr));

    if (cache_lookup_rv)
        return;

    // Choose upload layout for the variants below: static (never invalidated) -> twiddled;
    // dynamic (TEX_DYNAMIC set by a prior gfx_texture_cache_invalidate) -> non-twiddled.
    gfx_pvr_next_twiddled = !(rendering_state.textures[tile]->dirty & TEX_DYNAMIC);

    shz_dcache_alloc_line(rgba16_buf);

    // Converters upload directly; the invert post-pass is applied inside gfx_pvr upload via
    // pvr_tex_invert_mask (see gfx_pvr_upload_texture: 4444/1555 channel invert). Nothing here.
    PROF_INC(prof_texup);   // cache miss -> convert + upload
    if (fmt == G_IM_FMT_RGBA) {
        if (siz == G_IM_SIZ_16b) {
            if (alpha_noise) {
                import_texture_rgba16_alphanoise(tile);
            } else if (do_the_blur) {
                import_texture_rgba16_alphablur(tile);
            } else {
                import_texture_rgba16(tile);
            }
        } else if (siz == G_IM_SIZ_32b) {
            import_texture_rgba32(tile);
        }
    } else if (fmt == G_IM_FMT_IA) {
        if (siz == G_IM_SIZ_8b) {
            import_texture_ia8(tile);
        } else if (siz == G_IM_SIZ_16b) {
            import_texture_ia16(tile);
        }
    } else if (fmt == G_IM_FMT_CI) {
        if (rdp.palette_dirty) {
            DO_LOAD_TLUT();
        }

        if (siz == G_IM_SIZ_4b) {
            import_texture_ci4(tile);
        } else if (siz == G_IM_SIZ_8b) {
            import_texture_ci8(tile);
        }
    }
}

static void gfx_normalize_vector(float v[3]) {
    shz_vec3_t norm = shz_vec3_normalize((shz_vec3_t) { .x = v[0], .y = v[1], .z = v[2] });
    v[0] = norm.x;
    v[1] = norm.y;
    v[2] = norm.z;
}

static void gfx_transposed_matrix_mul(float res[3], const float a[3], const float b[4][4]) {
    *((shz_vec3_t*) res) = shz_matrix4x4_trans_vec3_transpose(b, *((shz_vec3_t*) a));
}

static void calculate_normal_dir(const Light_t* light, float coeffs[3]) {
    float light_dir[3] = { light->dir[0] * recip127, light->dir[1] * recip127, light->dir[2] * recip127 };
    gfx_transposed_matrix_mul(
        coeffs, light_dir,
        (const float (*)[4]) rsp.modelview_matrix_stack[0]); // rsp.modelview_matrix_stack_size - 1]);
    gfx_normalize_vector(coeffs);
}

static void gfx_matrix_mul(shz_matrix_4x4_t* res, const shz_matrix_4x4_t* a, const shz_matrix_4x4_t* b) {
    shz_xmtrx_load_4x4_apply_store(res, b, a);
}

static int matrix_dirty = 0;

static __attribute__((noinline)) void gfx_sp_matrix_impl(uint8_t parameters, const void* addr);
static void gfx_sp_matrix(uint8_t parameters, const void* addr) {
#if GFX_PROF
    uint64_t t0 = PROF_NOW(); gfx_sp_matrix_impl(parameters, addr); prof_t_mtx += PROF_NOW() - t0;
#else
    gfx_sp_matrix_impl(parameters, addr);
#endif
}
static __attribute__((noinline)) void gfx_sp_matrix_impl(uint8_t parameters, const void* addr) {
    void* segaddr = (void*) SEGMENTED_TO_VIRTUAL((void*) addr);
    float matrix[4][4] __attribute__((aligned(32)));
    int recompute = 0;

#ifndef GBI_FLOATS
    int32_t* saddr = (int32_t*) segaddr;
    // Original GBI where fixed point matrices are used
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j += 2) {
            int32_t int_part = saddr[i * 2 + j / 2];
            uint32_t frac_part = saddr[8 + i * 2 + j / 2];
            matrix[i][j] = (int32_t) ((int_part & 0xffff0000) | (frac_part >> 16)) / 65536.0f;
            matrix[i][j + 1] = (int32_t) ((int_part << 16) | (frac_part & 0xffff)) / 65536.0f;
        }
    }
#else
    // For a modified GBI where fixed point values are replaced with floats
#endif
    if (matrix_dirty) {
        recompute = 1;
    }
    PROF_INC(prof_mtx);

    matrix_dirty = 1;

    // the following is specialized for STAR FOX 64 ONLY
    if (parameters & G_MTX_PROJECTION) {
#ifdef GBI_FLOATS
        shz_xmtrx_load_4x4_unaligned(segaddr);
#endif
        shz_xmtrx_store_4x4(matrix);
        recompute = 1;
        if (parameters & G_MTX_LOAD) {
            shz_xmtrx_store_4x4(rsp.P_matrix);
        } else {
            gfx_matrix_mul(rsp.P_matrix, matrix, rsp.P_matrix);
        }
        // Classify the projection as orthographic (2D/overlay) vs perspective (3D) for the PVR
        // depth/overlay logic in gfx_sp_tri1. Ortho has P[3][3]~1 (w passthrough) and P[2][3]~0.
        proj_is_ortho = (rsp.P_matrix[3][3] > 0.5f) && (rsp.P_matrix[2][3] > -0.5f);
        // cur_frame_persp is deliberately NOT set here: merely LOADING a perspective matrix must
        // not arm the backdrop machinery. The option menus (Option_DrawMenuCard) end every frame
        // with Lib_InitPerspective and never draw a perspective triangle - with the flag set at
        // matrix load, prev_frame_had_persp stayed 1 on the pure-2D VS player-select screen,
        // has_drawn_persp_tri stayed 0, and the effectively-opaque-quad backdrop promotion stayed
        // permanently armed: the opaque RGBA16 face portraits got promoted to the far OP slab
        // while the (transparent-cornered, unpromoted) panel frame stayed near TR and its black
        // interior overdrew them. Set where perspective geometry actually EMITS (gfx_sp_tri1).
    } else {
        // G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
        if (parameters == 0) {
#ifdef GBI_FLOATS
            shz_xmtrx_load_4x4_unaligned(segaddr);
#endif
            shz_xmtrx_store_4x4(matrix);
            gfx_matrix_mul(rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1], matrix,
                           rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1]);
            rsp.lights_changed = 1;
            recompute = 1;
        } else
            // G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
            if (parameters == 2) {
#ifdef GBI_FLOATS
                shz_xmtrx_load_4x4_unaligned(segaddr);
#endif
                shz_xmtrx_store_4x4(matrix);
                if (rsp.modelview_matrix_stack_size == 0)
                    rsp.modelview_matrix_stack_size = 1;
                shz_xmtrx_store_4x4(rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1]);
                rsp.lights_changed = 1;
                recompute = 1;
            } else
                // G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW
                if (parameters == 4) {
                    if (rsp.modelview_matrix_stack_size < 4) {
                        ++rsp.modelview_matrix_stack_size;
                        shz_matrix_4x4_copy(rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1],
                                            rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 2]);
                    }
                    // STAR FOX 64 ONLY
                    // only ever pushing identity matrix, no need to multiply
                    // gfx_matrix_mul(rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1], matrix,
                    // rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1]);
                }
    }

    if (recompute) {
        gfx_matrix_mul(rsp.MP_matrix, rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1], rsp.P_matrix);
    }
}

// FOR STAR FOX 64 ONLY
// only used by Titania and it is fine if you dont reconstitute the MP matrix
// popping identity matrix
// count is only ever 1
static void __attribute__((noinline)) gfx_sp_pop_matrix(void) {
    if (rsp.modelview_matrix_stack_size > 0) {
        --rsp.modelview_matrix_stack_size;
    }
    matrix_dirty = 1;
}

#define MAX3(a, b, c) (MAX(MAX((a), (b)), (c)))
#define MAX4(a, b, c, d) (MAX(MAX3((a), (b), (c)), (d)))
#define MAX5(a, b, c, d, e) (MAX(MAX4((a), (b), (c), (d)), (e)))

// 4 / 127
#define light0_scale 0.03149606f
// 3 / 127
#define light4_scale 0.02362205f

#define MEM_BARRIER() asm volatile("" : : : "memory");

#include "sh4zam.h"

typedef enum {
    /*   0 */ GSTATE_NONE,
    /*   1 */ GSTATE_INIT,
    /*   2 */ GSTATE_TITLE,
    /*   3 */ GSTATE_MENU,
    /*   4 */ GSTATE_MAP,
    /*   5 */ GSTATE_GAME_OVER,
    /*   6 */ GSTATE_VS_INIT,
    /*   7 */ GSTATE_PLAY,
    /*   8 */ GSTATE_ENDING,
    /* 100 */ GSTATE_BOOT = 100,
    /* 101 */ GSTATE_BOOT_WAIT,
    /* 102 */ GSTATE_SHOW_LOGO,
    /* 103 */ GSTATE_CHECK_SAVE,
    /* 104 */ GSTATE_LOGO_WAIT,
    /* 105 */ GSTATE_START,
} GameState;
extern GameState gGameState;

float __attribute__((aligned(32))) ENV_MTX[3][3];
float __attribute__((aligned(32))) COEFF_MTX[3][3];
float __attribute__((aligned(32))) COLOR_MTX[3][3];

// N64 per-vertex fog coefficient from clip z/w: fog = clamp(fog_mul*(z/w)+fog_offset, 0..255).
// 0 when fog is off (G_FOG clear) or the vertex is behind the near plane, so non-fogged frames
// pay nothing. Baked into PVR oargb.alpha (HW vertex fog) at emit.
// ---- SF64 fog: faithful N64 formula + perceptual black/colour GAMMA -------------------------------
// alpha = fog_mul*(z/w) + fog_offset, clamped 0..255 - faithful N64 fog (z/w uses the authentic near=10
// in gameplay, see Lib_InitPerspective). Then a per-colour GAMMA reshapes the curve WITHOUT moving the
// endpoints (0->0, 255->255), so black fog still reaches full black at distance (no grey veil) - a plain
// multiply couldn't do that (scaling down lowers the ceiling => distant void goes grey).
// BLACK uses a GAMMA (gamma>1 -> clearer near/mid, endpoints pinned so far stays full black, no grey).
// COLOUR uses a clamped MULTIPLY (gFogColorGain) - that was already ~perfect, and multiplying UP keeps
// the far at full fog-colour via the clamp (only multiplying DOWN would lower the ceiling / grey it).
// Applied via a 256-entry LUT (per-vertex path = one table lookup); LUTs rebuilt only when a knob edits.
float gFogGammaDark = 1.5f;    // black/space fog: gamma reshape
float gFogColorGain = 1.15f;   // colored/planet fog: clamped multiply (the perfect one)
static uint8_t sFogLutDark[256], sFogLutColor[256];
static const uint8_t* sFogLut = NULL;                       // active LUT (by fog colour); NULL = identity
static float sLutGDark = -1.0f, sLutGColor = -1.0f;         // cached knobs to detect edits
static void gfx_fog_build_luts(void) {
    for (int i = 0; i < 256; i++) {
        float t = (float) i * (1.0f / 255.0f);
        sFogLutDark[i] = (uint8_t) (255.0f * powf(t, gFogGammaDark) + 0.5f);   // BLACK: gamma
        float c = (float) i * gFogColorGain;                                   // COLOUR: clamped gain
        sFogLutColor[i] = (uint8_t) (c > 255.0f ? 255.0f : c);
    }
    sLutGDark = gFogGammaDark;
    sLutGColor = gFogColorGain;
}
static inline uint8_t gfx_calc_fog(float z, float w) {
    if (!(rsp.geometry_mode & G_FOG)) return 0;
    if (z < -w) return 0;   // behind the near plane -> no fog
    float f = (float) pvr_fog_mul * (z * shz_fast_invf(w)) + (float) pvr_fog_ofs;
    if (f > 255.0f) f = 255.0f;
    else if (f < 0.0f) f = 0.0f;
    return sFogLut ? sFogLut[(int) f] : (uint8_t) f;
}

inline static uint8_t trivial_reject(float x, float y, float z, float w) {
    uint8_t cr = 0;

    if (z > w)
        cr |= 32;
    if (z < -w)
        cr |= 16;

    if (y > w)
        cr |= 8;
    if (y < -w)
        cr |= 4;

    if (x > w)
        cr |= 2;
    if (x < -w)
        cr |= 1;

    return cr;
}


static void __attribute__((noinline)) gfx_sp_vertex_light_step1(int n_vertices, int dest_index,
                                                                const Vtx* vertices) {
    shz_dcache_alloc_line(&rsp.loaded_vertices[dest_index]);
    for (int i = 0; i < n_vertices; i++, dest_index++) {
        const Vtx_tn* vn = &vertices[i].n;
        struct LoadedVertex* d = &rsp.loaded_vertices[dest_index];
        struct LoadedNormal* n = &rsp.loaded_normals[dest_index];
        MEM_BARRIER_PREF(n);

        float x, y, z, w;
        shz_vec4_t out =
            shz_xmtrx_trans_vec4((shz_vec4_t) { .x = vn->ob[0], .y = vn->ob[1], .z = vn->ob[2], .w = 1.0f });

        d->x = vn->ob[0];
        d->y = vn->ob[1];
        d->z = vn->ob[2];

        MEM_BARRIER();

        x = out.x;
        y = out.y;
        z = out.z;
        w = out.w;

        MEM_BARRIER();

        float recw = shz_fast_invf(w);

        shz_dcache_alloc_line(d + 1);

        // trivial clip rejection
        uint8_t cr = 128 | ((w < 0) ? 64 : 0x00);
        clip_rej[dest_index] = cr | trivial_reject(x, y, z, w);
        sc_gen_v[dest_index] = 0;   // scissor outcode stale (recomputed lazily in gfx_sp_tri1)

        d->u = (vn->tc[0] * rsp.texture_scaling_factor.s) * recip64k;
        d->v = (vn->tc[1] * rsp.texture_scaling_factor.t) * recip64k;

        MEM_BARRIER_PREF(vn + 1);

        // PVR: store RAW clip coords (the front-end does the perspective divide at emit) + fog.
        d->_x = x; d->_y = y; d->_z = z; d->_w = w;
        d->fog = gfx_calc_fog(z, w);

        n->x = vn->n[0];
        n->y = vn->n[1];
        n->z = vn->n[2];
        d->color.a = vn->a;
    }
}

static void __attribute__((noinline)) gfx_sp_vertex_light_step1b(int n_vertices, int dest_index) {
    //SHZ_PREFETCH(&rsp.loaded_normals[dest_index]);
    struct LoadedVertex* d = &rsp.loaded_vertices[dest_index];
    struct LoadedNormal* n = &rsp.loaded_normals[dest_index];
    for (int i = 0; i < n_vertices; i++) {
        shz_vec3_t dot = shz_xmtrx_trans_vec3(shz_vec3_deref(n));
        //MEM_BARRIER_PREF((n + 2));
        if (rsp.geometry_mode & G_TEXTURE_GEN_LINEAR) {
            dot.x = shz_acosf(dot.x) * recip2pi;
            dot.y = shz_acosf(dot.y) * recip2pi;
        } else {
            dot.x = (dot.x * 0.25f) + 0.25f;
            dot.y = (dot.y * 0.25f) + 0.25f;
        }
        n++;
        d->u = (dot.x * rsp.texture_scaling_factor.s);
        d->v = (dot.y * rsp.texture_scaling_factor.t);
        d++;
    }
}

static void __attribute__((noinline)) gfx_sp_vertex_light_step2(int n_vertices, int dest_index) {
    //SHZ_PREFETCH(&rsp.loaded_normals[dest_index]);
    struct LoadedNormal* n = &rsp.loaded_normals[dest_index];
    for (int i = 0; i < n_vertices; i++) {
        shz_vec3_t outinten = shz_xmtrx_trans_vec3(shz_vec3_deref(n));
        //SHZ_PREFETCH((n + 2));
        n->z = 1.0f;
        n->x = MAX(0.0f, outinten.x);
        n->y = MAX(0.0f, outinten.y);
        n++;
    }
}

static void __attribute__((noinline)) gfx_sp_vertex_light_step3(int n_vertices, int dest_index, const Vtx* vertices) {
    //SHZ_PREFETCH(&rsp.loaded_normals[dest_index]);
    struct LoadedVertex* d = &rsp.loaded_vertices[dest_index];
    struct LoadedNormal* n = &rsp.loaded_normals[dest_index];
    for (int i = 0; i < n_vertices; i++) {
        uint8_t r;
        uint8_t g;
        uint8_t b;
        shz_vec3_t outrgb = shz_xmtrx_trans_vec3(shz_vec3_deref(n));
        //SHZ_PREFETCH((n + 2));
#if SCALE_LIGHTS
        float max_c = MAX4(255.0f, outrgb.x, outrgb.y, outrgb.z);
        float maxc = shz_div_posf(255.0f, (float) max_c);

        r = (uint8_t) (outrgb.x * maxc);
        g = (uint8_t) (outrgb.y * maxc);
        b = (uint8_t) (outrgb.z * maxc);
#else
        r = (uint8_t) MIN(255.0f, outrgb.x);
        g = (uint8_t) MIN(255.0f, outrgb.y);
        b = (uint8_t) MIN(255.0f, outrgb.z);

#endif
        n++;
        d->color.r = table256[r];
        d->color.g = table256[g];
        d->color.b = table256[b];
        d++;
    }
}
static void __attribute__((noinline)) gfx_sp_vertex_no(uint8_t n_vertices, uint8_t dest_index, const Vtx* vertices) {
    MEM_BARRIER_PREF(vertices);
    shz_dcache_alloc_line(&rsp.loaded_vertices[dest_index]);
    for (uint8_t i = 0; i < n_vertices; i++, dest_index++) {
        const Vtx_t* v = &vertices[i].v;
        struct LoadedVertex* d = &rsp.loaded_vertices[dest_index];
        shz_vec4_t out = shz_xmtrx_trans_vec4((shz_vec4_t) { .x = v->ob[0], .y = v->ob[1], .z = v->ob[2], .w = 1.0f });
        MEM_BARRIER_PREF(v + 1);
        d->x = v->ob[0];
        d->y = v->ob[1];
        d->z = v->ob[2];
        d->color.r = table256[v->cn[0]];
        d->color.g = table256[v->cn[1]];
        d->color.b = table256[v->cn[2]];
        d->color.a = v->cn[3];
        MEM_BARRIER();
        float x, y, z, w;
        x = out.x;
        y = out.y;
        z = out.z;
        w = out.w;

        MEM_BARRIER();

        float recw = shz_fast_invf(w);

        d->u = (v->tc[0] * rsp.texture_scaling_factor.s) * recip64k;
        d->v = (v->tc[1] * rsp.texture_scaling_factor.t) * recip64k;

        shz_dcache_alloc_line(&rsp.loaded_vertices[dest_index + 1]);

        d->_x = x; d->_y = y; d->_z = z; d->_w = w;
        d->fog = gfx_calc_fog(z, w);

        // trivial clip rejection
        uint8_t cr = ((w < 0) ? 64 : 0x00);
        clip_rej[dest_index] = cr | trivial_reject(x, y, z, w);
        sc_gen_v[dest_index] = 0;   // scissor outcode stale (recomputed lazily in gfx_sp_tri1)
    }
}

// y before x in the index number in the abi cmd that loads the lookats
#define LOOKAT_Y_IDX 0
#define LOOKAT_X_IDX 1
#define LCOEFF_Y_IDX 1
#define LCOEFF_X_IDX 0

static void __attribute__((noinline)) gfx_sp_vertex_impl(uint8_t n_vertices, uint8_t dest_index, const Vtx* vertices);
static void gfx_sp_vertex(uint8_t n_vertices, uint8_t dest_index, const Vtx* vertices) {
    cc_cache_invalidate(dest_index, n_vertices);
#if GFX_PROF
    uint64_t t0 = PROF_NOW();
    gfx_sp_vertex_impl(n_vertices, dest_index, vertices);
    prof_t_vtx += PROF_NOW() - t0;
#else
    gfx_sp_vertex_impl(n_vertices, dest_index, vertices);
#endif
}
static void __attribute__((noinline)) GFX_HOT gfx_sp_vertex_impl(uint8_t n_vertices, uint8_t dest_index, const Vtx* vertices) {
    PROF_ADD(prof_verts, n_vertices);
    shz_xmtrx_load_4x4(&rsp.MP_matrix);

    if (!(rsp.geometry_mode & G_LIGHTING)) {
        gfx_sp_vertex_no(n_vertices, dest_index, SEGMENTED_TO_VIRTUAL(vertices));
        return;
    }

        if (rsp.lights_changed) {
            calculate_normal_dir(&rsp.current_lights[0], rsp.current_lights_coeffs[0]);
            calculate_normal_dir(&rsp.current_lights[4], rsp.current_lights_coeffs[4]);
            calculate_normal_dir(&rsp.lookat[0], rsp.current_lookat_coeffs[0]);
            calculate_normal_dir(&rsp.lookat[1], rsp.current_lookat_coeffs[1]);
            rsp.lights_changed = 0;

            ENV_MTX[0][0] = rsp.current_lookat_coeffs[1][0] * recip127;
            ENV_MTX[1][0] = rsp.current_lookat_coeffs[1][1] * recip127;
            ENV_MTX[2][0] = rsp.current_lookat_coeffs[1][2] * recip127;
            ENV_MTX[0][1] = rsp.current_lookat_coeffs[0][0] * recip127;
            ENV_MTX[1][1] = rsp.current_lookat_coeffs[0][1] * recip127;
            ENV_MTX[2][1] = rsp.current_lookat_coeffs[0][2] * recip127;

            COEFF_MTX[0][0] = rsp.current_lights_coeffs[0][0] * light0_scale;
            COEFF_MTX[1][0] = rsp.current_lights_coeffs[0][1] * light0_scale;
            COEFF_MTX[2][0] = rsp.current_lights_coeffs[0][2] * light0_scale;
            COEFF_MTX[0][1] = rsp.current_lights_coeffs[4][0] * light4_scale;
            COEFF_MTX[1][1] = rsp.current_lights_coeffs[4][1] * light4_scale;
            COEFF_MTX[2][1] = rsp.current_lights_coeffs[4][2] * light4_scale;

            COLOR_MTX[0][0] = rsp.current_lights[0].col[0];
            COLOR_MTX[1][0] = rsp.current_lights[4].col[0];
            COLOR_MTX[2][0] = rsp.current_lights[rsp.current_num_lights - 1].col[0];
            COLOR_MTX[0][1] = rsp.current_lights[0].col[1];
            COLOR_MTX[1][1] = rsp.current_lights[4].col[1];
            COLOR_MTX[2][1] = rsp.current_lights[rsp.current_num_lights - 1].col[1];
            COLOR_MTX[0][2] = rsp.current_lights[0].col[2];
            COLOR_MTX[1][2] = rsp.current_lights[4].col[2];
            COLOR_MTX[2][2] = rsp.current_lights[rsp.current_num_lights - 1].col[2];
        }

        gfx_sp_vertex_light_step1(n_vertices, dest_index, SEGMENTED_TO_VIRTUAL(vertices));

        if (rsp.geometry_mode & G_TEXTURE_GEN) {
            shz_xmtrx_load_3x3(&ENV_MTX);
            gfx_sp_vertex_light_step1b(n_vertices, dest_index);
        }

        shz_xmtrx_load_3x3(&COEFF_MTX);
        gfx_sp_vertex_light_step2(n_vertices, dest_index);

        shz_xmtrx_load_3x3(&COLOR_MTX);
        gfx_sp_vertex_light_step3(n_vertices, dest_index, SEGMENTED_TO_VIRTUAL(vertices));
}

int need_to_add = 0;
uint8_t add_r, add_g, add_b, add_a;

extern u16 aCoGroundGrassTex[];
extern u16 D_CO_6028A60[];
// Planet sky backdrops with CLAMP-S (Katina/Fortuna/Venom 2): drawn as two 7280-wide copies side by side;
// under CLAMP the first copy's last half-texel sits flat on column 63 while the second starts on column 0
// -> a vertical seam at the junction (HW 2026-08-16; Corneria's WRAP-S backdrop is clean). Force WRAP-S
// for these (same texture, tileable) so both sides converge on the same blend at the junction.
extern u16 aKaBackdropTex[];
extern u16 aFoBackdropTex[];
extern u16 aVe2BackdropTex[];
extern u16 aVe1GroundTex[];
extern u16 aMaGroundTex[];
extern u16 aAqGroundTex[];
extern u16 aAqWaterTex[];
extern u16 D_TI_6001BA8[];
extern u16 aZoWaterTex[];
int last_was_special = 0;

extern int path_priority_draw;

extern float get_current_u_scale(void);
extern float get_current_v_scale(void);

// ---- Raw-PVR colour combiner evaluator (raw N64 mux) -------------------------------------
// Evaluate cycle-0 (a-b)*c+d for colour AND alpha from the RAW G_SETCOMBINE words. TEXEL
// substitutes to 1.0 (the PVR does the real texture modulate), so argb is the modulate colour;
// a constant additive 'd' / a texture-independent colour is routed to oargb (added post-modulate
// via the always-on specular bit). (a/b/d 4-bit, c 5-bit; mux 6 == G_CCMUX_1 == 1.0.)
// Mux samplers. `comb` = the COMBINED colour for this channel (cycle-0 result), `comb_a` = the
// COMBINED alpha - both fed in cycle 1 of a 2-cycle combiner; pass 0 in cycle 0 / 1-cycle (mux 0
// then resolves to 0, exactly the old behaviour). texel terms substitute to `tex` (1.0 modulate /
// 0.0 decal); PVR multiplies the real texel in via the texenv.
static inline float pvr_cc4(int mux, int ch, float tex, float comb, const float prim[4], const float env[4], const float shade[4]) {
    switch (mux) {
        case 0: return comb;          // COMBINED
        case 1: case 2: return tex;   // TEXEL0 / TEXEL1 placeholder
        case 3: return prim[ch];
        case 4: return shade[ch];
        case 5: return env[ch];
        case 6: return 1.0f;
        default: return 0.0f;
    }
}
// RDP NOISE combiner source: valid ONLY in the colour 'a' slot (mux 7; in b/d, 7 means K4/ZERO,
// which pvr_cc4 correctly evaluates as 0). On N64 it is a per-PIXEL pseudo-random 0..1 (SX warp-zone
// enemies, Aquas boss, Arwing damage state: SETUPDL_32/35/59/87 = texel * NOISE * const). PVR can't
// do per-pixel, so approximate per-VERTEX: a fresh uniform 0..1 per vertex, Gouraud across the
// triangle -> flickering static instead of the old 0 (solid black). Cheap LCG, one draw per vertex.
static uint32_t pvr_noise_state = 0x2545F491u;
static inline float pvr_noise(void) {
    pvr_noise_state = pvr_noise_state * 1664525u + 1013904223u;
    return (float) (pvr_noise_state >> 8) * (1.0f / 16777216.0f);
}
// "NOISE x stale TEXEL1": 2-cycle, NOISE in a cycle's colour 'a' slot, and TEXEL1 in cycle 1's colour
// muxes (SETUPDL_35/87 - SX warp-zone enemies - which load only tile 0). On N64, TEXEL1 reads stale
// TMEM through tile 1's leftover descriptor = coloured garbage; the backend binds a raw-VRAM window
// for it (gfx_pvr_set_stale_texel1). Cheap bit tests on the raw combine words, once per draw.
static inline int pvr_combiner_stale_texel1(void) {
    if ((rdp.other_mode_h & (3U << G_MDSFT_CYCLETYPE)) != G_CYC_2CYCLE) return 0;
    uint32_t w0 = rdp.combine_w0, w1 = rdp.combine_w1;
    int a = (w0 >> 20) & 0xF, a2 = (w0 >> 5) & 0xF;
    if (a != 7 && a2 != 7) return 0;
    int b2 = (w1 >> 24) & 0xF, c2 = w0 & 0x1F, d2 = (w1 >> 6) & 0x7;
    return (a2 == 2 || b2 == 2 || c2 == 2 || c2 == 9 || d2 == 2);
}
extern void gfx_pvr_set_stale_texel1(uint8_t on);
extern void gfx_pvr_set_tex_invert_mask(uint8_t mask);   // backend applies it at upload

// Texel-as-INTERPOLANT lerp: 1-cycle colour = (a - d) * TEXEL0 + d = lerp(d, a, t) with a,d constant
// (PRIM/ENV). PVR gives col*tex + offset, so a channel where a < d (DEcreasing with t) can't be
// expressed... unless that channel of the texture is stored INVERTED (1-t):  d + (a-d)t = a + (d-a)(1-t).
// Return the per-channel mask of such channels; the texture cache keys on it (a variant upload) and
// pvr_eval_combiner emits col=|a-d|, offset=min(a,d) per channel -> EXACT. (Briefing TV glow:
// lerp(ENV white, PRIM blue, IA8 ramp) -> white core, blue fringe; was grey with r,g clamped to 0.)
static inline uint8_t pvr_combiner_tex_invert_mask(void) {
    if ((rdp.other_mode_h & (3U << G_MDSFT_CYCLETYPE)) == G_CYC_2CYCLE) return 0;
    uint32_t w0 = rdp.combine_w0, w1 = rdp.combine_w1;
    int a = (w0 >> 20) & 0xF, b = (w1 >> 28) & 0xF, c = (w0 >> 15) & 0x1F, d = (w1 >> 15) & 0x7;
    if (c != 1 || b != d) return 0;                     // needs (a - d) * TEXEL0 + d
    if ((a != 3 && a != 5) || (d != 3 && d != 5)) return 0;   // a,d constant: PRIM or ENV only
    const struct RGBA* pa = (a == 3) ? &rdp.prim_color : &rdp.env_color;
    const struct RGBA* pd = (d == 3) ? &rdp.prim_color : &rdp.env_color;
    uint8_t m = 0;
    if (pa->r < pd->r) m |= 1;
    if (pa->g < pd->g) m |= 2;
    if (pa->b < pd->b) m |= 4;
    return m;
}
// Colour 'a' slot fetch: NOISE (7) -> per-vertex noise, else the shared 4-bit mux.
static inline float pvr_cc_a(int mux, int ch, float tex, float comb, float noise, const float prim[4], const float env[4], const float shade[4]) {
    return (mux == 7) ? noise : pvr_cc4(mux, ch, tex, comb, prim, env, shade);
}
static inline float pvr_cc5(int mux, int ch, float tex, float comb, float comb_a, const float prim[4], const float env[4], const float shade[4]) {
    switch (mux) {
        case 0: return comb;          // COMBINED
        case 1: case 2: case 8: case 9: return tex;
        case 3: return prim[ch];
        case 4: return shade[ch];
        case 5: return env[ch];
        case 6: return 1.0f;
        case 7: return comb_a;        // COMBINED_ALPHA
        case 10: return prim[3];
        case 11: return shade[3];
        case 12: return env[3];
        default: return 0.0f;
    }
}
static inline float pvr_ca(int mux, float comb_a, const float prim[4], const float env[4], const float shade[4]) {
    switch (mux) {
        case 0: return comb_a;        // COMBINED (alpha)
        case 1: case 2: return 1.0f;
        case 3: return prim[3];
        case 4: return shade[3];
        case 5: return env[3];
        case 6: return 1.0f;
        default: return 0.0f;
    }
}

// Derive the PVR texenv from the N64 combiner (compact cc_id = rdp.combine_mode, the CC_ enum,
// 3 bits/input). Maps the combiner's texel<->colour relationship onto REPLACE/MODULATE/DECAL/
// MODULATEALPHA. 2-cycle ALWAYS uses MODULATEALPHA: pvr_eval_combiner evaluates both cycles to the
// final non-texel colour AND alpha (texel as 1.0), and PVR multiplies the one bound texture's rgb
// AND alpha in (MODULATE would drop the combiner alpha - a=tex.a - and re-opaque a glass surface).
static uint32_t derive_pvr_texenv(uint32_t cc_id) {
    if ((rdp.other_mode_h & (3U << G_MDSFT_CYCLETYPE)) == G_CYC_2CYCLE) return GFX_TEXENV_MODULATEALPHA;
    int ca = (cc_id >> 0) & 7, cb = (cc_id >> 3) & 7, cc = (cc_id >> 6) & 7, cd = (cc_id >> 9) & 7;
    int aa = (cc_id >> 12) & 7, ab = (cc_id >> 15) & 7, ac = (cc_id >> 18) & 7, ad = (cc_id >> 21) & 7;
    #define CC_ISTEX(v)  ((v) == CC_TEXEL0 || (v) == CC_TEXEL1)
    #define CC_ISTEXA(v) ((v) == CC_TEXEL0A)
    int color_has_tex = CC_ISTEX(ca) || CC_ISTEX(cb) || CC_ISTEX(cc) || CC_ISTEX(cd) ||
                        CC_ISTEXA(ca) || CC_ISTEXA(cb) || CC_ISTEXA(cc) || CC_ISTEXA(cd);
    if (!color_has_tex) return GFX_TEXENV_MODULATE;     // texel-free colour: MODULATE + oargb routing

    int color_single = (cc == CC_0) || (ca == cb);      // colour = cd
    int color_mul    = (cb == CC_0) && (cd == CC_0);    // colour = ca * cc
    int color_mix    = !color_single && (cb == cd);     // colour = lerp(cd, ca, cc)
    int alpha_single_tex = ((ac == CC_0) || (aa == ab)) && CC_ISTEX(ad);
    int alpha_uses_texel = CC_ISTEX(aa) || CC_ISTEX(ab) || CC_ISTEX(ac) || CC_ISTEX(ad);

    uint32_t mode = GFX_TEXENV_MODULATE;
    if (color_mix && CC_ISTEX(ca) && CC_ISTEXA(cc)) {
        mode = GFX_TEXENV_DECAL;                         // lerp(base, TEXEL0, TEXEL0A)
    } else if (color_single && CC_ISTEX(cd)) {
        mode = alpha_single_tex ? GFX_TEXENV_REPLACE
             : alpha_uses_texel  ? GFX_TEXENV_MODULATEALPHA
                                 : GFX_TEXENV_DECAL;
    } else if (color_mul && (CC_ISTEX(ca) || CC_ISTEX(cc))) {
        mode = alpha_single_tex ? GFX_TEXENV_MODULATE
                                : GFX_TEXENV_MODULATEALPHA;
    } else if (color_mix && alpha_uses_texel) {
        // Colour = lerp(cd, ca, cc) with the texel as interpolant but NOT the DECAL shape above
        // (e.g. Effect359: lerp(ENV, PRIM, TEXEL0), alpha = PRIM_a * TEXEL0_a). Falling through to
        // plain MODULATE drops the texel ALPHA (out alpha = vertex/prim alpha only) -> flat per-
        // triangle alpha = "blocky". MODULATEALPHA folds texel alpha in (alpha = texel_a * vtx_a).
        mode = GFX_TEXENV_MODULATEALPHA;
    }
    #undef CC_ISTEX
    #undef CC_ISTEXA
    return mode;
}

static inline void pvr_eval_combiner(uint32_t w0, uint32_t w1, const struct RGBA* sh,
                                     int textured, uint32_t mode, uint32_t* out_argb, uint32_t* out_oargb) {
    if (mode == GFX_TEXENV_REPLACE) { *out_argb = 0xFFFFFFFFu; *out_oargb = 0; return; }

    const float r255 = 1.0f / 255.0f;
    const float prim[4]  = { rdp.prim_color.r*r255, rdp.prim_color.g*r255, rdp.prim_color.b*r255, rdp.prim_color.a*r255 };
    const float env[4]   = { rdp.env_color.r*r255,  rdp.env_color.g*r255,  rdp.env_color.b*r255,  rdp.env_color.a*r255 };
    const float shade[4] = { sh->r*r255, sh->g*r255, sh->b*r255, sh->a*r255 };
    // DECAL: PVR lerps vtx<->texel by texel.a, so the FACE colour is the combiner with the texel
    // terms ABSENT (texel->0). MODULATE/MODULATEALPHA factor the texel OUT (texel->1).
    const float texv = (mode == GFX_TEXENV_DECAL) ? 0.0f : 1.0f;

    int a = (w0 >> 20) & 0xF, b = (w1 >> 28) & 0xF, c = (w0 >> 15) & 0x1F, d = (w1 >> 15) & 0x7;
    int aa = (w0 >> 12) & 0x7, ab = (w1 >> 12) & 0x7, ac = (w0 >> 9) & 0x7, ad = (w1 >> 9) & 0x7;

    int two_cycle = ((rdp.other_mode_h & (3U << G_MDSFT_CYCLETYPE)) == G_CYC_2CYCLE);
    // One NOISE sample per vertex (grey: same value for r,g,b), only drawn when a cycle's 'a' slot
    // asks for it so non-noise materials pay nothing and the LCG stream isn't perturbed.
    int a2 = two_cycle ? ((w0 >> 5) & 0xF) : 0;
    // Grey noise (r=g=b: one 9-bit random per pixel on the RDP). The COLOUR of the SX warp-zone
    // enemies (NOISE x stale TEXEL1) comes from the backend's raw-VRAM garbage texture instead
    // (pvr_combiner_stale_texel1 -> gfx_pvr_set_stale_texel1), mirroring N64's noise x stale TMEM.
    float noise = (a == 7 || a2 == 7) ? pvr_noise() : 0.0f;

    if (two_cycle) {
        // Full 2-cycle eval: cycle 0 -> COMBINED -> cycle 1, for colour AND alpha. texenv is forced
        // MODULATEALPHA (derive_pvr_texenv), so texv==1 and PVR multiplies the one bound texture's
        // rgb AND alpha into this final non-texel colour+alpha. (Two-texture 2-cycle - TEXEL1 in
        // cycle 1 - can't be exact on single-tile; that 2nd texel falls back to the texv placeholder.)
        int b2 = (w1 >> 24) & 0xF, c2 = (w0 >> 0) & 0x1F, d2 = (w1 >> 6) & 0x7;
        int aa2 = (w1 >> 21) & 0x7, ab2 = (w1 >> 3) & 0x7, ac2 = (w1 >> 18) & 0x7, ad2 = (w1 >> 0) & 0x7;
        // alpha: cycle 0, then cycle 1 with COMBINED = cycle-0 alpha
        float a0 = (pvr_ca(aa, 0.0f, prim, env, shade) - pvr_ca(ab, 0.0f, prim, env, shade)) * pvr_ca(ac, 0.0f, prim, env, shade)
                   + pvr_ca(ad, 0.0f, prim, env, shade);
        a0 = a0 < 0.0f ? 0.0f : (a0 > 1.0f ? 1.0f : a0);
        float a1 = (pvr_ca(aa2, a0, prim, env, shade) - pvr_ca(ab2, a0, prim, env, shade)) * pvr_ca(ac2, a0, prim, env, shade)
                   + pvr_ca(ad2, a0, prim, env, shade);
        a1 = a1 < 0.0f ? 0.0f : (a1 > 1.0f ? 1.0f : a1);
        uint32_t col2[4];
        for (int ch = 0; ch < 3; ch++) {
            float v0 = (pvr_cc_a(a, ch, texv, 0.0f, noise, prim, env, shade) - pvr_cc4(b, ch, texv, 0.0f, prim, env, shade))
                       * pvr_cc5(c, ch, texv, 0.0f, 0.0f, prim, env, shade) + pvr_cc4(d, ch, texv, 0.0f, prim, env, shade);
            v0 = v0 < 0.0f ? 0.0f : (v0 > 1.0f ? 1.0f : v0);
            float v1 = (pvr_cc_a(a2, ch, texv, v0, noise, prim, env, shade) - pvr_cc4(b2, ch, texv, v0, prim, env, shade))
                       * pvr_cc5(c2, ch, texv, v0, a0, prim, env, shade) + pvr_cc4(d2, ch, texv, v0, prim, env, shade);
            v1 = v1 < 0.0f ? 0.0f : (v1 > 1.0f ? 1.0f : v1);
            col2[ch] = (uint32_t)(v1 * 255.0f);
        }
        *out_argb  = PACK_ARGB8888(col2[0], col2[1], col2[2], (uint32_t)(a1 * 255.0f));
        *out_oargb = 0;
        return;
    }

    int has_texel = (a == 1 || a == 2 || b == 1 || b == 2 ||
                     c == 1 || c == 2 || c == 8 || c == 9 ||
                     d == 1 || d == 2);
    int color_const_textured = textured && !has_texel;   // (2-cycle returned above)
    // The additive d term can't be done in PVR's MODULATE, so route a constant/per-vertex d
    // (PRIM/SHADE/ENV) into oargb (added post-modulate). DECAL keeps its base in argb (texv=0).
    int offset = (mode != GFX_TEXENV_DECAL) && !color_const_textured &&
                 (d == 3 || d == 4 || d == 5) && has_texel;

    uint32_t col[4];
    uint32_t oarr[3] = { 0, 0, 0 };
    for (int ch = 0; ch < 3; ch++) {
        float va = pvr_cc_a(a, ch, texv, 0.0f, noise, prim, env, shade);
        float vb = pvr_cc4(b, ch, texv, 0.0f, prim, env, shade);
        float vc = pvr_cc5(c, ch, texv, 0.0f, 0.0f, prim, env, shade);
        if (color_const_textured) {
            float v = (va - vb) * vc + pvr_cc4(d, ch, texv, 0.0f, prim, env, shade);
            v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
            oarr[ch] = (uint32_t)(v * 255.0f);
            col[ch]  = 0;
        } else if (offset && (pvr_tex_invert_mask & (1 << ch))) {
            // Inverted-texel channel (texture stores 1-t): d + (a-d)t = a + (d-a)(1-t)
            //   -> col = d - a (positive), offset = a.  (b == d for this shape.)
            float vd = pvr_cc4(d, ch, texv, 0.0f, prim, env, shade);
            float v = vd - va;
            v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
            col[ch]  = (uint32_t)(v * 255.0f);
            oarr[ch] = (uint32_t)(va * 255.0f);
        } else {
            float vd = offset ? 0.0f : pvr_cc4(d, ch, texv, 0.0f, prim, env, shade);
            float v = (va - vb) * vc + vd;
            v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
            col[ch] = (uint32_t)(v * 255.0f);
        }
    }
    if (color_const_textured) {
        *out_oargb = PACK_ARGB8888(oarr[0], oarr[1], oarr[2], 0);
    } else if (offset) {
        const float* o = (d == 3) ? prim : (d == 4) ? shade : env;
        uint32_t oo[3];
        for (int ch = 0; ch < 3; ch++)
            oo[ch] = (pvr_tex_invert_mask & (1 << ch)) ? oarr[ch] : (uint32_t)(o[ch]*255.0f);
        *out_oargb = PACK_ARGB8888(oo[0], oo[1], oo[2], 0);
    } else {
        *out_oargb = 0;
    }
    float av = (pvr_ca(aa, 0.0f, prim, env, shade) - pvr_ca(ab, 0.0f, prim, env, shade)) * pvr_ca(ac, 0.0f, prim, env, shade)
               + pvr_ca(ad, 0.0f, prim, env, shade);
    av = av < 0.0f ? 0.0f : (av > 1.0f ? 1.0f : av);
    col[3] = (uint32_t)(av * 255.0f);
    *out_argb = PACK_ARGB8888(col[0], col[1], col[2], col[3]);
}

// ---- Prepared (per combiner-state) evaluator for the 3D bake --------------------------------
// pvr_eval_combiner above is exact but ~9us per call on SH4 (switch-based mux fetch, per-call int->
// float of prim/env/shade, 2-cycle = ~32 fetches). The 3D path evaluates once per unique vertex per
// state (cc_cache), so with ~2000 unique verts a frame that was ~19ms. Here the state-dependent part
// is decoded ONCE per cc_state_stamp change (pvr_cc_prepare): every mux slot becomes an index into a
// small value table whose constant rows (PRIM/ENV/TEX/ONE/ZERO/alpha consts) are pre-filled as floats;
// per vertex only the SHADE rows (and NOISE) are written, then the eval is straight-line loads+FMAs.
// Same maths/branches as pvr_eval_combiner (mode/DECAL texv, has_texel/offset/color_const_textured/
// invert-mask handling, 2-cycle) - keep them in sync.
enum { CR_COMB = 0, CR_TEX, CR_PRIM, CR_SHADE, CR_ENV, CR_ONE, CR_ZERO, CR_COMBA, CR_PRIMA, CR_SHADEA,
       CR_ENVA, CR_NOISE, CR_ROWS };
static struct ccp_s {
    float V[CR_ROWS][3];         // colour value table (alpha-ish rows replicated per channel)
    float A[CR_ROWS];            // alpha value table (rows: COMBA, ONE, PRIMA, SHADEA, ENVA, ZERO)
    uint8_t a, b, c, d, aa, ab, ac, ad;          // cycle 0 slot -> row
    uint8_t a2, b2, c2, d2, aa2, ab2, ac2, ad2;  // cycle 1
    uint8_t two_cycle, replace, cct, offset, noise, decal_o;
    uint8_t d_row;               // offset source row (PRIM/SHADE/ENV) for oargb
    uint8_t invert;              // pvr_tex_invert_mask at prep
    float prim[4], env[4];
    // Specialisation (pvr_cc_specialise): kind 0 = generic table eval, 1 = AFFINE in shade
    // (col[ch] = k[ch][0] + k[ch][1]*s_ch + k[ch][2]*s_a, oarr likewise, alpha = ka[0] + ka[1]*s_a),
    // 2 = CONSTANT (no shade dependence at all: const_argb/const_oargb precomputed).
    uint8_t kind;
    float kc[3][3], ko[3][3], ka[2];
    uint32_t const_argb, const_oargb;
} ccp;

static inline float cc_clamp01(float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); }
// ---- Affine-in-shade specialisation --------------------------------------------------------
// Value type: v = c0 + c1*s + c2*sa (s = this channel's shade, sa = shade alpha). ok=0 -> not affine.
typedef struct { float c0, c1, c2; uint8_t ok; } aff_t;
static inline aff_t aff_c(float c) { aff_t r = { c, 0.0f, 0.0f, 1 }; return r; }
static inline aff_t aff_bad(void) { aff_t r = { 0, 0, 0, 0 }; return r; }
static inline int aff_isconst(aff_t a) { return a.c1 == 0.0f && a.c2 == 0.0f; }
static inline aff_t aff_sub(aff_t a, aff_t b) { aff_t r = { a.c0 - b.c0, a.c1 - b.c1, a.c2 - b.c2, (uint8_t)(a.ok && b.ok) }; return r; }
static inline aff_t aff_add(aff_t a, aff_t b) { aff_t r = { a.c0 + b.c0, a.c1 + b.c1, a.c2 + b.c2, (uint8_t)(a.ok && b.ok) }; return r; }
static inline aff_t aff_mul(aff_t a, aff_t b) {
    if (!a.ok || !b.ok) return aff_bad();
    if (aff_isconst(a)) { aff_t r = { a.c0 * b.c0, a.c0 * b.c1, a.c0 * b.c2, 1 }; return r; }
    if (aff_isconst(b)) { aff_t r = { b.c0 * a.c0, b.c0 * a.c1, b.c0 * a.c2, 1 }; return r; }
    return aff_bad();   // shade * shade -> not affine
}
// clamp01 is the identity iff the affine range over s,sa in [0,1] stays inside [0,1].
static inline aff_t aff_clamp(aff_t a) {
    if (!a.ok) return a;
    float lo = a.c0 + (a.c1 < 0.0f ? a.c1 : 0.0f) + (a.c2 < 0.0f ? a.c2 : 0.0f);
    float hi = a.c0 + (a.c1 > 0.0f ? a.c1 : 0.0f) + (a.c2 > 0.0f ? a.c2 : 0.0f);
    if (lo < -1e-6f || hi > 1.0f + 1e-6f) {
        // A constant out of range clamps to a constant; anything shade-dependent that can leave
        // [0,1] needs the real clamp -> not affine.
        if (aff_isconst(a)) return aff_c(a.c0 < 0.0f ? 0.0f : (a.c0 > 1.0f ? 1.0f : a.c0));
        return aff_bad();
    }
    return a;
}
// Row -> affine value for colour channel ch (shade rows are the variables; NOISE is never affine).
static inline aff_t aff_row(int row, int ch, aff_t comb, aff_t comba) {
    switch (row) {
        case CR_COMB:   return comb;
        case CR_COMBA:  return comba;
        case CR_SHADE:  { aff_t r = { 0.0f, 1.0f, 0.0f, 1 }; return r; }
        case CR_SHADEA: { aff_t r = { 0.0f, 0.0f, 1.0f, 1 }; return r; }
        case CR_NOISE:  return aff_bad();
        default:        return aff_c(ccp.V[row][ch]);
    }
}
// Alpha table rows are only consts / SHADEA / COMBA.
static inline aff_t affa_row(int row, aff_t comba) {
    switch (row) {
        case CR_COMBA:  return comba;
        case CR_SHADEA: { aff_t r = { 0.0f, 0.0f, 1.0f, 1 }; return r; }
        default:        return aff_c(ccp.A[row]);
    }
}
static void pvr_cc_specialise(void) {
    ccp.kind = 0;
    if (ccp.replace || ccp.noise) return;
    aff_t zero = aff_c(0.0f);
    // alpha
    aff_t a0 = aff_clamp(aff_add(aff_mul(aff_sub(affa_row(ccp.aa, zero), affa_row(ccp.ab, zero)), affa_row(ccp.ac, zero)), affa_row(ccp.ad, zero)));
    aff_t aout = a0;
    if (ccp.two_cycle)
        aout = aff_clamp(aff_add(aff_mul(aff_sub(affa_row(ccp.aa2, a0), affa_row(ccp.ab2, a0)), affa_row(ccp.ac2, a0)), affa_row(ccp.ad2, a0)));
    if (!aout.ok || aout.c1 != 0.0f) return;   // alpha may only depend on sa
    aff_t col[3], oar[3];
    for (int ch = 0; ch < 3; ch++) {
        aff_t va = aff_row(ccp.a, ch, zero, zero), vb = aff_row(ccp.b, ch, zero, zero);
        aff_t vc = aff_row(ccp.c, ch, zero, zero), vd = aff_row(ccp.d, ch, zero, zero);
        if (ccp.two_cycle) {
            aff_t v0 = aff_clamp(aff_add(aff_mul(aff_sub(va, vb), vc), vd));
            aff_t va2 = aff_row(ccp.a2, ch, v0, a0), vb2 = aff_row(ccp.b2, ch, v0, a0);
            aff_t vc2 = aff_row(ccp.c2, ch, v0, a0), vd2 = aff_row(ccp.d2, ch, v0, a0);
            col[ch] = aff_clamp(aff_add(aff_mul(aff_sub(va2, vb2), vc2), vd2));
            oar[ch] = zero;
        } else if (ccp.cct) {
            oar[ch] = aff_clamp(aff_add(aff_mul(aff_sub(va, vb), vc), vd));
            col[ch] = zero;
        } else if (ccp.offset && (ccp.invert & (1 << ch))) {
            col[ch] = aff_clamp(aff_sub(vd, va));
            oar[ch] = va;   // no clamp in the generic path either
        } else {
            col[ch] = aff_clamp(aff_add(aff_mul(aff_sub(va, vb), vc), ccp.offset ? zero : vd));
            oar[ch] = ccp.offset ? aff_row(ccp.d_row, ch, zero, zero) : zero;
        }
        if (!col[ch].ok || !oar[ch].ok) return;
    }
    int allconst = aff_isconst(aout);
    for (int ch = 0; ch < 3; ch++) {
        ccp.kc[ch][0] = col[ch].c0; ccp.kc[ch][1] = col[ch].c1; ccp.kc[ch][2] = col[ch].c2;
        ccp.ko[ch][0] = oar[ch].c0; ccp.ko[ch][1] = oar[ch].c1; ccp.ko[ch][2] = oar[ch].c2;
        if (!aff_isconst(col[ch]) || !aff_isconst(oar[ch])) allconst = 0;
    }
    ccp.ka[0] = aout.c0; ccp.ka[1] = aout.c2;
    if (allconst) {
        uint32_t c[3], o[3];
        for (int ch = 0; ch < 3; ch++) { c[ch] = (uint32_t) (cc_clamp01(col[ch].c0) * 255.0f); o[ch] = (uint32_t) (cc_clamp01(oar[ch].c0) * 255.0f); }
        ccp.const_argb  = PACK_ARGB8888(c[0], c[1], c[2], (uint32_t) (cc_clamp01(aout.c0) * 255.0f));
        ccp.const_oargb = (ccp.two_cycle || (!ccp.cct && !ccp.offset)) ? 0 : PACK_ARGB8888(o[0], o[1], o[2], 0);
        ccp.kind = 2;
    } else {
        ccp.kind = 1;
    }
}
// mux code -> table row (colour 4-bit slot: a/b/d; c uses the 5-bit map; a-slot 7 = NOISE)
static const uint8_t cc4_row[16] = { CR_COMB, CR_TEX, CR_TEX, CR_PRIM, CR_SHADE, CR_ENV, CR_ONE, CR_ZERO,
                                     CR_ZERO, CR_ZERO, CR_ZERO, CR_ZERO, CR_ZERO, CR_ZERO, CR_ZERO, CR_ZERO };
static const uint8_t cc5_row[32] = { CR_COMB, CR_TEX, CR_TEX, CR_PRIM, CR_SHADE, CR_ENV, CR_ONE, CR_COMBA,
                                     CR_TEX, CR_TEX, CR_PRIMA, CR_SHADEA, CR_ENVA, CR_ZERO, CR_ZERO, CR_ZERO,
                                     CR_ZERO, CR_ZERO, CR_ZERO, CR_ZERO, CR_ZERO, CR_ZERO, CR_ZERO, CR_ZERO,
                                     CR_ZERO, CR_ZERO, CR_ZERO, CR_ZERO, CR_ZERO, CR_ZERO, CR_ZERO, CR_ZERO };
static const uint8_t ca_row[8]   = { CR_COMBA, CR_ONE, CR_ONE, CR_PRIMA, CR_SHADEA, CR_ENVA, CR_ONE, CR_ZERO };

static void pvr_cc_prepare(uint32_t w0, uint32_t w1, int textured, uint32_t mode) {
    const float r255 = 1.0f / 255.0f;
    ccp.replace = (mode == GFX_TEXENV_REPLACE);
    ccp.prim[0] = rdp.prim_color.r * r255; ccp.prim[1] = rdp.prim_color.g * r255;
    ccp.prim[2] = rdp.prim_color.b * r255; ccp.prim[3] = rdp.prim_color.a * r255;
    ccp.env[0]  = rdp.env_color.r * r255;  ccp.env[1]  = rdp.env_color.g * r255;
    ccp.env[2]  = rdp.env_color.b * r255;  ccp.env[3]  = rdp.env_color.a * r255;
    float texv = (mode == GFX_TEXENV_DECAL) ? 0.0f : 1.0f;
    for (int ch = 0; ch < 3; ch++) {
        ccp.V[CR_TEX][ch] = texv;  ccp.V[CR_PRIM][ch] = ccp.prim[ch]; ccp.V[CR_ENV][ch] = ccp.env[ch];
        ccp.V[CR_ONE][ch] = 1.0f;  ccp.V[CR_ZERO][ch] = 0.0f;
        ccp.V[CR_PRIMA][ch] = ccp.prim[3]; ccp.V[CR_ENVA][ch] = ccp.env[3];
    }
    ccp.A[CR_ONE] = 1.0f; ccp.A[CR_ZERO] = 0.0f; ccp.A[CR_PRIMA] = ccp.prim[3]; ccp.A[CR_ENVA] = ccp.env[3];

    int a = (w0 >> 20) & 0xF, b = (w1 >> 28) & 0xF, c = (w0 >> 15) & 0x1F, d = (w1 >> 15) & 0x7;
    int aa = (w0 >> 12) & 0x7, ab = (w1 >> 12) & 0x7, ac = (w0 >> 9) & 0x7, ad = (w1 >> 9) & 0x7;
    int a2 = (w0 >> 5) & 0xF, b2 = (w1 >> 24) & 0xF, c2 = (w0 >> 0) & 0x1F, d2 = (w1 >> 6) & 0x7;
    int aa2 = (w1 >> 21) & 0x7, ab2 = (w1 >> 3) & 0x7, ac2 = (w1 >> 18) & 0x7, ad2 = (w1 >> 0) & 0x7;
    ccp.two_cycle = ((rdp.other_mode_h & (3U << G_MDSFT_CYCLETYPE)) == G_CYC_2CYCLE);
    ccp.noise = (a == 7) || (a2 == 7);
    ccp.a = (a == 7) ? CR_NOISE : cc4_row[a];  ccp.b = cc4_row[b];  ccp.c = cc5_row[c];  ccp.d = cc4_row[d & 7];
    ccp.a2 = (a2 == 7) ? CR_NOISE : cc4_row[a2]; ccp.b2 = cc4_row[b2]; ccp.c2 = cc5_row[c2]; ccp.d2 = cc4_row[d2 & 7];
    ccp.aa = ca_row[aa]; ccp.ab = ca_row[ab]; ccp.ac = ca_row[ac]; ccp.ad = ca_row[ad];
    ccp.aa2 = ca_row[aa2]; ccp.ab2 = ca_row[ab2]; ccp.ac2 = ca_row[ac2]; ccp.ad2 = ca_row[ad2];

    int has_texel = (a == 1 || a == 2 || b == 1 || b == 2 || c == 1 || c == 2 || c == 8 || c == 9 || d == 1 || d == 2);
    ccp.cct = textured && !has_texel;
    ccp.offset = (mode != GFX_TEXENV_DECAL) && !ccp.cct && (d == 3 || d == 4 || d == 5) && has_texel;
    ccp.d_row = (d == 3) ? CR_PRIM : (d == 4) ? CR_SHADE : CR_ENV;
    ccp.invert = pvr_tex_invert_mask;
    pvr_cc_specialise();
}
// Prepared-state cache: prepare+specialise is ~60us; materials repeat every frame (and the 3D/2D
// paths alternate), so keep the last CCP_CACHE prepared states keyed by the 6-word combiner key.
#define CCP_CACHE 32
static struct { uint32_t key[6]; uint8_t valid; struct ccp_s val; } ccp_cache[CCP_CACHE];
static void pvr_cc_prepare_cached(const uint32_t k[6], uint32_t w0, uint32_t w1, int textured, uint32_t mode) {
    uint32_t h = (k[0] ^ (k[1] * 3u) ^ (k[2] * 5u) ^ (k[3] * 7u) ^ (k[4] * 11u) ^ (k[5] * 13u));
    h ^= h >> 16; h ^= h >> 8;
    unsigned slot = h & (CCP_CACHE - 1);
    if (ccp_cache[slot].valid && ccp_cache[slot].key[0] == k[0] && ccp_cache[slot].key[1] == k[1] &&
        ccp_cache[slot].key[2] == k[2] && ccp_cache[slot].key[3] == k[3] && ccp_cache[slot].key[4] == k[4] &&
        ccp_cache[slot].key[5] == k[5]) {
        ccp = ccp_cache[slot].val;
        return;
    }
    pvr_cc_prepare(w0, w1, textured, mode);
    for (int j = 0; j < 6; j++) ccp_cache[slot].key[j] = k[j];
    ccp_cache[slot].val = ccp; ccp_cache[slot].valid = 1;
}


static void __attribute__((noinline)) pvr_eval_combiner_generic(float sr, float sg, float sb, float sa,
                                                                uint32_t* out_argb, uint32_t* out_oargb);
static inline void pvr_eval_combiner_fast(const struct RGBA* sh, uint32_t* out_argb, uint32_t* out_oargb) {
    if (ccp.replace) { *out_argb = 0xFFFFFFFFu; *out_oargb = 0; return; }
    if (ccp.kind == 2) { *out_argb = ccp.const_argb; *out_oargb = ccp.const_oargb; return; }
    const float r255 = 1.0f / 255.0f;
    float sr = sh->r * r255, sg = sh->g * r255, sb = sh->b * r255, sa = sh->a * r255;
    if (ccp.kind == 1) {
        // Affine in shade (clamps proven no-ops at prepare; the final clamp only guards float noise).
        float sv[3] = { sr, sg, sb };
        uint32_t c[3], o[3];
        for (int ch = 0; ch < 3; ch++) {
            c[ch] = (uint32_t) (cc_clamp01(ccp.kc[ch][0] + ccp.kc[ch][1] * sv[ch] + ccp.kc[ch][2] * sa) * 255.0f);
            o[ch] = (uint32_t) (cc_clamp01(ccp.ko[ch][0] + ccp.ko[ch][1] * sv[ch] + ccp.ko[ch][2] * sa) * 255.0f);
        }
        uint32_t av = (uint32_t) (cc_clamp01(ccp.ka[0] + ccp.ka[1] * sa) * 255.0f);
        *out_argb  = PACK_ARGB8888(c[0], c[1], c[2], av);
        *out_oargb = (ccp.two_cycle || (!ccp.cct && !ccp.offset)) ? 0 : PACK_ARGB8888(o[0], o[1], o[2], 0);
        return;
    }
    pvr_eval_combiner_generic(sr, sg, sb, sa, out_argb, out_oargb);
}
// Generic (kind 0) table evaluator, kept OUT OF LINE: it is big and rare, and inlining it into the
// per-triangle bake loop blew the SH4's 8KB direct-mapped I-cache (PROF: ~26% of walk was I-cache
// stall). Only exotic combiners (shade*shade, NOISE, clamped mid-range) reach it.
static void __attribute__((noinline)) pvr_eval_combiner_generic(float sr, float sg, float sb, float sa,
                                                                uint32_t* out_argb, uint32_t* out_oargb) {
    ccp.V[CR_SHADE][0] = sr; ccp.V[CR_SHADE][1] = sg; ccp.V[CR_SHADE][2] = sb;
    ccp.V[CR_SHADEA][0] = ccp.V[CR_SHADEA][1] = ccp.V[CR_SHADEA][2] = sa;
    ccp.A[CR_SHADEA] = sa;
    if (ccp.noise) { float n = pvr_noise(); ccp.V[CR_NOISE][0] = ccp.V[CR_NOISE][1] = ccp.V[CR_NOISE][2] = n; }

    // alpha, cycle 0 (COMBA row is 0 for cycle 0 fetches)
    ccp.A[CR_COMBA] = 0.0f;
    float a0 = cc_clamp01((ccp.A[ccp.aa] - ccp.A[ccp.ab]) * ccp.A[ccp.ac] + ccp.A[ccp.ad]);
    if (ccp.two_cycle) {
        ccp.A[CR_COMBA] = a0;
        float a1 = cc_clamp01((ccp.A[ccp.aa2] - ccp.A[ccp.ab2]) * ccp.A[ccp.ac2] + ccp.A[ccp.ad2]);
        uint32_t col[3];
        for (int ch = 0; ch < 3; ch++) {
            ccp.V[CR_COMB][ch] = 0.0f; ccp.V[CR_COMBA][ch] = 0.0f;
            float v0 = cc_clamp01((ccp.V[ccp.a][ch] - ccp.V[ccp.b][ch]) * ccp.V[ccp.c][ch] + ccp.V[ccp.d][ch]);
            ccp.V[CR_COMB][ch] = v0; ccp.V[CR_COMBA][ch] = a0;
            float v1 = cc_clamp01((ccp.V[ccp.a2][ch] - ccp.V[ccp.b2][ch]) * ccp.V[ccp.c2][ch] + ccp.V[ccp.d2][ch]);
            col[ch] = (uint32_t) (v1 * 255.0f);
        }
        *out_argb = PACK_ARGB8888(col[0], col[1], col[2], (uint32_t) (a1 * 255.0f));
        *out_oargb = 0;
        return;
    }
    uint32_t col[3], oarr[3] = { 0, 0, 0 };
    for (int ch = 0; ch < 3; ch++) {
        ccp.V[CR_COMB][ch] = 0.0f; ccp.V[CR_COMBA][ch] = 0.0f;
        float va = ccp.V[ccp.a][ch], vb = ccp.V[ccp.b][ch], vc = ccp.V[ccp.c][ch];
        if (ccp.cct) {
            oarr[ch] = (uint32_t) (cc_clamp01((va - vb) * vc + ccp.V[ccp.d][ch]) * 255.0f);
            col[ch] = 0;
        } else if (ccp.offset && (ccp.invert & (1 << ch))) {
            float vd = ccp.V[ccp.d][ch];
            col[ch]  = (uint32_t) (cc_clamp01(vd - va) * 255.0f);
            oarr[ch] = (uint32_t) (va * 255.0f);
        } else {
            float vd = ccp.offset ? 0.0f : ccp.V[ccp.d][ch];
            col[ch] = (uint32_t) (cc_clamp01((va - vb) * vc + vd) * 255.0f);
        }
    }
    if (ccp.cct) {
        *out_oargb = PACK_ARGB8888(oarr[0], oarr[1], oarr[2], 0);
    } else if (ccp.offset) {
        uint32_t oo[3];
        for (int ch = 0; ch < 3; ch++)
            oo[ch] = (ccp.invert & (1 << ch)) ? oarr[ch] : (uint32_t) (ccp.V[ccp.d_row][ch] * 255.0f);
        *out_oargb = PACK_ARGB8888(oo[0], oo[1], oo[2], 0);
    } else {
        *out_oargb = 0;
    }
    *out_argb = PACK_ARGB8888(col[0], col[1], col[2], (uint32_t) (a0 * 255.0f));
}

// ---- Near-plane clip (z + w >= 0) --------------------------------------------------------
// The raw-PVR path bakes screen_x/y + 1/w per vertex, so a triangle straddling the near plane
// must be CLIPPED, not dropped. Clip against the NEAR plane z+w>=0 (NOT the eye plane w~0:
// eye-plane intersections project to ~infinity and explode the triangle). Sutherland-Hodgman;
// interpolates only the fields the PVR emit reads (clip _x.._w, UV, colour, fog).
static inline float sm_near_dist(const struct LoadedVertex *v) { return v->_z + v->_w; }
static struct LoadedVertex sm_clipv[4] __attribute__((aligned(32)));
static inline void sm_clip_lerp(struct LoadedVertex *o, const struct LoadedVertex *a,
                                const struct LoadedVertex *b, float t) {
    o->_x = a->_x + t * (b->_x - a->_x);
    o->_y = a->_y + t * (b->_y - a->_y);
    o->_z = a->_z + t * (b->_z - a->_z);
    o->_w = a->_w + t * (b->_w - a->_w);
    o->u  = a->u  + t * (b->u  - a->u);
    o->v  = a->v  + t * (b->v  - a->v);
    o->color.r = (uint8_t)(a->color.r + t * ((float)b->color.r - (float)a->color.r));
    o->color.g = (uint8_t)(a->color.g + t * ((float)b->color.g - (float)a->color.g));
    o->color.b = (uint8_t)(a->color.b + t * ((float)b->color.b - (float)a->color.b));
    o->color.a = (uint8_t)(a->color.a + t * ((float)b->color.a - (float)a->color.a));
    o->fog     = (uint8_t)(a->fog     + t * ((float)b->fog     - (float)a->fog));
}
static int __attribute__((noinline)) sm_near_clip_fan_slow(struct LoadedVertex *v1, struct LoadedVertex *v2,
                                                           struct LoadedVertex *v3, struct LoadedVertex *out[2][3],
                                                           float d1, float d2, float d3);
static inline int sm_near_clip_fan(struct LoadedVertex *v1, struct LoadedVertex *v2,
                                   struct LoadedVertex *v3, struct LoadedVertex *out[2][3]) {
    float d1 = sm_near_dist(v1), d2 = sm_near_dist(v2), d3 = sm_near_dist(v3);
    if (d1 >= 0.0f && d2 >= 0.0f && d3 >= 0.0f) {   // fully in front of the near plane (hot path)
        out[0][0] = v1; out[0][1] = v2; out[0][2] = v3;
        return 1;
    }
    return sm_near_clip_fan_slow(v1, v2, v3, out, d1, d2, d3);   // rare: out of line (I-cache)
}
static int __attribute__((noinline)) sm_near_clip_fan_slow(struct LoadedVertex *v1, struct LoadedVertex *v2,
                                                           struct LoadedVertex *v3, struct LoadedVertex *out[2][3],
                                                           float d1, float d2, float d3) {
    struct LoadedVertex *in[3] = { v1, v2, v3 };
    float din[3] = { d1, d2, d3 };
    int n = 0;
    for (int i = 0; i < 3; i++) {
        struct LoadedVertex *cur = in[i];
        struct LoadedVertex *nxt = in[(i + 1 == 3) ? 0 : (i + 1)];
        float dc = din[i], dn = din[(i + 1 == 3) ? 0 : (i + 1)];
        int inc = dc >= 0.0f, inn = dn >= 0.0f;
        if (inc && n < 4) sm_clipv[n++] = *cur;
        if (inc != inn && n < 4) {
            float t = dc / (dc - dn);
            sm_clip_lerp(&sm_clipv[n++], cur, nxt, t);
        }
    }
    if (n < 3) return 0;
    out[0][0] = &sm_clipv[0]; out[0][1] = &sm_clipv[1]; out[0][2] = &sm_clipv[2];
    if (n == 4) {
        out[1][0] = &sm_clipv[0]; out[1][1] = &sm_clipv[2]; out[1][2] = &sm_clipv[3];
        return 2;
    }
    return 1;
}

// ---- Software scissor: 5-plane homogeneous Sutherland-Hodgman clip (split-screen panes) -------
// Clips a triangle in clip space against the near plane (z+w>=0 - the SAME plane sm_near_clip_fan
// uses, NOT the eye plane, whose intersections project to ~infinity) and the 4 scissor planes.
// The crossing parameter t is applied to the emit-relevant attributes via sm_clip_lerp; because
// the projection is linear in homogeneous coords, lerp-in-clip-space is exact. Only planes named
// in clip_mask get a pass (convexity: clipping a crossed plane can't push verts outside an
// un-crossed one). Cold path: runs only for pane-edge-crossing triangles in split-screen.
#define CLIP_MAX 12
static struct LoadedVertex clip_bufA[CLIP_MAX] __attribute__((aligned(32)));
static struct LoadedVertex clip_bufB[CLIP_MAX] __attribute__((aligned(32)));

// signed distance = A*_x + B*_y + C*_z + D*_w + E ; inside when >= 0
static inline float clip_dist(const struct LoadedVertex *v, const float p[5]) {
    return p[0] * v->_x + p[1] * v->_y + p[2] * v->_z + p[3] * v->_w + p[4];
}

static int clip_against_plane(const struct LoadedVertex *in, int n, struct LoadedVertex *out,
                              const float p[5]) {
    int m = 0;
    for (int i = 0; i < n; i++) {
        const struct LoadedVertex *cur = &in[i];
        const struct LoadedVertex *nxt = &in[(i + 1 == n) ? 0 : (i + 1)];
        float dc = clip_dist(cur, p);
        float dn = clip_dist(nxt, p);
        int inc = (dc >= 0.0f);
        int inn = (dn >= 0.0f);
        if (inc && m < CLIP_MAX)
            out[m++] = *cur;
        if ((inc != inn) && m < CLIP_MAX) {
            float t = dc / (dc - dn);
            sm_clip_lerp(&out[m++], cur, nxt, t);
        }
    }
    return m;
}

// Clip (a,b,c) and emit the resulting polygon as a triangle fan of pointers into a static buffer.
// Returns the number of output triangles (0..6). The near plane is mapped to SC_FORCE and clipped
// FIRST so the scissor passes see near-side geometry only.
static int __attribute__((noinline)) gfx_build_clipped_fan(const struct LoadedVertex *a,
                                                           const struct LoadedVertex *b,
                                                           const struct LoadedVertex *c,
                                                           struct LoadedVertex *out_tris[][3],
                                                           uint8_t clip_mask) {
    static const uint8_t plane_bit[5] = { SC_FORCE, SC_LEFT, SC_RIGHT, SC_BOTTOM, SC_TOP };
    float planes[5][5];
    // near: _z + _w >= 0
    planes[0][0] = 0.0f;  planes[0][1] = 0.0f;  planes[0][2] = 1.0f; planes[0][3] = 1.0f;         planes[0][4] = 0.0f;
    // _x/_w >= xmin  ->  _x - xmin*_w >= 0
    planes[1][0] = 1.0f;  planes[1][1] = 0.0f;  planes[1][2] = 0.0f; planes[1][3] = -sc_ndc_xmin; planes[1][4] = 0.0f;
    // _x/_w <= xmax  ->  xmax*_w - _x >= 0
    planes[2][0] = -1.0f; planes[2][1] = 0.0f;  planes[2][2] = 0.0f; planes[2][3] = sc_ndc_xmax;  planes[2][4] = 0.0f;
    // _y/_w >= ymin
    planes[3][0] = 0.0f;  planes[3][1] = 1.0f;  planes[3][2] = 0.0f; planes[3][3] = -sc_ndc_ymin; planes[3][4] = 0.0f;
    // _y/_w <= ymax
    planes[4][0] = 0.0f;  planes[4][1] = -1.0f; planes[4][2] = 0.0f; planes[4][3] = sc_ndc_ymax;  planes[4][4] = 0.0f;

    clip_bufA[0] = *a;
    clip_bufA[1] = *b;
    clip_bufA[2] = *c;
    int n = 3;

    struct LoadedVertex *src = clip_bufA;
    struct LoadedVertex *dst = clip_bufB;
    for (int pi = 0; pi < 5; pi++) {
        if (!(clip_mask & plane_bit[pi]))
            continue;
        n = clip_against_plane(src, n, dst, planes[pi]);
        if (n < 3)
            return 0;
        struct LoadedVertex *tmp = src;
        src = dst;
        dst = tmp;
    }

    int nt = 0;
    for (int k = 1; k + 1 < n && nt < 6; k++) {
        out_tris[nt][0] = &src[0];
        out_tris[nt][1] = &src[k];
        out_tris[nt][2] = &src[k + 1];
        nt++;
    }
    return nt;
}

// Split-screen pane path for gfx_sp_tri1: classify by the per-vertex scissor outcodes (lazily
// refreshed per scissor generation, cheap byte compares) and software-clip pane-crossing
// triangles. Deliberately noinline and OUT of the GFX_HOT section: single-player never takes it
// (sc_is_fullscreen skips it entirely), and split-screen trades an out-of-section call for not
// bloating the 8KB direct-mapped I-cache hot block. i1/i2/i3 are the loaded_vertices indices of
// v1/v2/v3 (the caller's swapped order already applied). Returns the fan size (0 = drop).
static int __attribute__((noinline)) gfx_scissor_classify_fan(struct LoadedVertex *v1,
                                                              struct LoadedVertex *v2,
                                                              struct LoadedVertex *v3,
                                                              uint8_t i1, uint8_t i2, uint8_t i3,
                                                              struct LoadedVertex *out[][3]) {
    if (sc_gen_v[i1] != cur_scissor_gen) { sc_oc_v[i1] = compute_scissor_outcode(v1); sc_gen_v[i1] = cur_scissor_gen; }
    if (sc_gen_v[i2] != cur_scissor_gen) { sc_oc_v[i2] = compute_scissor_outcode(v2); sc_gen_v[i2] = cur_scissor_gen; }
    if (sc_gen_v[i3] != cur_scissor_gen) { sc_oc_v[i3] = compute_scissor_outcode(v3); sc_gen_v[i3] = cur_scissor_gen; }
    uint8_t oc_or  = sc_oc_v[i1] | sc_oc_v[i2] | sc_oc_v[i3];
    uint8_t oc_and = sc_oc_v[i1] & sc_oc_v[i2] & sc_oc_v[i3];

    if (oc_or == 0) {
        // fully inside every pane edge, in front of the near plane -> emit unclipped
        out[0][0] = v1; out[0][1] = v2; out[0][2] = v3;
        return 1;
    }
    if (oc_and & SC_EDGE_MASK) {
        // all three verts outside one pane edge -> whole triangle off-pane
        return 0;
    }
    // A SC_FORCE vertex's edge bits are meaningless -> clip every plane; otherwise clip only the
    // CROSSED, NON-REDUNDANT edges (sc_active_mask drops framebuffer-border edges - overhang past
    // those lands off-screen and the PVR userclip eats it).
    uint8_t clip_mask = (oc_or & SC_FORCE) ? (SC_FORCE | SC_EDGE_MASK)
                                           : (oc_or & sc_active_mask);
    if (clip_mask == 0) {
        out[0][0] = v1; out[0][1] = v2; out[0][2] = v3;
        return 1;
    }
    return gfx_build_clipped_fan(v1, v2, v3, out, clip_mask);
}

// ---- Per-STATE triangle setup (hoisted out of gfx_sp_tri1) -----------------------------------
// Everything below depends only on RDP/RSP state (other modes, combiner, textures, viewport, prim/env,
// geometry mode), NOT on the triangle. It used to run for EVERY triangle (~10us/tri = 2000 cycles,
// PROF 2026-08-16). Now it runs once per state generation: gfx_run_dl bumps rdp_state_gen on every
// non-triangle opcode (and per frame), and gfx_sp_tri1 re-runs this only when the generation moved.
// Results the per-triangle bake needs are left in `ts`.
static uint32_t rdp_state_gen = 1, tri_setup_gen = 0;
static uint8_t quad_setup_valid;   // defined with the 2D setup below
static struct {
    uint8_t depth_test, zmode_decal, usetex, stale_texel1, linear_filter, cc_nocache;
    float recip_tex_width, recip_tex_height, uls, ult;
    uint32_t texenv;
} ts;
static void __attribute__((noinline)) gfx_tri_state_setup(void) {
#if GFX_PROF
    uint64_t su0 = PROF_NOW();
#endif
    if (matrix_dirty) {
        matrix_dirty = 0;
    }

    uint8_t depth_test = (rsp.geometry_mode & G_ZBUFFER) == G_ZBUFFER;
    // Depth TEST follows the render mode's Z_CMP bit (the "ZB" in G_RM_*_ZB_*), not just the
    // global G_ZBUFFER geometry mode. Non-ZB modes (overlays, paint-order effects) clear Z_CMP and
    // passively fall into the ortho-overlay foreground path instead of z-rejecting behind 3D.
    depth_test = depth_test && (rdp.other_mode_l & Z_CMP);
    if ((depth_test != rendering_state.depth_test)) {
        gfx_rapi->set_depth_test(depth_test);
        rendering_state.depth_test = depth_test;
    }

    uint8_t z_upd = (rdp.other_mode_l & Z_UPD) == Z_UPD;
    if ((z_upd != rendering_state.depth_mask)) {
        gfx_rapi->set_depth_mask(z_upd);
        rendering_state.depth_mask = z_upd;
    }

    uint8_t zmode_decal = (rdp.other_mode_l & ZMODE_DEC) == ZMODE_DEC;
    if ((zmode_decal != rendering_state.decal_mode)) {
        gfx_rapi->set_zmode_decal(zmode_decal);
        rendering_state.decal_mode = zmode_decal;
    }

    if (rdp.viewport_or_scissor_changed) {
        if (memcmp(&rdp.viewport, &rendering_state.viewport, sizeof(rdp.viewport)) != 0) {
            gfx_rapi->set_viewport(rdp.viewport.x, rdp.viewport.y, rdp.viewport.width, rdp.viewport.height);
            rendering_state.viewport = rdp.viewport;
            // PVR set_viewport is a no-op. The screen map / pane planes update at
            // gfx_calc_and_set_viewport (float source), NOT from the uint16 rdp rect here -
            // the texrect path temporarily swaps rdp.viewport and must not retarget them.
        }
        if (memcmp(&rdp.scissor, &rendering_state.scissor, sizeof(rdp.scissor)) != 0) {
            gfx_rapi->set_scissor(rdp.scissor.x, rdp.scissor.y, rdp.scissor.width, rdp.scissor.height);
            rendering_state.scissor = rdp.scissor;
        }
        rdp.viewport_or_scissor_changed = 0;
    }

#if GFX_PROF
    uint64_t su1 = PROF_NOW(); prof_su[0] += su1 - su0;
#endif
    uint32_t cc_id = rdp.combine_mode;

    uint8_t use_alpha = (rdp.other_mode_l & (G_BL_A_MEM << 18)) == 0;
    uint8_t use_fog = (rdp.other_mode_l >> 30) == G_BL_CLR_FOG;
    if ((rsp.use_fog != use_fog)) {
        rsp.use_fog = use_fog;
    }
    uint8_t texture_edge = (rdp.other_mode_l & CVG_X_ALPHA) == CVG_X_ALPHA;
    uint8_t use_noise = (rdp.other_mode_h == 0x2ca0);

    if (alpha_noise) {
        alpha_noise = 0;
    }

    if (use_alpha) {
        cc_id |= SHADER_OPT_ALPHA;
    }
    if (use_fog) {
        cc_id |= SHADER_OPT_FOG;
    }
    if (texture_edge) {
        cc_id |= SHADER_OPT_TEXTURE_EDGE;
    }
    if (use_noise) {
        cc_id |= SHADER_OPT_NOISE;
    }
    if (!use_alpha) {
        cc_id &= ~0xfff000;
    }

    struct ColorCombiner* comb = gfx_lookup_or_create_color_combiner(cc_id);
    struct ShaderProgram* prg = comb->prg;
    if ((prg != rendering_state.shader_program)) {
        gfx_rapi->unload_shader(rendering_state.shader_program);
        gfx_rapi->load_shader(prg);
        rendering_state.shader_program = prg;
    }

    if ((use_alpha != rendering_state.alpha_blend)) {
        gfx_rapi->set_use_alpha(use_alpha);
        rendering_state.alpha_blend = use_alpha;
    }

    uint8_t num_inputs;
    uint8_t used_texture = gfx_rapi->shader_get_info(prg, &num_inputs);

#if GFX_PROF
    uint64_t su2 = PROF_NOW(); prof_su[1] += su2 - su1;
#endif
    void* texaddr = SEGMENTED_TO_VIRTUAL(rdp.texture_to_load.addr);
    void* grass = SEGMENTED_TO_VIRTUAL(aCoGroundGrassTex);
    void* water = SEGMENTED_TO_VIRTUAL(D_CO_6028A60);
    void* ve1ground = SEGMENTED_TO_VIRTUAL(aVe1GroundTex);
    void* maground = SEGMENTED_TO_VIRTUAL(aMaGroundTex);
    void* aqground = SEGMENTED_TO_VIRTUAL(aAqGroundTex);
    void* aqwater = SEGMENTED_TO_VIRTUAL(aAqWaterTex);
    void* ti_ground = SEGMENTED_TO_VIRTUAL(D_TI_6001BA8);
    void* zowater = SEGMENTED_TO_VIRTUAL(aZoWaterTex);
    void* kabackdrop = SEGMENTED_TO_VIRTUAL(aKaBackdropTex);
    void* fobackdrop = SEGMENTED_TO_VIRTUAL(aFoBackdropTex);
    void* ve2backdrop = SEGMENTED_TO_VIRTUAL(aVe2BackdropTex);
    float recip_tex_width = 0.03125f;  // 1 / 32
    float recip_tex_height = 0.03125f; // 1 / 32
    // shader_get_info reports TEXEL0 use only (single-tile). The NOISE x stale-TEXEL1 materials
    // reference ONLY TEXEL1, so they'd draw untextured; force texturing so tile 0 (the 4x4 white
    // texture they do load) is bound and the backend can swap in the raw-VRAM garbage window.
    int stale_texel1 = pvr_combiner_stale_texel1();
    uint8_t usetex = used_texture || stale_texel1;
    uint8_t linear_filter = (rdp.other_mode_h & (3U << G_MDSFT_TEXTFILT)) != G_TF_POINT;

    if (usetex) {
        // Texel-interpolant lerp with a decreasing channel -> inverted-texture variant (see
        // pvr_combiner_tex_invert_mask). Part of the cache key, so a mask change is a re-lookup.
        {
            uint8_t m = pvr_combiner_tex_invert_mask();
            if (m != pvr_tex_invert_mask) { pvr_tex_invert_mask = m; rdp.textures_changed[0] = 1; }
            gfx_pvr_set_tex_invert_mask(m);
        }
        if (rdp.textures_changed[0]) {
            import_texture(0);
            rdp.textures_changed[0] = 0;
        }
        uint8_t cms = rdp.texture_tile.cms;
        uint8_t cmt = rdp.texture_tile.cmt;

        uint8_t special_cm = (((gCurrentLevel == LEVEL_CORNERIA) && ((texaddr == grass) || (texaddr == water))) ||
                              ((gCurrentLevel == LEVEL_VENOM_1) && (texaddr == ve1ground)) ||
                              ((gCurrentLevel == LEVEL_MACBETH) && (texaddr == maground)) ||
                              ((gCurrentLevel == LEVEL_AQUAS) && ((texaddr == aqground) || (texaddr == aqwater))) ||
                              ((gCurrentLevel == LEVEL_TITANIA) && (texaddr == ti_ground)) ||
                              ((gCurrentLevel == LEVEL_ZONESS) && (texaddr == zowater)));

        // CLAMP-S sky backdrops -> WRAP-S (junction seam between the two side-by-side copies; see externs).
        uint8_t backdrop_wrap_s = ((gCurrentLevel == LEVEL_KATINA)  && (texaddr == kabackdrop)) ||
                                  ((gCurrentLevel == LEVEL_FORTUNA) && (texaddr == fobackdrop)) ||
                                  ((gCurrentLevel == LEVEL_VENOM_2) && (texaddr == ve2backdrop));

        if (special_cm) {
            cms = 0;
            cmt = 0;
            if (gCurrentLevel == LEVEL_TITANIA) {
                cms = G_TX_MIRROR;
            }
        } else if (backdrop_wrap_s) {
            cms = 0;   // G_TX_WRAP; T stays CLAMP as authored
        } else {
            uint32_t tex_size_bytes = rdp.loaded_texture[rdp.texture_to_load.tile_number].size_bytes;
            uint32_t line_size = rdp.texture_tile.line_size_bytes;
            uint32_t tex_height_i;
            if (line_size == 0) {
                line_size = 1;
                tex_height_i = tex_size_bytes;
            } else {
                tex_height_i = (uint32_t) ((float) tex_size_bytes / (float) line_size);
            }

            switch (rdp.texture_tile.siz) {
                case G_IM_SIZ_4b:
                    line_size <<= 1;
                    break;
                case G_IM_SIZ_8b:
                    break;
                case G_IM_SIZ_16b:
                    line_size >>= 1;
                    break;
                case G_IM_SIZ_32b:
                    line_size >>= 1;
                    tex_height_i >>= 1;
                    break;
            }
            uint32_t tex_width_i = line_size;

            uint32_t tex_width2_i = (rdp.texture_tile.lrs - rdp.texture_tile.uls + 4) >> 2;
            uint32_t tex_height2_i = (rdp.texture_tile.lrt - rdp.texture_tile.ult + 4) >> 2;

            uint32_t tex_width1 = tex_width_i << (cms & G_TX_MIRROR);
            uint32_t tex_height1 = tex_height_i << (cmt & G_TX_MIRROR);

            if ((cms & G_TX_CLAMP) && ((cms & G_TX_MIRROR) || (tex_width1 != tex_width2_i))) {
                cms &= (~G_TX_CLAMP);
            }

            if ((cmt & G_TX_CLAMP) && ((cmt & G_TX_MIRROR) || (tex_height1 != tex_height2_i))) {
                cmt &= (~G_TX_CLAMP);
            }
        }
        linear_filter = do_the_blur ? 0 : (rdp.other_mode_h & (3U << G_MDSFT_TEXTFILT)) != G_TF_POINT;

        gfx_rapi->set_sampler_parameters(linear_filter, cms, cmt);
        rendering_state.textures[0]->linear_filter = linear_filter;
        rendering_state.textures[0]->cms = cms;
        rendering_state.textures[0]->cmt = cmt;

        uint32_t tex_width = (rdp.texture_tile.lrs - rdp.texture_tile.uls + 4) >> 2;
        uint32_t tex_height = (rdp.texture_tile.lrt - rdp.texture_tile.ult + 4) >> 2;
        recip_tex_width = shz_fast_invf((float) tex_width);
        recip_tex_height = shz_fast_invf((float) tex_height);
    }

    if (!do_the_blur) {
        recip_tex_width *= gfx_pvr_get_u_scale();
        recip_tex_height *= gfx_pvr_get_v_scale();
    }

    float ofs = linear_filter ? 0.5f : 0.0f;
    float uls = (float) (rdp.texture_tile.uls * 0.25f) - ofs;
    float ult = (float) (rdp.texture_tile.ult * 0.25f) - ofs;

#if GFX_PROF
    uint64_t su3 = PROF_NOW(); prof_su[2] += su3 - su2;
#endif
    // ---- 3-way OP/PT/TR classification (raw alpha mux + render-mode flags) ----
    //   FORCE_BL set -> TR (real alpha blend, autosorted, composited last)
    //   else alpha-test CUTOUT -> PT (coverage edge OR texel-alpha with alpha compare on)
    //   else -> OP (opaque, live)
    {
        int aa = (rdp.combine_w0 >> 12) & 7, ab = (rdp.combine_w1 >> 12) & 7;
        int ac = (rdp.combine_w0 >>  9) & 7, ad = (rdp.combine_w1 >>  9) & 7;
        uint8_t alpha_uses_texel = (aa == 1 || aa == 2 || ab == 1 || ab == 2 ||
                                    ac == 1 || ac == 2 || ad == 1 || ad == 2);
        uint8_t alpha_compare_on = (rdp.other_mode_l & 3) != 0;   // G_AC_THRESHOLD/DITHER
        uint8_t real_blend = (rdp.other_mode_l & FORCE_BL) != 0;
        uint8_t cutout     = texture_edge || (alpha_uses_texel && alpha_compare_on);
        uint8_t kind = real_blend ? 2 : (cutout ? 1 : 0);
        if (kind != pvr_cur_kind) { gfx_pvr_set_blend(kind); pvr_cur_kind = kind; }
        if (kind == 2) {   // TR: derive + push blend factors
            uint8_t bs, bd;
            pvr_derive_blend(rdp.other_mode_l, rdp.other_mode_h, &bs, &bd);
            if (bs != pvr_cur_bsrc || bd != pvr_cur_bdst) {
                gfx_pvr_set_blend_factors(bs, bd);
                pvr_cur_bsrc = bs; pvr_cur_bdst = bd;
            }
        }
    }

    // Declare this draw's real texturing intent so the poly header matches (not cur_shader's flag).
    gfx_pvr_set_textured(usetex);
    // NOISE x stale-TEXEL1 materials get the backend's raw-VRAM garbage texture (header state).
    gfx_pvr_set_stale_texel1(stale_texel1);

    // texenv derived from the N64 combiner; a change ends the PVR batch (per-batch poly header).
    uint32_t texenv = usetex ? derive_pvr_texenv(rdp.combine_mode) : GFX_TEXENV_MODULATE;
    if (texenv != rendering_state.tex_env) { gfx_rapi->set_tex_env(texenv); rendering_state.tex_env = texenv; }

    // Combiner-eval cache key (see cc_cache): any change -> new stamp -> every vertex re-evaluates once.
    {
        uint32_t k[6] = { rdp.combine_w0, rdp.combine_w1,
                          PACK_ARGB8888(rdp.prim_color.r, rdp.prim_color.g, rdp.prim_color.b, rdp.prim_color.a),
                          PACK_ARGB8888(rdp.env_color.r,  rdp.env_color.g,  rdp.env_color.b,  rdp.env_color.a),
                          texenv | ((uint32_t) usetex << 8) | ((uint32_t) zmode_decal << 9) |
                              ((uint32_t) pvr_tex_invert_mask << 10),
                          rdp.other_mode_h & (3U << G_MDSFT_CYCLETYPE) };
        if (k[0] != cc_last_key[0] || k[1] != cc_last_key[1] || k[2] != cc_last_key[2] ||
            k[3] != cc_last_key[3] || k[4] != cc_last_key[4] || k[5] != cc_last_key[5]) {
            for (int j = 0; j < 6; j++) cc_last_key[j] = k[j];
            if (++cc_state_stamp == 0) cc_state_stamp = 1;
            PROF_INC(prof_stamps);
            pvr_cc_prepare_cached(k, rdp.combine_w0, rdp.combine_w1, usetex, texenv);   // once per state
        }
    }
    const int cc_nocache = (((rdp.combine_w0 >> 20) & 0xF) == 7) || (((rdp.combine_w0 >> 5) & 0xF) == 7);   // NOISE

    // PVR HW vertex fog on/off follows the RSP G_FOG geometry mode (the same signal feeding
    // gfx_calc_fog). fog_type is in the poly header, so a change ends the batch.
    uint8_t fog_on = (rsp.geometry_mode & G_FOG) != 0;
    if (fog_on != rendering_state.fog_enabled) { gfx_pvr_set_fog(fog_on); rendering_state.fog_enabled = fog_on; }

#if GFX_PROF
    prof_su[3] += PROF_NOW() - su3;
#endif
    ts.depth_test = depth_test; ts.zmode_decal = zmode_decal; ts.usetex = usetex;
    ts.stale_texel1 = (uint8_t) stale_texel1; ts.linear_filter = linear_filter; ts.cc_nocache = (uint8_t) cc_nocache;
    ts.recip_tex_width = recip_tex_width; ts.recip_tex_height = recip_tex_height; ts.uls = uls; ts.ult = ult;
    ts.texenv = texenv;
}

static void __attribute__((noinline)) gfx_sp_tri1_impl(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx);
static void gfx_sp_tri1(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx) {
#if GFX_PROF
    uint64_t t0 = PROF_NOW();
    prof_t_tri_mark = t0;
    gfx_sp_tri1_impl(vtx1_idx, vtx2_idx, vtx3_idx);
    prof_t_tri += PROF_NOW() - t0;
#else
    gfx_sp_tri1_impl(vtx1_idx, vtx2_idx, vtx3_idx);
#endif
}
static void __attribute__((noinline)) GFX_HOT gfx_sp_tri1_impl(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx) {
    struct LoadedVertex* v1 = &rsp.loaded_vertices[vtx3_idx];
    MEM_BARRIER_PREF(v1);
    struct LoadedVertex* v2 = &rsp.loaded_vertices[vtx2_idx];
    struct LoadedVertex* v3 = &rsp.loaded_vertices[vtx1_idx];
    uint8_t l_clip_rej[3] = { clip_rej[vtx3_idx], clip_rej[vtx2_idx], clip_rej[vtx1_idx] };
    MEM_BARRIER_PREF(v2);
    struct LoadedVertex* v_arr[3] = { v1, v2, v3 };

    uint8_t c0 = l_clip_rej[0];
    uint8_t c1 = l_clip_rej[1];
    uint8_t c2 = l_clip_rej[2];
    MEM_BARRIER_PREF(v3);

    if ((c0 & c1 & c2) & 0x3f) {
        // The whole triangle lies outside the visible area
        return;
    }
    if ((rsp.geometry_mode & G_CULL_BOTH) != 0) {
        // PVR stores RAW CLIP _x,_y; the back-face test needs NDC, so divide per vertex.
        float rw1 = shz_fast_invf(v1->_w), rw2 = shz_fast_invf(v2->_w), rw3 = shz_fast_invf(v3->_w);
        float dx1 = (v1->_x * rw1) - (v2->_x * rw2);
        float dy1 = (v1->_y * rw1) - (v2->_y * rw2);
        float dx2 = (v3->_x * rw3) - (v2->_x * rw2);
        float dy2 = (v3->_y * rw3) - (v2->_y * rw2);
        float cross = dx1 * dy2 - dy1 * dx2;
        if ((c0 ^ c1 ^ c2) & 0x40) {
            // If one vertex lies behind the eye, negating cross will give the correct result.
            // If all vertices lie behind the eye, the triangle will be rejected anyway.
            cross = -cross;
        }
        switch (rsp.geometry_mode & G_CULL_BOTH) {
            case G_CULL_FRONT:
                if (cross >= 0) {
                    return;
                }
                break;
            case G_CULL_BACK:
                if (cross <= 0) {
                    return;
                }
                break;
            default:
                break;
        }
    }

    if (tri_setup_gen != rdp_state_gen) {
        PROF_INC(prof_setups);
        gfx_tri_state_setup();
        tri_setup_gen = rdp_state_gen;
    }
    quad_setup_valid = 0;   // a 3D draw changes rendering_state -> the 2D setup must re-derive
    const uint8_t depth_test = ts.depth_test, zmode_decal = ts.zmode_decal, usetex = ts.usetex;
    const int cc_nocache = ts.cc_nocache;
    const float recip_tex_width = ts.recip_tex_width, recip_tex_height = ts.recip_tex_height;
    const float uls = ts.uls, ult = ts.ult;
    int i;
    (void) zmode_decal;
#if GFX_PROF
    prof_t_tri_setup += PROF_NOW() - prof_t_tri_mark;   // state/combiner/texture setup
#endif
    // Clip the triangle into a fan. Full-screen scissor (single player): near-plane clip only,
    // 1-2 tris - the pre-existing hot path, zero extra cost. Split-screen pane: outcode-classify
    // + software-clip pane-crossing triangles out of line - un-clipped frustum overhang would
    // bake to screen pixels inside the NEIGHBOURING pane and depth-stomp it (mk64-dc's scheme).
    // NB: v1 = loaded_vertices[vtx3_idx] (the load above swaps), so the indices pass swapped too.
    static struct LoadedVertex *fan_tris[6][3];
    int n_tris = sc_is_fullscreen
                     ? sm_near_clip_fan(v1, v2, v3, fan_tris)
                     : gfx_scissor_classify_fan(v1, v2, v3, vtx3_idx, vtx2_idx, vtx1_idx, fan_tris);
    if (n_tris == 0) return;
#if GFX_PROF
    uint64_t prof_tc = PROF_NOW(); prof_t_clip += prof_tc - prof_t_tri_mark;   // includes setup; subtract later
#endif

    PROF_INC(prof_tris);
    // Ortho 3D GEOMETRY (depth-tested): under ortho w~const, so 1/w carries no depth - use clip z.
    int ortho_3d = proj_is_ortho && depth_test;
    // OVERLAY = a Z-off layer drawn AFTER the 3D scene (foreground HUD / targeting reticle) -> near
    // paint-order z so it sits on top; a Z-off layer BEFORE any 3D is a true BACKDROP. The catch:
    // the ground is ALSO perspective + Z-off (non-Z_CMP painter's-order backdrop) but must keep 1/w
    // for perspective-correct texture, and draw-order (has_done_3d) does NOT separate it from the
    // reticle. The clean split is the LIST KIND: the ground is OPAQUE (OP), the reticle/effects are
    // translucent/punch-through (TR/PT). So an OPAQUE perspective Z-off surface is NOT an overlay
    // (falls through to dz=1/w below); only ortho, or non-opaque perspective, Z-off geom near-pins.
    // ORTHO Z-off content is a 2D layer -> its depth is paint order, ALWAYS overlay (near, staggered
    // by screen_2d_z). The has_done_3d/prev_frame_had_persp gate is only meaningful for PERSPECTIVE
    // Z-off (backdrop-before-3D like Area 6 vs overlay-after-3D like the reticle). Without the ortho
    // short-circuit, an ortho menu billboard drawn before that screen's own perspective decoration
    // (ranking planets vs the perspective medals) was stranded on the far-pin branch -> behind its
    // 2D boxes. Ortho short-circuit fixes the menu; the perspective branch (Area 6) is unchanged.
    int overlay = (!depth_test && (proj_is_ortho || !prev_frame_had_persp || has_done_3d)
                   && (proj_is_ortho || pvr_cur_kind != 0))
                  || force_paint_overlay;   // 'OVLY' escape hatch: any list kind. An OPAQUE 'OVLY' DL
                                            // (briefing TV face, Z_CMP+Z_UPD on N64, beaten by draw
                                            // order there) writes near paint-order depth, so the
                                            // autosorted nebula TR can no longer cut through it; the
                                            // glow drawn after it gets a later (nearer) z2d -> on top.
    float z2d = 0.0f;
    if (overlay) { screen_2d_z += 0.005f; z2d = screen_2d_z; }

    // Backdrop far-slab depth: coplanar backdrop tiles all share one far depth, and autosort TR
    // has no stable order at equal depth -> tile-seam shimmer. Give each successive backdrop
    // primitive a tiny draw-order stagger (later = fractionally nearer = wins, N64 painter's order),
    // clamped well inside the far slab so the whole planet stays behind the foreground. Computed
    // ONCE per call so all 3 verts share it (a per-vertex value would re-tilt the triangle).
    float bd_z = PVR_Z_BACKDROP0;
    if (!overlay && !depth_test && !proj_is_ortho && pvr_cur_kind != 0) {
        bd_z = backdrop_far_z;
        if (backdrop_far_z < PVR_Z_BACKDROP_MAX) backdrop_far_z += 0.0000002f;   // stays << foreground 1/w
    }

    // No buf_vbo: OP bakes into op_emit then DR-submits; PT/TR bake directly into their bucket.
    const int op_stream = (pvr_cur_kind == 0);
    size_t op_n = 0;
    pvr_vertex_t *emit = op_stream ? op_emit : pvr_reserve(pvr_cur_kind, (size_t) n_tris * 3);
    if (!emit) n_tris = 0;   // bucket overflow -> drop this source triangle (pvr_reserve logged it)
    for (int ti = 0; ti < n_tris; ti++) {
        v_arr[0] = fan_tris[ti][0];
        v_arr[1] = fan_tris[ti][1];
        v_arr[2] = fan_tris[ti][2];
        for (i = 0; i < 3; i++) {
            pvr_vertex_t * const bv = &emit[op_n];
            // perspective divide + viewport map -> screen pixels; z = 1/w (inverse depth).
            float invw = shz_fast_invf(v_arr[i]->_w);

            bv->x = sm_xscale * (v_arr[i]->_x * invw) + sm_xbias;
            bv->y = sm_yscale * (v_arr[i]->_y * invw) + sm_ybias;
            float dz;
            if (ortho_3d)               dz = 1.0f - (v_arr[i]->_z * invw);                // ortho clip-z depth
            else if (depth_test)        dz = zmode_decal ? invw * PVR_DECAL_ZBIAS : invw; // 1/w (decal nudge)
            else if (proj_is_ortho)     dz = PVR_Z_FARPIN;                                // ortho Z-off -> far-pin
            else if (pvr_cur_kind == 0) dz = invw;                                        // OPAQUE persp Z-off surface (ground) -> 1/w
            else                        dz = bd_z;                                        // translucent persp Z-off backdrop (space sky) -> far slab, draw-order staggered
            bv->z = overlay ? z2d : dz;

            if (usetex) {
                bv->u = (v_arr[i]->u - uls) * recip_tex_width;
                bv->v = (v_arr[i]->v - ult) * recip_tex_height;
            }

            // Evaluate the N64 colour+alpha combiner directly. SHADE = the per-vertex lit/material
            // colour from the matrix-lighting pipeline (NO (255+c)/2 / level hacks - accurate path).
            {
                uint32_t _argb, _oargb;
                int vi = (int) (v_arr[i] - rsp.loaded_vertices);
                int cacheable = !cc_nocache && vi >= 0 && vi < MAX_VERTICES + 2;   // fan temporaries excluded
                if (cacheable && cc_cache[vi].stamp == cc_state_stamp) {
                    _argb = cc_cache[vi].argb; _oargb = cc_cache[vi].oargb;
                    PROF_INC(prof_hits);
                } else {
                    PROF_INC(prof_evals);
                    pvr_eval_combiner_fast(&v_arr[i]->color, &_argb, &_oargb);
                    if (cacheable) { cc_cache[vi].stamp = cc_state_stamp; cc_cache[vi].argb = _argb; cc_cache[vi].oargb = _oargb; }
                }
                _oargb |= (uint32_t) v_arr[i]->fog << 24;   // fog density -> oargb.alpha (HW vertex fog)
                bv->argb = _argb;
                bv->oargb = _oargb;
            }
            // Stamp the PVR strip flag at vertex creation (TRIANGLES: EOL on every 3rd). PT/TR bucket
            // slots are already flagged by pvr_reserve, so only the OP stream needs it here.
            if (op_stream) bv->flags = (i == 2) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            op_n += 1;
        }
    }
#if GFX_PROF
    uint64_t prof_tb = PROF_NOW(); prof_t_bake += prof_tb - prof_tc;
#endif
    if (op_stream && op_n) { PROF_ADD(gfx_pvr_prof_op, op_n); pvr_submit_op_inline(op_emit, op_n); }
#if GFX_PROF
    prof_t_submit += PROF_NOW() - prof_tb;
#endif
    // Defer the has_done_3d flip: a perspective tri only *arms* it here; it is promoted
    // when the current display list ends (G_ENDDL). This keeps a multi-triangle backdrop
    // DL atomic -- its first triangle no longer flips the flag and shoves its own siblings
    // onto the near-overlay path (the Area 6 planet: 1 tri correct, the rest drawn over).
    // Also gate on depth_test: has_done_3d means "the DEPTH-TESTED 3D scene has been drawn", so a
    // Z-off BACKDROP (the Andross swirls, CLD_SURF perspective) does NOT flip it -- otherwise the
    // first of two backdrop DL draws would promote the flag at its G_ENDDL and turn the second into
    // a near overlay drawn OVER the foreground. The real actors are depth-tested and still set it,
    // so the reticle/HUD overlays are unaffected.
    if (!proj_is_ortho && depth_test) has_done_3d_pending = 1;
    // cur_frame_persp: "this frame DREW perspective geometry" (latched into prev_frame_had_persp
    // at start_frame). Set here - NOT at projection-matrix load - so a 2D menu that loads a
    // perspective matrix without using it doesn't arm the backdrop gates next frame.
    if (!proj_is_ortho) { has_drawn_persp_tri = 1; cur_frame_persp = 1; }
}

extern int gfx_pvr_bound_texture_opaque(void);   // 1 iff the bound texture has no transparent texels

int do_ext_fill = 0;

// ---- Per-STATE 2D quad setup (hoisted out of gfx_sp_quad_2d) --------------------------------
// The 2D rect path re-derived depth/viewport/combiner/shader/texture/blend state for EVERY rect
// (~20us/quad, PROF 2026-08-16; the starfield alone is up to 1000 rects). Valid while: the state
// generation is unchanged (rect/fill-colour opcodes do not bump it), the 2D mode key (geometry mode,
// other modes, ext-fill/blur) is unchanged, and no 3D triangle or far-pin special case ran since.
static uint32_t quad_setup_gen = 0, quad_key[4];
static uint8_t quad_setup_valid = 0;   // (declared above for the 3D path)
static struct {
    uint8_t use_texture, used_texture, linear_filter, texture_edge, kind;
    float recip_tex_width, recip_tex_height, uls, ult, uscl, vscl;
    uint32_t texenv2d;
} qs;
static void __attribute__((noinline)) gfx_quad_state_setup(void) {
    uint8_t depth_test = (rsp.geometry_mode & G_ZBUFFER) == G_ZBUFFER;
    if (depth_test != rendering_state.depth_test) {
        gfx_rapi->set_depth_test(depth_test);
        rendering_state.depth_test = depth_test;
    }

    uint8_t z_upd = (rdp.other_mode_l & Z_UPD) == Z_UPD;
    // Force depth WRITE for OPAQUE 2D (same OP/PT/TR test as the emit below). A far-pinned opaque
    // backdrop (starfield) must write far depth so the 3D scene resolves in front of it; an opaque
    // HUD overlay writes its near 2D depth so later TR can't paint over it.
    {
        int aa = (rdp.combine_w0 >> 12) & 7, ab = (rdp.combine_w1 >> 12) & 7;
        int ac = (rdp.combine_w0 >>  9) & 7, ad = (rdp.combine_w1 >>  9) & 7;
        uint8_t alpha_uses_texel = (aa == 1 || aa == 2 || ab == 1 || ab == 2 ||
                                    ac == 1 || ac == 2 || ad == 1 || ad == 2);
        uint8_t real_blend = (rdp.other_mode_l & FORCE_BL) != 0;
        uint8_t cutout = ((rdp.other_mode_l & CVG_X_ALPHA) != 0) ||
                         (alpha_uses_texel && ((rdp.other_mode_l & 3) != 0));
        if (!real_blend && !cutout) z_upd = 1;   // opaque 2D -> write depth
    }
    if (z_upd != rendering_state.depth_mask) {
        gfx_rapi->set_depth_mask(z_upd);
        rendering_state.depth_mask = z_upd;
    }

    uint8_t zmode_decal = (rdp.other_mode_l & ZMODE_DEC) == ZMODE_DEC;
    if (zmode_decal != rendering_state.decal_mode) {
        gfx_rapi->set_zmode_decal(zmode_decal);
        rendering_state.decal_mode = zmode_decal;
    }

    if (rdp.viewport_or_scissor_changed) {
        if (memcmp(&rdp.viewport, &rendering_state.viewport, sizeof(rdp.viewport)) != 0) {
            gfx_rapi->set_viewport(rdp.viewport.x, rdp.viewport.y, rdp.viewport.width, rdp.viewport.height);
            rendering_state.viewport = rdp.viewport;
        }
        if (memcmp(&rdp.scissor, &rendering_state.scissor, sizeof(rdp.scissor)) != 0) {
            gfx_rapi->set_scissor(rdp.scissor.x, rdp.scissor.y, rdp.scissor.width, rdp.scissor.height);
            rendering_state.scissor = rdp.scissor;
        }
        rdp.viewport_or_scissor_changed = 0;
    }

    uint32_t cc_id = rdp.combine_mode;

    uint8_t use_alpha = (rdp.other_mode_l & (G_BL_A_MEM << 18)) == 0;
    uint8_t use_fog = (rdp.other_mode_l >> 30) == G_BL_CLR_FOG;
    if (rsp.use_fog != use_fog) {
        rsp.use_fog = use_fog;
    }

    uint8_t texture_edge = (rdp.other_mode_l & CVG_X_ALPHA) == CVG_X_ALPHA;

    // this is literally only for the stupid sun in the intro and nothing else
    uint8_t use_noise = (rdp.other_mode_h == 0x2ca0);
    alpha_noise = use_noise;

    if (texture_edge) {
        use_alpha = 1;
    }

    if (use_alpha) {
        cc_id |= SHADER_OPT_ALPHA;
    }

    if (use_fog) {
        cc_id |= SHADER_OPT_FOG;
    }

    if (texture_edge) {
        cc_id |= SHADER_OPT_TEXTURE_EDGE;
    }

    if (use_noise) {
        cc_id |= SHADER_OPT_NOISE;
    }

    if (!use_alpha) {
        cc_id &= ~0xfff000;
    }

    struct ColorCombiner* comb = gfx_lookup_or_create_color_combiner(cc_id);
    struct ShaderProgram* prg = comb->prg;
    if (prg != rendering_state.shader_program) {
        gfx_rapi->unload_shader(rendering_state.shader_program);
        gfx_rapi->load_shader(prg);
        rendering_state.shader_program = prg;
    }

    if (use_alpha != rendering_state.alpha_blend) {
        gfx_rapi->set_use_alpha(use_alpha);
        rendering_state.alpha_blend = use_alpha;
    }

    uint8_t num_inputs;
    uint8_t used_texture = gfx_rapi->shader_get_info(prg, &num_inputs);
    uint8_t linear_filter = 1;

    if (used_texture) {
        // 2D texrects never use the inverted-texel variant; clear a mask left by the 3D path.
        if (pvr_tex_invert_mask) { pvr_tex_invert_mask = 0; rdp.textures_changed[0] = 1; }
        gfx_pvr_set_tex_invert_mask(0);
        if (rdp.textures_changed[0]) {
            import_texture(0);
            rdp.textures_changed[0] = 0;
        }
        linear_filter = do_the_blur ? 0 : ((rdp.other_mode_h & (3U << G_MDSFT_TEXTFILT)) != G_TF_POINT);
        gfx_rapi->set_sampler_parameters(linear_filter, rdp.texture_tile.cms, rdp.texture_tile.cmt);
        rendering_state.textures[0]->linear_filter = linear_filter;
        rendering_state.textures[0]->cms = rdp.texture_tile.cms;
        rendering_state.textures[0]->cmt = rdp.texture_tile.cmt;
    }

    uint8_t use_texture = !do_ext_fill && used_texture;
    float recip_tex_width = 0.03125f;  // 1 / 32;
    float recip_tex_height = 0.03125f; // 1 / 32
    float uls = 0.0f, ult = 0.0f, uscl = 1.0f, vscl = 1.0f;
    if (use_texture && !do_the_blur) {
        uint32_t tex_width = ((rdp.texture_tile.lrs - rdp.texture_tile.uls + 4) * 0.25f);
        uint32_t tex_height = ((rdp.texture_tile.lrt - rdp.texture_tile.ult + 4) * 0.25f);
        recip_tex_width = shz_fast_invf((float) tex_width);
        recip_tex_height = shz_fast_invf((float) tex_height);
        float offs = linear_filter ? 0.5f : 0.0f;
        uls = (float) (rdp.texture_tile.ult * 0.25f) - offs;   // (sic: both from ult, as before)
        ult = (float) (rdp.texture_tile.ult * 0.25f) - offs;
        // POT-pad UV correction for the bound texture (backend getter).
        uscl = gfx_pvr_get_u_scale();
        vscl = gfx_pvr_get_v_scale();
    }
    // OP/PT/TR classification (same rule as the 3D path); the per-quad far-pin special case may
    // still turn a TR quad into OP (handled per quad, and it invalidates this setup afterwards).
    uint8_t kind;
    {
        int aa = (rdp.combine_w0 >> 12) & 7, ab = (rdp.combine_w1 >> 12) & 7;
        int ac = (rdp.combine_w0 >>  9) & 7, ad = (rdp.combine_w1 >>  9) & 7;
        uint8_t alpha_uses_texel = (aa == 1 || aa == 2 || ab == 1 || ab == 2 ||
                                    ac == 1 || ac == 2 || ad == 1 || ad == 2);
        uint8_t alpha_compare_on = (rdp.other_mode_l & 3) != 0;
        uint8_t real_blend = (rdp.other_mode_l & FORCE_BL) != 0;
        uint8_t cutout = texture_edge || (alpha_uses_texel && alpha_compare_on);
        // FILL-cycle rects physically cannot blend -> always OP (see the long note in the old body).
        uint32_t cyc = rdp.other_mode_h & (3U << G_MDSFT_CYCLETYPE);
        kind = (cyc == G_CYC_FILL) ? 0 : (real_blend ? 2 : (cutout ? 1 : 0));
    }
    // Declare this quad's real texturing intent so the header is colour for an untextured fill.
    gfx_pvr_set_textured(use_texture);
    gfx_pvr_set_stale_texel1(0);   // 2D never wears the stale-TEXEL1 garbage texture
    // texenv from the combiner (so PVR does the texel<->colour combine); fog off for 2D.
    uint32_t texenv = use_texture ? derive_pvr_texenv(rdp.combine_mode) : GFX_TEXENV_MODULATE;
    if (texenv != rendering_state.tex_env) { gfx_rapi->set_tex_env(texenv); rendering_state.tex_env = texenv; }
    if (rendering_state.fog_enabled) { gfx_pvr_set_fog(0); rendering_state.fog_enabled = 0; }
    // Prepared/specialised evaluator for this 2D state (shares the 3D machinery + key).
    {
        uint32_t k[6] = { rdp.combine_w0, rdp.combine_w1,
                          PACK_ARGB8888(rdp.prim_color.r, rdp.prim_color.g, rdp.prim_color.b, rdp.prim_color.a),
                          PACK_ARGB8888(rdp.env_color.r,  rdp.env_color.g,  rdp.env_color.b,  rdp.env_color.a),
                          texenv | ((uint32_t) use_texture << 8) | (0u << 9) | (0u << 10),
                          rdp.other_mode_h & (3U << G_MDSFT_CYCLETYPE) };
        if (k[0] != cc_last_key[0] || k[1] != cc_last_key[1] || k[2] != cc_last_key[2] ||
            k[3] != cc_last_key[3] || k[4] != cc_last_key[4] || k[5] != cc_last_key[5]) {
            for (int j = 0; j < 6; j++) cc_last_key[j] = k[j];
            if (++cc_state_stamp == 0) cc_state_stamp = 1;
            PROF_INC(prof_stamps);
            pvr_cc_prepare_cached(k, rdp.combine_w0, rdp.combine_w1, use_texture, texenv);
        }
    }
    qs.use_texture = use_texture; qs.used_texture = used_texture; qs.linear_filter = linear_filter;
    qs.texture_edge = texture_edge; qs.kind = kind;
    qs.recip_tex_width = recip_tex_width; qs.recip_tex_height = recip_tex_height;
    qs.uls = uls; qs.ult = ult; qs.uscl = uscl; qs.vscl = vscl; qs.texenv2d = texenv;
}

static void __attribute__((noinline)) gfx_sp_quad_2d(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx,
                                                     uint8_t vtx1_idx2, uint8_t vtx2_idx2, uint8_t vtx3_idx2) {
    pvr_vertex_t* v2d = &rsp.loaded_vertices_2D[0];
    {
        uint32_t k[4] = { rsp.geometry_mode, rdp.other_mode_l, rdp.other_mode_h,
                          (uint32_t) do_ext_fill | ((uint32_t) do_the_blur << 1) };
        if (!quad_setup_valid || quad_setup_gen != rdp_state_gen ||
            k[0] != quad_key[0] || k[1] != quad_key[1] || k[2] != quad_key[2] || k[3] != quad_key[3]) {
            gfx_quad_state_setup();
            quad_setup_gen = rdp_state_gen; quad_setup_valid = 1;
            for (int j = 0; j < 4; j++) quad_key[j] = k[j];
        }
    }
    tri_setup_gen = 0;   // a 2D draw changed rendering_state -> the 3D setup must re-derive
    const uint8_t use_texture = qs.use_texture;
    const uint8_t texture_edge = qs.texture_edge;
    (void) texture_edge;
    pvr_vertex_t* tmpv = v2d;

    if (use_texture) {
        if (!do_the_blur) {
            const float recip_tex_width = qs.recip_tex_width, recip_tex_height = qs.recip_tex_height;
            const float uls = qs.uls, ult = qs.ult, uscl = qs.uscl, vscl = qs.vscl;
            for (int qi = 0; qi < 4; qi++, tmpv++) {
                float u = (tmpv->u * 0.03125f) - uls;   // / 32
                float v = (tmpv->v * 0.03125f) - ult;
                tmpv->u = (u * recip_tex_width) * uscl;
                tmpv->v = (v * recip_tex_height) * vscl;
            }
        } else {
            // Fullscreen blur quad. slot0=ul, slot1=ll; slots 2/3 follow the backend's quad order
            // (PVR strip: ul,ll,ur,lr).
            tmpv->x = 0;   tmpv->y = 0;   tmpv->u = 0.0f;   tmpv++->v = 0.0f;      // slot0 = ul
            tmpv->x = 0;   tmpv->y = 479; tmpv->u = 0.0f;   tmpv++->v = 0.9375f;   // slot1 = ll
            tmpv->x = 639; tmpv->y = 0;   tmpv->u = 0.625f; tmpv++->v = 0.0f;      // slot2 = ur
            tmpv->x = 639; tmpv->y = 479; tmpv->u = 0.625f; tmpv->v = 0.9375f;     // slot3 = lr
        }
    }

    // Evaluate the REAL N64 combiner per 2D vertex (prepared/specialised evaluator, same maths as
    // the 3D path). Additive/const terms come back in oargb (PVR OFFSET colour).
    for (int qi = 0; qi < 4; qi++) {
        uint32_t pk = rsp.loaded_vertices_2D[qi].argb;   // incoming shade (ARGB8888)
        struct RGBA shade = { (uint8_t)(pk >> 16), (uint8_t)(pk >> 8), (uint8_t) pk, (uint8_t)(pk >> 24) };
        uint32_t argb, oargb;
        pvr_eval_combiner_fast(&shade, &argb, &oargb);
        rsp.loaded_vertices_2D[qi].argb = argb;
        rsp.loaded_vertices_2D[qi].oargb = oargb;   // additive offset (glare brighten etc.)
    }

    // OP/PT/TR routing (base kind from the state setup) + the per-quad far-pin backdrop special case.
    {
        uint8_t kind = qs.kind;
        int aa = (rdp.combine_w0 >> 12) & 7, ab = (rdp.combine_w1 >> 12) & 7;
        int ac = (rdp.combine_w0 >>  9) & 7, ad = (rdp.combine_w1 >>  9) & 7;
        uint8_t alpha_uses_texel = (aa == 1 || aa == 2 || ab == 1 || ab == 2 ||
                                    ac == 1 || ac == 2 || ad == 1 || ad == 2);
        // Effectively-opaque XLU BACKDROP -> OP + far-pin + depth-test (see gfx_quad_state_setup and
        // the SF64 PVR 2D backdrop->OP note): alpha-over quad, output alpha 255, no texel alpha,
        // pre-3D in a perspective frame. Depth-TEST on, depth-WRITE off; z starts at 0.001 (thin
        // strips drop at the extreme far-pin) and staggers per quad so a later quad beats an earlier
        // one - an exact OP z tie is resolved arbitrarily, which put a planet backdrop on top of a
        // just-turned-opaque full-white fade. It changes rendering_state; invalidate the setup after.
        // !has_drawn_persp_tri: same guard the fill far-pin path learned from the Sector Y outro -
        // a quad drawn AFTER any perspective tri this frame is an OVERLAY even when nothing was
        // depth-tested (has_done_3d==0). Without it, a full-white fade rect at alpha exactly 255,
        // drawn last over a Z-off cutscene scene (Area 6 -> Venom warp), got promoted+far-pinned to
        // ~0.001 and the planet (opaque Z-off persp, real 1/w >= that) punched through it.
        if (kind == 2 && prev_frame_had_persp && !has_done_3d && !has_drawn_persp_tri &&
            (!alpha_uses_texel || gfx_pvr_bound_texture_opaque()) &&
            (uint8_t)(rsp.loaded_vertices_2D[0].argb >> 24) == 255) {
            uint8_t bs, bd;
            pvr_derive_blend(rdp.other_mode_l, rdp.other_mode_h, &bs, &bd);
            if (bs == GFX_BLENDF_SRCALPHA && bd == GFX_BLENDF_INVSRCALPHA) {
                kind = 0;
                rsp.loaded_vertices_2D[0].z = rsp.loaded_vertices_2D[1].z =
                rsp.loaded_vertices_2D[2].z = rsp.loaded_vertices_2D[3].z = quad_backdrop_z;
                if (quad_backdrop_z < PVR_Z_QUAD_BD_MAX) quad_backdrop_z += 0.0000002f;
                if (!rendering_state.depth_test) { gfx_rapi->set_depth_test(1); rendering_state.depth_test = 1; }
                if (rendering_state.depth_mask)  { gfx_rapi->set_depth_mask(0); rendering_state.depth_mask = 0; }
                quad_setup_valid = 0;
            }
        }
        if (kind != pvr_cur_kind) { gfx_pvr_set_blend(kind); pvr_cur_kind = kind; }
        if (kind == 2) {
            uint8_t bs, bd;
            pvr_derive_blend(rdp.other_mode_l, rdp.other_mode_h, &bs, &bd);
            if (bs != pvr_cur_bsrc || bd != pvr_cur_bdst) {
                gfx_pvr_set_blend_factors(bs, bd);
                pvr_cur_bsrc = bs; pvr_cur_bdst = bd;
            }
        }
    }

    gfx_rapi->draw_triangles_2d((void*) rsp.loaded_vertices_2D, 4, use_texture);
}

static void gfx_sp_geometry_mode(uint32_t clear, uint32_t set) {
    rsp.geometry_mode &= ~clear;
    rsp.geometry_mode |= set;
}

static void gfx_calc_and_set_viewport(const Vp_t* viewport) {
    // 2 bits fraction
    float width = viewport->vscale[0] * 0.5f;
    float height = viewport->vscale[1] * 0.5f;
    float x = (viewport->vtrans[0] * 0.25f) - width * 0.5f;
    float y = SCREEN_HEIGHT - ((viewport->vtrans[1] * 0.25f) + height * 0.5f);

    width *= RATIO_X;
    height *= RATIO_Y;
    x *= RATIO_X;
    y *= RATIO_Y;

    rdp.viewport.x = x;
    rdp.viewport.y = y;
    rdp.viewport.width = width;
    rdp.viewport.height = height;

    // Keep the un-truncated float rect for the pane-clip math (rdp.viewport is uint16: negative
    // transition coords wrap to ~65000). The raw-PVR backend has no viewport transform, so the
    // front-end screen map + scissor NDC planes refresh right here - NOT in the deferred state
    // flush, whose rdp.viewport also gets temporarily swapped by the texrect path.
    vpf_x = x;
    vpf_y = y;
    vpf_w = width;
    vpf_h = height;
    gfx_recompute_screen_map();
    gfx_recompute_scissor_planes();

    rdp.viewport_or_scissor_changed = 1;
}

static void gfx_update_light(uint8_t index, const void* data) {
    if (memcmp(rsp.current_lights + ((index - G_MV_L0) >> 1), data, sizeof(Light_t))) {
        // NOTE: reads out of bounds if it is an ambient light
        n64_memcpy(rsp.current_lights + ((index - G_MV_L0) >> 1), data, sizeof(Light_t));
        rsp.lights_changed = 1;
    }
}

static void gfx_sp_movemem(uint8_t index, const void* data) {
    switch (index) {
        case G_MV_VIEWPORT:
            gfx_calc_and_set_viewport((const Vp_t*) data);
            break;
        case G_MV_L0:
        case G_MV_L1:
        case G_MV_L2:
        case G_MV_L3:
        case G_MV_L4:
        case G_MV_L5:
        case G_MV_L6:
        case G_MV_L7:
            gfx_update_light(index, data);
            break;
        default:
            break;
    }
}

static void gfx_sp_moveword(uint8_t index, uint32_t data) {
    switch (index) {
        case G_MW_NUMLIGHT:
            // Ambient light is included
            // The 31th bit is a flag that lights should be recalculated
            if (rsp.current_num_lights != ((data - 0x80000000U) >> 5)) {
                rsp.current_num_lights = ((data - 0x80000000U) >> 5);
                rsp.lights_changed = 1;
            }
            break;
        case G_MW_FOG:
            int16_t fog_mul = (int16_t) (data >> 16);
            int16_t fog_ofs = (int16_t) (data & 0xFFFF);
            pvr_fog_mul = fog_mul;
            pvr_fog_ofs = fog_ofs;
            break;
        default:
            break;
    }
}

static void gfx_sp_texture(uint16_t sc, uint16_t tc) {
    rsp.texture_scaling_factor.s = sc * 0.03125f;
    rsp.texture_scaling_factor.t = tc * 0.03125f;
}

static void gfx_dp_set_scissor(uint32_t ulx, uint32_t uly, uint32_t lrx, uint32_t lry) {
    float x = ulx * 0.25f * RATIO_X;
    float y = (SCREEN_HEIGHT - lry * 0.25f) * RATIO_Y;
    float width = (lrx - ulx) * 0.25f * RATIO_X;
    float height = (lry - uly) * 0.25f * RATIO_Y;

    rdp.scissor.x = x;
    rdp.scissor.y = y;
    rdp.scissor.width = width;
    rdp.scissor.height = height;

    // Scissoring is done in software (homogeneous clip in gfx_sp_tri1) - the PVR userclip can't
    // track per-pane rects. Keep the float rect and recompute the NDC bounds vs the viewport.
    scf_x = x;
    scf_y = y;
    scf_w = width;
    scf_h = height;
    gfx_recompute_scissor_planes();

    rdp.viewport_or_scissor_changed = 1;
}

static void gfx_dp_set_texture_image(uint8_t size, uint32_t width, const void* addr) {
    rdp.texture_to_load.addr = SEGMENTED_TO_VIRTUAL((void*) addr);
    rdp.texture_to_load.siz = size;
    last_set_texture_image_width = width;
}

// 3 bits, 2 bits, 9 bits, 9 bits, 3 bits, 4 bits, 2 bits, 4 bits, 4 bits, 2 bits, 4 bits, 4 bits
typedef struct set_tile_s {
    uint8_t fmt;
    uint8_t siz;
    uint16_t line;
    uint16_t tmem;
    uint8_t tile;
    uint8_t palette;
    uint8_t cmt;
    uint8_t maskt;
    uint8_t shiftt;
    uint8_t cms;
    uint8_t masks;
    uint8_t shifts;
    uint16_t pad;
} set_tile_t;

static void gfx_dp_set_tile_size(uint16_t uls, uint16_t ult, uint16_t lrs, uint16_t lrt) {
    // moved check to `gfx_run_dl`
    // if (tile == G_TX_RENDERTILE)
    rdp.texture_tile.uls = uls;
    rdp.texture_tile.ult = ult;
    rdp.texture_tile.lrs = lrs;
    rdp.texture_tile.lrt = lrt;
    rdp.textures_changed[0] = 1;
    rdp.textures_changed[1] = 1;
}

extern u16 gTextCharPalettes[4][16];

static void __attribute__((noinline)) DO_LOAD_TLUT(void) {
    if (rdp.loaded_palette == rdp.palette) {
        rdp.palette_dirty = 0;
        return;
    }

    int font = 0;

    int high_index = rdp.palette_dirty;

    if (SEGMENTED_TO_VIRTUAL(rdp.palette) == SEGMENTED_TO_VIRTUAL(gTextCharPalettes[0])) {
        font = 1;
    } else if (SEGMENTED_TO_VIRTUAL(rdp.palette) == SEGMENTED_TO_VIRTUAL(gTextCharPalettes[1])) {
        font = 1;
    } else if (SEGMENTED_TO_VIRTUAL(rdp.palette) == SEGMENTED_TO_VIRTUAL(gTextCharPalettes[2])) {
        font = 1;
    } else if (SEGMENTED_TO_VIRTUAL(rdp.palette) == SEGMENTED_TO_VIRTUAL(gTextCharPalettes[3])) {
        font = 1;
    }

    memset(tlut, 0, 256 * 2);

    uint16_t* srcp = (uint16_t*) SEGMENTED_TO_VIRTUAL(rdp.palette);
    rdp.loaded_palette = rdp.palette;

    uint16_t* tlp = tlut;
    int start = 0, end = high_index;

    if (font) {
        start = 64;
        end = 256;
    }

    for (int i = start; i < end; i++) {
        uint16_t c1 = *srcp++;
        if (font) {
            if (c1 & 1)
                c1 = 0xffff;
            else
                c1 = 0;
        } else {
            if (end < 255) {
            c1 = (c1 << 15) | ((c1 >> 1) & 0x7FFF);
            } else {
            c1 = brightit_argb1555((c1 << 15) | ((c1 >> 1) & 0x7FFF));
            }
        }
        *tlp++ = c1;
    }

    rdp.palette_dirty = 0;
}

static void __attribute__((noinline)) gfx_dp_load_tlut(uint32_t high_index) {
    rdp.palette = (void*) ((uintptr_t) (void*) rdp.texture_to_load.addr);
    rdp.palette_dirty = high_index;
}

static void gfx_dp_load_block(uint32_t lrs) {
    // The lrs field rather seems to be number of pixels to load
    uint32_t word_size_shift = 0;
    switch (rdp.texture_to_load.siz) {
        case G_IM_SIZ_4b:
            word_size_shift = 0;
            break;
        case G_IM_SIZ_8b:
            word_size_shift = 0;
            break;
        case G_IM_SIZ_16b:
            word_size_shift = 1;
            break;
        case G_IM_SIZ_32b:
            word_size_shift = 2;
            break;
    }
    uint32_t size_bytes = (lrs + 1) << word_size_shift;
    rdp.loaded_texture[rdp.texture_to_load.tile_number].size_bytes = size_bytes;
    rdp.loaded_texture[rdp.texture_to_load.tile_number].addr = SEGMENTED_TO_VIRTUAL(rdp.texture_to_load.addr);

    rdp.textures_changed[rdp.texture_to_load.tile_number] = 1;
    last_set_texture_image_width = 0;
}

static void gfx_dp_load_tile(uint32_t uls, uint32_t ult, uint32_t lrs, uint32_t lrt) {
    uint32_t word_size_shift = 0;
    switch (rdp.texture_to_load.siz) {
        case G_IM_SIZ_4b:
            word_size_shift = 0;
            break;
        case G_IM_SIZ_8b:
            word_size_shift = 0;
            break;
        case G_IM_SIZ_16b:
            word_size_shift = 1;
            break;
        case G_IM_SIZ_32b:
            word_size_shift = 2;
            break;
    }

    uint32_t size_bytes = ((((lrs - uls) >> G_TEXTURE_IMAGE_FRAC) + 1) * (((lrt - ult) >> G_TEXTURE_IMAGE_FRAC) + 1))
                          << word_size_shift;
    rdp.loaded_texture[rdp.texture_to_load.tile_number].size_bytes = size_bytes;

    rdp.loaded_texture[rdp.texture_to_load.tile_number].addr = rdp.texture_to_load.addr;
    rdp.texture_tile.uls = uls;
    rdp.texture_tile.ult = ult;
    rdp.texture_tile.lrs = lrs;
    rdp.texture_tile.lrt = lrt;

    rdp.textures_changed[rdp.texture_to_load.tile_number] = 1;
}

static uint8_t color_comb_component(uint32_t v) {
    switch (v) {
        case G_CCMUX_TEXEL0:
            return CC_TEXEL0;
        case G_CCMUX_TEXEL1:
            return CC_TEXEL1;
        case G_CCMUX_PRIMITIVE:
            return CC_PRIM;
        case G_CCMUX_SHADE:
            return CC_SHADE;
        case G_CCMUX_ENVIRONMENT:
            return CC_ENV;
        case G_CCMUX_TEXEL0_ALPHA:
            return CC_TEXEL0A;
        case G_CCMUX_LOD_FRACTION:
            return CC_LOD;
        default:
            return CC_0;
    }
}

static inline uint32_t color_comb(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    return color_comb_component(a) | (color_comb_component(b) << 3) | (color_comb_component(c) << 6) |
           (color_comb_component(d) << 9);
}

static void gfx_dp_set_combine_mode(uint32_t rgb, uint32_t alpha) {
    rdp.combine_mode = rgb | (alpha << 12);
}

static void gfx_dp_set_env_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    rdp.env_color.r = table256[r];//table32[r >> 3] << 3;
    rdp.env_color.g = table256[g];//table32[g >> 3] << 3;
    rdp.env_color.b = table256[b];//table32[b >> 3] << 3;
    rdp.env_color.a = a;
}

static void gfx_dp_set_prim_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    rdp.prim_color.r = table256[r];//table32[r >> 3] << 3;
    rdp.prim_color.g = table256[g];//table32[g >> 3] << 3;
    rdp.prim_color.b = table256[b];//table32[b >> 3] << 3;
    pa = rdp.prim_color.a = a;
}

typedef enum LevelType {
    /* 0 */ LEVELTYPE_PLANET,
    /* 1 */ LEVELTYPE_SPACE,
} LevelType;

extern uint8_t gLevelType;
extern s32 sCutsceneState;
static void gfx_dp_set_fog_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (gCurrentLevel == LEVEL_ZONESS) {
        r = 13;
        g = 40;
        b = 41;
    } else if (gCurrentLevel == LEVEL_CORNERIA) {
        r = 77;
        g = 90;
        b = 113;
    } else if (gCurrentLevel == LEVEL_FORTUNA) {
        r = 213;
        g = 206;
        b = 176;
    } else if (gCurrentLevel == LEVEL_MACBETH) {
        r = 100;
        g = 100;
        b = 120;
    } else if (gCurrentLevel == LEVEL_KATINA) {
        r = 97;
        g = 90;
        b = 90;
    } else if (gCurrentLevel == LEVEL_TITANIA) {
        r = 173;
        g = 74;
        b = 0;
    } else {
        r = table256[r];
        g = table256[g];
        b = table256[b];
        //r = table32[r >> 3] << 3;
        //g = table32[g >> 3] << 3;
        //b = table32[b >> 3] << 3;
    }

    if (sCutsceneState == 2) {
        r = 0;
        g = 0;
        b = 0;
    }

    rdp.fog_color.r = r;
    rdp.fog_color.g = g;
    rdp.fog_color.b = b;
    // Perceptual black/colour fog GAMMA (see gfx_calc_fog): pick the LUT by final fog colour; rebuild
    // both LUTs only when a gamma constant was edited.
    if (sLutGDark != gFogGammaDark || sLutGColor != gFogColorGain) gfx_fog_build_luts();
    sFogLut = (r | g | b) ? sFogLutColor : sFogLutDark;
    if ((!rendering_state.fog_col_change)) {
        rendering_state.fog_col_change = 1;
        // PVR vertex-fog colour is ONE global per frame (first wins).
        gfx_pvr_set_fog_color(rdp.fog_color.r, rdp.fog_color.g, rdp.fog_color.b, 255);
    }
}

static void gfx_dp_set_fill_color(uint32_t packed_color) {
    uint16_t col16 = (uint16_t) packed_color;
    uint32_t r = (col16 >> 11) & 0x1f;
    uint32_t g = (col16 >> 6) & 0x1f;
    uint32_t b = (col16 >> 1) & 0x1f;
    uint32_t a = col16 & 1;

    // table32 = the port's N64 VI-gamma emulation (sqrt-ish); applies to fills too. Measured against
    // an N64 capture: star pixels ~64-100 where the raw gStarColors palette is 16-40 -> the boost is
    // correct here, keep it (a linear 5->8 was tried 2026-08-16 and is WRONG: stars far too dark).
    rdp.fill_color.r = table32[r] << 3;
    rdp.fill_color.g = table32[g] << 3;
    rdp.fill_color.b = table32[b] << 3;
    rdp.fill_color.a = a * 255;
}

static void __attribute__((noinline)) gfx_draw_rectangle_impl(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry);
static void gfx_draw_rectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry) {
#if GFX_PROF
    uint64_t t0 = PROF_NOW(); gfx_draw_rectangle_impl(ulx, uly, lrx, lry); prof_t_quad += PROF_NOW() - t0; prof_quads++;
#else
    gfx_draw_rectangle_impl(ulx, uly, lrx, lry);
#endif
}
static void __attribute__((noinline)) gfx_draw_rectangle_impl(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry) {
    uint32_t saved_other_mode_h = rdp.other_mode_h;
    uint32_t cycle_type = (rdp.other_mode_h & (3U << G_MDSFT_CYCLETYPE));

    if (cycle_type == G_CYC_COPY) {
        rdp.other_mode_h = (rdp.other_mode_h & ~(3U << G_MDSFT_TEXTFILT)) | G_TF_POINT;
    }

    // U10.2 coordinates
    float ulxf = ulx;
    float ulyf = uly;
    float lrxf = lrx;
    float lryf = lry;

    if (do_the_blur) {
        ulxf = -1.0f;
        ulyf = -1.0f;
        lrxf = 0.99375f;
        lryf = 0.99166348f;
    } else {
        ulxf = (ulxf * recip_4timeshalfscrwid) - 1.0f;
        ulyf = (ulyf * recip_4timeshalfscrhgt) - 1.0f;
        lrxf = (lrxf * recip_4timeshalfscrwid) - 1.0f;
        lryf = (lryf * recip_4timeshalfscrhgt) - 1.0f;
    }
    // NDC -> framebuffer pixels (fb_half_* = gfx_current_dimensions/2, so this tracks LOWRES).
    ulxf = (ulxf * fb_half_w) + fb_half_w;
    lrxf = (lrxf * fb_half_w) + fb_half_w;

    ulyf = (ulyf * fb_half_h) + fb_half_h;
    lryf = (lryf * fb_half_h) + fb_half_h;

    // FILL rects (starfield pixels): an integer-aligned square's ll->ur diagonal passes EXACTLY
    // through the pixel centre(s) (1x1 @320: the one centre; 2x2 @640: two of the four), so which of
    // the two tiny triangles owns the sample is a hardware tie-break -> star pixels flicker/turn into
    // 3-pixel triangles. Nudge the rect by a sub-pixel amount so it still covers exactly the same
    // pixel centres (>=0.05 inside / neighbours >=0.05 outside) but no centre lies on an edge or the
    // diagonal (>=0.5 clearance). Not needed for texrects/fades (large, ties resolve consistently).
    if (do_ext_fill && !do_the_blur) {
        ulxf += 0.05f; ulyf += 0.05f;
        lrxf += 0.45f; lryf += 0.45f;
    }

    pvr_vertex_t* ul = &rsp.loaded_vertices_2D[0];
    pvr_vertex_t* ll = &rsp.loaded_vertices_2D[1];
    // PVR strip order (ul, ll, ur, lr): a 4-vert strip tessellates as (ul,ll,ur)+(ll,ur,lr),
    // diagonal ll-ur.
    pvr_vertex_t* ur = &rsp.loaded_vertices_2D[2];
    pvr_vertex_t* lr = &rsp.loaded_vertices_2D[3];

    screen_2d_z += 1.0f;
    // A 2D rect drawn BEFORE any 3D in a perspective frame is a BACKDROP (e.g. the starfield) ->
    // pin it to the far plane so the 3D scene resolves in front of it. Anything else is an overlay
    // at the running 2D paint-order depth. (PVR z is 1/w, larger == nearer.)
    // Far-pin BACKDROP fills only - the untextured screen-clear fill + starfield (do_ext_fill==1) -
    // when drawn before any 3D in a perspective frame, so the 3D scene resolves in front of them.
    // Textured 2D (labels/HUD via texrect, do_ext_fill==0) is an OVERLAY: keep it at paint-order z
    // even when drawn pre-3D, so it isn't pinned to the far plane and depth-rejected into a black wedge.
    // (Dropping do_ext_fill here to far-pin the ending back-layer texrects far-pinned ALL textured 2D
    // backdrops -> autosort-TR wedge on full-screen 2D. Reverted; the ending needs a tighter fix.)
    // ...and ONLY if no perspective triangle has been drawn yet this frame. A fill AFTER perspective
    // geometry is an overlay even when nothing was depth-tested (Sector Y outro: Z-off nebula backdrop
    // + fade-to-black fill -> has_done_3d stayed 0, the fade far-pinned to 0.00001 and landed BETWEEN
    // the nebula's two far-slab-staggered triangles -> diagonal half-dimmed billboard).
    // The screen clear (1st fill) sits at PVR_Z_FARPIN; every later pre-scene fill (the starfield pixels)
    // at PVR_Z_FARPIN_FILL2. Both are OP + depth-write, and the OP list is depth-resolved per tile, NOT
    // draw-ordered: an EXACT z tie between the black clear and a star pixel is decided arbitrarily per
    // frame -> star brightness flicker. A distinct, nearer z makes the star win deterministically.
    float rz = screen_2d_z;
    if (do_ext_fill && prev_frame_had_persp && !has_done_3d && !has_drawn_persp_tri) {
        rz = (far_fill_count++ == 0) ? PVR_Z_FARPIN : PVR_Z_FARPIN_FILL2;
    }

    ul->x = ulxf;
    ul->y = ulyf;
    ul->z = rz;

    ll->x = ulxf;
    ll->y = lryf;
    ll->z = rz;

    lr->x = lrxf;
    lr->y = lryf;
    lr->z = rz;

    ur->x = lrxf;
    ur->y = ulyf;
    ur->z = rz;

    // The coordinates for texture rectangle shall bypass the viewport setting
    struct XYWidthHeight default_viewport = { 0, 0, gfx_current_dimensions.width, gfx_current_dimensions.height };
    struct XYWidthHeight viewport_saved = rdp.viewport;
    uint32_t geometry_mode_saved = rsp.geometry_mode;

    rdp.viewport = default_viewport;
    rdp.viewport_or_scissor_changed = 1;
    rsp.geometry_mode = 0;

    gfx_sp_quad_2d(0, 1, 3, 1, 2, 3);

    rsp.geometry_mode = geometry_mode_saved;
    rdp.viewport = viewport_saved;
    rdp.viewport_or_scissor_changed = 1;

    if (cycle_type == G_CYC_COPY) {
        rdp.other_mode_h = saved_other_mode_h;
    }
}
#define C0(pos, width) ((cmd->words.w0 >> (pos)) & ((1U << width) - 1))
#define C1(pos, width) ((cmd->words.w1 >> (pos)) & ((1U << width) - 1))
#define G_QUAD (G_IMMFIRST - 10)

static void
    __attribute__((noinline)) gfx_dp_texture_rectangle2(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry, int16_t uls,
                                                        int16_t ult, int16_t dsdx, int16_t dtdy, uint8_t flip) {
    uint32_t saved_combine_mode = rdp.combine_mode;
    if ((rdp.other_mode_h & (3U << G_MDSFT_CYCLETYPE)) == G_CYC_COPY) {
        // Per RDP Command Summary Set Tile's shift s and this dsdx should be set to 4 texels
        // Divide by 4 to get 1 instead
        dsdx >>= 2;

        // Color combiner is turned off in copy mode
        gfx_dp_set_combine_mode(color_comb(0, 0, 0, G_CCMUX_TEXEL0), color_comb(0, 0, 0, G_ACMUX_TEXEL0));

        // Per documentation one extra pixel is added in this modes to each edge
        lrx += 1 << 2;
        lry += 1 << 2;
    }

    // uls and ult are S10.5
    // dsdx and dtdy are S5.10
    // lrx, lry, ulx, uly are U10.2
    // lrs, lrt are S10.5
    if (flip) {
        dsdx = -dsdx;
        dtdy = -dtdy;
    }
    int16_t width = !flip ? lrx - ulx : lry - uly;
    int16_t height = !flip ? lry - uly : lrx - ulx;

    float lrs = ((uls << 7) + dsdx * width) >> 7;
    float lrt = ((ult << 7) + dtdy * height) >> 7;

    pvr_vertex_t* ul = &rsp.loaded_vertices_2D[0];
    pvr_vertex_t* ll = &rsp.loaded_vertices_2D[1];
    pvr_vertex_t* ur = &rsp.loaded_vertices_2D[2];   // PVR strip order (ul, ll, ur, lr)
    pvr_vertex_t* lr = &rsp.loaded_vertices_2D[3];

    ul->u = !flip ? uls : lrs;
    ul->v = !flip ? ult : lrt;
    lr->u = !flip ? lrs : uls;
    lr->v = !flip ? lrt : ult;

    ll->u = !flip ? uls : lrs;
    ll->v = !flip ? lrt : ult;
    ur->u = !flip ? lrs : uls;
    ur->v = !flip ? ult : lrt;

    gfx_draw_rectangle(ulx, uly, lrx, lry);
    rdp.combine_mode = saved_combine_mode;
}

static void __attribute__((noinline)) gfx_dp_texture_rectangle(Gfx* cmd, uint8_t flip) {
    int32_t lrx, lry, ulx, uly;
    int16_t uls, ult, dsdx, dtdy;
    lrx = C0(12, 12);
    lry = C0(0, 12);
    ulx = C1(12, 12);
    uly = C1(0, 12);
    ++cmd;
    uls = C1(16, 16);
    ult = C1(0, 16);
    ++cmd;
    dsdx = C1(16, 16);
    dtdy = C1(0, 16);
    gfx_dp_texture_rectangle2(ulx, uly, lrx, lry, uls, ult, dsdx, dtdy, flip);
}

// Section order = symbol CREATION order (first declaration), not definition order: declare gfx_run_dl
// first so the fill-rect functions follow it inside .text.hot.gfx (see the GFX_HOT note at the top).
static void __attribute__((noinline)) GFX_HOT gfx_run_dl(Gfx* cmd);
static void __attribute__((noinline)) GFX_HOT gfx_dp_fill_rectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry);   // defined after gfx_run_dl

static void gfx_dp_set_z_image(void* z_buf_address) {
    rdp.z_buf_address = z_buf_address;
}

static void gfx_dp_set_color_image(void* address) {
    rdp.color_image_address = address;
}

static void gfx_sp_set_other_mode(uint8_t shift, uint8_t num_bits, uint64_t mode) {
    uint64_t mask = (((uint64_t) 1 << num_bits) - 1) << shift;
    uint64_t om = rdp.other_mode_l | ((uint64_t) rdp.other_mode_h << 32);
    om = (om & ~mask) | mode;
    rdp.other_mode_l = (uint32_t) om;
    rdp.other_mode_h = (uint32_t) (om >> 32);
}

static inline void* seg_addr(uintptr_t w1) {
    return (void*) SEGMENTED_TO_VIRTUAL((void*) w1);
}

#define C0alt(pos, width) ((w0 >> (pos)) & ((1U << width) - 1))
#define C1alt(pos, width) ((w1 >> (pos)) & ((1U << width) - 1))

static void gfx_dp_set_tile2(uint32_t w0, uint32_t w1) {
    set_tile_t settile;
    set_tile_t* stile = &settile;

    settile.fmt = C0alt(21, 3);
    settile.siz = C0alt(19, 2);
    settile.line = C0alt(9, 9);
    settile.tmem = C0alt(0, 9);
    settile.tile = C1alt(24, 3);
    settile.palette = C1alt(20, 4);
    settile.cmt = C1alt(18, 2);
    settile.maskt = C1alt(14, 4);
    settile.shiftt = C1alt(10, 4);
    settile.cms = C1alt(8, 2);
    settile.masks = C1alt(4, 4);
    settile.shifts = C1alt(0, 4);

    if (stile->tile == G_TX_RENDERTILE) {
        rdp.texture_tile.fmt = stile->fmt;
        rdp.texture_tile.siz = stile->siz;

        if (stile->cms == G_TX_WRAP && stile->masks == G_TX_NOMASK) {
            stile->cms = G_TX_CLAMP;
        }
        if (stile->cmt == G_TX_WRAP && stile->maskt == G_TX_NOMASK) {
            stile->cmt = G_TX_CLAMP;
        }

        rdp.texture_tile.masks = stile->masks;
        rdp.texture_tile.maskt = stile->maskt;
        rdp.texture_tile.cms = stile->cms;
        rdp.texture_tile.cmt = stile->cmt;
        rdp.texture_tile.line_size_bytes = stile->line << 3;
        rdp.texture_tile.shifts = stile->shifts;
        rdp.texture_tile.shiftt = stile->shiftt;
        rdp.textures_changed[0] = 1;
        rdp.textures_changed[1] = 1;
    }

    if (stile->tile == G_TX_LOADTILE) {
        rdp.texture_to_load.tile_number = stile->tmem >> 8;
    } else {
        rdp.texture_to_load.tile_number = stile->tile;
    }
    rdp.texture_to_load.tmem = stile->tmem;
    rdp.last_palette = stile->palette;
}

#define GFX_DL_STACK_MAX 4 /* tune this to whatever nesting you expect */

static Gfx __attribute__((aligned(32))) * dl_stack[GFX_DL_STACK_MAX];

static void __attribute__((noinline)) GFX_HOT gfx_run_dl(Gfx* cmd) {
    int dl_sp = 0;

    cmd = seg_addr((uintptr_t) cmd);

    __builtin_prefetch(cmd);

    for (;;) {
        uint32_t opcode = cmd->words.w0 >> 24;

        // Custom "BLND" opcode (gSPTheBlur, w0 == 0x424C4E44): an explicit escape-hatch flag the
        // passive classifier can't infer. Currently only the framebuffer-capture blur overlay
        // (w1 == 0x46554360 -> do_the_blur toggle). The reticle/starfield/zfight/etc. sub-flags that
        // main used here were retired in favour of passive OP/PT/TR + overlay classification. Handle
        // and skip the normal opcode switch.
        if (cmd->words.w0 == 0x424C4E44) {
            rdp_state_gen++;
            if (cmd->words.w1 == 0x46554360) do_the_blur ^= 1;
            else if (cmd->words.w1 == 0x4F564C59) force_paint_overlay ^= 1;   // 'OVLY'
            cmd++;
            continue;
        }

        // Any opcode that is not a triangle/vertex/matrix/DL-flow op may change RDP/RSP state the
        // per-triangle setup derives from -> bump the generation (gfx_tri_state_setup re-runs lazily).
        switch (opcode) {
            // (uint8_t) casts are REQUIRED: the F3DEX immediate opcodes are G_IMMFIRST-relative and
            // NEGATIVE as ints (G_TRI2 = -79 -> 0xb1); without the cast they never match `opcode`.
            case (uint8_t) G_TRI1: case (uint8_t) G_TRI2: case (uint8_t) G_VTX: case (uint8_t) G_MTX:
            case (uint8_t) G_POPMTX: case (uint8_t) G_DL: case (uint8_t) G_ENDDL:
            // 2D rect ops + fill colour + texrect payload words: per-quad geometry/colour, not derived
            // state (the 2D setup keys separately on the mode words; a 3D<->2D switch re-derives both).
            case (uint8_t) G_FILLRECT: case (uint8_t) G_TEXRECT: case (uint8_t) G_TEXRECTFLIP:
            case (uint8_t) G_SETFILLCOLOR: case (uint8_t) G_RDPHALF_1: case (uint8_t) G_RDPHALF_2:
                break;
            default:
                rdp_state_gen++;
#if GFX_PROF
                prof_op_bumps[opcode & 0xFF]++;
#endif
                break;
        }
        switch (opcode) {
            case G_MTX:
                gfx_sp_matrix(C0(16, 8), (const void*) seg_addr(cmd->words.w1));
                break;

            case (uint8_t) G_POPMTX:
                gfx_sp_pop_matrix();
                break;

            case G_MOVEMEM:
                gfx_sp_movemem(C0(16, 8), seg_addr(cmd->words.w1));
                break;

            case (uint8_t) G_MOVEWORD:
                if (C0(0, 8) >= 2)
                    gfx_sp_moveword(C0(0, 8), cmd->words.w1);
                break;

            case (uint8_t) G_TEXTURE:
                gfx_sp_texture(C1(16, 16), C1(0, 16));
                break;

            case G_VTX:
                gfx_sp_vertex(C0(10, 6), C0(17, 7), seg_addr(cmd->words.w1));
                break;

            case G_DL:
                if (C0(16, 1) == 0) {
                    /* CALL-style: push return address, jump to new list */
                    // if (dl_sp >= GFX_DL_STACK_MAX) {
                    /* stack overflow – bail out or handle as needed */
                    // return;
                    //}

                    dl_stack[dl_sp++] = cmd + 1; /* return to next command */

                    cmd = (Gfx*) seg_addr(cmd->words.w1);

                    __builtin_prefetch(cmd);
                    --cmd; /* ++cmd at loop bottom will land on first cmd in new DL */
                } else {
                    /* JUMP-style: tail-call, no stack push */
                    __builtin_prefetch((Gfx*) seg_addr(cmd->words.w1));
                    cmd = (Gfx*) seg_addr(cmd->words.w1);
                    --cmd; /* ++cmd at loop bottom will land on first cmd in new DL */
                }
                break;

            case (uint8_t) G_ENDDL: {
                // A display list finished -> any 3D drawn inside it is now fully emitted,
                // so promote the deferred has_done_3d. Doing it here (not per-triangle)
                // keeps a backdrop DL's triangles classified against one consistent value.
                if (has_done_3d_pending) { has_done_3d = 1; has_done_3d_pending = 0; }
                if (dl_sp == 0) {
                    /* top-level ENDDL: we're done */
                    return;
                } else {
                    /* pop return address and resume caller DL */
                    cmd = dl_stack[--dl_sp];
                    --cmd; /* ++cmd at loop bottom -> first command after the call */
                }
                break;
            }

            case (uint8_t) G_SETGEOMETRYMODE:
                gfx_sp_geometry_mode(0, cmd->words.w1);
                break;

            case (uint8_t) G_CLEARGEOMETRYMODE:
                gfx_sp_geometry_mode(cmd->words.w1, 0);
                break;

            case (uint8_t) G_TRI1:
                gfx_sp_tri1(C1(17, 7), C1(9, 7), C1(1, 7));
                break;

            case (uint8_t) G_TRI2:
                gfx_sp_tri1(C0(17, 7), C0(9, 7), C0(1, 7));
                gfx_sp_tri1(C1(17, 7), C1(9, 7), C1(1, 7));
                break;

            case (uint8_t) G_SETOTHERMODE_L:
                gfx_sp_set_other_mode(C0(8, 8), C0(0, 8), cmd->words.w1);
                break;

            case (uint8_t) G_SETOTHERMODE_H:
                gfx_sp_set_other_mode(C0(8, 8) + 32, C0(0, 8), (uint64_t) cmd->words.w1 << 32);
                break;

            // RDP Commands:
            case G_SETTIMG:
                gfx_dp_set_texture_image(C0(19, 2), C0(0, 10), cmd->words.w1);
                break;

            case G_LOADBLOCK:
                gfx_dp_load_block(C1(12, 12));
                break;

            case G_LOADTILE:
                gfx_dp_load_tile(C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
                break;

            case G_SETTILE:
                gfx_dp_set_tile2(cmd->words.w0, cmd->words.w1);
                break;

            case G_SETTILESIZE:
                if (C1(24, 3) == G_TX_RENDERTILE)
                    gfx_dp_set_tile_size(C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
                break;

            case G_LOADTLUT:
                gfx_dp_load_tlut(C1(14, 10));
                break;

            case G_SETENVCOLOR:
                gfx_dp_set_env_color(C1(24, 8), C1(16, 8), C1(8, 8), C1(0, 8));
                break;

            case G_SETPRIMCOLOR:
                gfx_dp_set_prim_color(C1(24, 8), C1(16, 8), C1(8, 8), C1(0, 8));
                break;

            case G_SETFOGCOLOR:
                gfx_dp_set_fog_color(C1(24, 8), C1(16, 8), C1(8, 8), C1(0, 8));
                break;

            case G_SETFILLCOLOR:
                gfx_dp_set_fill_color(cmd->words.w1);
                break;

            case G_SETCOMBINE:
                gfx_dp_set_combine_mode(color_comb(C0(20, 4), C1(28, 4), C0(15, 5), C1(15, 3)),
                                        color_comb(C0(12, 3), C1(12, 3), C0(9, 3), C1(9, 3)));
                // Keep the RAW mux words for the PVR combiner evaluator (compact combine_mode loses them).
                rdp.combine_w0 = cmd->words.w0;
                rdp.combine_w1 = cmd->words.w1;
                break;

            case G_TEXRECT:
            case G_TEXRECTFLIP: {
                Gfx* texrectCmd = cmd;
                ++cmd;
                ++cmd;
                gfx_dp_texture_rectangle(texrectCmd, opcode == G_TEXRECTFLIP);
                break;
            }

            case G_FILLRECT:
                gfx_dp_fill_rectangle(C1(12, 12), C1(0, 12), C0(12, 12), C0(0, 12));
                break;

            case G_SETSCISSOR:
                gfx_dp_set_scissor(C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
                break;

            case G_SETZIMG:
                gfx_dp_set_z_image(seg_addr(cmd->words.w1));
                break;

            case G_SETCIMG:
                gfx_dp_set_color_image(seg_addr(cmd->words.w1));
                break;
        }

        __builtin_prefetch((void*) (++cmd) + 32);
    }
}

// (Defined AFTER gfx_run_dl on purpose: .text.hot.gfx keeps source order, and the 3D core
// {gfx_sp_vertex_impl, gfx_sp_tri1_impl, gfx_run_dl} must be a contiguous < 8KB run; the fill path
// only executes in the pre-3D star phase, so it may alias the head of that run harmlessly.)
// ---- FILL-cycle fill-rect fast path (the starfield: up to ~1000 1-pixel fills per frame) --------
// The generic route (gfx_dp_fill_rectangle -> gfx_draw_rectangle -> gfx_sp_quad_2d -> 4x combiner
// eval -> draw_triangles_2d) cost ~8us per star (PROF 2026-08-16). Once the hoisted 2D setup is
// valid for the FILL state (which the FIRST fill of a run establishes through the generic route),
// every further FILL rect under the same state is pure per-quad work: 4 positions, one cached
// colour, z, and one OP strip submit. Everything below mirrors the generic route exactly:
//   - gfx_dp_fill_rectangle: lrx/lry += 1<<2, geometry_mode & ~G_ZBUFFER, other_mode_l & ~Z_UPD
//   - gfx_draw_rectangle:    NDC -> pixels (== u10.2 * recip*fb_half), the [+0.05,+0.45] nudge,
//                            screen_2d_z++, far-pin (PVR_Z_FARPIN / _FILL2 via far_fill_count),
//                            rsp.geometry_mode = 0 for the quad key
//   - gfx_sp_quad_2d:        setup validity key {0, other_mode_l&~Z_UPD, other_mode_h, ext_fill=1},
//                            combiner eval of the (constant) fill colour, kind = qs.kind (FILL -> OP),
//                            OP strip submit (ul, ll, ur, lr; EOL on lr)
// The far-pin BACKDROP->OP special case in gfx_sp_quad_2d only concerns kind==2 quads; a FILL rect
// is kind 0 by construction (qs.kind), so it cannot apply here.
static pvr_vertex_t __attribute__((aligned(32))) fill_fast_q[4];
// Evaluated-colour cache keyed by (fill colour, prepared-state stamp): the starfield cycles through a
// small palette (gStarColors) with the colour changing almost every star, so a single last-value
// cache re-evaluated per star. 16 direct-mapped slots cover the palette.
#define FILL_FAST_CC 16
static struct { uint32_t src, stamp, argb, oargb; } fill_fast_cc[FILL_FAST_CC];
// Colour (re)evaluation for the fill fast path, OUT OF LINE: the inlined evaluator made the fast
// path 1.4KB of rarely-run code sitting inside the hot I-cache set. Runs only on palette/state change.
// (GFX_HOT: trails the 3D core in the hot block with the other fill-rect functions - it aliased
// them from outside the block and ping-ponged per star.)
static void __attribute__((noinline)) GFX_HOT fill_fast_recolor(uint32_t fc, unsigned slot) {
    struct RGBA shade = { rdp.fill_color.r, rdp.fill_color.g, rdp.fill_color.b, rdp.fill_color.a };
    pvr_eval_combiner_fast(&shade, &fill_fast_cc[slot].argb, &fill_fast_cc[slot].oargb);
    fill_fast_cc[slot].src = fc; fill_fast_cc[slot].stamp = cc_state_stamp;
}
static int __attribute__((noinline)) GFX_HOT gfx_dp_fill_rectangle_fast(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry) {
    if (!quad_setup_valid || quad_setup_gen != rdp_state_gen || do_the_blur ||
        quad_key[0] != 0 || quad_key[1] != (rdp.other_mode_l & ~(uint32_t) Z_UPD) ||
        quad_key[2] != rdp.other_mode_h || quad_key[3] != 1u || qs.kind != 0)
        return 0;
    PROF_INC(prof_quads_fast);
    tri_setup_gen = 0;   // (parity with gfx_sp_quad_2d: a 2D draw owns rendering_state now)

    // Colour: the fill colour is constant across the quad -> evaluate the combiner ONCE and cache it
    // by (fill colour, prepared-state stamp); the starfield palette has a handful of entries.
    uint32_t fc = PACK_ARGB8888(rdp.fill_color.r, rdp.fill_color.g, rdp.fill_color.b, rdp.fill_color.a);
    unsigned slot = ((fc >> 3) ^ (fc >> 11) ^ (fc >> 19)) & (FILL_FAST_CC - 1);
    if (fill_fast_cc[slot].src != fc || fill_fast_cc[slot].stamp != cc_state_stamp) fill_fast_recolor(fc, slot);
    const uint32_t fill_fast_argb = fill_fast_cc[slot].argb, fill_fast_oargb = fill_fast_cc[slot].oargb;

    // Positions (u10.2 -> pixels; the "+1<<2 per edge" of FILL/COPY rects folded in), + the nudge.
    const float sx = recip_4timeshalfscrwid * fb_half_w, sy = recip_4timeshalfscrhgt * fb_half_h;
    float x0 = (float) ulx * sx + 0.05f, y0 = (float) uly * sy + 0.05f;
    float x1 = (float) (lrx + 4) * sx + 0.45f, y1 = (float) (lry + 4) * sy + 0.45f;

    screen_2d_z += 1.0f;
    float rz = screen_2d_z;
    if (prev_frame_had_persp && !has_done_3d && !has_drawn_persp_tri)
        rz = (far_fill_count++ == 0) ? PVR_Z_FARPIN : PVR_Z_FARPIN_FILL2;

    pvr_vertex_t* q = fill_fast_q;
    q[0].flags = q[1].flags = q[2].flags = PVR_CMD_VERTEX; q[3].flags = PVR_CMD_VERTEX_EOL;
    q[0].x = x0; q[0].y = y0;   // ul
    q[1].x = x0; q[1].y = y1;   // ll
    q[2].x = x1; q[2].y = y0;   // ur
    q[3].x = x1; q[3].y = y1;   // lr
    for (int i = 0; i < 4; i++) {
        q[i].z = rz; q[i].u = 0.0f; q[i].v = 0.0f;
        q[i].argb = fill_fast_argb; q[i].oargb = fill_fast_oargb;
    }
    if (pvr_cur_kind != 0) { gfx_pvr_set_blend(0); pvr_cur_kind = 0; }
    PROF_ADD(gfx_pvr_prof_op, 4);
    pvr_submit_op_inline(q, 4);
    return 1;
}

static void __attribute__((noinline)) GFX_HOT gfx_dp_fill_rectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry) {
    int i;

    if (rdp.color_image_address == rdp.z_buf_address) {
        // Don't clear Z buffer here since we already did it with glClear
        return;
    }

    uint32_t mode = (rdp.other_mode_h & (3U << G_MDSFT_CYCLETYPE));

    if (mode == G_CYC_FILL) {
#if GFX_PROF
        uint64_t t0 = PROF_NOW();
        int done = gfx_dp_fill_rectangle_fast(ulx, uly, lrx, lry);
        if (done) { prof_t_quad += PROF_NOW() - t0; prof_quads++; return; }
#else
        if (gfx_dp_fill_rectangle_fast(ulx, uly, lrx, lry)) return;
#endif
    }

    do_ext_fill = 1;

    uint32_t saved_geom_mode = rsp.geometry_mode;
    uint32_t saved_other_mode_l = rdp.other_mode_l;

    if (mode == G_CYC_COPY || mode == G_CYC_FILL) {
        // Per documentation one extra pixel is added in this modes to each edge
        lrx += 1 << 2;
        lry += 1 << 2;
        rsp.geometry_mode &= ~G_ZBUFFER;
        rdp.other_mode_l &= ~Z_UPD;
    }

    for (i = 0; i < 4; i++) {
        rsp.loaded_vertices_2D[i].argb =
            PACK_ARGB8888(rdp.fill_color.r, rdp.fill_color.g, rdp.fill_color.b, rdp.fill_color.a);
    }

    gfx_draw_rectangle(ulx, uly, lrx, lry);
    rsp.geometry_mode = saved_geom_mode;
    rdp.other_mode_l = saved_other_mode_l;

    do_ext_fill = 0;
}


static void gfx_sp_reset() {
    rsp.modelview_matrix_stack_size = 0;
    rendering_state.fog_change = 0;
    rendering_state.fog_col_change = 0;
    alpha_noise = 0;
}

void gfx_get_dimensions(uint32_t* width, uint32_t* height) {
    gfx_wapi->get_dimensions(width, height);
}

void gfx_init(struct GfxWindowManagerAPI* wapi, struct GfxRenderingAPI* rapi, const char* game_name,
              uint8_t start_in_fullscreen) {
    size_t i;
    draw_rect = 0;
    gfx_wapi = wapi;
    gfx_rapi = rapi;
    gfx_wapi->init(game_name, start_in_fullscreen);
    gfx_rapi->init();
    rdp.palette_dirty = 0;

    table256[0] = table32[0] = table16[0] = 0;
    for (i = 1; i < 256; i++) {
        table256[i] = 255.0f * shz_sqrtf_fsrra(((float) i / 255.0f));
    }
    for (i = 1; i < 32; i++) {
        table32[i] = 31.0f * shz_sqrtf_fsrra(((float) i / 31.0f));
    }
    for (i = 1; i < 16; i++) {
        table16[i] = 15.0f * shz_sqrtf_fsrra(((float) i / 15.0f));
    }

    gfx_wapi->get_dimensions(&gfx_current_dimensions.width, &gfx_current_dimensions.height);
    if (gfx_current_dimensions.height == 0) {
        // Avoid division by zero
        gfx_current_dimensions.height = 1;
    }
#if GFX_PROF
    perf_cntr_timer_enable();   // PRFC0 elapsed CPU cycles for the GFXPROF timers
    prof_stall_idx = 0;
    perf_cntr_start(PRFC1, prof_stall_modes[0].mode, PMCR_COUNT_CPU_CYCLES);
#endif
    fb_half_w = (float) gfx_current_dimensions.width * 0.5f;
    fb_half_h = (float) gfx_current_dimensions.height * 0.5f;

    gfx_current_dimensions.aspect_ratio = (float) gfx_current_dimensions.width / (float) gfx_current_dimensions.height;

    rsp.lookat[LOOKAT_Y_IDX].dir[0] = 0;
    rsp.lookat[LOOKAT_Y_IDX].dir[1] = 127;
    rsp.lookat[LOOKAT_Y_IDX].dir[2] = 0;
    rsp.lookat[LOOKAT_X_IDX].dir[0] = 127;
    rsp.lookat[LOOKAT_X_IDX].dir[1] = 0;
    rsp.lookat[LOOKAT_X_IDX].dir[2] = 0;
}

struct GfxRenderingAPI* gfx_get_current_rendering_api(void) {
    return gfx_rapi;
}

void gfx_start_frame(void) {
    gfx_wapi->handle_events();
}

void gfx_run(Gfx* commands) {
    gfx_sp_reset();
    gfx_rapi->start_frame();
    rdp_state_gen++;   // per-frame state reset -> re-derive triangle setup on first tri
#if GFX_PROF
    uint64_t t0 = PROF_NOW();
    uint64_t st0 = perf_cntr_count(PRFC1);
#endif
    gfx_run_dl(commands);
#if GFX_PROF
    uint64_t t1 = PROF_NOW();
    prof_stall_walk = perf_cntr_count(PRFC1) - st0;
#endif
    gfx_rapi->end_frame();
#if GFX_PROF
    uint64_t t2 = PROF_NOW();
    prof_t_walk = t1 - t0; prof_t_flush = t2 - t1;
#endif
    gfx_wapi->swap_buffers_begin();
}

void gfx_end_frame(void) {
#if GFX_PROF
    uint64_t t3 = PROF_NOW();
#endif
    gfx_rapi->finish_render();
#if GFX_PROF
    prof_t_finish = PROF_NOW() - t3;
    prof_frame++;
    if ((prof_frame % 60) == 0 || prof_t_walk > (uint64_t) GFX_PROF_SLOW_US * 200u) {
        printf("GFXPROF f=%lu walk=%luus flush=%luus finish=%luus tris=%lu verts=%lu mtx=%lu texup=%lu op=%lu pt=%lu tr=%lu | vtx=%luus tri=%luus (setup=%luus clip=%luus bake=%luus submit=%luus) evals=%lu hits=%lu stamps=%lu setups=%lu su=%lu/%lu/%lu/%lu quads=%lu(fast %lu) q=%luus mtx=%luus\n",
               (unsigned long) prof_frame, PROF_US(prof_t_walk), PROF_US(prof_t_flush),
               PROF_US(prof_t_finish), (unsigned long) prof_tris, (unsigned long) prof_verts,
               (unsigned long) prof_mtx, (unsigned long) prof_texup, (unsigned long) gfx_pvr_prof_op,
               (unsigned long) gfx_pvr_prof_pt, (unsigned long) gfx_pvr_prof_tr,
               PROF_US(prof_t_vtx), PROF_US(prof_t_tri), PROF_US(prof_t_tri_setup),
               PROF_US(prof_t_clip - prof_t_tri_setup), PROF_US(prof_t_bake), PROF_US(prof_t_submit),
               (unsigned long) prof_evals, (unsigned long) prof_hits, (unsigned long) prof_stamps, (unsigned long) prof_setups,
               PROF_US(prof_su[0]), PROF_US(prof_su[1]), PROF_US(prof_su[2]), PROF_US(prof_su[3]),
               (unsigned long) prof_quads, (unsigned long) prof_quads_fast, PROF_US(prof_t_quad), PROF_US(prof_t_mtx));
    }
    prof_tris = prof_verts = prof_mtx = prof_texup = 0;
    prof_t_vtx = prof_t_tri = prof_t_tri_setup = prof_t_clip = prof_t_bake = prof_t_submit = 0;
    if (((prof_frame % 60) == 0 || prof_t_walk > (uint64_t) GFX_PROF_SLOW_US * 200u) && prof_stall_idx >= 0)
        printf("GFXPROF   stall %s = %luus of walk\n", prof_stall_modes[prof_stall_idx].name, PROF_US(prof_stall_walk));
    if ((prof_frame % 8) == 0 && prof_stall_idx >= 0) {   // rotate every 8 frames: all 5 modes in ~40
        prof_stall_idx = (prof_stall_idx + 1) % (int) (sizeof(prof_stall_modes) / sizeof(prof_stall_modes[0]));
        perf_cntr_stop(PRFC1); perf_cntr_clear(PRFC1);
        perf_cntr_start(PRFC1, prof_stall_modes[prof_stall_idx].mode, PMCR_COUNT_CPU_CYCLES);
    }
    if ((prof_frame % 60) == 0) {
        // top-4 generation-bumping opcodes this frame
        for (int t = 0; t < 4; t++) {
            int best = -1; uint32_t bv = 0;
            for (int o = 0; o < 256; o++) if (prof_op_bumps[o] > bv) { bv = prof_op_bumps[o]; best = o; }
            if (best < 0 || bv == 0) break;
            printf("GFXPROF   bump op 0x%02x x%lu\n", best, (unsigned long) bv);
            prof_op_bumps[best] = 0;
        }
    }
    for (int o = 0; o < 256; o++) prof_op_bumps[o] = 0;
    prof_evals = prof_hits = prof_stamps = prof_setups = 0;
    prof_su[0] = prof_su[1] = prof_su[2] = prof_su[3] = 0;
    prof_t_quad = prof_t_mtx = 0; prof_quads = prof_quads_fast = 0;
    gfx_pvr_prof_op = gfx_pvr_prof_pt = gfx_pvr_prof_tr = 0;
#endif
    gfx_wapi->swap_buffers_end();
}
