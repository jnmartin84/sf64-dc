#ifndef _LANGUAGE_C
#define _LANGUAGE_C
#endif

#define G_TX_LOADTILE 7
#define G_TX_RENDERTILE 0

#define G_TX_NOMIRROR 0
#define G_TX_WRAP 0
#define G_TX_MIRROR 0x1
#define G_TX_CLAMP 0x2
#define G_TX_NOMASK 0
#define G_TX_NOLOD 0

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

#define s32 long int
#define u16 unsigned short

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

extern LevelId gCurrentLevel;

extern uint8_t gLevelType;

extern uint8_t gorgon_alpha;

extern int use_gorgon_alpha;
extern int do_the_blur;
extern int do_reticle;
extern int do_rectdepfix;
extern int need_to_add;
extern int do_fillrect_blend;
extern int do_starfield;
extern int do_the_blur;
extern volatile int do_zfight;
extern volatile int do_zflip;
extern int do_menucard;
extern int do_andross;
extern int do_radar_mark;
extern int do_space_bg;

/* Used for rescaling textures into pow2 dims */
extern uint16_t __attribute__((aligned(32))) scaled[64 * 64];
extern uint16_t __attribute__((aligned(32))) scaled2[256 * 128];

// prim color
extern uint8_t pa;

extern void reset_texcache(void);
extern float screen_2d_z;
extern s32 D_800DC5D0;
extern s32 D_800DC5D4;
extern s32 D_800DC5D8;
void make_a_far_quad(void);

extern void ext_gfx_dp_fill_rectangle(int32_t ulx, int32_t uly, int32_t lrx, int32_t lry);

int ending_great_fox = 0;
extern void gfx_opengl_2d_projection(void);
extern void gfx_opengl_reset_projection(void);
int do_ending_bg = 0;

extern int path_priority_draw;

extern uint8_t add_r, add_g, add_b, add_a;
extern int gGameState;

extern u16 aAqWaterTex[];

static struct ShaderProgram shader_program_pool[64];
static uint8_t shader_program_pool_size;
static struct ShaderProgram* cur_shader = NULL;

static struct SamplerState tmu_state[2];

static const dc_fast_t* cur_buf = NULL;
static uint8_t gl_blend = 0;
static uint8_t gl_depth = 0;
extern int16_t fog_mul;
extern int16_t fog_ofs;
extern float gProjectNear;
extern float gProjectFar;
int fog_dirty = 0;

#include <kos.h>

void n64_memcpy(void* dst, const void* src, size_t size);

static void resample_tex(const uint16_t* in, int inwidth, int inheight, uint16_t* out, int outwidth, int outheight) {
    for (int y=0; y < inheight;y++) {
        n64_memcpy(out + (y*outwidth), in + (y*inwidth), inwidth * 2);
        uint16_t *ptr = out + (y*outwidth) + inwidth;
        ptr[0] = 0;
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
    if (n <= 16)
        return 8;
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
    return 0;
}

static inline GLenum texenv_set_color(void) {
    return GL_MODULATE;
}

static inline GLenum texenv_set_texture_color(struct ShaderProgram* prg) {
    GLenum mode = GL_MODULATE;

    // don't touch these
    switch (prg->shader_id) {
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

float gl_fog_start;
float gl_fog_end;

void gfx_opengl_change_fog(void) {
    float fog_scale = 0.5f;
    if ((gCurrentLevel == LEVEL_ZONESS) || (gCurrentLevel == LEVEL_TITANIA))
        fog_scale = 0.3f;
    else if ((gCurrentLevel == LEVEL_SOLAR) || (gCurrentLevel == LEVEL_MACBETH))
        fog_scale = 0.4f;
    else if ((gCurrentLevel == LEVEL_SECTOR_Y))
        fog_scale = 0.9f;

    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, (GLfloat)gl_fog_start * fog_scale * 0.9f);
    glFogf(GL_FOG_END, (GLfloat)gl_fog_end * fog_scale);
}

static void gfx_opengl_apply_shader(struct ShaderProgram* prg) {
    // vertices are always there
    glVertexPointer(3, GL_FLOAT, sizeof(dc_fast_t), &cur_buf[0].vert);
    glTexCoordPointer(2, GL_FLOAT, sizeof(dc_fast_t), &cur_buf[0].texture);
    glColorPointer(GL_BGRA, GL_UNSIGNED_BYTE, sizeof(dc_fast_t), &cur_buf[0].color);

    // have texture(s), specify same texcoords for every active texture
    if (prg->texture_used[0] || prg->texture_used[1]) {
        glEnable(GL_TEXTURE_2D);
    } else {
        glDisable(GL_TEXTURE_2D);
    }

    if (prg->shader_id & SHADER_OPT_FOG) {
        glEnable(GL_FOG);
        gfx_opengl_change_fog();
    } else {
        glDisable(GL_FOG);
    }

    if (prg->shader_id & SHADER_OPT_TEXTURE_EDGE) {
        glEnable(GL_ALPHA_TEST);
        glAlphaFunc(GL_GREATER, 0.125);
        glDisable(GL_BLEND);
    } else {
        glDisable(GL_ALPHA_TEST);
    }

    // configure texenv
    GLenum mode = texenv_set_color();
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
    return (uint32_t) ret;
}

static void gfx_opengl_select_texture(int tile, uint32_t texture_id) {
    tmu_state[tile].tex = texture_id;
    glBindTexture(GL_TEXTURE_2D, texture_id);
}

void capture_framebuffer(void) {
#if LOWRES
    for (int y=0;y<240;y+=2) {
        for (int x=0;x<320;x+=2) {
            scaled2[(y<<7) + (x>>1)] = vram_s[((y<<8) + (y << 6)) + x];
        }
    }
#else
    for (int y=0;y<480;y+=4) {
        for (int x=0;x<640;x+=4) {
            scaled2[(y<<6) + (x>>2)] = vram_s[((y<<9) + (y<<7)) + x];
        }
    }
#endif
}

static struct __attribute__((aligned(32))) {
    float u_scale[1024];
    float v_scale[1024];
} tex_scaler;

#define LET_GLDC_TWIDDLE 1

static void gfx_opengl_upload_texture(const uint16_t* rgba16_buf, int width, int height, unsigned int type) {
    GLint intFormat;

#if LET_GLDC_TWIDDLE
    if (do_the_blur && type == GL_UNSIGNED_SHORT_1_5_5_5_REV) {
        type = GL_UNSIGNED_SHORT_5_6_5;
        intFormat = GL_RGB565_KOS;
    } else if (type == GL_UNSIGNED_SHORT_1_5_5_5_REV) {
        intFormat = GL_ARGB1555_TWID_KOS;
    } else {
        intFormat = GL_ARGB4444_TWID_KOS;
    }

    if (do_the_blur) {
        glTexImage2D(GL_TEXTURE_2D, 0, intFormat, /* 128 */256, /* 64 */128, 0, GL_RGB, type, scaled2);
        return;
    }

#else
    if (type == GL_UNSIGNED_SHORT_1_5_5_5_REV) {
        intFormat = GL_ARGB1555_KOS;
    } else {
        intFormat = GL_ARGB4444_KOS;
    }
#endif

    int is_wp2 = is_pot(width);
    int is_hp2 = is_pot(height);
    int is_wl8 = width < 8;
    int is_hl8 = height < 8;

    // we don't support non power of two textures, *PAD* to next power of two if necessary
    if ((!is_wp2 || !is_hp2) || is_wl8 || is_hl8) {
        uint32_t final_w = width;
        uint32_t final_h = height;

        if (is_wl8)
            final_w = 8;
        else if (!is_wp2) {
            final_w = next_pot(final_w);
        }

        if (is_hl8)
            final_h = 8;
        else if (!is_hp2) {
            final_h = next_pot(final_h);
        }

        int scale_index = tmu_state[0].tex;
        tex_scaler.u_scale[scale_index] = (float)width / (float)final_w;
        tex_scaler.v_scale[scale_index] = (float)height / (float)final_h;

        if (!(is_wp2 && (!is_wl8) && ((!is_hp2) || is_hl8))) {
            // when the texture isnt pow2 or >= 8 pixels in either dimension
            // it has to be padded in width and height
            resample_tex((const uint16_t*) rgba16_buf, width, height, scaled, final_w, final_h);
            rgba16_buf = (uint8_t*) scaled;
        } else {
            // but when the texture is pow2 in width
            // and > 8 pixels tall, it only needs to be padded in height
            // because of the way texture filtering works
            // we need to clear a row at the bottom of the buffer
            // but only a single row
            memset(rgba16_buf + (height*width), 0, width*2);
        }   

        width = final_w;
        height = final_h;
    } else {
        int scale_index = tmu_state[0].tex;
        tex_scaler.u_scale[scale_index] = 1.0f;
        tex_scaler.v_scale[scale_index] = 1.0f;
    }

    glTexImage2D(GL_TEXTURE_2D, 0, intFormat, width, height, 0, GL_BGRA, type, rgba16_buf);
}

float get_current_u_scale(void) {
    int scale_index = tmu_state[0].tex;
    return tex_scaler.u_scale[scale_index];
}

float get_current_v_scale(void) {
    int scale_index = tmu_state[0].tex;
    return tex_scaler.v_scale[scale_index];
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
    const GLenum wrap_s = gfx_cm_to_opengl(cms);
    const GLenum wrap_t = gfx_cm_to_opengl(cmt);

    tmu_state[tile].min_filter = filter;
    tmu_state[tile].mag_filter = filter;
    tmu_state[tile].wrap_s = wrap_s;
    tmu_state[tile].wrap_t = wrap_t;

    // set state for the first texture right away
    if (!tile)
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

static uint8_t is_zmode_decal = 0;
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

// 0x01200200
static void skybox_setup_pre(void) {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_BLEND);
    glDisable(GL_FOG);
}

// 0x01200200
static void skybox_setup_post(void) {
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glEnable(GL_BLEND);
    glEnable(GL_FOG);
}

// 0x01a00200
static void over_skybox_setup_pre(void) {
    glEnable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE); // don’t write depth (so it won’t block later geometry)
}

static void over_skybox_setup_post(void) {
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

static void zmode_decal_setup_pre(void) {
    // Adjust depth values slightly for zmode_decal objects
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);

    // Push the geometry slightly towards the camera
    glPushMatrix();
    glTranslatef(0.0f, 0.01f, 0.01f);
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

void gfx_opengl_2d_projection(void);
void gfx_opengl_reset_projection(void);

static void gfx_opengl_draw_triangles(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris) {
    //buf_vbo_len = 3 * buf_vbo_num_tris;
    cur_buf = (void*) buf_vbo;

    gfx_opengl_apply_shader(cur_shader);
    glEnable(GL_BLEND);

    if (cur_shader->texture_used[0] || cur_shader->texture_used[1])
        glBindTexture(GL_TEXTURE_2D, tmu_state[cur_shader->texture_ord[0]].tex);
    else
        glDisable(GL_TEXTURE_2D);

    if (!do_andross && do_rectdepfix) {
        skybox_setup_pre();
    }

    if (do_menucard) {
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE); // don’t write depth (so it won’t block later geometry)
    }

    if (cur_shader->shader_id == 0x01a00a00 && gLevelType == LEVELTYPE_SPACE)
        over_skybox_setup_pre();

    if (is_zmode_decal)
        zmode_decal_setup_pre();

    if (/* do_andross ||  */do_fillrect_blend) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE);
    }

    if (do_zfight) {
        dc_fast_t* tris = (dc_fast_t*) buf_vbo;
        for (size_t i = 0; i < 3 * buf_vbo_num_tris; i++) {
            if (do_zflip == 2)
                tris[i].vert.z -= 1.0f;
            else if (do_zflip == 1)
                tris[i].vert.z -= 1.0f;
            else
                tris[i].vert.z += 1.0f;
        }
    }

    if (use_gorgon_alpha) {
        dc_fast_t* tris = (dc_fast_t*) buf_vbo;
        for (size_t i = 0; i < 3 * buf_vbo_num_tris; i++) {
            tris[i].color.array.a = gorgon_alpha;
        }
    }

    if (do_space_bg) {
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE); // don’t write depth (so it won’t block later geometry)
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_BLEND);
    }

    if (cur_shader->shader_id == 0x01045551) {
        particle_blend_setup_pre();
        dc_fast_t* tris = (dc_fast_t*) buf_vbo;
        for (size_t i = 0; i < 3 * buf_vbo_num_tris; i++) {
            tris[i].color.array.a = pa;
        }
    }

    if(do_radar_mark) {
        gfx_opengl_2d_projection();
    }

    if (do_the_blur) {
        uint32_t olda;
        uint32_t oldc;
        dc_fast_t* tris = (dc_fast_t*) buf_vbo;
        // draw opaque "framebuffer"
        olda = tris[0].color.array.a;
/*         if (olda > 128) olda = 128;
        if (olda < 32) olda = 32;
 */        olda <<= 24; 
        for (size_t i = 0; i < 3 * buf_vbo_num_tris; i++) {
            tris[i].color.packed = 0xffffffff;
        }
        // no blend, force it to be OP for depth write reasons
        glDisable(GL_BLEND);
        glDrawArrays(GL_TRIANGLES, 0, 3 * buf_vbo_num_tris);
        // now put up an untextured black quad with blur alpha
        glDisable(GL_TEXTURE_2D);
        // re-enable blend so it is TR for overdraw purposes
        glEnable(GL_BLEND);
        for (size_t i = 0; i < 3 * buf_vbo_num_tris; i++) {
//            tris[i].vert.z -= 0.001f;
            tris[i].color.packed = olda;
        }
       // glBlendFunc(/* GL_SRC_ALPHA, GL_ONE);// */GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA); 
    }

    glDrawArrays(GL_TRIANGLES, 0, 3 * buf_vbo_num_tris);

    if (do_the_blur) {
        glEnable(GL_TEXTURE_2D);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }

    if(do_radar_mark) {
        gfx_opengl_reset_projection();
    }

    if (cur_shader->shader_id == 0x01045551)
        particle_blend_setup_post();

    if (do_space_bg) {
        glEnable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE); // don’t write depth (so it won’t block later geometry)
        glDepthFunc(GL_LESS);
        glEnable(GL_BLEND);
    }

    if (do_zfight) {
        glDepthFunc(GL_LESS);
    }

    if (do_andross || do_fillrect_blend) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    }
    
    if (!do_andross && do_rectdepfix)
        skybox_setup_post();

    if (is_zmode_decal)
        zmode_decal_setup_post();

    if (do_menucard || (cur_shader->shader_id == 0x01a00a00 && gLevelType == LEVELTYPE_SPACE))
        over_skybox_setup_post();
}

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
        goto end_tri2d;
    }

    else if (do_starfield) {
        over_skybox_setup_pre();
        glDisable(GL_BLEND);

        glDrawArrays(GL_QUADS, 0, 4);
        glEnable(GL_BLEND);

        over_skybox_setup_post();
        goto end_tri2d;
    }

    else if (gGameState == 8) { // ending
        if (do_ending_bg) {
            skybox_setup_pre();
        } else {
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glDepthFunc(GL_ALWAYS);
            glEnable(GL_BLEND);
        }
    } else {
        if (cur_shader->shader_id == 0x01200200)
            skybox_setup_pre();
        if (cur_shader->shader_id == 0x01045045)
            glEnable(GL_BLEND);
        if (cur_shader->shader_id == 0x01a00a00)
            over_skybox_setup_pre();
        if (do_fillrect_blend) {
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE);
        }
        if (do_the_blur) {
            glPushMatrix();
            glTranslatef(0.0f, 0.0f, 100000.0f);
            glEnable(GL_BLEND);
        }
        if (path_priority_draw)
            glDepthFunc(GL_ALWAYS);
    }

    glDrawArrays(GL_QUADS, 0, 4);

    if (gGameState == 8) { // ending
        if (do_ending_bg) {
            skybox_setup_post();
        } else {
            glEnable(GL_DEPTH_TEST);
            glDepthMask(GL_TRUE);
            glDepthFunc(GL_LESS);
        }
    } else {
        if (path_priority_draw)
            glDepthFunc(GL_LESS);
        if (cur_shader->shader_id == 0x01200200)
            skybox_setup_post();
        if (cur_shader->shader_id == 0x01a00a00)
            over_skybox_setup_post();
        if (do_the_blur) {
            glEnable(GL_TEXTURE_2D);
            glPopMatrix();
            glEnable(GL_BLEND);
            glDepthMask(GL_TRUE);
            glDepthFunc(GL_LEQUAL);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
        if (do_fillrect_blend) {
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        }
    }
end_tri2d:
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

static void gfx_opengl_init(void) {
    newest_texture = 0;

    GLdcConfig config;
    glKosInitConfig(&config);
    config.autosort_enabled = GL_TRUE;
    config.fsaa_enabled = GL_FALSE;
    /*@Note: These should be adjusted at some point */
    config.initial_op_capacity = 128;
    config.initial_pt_capacity = 32;
    config.initial_tr_capacity = 256;
    config.initial_immediate_capacity = 0;

#ifdef __DREAMCAST__
    if (vid_check_cable() != CT_VGA) {
#if LOWRES
        vid_set_mode(DM_320x240_NTSC, PM_RGB565);
#else
        vid_set_mode(DM_640x480_NTSC_IL, PM_RGB565);
#endif
    } else {
#if LOWRES
        vid_set_mode(DM_320x240_VGA, PM_RGB565);
#else
        vid_set_mode(DM_640x480_VGA, PM_RGB565);
#endif
    }
#endif

    glKosInitEx(&config);

#ifdef __DREAMCAST__
    if (vid_check_cable() != CT_VGA) {
#if LOWRES
        vid_set_mode(DM_320x240_NTSC, PM_RGB565);
#else
        vid_set_mode(DM_640x480_NTSC_IL, PM_RGB565);
#endif
    } else {
#if LOWRES
        vid_set_mode(DM_320x240_VGA, PM_RGB565);
#else
        vid_set_mode(DM_640x480_VGA, PM_RGB565);
#endif
    }
#endif

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

#if LOWRES
    glViewport(0, 0, 320, 240);
#else
    glViewport(0, 0, 640, 480);
#endif
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    static float fog[4] = { 0.f, 0.f, 0.f, 1.0f };

    glFogi(GL_FOG_MODE, GL_LINEAR);
    glFogf(GL_FOG_START, 0.f);
    glFogf(GL_FOG_END, 256.f);
    glFogfv(GL_FOG_COLOR, fog);
}

static void gfx_opengl_on_resize(void) {
}

void nuke_everything(void) {
    gfx_clear_all_textures();
    reset_texcache();
}

static void gfx_opengl_start_frame(void) {
    screen_2d_z = -1.0f;
    do_fillrect_blend = 0;
    do_starfield = 0;
    glDisable(GL_SCISSOR_TEST);
    glDepthMask(GL_TRUE); // Must be set to clear Z-buffer
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_SCISSOR_TEST);
}

static void gfx_opengl_end_frame(void) {
    ;
}

static void gfx_opengl_finish_render(void) {
    ;
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
