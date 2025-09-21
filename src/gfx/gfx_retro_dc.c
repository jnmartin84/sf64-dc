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

#define u32 uint32_t
#define s32 int32_t
#define u16 uint16_t
#define s16 int16_t


#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif
#include <PR/gbi.h>

#define	G_TX_LOADTILE	7
#define	G_TX_RENDERTILE	0

#define	G_TX_NOMIRROR	0
#define	G_TX_WRAP	0
#define	G_TX_MIRROR	0x1
#define	G_TX_CLAMP	0x2
#define	G_TX_NOMASK	0
#define	G_TX_NOLOD	0



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
extern int in_intro;

uint32_t last_set_texture_image_width;
int draw_rect;
uint32_t oops_texture_id;
int do_rectdepfix = 0;
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

#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 240
#define HALF_SCREEN_WIDTH (SCREEN_WIDTH / 2)
#define HALF_SCREEN_HEIGHT (SCREEN_HEIGHT / 2)

#define RATIO_X (gfx_current_dimensions.width / (2.0f * HALF_SCREEN_WIDTH))
#define RATIO_Y (gfx_current_dimensions.height / (2.0f * HALF_SCREEN_HEIGHT))
uint8_t er,eg,eb,ea;
uint8_t pr,pg,pb,pa;

// enough to submit the nintendo logo in one go
//#define MAX_BUFFERED (384)
// need to save some memory
#define MAX_BUFFERED 128
#define MAX_LIGHTS 8
#define MAX_VERTICES 64
static uint32_t LightGetHSV(uint8_t r, uint8_t g, uint8_t b);

static uint32_t LightGetRGB(uint8_t h, uint8_t s, uint8_t v);

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


struct __attribute__((aligned(32))) LoadedVertex {
	// 0
	float x, y, z, w;
	// 16
	float _x, _y, _z, _w;
	// 32
	float u, v;
	// 40
	struct RGBA color;
	// 44
	uint8_t clip_rej;
	// 45
	uint8_t	wlt0;
	//
	uint8_t lit;
};


static inline uint32_t pack_key(uint32_t a, uint8_t d, uint8_t e, uint8_t pal) {
    uint32_t key = 0;
    key |= ((uint32_t)((a >> 5) & 0x7FFFF)) << 12; // 19 bits
    key |= ((uint32_t)d & 0xFF)   << 4;            // 8 bits
    key |= ((uint32_t)(e & 0x3))<<2;               // 2 bits
    key |= ((uint32_t)(pal & 0x3));                // 2 bits
	return key;
}


// exactly one cache-line in size
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

#if 0
	// 8
	const uint8_t* texture_addr;
	// 12
	uint32_t tmem;
	// 16
	uint16_t uls;
	// 18
	uint16_t ult;
	// 20
	uint8_t fmt, siz;
	// 22
	uint8_t dirty;
	// 23
	uint8_t linear_filter;
	// 24
	uint8_t cms, cmt;
	// 26
	uint8_t pal;
	// 27
	uint8_t pad11;
	// 28
	uint32_t pad41;
#endif
};

uint32_t last_palette = 0;

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
	struct __attribute__((aligned(32))) dc_fast_t loaded_vertices_2D[4];

	float modelview_matrix_stack[11][4][4] __attribute__((aligned(32)));

	float MP_matrix[4][4] __attribute__((aligned(32)));

	float P_matrix[4][4] __attribute__((aligned(32)));

	Light_t __attribute__((aligned(32))) current_lights[MAX_LIGHTS + 1];

	float __attribute__((aligned(32))) current_lights_coeffs[MAX_LIGHTS][3];
	Light_t __attribute__((aligned(32))) lookat[2];

	float __attribute__((aligned(32))) current_lookat_coeffs[2][3]; // lookat_x, lookat_y

	uint32_t __attribute__((aligned(32))) geometry_mode;
	struct {
		// U0.16
		uint16_t s, t;
	} texture_scaling_factor;
	int16_t fog_mul, fog_offset;
	uint8_t modelview_matrix_stack_size;
	// 866
	uint8_t current_num_lights;        // includes ambient light
	// 867
	uint8_t lights_changed;
} rsp __attribute__((aligned(32)));

static struct RDP {
	const uint8_t* palette;
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
		uint8_t shifts,shiftt;
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
	uint32_t pad;
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

static  __attribute__((noinline)) void gfx_flush(void) {
	if ((buf_vbo_len > 2) && ((buf_vbo_len % 3) == 0)) {
		gfx_rapi->draw_triangles((void*) buf_vbo, buf_vbo_len, buf_vbo_num_tris);
		buf_vbo_len = 0;
		buf_num_vert = 0;
		buf_vbo_num_tris = 0;
	}
}

static  __attribute__((noinline)) struct ShaderProgram* gfx_lookup_or_create_shader_program(uint32_t shader_id) {
	struct ShaderProgram* prg = gfx_rapi->lookup_shader(shader_id);
	if (prg == NULL) {
		gfx_rapi->unload_shader(rendering_state.shader_program);
		prg = gfx_rapi->create_and_load_new_shader(shader_id);
		rendering_state.shader_program = prg;
	}
	return prg;
}

void n64_memcpy(void *dst, const void *src, size_t size);

static  __attribute__((noinline)) void gfx_generate_cc(struct ColorCombiner* comb, uint32_t cc_id) {
	uint8_t c[2][4];
	uint32_t shader_id = cc_id & 0xff000000;//(cc_id >> 24) << 24;
	uint8_t shader_input_mapping[2][4] = { { 0 } };
	int i, j;

	for (i = 0; i < 4; i++) {
//		c[0][i] = (cc_id >> (i * 3)) & 7;
//		c[1][i] = (cc_id >> (12 + i * 3)) & 7;
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
//			shader_id |= val << (i * 12 + j * 3);
			shader_id |= val << (((i << 3) + (i << 2)) + ((j << 1) + j));
		}
	}
	comb->cc_id = cc_id;
	comb->prg = gfx_lookup_or_create_shader_program(shader_id);
	n64_memcpy(comb->shader_input_mapping, shader_input_mapping, sizeof(shader_input_mapping));
}

static  __attribute__((noinline)) struct ColorCombiner* gfx_lookup_or_create_color_combiner(uint32_t cc_id) {
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


#if 0
struct TextureHashmapNode {
	// 0
	struct TextureHashmapNode* next;
	// 4
	uint32_t texture_id;
	// 8
	const uint8_t* texture_addr;
	// 12
	uint32_t tmem;
	// 16
	uint16_t uls;
	// 18
	uint16_t ult;
	// 20
	uint8_t fmt, siz;
	// 22
	uint8_t dirty;
	// 23
	uint8_t linear_filter;
	// 24
	uint8_t cms, cmt;
	// 26
	uint8_t pal;
	// 27
	uint8_t pad11;
	// 28
	uint32_t pad41;
};
#endif
void reset_texcache(void) {
	gfx_texture_cache.pool_pos = 0;
//	printf("BEGIN DUMP !!!\n\n");
//	for (uint32_t i=0;i<gfx_texture_cache.pool_pos;i++) {
//		if (gfx_texture_cache.pool[i].pad11) {
//			if (!gfx_texture_cache.pool[i].pad41) {
//				printf("0x%08x %u %u %u %u %u %u \n", gfx_texture_cache.pool[i].texture_addr,
//				gfx_texture_cache.pool[i].tmem, gfx_texture_cache.pool[i].uls, gfx_texture_cache.pool[i].ult,
//			gfx_texture_cache.pool[i].fmt, gfx_texture_cache.pool[i].siz, gfx_texture_cache.pool[i].pal );
//			}
//		}
//	}
//	printf("END DUMP !!!\n\n");
	memset(&gfx_texture_cache, 0, sizeof(gfx_texture_cache));
//	memset(gfx_texture_cache.hashmap, 0, sizeof(gfx_texture_cache.hashmap));
}


static inline uint32_t unpack_A(uint64_t key) {
    // Extract 19-bit field
    uint32_t a19 = (uint32_t)((key >> 12) & 0x7FFFF);

    // Shift back to original position (restore the lost low 5 bits as 0s)
//    uint32_t a = (a19 << 5);

    // Restore the constant top 8 bits (0x8C per your earlier note)
//    a |= 0x8C000000;

    return a19;
}

static inline uint64_t hash64(uint64_t key) {
    uint64_t h = 1469598103934665603ULL; // FNV offset basis
    h ^= key; 
    h *= 1099511628211ULL;               // FNV prime
    return h;
}

void *virtual_to_segmented(const void *addr);
void gfx_texture_cache_invalidate(void* orig_addr) {
 	void* segaddr = segmented_to_virtual(orig_addr);

	size_t hash = (uintptr_t) segaddr;
	hash = (hash >> 5) & 0x3ff;

	uintptr_t addrcomp = ((uintptr_t)segaddr>>5)&0x7FFFF;

	struct TextureHashmapNode** node = &gfx_texture_cache.hashmap[hash];
	uintptr_t last_node = &gfx_texture_cache.pool[gfx_texture_cache.pool_pos];
	while (*node != NULL && ((uintptr_t)*node < last_node)) {
		struct TextureHashmapNode* cur_node = (*node);
		__builtin_prefetch(cur_node->next);
//		if (cur_node->texture_addr == segaddr) {
		uintptr_t unpaddr = (uintptr_t)unpack_A((*node)->key);
		if (unpaddr == addrcomp) {
			if (cur_node->dirty)
				return;
//			printf("inval %d (%08x)\n", cur_node->texture_id, virtual_to_segmented(segaddr));
			cur_node->dirty = 1;
//			cur_node->pad41++;
		}
		node = &cur_node->next;
	}
}


void gfx_opengl_replace_texture(const uint8_t* rgba32_buf, int width, int height, unsigned int type);


extern void gfx_opengl_set_tile_addr(int tile, GLuint addr);


static  __attribute__((noinline)) uint8_t gfx_texture_cache_lookup(int tile, struct TextureHashmapNode** n, const uint8_t* orig_addr,
										uint32_t tmem, uint32_t fmt, uint32_t siz, uint8_t pal) {
	void* segaddr = segmented_to_virtual((void *)orig_addr);
	size_t hash = (uintptr_t) segaddr;
	hash = (hash >> 5) & 0x3ff;

	uint32_t newkey = pack_key(segaddr, fmt, siz, pal);

	struct TextureHashmapNode** node = &gfx_texture_cache.hashmap[hash];
	if ((uintptr_t)rdp.loaded_texture[tile].addr < 0x8c010000u) {
		gfx_rapi->select_texture(tile, oops_texture_id);
		*node = &oops_node;
		printf("invalid texturwe address\n");
		return 3;
	}
	__builtin_prefetch(*node);

	uintptr_t last_node = &gfx_texture_cache.pool[gfx_texture_cache.pool_pos];
	while (*node != NULL && ((uintptr_t)*node < last_node)) {
		__builtin_prefetch((*node)->next);

		//if ((*node)->texture_addr == segaddr && (*node)->fmt == fmt && (*node)->siz == siz && (*node)->pal == pal) {
		if ((*node)->key == newkey) {
			gfx_rapi->select_texture(tile, (*node)->texture_id);
			gfx_opengl_set_tile_addr(tile, segaddr);

			if ((*node)->dirty) {
				(*node)->dirty = 0;
				return 2;
			} else {
				*n = *node;
				return 1;
			}
		}
		node = &(*node)->next;
	}

	if (gfx_texture_cache.pool_pos == sizeof(gfx_texture_cache.pool) / sizeof(struct TextureHashmapNode)) {
		// jnmartin84
		// Pool is full. We have a backup texture that is empty for this...
		printf("exhausted pool\n");
		exit(-1);
		gfx_rapi->select_texture(tile, oops_texture_id);
		*node = &oops_node;
		return 1;
	}

	*node = &gfx_texture_cache.pool[gfx_texture_cache.pool_pos++];
	//if ((*node)->texture_addr == NULL) {
//	if ((*node)->key == 0) {
	(*node)->texture_id = gfx_rapi->new_texture();
//	}
	gfx_rapi->select_texture(tile, (*node)->texture_id);
	gfx_opengl_set_tile_addr(tile, segaddr);

	gfx_rapi->set_sampler_parameters(tile, 0, 0, 0);

//	(*node)->texture_addr = segaddr;
//	(*node)->fmt = fmt;
//	(*node)->siz = siz;
/* 	(*node)->uls = uls;
	(*node)->ult = ult; */
//	(*node)->pal = pal;

	(*node)->key = newkey;

	(*node)->dirty = 0;

//	(*node)->tmem = tmem;
	(*node)->cms = 0;
	(*node)->cmt = 0;
	(*node)->linear_filter = 0;
	(*node)->next = NULL;
//	(*node)->pad11 = 1;
	*n = *node;

	return 0;
}

// enough for a 64 * 64 16-bit image, no overflow should happen at this point
uint16_t __attribute__((aligned(32))) rgba16_buf[512 * 256/* 4096 * 8 */];
uint8_t __attribute__((aligned(32))) xform_buf[8192];

int last_cl_rv;
static void __attribute__((noinline)) import_texture(int tile);

#include "sh4zam.h"

/* Piecewise sRGB transfer: linear [0..1] -> sRGB [0..1] */
static inline float linear_to_srgb1(float x) {
	return shz_sqrtf_fsrra(x + 0.000000001f);
}

uint8_t __attribute__((aligned(32))) table256[256] = {
0
};


uint8_t __attribute__((aligned(32))) table32[32] = {
    0, 6, 8, 10, 11, 12, 14, 15,
    16, 17, 18, 18, 19, 20, 21, 22,
    22, 23, 24, 24, 25, 26, 26, 27,
    27, 28, 28, 29, 29, 30, 30, 31
};


uint8_t __attribute__((aligned(32))) table16[16] = {
    0, 4, 5, 7, 8, 9, 9, 10,
    11, 12, 12, 13, 13, 14, 14, 15
};

static uint16_t brightit_argb1555(uint16_t c) {
//	return c;
#if 1
//	__builtin_prefetch(table32);
	uint8_t a = (c >> 15) &    1;
	uint8_t r = (c >> 10) & 0x1f;
	uint8_t g = (c >>  5) & 0x1f;
	uint8_t b = (c      ) & 0x1f;

//	float r01 = (float)r * recip31;
//	float g01 = (float)g * recip31;
//	float b01 = (float)b * recip31;

//	r01 = linear_to_srgb1(r01);
//	g01 = linear_to_srgb1(g01);
//	b01 = linear_to_srgb1(b01);

//	r = (uint8_t)(r01 * 31.0f);
//	g = (uint8_t)(g01 * 31.0f);
//	b = (uint8_t)(b01 * 31.0f);

	return (a << 15) | (table32[r] << 10) | (table32[g] << 5) | (table32[b]);
#endif
}
extern float Rand_ZeroOne(void);

static void import_texture_rgba16_alphanoise(int tile) {
	uint32_t i;
	uint32_t width = rdp.texture_tile.line_size_bytes >> 1;
	uint32_t height = (uint32_t)((float)rdp.loaded_texture[tile].size_bytes / (float)rdp.texture_tile.line_size_bytes);
	
	if (last_set_texture_image_width == 0) {
		uint32_t loopcount = rdp.loaded_texture[tile].size_bytes >> 1;
		uint16_t* start = (uint16_t *)rdp.loaded_texture[tile].addr;
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
	} else {
		u32 src_width = last_set_texture_image_width + 1;
		uint32_t somewidth = src_width;
		if (width <= ((src_width >> 1) + 4)) {
			somewidth = width;
		} else {
			if (width == 20 && last_set_texture_image_width == 30) { 
				somewidth = width - 4;
			}
		}

		uint16_t* start = (uint16_t*) &rdp.loaded_texture[tile]
					.addr[(((rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC)) << 1) +
						(((((rdp.texture_tile.ult) >> G_TEXTURE_IMAGE_FRAC)) * (src_width)) << 1)];

		uint16_t *tex16 = rgba16_buf;
		for (i = 0; i < height; i++) {
			for (uint32_t x = 0; x < somewidth; x++) {
				uint16_t col16 = start[x];
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
			start += src_width;
		}

		width = somewidth;
	}

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_4_4_4_4_REV);
}
extern uint16_t scaled2[];
int do_the_blur = 0;

static void import_texture_rgba16(int tile) {
	uint32_t i;

	if (do_the_blur) {
		gfx_rapi->upload_texture((uint8_t*) scaled2, 64, 64, GL_UNSIGNED_SHORT_1_5_5_5_REV);
		return;
	}

	__builtin_prefetch(table32);
	uint32_t width = rdp.texture_tile.line_size_bytes >> 1;
	uint32_t height = (uint32_t)((float)rdp.loaded_texture[tile].size_bytes / (float)rdp.texture_tile.line_size_bytes);

//	if (last_set_texture_image_width == 0) {
		uint32_t loopcount = rdp.loaded_texture[tile].size_bytes >> 1;
		uint16_t* start = (uint16_t *)rdp.loaded_texture[tile].addr;
		for (i = 0; i < loopcount; i++) {
			uint16_t col16 = start[i];
			rgba16_buf[i] = brightit_argb1555(((col16 & 1) << 15) | (col16 >> 1));
		}
//	} 
/* else {
		u32 src_width = last_set_texture_image_width + 1;
		uint32_t somewidth = src_width;
		if (width <= ((src_width >> 1) + 4)) {
			somewidth = width;
		}

		uint16_t* start = (uint16_t*) &rdp.loaded_texture[tile]
					.addr[(((rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC)) << 1) +
						(((((rdp.texture_tile.ult) >> G_TEXTURE_IMAGE_FRAC)) * (src_width)) << 1)];

		uint16_t *tex16 = rgba16_buf;
		for (i = 0; i < height; i++) {
			for (uint32_t x = 0; x < somewidth; x++) {
				uint16_t col16 = start[x];
					*tex16++ = brightit_argb1555(((col16 & 1) << 15) | (col16 >> 1));
			}
			start += src_width;
		}

		width = somewidth;
	} */

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
}


static void import_texture_rgba32(int tile) {
	uint32_t width = rdp.texture_tile.line_size_bytes >> 1;
//	uint32_t height = (rdp.loaded_texture[tile].size_bytes >> 1) / rdp.texture_tile.line_size_bytes;
	uint32_t height = (uint32_t)((float)(rdp.loaded_texture[tile].size_bytes >> 1) / (float)rdp.texture_tile.line_size_bytes);

	uint32_t wxh = width*height;
	__builtin_prefetch(table16);
	if (((uintptr_t)rdp.loaded_texture[tile].addr & 3) == 0) {
		// fast path
		uint32_t *addr32 = (uint32_t *)rdp.loaded_texture[tile].addr;
		for (uint32_t i = 0; i < wxh; i++) {
			uint32_t nc = addr32[i];
			uint8_t r = (nc >> 28) & 0x0f;
			uint8_t g = (nc >> 20) & 0x0f;
			uint8_t b = (nc >> 12) & 0x0f;
			uint8_t a = (nc >>  4) & 0x0f;
#if 0
			float r01 = (float)r * recip255;
			float g01 = (float)g * recip255;
			float b01 = (float)b * recip255;

			r01 = linear_to_srgb1(r01);
			g01 = linear_to_srgb1(g01);
			b01 = linear_to_srgb1(b01);

			r = (uint8_t)(r01 * 15.0f);
			g = (uint8_t)(g01 * 15.0f);
			b = (uint8_t)(b01 * 15.0f);
#endif
			r = table16[r];
			g = table16[g];
			b = table16[b];
//			a = (a >> 4) & 0xf;	

			rgba16_buf[i] = (a << 12) | (r << 8) | (g << 4) | (b);
		}
	} else {
		// slower path
		for (uint32_t i = 0; i < wxh; i++) {
			uint32_t itimes4 = i << 2;
			uint8_t a = ((rdp.loaded_texture[tile].addr[itimes4 + 0]) >> 4) & 0x0f;
			uint8_t b = table16[((rdp.loaded_texture[tile].addr[itimes4 + 1]) >> 4) & 0x0f];
			uint8_t g = table16[((rdp.loaded_texture[tile].addr[itimes4 + 2]) >> 4) & 0x0f];
			uint8_t r = table16[((rdp.loaded_texture[tile].addr[itimes4 + 3]) >> 4) & 0x0f];
#if 0
			float r01 = (float)r * recip255;
			float g01 = (float)g * recip255;
			float b01 = (float)b * recip255;

			r01 = linear_to_srgb1(r01);
			g01 = linear_to_srgb1(g01);
			b01 = linear_to_srgb1(b01);

			r = (uint8_t)(r01 * 15.0f);
			g = (uint8_t)(g01 * 15.0f);
			b = (uint8_t)(b01 * 15.0f);
			a = (a >> 4) & 0xf;	
#endif
			rgba16_buf[i] = (a << 12) | (r << 8) | (g << 4) | (b);
		}
	}
	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_4_4_4_4_REV);
}


static void import_texture_ia4(int tile) {
	uint32_t width = rdp.texture_tile.line_size_bytes << 1;
//	uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;
	uint32_t height = (uint32_t)((float)rdp.loaded_texture[tile].size_bytes / (float)rdp.texture_tile.line_size_bytes);
	uint32_t i;
__builtin_prefetch(table32);
	if (last_set_texture_image_width == 0) {
		uint8_t byte = 0;
		uint8_t part;
		uint8_t intensity;
		uint8_t alpha;
		float intf;

		for (i = 0; i < rdp.loaded_texture[tile].size_bytes << 1; i++) {
			if (!(i & 1))
				byte = rdp.loaded_texture[tile].addr[i >> 1];
			part = (byte >> (4 - ((i & 1) << 2))) & 0xf;
			intensity = table32[(SCALE_3_8(part >> 1) >> 3) & 0x1f];
			alpha = part & 1;
#if 0
			intf = (float)intensity * recip31;
			intf = linear_to_srgb1(intf);
			intensity = intf * 31.0f;
#endif
			
			rgba16_buf[i] = (alpha << 15) | (intensity << 10) | (intensity << 5) | (intensity);
		}
	} else {
		uint8_t* start =
			(uint8_t*) &rdp.loaded_texture[tile]
				.addr[(((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC) >> 1) * (last_set_texture_image_width + 1)) +
					  ((rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC) >> 1)];
		uint8_t *tex8 = xform_buf;
		for (uint32_t i = 0; i < height; i++) {
			for (uint32_t x = 0; x < (last_set_texture_image_width + 1) << 1; x += 2) {
				uint32_t sidx = x>>1;
				if (i & 1) {
					tex8[(x)] = (start[(sidx)] & 0xf);
					tex8[(x) + 1] = (start[(sidx)] >> 4) & 0xf;

				} else {
					tex8[(x)] = (start[(sidx)] >> 4) & 0xf;
					tex8[(x) + 1] = (start[(sidx)] & 0xf);
				}
			}
			start += (last_set_texture_image_width + 1);
			tex8 += (last_set_texture_image_width + 1);
		}
		for (uint32_t i = 0; i < rdp.loaded_texture[tile].size_bytes << 1; i++) {
			uint8_t byte = xform_buf[i];
			uint8_t intensity = table32[(SCALE_3_8(byte >> 1) >> 3) & 0x1f];
			uint8_t alpha = byte & 1;
#if 0
			float intf = (float)intensity * recip31;
			intf = linear_to_srgb1(intf);
			intensity = intf * 31.0f;
#endif
			uint16_t col16 = (alpha << 15) | (intensity << 10) | (intensity << 5) | (intensity);
			rgba16_buf[i] = col16;
		}
	}

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
}


static void import_texture_ia8(int tile) {
	uint32_t width = rdp.texture_tile.line_size_bytes;
//	uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;
	uint32_t height = (uint32_t)((float)rdp.loaded_texture[tile].size_bytes / (float)rdp.texture_tile.line_size_bytes);
	__builtin_prefetch(table16);
	uint8_t* start = (uint8_t*) rdp.loaded_texture[tile]
						 .addr;

	uint16_t *tex16 = rgba16_buf;
	for (uint32_t i = 0; i < rdp.loaded_texture[tile].size_bytes; i++) {
			uint8_t val = start[i];
			uint8_t in = table16[((val >> 4) & 0xf)];
			uint8_t al = (val & 0xf);
#if 0
			float intf = (float)in * recip15;
			intf = linear_to_srgb1(intf);
			in = intf * 15.0f;
#endif
			tex16[i] = (al << 12) | (in << 8) | (in << 4) | in;
	}
	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_4_4_4_4_REV);
}


static void import_texture_ia16(int tile) {
	uint32_t i;
	uint32_t width = rdp.texture_tile.line_size_bytes >> 1;
	__builtin_prefetch(table16);
//	uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;
	uint32_t height = (uint32_t)((float)rdp.loaded_texture[tile].size_bytes / (float)rdp.texture_tile.line_size_bytes);
	u32 src_width;
	if (last_set_texture_image_width) {
		src_width = last_set_texture_image_width + 1;
	} else {
		src_width = width;
	}

	uint16_t* start = (uint16_t *)rdp.loaded_texture[tile].addr;
	if (last_set_texture_image_width) {
		start =
			(uint16_t*) &rdp.loaded_texture[tile]
				.addr[((((rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC))) << 1) +
					  (((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC)) * ((src_width) << 1))];
	}

	uint16_t *tex16 = rgba16_buf;
	for (i = 0; i < height; i++) {
		for (uint32_t x = 0; x < src_width; x++) {
			uint16_t np = start[x];
			np = (np << 8) | ((np >> 8) & 0xff);
			uint8_t al = (np >> 12) & 0xf;
			uint8_t in = table16[(np >> 4) & 0xf];
#if 0
			float intf = (float)in * recip15;
			intf = linear_to_srgb1(intf);
			in = intf * 15.0f;
#endif
			tex16[x] = (al << 12) | (in << 8) | (in << 4) | in;
		}
		start += src_width;
		tex16 += src_width;
	}

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, src_width, height, GL_UNSIGNED_SHORT_4_4_4_4_REV);
}


static void import_texture_i4(int tile) {
	uint32_t i;
	uint32_t *rgba32_buf = rgba16_buf;
	uint32_t width = rdp.texture_tile.line_size_bytes * 2;
//	uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;
	uint32_t height = (uint32_t)((float)rdp.loaded_texture[tile].size_bytes / (float)rdp.texture_tile.line_size_bytes);
	height = (height + 3) & ~3;
	__builtin_prefetch(table16);

	if (last_set_texture_image_width == 0) {
		for (i = 0; i < rdp.loaded_texture[tile].size_bytes; i++) {
//			uint16_t idx = (i<<1);
			uint8_t byte = rdp.loaded_texture[tile].addr[i];
			uint8_t part1,part2;
			uint8_t p1s,p2s;
			part1 = (byte >> 4) & 0xf;
#if 0
			float intf = (float)part1 * recip15;
			intf = linear_to_srgb1(intf);
			p1s = intf * 15.0f;
#endif
			p1s = table16[part1];
			part2 = byte & 0xf;
#if 0
			intf = (float)part2 * recip15;
			intf = linear_to_srgb1(intf);
			p2s = intf * 15.0f;
#endif
			p2s = table16[part2];
			// todo 32-bit write
			uint16_t c1 = (part1 << 12) | (p1s << 8) | (p1s << 4) | p1s;
			uint16_t c2 = (part2 << 12) | (p2s << 8) | (p2s << 4) | p2s;
			uint32_t c = (c2 << 16) | c1;
			rgba32_buf[i] = c;
//			rgba16_buf[idx    ] = 
//			rgba16_buf[idx + 1] = 
		}
	} else {
		uint8_t* start =
			(uint8_t*) &rdp.loaded_texture[tile]
				.addr[(((((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC)-1)>>1) * (width>>1))) +
					  (((rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC)-1)>>1)];
		for (uint32_t i = 0; i < height; i++) {
			uint32_t iw = i * width;
			for (uint32_t x = 0; x < (last_set_texture_image_width + 1); x ++) {
				uint8_t startin = start[x];
				uint8_t in = (startin >> 4) & 0xf;
				uint8_t in2 = startin & 0xf;

				uint8_t p1s,p2s;
#if 0
				float intf = (float)in * recip15;
				intf = linear_to_srgb1(intf);
				p1s = intf * 15.0f;

				intf = (float)in2 * recip15;
				intf = linear_to_srgb1(intf);
				p2s = intf * 15.0f;
#endif
				p1s = table16[in];
				p2s = table16[in2];

				// todo 32-bit write
				uint16_t c1 = (in << 12) | (p1s << 8) | (p1s << 4) | p1s;
				uint16_t c2 = (in2 << 12) | (p2s << 8) | (p2s << 4) | p2s;
				uint32_t c = (c2 << 16) | c1;
				rgba32_buf[i + x] = c;
//				rgba16_buf[iw + x    ] = (in  << 12) | (p1s << 8) | (p1s << 4) | p1s;
//				rgba16_buf[iw + x + 1] = (in2 << 12) | (p2s << 8) | (p2s << 4) | p2s;
			}
			start += (last_set_texture_image_width + 1);
		}
	}

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_4_4_4_4_REV);
}


static void import_texture_i8(int tile) {
	uint32_t width = rdp.texture_tile.line_size_bytes;
//	uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;
	uint32_t height = (uint32_t)((float)rdp.loaded_texture[tile].size_bytes / (float)rdp.texture_tile.line_size_bytes);
	__builtin_prefetch(table16);
	uint8_t* start = (uint8_t*) &rdp.loaded_texture[tile]
						 .addr[((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC) * (last_set_texture_image_width + 1)) +
							   (rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC)];

	u32 src_width;
	if (last_set_texture_image_width) {
		src_width = last_set_texture_image_width + 1;
	} else {
		src_width = width;
	}
	for (uint32_t i = 0; i < height; i++) {
		for (uint32_t x = 0; x < src_width; x++) {
			xform_buf[(i * width) + x] = start[x];
		}
		start += (src_width);
	}

	for (uint32_t i = 0; i < rdp.loaded_texture[tile].size_bytes; i++) {
		uint8_t in = (xform_buf[i] >> 4) & 0xf;
#if 0
		float intf = (float)in * recip15;
		intf = linear_to_srgb1(intf);
		uint8_t ins = intf * 15.0f;
#endif
		uint8_t ins = table16[in];
		rgba16_buf[i] = (in << 12) | (ins << 8) | (ins << 4) | ins;
	}

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_4_4_4_4_REV);
}


static void import_texture_ci4(int tile) {
	uint32_t width = rdp.texture_tile.line_size_bytes << 1;
//	uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;
	uint32_t height = (uint32_t)((float)rdp.loaded_texture[tile].size_bytes / (float)rdp.texture_tile.line_size_bytes);
	uint32_t i;
	uint8_t offs = last_palette << 4;

	uint16_t *offs_tlut = (uint16_t *)&tlut[offs];

	__builtin_prefetch(offs_tlut);
	if (__builtin_expect((last_set_texture_image_width == 0),1)) {
		uint32_t *rgba32_buf = (uint32_t *)rgba16_buf;
		uint8_t part1,part2;
		uint8_t byte;
//		for (i = 0; i < rdp.loaded_texture[tile].size_bytes * 2; i+=2) {
		for (i = 0; i < rdp.loaded_texture[tile].size_bytes; i++) {
//			if (((i*2)+32) & 31 == 0) {
//				__builtin_prefetch(&rdp.loaded_texture[tile].addr[((i*2)+32) >> 1]);
//			}
			uint8_t byte = rdp.loaded_texture[tile].addr[i];
			part1 = (byte >> 4) & 0xf;
			part2 = byte & 0xf;
//			rgba16_buf[(i<<1)    ] = offs_tlut[part1];
//			rgba16_buf[(i<<1) + 1] = offs_tlut[part2];
			rgba32_buf[i] = (offs_tlut[part2] << 16) | offs_tlut[part1];
		}
	} else {
		uint32_t widthdiv2 = width >> 1;
		uint8_t byte;
		uint8_t* start =
			(uint8_t*) &rdp.loaded_texture[tile]
				.addr[(((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC) >> 1) * (last_set_texture_image_width + 1)) +
					  ((rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC) >> 1)];
		for (uint32_t i = 0; i < height; i++) {
			for (uint32_t x = 0; x < (last_set_texture_image_width + 1); x++ ) {
				uint32_t xtimes2 = x<<1;
				byte = start[x];
				uint32_t index = (i * (widthdiv2)) + (xtimes2);
				if (i & 1) {
					xform_buf[index] = (byte & 0xf);
					xform_buf[index + 1] = (byte >> 4) & 0xf;
				} else {
					xform_buf[index] = (byte >> 4) & 0xf;
					xform_buf[index + 1] = (byte & 0xf);
				}
				rgba16_buf[index    ] = offs_tlut[xform_buf[index    ]];
				rgba16_buf[index + 1] = offs_tlut[xform_buf[index + 1]];
			}
			start += (last_set_texture_image_width + 1);
		}
#if 0
		for (uint32_t i = 0; i < rdp.loaded_texture[tile].size_bytes * 2; i+=4) {
			rgba16_buf[i    ] = offs_tlut[xform_buf[i + 0]];
			rgba16_buf[i + 1] = offs_tlut[xform_buf[i + 1]];
			rgba16_buf[i + 2] = offs_tlut[xform_buf[i + 2]];
			rgba16_buf[i + 3] = offs_tlut[xform_buf[i + 3]];
		}
#endif
	}

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
}


static __attribute__((noinline)) void import_texture_ci8(int tile) {
	uint32_t width = rdp.texture_tile.line_size_bytes;
//	uint32_t height = rdp.loaded_texture[tile].size_bytes / rdp.texture_tile.line_size_bytes;
	uint32_t height = (uint32_t)((float)rdp.loaded_texture[tile].size_bytes / (float)rdp.texture_tile.line_size_bytes);

	__builtin_prefetch(tlut);
	if (__builtin_expect((last_set_texture_image_width == 0),1)) {
		uint8_t *tex8 = rdp.loaded_texture[tile].addr;
		if (__builtin_expect((!((uintptr_t)tex8 & 3)),1)) {
			uint32_t *intex32 = (uint32_t *)tex8;
			__builtin_prefetch(intex32);
			uint32_t count = rdp.loaded_texture[tile].size_bytes;
			uint32_t *tex32 = (uint32_t)rgba16_buf;
			for (uint32_t i = 0; i < count; i+=8) {
				__builtin_prefetch((void*)(((uintptr_t)intex32&0xffffffe0)+32));

				uint32_t fourpix1 = *intex32++;
				uint32_t fourpix2 = *intex32++;

				asm volatile("": : : "memory");

				uint16_t t1,t2,t3,t4,t5,t6,t7,t8;

				t4 = tlut[(fourpix1 >> 24) & 0xff];
				t3 = tlut[(fourpix1 >> 16) & 0xff];
				t2 = tlut[(fourpix1 >>  8) & 0xff];
				t1 = tlut[(fourpix1      ) & 0xff];
				t8 = tlut[(fourpix2 >> 24) & 0xff];
				t7 = tlut[(fourpix2 >> 16) & 0xff];
				t6 = tlut[(fourpix2 >>  8) & 0xff];
				t5 = tlut[(fourpix2      ) & 0xff];

				asm volatile("": : : "memory");

				*tex32++ = (t2 << 16) | t1;
				*tex32++ = (t4 << 16) | t3;
				*tex32++ = (t6 << 16) | t5;
				*tex32++ = (t8 << 16) | t7;

			}
		} else {
			__builtin_prefetch(tex8);
			uint32_t count = rdp.loaded_texture[tile].size_bytes;
			uint32_t *tex32 = (uint32_t)rgba16_buf;
			for (uint32_t i = 0; i < count; i+=8) {
				uint16_t t1,t2,t3,t4,t5,t6,t7,t8;
				__builtin_prefetch(tex8+16);
				t1 = tlut[*tex8++];
				t2 = tlut[*tex8++];
				t3 = tlut[*tex8++];
				t4 = tlut[*tex8++];
				t5 = tlut[*tex8++];
				t6 = tlut[*tex8++];
				t7 = tlut[*tex8++];
				t8 = tlut[*tex8++];
	#if 1
				asm volatile("": : : "memory");
	#endif

				*tex32++ = (t2 << 16) | t1;
				*tex32++ = (t4 << 16) | t3;
				*tex32++ = (t6 << 16) | t5;
				*tex32++ = (t8 << 16) | t7;
			}
		}
	} else {
		u32 src_width;
		if (last_set_texture_image_width) {
			src_width = last_set_texture_image_width + 1;
		} else {
			src_width = width;
		}

		uint8_t* start = (uint8_t *)(&rdp.loaded_texture[tile]
							.addr[((rdp.texture_tile.ult >> G_TEXTURE_IMAGE_FRAC) * last_set_texture_image_width) +
									(rdp.texture_tile.uls >> G_TEXTURE_IMAGE_FRAC)]);
		uint16_t *tex16 = rgba16_buf;
		for (uint32_t h = 0; h < height; h ++) {
			uint16_t *tx32 = tex16;
			for (uint32_t w = 0; w < src_width; w += 4) {
				uint16_t t1,t2,t3,t4;
				t1 = tlut[*start++];
				t2 = tlut[*start++];
				t3 = tlut[*start++];
				t4 = tlut[*start++];

				*tx32++ = (t2 << 16) | t1;
				*tx32++ = (t4 << 16) | t3;
			}
			tex16 += width;
		}
	}

	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, width, height, GL_UNSIGNED_SHORT_1_5_5_5_REV);
}

int tc_lookup = 0;
int tc_found = 0;
static void __attribute__((noinline)) import_texture(int tile) {
	uint8_t fmt = rdp.texture_tile.fmt;
	uint8_t siz = rdp.texture_tile.siz;

	uint32_t tmem = rdp.texture_to_load.tmem;

	int cl_rv;
	
	tc_lookup++;

	if (alpha_noise)
		gfx_texture_cache_invalidate(rdp.loaded_texture[tile].addr);

	cl_rv = gfx_texture_cache_lookup(tile, &rendering_state.textures[tile], rdp.loaded_texture[tile].addr, tmem,
		fmt, siz, last_palette);
	 
	if (cl_rv == 1) {
		tc_found++;
		return;
	}

	last_cl_rv = cl_rv;

	if (fmt == G_IM_FMT_RGBA) {
		if (siz == G_IM_SIZ_16b) {
			if (!alpha_noise)
				import_texture_rgba16(tile);
			else 
				import_texture_rgba16_alphanoise(tile);
		} else if (siz == G_IM_SIZ_32b) {
			import_texture_rgba32(tile);
		}
	} else if (fmt == G_IM_FMT_IA) {
		if (siz == G_IM_SIZ_4b) {
			import_texture_ia4(tile);
		} else if (siz == G_IM_SIZ_8b) {
			import_texture_ia8(tile);
		} else if (siz == G_IM_SIZ_16b) {
			import_texture_ia16(tile);
		}
	} else if (fmt == G_IM_FMT_CI) {
		if (siz == G_IM_SIZ_4b) {
			import_texture_ci4(tile);
		} else if (siz == G_IM_SIZ_8b) {
			import_texture_ci8(tile);
		}
	} else if (fmt == G_IM_FMT_I) {
		if (siz == G_IM_SIZ_4b) {
			import_texture_i4(tile);
		} else if (siz == G_IM_SIZ_8b) {
			import_texture_i8(tile);
		}
	}
}

#include <kos.h>

static void gfx_normalize_vector(float v[3]) {
	vec3f_normalize(v[0], v[1], v[2]);
}

static void gfx_transposed_matrix_mul(float res[3], const float a[3], const float b[4][4]) {
	res[0] = fipr(a[0],a[1],a[2],0,b[0][0],b[0][1],b[0][2],0);
//	res[1] = fipr(a[0],a[1],a[2],0,b[1][0],b[1][1],b[1][2],0);
	res[1] = a[0] * b[1][0] + a[1] * b[1][1] + a[2] * b[1][2];
	res[2] = fipr(a[0],a[1],a[2],0,b[2][0],b[2][1],b[2][2],0);
}


static void calculate_normal_dir(const Light_t* light, float coeffs[3]) {
	float light_dir[3] = { light->dir[0] * recip127, light->dir[1] * recip127, light->dir[2] * recip127 };
	gfx_transposed_matrix_mul(coeffs, light_dir,
							  (const float (*)[4]) rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1]);
	gfx_normalize_vector(coeffs);
}

// thanks @FalcoGirgis
inline static void fast_mat_store(matrix_t* mtx) {
	asm volatile(
		R"(
			fschg
			add            #64-8,%[mtx]
			fmov.d    xd14,@%[mtx]
			add            #-32,%[mtx]
			pref    @%[mtx]
			add         #32,%[mtx]
			fmov.d    xd12,@-%[mtx]
			fmov.d    xd10,@-%[mtx]
			fmov.d    xd8,@-%[mtx]
			fmov.d    xd6,@-%[mtx]
			fmov.d    xd4,@-%[mtx]
			fmov.d    xd2,@-%[mtx]
			fmov.d    xd0,@-%[mtx]
			fschg
		)"
		: [mtx] "+&r"(mtx), "=m"(*mtx)
		:
		:);
}

// thanks @FalcoGirgis
inline static void fast_mat_load(const matrix_t* mtx) {
	asm volatile(
		R"(
			fschg
			fmov.d    @%[mtx],xd0
			add        #32,%[mtx]
			pref    @%[mtx]
			add        #-(32-8),%[mtx]
			fmov.d    @%[mtx]+,xd2
			fmov.d    @%[mtx]+,xd4
			fmov.d    @%[mtx]+,xd6
			fmov.d    @%[mtx]+,xd8
			fmov.d    @%[mtx]+,xd10
			fmov.d    @%[mtx]+,xd12
			fmov.d    @%[mtx]+,xd14
			fschg
		)"
		: [mtx] "+r"(mtx)
		:
		:);
}

// thanks @FalcoGirgis
inline static void mat_load_apply(const matrix_t* matrix1, const matrix_t* matrix2) {
	unsigned int prefetch_scratch;

	asm volatile("mov %[bmtrx], %[pref_scratch]\n\t"
				 "add #32, %[pref_scratch]\n\t"
				 "fschg\n\t"
				 "pref @%[pref_scratch]\n\t"
				 // back matrix
				 "fmov.d @%[bmtrx]+, XD0\n\t"
				 "fmov.d @%[bmtrx]+, XD2\n\t"
				 "fmov.d @%[bmtrx]+, XD4\n\t"
				 "fmov.d @%[bmtrx]+, XD6\n\t"
				 "pref @%[fmtrx]\n\t"
				 "fmov.d @%[bmtrx]+, XD8\n\t"
				 "fmov.d @%[bmtrx]+, XD10\n\t"
				 "fmov.d @%[bmtrx]+, XD12\n\t"
				 "mov %[fmtrx], %[pref_scratch]\n\t"
				 "add #32, %[pref_scratch]\n\t"
				 "fmov.d @%[bmtrx], XD14\n\t"
				 "pref @%[pref_scratch]\n\t"
				 // front matrix
				 // interleave loads and matrix multiply 4x4
				 "fmov.d @%[fmtrx]+, DR0\n\t"
				 "fmov.d @%[fmtrx]+, DR2\n\t"
				 "fmov.d @%[fmtrx]+, DR4\n\t"
				 "ftrv XMTRX, FV0\n\t"

				 "fmov.d @%[fmtrx]+, DR6\n\t"
				 "fmov.d @%[fmtrx]+, DR8\n\t"
				 "ftrv XMTRX, FV4\n\t"

				 "fmov.d @%[fmtrx]+, DR10\n\t"
				 "fmov.d @%[fmtrx]+, DR12\n\t"
				 "ftrv XMTRX, FV8\n\t"

				 "fmov.d @%[fmtrx], DR14\n\t"
				 "fschg\n\t"
				 "ftrv XMTRX, FV12\n\t"
				 "frchg\n"
				 : [bmtrx] "+&r"((unsigned int) matrix1), [fmtrx] "+r"((unsigned int) matrix2),
				   [pref_scratch] "=&r"(prefetch_scratch)
				 : // no inputs
				 : "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6", "fr7", "fr8", "fr9", "fr10", "fr11", "fr12", "fr13",
				   "fr14", "fr15");
}

static void gfx_matrix_mul(matrix_t res, const matrix_t a, const matrix_t b) {
#if 1
	mat_load_apply((matrix_t *)b, (matrix_t *)a);
	fast_mat_store((matrix_t *)res);
#else
float tmp[4][4];
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            tmp[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + a[i][3] * b[3][j];
        }
    }
    memcpy(res, tmp, sizeof(tmp));
#endif
}

static int matrix_dirty = 0;

void *memcpy32(void *restrict dst, const void *restrict src, size_t bytes);

static __attribute__((noinline)) void gfx_sp_matrix(uint8_t parameters, const void* addr) {
	void* segaddr = (void*) segmented_to_virtual((void*) addr);

	float matrix[4][4] __attribute__((aligned(32)));

#ifndef GBI_FLOATS
	int32_t *saddr = (int32_t *)segaddr;
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
	n64_memcpy(matrix, segaddr, sizeof(matrix));
#endif

	matrix_dirty = 1;

	// the following is specialized for STAR FOX 64 ONLY
	if (parameters & G_MTX_PROJECTION) {
        if (parameters & G_MTX_LOAD) {
            memcpy32(rsp.P_matrix, matrix, sizeof(matrix));
        } else {
            gfx_matrix_mul(rsp.P_matrix, matrix, rsp.P_matrix);
        }
	} else { 
		// G_MTX_NOPUSH | G_MTX_MUL | G_MTX_MODELVIEW
		if (parameters == 0) {
			gfx_matrix_mul(rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1], matrix,
							rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1]);
			rsp.lights_changed = 1;
		} else 
		// G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW
		if (parameters == 2) {
			//printf("\n");
			if (rsp.modelview_matrix_stack_size == 0)
				++rsp.modelview_matrix_stack_size;
			memcpy32(rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1], matrix, sizeof(matrix));
			rsp.lights_changed = 1;
		} else 
		// G_MTX_PUSH | G_MTX_MUL | G_MTX_MODELVIEW
		if (parameters == 4) {
			if (rsp.modelview_matrix_stack_size < 11) {
				++rsp.modelview_matrix_stack_size;
				memcpy32(rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1],
					rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 2], sizeof(matrix));
			}
			// STAR FOX 64 ONLY
			// only ever pushing identity matrix, no need to multiply
			//gfx_matrix_mul(rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1], matrix,
			//	rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1]);
		}
	}

	gfx_matrix_mul(rsp.MP_matrix, rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1],
					rsp.P_matrix);
}

// FOR STAR FOX 64 ONLY
// only used by Titania and it is fine if you dont reconstitute the MP matrix
// popping identity matrix
// count is only ever 1
static void  __attribute__((noinline)) gfx_sp_pop_matrix(UNUSED uint32_t count) {
#if 0
	if (count != 1) {
		printf("popped non-1\n");
		exit(-1);
	}
#endif

#if 1
	if (rsp.modelview_matrix_stack_size > 0) {
		--rsp.modelview_matrix_stack_size;
	}
#else
	while (count--) {
        if (rsp.modelview_matrix_stack_size > 0) {
            --rsp.modelview_matrix_stack_size;
            if (rsp.modelview_matrix_stack_size > 0) {
                gfx_matrix_mul(rsp.MP_matrix, rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1],
                               rsp.P_matrix);
            }
        }
    }
#endif
	rsp.lights_changed = 0;	
	matrix_dirty = 1;
}


//int nearz_clip_verts = 0;
//int total_verts = 0;

// 4 / 127
#define light0_scale 0.03149606f
// 3 / 127
#define light4_scale 0.02362205f

#define MEM_BARRIER() asm volatile("" : : : "memory");

#include "sh4zam.h"

#define fmac(a, b, c)  (((a)*(b))+(c))


float my_acosf (float a)
{
    float r, s, t;
    s = (a < 0.0f) ? 2.0f : (-2.0f);
    t = fmac (s, a, 2.0f) + 0.0000001f;
    s = shz_sqrtf_fsrra (t);
    r =              4.25032340e-7f;
    r = fmac (r, t, -1.92483935e-6f);
    r = fmac (r, t,  5.97197595e-6f);
    r = fmac (r, t, -2.54363249e-6f);
    r = fmac (r, t,  2.69393295e-5f);
    r = fmac (r, t,  1.16575764e-4f);
    r = fmac (r, t,  6.97973708e-4f);
    r = fmac (r, t,  4.68746712e-3f);
    r = fmac (r, t,  4.16666567e-2f);
    r = r * t;
    r = fmac (r, s, s);
    t = fmac (0x1.ddcb02p+0f, 0x1.aee9d6p+0f, 0.0f - r); // PI-r
    r = (a < 0.0f) ? t : r;
    return r;
}

static inline float approx_recip_sign(float v) {
	float _v = 1.0f / sqrtf(v * v);
	return copysignf(_v, v);
}

static inline float approx_recip(float v) {
	return 1.0f / sqrtf(v * v);
}


//GSTATE_MAP is 4
extern uint16_t gGameState;
static void __attribute__((noinline)) gfx_sp_vertex_light(size_t n_vertices, size_t dest_index, const Vtx* vertices) {
	__builtin_prefetch((void*)table32);
	for (size_t i = 0; i < n_vertices; i++, dest_index++) {
        const Vtx_tn* vn = &vertices[i].n;
        struct LoadedVertex* d = &rsp.loaded_vertices[dest_index];
		
		d->lit = 0;

        float x, y, z, w;
        x = vn->ob[0];
        y = vn->ob[1];
        z = vn->ob[2];
        mat_trans_single3_nodivw(x, y, z, w);

        short U = (vn->tc[0] * (int)rsp.texture_scaling_factor.s) >> 16;
        short V = (vn->tc[1] * (int)rsp.texture_scaling_factor.t) >> 16;
		float intensity[2] = {0.0f, 0.0f};

		intensity[0] = light0_scale * shz_dot8f(vn->n[0], vn->n[1], vn->n[2], 0, rsp.current_lights_coeffs[0][0],
                                             rsp.current_lights_coeffs[0][1], rsp.current_lights_coeffs[0][2], 0);
		MEM_BARRIER();

        int r = rsp.current_lights[rsp.current_num_lights - 1].col[0];
        int g = rsp.current_lights[rsp.current_num_lights - 1].col[1];
        int b = rsp.current_lights[rsp.current_num_lights - 1].col[2];
		MEM_BARRIER();

		if (rsp.current_lights[4].col[0] || rsp.current_lights[4].col[1] || rsp.current_lights[4].col[2]) {
			intensity[1] = light4_scale * shz_dot8f(vn->n[0], vn->n[1], vn->n[2], 0, rsp.current_lights_coeffs[4][0],
												rsp.current_lights_coeffs[4][1], rsp.current_lights_coeffs[4][2], 0);
			MEM_BARRIER();

			if (intensity[0] > 0.0f) {
					r += (intensity[0] * rsp.current_lights[0].col[0]);
					g += (intensity[0] * rsp.current_lights[0].col[1]);
					b += (intensity[0] * rsp.current_lights[0].col[2]);
			}

			MEM_BARRIER();

			if (intensity[1] > 0.0f) {
					r += (intensity[1] * rsp.current_lights[4].col[0]);
					g += (intensity[1] * rsp.current_lights[4].col[1]);
					b += (intensity[1] * rsp.current_lights[4].col[2]);
			}
		} else if (intensity[0] > 0.0f) {
            r += (intensity[0] * rsp.current_lights[0].col[0]);
            g += (intensity[0] * rsp.current_lights[0].col[1]);
            b += (intensity[0] * rsp.current_lights[0].col[2]);
        }

        d->lit = 1;
        if (r > 255)
            r = 255;
        if (g > 255)
            g = 255;
        if (b > 255)
            b = 255;
        d->color.r = table256[r];
        d->color.g = table256[g];
        d->color.b = table256[b];
        d->color.a = vn->a;

        if (rsp.geometry_mode & G_TEXTURE_GEN) {
            float dotx; 
            float doty;
            dotx = recip127 * shz_dot8f(vn->n[0], vn->n[1], vn->n[2], 0, rsp.current_lookat_coeffs[1][0],
                                        rsp.current_lookat_coeffs[1][1], rsp.current_lookat_coeffs[1][2], 0);

            doty = recip127 * shz_dot8f(vn->n[0], vn->n[1], vn->n[2], 0, rsp.current_lookat_coeffs[0][0],
                                        rsp.current_lookat_coeffs[0][1], rsp.current_lookat_coeffs[0][2], 0);

            if (dotx < -1.0f)
                dotx = -1.0f;
            else if (dotx > 1.0f)
                dotx = 1.0f;

            if (doty < -1.0f)
                doty = -1.0f;
            else if (doty > 1.0f)
                doty = 1.0f;

            if (rsp.geometry_mode & G_TEXTURE_GEN_LINEAR) {
                dotx = my_acosf(-dotx) * recip2pi;
                doty = my_acosf(-doty) * recip2pi;
            } else {
                dotx = (dotx * 0.25f) + 0.25f;
                doty = (doty * 0.25f) + 0.25f;
            }

            U = (int32_t) (dotx * rsp.texture_scaling_factor.s);
            V = (int32_t) (doty * rsp.texture_scaling_factor.t);
        }

        d->u = U;
        d->v = V;

        // trivial clip rejection
        d->wlt0 = (w < 0);
		d->clip_rej = 0;
#if 0
		uint8_t cr = 0;
        cr = !(z > w);
        cr = (cr << 1) | !(z < -w);
        cr = (cr << 1) | !(y > w);
        cr = (cr << 1) | !(y < -w);
        cr = (cr << 1) | !(x > w);
        cr = (cr << 1) | !(x < -w);
        d->clip_rej = cr;
#endif
		if (x < -w)
			d->clip_rej |= 1;
		if (x > w)
			d->clip_rej |= 2;
		if (y < -w)
			d->clip_rej |= 4;
		if (y > w)
			d->clip_rej |= 8;
		if (z < -w)
			d->clip_rej |= 16;
		if (z > w)
			d->clip_rej |= 32;

		d->x = vn->ob[0];
        d->y = vn->ob[1];
        d->z = vn->ob[2];
        d->w = 1.0f;

        d->_x = x;
        d->_y = y;
        d->_z = z;
        d->_w = /* approx_recip_sign */(w);
    }
}

static void __attribute__((noinline)) gfx_sp_vertex_no(size_t n_vertices, size_t dest_index, const Vtx* vertices) {
	__builtin_prefetch(table32);
    for (size_t i = 0; i < n_vertices; i++, dest_index++) {
        const Vtx_t* v = &vertices[i].v;
        struct LoadedVertex* d = &rsp.loaded_vertices[dest_index];
        float x, y, z, w;

		x = d->x = v->ob[0];
        y = d->y = v->ob[1];
        z = d->z = v->ob[2];
        d->w = 1.0f;

		mat_trans_single3_nodivw(x, y, z, w);

		d->_x = x;
        d->_y = y;
        d->_z = z;
        d->_w = /* approx_recip_sign */(w);
		d->lit = 0;

		d->color.r = table256[v->cn[0]];
        d->color.g = table256[v->cn[1]];
        d->color.b = table256[v->cn[2]];
	    d->color.a = v->cn[3];

        d->u = (v->tc[0] * (int)rsp.texture_scaling_factor.s) >> 16;
        d->v = (v->tc[1] * (int)rsp.texture_scaling_factor.t) >> 16;

        // trivial clip rejection
        d->wlt0 = (w < 0);
		d->clip_rej = 0;
#if 0
		uint8_t cr = 0;
        cr = !(z > w);
        cr = (cr << 1) | !(z < -w);
        cr = (cr << 1) | !(y > w);
        cr = (cr << 1) | !(y < -w);
        cr = (cr << 1) | !(x > w);
        cr = (cr << 1) | !(x < -w);
        d->clip_rej = cr;
#endif
		if (x < -w)
			d->clip_rej |= 1;
		if (x > w)
			d->clip_rej |= 2;
		if (y < -w)
			d->clip_rej |= 4;
		if (y > w)
			d->clip_rej |= 8;
		if (z < -w)
			d->clip_rej |= 16;
		if (z > w)
			d->clip_rej |= 32;
    }
}
// y before x in the index number in the abi cmd that loads the lookats
#define LOOKAT_Y_IDX 0
#define LOOKAT_X_IDX 1
#define LCOEFF_Y_IDX 1
#define LCOEFF_X_IDX 0


static void __attribute__((noinline)) gfx_sp_vertex(size_t n_vertices, size_t dest_index, const Vtx* vertices) {
//    total_verts += n_vertices;
	fast_mat_load(&rsp.MP_matrix);
    if (rsp.geometry_mode & G_LIGHTING) {
        if (rsp.lights_changed) {
			gfx_flush();
//            for (int n = 0; n < rsp.current_num_lights - 1; n++) {
            calculate_normal_dir(&rsp.current_lights[0], rsp.current_lights_coeffs[0]);
            calculate_normal_dir(&rsp.current_lights[4], rsp.current_lights_coeffs[4]);
//            }
            calculate_normal_dir(&rsp.lookat[0], rsp.current_lookat_coeffs[0]);
            calculate_normal_dir(&rsp.lookat[1], rsp.current_lookat_coeffs[1]);
            rsp.lights_changed = 0;
        }
        gfx_sp_vertex_light(n_vertices, dest_index, vertices);
	} else {
        gfx_sp_vertex_no(n_vertices, dest_index, vertices);
    }
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


int need_to_add = 0;
uint8_t add_r,add_g,add_b,add_a;
extern int cc_debug_toggle;
//int total_tri=0;
//int rej_tri=0;
//int nearz_tri = 0;
#if 1
extern u16 aCoGroundGrassTex[];
extern u16 D_CO_6028A60[];
extern u16 aVe1GroundTex[];
extern u16 aMaGroundTex[];
extern u16 aAqGroundTex[];
extern u16 aAqWaterTex[];
int last_was_special = 0;
#endif
static void  __attribute__((noinline)) gfx_sp_tri1(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx) {
    struct LoadedVertex* v1 = &rsp.loaded_vertices[vtx1_idx];
    struct LoadedVertex* v2 = &rsp.loaded_vertices[vtx2_idx];
    struct LoadedVertex* v3 = &rsp.loaded_vertices[vtx3_idx];
    struct LoadedVertex* v_arr[3] = { v1, v2, v3 };

//	total_tri++;
#if 0
			if (last_was_special) gfx_flush();
#endif
	if (v1->clip_rej & v2->clip_rej & v3->clip_rej) {
#if 0
		last_was_special = 0;
#endif
		// The whole triangle lies outside the visible area

//		nearz_tri++;
/* 		if (nearz_tri == 1) {
*/	//		printf ("v1 %f,%f,%f,%f\n", v1->_x, v1->_y, v1->_z, v1->_w);
/*			for(int i=0;i<4;i++) {
			for(int j=0;j<4;j++) {
			printf("%f, ",		rsp.MP_matrix[i][j]);
					}
					printf("\n");
			}
		} */
 		return;
    }
#if 1
	float rw1 = 1.0f / (v1->_w);
	float rw2 = 1.0f / (v2->_w);
	float rw3 = 1.0f / (v3->_w);
	if ((rsp.geometry_mode & G_CULL_BOTH) != 0) {

        float dx1 = (v1->_x * rw1) - (v2->_x * rw2);
        float dy1 = (v1->_y * rw1) - (v2->_y * rw2);
        float dx2 = (v3->_x * rw3) - (v2->_x * rw2);
        float dy2 = (v3->_y * rw3) - (v2->_y * rw2);
        float cross = dx1 * dy2 - dy1 * dx2;

        if ((v1->wlt0) ^ (v2->wlt0) ^ (v3->wlt0)) {
//			return;
            // If one vertex lies behind the eye, negating cross will give the correct result.
            // If all vertices lie behind the eye, the triangle will be rejected anyway.
            cross = -cross;
        }

                switch (rsp.geometry_mode & G_CULL_BOTH) {
                    case G_CULL_FRONT:
                        if (cross <= 0) {
//		last_was_special = 0;
//							rej_tri++;
							return;
                        }
						break;
                    case G_CULL_BACK:
                        if (cross >= 0) {
//		last_was_special = 0;
//							rej_tri++;
							return;
                        }
						break;
                    case G_CULL_BOTH: {
//		last_was_special = 0;
                        // Why is this even an option?
//						rej_tri++;
                        return;
						}
						break;        
					}
    }
#endif

	//frame_tris++;
	if (matrix_dirty) {
        glMatrixMode(GL_PROJECTION);
        glLoadMatrixf((const float*) rsp.P_matrix); //MP_matrix);
  		//glLoadIdentity();
      	glMatrixMode(GL_MODELVIEW);
		glLoadMatrixf((const float*) rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1]);
		//glLoadIdentity();
		matrix_dirty = 0;
		gfx_flush();
	}
//	printf("rw1 %f\n");
#if 0
	v1->x = v1->_x * rw1;
	v1->y = v1->_y * rw1;
	v1->z = v1->_z * rw1;
	v1->w = 1;//v1->_w;

	v2->x = v2->_x * rw2;
	v2->y = v2->_y * rw2;
	v2->z = v2->_z * rw2;
	v2->w = 1;//v2->_w;

	v3->x = v3->_x * rw3;
	v3->y = v3->_y * rw3;
	v3->z = v3->_z * rw3;
	v3->w = 1;//v3->_w;
#endif
	uint8_t depth_test = (rsp.geometry_mode & G_ZBUFFER) == G_ZBUFFER;
    if (depth_test != rendering_state.depth_test) {
        gfx_flush();
        gfx_rapi->set_depth_test(depth_test);
        rendering_state.depth_test = depth_test;
    }

    uint8_t z_upd = (rdp.other_mode_l & Z_UPD) == Z_UPD;
    if (z_upd != rendering_state.depth_mask) {
        gfx_flush();
        gfx_rapi->set_depth_mask(z_upd);
        rendering_state.depth_mask = z_upd;
    }

    uint8_t zmode_decal = (rdp.other_mode_l & ZMODE_DEC) == ZMODE_DEC;
    if (zmode_decal != rendering_state.decal_mode) {
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
    uint8_t texture_edge = (rdp.other_mode_l & CVG_X_ALPHA) == CVG_X_ALPHA;
	uint8_t use_noise = (rdp.other_mode_h == 0x2ca0);// (1U << 4)) == (1U << 4);
	if (alpha_noise) {
		gfx_flush();
		alpha_noise = 0;//use_noise;
	}

    if (use_alpha)
        cc_id |= SHADER_OPT_ALPHA;
    if (use_fog)
        cc_id |= SHADER_OPT_FOG;
    if (texture_edge)
        cc_id |= SHADER_OPT_TEXTURE_EDGE;
	if (use_noise) {
        cc_id |= SHADER_OPT_NOISE;
	}

    if (!use_alpha)
        cc_id &= ~0xfff000;

    struct ColorCombiner* comb = gfx_lookup_or_create_color_combiner(cc_id);
    struct ShaderProgram* prg = comb->prg;
    if (prg != rendering_state.shader_program) {
        gfx_flush();
        gfx_rapi->unload_shader(rendering_state.shader_program);
        gfx_rapi->load_shader(prg);
        rendering_state.shader_program = prg;
    }

    if (use_alpha != rendering_state.alpha_blend) {
        gfx_flush();
        gfx_rapi->set_use_alpha(use_alpha);
        rendering_state.alpha_blend = use_alpha;
    }

    uint8_t num_inputs;
    uint8_t used_textures[2];
    gfx_rapi->shader_get_info(prg, &num_inputs, used_textures);
    int i;
	#if 1
	void * texaddr = segmented_to_virtual(rdp.texture_to_load.addr);
	void * grass = segmented_to_virtual(aCoGroundGrassTex);
	void * water = segmented_to_virtual(D_CO_6028A60);
	void * ve1ground = segmented_to_virtual(aVe1GroundTex);
	void * maground = segmented_to_virtual(aMaGroundTex);
	void * aqground = segmented_to_virtual(aAqGroundTex);
	void * aqwater = segmented_to_virtual(aAqWaterTex);
#endif
    for (i = 0; i < 1/* 2 */; i++) {
        if (used_textures[i]) {
            if (rdp.textures_changed[i]) {
				// necessary
                gfx_flush();
                import_texture(i);
                rdp.textures_changed[i] = 0;
            }
            uint8_t cms = rdp.texture_tile.cms;
            uint8_t cmt = rdp.texture_tile.cmt;
//printf("grass %08x water %08x ve1ground %08x maground %08x aqground %08x aqwater %08x\n",
//grass,water,ve1ground,maground,aqground,aqwater);
#if 1
uint8_t special_cm = (
			((gCurrentLevel == LEVEL_CORNERIA) && ((texaddr == grass) || (texaddr == water))) 
			|| 
			((gCurrentLevel == LEVEL_VENOM_1) && (texaddr == ve1ground))
			||
			((gCurrentLevel == LEVEL_MACBETH) && (texaddr == maground))
			||
			((gCurrentLevel == LEVEL_AQUAS) && ((texaddr == aqground) || (texaddr == aqwater)))
			);
//			if (((!last_was_special) && special_cm) || (last_was_special && (!special_cm))) {
//				gfx_flush();
//			}
/* 			if (texaddr == aqwater) {
				printf("cms %d cmt %d\n", cms, cmt);
				cms = 1;
				cmt = 1;
			} else */ if (special_cm) {
//				last_was_special = 1;
//				printf("prev cms %d cmt %d\n", cms, cmt);
				cms = 0;
				cmt = 0;
			} else { // if (!(special_cm  || (texaddr == aqwater ))) {
//				return;
//				last_was_special = 0;
#endif
				uint32_t tex_size_bytes = rdp.loaded_texture[rdp.texture_to_load.tile_number].size_bytes;
				uint32_t line_size = rdp.texture_tile.line_size_bytes;

				if (line_size == 0) {
					line_size = 1;
				}

				uint32_t tex_height_i = tex_size_bytes / line_size;
				switch (rdp.texture_tile.siz) {
					case G_IM_SIZ_4b:
						line_size <<= 1;
						break;
					case G_IM_SIZ_8b:
						break;
					case G_IM_SIZ_16b:
						line_size >>= 1; // /= G_IM_SIZ_16b_LINE_BYTES;
						break;
					case G_IM_SIZ_32b:
						line_size >>= 1; // /= G_IM_SIZ_32b_LINE_BYTES; // this is 2!
						tex_height_i >>=1 ; // /= 2;
						break;
				}
	//			uint8_t tm = 0;
				uint32_t tex_width_i = line_size;

				uint32_t tex_width2_i = (rdp.texture_tile.lrs - rdp.texture_tile.uls + 4) >> 2;
				uint32_t tex_height2_i = (rdp.texture_tile.lrt - rdp.texture_tile.ult + 4) >> 2;

				uint32_t tex_width1 = tex_width_i << (cms & G_TX_MIRROR);
				uint32_t tex_height1 = tex_height_i << (cmt & G_TX_MIRROR);

				if ((cms & G_TX_CLAMP) && ((cms & G_TX_MIRROR) || (tex_width1 != tex_width2_i))) {
	//                tm |= (1 << (2 * i));
					cms &= (~G_TX_CLAMP);
				}

				if ((cmt & G_TX_CLAMP) && ((cmt & G_TX_MIRROR) || (tex_height1 != tex_height2_i))) {
	//                tm |= ((1 << (2 * i)) + 1);
					cmt &= (~G_TX_CLAMP);
				}
#if 1
			}
#endif
			uint8_t linear_filter = (rdp.other_mode_h & (3U << G_MDSFT_TEXTFILT)) != G_TF_POINT;

      		if ((linear_filter != rendering_state.textures[i]->linear_filter) ||
                (rendering_state.textures[i]->cms != cms) ||
                (rendering_state.textures[i]->cmt != cmt)) {
                gfx_flush();
            }

			gfx_rapi->set_sampler_parameters(i, linear_filter, cms, cmt);
	        rendering_state.textures[i]->linear_filter = linear_filter;
            rendering_state.textures[i]->cms = cms;
            rendering_state.textures[i]->cmt = cmt;
        }
    }

    if (use_fog) {
        float fog_color[4] = { rdp.fog_color.r * recip255, rdp.fog_color.g * recip255,
                               rdp.fog_color.b * recip255, rdp.fog_color.a * recip255 };
        glFogfv(GL_FOG_COLOR, fog_color);
    }

    uint8_t use_texture = used_textures[0] || used_textures[1];
	float recip_tex_width;
	float recip_tex_height;

	if (use_texture) {
		uint32_t tex_width = (rdp.texture_tile.lrs - rdp.texture_tile.uls + 4) >> 2;
		uint32_t tex_height = (rdp.texture_tile.lrt - rdp.texture_tile.ult + 4) >> 2;
		recip_tex_width = approx_recip((float)tex_width);
		recip_tex_height = approx_recip((float)tex_height);
	}

	uint8_t lit = v_arr[0]->lit;

    for (i = 0; i < 3; i++) {
        buf_vbo[buf_num_vert].vert.x = v_arr[i]->x;
        buf_vbo[buf_num_vert].vert.y = v_arr[i]->y;
        buf_vbo[buf_num_vert].vert.z = v_arr[i]->z;

        if (use_texture) {
            float u = (v_arr[i]->u * 0.03125f);
            float v = (v_arr[i]->v * 0.03125f);
#if 0
			uint8_t shifts = rdp.texture_tile.shifts;
            uint8_t shiftt = rdp.texture_tile.shiftt;
			if (shifts != 0) {
                if (shifts <= 10) {
                    u /= (float)(1 << shifts);
                } else {
                    u *= (float)(1 << (16 - shifts));
                }
            }
            if (shiftt != 0) {
                if (shiftt <= 10) {
                    v /= (float)(1 << shiftt);
                } else {
                    v *= (float)(1 << (16 - shiftt));
                }
            }
#endif
			u -= (float)(rdp.texture_tile.uls * 0.25f);
			v -= (float)(rdp.texture_tile.ult * 0.25f);
//			if (texaddr == aqwater) {
//				printf("u %f v %f\n", u * recip_tex_width, v * recip_tex_height);
//			}
			if ((rdp.other_mode_h & (3U << G_MDSFT_TEXTFILT)) != G_TF_POINT) {
                // Linear filter adds 0.5f to the coordinates
                u += 0.5f;
                v += 0.5f;
            }

			buf_vbo[buf_num_vert].texture.u = u * recip_tex_width;
            buf_vbo[buf_num_vert].texture.v = v * recip_tex_height;
        }

        int j, k;
        buf_vbo[buf_num_vert].color.packed = 0xffffffff;
        uint32_t color_r = 0;
        uint32_t color_g = 0;
        uint32_t color_b = 0;
        uint32_t color_a = 0;

		uint32_t cc_rgb = rdp.combine_mode & 0xFFF;
		uint32_t cc_alpha = (rdp.combine_mode >> 12) & 0xFFF;

		//#if 1
#if 0
if (i == 0 && cc_debug_toggle) {
//static inline uint32_t color_comb(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
//	return color_comb_component(a) | (color_comb_component(b) << 3) | (color_comb_component(c) << 6) |
//		   (color_comb_component(d) << 9);
//}
//static void gfx_dp_set_combine_mode(uint32_t rgb, uint32_t alpha) {
//	rdp.combine_mode = rgb | (alpha << 12);
//}
		uint8_t cc_a,cc_b,cc_c,cc_d;
		uint8_t cc_aa,cc_ba,cc_ca,cc_da;

		#if 0
need_to_add = 0;
//0x00000a6b -> (G_CCMUX_PRIM - G_CCMUX_ENV) * 1 + G_CCMUX_ENV
//	 0x000000c1 -> (1 - 0) * G_ACMUX_PRIM + 0
switch(cc_rgb) {
	case 0x200:
		color_r = 255;
		color_g = 255;
		color_b = 255;
		break;
	case 0xc1:
		color_r = rdp.prim_color.r;
		color_g = rdp.prim_color.g;
		color_b = rdp.prim_color.b;
		break;
	case 0x101:
		// (texel - combined) * G_CCMUX_SHADE + combined. sorry , youre getting shade and thats it
		color_r = v_arr[i]->color.r;
		color_g = v_arr[i]->color.g;
        color_b = v_arr[i]->color.b;
		break;
	case 0xa6b:
		color_r = rdp.prim_color.r/*  - rdp.env_color.r */;
		color_g = rdp.prim_color.g /* - rdp.env_color.g */;
		color_b = rdp.prim_color.b /* - rdp.env_color.b */;
		need_to_add = 1;
		add_r = rdp.env_color.r;
		add_g = rdp.env_color.g;
		add_b = rdp.env_color.b;
		break;
	case 0x668:
		//(0 - G_CCMUX_ENV) * 1 + G_CCMUX_PRIM
		// lets try 255-env + prim
		if (i == 0) {
			gfx_flush();
		}
		need_to_add = 1;
		color_r =  255 -  rdp.env_color.r + rdp.prim_color.r;
		color_g =  255 -  rdp.env_color.g + rdp.prim_color.g;
		color_b =  255 -  rdp.env_color.b + rdp.prim_color.b;
		add_r = rdp.prim_color.r;
		add_g = rdp.prim_color.g;
		add_b = rdp.prim_color.b;
		break;
	default:
if (cc_debug_toggle) {
goto ccdebug;
} else {

buf_vbo[buf_num_vert].color.packed =
                                PACK_ARGB8888(0,0,0,255);
goto nextvert;
								//		goto old_color_style;
}
								break;
}
switch(cc_alpha) {
	case 0x800:
		color_a = v_arr[i]->color.a;
		add_a = color_a;
		break;
	case 0xc1:
		color_a = rdp.prim_color.a;
		break;
	case 0x00000200:
		color_a = 255;
		break;
	case 0x00000043:
		// (G_CCMUX_PRIM - 0) * 1 + 0
		color_a = rdp.prim_color.a;
		break;
	default:

		color_a = 255;
		break;

	}


buf_vbo[buf_num_vert].color.packed =
                                PACK_ARGB8888(color_r,color_g,color_b,color_a);

goto nextvert;
#endif
ccdebug:
		cc_a = cc_rgb & 0x7;
		cc_b = (cc_rgb>>3) & 0x7;
		cc_c = (cc_rgb>>6) & 0x7;
		cc_d = (cc_rgb>>9) & 0x7;

		cc_aa = cc_alpha & 0x7;
		cc_ba = (cc_alpha>>3) & 0x7;
		cc_ca = (cc_alpha>>6) & 0x7;
		cc_da = (cc_alpha>>9) & 0x7;

		printf("0x%08x -> (", cc_rgb);
		uint32_t fake_r=0,fake_g=0,fake_b=0,fake_a=0;
		switch (cc_a) {
			case G_CCMUX_0:
				printf("G_CCMUX_0 - ");
				break;
			case G_CCMUX_1:
				printf("G_CCMUX_1 - ");
				break;
			case G_CCMUX_PRIMITIVE:
				printf("G_CCMUX_PRIM - ");
				break;
			case G_CCMUX_ENVIRONMENT:
				printf("G_CCMUX_ENV - ");
				break;
			case G_CCMUX_SHADE:
				printf("G_CCMUX_SHADE - ");
				break;
			default:
			    printf("%d - ", cc_a);
				break;
		}

		switch (cc_b) {
			case G_CCMUX_0:
				printf("G_CCMUX_0) * ");
 				break;
			case G_CCMUX_1:
				printf("G_CCMUX_1) * ");
				break;
			case G_CCMUX_PRIMITIVE:
				printf("G_CCMUX_PRIM) * ");
				break;
			case G_CCMUX_ENVIRONMENT:
				printf("G_CCMUX_ENV) * ");
				break;
			case G_CCMUX_SHADE:
				printf("G_CCMUX_SHADE) * ");
				break;
			default:
				printf("%d) * ", cc_b);
				break;
		}

		switch (cc_c) {
			case G_CCMUX_0:
				printf("G_CCMUX_0 + ");
				break;
			case G_CCMUX_1:
				printf("G_CCMUX_1 + ");
				break;
			case G_CCMUX_PRIMITIVE:
				printf("G_CCMUX_PRIM + ");
				break;
			case G_CCMUX_ENVIRONMENT:
				printf("G_CCMUX_ENV + ");
				break;
			case G_CCMUX_SHADE:
				printf("G_CCMUX_SHADE + ");
				break;
			default:
				printf("%d + ", cc_c);
				break;
		}

		switch (cc_d) {
			case G_CCMUX_0:
				printf("G_CCMUX_0\n");
				break;
			case G_CCMUX_1:
				printf("G_CCMUX_1\n");
				break;
			case G_CCMUX_PRIMITIVE:
				printf("G_CCMUX_PRIM\n");
				break;
			case G_CCMUX_ENVIRONMENT:
				printf("G_CCMUX_ENV\n");
				break;
			case G_CCMUX_SHADE:
				printf("G_CCMUX_SHADE\n");
				break;
			default:
				printf("%d\n", cc_d);
				break;
		}


printf("\t 0x%08x -> (", cc_alpha);
		switch (cc_aa) {
			case G_ACMUX_0:
				printf("G_ACMUX_0 - ");
	//			fake_r = 0;
	//			fake_g = 0;
	//			fake_b = 0;
				break;
			case G_ACMUX_1:
				printf("G_ACMUX_1 - ");
	//			fake_r = 255;
	//			fake_g = 255;
	//			fake_b = 255;
				break;
			case G_ACMUX_PRIMITIVE:
				printf("G_ACMUX_PRIM - ");
	//			fake_r = pr;
	//			fake_g = pg;
	//			fake_b = pb;
				break;
			case G_ACMUX_ENVIRONMENT:
				printf("G_ACMUX_ENV - ");
	//			fake_r = er;
	//			fake_g = eg;
	//			fake_b = eb;
				break;
			case G_ACMUX_SHADE:
				printf("G_ACMUX_SHADE - ");
	//			fake_r = v_arr[i]->color.r;
	//			fake_g = v_arr[i]->color.g;
      //          fake_b = v_arr[i]->color.b;
				break;
			default:
			    printf("%d - ", cc_aa);
		//		fake_r = fake_g = fake_b = 255;
				break;
		}

		switch (cc_ba) {
			case G_ACMUX_0:
printf("G_ACMUX_0) * ");
			/* 				fake_r -= 0;
				fake_g -= 0;
				fake_b -= 0;
 */				break;
			case G_ACMUX_1:
printf("G_ACMUX_1) * ");
/* 				fake_r -= 255;
				fake_g -= 255;
				fake_b -= 255;
 */				break;
			case G_ACMUX_PRIMITIVE:
			printf("G_ACMUX_PRIM) * ");

/* 				fake_r -= pr;
				fake_g -= pg;
				fake_b -= pb; */
				break;
			case G_ACMUX_ENVIRONMENT:
			printf("G_ACMUX_ENV) * ");

/* 				fake_r -= er;
				fake_g -= eg;
				fake_b -= eb; */
				break;
			case G_ACMUX_SHADE:
			printf("G_ACMUX_SHADE) * ");
/* 				fake_r -= v_arr[i]->color.r;
				fake_g -= v_arr[i]->color.g;
                fake_b -= v_arr[i]->color.b; */
				break;
			default:
						printf("%d) * ", cc_ba);

/* 				fake_r -= 0;
				fake_g -= 0;
				fake_b -= 0;
 */				break;
		}

		switch (cc_ca) {
			case G_ACMUX_0:
						printf("G_ACMUX_0 + ");

/* 				fake_r *= 0;
				fake_g *= 0;
				fake_b *= 0;
 */				break;
			case G_ACMUX_1:
						printf("G_ACMUX_1 + ");
/* 				fake_r *= 255;
				fake_g *= 255;
				fake_b *= 255; */
				break;
			case G_ACMUX_PRIMITIVE:
						printf("G_ACMUX_PRIM + ");

			/* 				fake_r *= pr;
				fake_g *= pg;
				fake_b *= pb; */
				break;
			case G_ACMUX_ENVIRONMENT:
						printf("G_ACMUX_ENV + ");
/* 				fake_r *= er;
				fake_g *= eg;
				fake_b *= eb; */
				break;
			case G_ACMUX_SHADE:
						printf("G_ACMUX_SHADE + ");
/* 				fake_r *= v_arr[i]->color.r;
				fake_g *= v_arr[i]->color.g;
                fake_b *= v_arr[i]->color.b; */
				break;
			default:
						printf("%d + ", cc_ca);
/* 				fake_r *= 255;
				fake_g *= 255;
				fake_b *= 255; */
				break;
		}

/* 		fake_r >>= 8;
		fake_g >>= 8;
		fake_b >>= 8; */

		switch (cc_da) {
			case G_ACMUX_0:
						printf("G_ACMUX_0\n");
/* 				fake_r += 0;
				fake_g += 0;
				fake_b += 0;
 */				break;
			case G_ACMUX_1:
						printf("G_ACMUX_1\n");
/* 				fake_r += 255;
				fake_g += 255;
				fake_b += 255;
 */				break;
			case G_ACMUX_PRIMITIVE:
						printf("G_ACMUX_PRIM\n");
/* 				fake_r += pr;
				fake_g += pg;
				fake_b += pb; */
				break;
			case G_ACMUX_ENVIRONMENT:
						printf("G_ACMUX_ENV\n");
/* 				fake_r += er;
				fake_g += eg;
				fake_b += eb; */
				break;
			case G_ACMUX_SHADE:
						printf("G_ACMUX_SHADE\n");
/* 				fake_r += v_arr[i]->color.r;
				fake_g += v_arr[i]->color.g;
                fake_b += v_arr[i]->color.b; */
				break;
			default:
						printf("%d\n", cc_da);
				//
				break;
		}

#if 0
		switch (cc_aa) {
			case G_CCMUX_0:
				fake_a = 0;
				break;
			case G_CCMUX_1:
				fake_a = 255;
				break;
			case G_CCMUX_PRIMITIVE_ALPHA:
				fake_a = pa;
				break;
			case G_CCMUX_ENV_ALPHA:
				fake_a = ea;
				break;
			case G_CCMUX_SHADE_ALPHA:	
                fake_a = v_arr[i]->color.a;
				break;
			default:
				fake_a = 255;
				break;
		}

		switch (cc_ba) {
			case G_CCMUX_0:
				break;
			case G_CCMUX_1:
				fake_a -= 255;
				break;
			case G_CCMUX_PRIMITIVE_ALPHA:
				fake_a -= pa;
				break;
			case G_CCMUX_ENV_ALPHA:
				fake_a -= ea;
				break;
			case G_CCMUX_SHADE_ALPHA:
                fake_a -= v_arr[i]->color.a;
				break;
			default:
				break;
		}

		switch (cc_ca) {
			case G_CCMUX_0:
				fake_a *= 0;
				break;
			case G_CCMUX_1:
				fake_a *= 255;
				break;
			case G_CCMUX_PRIMITIVE_ALPHA:
				fake_a *= pa;
				break;
			case G_CCMUX_ENV_ALPHA:
				fake_a *= ea;
				break;
			case G_CCMUX_SHADE_ALPHA:
                fake_a *= v_arr[i]->color.a;
				break;
			default:
				fake_a *= 255;
				break;
		}

		fake_a >>= 8;

		switch (cc_da) {
			case G_CCMUX_0:
				break;
			case G_CCMUX_1:
				fake_a += 255;
				break;
			case G_CCMUX_PRIMITIVE_ALPHA:
				fake_a += pa;
				break;
			case G_CCMUX_ENV_ALPHA:
				fake_a += ea;
				break;
			case G_CCMUX_SHADE_ALPHA:
                fake_a += v_arr[i]->color.a;
				break;
			default:
				//
				break;
		}

                uint32_t max_c = 255;
                if (fake_r > max_c)
                    max_c = fake_r;
                if (fake_g > max_c)
                    max_c = fake_g;
                if (fake_b > max_c)
                    max_c = fake_b;
                if (fake_a > max_c)
                    max_c = fake_a;

                float rn, gn, bn, an;
                rn = (float) fake_r;
                gn = (float) fake_g;
                bn = (float) fake_b;
                an = (float) fake_a;
                float maxc = 255.0f / (float) max_c;
                rn *= maxc;
                gn *= maxc;
                bn *= maxc;
                an *= maxc;

				fake_r = rn;
				fake_g = gn;
				fake_b = bn;
				fake_a = rn;

buf_vbo[buf_num_vert].color.packed =
                                PACK_ARGB8888(fake_r,fake_g,fake_b,fake_a);
#endif
	}
#endif
#if 1
//old_color_style:	
 		if (lit) {
			color_r = v_arr[i]->color.r;
			color_g = v_arr[i]->color.g;
			color_b = v_arr[i]->color.b;
			color_a = 255;
			buf_vbo[buf_num_vert].color.packed = PACK_ARGB8888(color_r, color_g, color_b, color_a);
		} 
		
		if (cc_rgb == 0x668) {
//			if (!lit) {
			//(0 - G_CCMUX_ENV) * 1 + G_CCMUX_PRIM
			color_r = /* v_arr[i]->color.r +  */(rdp.prim_color.r - rdp.env_color.r);
			color_g = /* v_arr[i]->color.g +  */(rdp.prim_color.g - rdp.env_color.g);
			color_b = /* v_arr[i]->color.b +  */(rdp.prim_color.b - rdp.env_color.b);
//			}
			color_a = rdp.prim_color.a;
			buf_vbo[buf_num_vert].color.packed = PACK_ARGB8888(color_r, color_g, color_b, color_a);
		}
		else if (num_inputs > 1) {
            int i0 = comb->shader_input_mapping[0][1] == CC_PRIM;
            int i2 = comb->shader_input_mapping[0][0] == CC_ENV;

            int i3 = comb->shader_input_mapping[0][0] == CC_PRIM;
            int i4 = comb->shader_input_mapping[0][1] == CC_ENV;

            if (i0 && i2) {
				if (!lit) {
					color_r = 255 - rdp.env_color.r;
					color_g = 255 - rdp.env_color.g;
					color_b = 255 - rdp.env_color.b;
				}
				color_a = rdp.prim_color.a;

				if(!lit)
                	buf_vbo[buf_num_vert].color.packed = PACK_ARGB8888(color_r, color_g, color_b, color_a);
            } else if (i3 && i4) {
				if (!lit) {
					color_r = rdp.prim_color.r;
					color_g = rdp.prim_color.g;
					color_b = rdp.prim_color.b;
				}
                color_a = rdp.prim_color.a;

				if(!lit) {
					color_r *= ((rdp.env_color.r + 255));
					color_g *= ((rdp.env_color.g + 255));
					color_b *= ((rdp.env_color.b + 255));
				}
                color_a *= rdp.env_color.a;

				if (!lit) {
					color_r >>= 8;// /= 255;
					color_g >>= 8;// /= 255;
					color_b >>= 8;// /= 255;
				}
                color_a >>= 8;// /= 255;

                uint32_t max_c = 255;
				if (!lit) {
					if (color_r > max_c)
						max_c = color_r;
					if (color_g > max_c)
						max_c = color_g;
					if (color_b > max_c)
						max_c = color_b;
					if (color_a > max_c)
						max_c = color_a;

					float rmc = approx_recip((float)max_c);

					float rn, gn, bn, an;
					rn = (float) color_r;
					gn = (float) color_g;
					bn = (float) color_b;
					an = (float) color_a;
					float maxc = 255.0f * rmc;
					rn *= maxc;
					gn *= maxc;
					bn *= maxc;
					an *= maxc;

					color_r = (uint32_t) rn;
					color_g = (uint32_t) gn;
					color_b = (uint32_t) bn;
					color_a = (uint32_t) an;
					buf_vbo[buf_num_vert].color.packed = PACK_ARGB8888(color_r, color_g, color_b, color_a);
				}
			} else {
                goto thenextthing;
            }
        } else {
        thenextthing:
#if 1
		for (j = 0; j < num_inputs; j++) {
                for (k = 0; k < 1 + (use_alpha ? 1 : 0); k++) {
                    switch (comb->shader_input_mapping[k][j]) {
						case G_CCMUX_PRIMITIVE_ALPHA:
							if (!lit)
								buf_vbo[buf_num_vert].color.packed = PACK_ARGB8888(rdp.prim_color.a, rdp.prim_color.a, rdp.prim_color.a, k ? rdp.prim_color.a : 255);
							break;
						case G_CCMUX_ENV_ALPHA: 
							if (!lit)
								buf_vbo[buf_num_vert].color.packed = PACK_ARGB8888(rdp.env_color.a, rdp.env_color.a, rdp.env_color.a, k ? rdp.env_color.a : 255);
							break;
                        case CC_PRIM:
							if(lit)
								color_a = k ? rdp.prim_color.a : 255;
							else
								buf_vbo[buf_num_vert].color.packed = PACK_ARGB8888(rdp.prim_color.r, rdp.prim_color.g, rdp.prim_color.b, k ? rdp.prim_color.a : 255);
                            break;
                        case CC_SHADE:
							if (lit)
								color_a = k ? v_arr[i]->color.a : 255;
							else
                        		buf_vbo[buf_num_vert].color.packed = PACK_ARGB8888(v_arr[i]->color.r, v_arr[i]->color.g, v_arr[i]->color.b, k ? v_arr[i]->color.a : 255);
                            break;
                        case CC_ENV:
							if (lit)
								color_a =  k ? rdp.env_color.a : 255;
							else 
            	                buf_vbo[buf_num_vert].color.packed = PACK_ARGB8888(rdp.env_color.r, rdp.env_color.g, rdp.env_color.b, k ? rdp.env_color.a : 255);
               				break;
                        case CC_LOD: {
                            float distance_frac = (v1->w - 3000.0f) / 3000.0f;
                            if (distance_frac < 0.0f)
                                distance_frac = 0.0f;
                            if (distance_frac > 1.0f)
                                distance_frac = 1.0f;
                            const uint8_t frac = (uint8_t) (distance_frac * 255.0f);
							if(lit)
								color_a =  k ? frac : 255;
							else
    	                        buf_vbo[buf_num_vert].color.packed = PACK_ARGB8888(frac, frac, frac, k ? frac : 255);
							break;
                        }
                        default:
							if (lit)
								color_a = 255;
						else
						    buf_vbo[buf_num_vert].color.packed = PACK_ARGB8888(0xff, 0xff, 0xff, 0xff);
                            break;
                    }
                }
            }
#endif
        }
#endif
		if(lit)
			buf_vbo[buf_num_vert].color.packed = PACK_ARGB8888(color_r, color_g, color_b, color_a);

nextvert:
		buf_num_vert++;
        buf_vbo_len += sizeof(dc_fast_t);
    }
    buf_vbo_num_tris += 1;

	if (do_rectdepfix || do_menucard || buf_vbo_num_tris == MAX_BUFFERED) {
        gfx_flush();
    }
}

extern int first_2d;
extern void gfx_opengl_reset_frame(int r, int g, int b);
extern void gfx_opengl_draw_triangles_2d(void* buf_vbo, size_t buf_vbo_len, size_t buf_vbo_num_tris);

int do_ext_fill = 0;

static void  __attribute__((noinline)) gfx_sp_quad_2d(uint8_t vtx1_idx, uint8_t vtx2_idx, uint8_t vtx3_idx, uint8_t vtx1_idx2, uint8_t vtx2_idx2,
						   uint8_t vtx3_idx2) {
	if (do_the_blur) 
		gfx_flush();
	dc_fast_t* v1 = &rsp.loaded_vertices_2D[vtx1_idx];
	dc_fast_t* v2 = &rsp.loaded_vertices_2D[vtx2_idx];
	dc_fast_t* v3 = &rsp.loaded_vertices_2D[vtx2_idx2];
	dc_fast_t* v4 = &rsp.loaded_vertices_2D[vtx3_idx];
	dc_fast_t* pre_v_arr[4] = { v1, v2, v3, v4 };
	dc_fast_t* v_arr[6] = { v1, v2, v4, v2, v3, v4 };
	uint8_t depth_test = (rsp.geometry_mode & G_ZBUFFER) == G_ZBUFFER;
	if (depth_test != rendering_state.depth_test) {
		gfx_flush();
		gfx_rapi->set_depth_test(depth_test);
		rendering_state.depth_test = depth_test;
	}

	uint8_t z_upd = (rdp.other_mode_l & Z_UPD) == Z_UPD;
	if (z_upd != rendering_state.depth_mask) {
		gfx_flush();
		gfx_rapi->set_depth_mask(z_upd);
		rendering_state.depth_mask = z_upd;
	}

	uint8_t zmode_decal = (rdp.other_mode_l & ZMODE_DEC) == ZMODE_DEC;
	if (zmode_decal != rendering_state.decal_mode) {
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
	uint8_t texture_edge = (rdp.other_mode_l & CVG_X_ALPHA) == CVG_X_ALPHA;
	// this is literally only for the stupid sun in the intro and nothing else
	uint8_t use_noise = (rdp.other_mode_h == 0x2ca0);
	if (!alpha_noise && use_noise) {
		gfx_flush();
	} else if (alpha_noise && !use_noise) {
		gfx_flush();
	}
	alpha_noise = use_noise;
	if (texture_edge) {
		use_alpha = 1;
	}

	if (use_alpha)
		cc_id |= SHADER_OPT_ALPHA;
	if (use_fog)
		cc_id |= SHADER_OPT_FOG;
	if (texture_edge)
		cc_id |= SHADER_OPT_TEXTURE_EDGE;
	if (use_noise)
		cc_id |= SHADER_OPT_NOISE;
	if (!use_alpha)
		cc_id &= ~0xfff000;

	struct ColorCombiner* comb = gfx_lookup_or_create_color_combiner(cc_id);
	struct ShaderProgram* prg = comb->prg;
	if (prg != rendering_state.shader_program) {
		gfx_flush();
		gfx_rapi->unload_shader(rendering_state.shader_program);
		gfx_rapi->load_shader(prg);
		rendering_state.shader_program = prg;
	}

	if (use_alpha != rendering_state.alpha_blend) {
		gfx_flush();
		gfx_rapi->set_use_alpha(use_alpha);
		rendering_state.alpha_blend = use_alpha;
	}

	uint8_t num_inputs;
	uint8_t used_textures[2];
	gfx_rapi->shader_get_info(prg, &num_inputs, used_textures);
	int i;

	for (i = 0; i < 1/* 2 */; i++) {
		if (used_textures[i]) {
			if (rdp.textures_changed[i]) {
				// necessary
				gfx_flush();
				import_texture(i);
				rdp.textures_changed[i] = 0;
			}
			uint8_t linear_filter = /* do_the_blur ? 0 : */ (rdp.other_mode_h & (3U << G_MDSFT_TEXTFILT)) != G_TF_POINT;
			if (linear_filter != rendering_state.textures[i]->linear_filter ||
				rdp.texture_tile.cms != rendering_state.textures[i]->cms ||
				rdp.texture_tile.cmt != rendering_state.textures[i]->cmt) {
				gfx_flush();
			}

			gfx_rapi->set_sampler_parameters(i, linear_filter, rdp.texture_tile.cms, rdp.texture_tile.cmt);
			rendering_state.textures[i]->linear_filter = linear_filter;
			rendering_state.textures[i]->cms = rdp.texture_tile.cms;
			rendering_state.textures[i]->cmt = rdp.texture_tile.cmt;
 
		}
	}

	uint8_t use_texture = !do_ext_fill && (used_textures[0] || used_textures[1]);
	float recip_tex_width;
	float recip_tex_height;

	if (use_texture) {
		uint32_t tex_width = ((rdp.texture_tile.lrs - rdp.texture_tile.uls + 4) * 0.25f);
		uint32_t tex_height = ((rdp.texture_tile.lrt - rdp.texture_tile.ult + 4) * 0.25f);
		recip_tex_width = approx_recip((float)tex_width);
		recip_tex_height = approx_recip((float)tex_height);
	}

	int tri_num_vert = 0;
//	int total_num_vert = do_starfield ? 3 : 6;

	if (use_texture) {
		if (/* __builtin_expect( */do_the_blur/* , 0) */) {
	//        if (tri_num_vert == 0) { // 0 -> upper left
				pre_v_arr[0]->texture.u = 0.0f;
				pre_v_arr[0]->texture.v = 0.0f;
	//        } else if (tri_num_vert == 1 || tri_num_vert == 3) { // 1,4 -> lower left
				pre_v_arr[1]->texture.u = 0.0f;
				pre_v_arr[1]->texture.v = 1.0f;         // 0.93333333f;
	//        } else if (tri_num_vert == 2 || tri_num_vert == 5) { // 2,5->3 -> upper right
				pre_v_arr[2]->texture.u = 1.0f;
				pre_v_arr[2]->texture.v = 1.0f; // 0.93333333f;
				pre_v_arr[3]->texture.u = 1.0f;
				pre_v_arr[3]->texture.v = 0.0f;
	//        } else if (tri_num_vert == 4) { // 4->2 lower right
	//        }
		}
	}
    for (tri_num_vert = 0; tri_num_vert < 4/* total_num_vert */; tri_num_vert++) {
		quad_vbo[tri_num_vert].vert.x = pre_v_arr[tri_num_vert]->vert.x;
		quad_vbo[tri_num_vert].vert.y = pre_v_arr[tri_num_vert]->vert.y;
		quad_vbo[tri_num_vert].vert.z = pre_v_arr[tri_num_vert]->vert.z;

		if (use_texture) {
 			if (/* __builtin_expect( */!do_the_blur/* ,1) */) {
				// div by 32
				float u = (pre_v_arr[tri_num_vert]->texture.u * 0.03125f);
				float v = (pre_v_arr[tri_num_vert]->texture.v * 0.03125f);
				u -= (float)((rdp.texture_tile.uls * 0.25f));
				v -= (float)((rdp.texture_tile.ult * 0.25f));

				if ((rdp.other_mode_h & (3U << G_MDSFT_TEXTFILT)) != G_TF_POINT) {
					// Linear filter adds 0.5f to the coordinates
					u += 0.5f;
					v += 0.5f;
				}
			
				pre_v_arr[tri_num_vert]->texture.u = u * recip_tex_width;
				pre_v_arr[tri_num_vert]->texture.v = v * recip_tex_height;
			}
		}

		struct RGBA tmp = (struct RGBA) { 0xff, 0xff, 0xff, 0xff };
		struct RGBA white = (struct RGBA) { 0xff, 0xff, 0xff, 0xff };
		struct RGBA* color = &white;
		int j, k;

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

				pre_v_arr[tri_num_vert]->color.packed = PACK_ARGB8888(color_r, color_g, color_b, color_a);
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

				float rn, gn, bn, an;
				rn = (float) color_r;
				gn = (float) color_g;
				bn = (float) color_b;
				an = (float) color_a;
                float max_c = 255.0f;
				if (rn > max_c)
					max_c = rn;
				if (gn > max_c)
					max_c = gn;
				if (bn > max_c)
					max_c = bn;
				if (an > max_c)
					max_c = an;

				float rmc = approx_recip(max_c) * 255.0f;
				rn *= rmc;
				gn *= rmc;
				bn *= rmc;
				an *= rmc;

				color_r = (uint32_t) rn;
				color_g = (uint32_t) gn;
				color_b = (uint32_t) bn;
				color_a = (uint32_t) an;
				pre_v_arr[tri_num_vert]->color.packed = PACK_ARGB8888(color_r, color_g, color_b, color_a);
			} else {
				goto thenext2dthing;
			}
		} else {
thenext2dthing:
			for (j = 0; j < num_inputs; j++) {
				/*@Note: use_alpha ? 1 : 0 */
				for (k = 0; k < 1 + (use_alpha ? 0 : 0); k++) {
					switch (comb->shader_input_mapping[k][j]) {
						case CC_PRIM:
							color = &rdp.prim_color;
							break;
						case CC_SHADE:
							tmp.a = v_arr[i]->color.array.a;
							tmp.r = v_arr[i]->color.array.r;
							tmp.g = v_arr[i]->color.array.g;
							tmp.b = v_arr[i]->color.array.b;
							color = &tmp;
							break;
						case CC_ENV:
							color = &rdp.env_color;
							break;
						default:
							color = &white;
							break;
					}
				}
			}

			pre_v_arr[tri_num_vert]->color.array.r = color->r;
			pre_v_arr[tri_num_vert]->color.array.g = color->g;
			pre_v_arr[tri_num_vert]->color.array.b = color->b;
			pre_v_arr[tri_num_vert]->color.array.a = color->a;
		}
	}

	memcpy32(&quad_vbo[0], v_arr[0], 32);
	memcpy32(&quad_vbo[1], v_arr[1], 32);
	memcpy32(&quad_vbo[2], v_arr[2], 32);
	if (!do_starfield) {
		memcpy32(&quad_vbo[3], v_arr[3], 32);
		memcpy32(&quad_vbo[4], v_arr[4], 32);
		memcpy32(&quad_vbo[5], v_arr[5], 32);
	}
	gfx_opengl_draw_triangles_2d((void*) quad_vbo, /* total_num_vert */do_starfield ? 3 : 6, use_texture);
	if (do_the_blur) 
		gfx_flush();
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

static void gfx_update_lookat(uint8_t index, const void *data) {
	if (memcmp(rsp.lookat + ((index - G_MV_LOOKATY) >> 1), data, sizeof(Light_t))) {
		n64_memcpy(rsp.lookat + ((index - G_MV_LOOKATY) >> 1), data, sizeof(Light_t));
		rsp.lights_changed = 1;
	}
}

static void gfx_update_light(uint8_t index, const void* data) {
	if (memcmp(rsp.current_lights + ((index - G_MV_L0) >> 1), data, sizeof(Light_t))) {
		// NOTE: reads out of bounds if it is an ambient light
		n64_memcpy(rsp.current_lights + ((index - G_MV_L0) >> 1), data, sizeof(Light_t));
		rsp.lights_changed = 1;
	}
}

static void  gfx_sp_movemem(uint8_t index, UNUSED uint8_t offset, const void* data) {
	switch (index) {
		case G_MV_VIEWPORT:
			gfx_calc_and_set_viewport((const Vp_t*) data);
			break;
		case G_MV_LOOKATY:
		case G_MV_LOOKATX:
			gfx_update_lookat(index, data);
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
int16_t fog_mul;
int16_t fog_ofs;
extern float gl_fog_start;
extern float gl_fog_end;
extern float gProjectNear;
extern float gProjectFar;


static inline float exp_map_0_1000_f(float x) {
    const float a = 138.62943611198894f;
    if (x <= 0.0f)    return 0.0f;
    if (x >= 1000.0f) return 1000.0f;

    const float t = x * 0.001f;                 // t in [0,1]
    const float num = expm1f(a * (t - 1.0f));    // argument in [-a, 0] (no overflow)
    const float den = expm1f(-a);                // finite (~ -1)
    return 1000.0f * (1.0f - num * approx_recip_sign(den));
}

static void  gfx_sp_moveword(uint8_t index, UNUSED uint16_t offset, uint32_t data) {
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
			fog_mul = rsp.fog_mul = (int16_t) (data >> 16);
			fog_ofs = rsp.fog_offset = (int16_t) data;
	        float recip_fog_mul = approx_recip(fog_mul);
        	float n64_min = 500.0f * (1.0f - (float)fog_ofs * recip_fog_mul);
        	float n64_max = n64_min + 128000.0f * recip_fog_mul;
			/* Convert N64 [0..1000] depth to your eye-space distances [zNear..zFar] */
			float scale = (gProjectFar - gProjectNear) * 0.001f;
			gl_fog_start = gProjectNear + scale * exp_map_0_1000_f(n64_min);
			gl_fog_end   = gProjectNear + scale * exp_map_0_1000_f(n64_max);
			// would be a good place for a
			// fog_dirty = 1;
			break;
		default:
			break;
	}
}

static void gfx_sp_texture(uint16_t sc, uint16_t tc, UNUSED uint8_t level, UNUSED uint8_t tile, UNUSED uint8_t on) {
	rsp.texture_scaling_factor.s = sc;
	rsp.texture_scaling_factor.t = tc;
}

static void gfx_dp_set_scissor(UNUSED uint32_t mode, uint32_t ulx, uint32_t uly, uint32_t lrx,
							   uint32_t lry) {
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

static void gfx_dp_set_texture_image(UNUSED uint32_t format, uint32_t size, uint32_t width,
									 const void* addr) {
	rdp.texture_to_load.addr = segmented_to_virtual((void*)addr);
	rdp.texture_to_load.siz = size;
	last_set_texture_image_width = width;
}


static void  gfx_dp_set_tile(uint8_t fmt, uint32_t siz, uint32_t line, uint32_t tmem, uint8_t tile,
							uint32_t palette, uint32_t cmt,  uint32_t maskt,  uint32_t shiftt,
							uint32_t cms, uint32_t masks, uint32_t shifts) {
	if (tile == G_TX_RENDERTILE) {
		//SUPPORT_CHECK(palette == 0); // palette should set upper 4 bits of color index in 4b mode
		rdp.texture_tile.fmt = fmt;
		rdp.texture_tile.siz = siz;

		if (cms == G_TX_WRAP && masks == G_TX_NOMASK) {
			cms = G_TX_CLAMP;
		}
		if (cmt == G_TX_WRAP && maskt == G_TX_NOMASK) {
			cmt = G_TX_CLAMP;
		}

		rdp.texture_tile.masks = masks;
		rdp.texture_tile.maskt = maskt;
		rdp.texture_tile.cms = cms;
		rdp.texture_tile.cmt = cmt;
		rdp.texture_tile.line_size_bytes = line * 8;
		rdp.texture_tile.shifts = shifts;
		rdp.texture_tile.shiftt = shiftt;
		rdp.textures_changed[0] = 1;
		rdp.textures_changed[1] = 1;
	}

	if (tile == G_TX_LOADTILE) {
		rdp.texture_to_load.tile_number = tmem >> 8;
	} else {
		rdp.texture_to_load.tile_number = tile;
	}
	rdp.texture_to_load.tmem = tmem;
	last_palette = palette;
}

static void  gfx_dp_set_tile_size(uint8_t tile, uint16_t uls, uint16_t ult, uint16_t lrs, uint16_t lrt) {
	if (tile == G_TX_RENDERTILE) {
		rdp.texture_tile.uls = uls;
		rdp.texture_tile.ult = ult;
		rdp.texture_tile.lrs = lrs;
		rdp.texture_tile.lrt = lrt;
		rdp.textures_changed[0] = 1;
		rdp.textures_changed[1] = 1;
	}
}

static void  __attribute__((noinline)) gfx_dp_load_tlut(UNUSED uint8_t tile, uint32_t high_index) {
	rdp.palette = rdp.texture_to_load.addr;

	uint16_t* srcp = segmented_to_virtual((void*)rdp.texture_to_load.addr);

	uint16_t* tlp = tlut;
	for (int i = 0; i < high_index; i++) {
		uint16_t c = *srcp++;
		*tlp++ = brightit_argb1555(((c & 1) << 15) | (c >> 1));
	}
}

static void  gfx_dp_load_block(UNUSED uint8_t tile, UNUSED uint32_t uls, UNUSED uint32_t ult, uint32_t lrs,
							  UNUSED uint32_t dxt) {
	// The lrs field rather seems to be number of pixels to load
	uint32_t word_size_shift = 0;
	switch (rdp.texture_to_load.siz) {
		case G_IM_SIZ_4b:
			word_size_shift = 0; // Or -1? It's unused in SM64 anyway.
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
	rdp.loaded_texture[rdp.texture_to_load.tile_number].addr = rdp.texture_to_load.addr;

	rdp.textures_changed[rdp.texture_to_load.tile_number] = 1;
	last_set_texture_image_width = 0;
}

static void gfx_dp_load_tile(UNUSED uint8_t tile, uint32_t uls, uint32_t ult, uint32_t lrs, uint32_t lrt) {
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
	return color_comb_component(a) | (color_comb_component(b) << 3) | (color_comb_component(c) << 6) | (color_comb_component(d) << 9);
}
static void gfx_dp_set_combine_mode(uint32_t rgb, uint32_t alpha) {
	rdp.combine_mode = rgb | (alpha << 12);
}


static void  gfx_dp_set_env_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
#if 0
	float r01 = (float)r * recip255;
	float g01 = (float)g * recip255;
	float b01 = (float)b * recip255;

	r01 = linear_to_srgb1(r01);
	g01 = linear_to_srgb1(g01);
	b01 = linear_to_srgb1(b01);

	r = (uint8_t)(r01*255.0f);
	g = (uint8_t)(g01*255.0f);
	b = (uint8_t)(b01*255.0f);
#endif
	er = rdp.env_color.r = table32[r>>3]<<3;
	eg = rdp.env_color.g = table32[g>>3]<<3;
	eb = rdp.env_color.b = table32[b>>3]<<3;
	ea = rdp.env_color.a = a;
}

static void gfx_dp_set_prim_color(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
#if 0
	float r01 = (float)r * recip255;
	float g01 = (float)g * recip255;
	float b01 = (float)b * recip255;

	r01 = linear_to_srgb1(r01);
	g01 = linear_to_srgb1(g01);
	b01 = linear_to_srgb1(b01);

	r = (uint8_t)(r01*255.0f);
	g = (uint8_t)(g01*255.0f);
	b = (uint8_t)(b01*255.0f);
#endif
	pr = rdp.prim_color.r = table32[r>>3]<<3;
	pg = rdp.prim_color.g = table32[g>>3]<<3;
	pb = rdp.prim_color.b = table32[b>>3]<<3;
	pa = rdp.prim_color.a = a;
}



typedef enum LevelType {
    /* 0 */ LEVELTYPE_PLANET,
    /* 1 */ LEVELTYPE_SPACE,
} LevelType;

extern uint8_t gLevelType;

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
		r= 100;
		g= 100;
		b= 120;
	} else if (gCurrentLevel == LEVEL_KATINA) {
		r = 97;
		g = 90;
		b = 90;
	} else if (gCurrentLevel == LEVEL_TITANIA) {
		r = 161;
		g = 80;
		b = 29;
	} else {
#if 0
		float r01 = (float)r * recip255;
		float g01 = (float)g * recip255;
		float b01 = (float)b * recip255;

		r01 = linear_to_srgb1(r01);
		g01 = linear_to_srgb1(g01);
		b01 = linear_to_srgb1(b01);

		r = (uint8_t)(r01*255.0f);
		g = (uint8_t)(g01*255.0f);
		b = (uint8_t)(b01*255.0f);
#endif
		r = table32[r>>3]<<3;
		g = table32[g>>3]<<3;
		b = table32[b>>3]<<3;
	}
	rdp.fog_color.r = r;
	rdp.fog_color.g = g;
	rdp.fog_color.b = b;
	rdp.fog_color.a = a;

}

static void  gfx_dp_set_fill_color(uint32_t packed_color) {
	uint16_t col16 = (uint16_t) packed_color;
	uint32_t r = (col16 >> 11) & 0x1f;
	uint32_t g = (col16 >> 6) & 0x1f;
	uint32_t b = (col16 >> 1) & 0x1f;
	uint32_t a = col16 & 1;
#if 0
	float r01 = (float)r * recip31;
	float g01 = (float)g * recip31;
	float b01 = (float)b * recip31;

	r01 = linear_to_srgb1(r01);
	g01 = linear_to_srgb1(g01);
	b01 = linear_to_srgb1(b01);

	r = (uint8_t)(r01*255.0f);
	g = (uint8_t)(g01*255.0f);
	b = (uint8_t)(b01*255.0f);
#endif
	rdp.fill_color.r = table32[r]<<3;
	rdp.fill_color.g = table32[g]<<3;
	rdp.fill_color.b = table32[b]<<3;
	rdp.fill_color.a = a * 255;
}

float screen_2d_z;


// in here instead of gfx_gldc.c becasue of `matrix_dirty` and `rsp` references
void  __attribute__((noinline)) gfx_opengl_2d_projection(void) {
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(0, 640, 480, 0, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	matrix_dirty = 1;
}


void  __attribute__((noinline)) gfx_opengl_reset_projection(void) {
	glMatrixMode(GL_PROJECTION);
	glLoadMatrixf((const float*) rsp.P_matrix);
	glMatrixMode(GL_MODELVIEW);
	glLoadMatrixf((const float*) rsp.modelview_matrix_stack[rsp.modelview_matrix_stack_size - 1]);
	matrix_dirty = 1;
}


#define recip_4timeshalfscrwid 0.0015625f
#define recip_4timeshalfscrhgt 0.00208333f

static void  __attribute__((noinline)) gfx_draw_rectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry) {
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
		ulxf = -1.0f;//(ulxf * recip_4timeshalfscrwid) - 1.0f;
		ulyf = -1.0f;//(ulyf * recip_4timeshalfscrhgt) - 1.0f;
		lrxf = 0.99375f;//1276;//(lrxf * recip_4timeshalfscrwid) - 1.0f;
		lryf = 0.99166348f;//0.85833036;// 956;//(lryf * recip_4timeshalfscrhgt) - 1.0f;
	} else {
		ulxf = (ulxf * recip_4timeshalfscrwid) - 1.0f;
		ulyf = (ulyf * recip_4timeshalfscrhgt) - 1.0f;
		lrxf = (lrxf * recip_4timeshalfscrwid) - 1.0f;
		lryf = (lryf * recip_4timeshalfscrhgt) - 1.0f;
	}
	ulxf = (ulxf * 320.0f) + 320.0f;
	lrxf = (lrxf * 320.0f) + 320.0f;

	ulyf = (ulyf * 240.0f) + 240.0f;
	lryf = (lryf * 240.0f) + 240.0f;

	dc_fast_t* ul = &rsp.loaded_vertices_2D[0];
	dc_fast_t* ll = &rsp.loaded_vertices_2D[1];
	dc_fast_t* lr = &rsp.loaded_vertices_2D[2];
	dc_fast_t* ur = &rsp.loaded_vertices_2D[3];

	screen_2d_z += 0.0005f;
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

static void  __attribute__((noinline)) gfx_dp_texture_rectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry, UNUSED uint8_t tile,
									 int16_t uls, int16_t ult, int16_t dsdx, int16_t dtdy, uint8_t flip) {
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
	ul->texture.u = uls;
	ul->texture.v = ult;
	lr->texture.u = lrs;
	lr->texture.v = lrt;

	if (!flip) {
		ll->texture.u = uls;
		ll->texture.v = lrt;
		ur->texture.u = lrs;
		ur->texture.v = ult;
	} else {
		ll->texture.u = lrs;
		ll->texture.v = ult;
		ur->texture.u = uls;
		ur->texture.v = lrt;
	}

	gfx_draw_rectangle(ulx, uly, lrx, lry);
	rdp.combine_mode = saved_combine_mode;
}


static void  __attribute__((noinline)) gfx_dp_fill_rectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry) {
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

	for (i = 0; i < 4; i++) {
		dc_fast_t* v = &rsp.loaded_vertices_2D[i];
		v->color.array.a = rdp.fill_color.a;
		v->color.array.b = rdp.fill_color.b;
		v->color.array.g = rdp.fill_color.g;
		v->color.array.r = rdp.fill_color.r;
	}

//	uint32_t saved_combine_mode = rdp.combine_mode;
//	gfx_dp_set_combine_mode(color_comb(0, 0, 0, G_CCMUX_SHADE), color_comb(0, 0, 0, G_ACMUX_SHADE));
	gfx_draw_rectangle(ulx, uly, lrx, lry);
//	rdp.combine_mode = saved_combine_mode;
	rsp.geometry_mode = saved_geom_mode;
	rdp.other_mode_l = saved_other_mode_l;
	do_ext_fill = 0;
}

static void gfx_dp_set_z_image(void* z_buf_address) {
	rdp.z_buf_address = z_buf_address;
}

static void  gfx_dp_set_color_image(UNUSED uint32_t format, UNUSED uint32_t size, UNUSED uint32_t width, void* address) {
	rdp.color_image_address = address;
}

static void  gfx_sp_set_other_mode(uint32_t shift, uint32_t num_bits, uint64_t mode) {
	uint64_t mask = (((uint64_t) 1 << num_bits) - 1) << shift;
	uint64_t om = rdp.other_mode_l | ((uint64_t) rdp.other_mode_h << 32);
	om = (om & ~mask) | mode;
	rdp.other_mode_l = (uint32_t) om;
	rdp.other_mode_h = (uint32_t) (om >> 32);
}

static inline void* seg_addr(uintptr_t w1) {
	return (void*) segmented_to_virtual((void*) w1);
}

#define C0(pos, width) ((cmd->words.w0 >> (pos)) & ((1U << width) - 1))
#define C1(pos, width) ((cmd->words.w1 >> (pos)) & ((1U << width) - 1))
#define G_QUAD (G_IMMFIRST - 10)

volatile int do_reticle = 0;
int do_fillrect_blend = 0;
#if 0
extern Gfx aCoBuilding1DL[];
extern uint32_t cob1_uls, cob1_ult, cob1_lrs, cob1_lrt;
extern Gfx aSoLava1DL[];
extern Gfx aSoLava2DL[];
extern uint32_t sol_uls, sol_ult, sol_lrs, sol_lrt;
extern Gfx D_10182C0[];
extern uint32_t e383_uls, e383_ult, e383_lrs, e383_lrt;
#endif
static void  __attribute__((noinline)) gfx_run_dl(Gfx* cmd) {
	cmd = seg_addr((uintptr_t) cmd);
	for (;;) {
#if 0
		if (cmd == (Gfx *)segmented_to_virtual((void *)((Gfx*)(aCoBuilding1DL + 36)))) {
			cmd->words.w0 = (G_SETTILESIZE << 24) | (cob1_uls << 12) | cob1_ult;
            cmd->words.w1 = (cmd->words.w1 & 0x07000000) | (cob1_lrs << 12) | cob1_lrt;
		}

		if (cmd == (Gfx *)segmented_to_virtual((void *)((Gfx*)(aSoLava1DL + 2)))) {
			cmd->words.w0 = (G_SETTILESIZE << 24) | (sol_uls << 12) | sol_ult;
            cmd->words.w1 = (cmd->words.w1 & 0x07000000) | (sol_lrs << 12) | sol_lrt;
		}

		if (cmd == (Gfx *)segmented_to_virtual((void *)((Gfx*)(aSoLava2DL + 2)))) {
			cmd->words.w0 = (G_SETTILESIZE << 24) | (sol_uls << 12) | sol_ult;
            cmd->words.w1 = (cmd->words.w1 & 0x07000000) | (sol_lrs << 12) | sol_lrt;
		}

		if (cmd == (Gfx *)segmented_to_virtual((void *)((Gfx*)(D_10182C0 + 6)))) {
			cmd->words.w0 = (G_SETTILESIZE << 24) | (e383_uls << 12) | e383_ult;
            cmd->words.w1 = (cmd->words.w1 & 0x07000000) | (e383_lrs << 12) | e383_lrt;
		}
#endif
		uint32_t opcode = cmd->words.w0 >> 24;

		// custom f3d opcodes, sorry
		if (cmd->words.w0 == 0x424C4E44) {
			if(cmd->words.w1 == 0x46554350) {
				do_reticle ^= 1;
			} else if(cmd->words.w1 == 0x46554370) {
				do_starfield ^= 1;
			} else if(cmd->words.w1 == 0x46554380) {
				do_fillrect_blend ^= 1;
			} else if(cmd->words.w1 == 0x465543F0) {
				do_floorscroll ^= 1;
			} else if((cmd->words.w1) == 0x465543DD) {
				do_zfight ^= 1;
				do_zflip = 0;
			} else if((cmd->words.w1) == 0x465543DE) {
				do_zfight ^= 1;
				do_zflip = 1;
			} else if((cmd->words.w1) == 0x465543DF) {
				do_zfight ^= 1;
				do_zflip = 2;
//				0x465543EE
			} else if((cmd->words.w1) == 0x465543EE) {
				do_menucard ^= 1;
			} else if((cmd->words.w1) == 0x46554369) {
				do_rectdepfix ^= 1;
			} else if((cmd->words.w1) == 0x46554360) {
				do_the_blur ^= 1;
			}

			++cmd;
			continue;
		}

		switch (opcode) {
			// RSP commands:
			case G_MTX:
				gfx_flush();
				gfx_sp_matrix(C0(16, 8), (const void*) seg_addr(cmd->words.w1));
				break;

			case (uint8_t) G_POPMTX:
				//gfx_flush();
				gfx_sp_pop_matrix(1);
				break;

			case G_MOVEMEM:
				gfx_sp_movemem(C0(16, 8), 0, seg_addr(cmd->words.w1));
				break;

			case (uint8_t) G_MOVEWORD:
				gfx_sp_moveword(C0(0, 8), C0(8, 16), cmd->words.w1);
				break;

			case (uint8_t) G_TEXTURE:
				gfx_sp_texture(C1(16, 16), C1(0, 16), C0(11, 3), C0(8, 3), C0(0, 8));
				break;

			case G_VTX:
				gfx_sp_vertex(C0(10, 6), C0(16, 8) / 2, seg_addr(cmd->words.w1));
				break;

			case G_DL:
				if (C0(16, 1) == 0) {
					// Push return address
					gfx_run_dl((Gfx*) seg_addr(cmd->words.w1));
				} else {
					cmd = (Gfx*) seg_addr(cmd->words.w1);
					--cmd; // increase after break
				}
				break;

			case (uint8_t) G_ENDDL:
				return;

			case (uint8_t) G_SETGEOMETRYMODE:
				gfx_sp_geometry_mode(0, cmd->words.w1);
				break;

			case (uint8_t) G_CLEARGEOMETRYMODE:
				gfx_sp_geometry_mode(cmd->words.w1, 0);
				break;

			case (uint8_t) G_QUAD:
				// C1(24, 8) / 2   3
				// C1(16, 8) / 2   0
				// C1(8, 8) / 2    1
				// C1(0, 8) / 2    2
				gfx_sp_tri1(C1(16, 8) >> 1, C1(8, 8) >> 1, C1(24, 8) >> 1);
				gfx_sp_tri1(C1(8, 8) >> 1, C1(0, 8) >> 1, C1(24, 8) >> 1);
//				gfx_sp_tri1(C1(16, 8) / 2, C1(8, 8) / 2, C1(0, 8) /2);
//				gfx_sp_tri1(C1(16, 8) / 2, C1(0, 8) / 2, C1(24, 8)/2);
				break;

			case (uint8_t) G_TRI1:
				gfx_sp_tri1(C1(16, 8) /2, C1(8, 8) /2, C1(0, 8) /2 );
				break;

			case (uint8_t) G_TRI2:
//				gfx_sp_tri1(C0(16, 8) >> 1, C0(8, 8) >> 1, C0(0, 8) >> 1);
//				gfx_sp_tri1(C1(16, 8) >> 1, C1(8, 8) >> 1, C1(0, 8) >> 1);
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
				gfx_dp_set_texture_image(C0(21, 3), C0(19, 2), C0(0, 10), cmd->words.w1);
				break;

			case G_LOADBLOCK:
				gfx_dp_load_block(C1(24, 3), C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
				break;

			case G_LOADTILE:
				gfx_dp_load_tile(C1(24, 3), C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
				break;

			case G_SETTILE:
				gfx_dp_set_tile(C0(21, 3), C0(19, 2), C0(9, 9), C0(0, 9), C1(24, 3), C1(20, 4), C1(18, 2), C1(14, 4),
								C1(10, 4), C1(8, 2), C1(4, 4), C1(0, 4));
				break;

			case G_SETTILESIZE:
				gfx_dp_set_tile_size(C1(24, 3), C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
				break;

			case G_LOADTLUT:
				gfx_dp_load_tlut(C1(24, 3), C1(14, 10));
				break;

			case G_SETENVCOLOR:
				__builtin_prefetch(table32);
				gfx_dp_set_env_color(C1(24, 8), C1(16, 8), C1(8, 8), C1(0, 8));
				break;

			case G_SETPRIMCOLOR:
				__builtin_prefetch(table32);
				gfx_dp_set_prim_color(C1(24, 8), C1(16, 8), C1(8, 8), C1(0, 8));
				break;
				
			case G_SETFOGCOLOR:
				__builtin_prefetch(table32);
				gfx_dp_set_fog_color(C1(24, 8), C1(16, 8), C1(8, 8), C1(0, 8));
				break;

			case G_SETFILLCOLOR:
				__builtin_prefetch(table32);
				gfx_dp_set_fill_color(cmd->words.w1);
				break;

			case G_SETCOMBINE:
				gfx_dp_set_combine_mode(color_comb(C0(20, 4), C1(28, 4), C0(15, 5), C1(15, 3)),
										color_comb(C0(12, 3), C1(12, 3), C0(9, 3), C1(9, 3)));
				break;

			case G_TEXRECT:
			case G_TEXRECTFLIP: {
				int32_t lrx, lry, tile, ulx, uly;
				uint32_t uls, ult, dsdx, dtdy;
				lrx = C0(12, 12);
				lry = C0(0, 12);
				tile = C1(24, 3);
				ulx = C1(12, 12);
				uly = C1(0, 12);
				++cmd;
				uls = C1(16, 16);
				ult = C1(0, 16);
				++cmd;
				dsdx = C1(16, 16);
				dtdy = C1(0, 16);
				gfx_dp_texture_rectangle(ulx, uly, lrx, lry, tile, uls, ult, dsdx, dtdy, opcode == G_TEXRECTFLIP);
				break;
			}

			case G_FILLRECT:
				gfx_dp_fill_rectangle(C1(12, 12), C1(0, 12), C0(12, 12), C0(0, 12));
				break;

			case G_SETSCISSOR:
				gfx_dp_set_scissor(C1(24, 2), C0(12, 12), C0(0, 12), C1(12, 12), C1(0, 12));
				break;

			case G_SETZIMG:
				gfx_dp_set_z_image(seg_addr(cmd->words.w1));
				break;

			case G_SETCIMG:
				gfx_dp_set_color_image(C0(21, 3), C0(19, 2), C0(0, 11), seg_addr(cmd->words.w1));
				break;
		}
		++cmd;
	}
}

static void gfx_sp_reset() {
	rsp.modelview_matrix_stack_size = 1;
	rsp.current_num_lights = 0;
	rsp.lights_changed = 1;
	rsp.lookat[LOOKAT_Y_IDX].dir[0] = 0;
	rsp.lookat[LOOKAT_Y_IDX].dir[1] = 127;
	rsp.lookat[LOOKAT_Y_IDX].dir[2] = 0;
	rsp.lookat[LOOKAT_X_IDX].dir[0] = 127;
	rsp.lookat[LOOKAT_X_IDX].dir[1] = 0;
	rsp.lookat[LOOKAT_X_IDX].dir[2] = 0;
	rendering_state.textures[0]->cms = 6;
	rendering_state.textures[0]->cmt = 6;
	rendering_state.textures[1]->cms = 6;
	rendering_state.textures[1]->cmt = 6;
	alpha_noise = 0;
//    calculate_normal_dir(&rsp.lookat[0], rsp.current_lookat_coeffs[0]);
//    calculate_normal_dir(&rsp.lookat[1], rsp.current_lookat_coeffs[1]);
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
#if 0
printf("\n");
for(float x=0.0f;x<1.0f;x+=0.01f) {
	printf("%f, ", sqrtf(x));
}
printf("\n");
for(float x=0.0f;x<1.0f;x+=0.01f) {
printf("%f, ", shz_sqrtf_fsrra(x));
}
printf("\n");
for(float x=0.0f;x<1.0f;x+=0.01f) {
	printf("%f, ", sqrtf(x) - shz_sqrtf_fsrra(x));
}
#endif
 	for (i = 0; i < 256; i++) {
		table256[i] = 255.0f * shz_sqrtf_fsrra(((float)i/255.0f) + 0.0000001f);
	}
	for (i = 0; i < 32; i++) {
		table32[i] = 31.0f * shz_sqrtf_fsrra(((float)i/31.0f) + 0.0000001f);
	}
	for (i = 0; i < 16; i++) {
		table16[i] = 15.0f * shz_sqrtf_fsrra(((float)i/15.0f) + 0.0000001f);
	}

	// Used in the 120 star TAS
	/*static uint32_t precomp_shaders[] = { 0x01200200, 0x00000045, 0x00000200, 0x01200a00, 0x00000a00, 0x01a00045,
										  0x00000551, 0x01045045, 0x05a00a00, 0x01200045, 0x05045045, 0x01045a00,
										  0x01a00a00, 0x0000038d, 0x01081081, 0x0120038d, 0x03200045, 0x03200a00,
										  0x01a00a6f, 0x01141045, 0x07a00a00, 0x05200200, 0x03200200, 0x09200200,
										  0x0920038d, 0x09200045 };
	for (i = 0; i < sizeof(precomp_shaders) / sizeof(uint32_t); i++) {
		gfx_lookup_or_create_shader_program(precomp_shaders[i]);
	}*/
	gfx_wapi->get_dimensions(&gfx_current_dimensions.width, &gfx_current_dimensions.height);
	if (gfx_current_dimensions.height == 0) {
		// Avoid division by zero
		gfx_current_dimensions.height = 1;
	}
	gfx_current_dimensions.aspect_ratio = (float) gfx_current_dimensions.width / (float) gfx_current_dimensions.height;

	oops_texture_id = gfx_rapi->new_texture();
	gfx_rapi->select_texture(0, oops_texture_id);
	gfx_rapi->upload_texture((uint8_t*) rgba16_buf, 64, 64, GL_UNSIGNED_SHORT_4_4_4_4_REV);

	memset(&oops_node, 0, sizeof(oops_node));
	oops_node.texture_id = oops_texture_id;
}


struct GfxRenderingAPI* gfx_get_current_rendering_api(void) {
	return gfx_rapi;
}


void gfx_start_frame(void) {
	gfx_wapi->handle_events();
}


void gfx_run(Gfx* commands) {
	gfx_sp_reset();

	if (!gfx_wapi->start_frame()) {
		dropped_frame = 1;
		return;
	}

	dropped_frame = 0;

	gfx_rapi->start_frame();
	gfx_run_dl(commands);
	gfx_flush();
	gfx_rapi->end_frame();
	gfx_wapi->swap_buffers_begin();
}


void gfx_end_frame(void) {
//	printf("total %d lookup %d found %d miss rate %f\n", gfx_texture_cache.pool_pos, tc_lookup, tc_found, 1.0f - ((float)tc_found/(float)tc_lookup));
	if (!dropped_frame) {
		gfx_rapi->finish_render();
		gfx_wapi->swap_buffers_end();
	}
	tc_lookup = 0;
	tc_found = 0;
}





