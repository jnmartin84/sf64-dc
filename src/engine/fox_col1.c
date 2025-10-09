#include "global.h"
#if 0
f32 __dx1;
f32 __dx2;
f32 __dy1;
f32 __dy2;
f32 __dz1;
f32 __dz2;
f32 __pos_x;
f32 __pos_z;
#endif

static struct __attribute__((aligned(32))) {
    f32 __dx1;
    f32 __dx2;
    f32 __dy1;
    f32 __dy2;
    f32 __dz1;
    f32 __dz2;
    f32 __pos_x;
    f32 __pos_z;
} xz_vars;


f32 __vtx0_x;
f32 __vtx0_y;
f32 __vtx0_z;
f32 __vtx1_x;
f32 __vtx1_y;
f32 __vtx1_z;
f32 __vtx2_x;
f32 __vtx2_y;
f32 __vtx2_z;
f32 __vtx3_x;
f32 __vtx3_y;
f32 __vtx3_z;

#define TRINORM_X(A, B, C) ((B##_y - A##_y) * (C##_z - B##_z) - (B##_z - A##_z) * (C##_y - B##_y))
#define TRINORM_Y(A, B, C) ((B##_z - A##_z) * (C##_x - B##_x) - (B##_x - A##_x) * (C##_z - B##_z))
#define TRINORM_Z(A, B, C) ((B##_x - A##_x) * (C##_y - B##_y) - (B##_y - A##_y) * (C##_x - B##_x))

 f32 SHZ_NO_INLINE __attribute__((noinline)) triple_product_mul(Vec3s a, Vec3s b, Vec3s c) {
    f32 dist = 
       0.0f - (f32)((a.x * b.y * c.z) - (b.x * c.y * a.z) - (c.x * a.y * b.z) + (a.x * c.y * b.z) + (b.x * a.y * c.z) + (c.x * b.y * a.z));
       return dist;
}
#include "sh4zam.h"
 f32 SHZ_NO_INLINE  __attribute__((noinline)) triple_product_fipr(Vec3s a, Vec3s b, Vec3s c) {
    f32 px = (f32)(a.x * c.y);
    f32 py = (f32)(a.y * b.x);
    f32 pz = (f32)(a.z * b.y);

    f32 rx = (f32)(a.x * b.y);
    f32 ry = (f32)(b.x * c.y);
    f32 rz = (f32)(c.x * a.y);

    f32 dotPQ = shz_dot8f(px,py,pz,0,(f32)b.z,(f32)c.z,(f32)c.x,0);
    f32 dotRS = shz_dot8f(rx,ry,rz,0,(f32)c.z,(f32)a.z,(f32)b.z,0);

    return (dotPQ - dotRS);

    //    f32 dist = 
  //     0.0f - (f32)((a.x * b.y * c.z) - (b.x * c.y * a.z) - (c.x * a.y * b.z) + (a.x * c.y * b.z) + (b.x * a.y * c.z) + (c.x * b.y * a.z));
    //   return dist;
}

// Calculate the directed plane that contains the ordered triangle tri, given as an array of Vec3s
void func_col1_80097380(PlaneF* plane, Vec3s** tri) {
    Vec3s a;
    Vec3s b;
    Vec3s c;
//    s32 new_var;
//    Vec3s* tri2 = *tri;

    a.x = (*tri)->x;
    a.y = (*tri)->y; //tri2->y; // fake
    a.z = (*tri)->z;
    tri++;
    b.x = (*tri)->x;
    b.y = (*tri)->y;
    b.z = (*tri)->z;
    tri++;
    xz_vars.__dx1 = (f32)(b.x - a.x);
    xz_vars.__dy1 = (f32)(b.y - a.y);
    xz_vars.__dz1 = (f32)(b.z - a.z);

    c.x = (*tri)->x;
    c.y = (*tri)->y;
    c.z = (*tri)->z;
    tri++;
//    do {
    xz_vars.__dy2 = (f32)(c.y - b.y);
    xz_vars.__dx2 = (f32)(c.x - b.x);
    xz_vars.__dz2 = (f32)(c.z - b.z);
//    } while (0); // wut

    plane->normal.x = (xz_vars.__dy1 * xz_vars.__dz2) - (xz_vars.__dz1 * xz_vars.__dy2);
    plane->normal.y = (xz_vars.__dz1 * xz_vars.__dx2) - (xz_vars.__dx1 * xz_vars.__dz2);
    plane->normal.z = (xz_vars.__dx1 * xz_vars.__dy2) - (xz_vars.__dy1 * xz_vars.__dx2);
    plane->dist = triple_product_mul(a,b,c);
      // 0.0f - (f32)((a.x * b.y * c.z) - (b.x * c.y * a.z) - (c.x * a.y * b.z) + (a.x * c.y * b.z) + (b.x * a.y * c.z) + (c.x * b.y * a.z));
}

// Calculate the directed plane that contains the ordered triangle tri, given as an array of Vec3s. Duplicate of
// previous
void func_col1_80097558(PlaneF* plane, Vec3s** tri) {
    Vec3s a;
    Vec3s b;
    Vec3s c;
//    s32 new_var;
//    Vec3s* tri2 = *tri;

    a.x = (*tri)->x;
    a.y = (*tri)->y;//tri2->y; // fake
    a.z = (*tri)->z;
    tri++;
    b.x = (*tri)->x;
    b.y = (*tri)->y;
    b.z = (*tri)->z;
    tri++;
    c.x = (*tri)->x;
    c.y = (*tri)->y;
    c.z = (*tri)->z;
    tri++;

    xz_vars.__dx1 = b.x - a.x;
    xz_vars.__dx2 = c.x - b.x;
    xz_vars.__dy1 = b.y - a.y;
    do {
        xz_vars.__dy2 = c.y - b.y;
        xz_vars.__dz1 = b.z - a.z;
        xz_vars.__dz2 = c.z - b.z;
    } while (0); // wut

    plane->normal.x = (xz_vars.__dy1 * xz_vars.__dz2) - (xz_vars.__dz1 * xz_vars.__dy2);
    plane->normal.y = (xz_vars.__dz1 * xz_vars.__dx2) - (xz_vars.__dx1 * xz_vars.__dz2);
    plane->normal.z = (xz_vars.__dx1 * xz_vars.__dy2) - (xz_vars.__dy1 * xz_vars.__dx2);
    plane->dist =triple_product_fipr(a,b,c);
        //-a.x * b.y * c.z - b.x * c.y * a.z - c.x * a.y * b.z + a.x * c.y * b.z + b.x * a.y * c.z + c.x * b.y * a.z;
}

// Calculate the normal vector of an ordered triangle, given as a Vec3f array
void func_col1_80097730(Vec3f* norm, Vec3f* tri) {
    __vtx0_x = tri->x;
    __vtx0_y = tri->y;
    __vtx0_z = tri->z;
    tri++;

    __vtx1_x = tri->x;
    __vtx1_y = tri->y;
    __vtx1_z = tri->z;
    tri++;

    __vtx2_x = tri->x;
    __vtx2_y = tri->y;
    __vtx2_z = tri->z;

    xz_vars.__dx1 = __vtx1_x - __vtx0_x;
    xz_vars.__dx2 = __vtx2_x - __vtx1_x;
    xz_vars.__dy1 = __vtx1_y - __vtx0_y;
    xz_vars.__dy2 = __vtx2_y - __vtx1_y;
    xz_vars.__dz1 = __vtx1_z - __vtx0_z;
    xz_vars.__dz2 = __vtx2_z - __vtx1_z;
    norm->x = (xz_vars.__dy1 * xz_vars.__dz2) - (xz_vars.__dz1 * xz_vars.__dy2);
    norm->y = (xz_vars.__dz1 * xz_vars.__dx2) - (xz_vars.__dx1 * xz_vars.__dz2);
    norm->z = (xz_vars.__dx1 * xz_vars.__dy2) - (xz_vars.__dy1 * xz_vars.__dx2);
}

// Calculate the normal vector of an ordered triangle, given as integer coordinates
void func_col1_800978C4(Vec3f* norm, s32 ax, s32 ay, s32 az, s32 bx, s32 by, s32 bz, s32 cx, s32 cy, s32 cz) {
    xz_vars.__dx1 = bx - ax;
    xz_vars.__dx2 = cx - bx;
    xz_vars.__dy1 = by - ay;
    xz_vars.__dy2 = cy - by;
    xz_vars.__dz1 = bz - az;
    xz_vars.__dz2 = cz - bz;

    norm->x = (xz_vars.__dy1 * xz_vars.__dz2) - (xz_vars.__dz1 * xz_vars.__dy2);
    norm->y = (xz_vars.__dz1 * xz_vars.__dx2) - (xz_vars.__dx1 * xz_vars.__dz2);
    norm->z = (xz_vars.__dx1 * xz_vars.__dy2) - (xz_vars.__dy1 * xz_vars.__dx2);
}

// Calculate the normal vector of an ordered triangle, given as vertices
void func_col1_800979E8(Vtx_tn* tri) {
    s32 i;
    Vec3f temp;
    f32 temp_fv0;

    tri++;
    tri--; // fake and weird

    __vtx0_x = tri[0].ob[0];
    __vtx0_y = tri[0].ob[1];
    __vtx0_z = tri[0].ob[2];
    __vtx1_x = tri[1].ob[0];
    __vtx1_y = tri[1].ob[1];
    __vtx1_z = tri[1].ob[2];
    __vtx2_x = tri[2].ob[0];
    __vtx2_y = tri[2].ob[1];
    __vtx2_z = tri[2].ob[2];

    xz_vars.__dx1 = __vtx1_x - __vtx0_x;
    xz_vars.__dx2 = __vtx2_x - __vtx1_x;
    xz_vars.__dy1 = __vtx1_y - __vtx0_y;
    xz_vars.__dy2 = __vtx2_y - __vtx1_y;
    xz_vars.__dz1 = __vtx1_z - __vtx0_z;
    xz_vars.__dz2 = __vtx2_z - __vtx1_z;

    temp.x = (xz_vars.__dy1 * xz_vars.__dz2) - (xz_vars.__dz1 * xz_vars.__dy2);
    temp.y = (xz_vars.__dz1 * xz_vars.__dx2) - (xz_vars.__dx1 * xz_vars.__dz2);
    temp.z = (xz_vars.__dx1 * xz_vars.__dy2) - (xz_vars.__dy1 * xz_vars.__dx2);
//    var_fv0 = 127.0f * shz_vec3_magnitude_inv((shz_vec3_t){v3.x,v3.y,v3.z});
//    temp_fv0 = VEC3F_MAG(&temp);
    temp_fv0 = 127.0f * shz_vec3_magnitude_inv((shz_vec3_t){temp.x,temp.y,temp.z});
    if (temp_fv0 != 0.0f) {
        temp.x = temp.x * temp_fv0; // 127.0f / temp_fv0;
        temp.y = temp.y * temp_fv0; // 127.0f / temp_fv0;
        temp.z = temp.z * temp_fv0; // 127.0f / temp_fv0;
        for (i = 0; i < 3; i++) {
            tri->n[0] = temp.x;
            tri->n[1] = temp.y;
            tri->n[2] = temp.z;
            tri++;
        }
    }
}

// calculate the normal vectors of the two ordered triangles in a quad
void func_col1_80097C88(Vec3f* norms, Vtx_tn* quad) {
    f32 temp_fv0;

    __vtx0_x = quad[3].ob[0];
    __vtx0_y = quad[3].ob[1];
    __vtx0_z = quad[3].ob[2];
    __vtx1_x = quad->ob[0];
    __vtx1_y = quad->ob[1];
    __vtx1_z = quad->ob[2];

    quad++;
    __vtx2_x = quad->ob[0];
    __vtx2_y = quad->ob[1];
    __vtx2_z = quad->ob[2];

    xz_vars.__dx1 = __vtx1_x - __vtx0_x;
    xz_vars.__dx2 = __vtx2_x - __vtx1_x;
    xz_vars.__dy1 = __vtx1_y - __vtx0_y;
    xz_vars.__dy2 = __vtx2_y - __vtx1_y;
    xz_vars.__dz1 = __vtx1_z - __vtx0_z;
    xz_vars.__dz2 = __vtx2_z - __vtx1_z;

    norms->x = (xz_vars.__dy1 * xz_vars.__dz2) - (xz_vars.__dz1 * xz_vars.__dy2);
    norms->y = (xz_vars.__dz1 * xz_vars.__dx2) - (xz_vars.__dx1 * xz_vars.__dz2);
    norms->z = (xz_vars.__dx1 * xz_vars.__dy2) - (xz_vars.__dy1 * xz_vars.__dx2);

//    temp_fv0 = VEC3F_MAG(norms);
    temp_fv0 = 127.0f * shz_vec3_magnitude_inv((shz_vec3_t){norms->x,norms->y,norms->z});
    if (temp_fv0 != 0.0f) {
        norms->x = (norms->x * temp_fv0);// / temp_fv0) * 127.0f;
        norms->y = (norms->y * temp_fv0);// / temp_fv0) * 127.0f;
        norms->z = (norms->z * temp_fv0);// / temp_fv0) * 127.0f;
    }

    norms++;
    quad++;

    __vtx1_x = __vtx2_x;
    __vtx1_y = __vtx2_y;
    __vtx1_z = __vtx2_z;

    __vtx2_x = quad->ob[0];
    __vtx2_y = quad->ob[1];
    __vtx2_z = quad->ob[2];

    xz_vars.__dx1 = __vtx1_x - __vtx0_x;
    xz_vars.__dx2 = __vtx2_x - __vtx1_x;
    xz_vars.__dy1 = __vtx1_y - __vtx0_y;
    xz_vars.__dy2 = __vtx2_y - __vtx1_y;
    xz_vars.__dz1 = __vtx1_z - __vtx0_z;
    xz_vars.__dz2 = __vtx2_z - __vtx1_z;

    norms->x = (xz_vars.__dy1 * xz_vars.__dz2) - (xz_vars.__dz1 * xz_vars.__dy2);
    norms->y = (xz_vars.__dz1 * xz_vars.__dx2) - (xz_vars.__dx1 * xz_vars.__dz2);
    norms->z = (xz_vars.__dx1 * xz_vars.__dy2) - (xz_vars.__dy1 * xz_vars.__dx2);

//    temp_fv0 = VEC3F_MAG(norms);
    temp_fv0 = 127.0f * shz_vec3_magnitude_inv((shz_vec3_t){norms->x,norms->y,norms->z});
    if (temp_fv0 != 0.0f) {
        norms->x = (norms->x * temp_fv0);// / temp_fv0) * 127.0f;
        norms->y = (norms->y * temp_fv0);// / temp_fv0) * 127.0f;
        norms->z = (norms->z * temp_fv0);// / temp_fv0) * 127.0f;
    }
}

// check if the projection of pos onto either ordered triangle of quad lies within that triangle
// If it does and lies on the positive side, return which triangle it's within and that triangle's normal
s32 func_col1_8009808C(Vec3f* pos, Vtx_tn* quad, Vec3f* normOut) {
    f32 temp_fv0;
    s32 var_v1 = 0;
    f32 test1;
    f32 test2;

    __vtx0_x = quad[0].ob[0];
    __vtx0_y = quad[0].ob[1];
    __vtx0_z = quad[0].ob[2];
    __vtx1_x = quad[1].ob[0];
    __vtx1_y = quad[1].ob[1];
    __vtx1_z = quad[1].ob[2];
    __vtx2_x = quad[2].ob[0];
    __vtx2_y = quad[2].ob[1];
    __vtx2_z = quad[2].ob[2];
    __vtx3_x = quad[3].ob[0];
    __vtx3_y = quad[3].ob[1];
    __vtx3_z = quad[3].ob[2];

    xz_vars.__pos_x = pos->x;
    xz_vars.__pos_z = pos->z;

    test1 = TRINORM_Y(__vtx0, __vtx1, xz_vars.__pos);
    if (test1 >= 0.0f) {
        test1 = TRINORM_Y(__vtx1, __vtx2, xz_vars.__pos);
        if (test1 >= 0.0f) {
            test1 = TRINORM_Y(__vtx2, __vtx0, xz_vars.__pos);
            if (test1 >= 0.0f) {
                normOut->x = TRINORM_X(__vtx0, __vtx1, __vtx2);
                normOut->y = TRINORM_Y(__vtx0, __vtx1, __vtx2);
                normOut->z = TRINORM_Z(__vtx0, __vtx1, __vtx2);
                if ((normOut->x != 0.0f) || (normOut->y != 0.0f) || (normOut->z != 0.0f)) {
                    var_v1 = 1;
                }
            }
        }
    }

    if (var_v1 == 0) {
        test1 = TRINORM_Y(__vtx0, __vtx2, xz_vars.__pos);
        if ((test1 >= 0.0f)) {
            test1 = TRINORM_Y(__vtx2, __vtx3, xz_vars.__pos);
            if (test1 >= 0.0f) {
                test1 = TRINORM_Y(__vtx3, __vtx0, xz_vars.__pos);
                if (test1 >= 0.0f) {
                    normOut->x = TRINORM_X(__vtx0, __vtx2, __vtx3);
                    normOut->y = TRINORM_Y(__vtx0, __vtx2, __vtx3);
                    normOut->z = TRINORM_Z(__vtx0, __vtx2, __vtx3);
                    if ((normOut->x != 0.0f) || (normOut->y != 0.0f) || (normOut->z != 0.0f)) {
                        var_v1 = 2;
                    }
                }
            }
        }
    }

    if (var_v1 != 0) {
//        temp_fv0 = VEC3F_MAG(normOut);
        temp_fv0 = 127.0f * shz_vec3_magnitude_inv((shz_vec3_t){normOut->x,normOut->y,normOut->z});
        if (temp_fv0 == 0) {
            return 0;
        }
        normOut->x = (normOut->x * temp_fv0);// / temp_fv0) * 127.0f;
        normOut->y = (normOut->y * temp_fv0);// / temp_fv0) * 127.0f;
        normOut->z = (normOut->z * temp_fv0);// / temp_fv0) * 127.0f;
    }
    return var_v1;
}

// check if vec lies within tri when projected to the xz-plane
bool func_col1_800985CC(Vec3f* vec, Vtx_tn* tri) {
    f32 sp24;
    f32 sp20;

    xz_vars.__pos_x = __vtx0_x = tri->ob[0];
    xz_vars.__pos_z = __vtx0_z = tri->ob[2];
    tri++;
    __vtx1_x = tri->ob[0];
    __vtx1_z = tri->ob[2];
    __vtx2_x = vec->x;
    __vtx2_z = vec->z;
    tri++;

    sp24 = TRINORM_Y(__vtx0, __vtx1, __vtx2);

    __vtx0_x = tri->ob[0];
    __vtx0_z = tri->ob[2];
    sp20 = TRINORM_Y(__vtx1, __vtx0, __vtx2);

    if (SIGN_OF(sp24) != SIGN_OF(sp20)) {
        return false;
    }
    sp24 = TRINORM_Y(__vtx0, xz_vars.__pos, __vtx2);

    if (SIGN_OF(sp24) != SIGN_OF(sp20)) {
        return false;
    }
    return true;
}

// PlaneF from normal and point
void func_col1_80098860(PlaneF* plane, Vec3f* point, Vec3f* normal) {
    plane->normal.x = normal->x;
    plane->normal.y = normal->y;
    plane->normal.z = normal->z;
    plane->dist = -normal->x * point->x - normal->y * point->y - normal->z * point->z;
//    shz_dot8f(-normal.x, -normal.y, -normal.z, 0, point->x, point->y, point->z, 0);
}

// y dist to closest point on plane
s32 func_col1_800988B4(Vec3f* vec, PlaneF* plane) {
    f32 recY = shz_fast_invf(plane->normal.y);
    return (-plane->normal.x * vec->x - plane->normal.z * vec->z - plane->dist) * recY; // / plane->normal.y;
}

// z dist to closest point on plane
s32 func_col1_800988F8(Vec3f* vec, PlaneF* plane) {
    f32 recZ = shz_fast_invf(plane->normal.z);
    return (-plane->normal.x * vec->x - plane->normal.y * vec->y - plane->dist) * recZ; // / plane->normal.z;
}

// x dist to closest point on plane
s32 func_col1_8009893C(Vec3f* vec, PlaneF* plane) {
    f32 recX = shz_fast_invf(plane->normal.x);
    return (-plane->normal.y * vec->y - plane->normal.z * vec->z - plane->dist) * recX; // / plane->normal.x;
}

#define INTSIGN_OF(x) ((((x) >= 1.0f) || ((x) <= -1.0f)) ? (f32) SIGN_OF(x) : 0.0f)
static inline Vec3f v3f_sub(Vec3f a, Vec3f b) {
    return (Vec3f){a.x - b.x, a.y - b.y, a.z - b.z};
}
static inline Vec3f v3f_cross(Vec3f a, Vec3f b) {
    return (Vec3f){a.y*b.z - a.z*b.y, a.z*b.x - a.x*b.z, a.x*b.y - a.y*b.x};
}
static inline float v3f_dot(Vec3f a, Vec3f b) {
    return a.x*b.x + a.y*b.y + a.z*b.z;
}

static inline Vec3f to_f(Vec3s *v) {
    return (Vec3f){ (float)v->x, (float)v->y, (float)v->z };
}
bool not_func_col1_80098980( Vec3f *p, Vec3s **tri,  Vec3f *n)
{
    Vec3f v0 = to_f(*tri++), v1 = to_f(*tri++), v2 = to_f(*tri++);

    // choose dominant axis of the normal
    float ax = fabsf(n->x), ay = fabsf(n->y), az = fabsf(n->z);
    int axis = (ax > ay && ax > az) ? 0 : (ay > az ? 1 : 2);

    // project to the plane orthogonal to 'axis'
    float pA, pB, v0A, v0B, v1A, v1B, v2A, v2B;
    if (axis == 0) {        // drop X -> use (y,z)
        pA = p->y; pB = p->z;
        v0A = v0.y; v0B = v0.z; v1A = v1.y; v1B = v1.z; v2A = v2.y; v2B = v2.z;
    } else if (axis == 1) { // drop Y -> use (z,x)
        pA = p->z; pB = p->x;
        v0A = v0.z; v0B = v0.x; v1A = v1.z; v1B = v1.x; v2A = v2.z; v2B = v2.x;
    } else {                // drop Z -> use (x,y)
        pA = p->x; pB = p->y;
        v0A = v0.x; v0B = v0.y; v1A = v1.x; v1B = v1.y; v2A = v2.x; v2B = v2.y;
    }

    // 2D edge “left of” tests; sign must match dominant normal component
    float s = (axis == 0 ? n->x : (axis == 1 ? n->y : n->z));
    float e0 = (pA - v0A)*(v1B - v0B) - (pB - v0B)*(v1A - v0A);
    float e1 = (pA - v1A)*(v2B - v1B) - (pB - v1B)*(v2A - v1A);
    float e2 = (pA - v2A)*(v0B - v2B) - (pB - v2B)*(v0A - v2A);

    if (s >= 0.0f) return (e0 >= 0.0f && e1 >= 0.0f && e2 >= 0.0f);
    else           return (e0 <= 0.0f && e1 <= 0.0f && e2 <= 0.0f);
}

// checks if the projection of pos onto the plane of tri lies within tri and it is on the same side as the normal.
bool func_col1_80098980(Vec3f* pos, Vec3s** tri, Vec3f* normal) {
    s32 normSignY;
    s32 normSignZ;
    s32 signCross;
    s32 normSignX;
    f32 cross;
    s32 var_v1;
    Vec3s* pad;
    f32 t0x;
    f32 t0y;
    f32 t0z;
    f32 t1x;
    f32 t1y;
    f32 t1z;
    f32 t2x;
    f32 t2y;
    f32 t2z;
    f32 xPos;
    f32 yPos;
    f32 zPos;
    f32 d01x;
    f32 d12x;
    f32 d20x;
    f32 d01y;
    f32 d12y;
    f32 d20y;
    f32 d01z;
    f32 d12z;
    f32 d20z;

    var_v1 = false;

    pad = *tri++;
    if (1) { // some sort of macro?
        t0x = pad->x;
        t0y = pad->y;
        t0z = pad->z;

        pad = *tri++;

        t1x = pad->x;
        t1y = pad->y;
        t1z = pad->z;

        pad = *tri++;

        t2x = pad->x;
        t2y = pad->y;
        t2z = pad->z;

        xPos = pos->x;
        yPos = pos->y;
        zPos = pos->z;

        d01x = t1x - t0x;
        d12x = t2x - t1x;
        d20x = t0x - t2x;
        d01y = t1y - t0y;
        d12y = t2y - t1y;
        d20y = t0y - t2y;
        d01z = t1z - t0z;
        d12z = t2z - t1z;
        d20z = t0z - t2z;

        normSignX = SIGN_OF(normal->x);
        normSignY = SIGN_OF(normal->y);
        normSignZ = SIGN_OF(normal->z);
    }

    cross = ((zPos - t1z) * d01y) - (d01z * (yPos - t1y));
    signCross = INTSIGN_OF(cross);
    if ((normSignX == 0) || (signCross == normSignX) || (signCross == 0)) {
        cross = ((zPos - t2z) * d12y) - (d12z * (yPos - t2y));
        signCross = INTSIGN_OF(cross);
        if ((normSignX == 0) || (signCross == normSignX) || (signCross == 0)) {
            cross = ((zPos - t0z) * d20y) - (d20z * (yPos - t0y));
            signCross = INTSIGN_OF(cross);
            if ((normSignX == 0) || (signCross == normSignX) || (signCross == 0)) {
                cross = ((xPos - t1x) * d01z) - (d01x * (zPos - t1z));
                signCross = INTSIGN_OF(cross);
                if ((normSignY == 0) || (signCross == normSignY) || (signCross == 0)) {
                    cross = ((xPos - t2x) * d12z) - (d12x * (zPos - t2z));
                    signCross = INTSIGN_OF(cross);
                    if ((normSignY == 0) || (signCross == normSignY) || (signCross == 0)) {
                        cross = ((xPos - t0x) * d20z) - (d20x * (zPos - t0z));
                        signCross = INTSIGN_OF(cross);
                        if ((normSignY == 0) || (signCross == normSignY) || (signCross == 0)) {
                            cross = ((yPos - t1y) * d01x) - (d01y * (xPos - t1x));
                            signCross = INTSIGN_OF(cross);
                            if ((normSignZ == 0) || (signCross == normSignZ) || (signCross == 0)) {
                                cross = ((yPos - t2y) * d12x) - (d12y * (xPos - t2x));
                                signCross = INTSIGN_OF(cross);
                                if ((normSignZ == 0) || (signCross == normSignZ) || (signCross == 0)) {
                                    cross = ((yPos - t0y) * d20x) - (d20y * (xPos - t0x));
                                    signCross = INTSIGN_OF(cross);
                                    if ((normSignZ == 0) || (signCross == 0) || (signCross == normSignZ)) {
                                        var_v1 = true;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return var_v1;
}

#ifndef F_PI
#define F_PI        3.1415926f   /* pi             */
#endif
float my_acosf (float a);

bool func_80099254(Vec3f* objPos, Vec3f* colliderPos, Vec3f* objVel, CollisionHeader* colHeader, Vec3f* hitPosOut,
                   f32* hitAnglesOut) {
    Vec3s* polyVtxPos[3];
    Vec3f objRel;
    s32 pad11C;
    s32 pad118;
    PlaneF polyPlane;
    f32 tempf;
    s32 didHit = false;
    s32 swapBuff;
    CollisionPoly* colPoly;
    Vec3s* mesh;
    s32 polyCount;
    f32 speed;
    s32 i;
    Vec3f hitPosRel;
    s32 padD8;
    s32 polyMinX;
    s32 polyMaxX;
    s32 polyMinY;
    s32 polyMaxY;
    s32 polyMinZ;
    s32 polyMaxZ;
    s32 objMinX;
    s32 objMaxX;
    s32 objMinY;
    s32 objMaxY;
    s32 objMinZ;
    s32 objMaxZ;
    Vec3f min;
    Vec3f max;

    hitPosOut->x = hitPosOut->y = hitPosOut->z = hitAnglesOut[0] = hitAnglesOut[1] = 0.0f;
    objRel.x = objPos->x - colliderPos->x;
    objRel.y = objPos->y - colliderPos->y;
    objRel.z = objPos->z - colliderPos->z;
    min.x = colHeader->min.x;
    min.y = colHeader->min.y;
    min.z = colHeader->min.z;
    max.x = colHeader->max.x;
    max.y = colHeader->max.y;
    max.z = colHeader->max.z;

    // check if object is outside the collision's bounding box
    if ((objRel.x < min.x) || (objRel.y < min.y) || (objRel.z < min.z) || (max.x < objRel.x) || (max.y < objRel.y) ||
        ((max.z < objRel.z))) {
        return false;
    }
    if ((objVel->x == 0.0f) && (objVel->y == 0.0f) && (objVel->z == 0.0f)) {
        objVel->y = -5.0f;
    }

    // find the bounding box of the object's movement from the last frame to this one.
    objMinX = objRel.x;
    objMinY = objRel.y;
    objMinZ = objRel.z;
    objMaxX = objRel.x - objVel->x;
    objMaxY = objRel.y - objVel->y;
    objMaxZ = objRel.z - objVel->z;

    if (objMaxX < objMinX) {
        swapBuff = objMaxX;
        objMaxX = objMinX;
        objMinX = swapBuff;
    }
    if (objMaxY < objMinY) {
        swapBuff = objMaxY;
        objMaxY = objMinY;
        objMinY = swapBuff;
    }

    if (objMaxZ < objMinZ) {
        swapBuff = objMaxZ;
        objMaxZ = objMinZ;
        objMinZ = swapBuff;
    }

    colPoly = SEGMENTED_TO_VIRTUAL(colHeader->polys);
    mesh = SEGMENTED_TO_VIRTUAL(colHeader->mesh);
    polyCount = colHeader->polyCount;
    speed = VEC3F_MAG(objVel);

    for (i = 0; i < polyCount; i++, colPoly++) {
        polyVtxPos[0] = &mesh[colPoly->tri.vtx[0]];
        polyVtxPos[1] = &mesh[colPoly->tri.vtx[1]];
        polyVtxPos[2] = &mesh[colPoly->tri.vtx[2]];
        Math_MinMax(&polyMinX, &polyMaxX, polyVtxPos[0]->x, polyVtxPos[1]->x, polyVtxPos[2]->x);
        Math_MinMax(&polyMinY, &polyMaxY, polyVtxPos[0]->y, polyVtxPos[1]->y, polyVtxPos[2]->y);
        Math_MinMax(&polyMinZ, &polyMaxZ, polyVtxPos[0]->z, polyVtxPos[1]->z, polyVtxPos[2]->z);

        // check if bounding boxes of the object's movement and the collision polygon overlap
        if ((objMinX < polyMaxX) && (objMaxX > polyMinX) && (objMinY < polyMaxY) && (objMaxY > polyMinY) &&
            (objMinZ < polyMaxZ) && (objMaxZ > polyMinZ)) {
            polyPlane.normal.x = colPoly->plane.normal.x;
            polyPlane.normal.y = colPoly->plane.normal.y;
            polyPlane.normal.z = colPoly->plane.normal.z;
            polyPlane.dist = colPoly->plane.dist;

            // check if object is on the "back" side of the polygon
            if ((DOT_XYZ(&polyPlane.normal, &objRel) + polyPlane.dist) <= 0.0f) {

                // calculate the normal component of velocity
                tempf = DOT_XYZ(&polyPlane.normal, objVel);

                // check if the angle between the normal and velocity is > 90. That is, the object was moving toward the
                // front of the polygon
                f32 recipThing = shz_fast_invf((VEC3F_MAG(&polyPlane.normal) * speed));
                if (/* Math_FAcosF */my_acosf(tempf * recipThing) > DEG_TO_RAD(90.0f)) {
                    // Calculate the time since the plane was crossed. Reusing the temp is required to match
                    f32 reciptemp = shz_fast_invf(tempf);
                    tempf = (DOT_XYZ(&polyPlane.normal, &objRel) + polyPlane.dist) * reciptemp; // / tempf;

                    // find the point where the object crossed the plane of the polygon
                    hitPosRel.x = objRel.x - (objVel->x * tempf);
                    hitPosRel.y = objRel.y - (objVel->y * tempf);
                    hitPosRel.z = objRel.z - (objVel->z * tempf);

                    // check if that point is within the polygon
                    if (func_col1_80098980(&hitPosRel, polyVtxPos, &polyPlane.normal) == true) {
                        hitPosOut->x = colliderPos->x + hitPosRel.x;
                        hitPosOut->y = colliderPos->y + hitPosRel.y;
                        hitPosOut->z = colliderPos->z + hitPosRel.z;
                        if (polyPlane.normal.x != 0.0) {
                            f32 recipN = shz_fast_invf(polyPlane.normal.x);
                            polyPlane.normal.x = -polyPlane.dist * recipN; // / polyPlane.normal.x;
                        }
                        if (polyPlane.normal.y != 0.0f) {
                            f32 recipN = shz_fast_invf(polyPlane.normal.y);
                            polyPlane.normal.y = -polyPlane.dist * recipN; //  / polyPlane.normal.y;
                        }
                        if (polyPlane.normal.z != 0.0f) {
                            f32 recipN = shz_fast_invf(polyPlane.normal.z);
                            polyPlane.normal.z = -polyPlane.dist * recipN; // / polyPlane.normal.z;
                        }
                        hitAnglesOut[0] = Math_Atan2F_XY(polyPlane.normal.y, polyPlane.normal.z);
                        if (polyPlane.normal.z != 0.0f) {
                            hitAnglesOut[1] = -Math_Atan2F_XY(
                                sinf(Math_Atan2F_XY(polyPlane.normal.y, polyPlane.normal.z)) * polyPlane.normal.z,
                                polyPlane.normal.x);
                        } else if (hitAnglesOut[0] >= F_PI) {
                            hitAnglesOut[1] = Math_Atan2F_XY(polyPlane.normal.y, polyPlane.normal.x);
                        } else {
                            hitAnglesOut[1] = -Math_Atan2F_XY(polyPlane.normal.y, polyPlane.normal.x);
                        }

                        didHit = true;
                        break;
                    }
                }
            }
        }
    }
    return didHit;
}

bool func_col1_800998FC(Vec3f* objPos, Vec3f* colliderPos, Vec3f* objVel, s32 colId, Vec3f* hitPosOut,
                        f32* hitAnglesOut) {
    return func_80099254(objPos, colliderPos, objVel, SEGMENTED_TO_VIRTUAL(&D_800D2B38[colId]), hitPosOut,
                         hitAnglesOut);
}
