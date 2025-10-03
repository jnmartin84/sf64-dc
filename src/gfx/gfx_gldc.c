#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif

#define	G_TX_LOADTILE	7
#define	G_TX_RENDERTILE	0

#define	G_TX_NOMIRROR	0
#define	G_TX_WRAP	0
#define	G_TX_MIRROR	0x1
#define	G_TX_CLAMP	0x2
#define	G_TX_NOMASK	0
#define	G_TX_NOLOD	0

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h>
#include <assert.h>
#include <unistd.h>

#include <GL/gl.h>
#include <GL/glu.h>
#include <GL/glext.h>
#include <GL/glkos.h>

#define FOR_WINDOWS 0

#include "gfx_cc.h"
#include "gfx_rendering_api.h"
#include "macros.h"
#include "gl_fast_vert.h"

typedef enum LevelType {
    /* 0 */ LEVELTYPE_PLANET,
    /* 1 */ LEVELTYPE_SPACE,
} LevelType;

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

extern uint8_t gLevelType;

#define s32 long int
#define u16 unsigned short

enum MixType {
    SH_MT_NONE,
    SH_MT_TEXTURE,
    SH_MT_COLOR,
    SH_MT_TEXTURE_TEXTURE,
    SH_MT_TEXTURE_COLOR,
    SH_MT_COLOR_COLOR,
};

struct ShaderProgram {
    uint8_t enabled;
    uint32_t shader_id;
    struct CCFeatures cc;
    enum MixType mix;
    uint8_t texture_used[2];
    int texture_ord[2];
    int num_inputs;
};

struct SamplerState {
    GLuint srcaddr;
    GLenum min_filter;
    GLenum mag_filter;
    GLenum wrap_s;
    GLenum wrap_t;
    GLuint tex;
};

extern u16 aAqWaterTex[];

uint32_t shaderlist[64];
uint8_t shaderidx;

static struct ShaderProgram shader_program_pool[64];
static uint8_t shader_program_pool_size;
static struct ShaderProgram* cur_shader = NULL;

static struct SamplerState tmu_state[2];

static const dc_fast_t* cur_buf = NULL;
static uint8_t gl_blend = 0;
static uint8_t gl_depth = 0;

static void resample_32bit(const uint16_t* in, int inwidth, int inheight, uint16_t* out, int outwidth, int outheight ) {
    int i, j;
    __builtin_prefetch(in);
    float scale = (float)inheight / (float)outheight;
    uint32_t* out32 = (uint32_t*)out;
    int fracstep = (inwidth << 16) / outwidth;
    int outwidth32 = outwidth >> 1; // two pixels per 32-bit write

    for (i = 0; i < outheight; i++, out32 += outwidth32) {
        const uint16_t* inrow = in + inwidth * (int)((float)i * scale);
        int frac = fracstep >> 1;

        for (j = 0; j < outwidth32; j++) {

            uint16_t p1 = inrow[frac >> 16];
            frac += fracstep;
            uint16_t p2 = inrow[frac >> 16];
            frac += fracstep;

            out32[j] = ((uint32_t)p2 << 16) | p1;
        }
        __builtin_prefetch(inrow + 32);
    }
}




static inline uint32_t next_pot(uint32_t v) {
    if (v <= 8)
        return 8;
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v++;
    return v;
}


static inline uint32_t prev_pot(uint32_t n) {
    if (n <= 16) return 8;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    return n - (n >> 1);
}


static inline uint32_t is_pot(const uint32_t v) {
    return (v & (v - 1)) == 0;
}


static uint8_t gfx_opengl_z_is_from_0_to_1(void) {
    return 1;
}


static inline GLenum texenv_set_color(UNUSED struct ShaderProgram* prg) {
    return GL_MODULATE;
}


static inline GLenum texenv_set_texture(UNUSED struct ShaderProgram* prg) {
    return GL_MODULATE;
}


static inline GLenum texenv_set_texture_color(struct ShaderProgram* prg) {
    GLenum mode = GL_MODULATE;

    switch(prg->shader_id) {
        case 0x0000038D: // ???
        case 0x00045A00: // ???
            mode = GL_DECAL;
            break;
        case 0x01a00a00: // ???
            mode = GL_DECAL;
            break;
        case 0x00000551: // ???
            mode = GL_REPLACE;
            break;
    }

    return mode;
}

static inline GLenum texenv_set_texture_texture(UNUSED struct ShaderProgram* prg) {
    return GL_MODULATE;
}
extern int16_t fog_mul;
extern int16_t fog_ofs;
extern float gProjectNear;
extern float gProjectFar;

extern int shader_debug_toggle;

#include <kos.h>


static inline float approx_recip_sign(float v) {
	float _v = 1.0f / sqrtf(v * v);
	return copysignf(_v, v);
}

static inline float approx_recip(float v) {
	return 1.0f / sqrtf(v * v);
}


static inline float exp_map_0_1000_f(float x) {
    const float a = 138.62943611198894f;
    if (x <= 0.0f)    return 0.0f;
    if (x >= 1000.0f) return 1000.0f;

    const float t = x * 0.001f;                 // t in [0,1]
    const float num = expm1f(a * (t - 1.0f));    // argument in [-a, 0] (no overflow)
    const float den = expm1f(-a);                // finite (~ -1)
    return 1000.0f * (1.0f - num * approx_recip_sign(den));
}


static inline float exp_map_custom_f(float x) {
    const float a    = 110.0f;
    const float ymax = 990.0f;
    const float den  = expm1f(-a);

    if (x <= 0.0f)    return 0.0f;
    if (x >= 1000.0f) return ymax;

    const float t   = x * 0.001f;
    const float num = expm1f(a * (t - 1.0f));
    return ymax * (1.0f - num * approx_recip_sign(den));
}

float n64_min;
float n64_max;
float gl_fog_start;
float gl_fog_end;

static void gfx_opengl_apply_shader(struct ShaderProgram* prg) {
    // vertices are always there
    glVertexPointer(3, GL_FLOAT, sizeof(dc_fast_t), &cur_buf[0].vert);
    glTexCoordPointer(2, GL_FLOAT, sizeof(dc_fast_t), &cur_buf[0].texture);
    glColorPointer(GL_BGRA, GL_UNSIGNED_BYTE, sizeof(dc_fast_t), &cur_buf[0].color);

#if 0
    if (shader_debug_toggle) {
        int shaderfound = 0;
        for (int i = 0; i < shaderidx; i++) {
            if (shaderlist[i] == prg->shader_id) {
                shaderfound = 1;
                break;
            }
        }
        if (!shaderfound) {
            shaderlist[shaderidx++] = prg->shader_id;
        }
    }
#endif

    // have texture(s), specify same texcoords for every active texture
    if (prg->texture_used[0] || prg->texture_used[1]) {
        glEnable(GL_TEXTURE_2D);
    } else {
        glDisable(GL_TEXTURE_2D);
    }

    if (prg->shader_id & SHADER_OPT_FOG) {
        float fog_scale;
        if (gCurrentLevel == LEVEL_ZONESS || gCurrentLevel == LEVEL_TITANIA || gCurrentLevel == LEVEL_SOLAR) 
            fog_scale = 0.4f;
        else
            fog_scale = 0.6f;

        glEnable(GL_FOG);

        glFogi(GL_FOG_MODE, GL_LINEAR);
        glFogf(GL_FOG_START, (GLfloat)gl_fog_start * fog_scale);
        glFogf(GL_FOG_END,   (GLfloat)  gl_fog_end * fog_scale);      
    } else {
        glDisable(GL_FOG);
    }

    if (prg->shader_id & SHADER_OPT_TEXTURE_EDGE) {
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 0.125); //1.0f/8.0f);
        glDisable(GL_BLEND);
    } else {
        glDisable(GL_ALPHA_TEST);
    }

    // configure texenv
    GLenum mode;
    switch (prg->mix) {
    case SH_MT_TEXTURE:
        mode = texenv_set_texture(prg);
        break;
    case SH_MT_TEXTURE_TEXTURE:
        mode = texenv_set_texture_texture(prg);
        break;
    case SH_MT_TEXTURE_COLOR:
        mode = texenv_set_texture_color(prg);
        break;
    default:
        mode = texenv_set_color(prg);
        break;
    }

    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, mode);
}


static void gfx_opengl_unload_shader(struct ShaderProgram* old_prg) {
    if (cur_shader && ((cur_shader == old_prg) || (NULL == old_prg))) {
        cur_shader->enabled = 0;
        cur_shader = NULL;
    }
}

static void gfx_opengl_load_shader(struct ShaderProgram* new_prg) {
    cur_shader = new_prg;
    if (cur_shader)
        cur_shader->enabled = 0;
    if (cur_shader->texture_used[1]) {
        printf("shader %08x uses texture[1]\n", cur_shader->shader_id);
    }
}

static struct ShaderProgram* gfx_opengl_create_and_load_new_shader(uint32_t shader_id) {
    struct CCFeatures ccf;
    gfx_cc_get_features(shader_id, &ccf);

    struct ShaderProgram* prg = &shader_program_pool[shader_program_pool_size++];

    prg->shader_id = shader_id;
    prg->cc = ccf;
    prg->num_inputs = ccf.num_inputs;
    prg->texture_used[0] = ccf.used_textures[0];
    prg->texture_used[1] = ccf.used_textures[1];

    if (ccf.used_textures[0] && ccf.used_textures[1]) {
        printf("multitex\n");   
        prg->mix = SH_MT_TEXTURE_TEXTURE;
        if (ccf.do_single[1]) {
            prg->texture_ord[0] = 1;
            prg->texture_ord[1] = 0;
        } else {
            prg->texture_ord[0] = 0;
            prg->texture_ord[1] = 1;
        }
    } else if (ccf.used_textures[0] && ccf.num_inputs) {
        prg->mix = SH_MT_TEXTURE_COLOR;
    } else if (ccf.used_textures[0]) {
        prg->mix = SH_MT_TEXTURE;
    } else if (ccf.num_inputs > 1) {
        prg->mix = SH_MT_COLOR_COLOR;
    } else if (ccf.num_inputs) {
        prg->mix = SH_MT_COLOR;
    }

    prg->enabled = 0;

    gfx_opengl_load_shader(prg);

    return prg;
}

static struct ShaderProgram* gfx_opengl_lookup_shader(uint32_t shader_id) {
    size_t i;

    for (i = 0; i < shader_program_pool_size; i++)
        if (shader_program_pool[i].shader_id == shader_id)
            return &shader_program_pool[i];

    return NULL;
}

static void gfx_opengl_shader_get_info(struct ShaderProgram* prg, uint8_t* num_inputs, uint8_t used_textures[2]) {
    *num_inputs = prg->num_inputs;
    used_textures[0] = prg->texture_used[0];
    used_textures[1] = prg->texture_used[1];

#if 0
    if (shader_debug_toggle) {
        int shaderfound = 0;
        for (int i = 0; i < shaderidx; i++) {
            if (shaderlist[i] == prg->shader_id) {
                shaderfound = 1;
                break;
            }
        }
        if (!shaderfound) {
            shaderlist[shaderidx++] = prg->shader_id;
        }
    }
#endif
}

GLuint newest_texture = 0;


static void gfx_clear_all_textures(void) {
    GLuint index = 0;
    if (newest_texture != 0) {
        for (index = 2; index <= newest_texture; index++)
            glDeleteTextures(1, &index);

        tmu_state[0].tex = 0;
        tmu_state[1].tex = 0;
    }
    newest_texture = 0;
}


void gfx_clear_texidx(GLuint texidx) {
    GLuint index = texidx;
    glDeleteTextures(0, &index);
    if (tmu_state[0].tex == texidx)
        tmu_state[0].tex = 0;
    if (tmu_state[1].tex == texidx)
        tmu_state[1].tex = 0;
}


static uint32_t gfx_opengl_new_texture(void) {
    GLuint ret;
    glGenTextures(1, &ret);
    newest_texture = ret;
//    printf("new tex %d\n", ret);
    return (uint32_t) ret;
}


void gfx_opengl_set_tile_addr(int tile, GLuint addr) {
    tmu_state[tile].srcaddr = (GLuint) addr;
}


static void gfx_opengl_select_texture(int tile, uint32_t texture_id) {
    tmu_state[tile].tex = texture_id; // remember this for multitexturing later
    glBindTexture(GL_TEXTURE_2D, texture_id);
}


/* Used for rescaling textures into pow2 dims */
/* static */ uint8_t __attribute__((aligned(32))) scaled[64 * 64 * 8 * 2];

/* static */ uint16_t __attribute__((aligned(32))) scaled2[128*256];//64 * 64 * 8 * 2];


#define LET_GLDC_TWIDDLE 1

static inline uint16_t rgb565_to_argb1555(uint16_t rgb565) {
    // Extract components from RGB565
    uint8_t r5 = (rgb565 >> 11) & 0x1F;         // 5 bits red
    uint8_t g6 = (rgb565 >> 5) & 0x3F;          // 6 bits green
    uint8_t b5 = rgb565 & 0x1F;                 // 5 bits blue

    // Convert 6-bit green to 5-bit by shifting (losing LSB)
    uint8_t g5 = (g6 >> 1) & 0x1F;

    // Alpha = 1 (opaque)
    uint8_t a1 = 1;

    // Pack into RGBA5551
    uint16_t rgba5551 = (a1 << 15) | (r5 << 10) | (g5 << 5) | b5;

    return rgba5551;
}

void capture_framebuffer(int num) {
#if 1
    const uint16_t* in = vram_s;// + (num * 640 * 120);
    int inwidth = 640;
    int inheight = 480;//120;
    uint16_t* out = scaled2;// + (num * 128 * 32);
    int outwidth = 64;//128;
    int outheight = 64;//32;
    int i, j;
    uint32_t* out32 = (uint32_t*)out;
    int fracstep = (inwidth << 16) / outwidth;
    int outwidth32 = outwidth >> 1; // two pixels per 32-bit write

    for (i = 0; i < outheight; i++, out32 += outwidth32) {
        const uint16_t* inrow = in + inwidth * (i * inheight /outheight);
        int frac = fracstep >> 1;

        for (j = 0; j < outwidth32; j++) {
//            if (((j << 1) & 7) == 0)
//                __builtin_prefetch(inrow + 16);

            uint16_t p1 = rgb565_to_argb1555(inrow[frac >> 16]);
//if (p1 > 1)
  //          printf("p1 %04x\n", p1);
            frac += fracstep;
            uint16_t p2 = rgb565_to_argb1555(inrow[frac >> 16]);
            frac += fracstep;

            out32[j] = ((uint32_t)p2 << 16) | p1;
        }
    }
#else
    uint16_t *scaled216 = (uint16_t*)scaled2;
    for (int y=0;y<448;y+=7) {
        for (int x=0;x<640;x+=5) {
            scaled216[(int)(((y/7)*64) + (x/5))] =  rgb565_to_rgba5551(vram_s[(y*640) + x]);
        }

    }
#endif
}
extern int do_the_blur;
static void gfx_opengl_upload_texture(const uint8_t* rgba32_buf, int width, int height, unsigned int type) {
    GLint intFormat;

#if LET_GLDC_TWIDDLE
    if (do_the_blur && type == GL_UNSIGNED_SHORT_1_5_5_5_REV) {
        intFormat = GL_ARGB1555_KOS;
    }
    else if (type == GL_UNSIGNED_SHORT_1_5_5_5_REV) {
        intFormat = GL_ARGB1555_TWID_KOS;
    } else {
        intFormat = GL_ARGB4444_TWID_KOS;
    }
#else
    if (type == GL_UNSIGNED_SHORT_1_5_5_5_REV) {
        intFormat = GL_ARGB1555_KOS;
    } else {
        intFormat = GL_ARGB4444_KOS;
    }
#endif

    // we don't support non power of two textures, scale to next power of two if necessary
    if ((!is_pot(width) || !is_pot(height)) || (width < 8) || (height < 8)) {
        uint32_t final_w = width;
        uint32_t final_h = height;

        if (final_w < 8)
            final_w = 8;
        if (final_h < 8)
            final_h = 8;
        if (!is_pot(final_w)) {
            uint32_t prev_w = prev_pot(final_w);
            uint32_t next_w = next_pot(final_w);
            uint32_t avg = (next_w + prev_w) >> 1;
            if (final_w < avg) {
                final_w = prev_w;
            } else {
                final_w = next_w;
            }
        }

        if (!is_pot(final_h)) {
            uint32_t prev_h = prev_pot(final_h);
            uint32_t next_h = next_pot(final_h);
            uint32_t avg = (next_h + prev_h) >> 1;
            if (final_h < avg) {
                final_h = prev_h;
            } else {
                final_h = next_h;
            }
        }

        resample_32bit((const uint16_t*) rgba32_buf, width, height, (uint16_t*) scaled, final_w, final_h);

        rgba32_buf = (uint8_t*) scaled;

        width = final_w;
        height = final_h;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, intFormat, width, height, 0, GL_BGRA, type, rgba32_buf);
}

static inline GLenum gfx_cm_to_opengl(uint32_t val) {
    if (val & G_TX_CLAMP)
        return GL_CLAMP;

    return (val & G_TX_MIRROR) ? GL_MIRRORED_REPEAT : GL_REPEAT;
}


static inline void gfx_opengl_apply_tmu_state(const int tile) {
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, tmu_state[tile].min_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, tmu_state[tile].mag_filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, tmu_state[tile].wrap_s);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, tmu_state[tile].wrap_t);
}


static void gfx_opengl_set_sampler_parameters(int tile, uint8_t linear_filter, uint32_t cms, uint32_t cmt) {
    const GLenum filter = linear_filter ? GL_LINEAR : GL_NEAREST;
//	void * aqwater = segmented_to_virtual(aAqWaterTex);
  //  if (tmu_state[tile].srcaddr == aqwater) {
 /*    if (gCurrentLevel == LEVEL_AQUAS) {
  cms = 0;
  cmt = 0;
    } */

    const GLenum wrap_s = gfx_cm_to_opengl(cms);
    const GLenum wrap_t = gfx_cm_to_opengl(cmt);

    tmu_state[tile].min_filter = filter;
    tmu_state[tile].mag_filter = filter;
    tmu_state[tile].wrap_s = wrap_s;
    tmu_state[tile].wrap_t = wrap_t;

    // set state for the first texture right away
    //if (!tile)
        gfx_opengl_apply_tmu_state(tile);
}


static void gfx_opengl_set_depth_test(uint8_t depth_test) {
    if (depth_test != 0)
        glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
}


static void gfx_opengl_set_depth_mask(uint8_t z_upd) {
    gl_depth = z_upd;
    glDepthMask(z_upd ? GL_TRUE : GL_FALSE);
}


static uint8_t is_zmode_decal = false;
// Polyoffset currently doesn't work so gotta workaround it.
static void gfx_opengl_set_zmode_decal(uint8_t zmode_decal) {
    is_zmode_decal = zmode_decal;
    if (zmode_decal) {
        glDepthFunc(GL_LEQUAL);
    } else {
        glDepthFunc(GL_LESS);
    }
}


static void gfx_opengl_set_viewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}


static void gfx_opengl_set_scissor(int x, int y, int width, int height) {
    glScissor(x, y, width, height);
}


static void gfx_opengl_set_use_alpha(uint8_t use_alpha) {
    gl_blend = use_alpha;
    if (use_alpha)
        glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
}


// prim color
extern uint8_t pa;

// save original vertex colors for multi-pass tricks
// this is more than enough for the circumstances where that code runs
uint32_t backups[64];


// 0x01200200
static void skybox_setup_pre(void) {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_BLEND);
    glDisable(GL_FOG);
//    glPushMatrix();
}


// 0x01200200
static void skybox_setup_post(void) {
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glEnable(GL_BLEND);
    //glEnable(GL_FOG);
//    glPopMatrix();
}

extern int do_reticle;

// 0x01a00200
static void over_skybox_setup_pre(void) {
//    if(do_reticle) {
        glEnable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE); // don’t write depth (so it won’t block later geometry)
/*     } else {
        glDisable(GL_BLEND);            // pure cutout; no soft edges
        glEnable(GL_ALPHA_TEST);        // "punch-through"
        glAlphaFunc(GL_GREATER, 0.5f);  // adjust threshold as needed

        glDisable(GL_DEPTH_TEST);       // draw regardless of background depth
        glDepthMask(GL_FALSE);          // but DO NOT write depth
        glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    } */
}


static void over_skybox_setup_post(void) {
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glEnable(GL_BLEND);
    //glEnable(GL_FOG);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}


static void zmode_decal_setup_pre(void) {
    // Adjust depth values slightly for zmode_decal objects
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);

    // Push the geometry slightly towards the camera
//    glPushMatrix();
//    glTranslatef(0.0f, 0.01f, 0.01f);
}


static void zmode_decal_setup_post(void) {
    glPopMatrix();
    glDepthFunc(GL_LESS);
}


static void particle_blend_setup_pre() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE);
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
}

static void particle_blend_setup_post(void) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

extern int do_rectdepfix;

#if 0
static void twinkling_star_setup_pre(void) {
    // like the clouds over skybox
    glEnable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glPushMatrix();
    glTranslatef(0.0f, 0.0f, -3500.0f);
}

static void twinkling_star_setup_post(void) {
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glPopMatrix();
}

static void one_inv_setup_pre(void) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
}

static void one_inv_setup_post(void) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}
#endif


static void add_a_color_pre(void* vbo, size_t num_tris, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    // when there is any fixed color to additively blend... it is time for:
    // * advanced multi-pass sorcery *
    dc_fast_t* tris = (dc_fast_t*) vbo;
    // modulate texture
    glTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
     
    glEnable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glBlendFunc(GL_ONE, GL_ONE);
    // render opaque black untextured polys first to reset bg alpha
    
    // store copy of original vertex colors
    // AND
    // set them to solid black with 255 alpha
    // the actual color doesn't matter but the alpha does
    for (size_t i = 0; i < 3 * num_tris; i++) {
        backups[i] = tris[i].color.packed;
        tris[i].color.array.r = 0;
        tris[i].color.array.g = 0;
        tris[i].color.array.b = 0;
        tris[i].color.array.a = 255;
    }
    glDrawArrays(GL_TRIANGLES, 0, 3 * num_tris);
    
    glEnable(GL_TEXTURE_2D);
    // generate a solid "inverse cutout" texture
    // all opaque pixels become one solid color
    // transparent stay transparent
    glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDrawArrays(GL_TRIANGLES, 0, 3 * num_tris);
    
    // now that we've drawn the cutout version 
    // set the vert colors to PRIM color  
    for (size_t i = 0; i < 3 * num_tris; i++) {
        tris[i].color.array.r = r;
        tris[i].color.array.g = g;
        tris[i].color.array.b = b;
        tris[i].color.array.a = 255; // I'm not sure this is valid
    }
    // ensure depth test is on no matter what the RDP state said
    glEnable(GL_DEPTH_TEST);
    // and write to anything with equal depth
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_EQUAL);
    // TURN OFF TEXTURING, THIS IS KEY TO SECOND PASS
    glDisable(GL_TEXTURE_2D);
    // blend solid PRIM color triangles with the cutout-textured triangles
    glBlendFunc(GL_ONE_MINUS_DST_ALPHA, GL_DST_ALPHA);
    // with blending, we now have a cutout-textured object colored with PRIM color
    glDrawArrays(GL_TRIANGLES, 0, 3 * num_tris);

    // restore the original vertex colors for final pass
    for (size_t i = 0; i < 3 * num_tris; i++) {
        tris[i].color.packed = backups[i];
    }
    // turn texture back on
    glEnable(GL_TEXTURE_2D);
    // ONE+ONE blend of colored cutout and original texture
    glBlendFunc(GL_ONE, GL_ONE);
    // upon exit, draws the original texture
}


static void add_a_color_post(void) {
    // restore default blend and depth funcs
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthFunc(GL_LESS);
}


static float depbump = 0.0f;

extern uint8_t add_r,add_g,add_b,add_a;
extern int need_to_add;
extern int do_fillrect_blend;
extern int gGameState;
extern volatile int do_zfight;
extern volatile int do_zflip;
extern int do_menucard;
static void gfx_opengl_draw_triangles(float buf_vbo[], UNUSED size_t buf_vbo_len, size_t buf_vbo_num_tris) {
    cur_buf = (void*) buf_vbo;

    gfx_opengl_apply_shader(cur_shader);
    glEnable(GL_BLEND);

    if (cur_shader->texture_used[0] || cur_shader->texture_used[1])
        glBindTexture(GL_TEXTURE_2D, tmu_state[cur_shader->texture_ord[0]].tex);
    else
        glDisable(GL_TEXTURE_2D);

    // if(do_menucard)
    //   printf("menucard shader 0x%08x\n", cur_shader->shader_id);

    //    if(!do_rectdepfix && !do_menucard && cur_shader->shader_id == 0x00000a00 && gLevelType == LEVELTYPE_PLANET) {
    //        skybox_setup_pre();
    //    }

    if (do_rectdepfix) {
        skybox_setup_pre();
    }

    if (do_menucard) {
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE); // don’t write depth (so it won’t block later geometry)
    }

    if (/* do_menucard || */ (cur_shader->shader_id == 0x01a00a00 && gLevelType == LEVELTYPE_SPACE))
        over_skybox_setup_pre();

    if (is_zmode_decal)
        zmode_decal_setup_pre();

    if (cur_shader->shader_id == 0x01045551) {
        particle_blend_setup_pre();
        dc_fast_t* tris = (dc_fast_t*) buf_vbo;
        for (size_t i = 0; i < 3 * buf_vbo_num_tris; i++) {
            tris[i].color.array.a = pa;
        }
    }

    if (do_fillrect_blend) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE); ///* GL_ONE_MINUS_ */GL_SRC_ALPHA);
    }

    if (do_zfight) {
        dc_fast_t* tris = (dc_fast_t*) buf_vbo;
        for (size_t i = 0; i < 3 * buf_vbo_num_tris; i++) {
            if (do_zflip == 2)
                tris[i].vert.z -= 0.03f;
            else if (do_zflip == 1)
                tris[i].vert.z -= 0.03f;
            else
                tris[i].vert.z += 0.03f;
        }
    }

    glDrawArrays(GL_TRIANGLES, 0, 3 * buf_vbo_num_tris);

    if (do_zfight) {

        glDepthFunc(GL_LESS);
    }
    if (do_fillrect_blend) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    if (cur_shader->shader_id == 0x01045551)
        particle_blend_setup_post();
    if (do_rectdepfix)
        skybox_setup_post();

    if (is_zmode_decal)
        zmode_decal_setup_post();

    if (do_menucard || (cur_shader->shader_id == 0x01a00a00 && gLevelType == LEVELTYPE_SPACE))
        over_skybox_setup_post();

    //    if(cur_shader->shader_id == 0x00000a00 && gLevelType == LEVELTYPE_PLANET) {
    //        skybox_setup_post();
    //    }
}

extern void  __attribute__((noinline)) gfx_opengl_2d_projection(void);

extern void  __attribute__((noinline)) gfx_opengl_reset_projection(void);
extern int do_starfield;
extern int do_the_blur;
void gfx_opengl_draw_triangles_2d(void* buf_vbo, size_t buf_vbo_len, size_t buf_vbo_num_tris) {
    dc_fast_t* tris = buf_vbo;

    gfx_opengl_apply_shader(cur_shader);
    gfx_opengl_2d_projection();
    glDisable(GL_FOG);
    glEnable(GL_BLEND);

    glVertexPointer(3, GL_FLOAT, sizeof(dc_fast_t), &tris[0].vert);
    glTexCoordPointer(2, GL_FLOAT, sizeof(dc_fast_t), &tris[0].texture);
    glColorPointer(GL_BGRA, GL_UNSIGNED_BYTE, sizeof(dc_fast_t), &tris[0].color);

    if (buf_vbo_num_tris) {
        glEnable(GL_TEXTURE_2D);
        if (cur_shader->texture_used[0] || cur_shader->texture_used[1])
            glBindTexture(GL_TEXTURE_2D, tmu_state[cur_shader->texture_ord[0]].tex);
    } else {
        glDisable(GL_TEXTURE_2D);
    }

if (do_rectdepfix) {
            skybox_setup_pre();
        glDrawArrays(GL_QUADS, 0, 4);
skybox_setup_post();
}
else {
    if (do_starfield) {
        if (cur_shader->shader_id == 0x01200200)
            skybox_setup_pre();
        if (cur_shader->shader_id == 0x01a00a00)
            over_skybox_setup_pre();
//        glDisable(GL_BLEND);
/*         for (size_t i = 0; i < 4; i++) {
            tris[i].color.array.r = (tris[i].color.array.r + 64) > 255 ? 255 : (tris[i].color.array.r + 64);
            tris[i].color.array.g = (tris[i].color.array.g + 64) > 255 ? 255 : (tris[i].color.array.g + 64);
            tris[i].color.array.b = (tris[i].color.array.b + 64) > 255 ? 255 : (tris[i].color.array.b + 64);
            tris[i].color.array.a = pa;
        } */
        glDrawArrays(GL_QUADS, 0, 4);
  //      glEnable(GL_BLEND);
        if (cur_shader->shader_id == 0x01a00a00)
            over_skybox_setup_post();
        if (cur_shader->shader_id == 0x01200200)
            skybox_setup_post();
    }
    else {
        

        if (cur_shader->shader_id == 0x01200200)
            skybox_setup_pre();
        if(cur_shader->shader_id == 0x01045045)
            glEnable(GL_BLEND);
        if (cur_shader->shader_id == 0x01a00a00)
            over_skybox_setup_pre();
        if (do_fillrect_blend) {
            glEnable(GL_BLEND);
//            printf("do the glare thing\n");
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);///* GL_ONE_MINUS_ */GL_SRC_ALPHA);
        }
        if (do_the_blur) {
            glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
    /*     if (do_xt_fill) {
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glDepthFunc(GL_LEQUAL);
            glDisable(GL_BLEND);
            glDisable(GL_FOG);
        } */
        if (gGameState == 8) {  // ending
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glDepthFunc(GL_ALWAYS);
            glDisable(GL_BLEND);
        }
        glDrawArrays(GL_QUADS, 0, 4);

        if (cur_shader->shader_id == 0x01200200)
            skybox_setup_post();
        if (cur_shader->shader_id == 0x01a00a00)
            over_skybox_setup_post();
        if (do_fillrect_blend || do_the_blur)
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    /*     if (do_xt_fill) {
            glDepthMask(GL_TRUE);
            glDepthFunc(GL_LESS);
            glEnable(GL_BLEND);
            glEnable(GL_FOG);
        } */
    }
}
    gfx_opengl_reset_projection();
}


static inline uint8_t gl_check_ext(const char* name) {
    static const char* extstr = NULL;

    if (extstr == NULL)
        extstr = (const char*) glGetString(GL_EXTENSIONS);

    if (!strstr(extstr, name))
        return 0;

    return 1;
}


static inline uint8_t gl_get_version(int* major, int* minor, uint8_t* is_es) {
    const char* vstr = (const char*) glGetString(GL_VERSION);
    if (!vstr || !vstr[0])
        return 0;

    if (!strncmp(vstr, "OpenGL ES ", 10)) {
        vstr += 10;
        *is_es = 1;
    } else if (!strncmp(vstr, "OpenGL ES-CM ", 13)) {
        vstr += 13;
        *is_es = 1;
    }

    return (sscanf(vstr, "%d.%d", major, minor) == 2);
}

#define sys_fatal printf

extern void getRamStatus(void);
static void gfx_opengl_init(void) {
#if FOR_WINDOWS || defined(OSX_BUILD)
    GLenum err;
    if ((err = glewInit()) != GLEW_OK)
        sys_fatal("could not init GLEW:\n%s", glewGetErrorString(err));
#endif
    newest_texture = 0;
#if 0
    shaderidx = 0;
    memset(shaderlist, 0, sizeof(shaderlist));
#endif
    GLdcConfig config;
    glKosInitConfig(&config);
    config.autosort_enabled = GL_TRUE;
    config.fsaa_enabled = GL_FALSE;
    /*@Note: These should be adjusted at some point */
    config.initial_op_capacity = 128;
    config.initial_pt_capacity = 32;
    config.initial_tr_capacity = 256;
    config.initial_immediate_capacity = 0;
    glKosInitEx(&config);
    // glKosInit();
#ifdef __DREAMCAST__
    if (vid_check_cable() != CT_VGA)
    {
        vid_set_mode(DM_640x480_NTSC_IL, PM_RGB565);
    }
    else
    {
        vid_set_mode(DM_640x480_VGA, PM_RGB565);
    }
#endif
    //    getRamStatus();
    fflush(stdout);

    // check GL version
    int vmajor, vminor;
    uint8_t is_es = 0;
    gl_get_version(&vmajor, &vminor, &is_es);
    if ((vmajor < 2 && vminor < 1) || is_es)
        sys_fatal("OpenGL 1.1+ is required.\nReported version: %s%d.%d\n", is_es ? "ES" : "", vmajor, vminor);

    printf("GL_VERSION = %s\n", glGetString(GL_VERSION));
    printf("GL_EXTENSIONS =\n%s\n", glGetString(GL_EXTENSIONS));

    glDisable(GL_LIGHTING);
    glDisable(GL_CULL_FACE);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0f);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glShadeModel(GL_SMOOTH);

    glEnableClientState(GL_VERTEX_ARRAY);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    glEnableClientState(GL_COLOR_ARRAY);


    
    glViewport(0, 0, 640, 480);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    static float fog[4] = { 1.f, 1.f, 1.f, 0.5f };

    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 0.f);
    glFogf(GL_FOG_END, 256.f);
    glFogfv(GL_FOG_COLOR, fog);
}

static void gfx_opengl_on_resize(void) {

}

extern void reset_texcache(void);
extern float screen_2d_z;

void nuke_everything(void) {
//    printf("NUKE IT ALL\n");
    gfx_clear_all_textures();
    reset_texcache();
}

extern s32 D_800DC5D0;
extern s32 D_800DC5D4;
extern s32 D_800DC5D8;
void make_a_far_quad(void);

extern void  __attribute__((noinline)) ext_gfx_dp_fill_rectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry);

static void gfx_opengl_start_frame(void) {
    screen_2d_z = -1.0f;
    depbump = 0.0f;
    do_fillrect_blend = 0;
    do_starfield = 0;
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_TRUE); // Must be set to clear Z-buffer
#if 0
    memset(shaderlist,0,sizeof(shaderlist));
shaderidx = 0;
#endif
//    glClearColor((float)D_800DC5D0/255.0f, (float)D_800DC5D4/255.0f,
  //  (float)D_800DC5D8/255.0f,1.0f);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
#if 0
	glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
	glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
	glLoadIdentity();
    glDepthMask(GL_FALSE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    make_a_far_quad();
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glPopMatrix();
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
#endif
//ext_gfx_dp_fill_rectangle(0, 0, 320<<2,240<<2);
//    newest_texture = 0;
}

static void gfx_opengl_end_frame(void) {
#if 0
    if(shader_debug_toggle) {
    printf("shaders in frame:\n");
    for(int i=0;i<shaderidx;i++)
        printf("\t 0x%08x\n", shaderlist[i]);
}
        #endif
    return;
}

static void gfx_opengl_finish_render(void) {
    return;
}

struct GfxRenderingAPI gfx_opengl_api = { gfx_opengl_z_is_from_0_to_1, gfx_opengl_unload_shader,
                                          gfx_opengl_load_shader,      gfx_opengl_create_and_load_new_shader,
                                          gfx_opengl_lookup_shader,    gfx_opengl_shader_get_info,
                                          gfx_opengl_new_texture,      gfx_opengl_select_texture,
                                          gfx_opengl_upload_texture,   gfx_opengl_set_sampler_parameters,
                                          gfx_opengl_set_depth_test,   gfx_opengl_set_depth_mask,
                                          gfx_opengl_set_zmode_decal,  gfx_opengl_set_viewport,
                                          gfx_opengl_set_scissor,      gfx_opengl_set_use_alpha,
                                          gfx_opengl_draw_triangles,   gfx_opengl_init,
                                          gfx_opengl_on_resize,        gfx_opengl_start_frame,
                                          gfx_opengl_end_frame,        gfx_opengl_finish_render };