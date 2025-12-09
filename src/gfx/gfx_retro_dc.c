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

#include "gfx_pc.h"
#include "gfx_cc.h"
#include "gfx_window_manager_api.h"
#include "gfx_rendering_api.h"
#include "gfx_screen_config.h"
#include "macros.h"

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>
#include <GL/glkos.h>
#include "gl_fast_vert.h"

#include "sh4zam.h"

uint32_t last_set_texture_image_width;
int draw_rect;
uint32_t oops_texture_id;
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

extern int do_radar_mark;
extern int use_gorgon_alpha;

int do_andross = 0;

// enough to submit the nintendo logo in one go
// #define MAX_BUFFERED (384)
// need to save some memory
#define MAX_BUFFERED 128
#define MAX_LIGHTS 8
#define MAX_VERTICES 32

int alpha_noise = 0;

int do_starfield = 0;

int do_menucard = 0;

struct ShaderProgram {
    uint8_t enabled;
    uint32_t shader_id;
    struct CCFeatures cc;
    int mix;
    uint8_t texture_used[2];
    int texture_ord[2];
    int num_inputs;
};

struct RGBA {
    uint8_t r, g, b, a;
};

struct XYWidthHeight {
    uint16_t x, y, width, height;
};

// exactly 32 bytes
struct __attribute__((aligned(32))) LoadedVertex {
    // 0, 4, 8, 12
    float _x, _y /* , _z, _w */;
    // 16, 20, 24, 28
    float x, y, z /* , w */;
    // 32, 36
    float u, v;
    // 40
    struct RGBA color;
};

struct __attribute__((aligned(16))) LoadedNormal {
    float x, y, z, w;
};

// bits 0 - 5 -> clip_rej
// bit 6 - wlt0
// bit 7 - lit
uint8_t __attribute__((aligned(32))) clip_rej[MAX_VERTICES];

// exactly half of a cache-line in size
struct TextureHashmapNode {
    // 0
    struct TextureHashmapNode* next;
    // 4
    uint32_t texture_id;
    // 8
    uint32_t key;
    // 12
    uint8_t dirty;
    // 13
    uint8_t linear_filter;
    // 14
    uint8_t cms;
    // 15
    uint8_t cmt;
};

struct TextureHashmapNode oops_node;

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
    struct LoadedVertex __attribute__((aligned(32))) loaded_vertices[MAX_VERTICES + 4];
    struct LoadedNormal __attribute__((aligned(32))) loaded_normals[MAX_VERTICES + 4];
    struct __attribute__((aligned(32))) dc_fast_t loaded_vertices_2D[4];

    float modelview_matrix_stack[11][4][4] __attribute__((aligned(32)));

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
    uint8_t old_cull;
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

    struct RGBA env_color, prim_color, fog_color, fill_color;
    struct XYWidthHeight viewport, scissor;
    uint8_t viewport_or_scissor_changed;
    void* z_buf_address;
    void* color_image_address;
} rdp;

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
    uint16_t pad;
};

struct RenderingState rendering_state;

struct GfxDimensions gfx_current_dimensions;

static uint8_t dropped_frame;

static dc_fast_t __attribute__((aligned(32))) buf_vbo[MAX_BUFFERED * 3]; // 3 vertices in a triangle
static dc_fast_t __attribute__((aligned(32))) quad_vbo[2 * 3];           // 2 tris make a quad
static size_t buf_vbo_len = 0;
static size_t buf_num_vert = 0;
static size_t buf_vbo_num_tris = 0;

static struct GfxWindowManagerAPI* gfx_wapi;
static struct GfxRenderingAPI* gfx_rapi;

static uint16_t __attribute__((aligned(32))) tlut[256];

static void gfx_flush(void) {
    if (buf_vbo_len > 2) {
        gfx_rapi->draw_triangles((void*) buf_vbo, buf_vbo_len, buf_vbo_num_tris);
        buf_vbo_len = 0;
        buf_num_vert = 0;
        buf_vbo_num_tris = 0;
    }
}

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
    gfx_flush();
    struct ColorCombiner* comb = &color_combiner_pool[color_combiner_pool_size++];
    gfx_generate_cc(comb, cc_id);
    return prev_combiner = comb;
}

void gfx_clear_texidx(GLuint texidx);

void reset_texcache(void) {
    gfx_texture_cache.pool_pos = 0;
    memset(&gfx_texture_cache, 0, sizeof(gfx_texture_cache));
}

static inline uint32_t pack_key(uint32_t a, uint8_t pal) {
    uint32_t key = 0;
    key |= ((uint32_t) (a >> 5)) << 2; // the rest of the bits
    key |= ((uint32_t) (pal & 0x3));   // 2 bits
    return key;
}

static inline uint32_t unpack_A(uint32_t key) {
    uint32_t addr = (uint32_t) ((key >> 2));
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
    uintptr_t addrcomp = ((uintptr_t) segaddr >> 5);

    struct TextureHashmapNode** node = &gfx_texture_cache.hashmap[hash];
    uintptr_t last_node = &gfx_texture_cache.pool[gfx_texture_cache.pool_pos];
    while (*node != NULL && ((uintptr_t) *node < last_node)) {
        struct TextureHashmapNode* cur_node = (*node);
        __builtin_prefetch(cur_node->next);
        uintptr_t unpaddr = (uintptr_t) unpack_A((*node)->key);
        if (unpaddr == addrcomp) {
            cur_node->dirty = 1;
            return;
        }
        node = &cur_node->next;
    }
}

void gfx_opengl_replace_texture(const uint8_t* rgba32_buf, int width, int height, unsigned int type);

#define MEM_BARRIER_PREF(ptr) asm volatile("pref @%0" : : "r"((ptr)) : "memory")

static __attribute__((noinline)) uint8_t gfx_texture_cache_lookup(int tile, struct TextureHashmapNode** n,
                                                                  const uint8_t* orig_addr, uint32_t tmem, uint32_t siz,
                                                                  uint8_t pal) {
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
            gfx_rapi->select_texture(tile, (*node)->texture_id);

            if ((*node)->dirty) {
                (*node)->dirty = 0;
                return 0;
            } else {
                return 1;
            }
        }
        node = &(*node)->next;
    }

    *node = &gfx_texture_cache.pool[gfx_texture_cache.pool_pos++];
    if ((*node)->key == 0) {
        (*node)->texture_id = gfx_rapi->new_texture();
    }
    gfx_rapi->select_texture(tile, (*node)->texture_id);

    gfx_rapi->set_sampler_parameters(tile, 0, 0, 0);

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

    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_4_4_4_4_REV);
}

static void import_texture_rgba16_alphablur(int tile) {
    gfx_rapi->upload_texture((uint8_t*) scaled2, 160, 120, GL_UNSIGNED_SHORT_1_5_5_5_REV);
}

static void import_texture_rgba16(int tile) {
    uint32_t i;

    __builtin_prefetch(table32);
    uint32_t width = rdp.texture_tile.line_size_bytes >> 1;
    uint32_t height =
        (uint32_t) ((float) rdp.loaded_texture[tile].size_bytes / (float) rdp.texture_tile.line_size_bytes);

    uint32_t loopcount = rdp.loaded_texture[tile].size_bytes >> 1;
    uint16_t* start = (uint16_t*) rdp.loaded_texture[tile].addr;
    for (i = 0; i < loopcount; i++) {
        uint16_t col16 = start[i];
        rgba16_buf[i] = brightit_argb1555(((col16 & 1) << 15) | (col16 >> 1));
    }
    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
}

static void import_texture_rgba32(int tile) {
    uint32_t width = rdp.texture_tile.line_size_bytes >> 1;
    uint32_t height =
        (uint32_t) ((float) (rdp.loaded_texture[tile].size_bytes >> 1) / (float) rdp.texture_tile.line_size_bytes);
    __builtin_prefetch(table16);
    uint32_t* startaddr = (uint8_t*) ((uintptr_t) rdp.loaded_texture[tile].addr & ~3);
    uint32_t wxh = width * height;
    for (uint32_t i = 0; i < wxh; i++) {
        uint32_t p = startaddr[i];
        uint8_t r = table16[(p >> 28) & 0x0f];
        uint8_t g = table16[(p >> 20) & 0x0f];
        uint8_t b = table16[(p >> 12) & 0x0f];
        uint8_t a = (p >> 4) & 0x0f;

        rgba16_buf[i] = (a << 12) | (r << 8) | (g << 4) | (b);
    }
    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_4_4_4_4_REV);
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
    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_4_4_4_4_REV);
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

    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_4_4_4_4_REV);
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

    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
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

    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
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

    shz_dcache_alloc_line(rgba16_buf);

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

static __attribute__((noinline)) void gfx_sp_matrix(uint8_t parameters, const void* addr) {
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
                    if (rsp.modelview_matrix_stack_size < 11) {
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

        d->u = (vn->tc[0] * rsp.texture_scaling_factor.s) * recip64k;
        d->v = (vn->tc[1] * rsp.texture_scaling_factor.t) * recip64k;

        MEM_BARRIER_PREF(vn + 1);

        d->_x = x * recw;
        d->_y = y * recw;

        n->x = vn->n[0];
        n->y = vn->n[1];
        n->z = vn->n[2];
        d->color.a = vn->a;
    }
}

static void __attribute__((noinline)) gfx_sp_vertex_light_step1b(int n_vertices, int dest_index) {
    // SHZ_PREFETCH(&rsp.loaded_normals[dest_index]);
    for (int i = 0; i < n_vertices; i++, dest_index++) {
        struct LoadedVertex* d = &rsp.loaded_vertices[dest_index];
        struct LoadedNormal* n = &rsp.loaded_normals[dest_index];

        shz_vec3_t dot = shz_xmtrx_trans_vec3(shz_vec3_deref(n));

        // MEM_BARRIER_PREF(&rsp.loaded_normals[dest_index]+1);

        if (rsp.geometry_mode & G_TEXTURE_GEN_LINEAR) {
            dot.x = shz_acosf(dot.x) * recip2pi;
            dot.y = shz_acosf(dot.y) * recip2pi;
        } else {
            dot.x = (dot.x * 0.25f) + 0.25f;
            dot.y = (dot.y * 0.25f) + 0.25f;
        }

        d->u = (dot.x * rsp.texture_scaling_factor.s);
        d->v = (dot.y * rsp.texture_scaling_factor.t);
    }
}

static void __attribute__((noinline)) gfx_sp_vertex_light_step2(int n_vertices, int dest_index) {
    // SHZ_PREFETCH(&rsp.loaded_normals[dest_index]);
    for (int i = 0; i < n_vertices; i++, dest_index++) {
        struct LoadedNormal* n = &rsp.loaded_normals[dest_index];

        shz_vec3_t outinten = shz_xmtrx_trans_vec3(shz_vec3_deref(n));
        // SHZ_PREFETCH(&rsp.loaded_normals[dest_index+2]);
        n->z = 1.0f;
        n->x = MAX(0.0f, outinten.x);
        n->y = MAX(0.0f, outinten.y);
    }
}

static void __attribute__((noinline)) gfx_sp_vertex_light_step3(int n_vertices, int dest_index,
                                                                const Vtx* vertices) {
    // SHZ_PREFETCH(&rsp.loaded_vertices[dest_index]);
    for (int i = 0; i < n_vertices; i++, dest_index++) {
        struct LoadedVertex* d = &rsp.loaded_vertices[dest_index];
        struct LoadedNormal* n = &rsp.loaded_normals[dest_index];

        uint8_t r;
        uint8_t g;
        uint8_t b;
        shz_vec3_t outrgb = shz_xmtrx_trans_vec3(shz_vec3_deref(n));
        // SHZ_PREFETCH(&rsp.loaded_vertices[dest_index+1]);
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
        d->color.r = table256[r];
        d->color.g = table256[g];
        d->color.b = table256[b];
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

        d->_x = x * recw;
        d->_y = y * recw;

        // trivial clip rejection
        uint8_t cr = ((w < 0) ? 64 : 0x00);
        clip_rej[dest_index] = cr | trivial_reject(x, y, z, w);
    }
}
// y before x in the index number in the abi cmd that loads the lookats
#define LOOKAT_Y_IDX 0
#define LOOKAT_X_IDX 1
#define LCOEFF_Y_IDX 1
#define LCOEFF_X_IDX 0

static void __attribute__((noinline)) gfx_sp_vertex(uint8_t n_vertices, uint8_t dest_index, const Vtx* vertices) {
    shz_xmtrx_load_4x4(&rsp.MP_matrix);

    if (rsp.geometry_mode & G_LIGHTING) {
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
    } else {
        gfx_sp_vertex_no(n_vertices, dest_index, SEGMENTED_TO_VIRTUAL(vertices));
    }
}

int need_to_add = 0;
uint8_t add_r, add_g, add_b, add_a;

extern u16 aCoGroundGrassTex[];
extern u16 D_CO_6028A60[];
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

static void __attribute__((noinline)) gfx_sp_tri1(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx) {
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
        float dx1 = v1->_x - v2->_x;
        float dy1 = v1->_y - v2->_y;
        float dx2 = v3->_x - v2->_x;
        float dy2 = v3->_y - v2->_y;
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

    if (matrix_dirty) {
        gfx_flush();
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf((const float*) rsp.MP_matrix);
        matrix_dirty = 0;
    }

    if (do_radar_mark)
        gfx_flush();

    uint8_t depth_test = (rsp.geometry_mode & G_ZBUFFER) == G_ZBUFFER;
    if ((depth_test != rendering_state.depth_test)) {
        gfx_flush();
        gfx_rapi->set_depth_test(depth_test);
        rendering_state.depth_test = depth_test;
    }

    uint8_t z_upd = (rdp.other_mode_l & Z_UPD) == Z_UPD;
    if ((z_upd != rendering_state.depth_mask)) {
        gfx_flush();
        gfx_rapi->set_depth_mask(z_upd);
        rendering_state.depth_mask = z_upd;
    }

    uint8_t zmode_decal = (rdp.other_mode_l & ZMODE_DEC) == ZMODE_DEC;
    if ((zmode_decal != rendering_state.decal_mode)) {
        gfx_flush();
        gfx_rapi->set_zmode_decal(zmode_decal);
        rendering_state.decal_mode = zmode_decal;
    }

    if (rdp.viewport_or_scissor_changed) {
        if (memcmp(&rdp.viewport, &rendering_state.viewport, sizeof(rdp.viewport)) != 0) {
            gfx_flush();
            gfx_rapi->set_viewport(rdp.viewport.x, rdp.viewport.y, rdp.viewport.width, rdp.viewport.height);
            rendering_state.viewport = rdp.viewport;
        }
        if (memcmp(&rdp.scissor, &rendering_state.scissor, sizeof(rdp.scissor)) != 0) {
            gfx_flush();
            gfx_rapi->set_scissor(rdp.scissor.x, rdp.scissor.y, rdp.scissor.width, rdp.scissor.height);
            rendering_state.scissor = rdp.scissor;
        }
        rdp.viewport_or_scissor_changed = 0;
    }

    uint32_t cc_id = rdp.combine_mode;

    uint8_t use_alpha = (rdp.other_mode_l & (G_BL_A_MEM << 18)) == 0;
    uint8_t use_fog = (rdp.other_mode_l >> 30) == G_BL_CLR_FOG;
    if ((rsp.use_fog != use_fog)) {
        gfx_flush();
        rsp.use_fog = use_fog;
    }
    uint8_t texture_edge = (rdp.other_mode_l & CVG_X_ALPHA) == CVG_X_ALPHA;
    uint8_t use_noise = (rdp.other_mode_h == 0x2ca0);

    if (alpha_noise) {
        gfx_flush();
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
        gfx_flush();
        gfx_rapi->unload_shader(rendering_state.shader_program);
        gfx_rapi->load_shader(prg);
        rendering_state.shader_program = prg;
    }

    if ((use_alpha != rendering_state.alpha_blend)) {
        gfx_flush();
        gfx_rapi->set_use_alpha(use_alpha);
        rendering_state.alpha_blend = use_alpha;
    }

    uint8_t num_inputs;
    uint8_t used_textures[2];
    gfx_rapi->shader_get_info(prg, &num_inputs, used_textures);
    int i;

    void* texaddr = SEGMENTED_TO_VIRTUAL(rdp.texture_to_load.addr);
    void* grass = SEGMENTED_TO_VIRTUAL(aCoGroundGrassTex);
    void* water = SEGMENTED_TO_VIRTUAL(D_CO_6028A60);
    void* ve1ground = SEGMENTED_TO_VIRTUAL(aVe1GroundTex);
    void* maground = SEGMENTED_TO_VIRTUAL(aMaGroundTex);
    void* aqground = SEGMENTED_TO_VIRTUAL(aAqGroundTex);
    void* aqwater = SEGMENTED_TO_VIRTUAL(aAqWaterTex);
    void* ti_ground = SEGMENTED_TO_VIRTUAL(D_TI_6001BA8);
    void* zowater = SEGMENTED_TO_VIRTUAL(aZoWaterTex);
    float recip_tex_width = 0.03125f;  // 1 / 32
    float recip_tex_height = 0.03125f; // 1 / 32
    uint8_t usetex = used_textures[0];
    uint8_t linear_filter = (rdp.other_mode_h & (3U << G_MDSFT_TEXTFILT)) != G_TF_POINT;

    if (usetex) {
        if (rdp.textures_changed[0]) {
            // necessary
            gfx_flush();
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

        if (special_cm) {
            cms = 0;
            cmt = 0;
            if (gCurrentLevel == LEVEL_TITANIA) {
                cms = G_TX_MIRROR;
            }
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

        if (((linear_filter != rendering_state.textures[0]->linear_filter) ||
             (rendering_state.textures[0]->cms != cms) || (rendering_state.textures[0]->cmt != cmt))) {
            gfx_flush();
        }

        gfx_rapi->set_sampler_parameters(0, linear_filter, cms, cmt);
        rendering_state.textures[0]->linear_filter = linear_filter;
        rendering_state.textures[0]->cms = cms;
        rendering_state.textures[0]->cmt = cmt;

        uint32_t tex_width = (rdp.texture_tile.lrs - rdp.texture_tile.uls + 4) >> 2;
        uint32_t tex_height = (rdp.texture_tile.lrt - rdp.texture_tile.ult + 4) >> 2;
        recip_tex_width = shz_fast_invf((float) tex_width);
        recip_tex_height = shz_fast_invf((float) tex_height);
    }

    if (!do_the_blur) {
        recip_tex_width *= get_current_u_scale();
        recip_tex_height *= get_current_v_scale();
    }

    uint8_t lit = l_clip_rej[0] & 0x80;
    float ofs = linear_filter ? 0.5f : 0.0f;
    float uls = (float) (rdp.texture_tile.uls * 0.25f) - ofs;
    float ult = (float) (rdp.texture_tile.ult * 0.25f) - ofs;
    if (do_radar_mark) {
        screen_2d_z += 1.0f;
    }

    uint32_t color_r, color_g, color_b, color_a;
    uint32_t cc_rgb = rdp.combine_mode & 0xFFF;
    uint32_t packedc;
    color_r = color_g = color_b = color_a = 255;
    int use_shade = 0;

    if (cc_rgb == 0x668) {
        // (0 - G_CCMUX_ENV) * 1 + G_CCMUX_PRIM
        color_r = (rdp.prim_color.r - rdp.env_color.r);
        color_g = (rdp.prim_color.g - rdp.env_color.g);
        color_b = (rdp.prim_color.b - rdp.env_color.b);
        color_a = rdp.prim_color.a;
    } else if (num_inputs > 1) {
        int i0 = comb->shader_input_mapping[0][1] == CC_PRIM;
        int i2 = comb->shader_input_mapping[0][0] == CC_ENV;

        int i3 = comb->shader_input_mapping[0][0] == CC_PRIM;
        int i4 = comb->shader_input_mapping[0][1] == CC_ENV;

        if (i0 && i2) {
            color_r = 255 - rdp.env_color.r;
            color_g = 255 - rdp.env_color.g;
            color_b = 255 - rdp.env_color.b;
            color_a = rdp.prim_color.a;
        } else if (i3 && i4) {
            color_r = rdp.prim_color.r;
            color_g = rdp.prim_color.g;
            color_b = rdp.prim_color.b;
            color_a = rdp.prim_color.a;

            color_r *= ((rdp.env_color.r + 255));
            color_g *= ((rdp.env_color.g + 255));
            color_b *= ((rdp.env_color.b + 255));
            color_a *= rdp.env_color.a;

            color_r >>= 8;
            color_g >>= 8;
            color_b >>= 8;
            color_a >>= 8;

            uint32_t max_c;
            max_c = MAX4(255, color_r, color_g, color_b);
            float maxc = shz_div_posf(255.0f, (float) max_c);

            float rn, gn, bn;
            rn = (float) color_r * maxc;
            gn = (float) color_g * maxc;
            bn = (float) color_b * maxc;
            color_r = (uint32_t) rn;
            color_g = (uint32_t) gn;
            color_b = (uint32_t) bn;
        } else {
            goto othercolorcode;
        }
    } else {
    othercolorcode:
        int k;
        for (k = 0; k < 1 + (use_alpha ? 1 : 0); k++) {
            switch (comb->shader_input_mapping[k][0]) {
                case G_CCMUX_PRIMITIVE_ALPHA:
                    color_r = color_g = color_b = rdp.prim_color.a;
                    color_a = k ? rdp.prim_color.a : 255;
                    use_shade = 0;
                    break;
                case G_CCMUX_ENV_ALPHA:
                    color_r = color_g = color_b = rdp.env_color.a;
                    color_a = k ? rdp.env_color.a : 255;
                    use_shade = 0;
                    break;
                case CC_PRIM:
                    color_r = rdp.prim_color.r;
                    color_g = rdp.prim_color.g;
                    color_b = rdp.prim_color.b;
                    color_a = k ? rdp.prim_color.a : 255;
                    use_shade = 0;
                    break;
                case CC_SHADE:
                    use_shade = 1 + k;
                    break;
                case CC_ENV:
                    color_r = rdp.env_color.r;
                    color_g = rdp.env_color.g;
                    color_b = rdp.env_color.b;
                    color_a = k ? rdp.env_color.a : 255;
                    use_shade = 0;
                    break;
                default:
                    color_a = 255;
                    if (!lit) {
                        color_r = color_g = color_b = 255;
                    }
                    use_shade = 0;
                    break;
            }
        }
    }

    packedc = PACK_ARGB8888(color_r, color_g, color_b, color_a);

    for (i = 0; i < 3; i++) {
        if (!do_radar_mark) {
            buf_vbo[buf_num_vert].vert.x = v_arr[i]->x;
            buf_vbo[buf_num_vert].vert.y = v_arr[i]->y;
            buf_vbo[buf_num_vert].vert.z = v_arr[i]->z;
        } else {
            buf_vbo[buf_num_vert].vert.x = (v_arr[i]->_x * SCREEN_WIDTH) + SCREEN_WIDTH;
            buf_vbo[buf_num_vert].vert.y = SCREEN_HEIGHT - (v_arr[i]->_y * SCREEN_HEIGHT);
            buf_vbo[buf_num_vert].vert.z = screen_2d_z;
        }

        if (usetex) {
            buf_vbo[buf_num_vert].texture.u = ((v_arr[i]->u - uls) * recip_tex_width);
            buf_vbo[buf_num_vert].texture.v = ((v_arr[i]->v - ult) * recip_tex_height);
        }

        if (use_shade) {
            color_r = v_arr[i]->color.r;
            color_g = v_arr[i]->color.g;
            color_b = v_arr[i]->color.b;
            color_a = (use_shade - 1) ? v_arr[i]->color.a : 255;
            packedc = PACK_ARGB8888(color_r, color_g, color_b, color_a);
        }

        if (lit) {
            uint32_t tc_r;
            uint32_t tc_g;
            uint32_t tc_b;
            uint32_t light_r = v_arr[i]->color.r;
            uint32_t light_g = v_arr[i]->color.g;
            uint32_t light_b = v_arr[i]->color.b;
            if (/* (gGameState == 8) || */ gCurrentLevel == LEVEL_AREA_6 || gCurrentLevel == LEVEL_VENOM_ANDROSS) {
                tc_r = ((color_r * light_r) >> 8) & 0xff;
                tc_g = ((color_g * light_g) >> 8) & 0xff;
                tc_b = ((color_b * light_b) >> 8) & 0xff;
            } else {
                tc_r = ((((255 + color_r) >> 1) * light_r) >> 8) & 0xff;
                tc_g = ((((255 + color_g) >> 1) * light_g) >> 8) & 0xff;
                tc_b = ((((255 + color_b) >> 1) * light_b) >> 8) & 0xff;
            }
            packedc = PACK_ARGB8888(tc_r, tc_g, tc_b, color_a);
        }

        buf_vbo[buf_num_vert].color.packed = packedc;

        buf_num_vert++;
        buf_vbo_len += sizeof(dc_fast_t);
    }
    buf_vbo_num_tris += 1;

    if (do_the_blur || do_radar_mark || path_priority_draw || do_space_bg || do_rectdepfix || do_menucard ||
        (buf_vbo_num_tris == (MAX_BUFFERED - 1))) {
        gfx_flush();
    }
}

extern void gfx_opengl_reset_frame(int r, int g, int b);
extern void gfx_opengl_draw_triangles_2d(void* buf_vbo, size_t buf_vbo_len, size_t buf_vbo_num_tris);

int do_ext_fill = 0;
extern int do_ending_bg;
int last_was_starfield = 0;

static void __attribute__((noinline)) gfx_sp_quad_2d(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx,
                                                     uint8_t vtx1_idx2, uint8_t vtx2_idx2, uint8_t vtx3_idx2) {
    dc_fast_t* v2d = &rsp.loaded_vertices_2D[0];
    if (!last_was_starfield || !do_starfield)
        gfx_flush();

    uint8_t depth_test = (rsp.geometry_mode & G_ZBUFFER) == G_ZBUFFER;
    if (depth_test != rendering_state.depth_test) {
        gfx_rapi->set_depth_test(depth_test);
        rendering_state.depth_test = depth_test;
    }

    uint8_t z_upd = (rdp.other_mode_l & Z_UPD) == Z_UPD;
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
    uint8_t used_textures[2];
    gfx_rapi->shader_get_info(prg, &num_inputs, used_textures);
    uint8_t linear_filter = 1;

    if (used_textures[0]) {
        if (rdp.textures_changed[0]) {
            import_texture(0);
            rdp.textures_changed[0] = 0;
        }
        linear_filter = do_the_blur ? 0 : ((rdp.other_mode_h & (3U << G_MDSFT_TEXTFILT)) != G_TF_POINT);
        gfx_rapi->set_sampler_parameters(0, linear_filter, rdp.texture_tile.cms, rdp.texture_tile.cmt);
        rendering_state.textures[0]->linear_filter = linear_filter;
        rendering_state.textures[0]->cms = rdp.texture_tile.cms;
        rendering_state.textures[0]->cmt = rdp.texture_tile.cmt;
    }

    uint8_t use_texture = !do_ext_fill && used_textures[0];
    float recip_tex_width = 0.03125f;  // 1 / 32;
    float recip_tex_height = 0.03125f; // 1 / 32

    dc_fast_t* tmpv = v2d;

    if (use_texture) {
        if (!do_the_blur) {
            uint32_t tex_width = ((rdp.texture_tile.lrs - rdp.texture_tile.uls + 4) * 0.25f);
            if (tex_width > 300 && gGameState == 8) {
                do_ending_bg = 1;
            } else {
                do_ending_bg = 0;
            }
            uint32_t tex_height = ((rdp.texture_tile.lrt - rdp.texture_tile.ult + 4) * 0.25f);
            recip_tex_width = shz_fast_invf((float) tex_width);
            recip_tex_height = shz_fast_invf((float) tex_height);
            float offs = linear_filter ? 0.5f : 0.0f;
            float uls = (float) (rdp.texture_tile.ult * 0.25f) - offs;
            float ult = (float) (rdp.texture_tile.ult * 0.25f) - offs;
            float u;
            float v;

            // / 32
            u = (tmpv->texture.u * 0.03125f) - uls;
            tmpv->texture.u = (u * recip_tex_width) * get_current_u_scale();
            v = (tmpv->texture.v * 0.03125f) - ult;
            tmpv++->texture.v = (v * recip_tex_height) * get_current_v_scale();

            u = (tmpv->texture.u * 0.03125f) - uls;
            tmpv->texture.u = (u * recip_tex_width) * get_current_u_scale();
            v = (tmpv->texture.v * 0.03125f) - ult;
            tmpv++->texture.v = (v * recip_tex_height) * get_current_v_scale();

            u = (tmpv->texture.u * 0.03125f) - uls;
            tmpv->texture.u = u * recip_tex_width * get_current_u_scale();
            v = (tmpv->texture.v * 0.03125f) - ult;
            tmpv++->texture.v = v * recip_tex_height * get_current_v_scale();

            u = (tmpv->texture.u * 0.03125f) - uls;
            tmpv->texture.u = u * recip_tex_width * get_current_u_scale();
            v = (tmpv->texture.v * 0.03125f) - ult;
            tmpv->texture.v = v * recip_tex_height * get_current_v_scale();
        } else {
            tmpv->vert.x = 0;
            tmpv->vert.y = 0;
            tmpv->texture.u = 0.0f;
            tmpv++->texture.v = 0.0f;
            tmpv->vert.x = 0;
            tmpv->vert.y = 479;
            tmpv->texture.u = 0.0f;
            tmpv++->texture.v = 0.9375f;
            tmpv->vert.x = 639;
            tmpv->vert.y = 479;
            tmpv->texture.u = 0.625f;
            tmpv++->texture.v = 0.9375f;
            tmpv->vert.x = 639;
            tmpv->vert.y = 0;
            tmpv->texture.u = 0.625f;
            tmpv->texture.v = 0.0f;
        }
    }

    uint32_t rectcolor = 0xffffffff;
    uint32_t color_r = 0;
    uint32_t color_g = 0;
    uint32_t color_b = 0;
    uint32_t color_a = 0;

    if (num_inputs > 1) {
        int i0 = comb->shader_input_mapping[0][1] == CC_PRIM;
        int i2 = comb->shader_input_mapping[0][0] == CC_ENV;

        int i3 = comb->shader_input_mapping[0][0] == CC_PRIM;
        int i4 = comb->shader_input_mapping[0][1] == CC_ENV;

        if (i0 && i2) {
            color_r = 255 - rdp.env_color.r;
            color_g = 255 - rdp.env_color.g;
            color_b = 255 - rdp.env_color.b;
            color_a = rdp.prim_color.a;

            rectcolor = PACK_ARGB8888(color_r, color_g, color_b, color_a);
        } else if (i3 && i4) {
            color_r = rdp.prim_color.r;
            color_g = rdp.prim_color.g;
            color_b = rdp.prim_color.b;
            color_a = rdp.prim_color.a;

            color_r *= ((rdp.env_color.r + 255));
            color_g *= ((rdp.env_color.g + 255));
            color_b *= ((rdp.env_color.b + 255));
            color_a *= rdp.env_color.a;

            color_r >>= 8;
            color_g >>= 8;
            color_b >>= 8;
            color_a >>= 8;

            float rn, gn, bn;
            uint32_t max_c = MAX4(255, color_r, color_g, color_b);
            float maxc = shz_div_posf(255.0f, (float) max_c);

            rn = (float) color_r * maxc;
            gn = (float) color_g * maxc;
            bn = (float) color_b * maxc;
            color_r = (uint32_t) rn;
            color_g = (uint32_t) gn;
            color_b = (uint32_t) bn;

            rectcolor = PACK_ARGB8888(color_r, color_g, color_b, color_a);
        } else {
            goto thenext2dthing;
        }
    } else {
    thenext2dthing:
        int k;
        for (k = 0; k < 1 + (use_alpha ? 0 : 0); k++) {
            switch (comb->shader_input_mapping[k][0]) {
                case CC_PRIM:
                    rectcolor = PACK_ARGB8888(rdp.prim_color.r, rdp.prim_color.g, rdp.prim_color.b, rdp.prim_color.a);
                    break;
                case CC_SHADE:
                    rectcolor = v2d[0].color.packed;
                    break;
                case CC_ENV:
                    rectcolor = PACK_ARGB8888(rdp.env_color.r, rdp.env_color.g, rdp.env_color.b, rdp.env_color.a);
                    break;
                default:
                    rectcolor = 0xffffffff;
                    break;
            }
        }
    }

    v2d++->color.packed = rectcolor;
    v2d++->color.packed = rectcolor;
    v2d++->color.packed = rectcolor;
    v2d->color.packed = rectcolor;

    if (do_starfield) {
        last_was_starfield = 1;
    } else {
        last_was_starfield = 0;
    }

    gfx_opengl_draw_triangles_2d((void*) rsp.loaded_vertices_2D, 4, use_texture);
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

extern float gl_fog_start;
extern float gl_fog_end;
extern float gProjectNear;
extern float gProjectFar;

// the following isn't very rigorous
// I eyeballed it and tweaked the `a` param until it seemed almost right
// and still need to scale it more per-level in `gfx_gldc` when setting gl fog params
static inline float exp_map_0_1000_f(float x) {
    const float a = 138.62943611198894f;
    if (x <= 0.0f)
        return 0.0f;
    if (x >= 1000.0f)
        return x * 1.01f; // 1000.0f;

    const float t = x * 0.001f;
    const float num = expm1f(a * (t - 1.0f));
    const float den = expm1f(-a);
    return 1000.0f * (1.0f - shz_divf(num, den));
}

extern int fog_dirty;

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
            int16_t fog_ofs = (int16_t) data;
            if ((!rendering_state.fog_change)) {
                rendering_state.fog_change = 1;
                float recip_fog_mul = shz_fast_invf(fog_mul);
                float n64_min = 500.0f * (1.0f - (float) fog_ofs * recip_fog_mul);
                float n64_max = n64_min + 128000.0f * recip_fog_mul;
                float scale = (gProjectFar - gProjectNear) * 0.001f;
                gl_fog_start = gProjectNear + scale * exp_map_0_1000_f(n64_min);
                gl_fog_end = gProjectNear + scale * exp_map_0_1000_f(n64_max);
                fog_dirty = 1;
            }
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
            c1 = brightit_argb1555((c1 << 15) | ((c1 >> 1) & 0x7FFF));
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
    rdp.env_color.r = table32[r >> 3] << 3;
    rdp.env_color.g = table32[g >> 3] << 3;
    rdp.env_color.b = table32[b >> 3] << 3;
    rdp.env_color.a = a;
}

static void gfx_dp_set_prim_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    rdp.prim_color.r = table32[r >> 3] << 3;
    rdp.prim_color.g = table32[g >> 3] << 3;
    rdp.prim_color.b = table32[b >> 3] << 3;
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
        r = table32[r >> 3] << 3;
        g = table32[g >> 3] << 3;
        b = table32[b >> 3] << 3;
    }

    if (sCutsceneState == 2) {
        r = 0;
        g = 0;
        b = 0;
    }

    rdp.fog_color.r = r;
    rdp.fog_color.g = g;
    rdp.fog_color.b = b;
    if ((!rendering_state.fog_col_change)) {
        rendering_state.fog_col_change = 1;
        float fog_color[4] = { rdp.fog_color.r * recip255, rdp.fog_color.g * recip255, rdp.fog_color.b * recip255,
                               1.0f };
        glFogfv(GL_FOG_COLOR, fog_color);
    }
}

static void gfx_dp_set_fill_color(uint32_t packed_color) {
    uint16_t col16 = (uint16_t) packed_color;
    uint32_t r = (col16 >> 11) & 0x1f;
    uint32_t g = (col16 >> 6) & 0x1f;
    uint32_t b = (col16 >> 1) & 0x1f;
    uint32_t a = col16 & 1;

    rdp.fill_color.r = table32[r] << 3;
    rdp.fill_color.g = table32[g] << 3;
    rdp.fill_color.b = table32[b] << 3;
    rdp.fill_color.a = a * 255;
}

// in here instead of gfx_gldc.c becasue of `matrix_dirty` and `rsp` references
void gfx_opengl_2d_projection(void) {
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 640, 480, 0, -20000.0f, 20000.0f);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    matrix_dirty = 1;
}

void gfx_opengl_reset_projection(void) {
    matrix_dirty = 1;
}

static void __attribute__((noinline)) gfx_draw_rectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry) {
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
    ulxf = (ulxf * SCREEN_WIDTH) + SCREEN_WIDTH;
    lrxf = (lrxf * SCREEN_WIDTH) + SCREEN_WIDTH;

    ulyf = (ulyf * SCREEN_HEIGHT) + SCREEN_HEIGHT;
    lryf = (lryf * SCREEN_HEIGHT) + SCREEN_HEIGHT;

    dc_fast_t* ul = &rsp.loaded_vertices_2D[0];
    dc_fast_t* ll = &rsp.loaded_vertices_2D[1];
    dc_fast_t* lr = &rsp.loaded_vertices_2D[2];
    dc_fast_t* ur = &rsp.loaded_vertices_2D[3];

    screen_2d_z += 1.0f;

    ul->vert.x = ulxf;
    ul->vert.y = ulyf;
    ul->vert.z = screen_2d_z;

    ll->vert.x = ulxf;
    ll->vert.y = lryf;
    ll->vert.z = screen_2d_z;

    lr->vert.x = lrxf;
    lr->vert.y = lryf;
    lr->vert.z = screen_2d_z;

    ur->vert.x = lrxf;
    ur->vert.y = ulyf;
    ur->vert.z = screen_2d_z;

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

    dc_fast_t* ul = &rsp.loaded_vertices_2D[0];
    dc_fast_t* ll = &rsp.loaded_vertices_2D[1];
    dc_fast_t* lr = &rsp.loaded_vertices_2D[2];
    dc_fast_t* ur = &rsp.loaded_vertices_2D[3];
    ul->texture.u = !flip ? uls : lrs;
    ul->texture.v = !flip ? ult : lrt;
    lr->texture.u = !flip ? lrs : uls;
    lr->texture.v = !flip ? lrt : ult;

    ll->texture.u = !flip ? uls : lrs;
    ll->texture.v = !flip ? lrt : ult;
    ur->texture.u = !flip ? lrs : uls;
    ur->texture.v = !flip ? ult : lrt;

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

static void __attribute__((noinline)) gfx_dp_fill_rectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry) {
    int i;

    if (rdp.color_image_address == rdp.z_buf_address) {
        // Don't clear Z buffer here since we already did it with glClear
        return;
    }
    do_ext_fill = 1;

    uint32_t mode = (rdp.other_mode_h & (3U << G_MDSFT_CYCLETYPE));

    uint32_t saved_geom_mode = rsp.geometry_mode;
    uint32_t saved_other_mode_l = rdp.other_mode_l;

    if (mode == G_CYC_COPY || mode == G_CYC_FILL) {
        // Per documentation one extra pixel is added in this modes to each edge
        lrx += 1 << 2;
        lry += 1 << 2;
        rsp.geometry_mode &= ~G_ZBUFFER;
        rdp.other_mode_l &= ~Z_UPD;
    }
    if (!do_starfield) {
        lrx += ((1 << 2) + 2);
        lry += ((1 << 2) + 2);
    }
    for (i = 0; i < 4; i++) {
        dc_fast_t* v = &rsp.loaded_vertices_2D[i];
        v->color.array.a = rdp.fill_color.a;
        v->color.array.b = rdp.fill_color.b;
        v->color.array.g = rdp.fill_color.g;
        v->color.array.r = rdp.fill_color.r;
    }

    gfx_draw_rectangle(ulx, uly, lrx, lry);
    rsp.geometry_mode = saved_geom_mode;
    rdp.other_mode_l = saved_other_mode_l;
    do_ext_fill = 0;
}

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

volatile int do_reticle = 0;
int do_fillrect_blend = 0;
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

extern uint8_t gorgon_alpha;
extern Gfx aAndBackdropDL[];
extern Gfx aLandmasterModelDL[];
extern Gfx D_A6_6015EE0[];
extern Gfx aAwBodyDL[];
extern Gfx aSxCanineDL[];
int ever_did = 0;
extern int ending_great_fox;
extern Gfx aGreatFoxDamagedDL[];
extern Gfx aGreatFoxIntactDL[];

#define GFX_DL_STACK_MAX 4 /* tune this to whatever nesting you expect */

static Gfx __attribute__((aligned(32))) * dl_stack[GFX_DL_STACK_MAX];

static void __attribute__((noinline)) gfx_run_dl(Gfx* cmd) {
    int dl_sp = 0;

    ending_great_fox = 0;

    cmd = seg_addr((uintptr_t) cmd);

    if ((cmd == seg_addr(aGreatFoxDamagedDL)) || (cmd == seg_addr(aGreatFoxIntactDL))) {
        if (gGameState == 8)
            ending_great_fox = 1;
    }

    __builtin_prefetch(cmd);

    for (;;) {
        uint32_t opcode = cmd->words.w0 >> 24;

        if (cmd == seg_addr(aAndBackdropDL))
            do_andross = 1;
        else
            do_andross = 0;

        if (cmd->words.w0 == 0x424C4E44) {
            if ((cmd->words.w1 & 0xffffff00) == 0x46437700) {
                use_gorgon_alpha ^= 1;
                gorgon_alpha = cmd->words.w1 & 0x000000ff;
            } else {
                if (cmd->words.w1 == 0x12345678) {
                    do_radar_mark ^= 1;
                } else if (cmd->words.w1 == 0x46004400) {
                    path_priority_draw ^= 1;
                } else if (cmd->words.w1 == 0x46554350) {
                    do_reticle ^= 1;
                } else if (cmd->words.w1 == 0x46554360) {
                    do_the_blur ^= 1;
                } else if (cmd->words.w1 == 0x46554369) {
                    do_rectdepfix ^= 1;
                } else if (cmd->words.w1 == 0x46554370) {
                    do_starfield ^= 1;
                } else if (cmd->words.w1 == 0x46554380) {
                    do_fillrect_blend ^= 1;
                } else if (cmd->words.w1 == 0x465543DD) {
                    do_zfight ^= 1;
                    do_zflip = 0;
                } else if (cmd->words.w1 == 0x465543DE) {
                    do_zfight ^= 1;
                    do_zflip = 1;
                } else if (cmd->words.w1 == 0x465543DF) {
                    do_zfight ^= 1;
                    do_zflip = 2;
                } else if (cmd->words.w1 == 0x465543EE) {
                    do_menucard ^= 1;
                } else if (cmd->words.w1 == 0x465543F0) {
                    do_floorscroll ^= 1;
                } else if (cmd->words.w1 == 0x46664369) {
                    do_space_bg ^= 1;
                }
            }
            __builtin_prefetch((void*) (++cmd) + 32);
            continue;
        }

        switch (opcode) {
            case G_RDPPIPESYNC:
                gfx_flush();
                break;

            case G_MTX:
                gfx_sp_matrix(C0(16, 8), (const void*) seg_addr(cmd->words.w1));
                break;

            case (uint8_t) G_POPMTX:
                gfx_sp_pop_matrix();
                break;

            case G_MOVEMEM:
                // if (C0(16, 8) >= 0x80)
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

                    /* mimic the per-call prologue behaviour for Great Fox */
                    ending_great_fox = 0;
                    if ((cmd == seg_addr(aGreatFoxDamagedDL)) || (cmd == seg_addr(aGreatFoxIntactDL))) {
                        if (gGameState == 8)
                            ending_great_fox = 1;
                    }

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
                ending_great_fox = 0;
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

            case (uint8_t) G_QUAD:
                gfx_sp_tri1(C1(17, 7), C1(9, 7), C1(25, 7));
                gfx_sp_tri1(C1(9, 7), C1(1, 7), C1(25, 7));
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

static void gfx_sp_reset() {
    rsp.modelview_matrix_stack_size = 0;
    rendering_state.textures[0]->cms = 6;
    rendering_state.textures[0]->cmt = 6;
    rendering_state.textures[1]->cms = 6;
    rendering_state.textures[1]->cmt = 6;
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
    gfx_current_dimensions.aspect_ratio = (float) gfx_current_dimensions.width / (float) gfx_current_dimensions.height;

    oops_texture_id = gfx_rapi->new_texture();
    gfx_rapi->select_texture(0, oops_texture_id);
    gfx_rapi->upload_texture((uint8_t*) rgba16_buf, 64, 64, GL_UNSIGNED_SHORT_1_5_5_5_REV);

    memset(&oops_node, 0, sizeof(oops_node));
    oops_node.texture_id = oops_texture_id;

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

    // if (!gfx_wapi->start_frame()) {
    //	dropped_frame = 1;
    //	return;
    // }

    // dropped_frame = 0;

    gfx_rapi->start_frame();
    gfx_run_dl(commands);
    gfx_flush();
    gfx_rapi->end_frame();
    gfx_wapi->swap_buffers_begin();
}

void gfx_end_frame(void) {
    // if (!dropped_frame) {
    gfx_rapi->finish_render();
    gfx_wapi->swap_buffers_end();
    //}
}