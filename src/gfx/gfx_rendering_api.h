#ifndef GFX_RENDERING_API_H
#define GFX_RENDERING_API_H

#include <stddef.h>
#include <stdint.h>
//#include <stduint8_t.h>

// Abstract blend factors: the front-end derives these from the N64 blender (other_mode_l) and the
// raw-PVR backend maps them onto PVR blend modes. GLdc ignores them. (Used by the PVR backend.)
enum gfx_blend_factor {
    GFX_BLENDF_ZERO = 0,
    GFX_BLENDF_ONE,
    GFX_BLENDF_SRCALPHA,
    GFX_BLENDF_INVSRCALPHA,
    GFX_BLENDF_DSTALPHA,
    GFX_BLENDF_INVDSTALPHA,
};

// Abstract texture-environment mode, derived in the front-end from the N64 color combiner. Values
// match the PVR pvr_txr_shading_mode order so the raw-PVR backend maps 1:1. GLdc no-ops set_tex_env
// (it keys texenv off shader ids itself).
enum gfx_tex_env {
    GFX_TEXENV_REPLACE = 0,        // px = tex
    GFX_TEXENV_MODULATE,           // rgb = col*tex, a = tex.a
    GFX_TEXENV_DECAL,              // rgb = lerp(col,tex,tex.a), a = col.a
    GFX_TEXENV_MODULATEALPHA,      // rgb = col*tex, a = col.a*tex.a
};

struct ShaderProgram;

struct GfxRenderingAPI {
    uint8_t (*z_is_from_0_to_1)(void);
    void (*unload_shader)(struct ShaderProgram *old_prg);
    void (*load_shader)(struct ShaderProgram *new_prg);
    struct ShaderProgram *(*create_and_load_new_shader)(uint32_t shader_id);
    struct ShaderProgram *(*lookup_shader)(uint32_t shader_id);
    // Single-tile: returns whether the combiner uses a texture (no used_textures[2] out-param).
    uint8_t (*shader_get_info)(struct ShaderProgram *prg, uint8_t *num_inputs);
    uint32_t (*new_texture)(void);
    void (*select_texture)(uint32_t texture_id);
    void (*upload_texture)(const uint16_t *rgba16_buf, int width, int height, unsigned int type);
    void (*set_sampler_parameters)(uint8_t linear_filter, uint32_t cms, uint32_t cmt);
    void (*set_depth_test)(uint8_t depth_test);
    void (*set_depth_mask)(uint8_t z_upd);
    void (*set_zmode_decal)(uint8_t zmode_decal);
    // Texel<->vertex-color combine (enum gfx_tex_env), derived from the N64 combiner. Raw-PVR folds
    // it into the poly header; GLdc no-ops (it keys texenv off shader ids itself).
    void (*set_tex_env)(uint32_t mode);
    void (*set_viewport)(int x, int y, int width, int height);
    void (*set_scissor)(int x, int y, int width, int height);
    void (*set_use_alpha)(uint8_t use_alpha);
    void (*draw_triangles)(float buf_vbo[], size_t buf_vbo_len, size_t buf_vbo_num_tris);
    // 2D screen-space quad (4 verts). GLdc draws via its existing path; the raw-PVR backend submits a
    // native 4-vertex strip. Dormant until the front-end's 2D path is routed here (later stage).
    void (*draw_triangles_2d)(void *buf_vbo, size_t buf_vbo_len, size_t buf_vbo_num_tris);
    void (*init)(void);
    void (*on_resize)(void);
    void (*start_frame)(void);
    void (*end_frame)(void);
    void (*finish_render)(void);
};

#endif
