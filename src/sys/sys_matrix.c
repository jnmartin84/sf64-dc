#include <dc/fmath.h>
#include "n64sys.h"

#include <stdint.h>
#include <math.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>


//#define M_DTOR	(M_PI / 180.0f)
#define M_RTOD	(180.0f / F_PI)
#if GBI_FLOATS
Mtx gIdentityMtx = { {
    /*{*/
        { 1.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 1.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 1.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 1.0f },
    /*} ,
     {
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f },
    }, */
} };
Mtx gIdentityMtx2 = { {
 { 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f },
        { 0.0f, 0.0f, 0.0f, 0.0f },
}};
#else

Mtx gIdentityMtx = { {
    {
        { 1, 0, 0, 0 },
        { 0, 1, 0, 0 },
        { 0, 0, 1, 0 },
        { 0, 0, 0, 1 },
    },
    {
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 },
        { 0, 0, 0, 0 },
    },
} };

#endif


Matrix /* __attribute__((aligned(32))) */ gIdentityMatrix = { {
    { 1.0f, 0.0f, 0.0f, 0.0f },
    { 0.0f, 1.0f, 0.0f, 0.0f },
    { 0.0f, 0.0f, 1.0f, 0.0f },
    { 0.0f, 0.0f, 0.0f, 1.0f },
} };

Matrix sCalcMatrixStack[0x20];// = {0};
Matrix* gCalcMatrix;
Matrix sGfxMatrixStack[0x20];// = {0};
Matrix* gGfxMatrix;

// Copies src Matrix into dst
void n64_memcpy(void *dst, const void *src, size_t size);
void  Matrix_Copy(Matrix* dst, Matrix* src) {
    n64_memcpy(dst->m, src->m, sizeof(float)*16);
#if 0
    s32 i;
    for (i = 0; i < 4; i++) {
        dst->m[i][0] = src->m[i][0];
        dst->m[i][1] = src->m[i][1];
        dst->m[i][2] = src->m[i][2];
        dst->m[i][3] = src->m[i][3];
    }
#endif
}

// Makes a copy of the stack's current matrix and puts it on the top of the stack
void  Matrix_Push(Matrix** mtxStack) {
    Matrix_Copy(*mtxStack + 1, *mtxStack);
    (*mtxStack)++;
}

// Removes the top matrix of the stack
void  Matrix_Pop(Matrix** mtxStack) {
    (*mtxStack)--;
}
#if 0
static inline void shz_xmtrx_store_4x4_unaligned(float matrix[16]) {
    asm volatile(R"(
        frchg
        add     #64, %[mtx]
        fmov.s  fr15, @-%[mtx]
        add     #-32, %[mtx]
        pref    @%[mtx]
        add     #32, %[mtx]
        fmov.s  fr14, @-%[mtx]
        fmov.s  fr13, @-%[mtx]
        fmov.s  fr12, @-%[mtx]
        fmov.s  fr11, @-%[mtx]
        fmov.s  fr10, @-%[mtx]
        fmov.s  fr9, @-%[mtx]
        fmov.s  fr8, @-%[mtx]
        fmov.s  fr7, @-%[mtx]
        fmov.s  fr6, @-%[mtx]
        fmov.s  fr5, @-%[mtx]
        fmov.s  fr4, @-%[mtx]
        fmov.s  fr3, @-%[mtx]
        fmov.s  fr2, @-%[mtx]
        fmov.s  fr1, @-%[mtx]
        fmov.s  fr0, @-%[mtx]
        frchg
    )"
    : "=m" (*matrix)
    : [mtx] "r" (matrix));
}
static inline void shz_xmtrx_load_4x4_unaligned(const float matrix[16]) {
    asm volatile(R"(
        frchg
        fmov.s  @%[mtx]+, fr0
        add     #32, %[mtx]
        pref    @%[mtx]
        add     #-32, %[mtx]
        fmov.s  @%[mtx]+, fr1
        fmov.s  @%[mtx]+, fr2
        fmov.s  @%[mtx]+, fr3
        fmov.s  @%[mtx]+, fr4
        fmov.s  @%[mtx]+, fr5
        fmov.s  @%[mtx]+, fr6
        fmov.s  @%[mtx]+, fr7
        fmov.s  @%[mtx]+, fr8
        fmov.s  @%[mtx]+, fr9
        fmov.s  @%[mtx]+, fr10
        fmov.s  @%[mtx]+, fr11
        fmov.s  @%[mtx]+, fr12
        fmov.s  @%[mtx]+, fr13
        fmov.s  @%[mtx]+, fr14
        fmov.s  @%[mtx]+, fr15
        frchg
    )"
    : [mtx] "+r" (matrix)
    :  "m" (*matrix));
}
static inline void shz_xmtrx_apply_4x4_unaligned(const float matrix[16]) {
    asm volatile(R"(
        mov     r15, r0
        pref    @%[mtx]
        or      #0x0f, r0
        xor     #0x0f, r0
        mov     r15, r7

        fschg
        mov     r0, r15
        fmov.d  dr14, @-r15
        fmov.d  dr12, @-r15
        fschg

        fmov.s  @%[mtx]+, fr0
        add     #32, %[mtx]
        pref    @%[mtx]
        add     #-32, %[mtx]
        fmov.s  @%[mtx]+, fr1
        fmov.s  @%[mtx]+, fr2
        fmov.s  @%[mtx]+, fr3

        ftrv    xmtrx, fv0

        fmov.s  @%[mtx]+, fr4
        fmov.s  @%[mtx]+, fr5
        fmov.s  @%[mtx]+, fr6
        fmov.s  @%[mtx]+, fr7

        ftrv    xmtrx, fv4

        fmov.s  @%[mtx]+, fr8
        fmov.s  @%[mtx]+, fr9
        fmov.s  @%[mtx]+, fr10
        fmov.s  @%[mtx]+, fr11

        ftrv    xmtrx, fv8

        fmov.s  @%[mtx]+, fr12
        fmov.s  @%[mtx]+, fr13
        fmov.s  @%[mtx]+, fr14
        fmov.s  @%[mtx]+, fr15

        ftrv    xmtrx, fv12

        frchg

        fschg
        fmov.d  @r15+, dr12
        fmov.d  @r15, dr14
        mov     r7, r15
        fschg
    )"
    : [mtx] "+r" (matrix)
    :  "m" (*matrix)
    : "r0", "r7", "fr0", "fr1", "fr2", "fr3", "fr4", "fr5", "fr6",
      "fr7", "fr8", "fr9", "fr10", "fr11", "fr12");
}
#endif
// Copies tf into mtx (MTXF_NEW) or applies it to mtx (MTXF_APPLY)
void  Matrix_Mult(Matrix* mtx, Matrix* tf, u8 mode) {
    f32 rx;
    f32 ry;
    f32 rz;
    f32 rw;
    s32 i0;
    s32 i1;
    s32 i2;
    s32 i3;

    if (mode == MTXF_APPLY) {
        shz_xmtrx_load_4x4_unaligned(mtx);
        shz_xmtrx_apply_4x4_unaligned(tf);
        shz_xmtrx_store_4x4_unaligned(mtx);
#if 0
        rx = mtx->m[0][0];
        ry = mtx->m[1][0];
        rz = mtx->m[2][0];
        rw = mtx->m[3][0];

        for (i0 = 0; i0 < 4; i0++) {
            mtx->m[i0][0] = //fipr(rx,ry,rz,rw,tf->m[i0][0],tf->m[i0][1],tf->m[i0][2],tf->m[i0][3]);
             (rx * tf->m[i0][0]) + (ry * tf->m[i0][1]) + (rz * tf->m[i0][2]) + (rw * tf->m[i0][3]);
        }

        rx = mtx->m[0][1];
        ry = mtx->m[1][1];
        rz = mtx->m[2][1];
        rw = mtx->m[3][1];

        for (i1 = 0; i1 < 4; i1++) {
            mtx->m[i1][1] = //fipr(rx,ry,rz,rw,tf->m[i1][0],tf->m[i1][1],tf->m[i1][2],tf->m[i1][3]);
//            mtx->m[i1][1] = 
(rx * tf->m[i1][0]) + (ry * tf->m[i1][1]) + (rz * tf->m[i1][2]) + (rw * tf->m[i1][3]);
        }

        rx = mtx->m[0][2];
        ry = mtx->m[1][2];
        rz = mtx->m[2][2];
        rw = mtx->m[3][2];

        for (i2 = 0; i2 < 4; i2++) {
            mtx->m[i2][2] = //fipr(rx,ry,rz,rw,tf->m[i2][0],tf->m[i2][1],tf->m[i2][2],tf->m[i2][3]);
//            mtx->m[i2][2] = 
(rx * tf->m[i2][0]) + (ry * tf->m[i2][1]) + (rz * tf->m[i2][2]) + (rw * tf->m[i2][3]);
        }

        rx = mtx->m[0][3];
        ry = mtx->m[1][3];
        rz = mtx->m[2][3];
        rw = mtx->m[3][3];

        for (i3 = 0; i3 < 4; i3++) {
            mtx->m[i3][3] = //fipr(rx,ry,rz,rw,tf->m[i3][0],tf->m[i3][1],tf->m[i3][2],tf->m[i3][3]);
//            mtx->m[i3][3] = 
(rx * tf->m[i3][0]) + (ry * tf->m[i3][1]) + (rz * tf->m[i3][2]) + (rw * tf->m[i3][3]);
        }
#endif
    } else {
        Matrix_Copy(mtx, tf);
    }
}

// Creates a translation matrix in mtx (MTXF_NEW) or applies one to mtx (MTXF_APPLY)
void Matrix_Translate(Matrix* mtx, f32 x, f32 y, f32 z, u8 mode) {
    f32 rx;
    f32 ry;
    s32 i;

    if (mode == MTXF_APPLY) {
        for (i = 0; i < 4; i++) {
            rx = mtx->m[0][i];
            ry = mtx->m[1][i];

            mtx->m[3][i] += fipr(rx,ry,mtx->m[2][i],0,x,y,z,0);
            //(rx * x) + (ry * y) + (mtx->m[2][i] * z);
        }
    } else {
        mtx->m[3][0] = x;
        mtx->m[3][1] = y;
        mtx->m[3][2] = z;
        mtx->m[0][1] = mtx->m[0][2] = mtx->m[0][3] = mtx->m[1][0] = mtx->m[1][2] = mtx->m[1][3] = mtx->m[2][0] =
            mtx->m[2][1] = mtx->m[2][3] = 0.0f;
        mtx->m[0][0] = mtx->m[1][1] = mtx->m[2][2] = mtx->m[3][3] = 1.0f;
    }
}

// Creates a scale matrix in mtx (MTXF_NEW) or applies one to mtx (MTXF_APPLY)
void Matrix_Scale(Matrix* mtx, f32 xScale, f32 yScale, f32 zScale, u8 mode) {
    f32 rx;
    f32 ry;
    s32 i;

    if (mode == MTXF_APPLY) {
        for (i = 0; i < 4; i++) {
            rx = mtx->m[0][i];
            ry = mtx->m[1][i];

            mtx->m[0][i] = rx * xScale;
            mtx->m[1][i] = ry * yScale;
            mtx->m[2][i] *= zScale;
        }
    } else {
        mtx->m[0][0] = xScale;
        mtx->m[1][1] = yScale;
        mtx->m[2][2] = zScale;
        mtx->m[0][1] = mtx->m[0][2] = mtx->m[0][3] = mtx->m[1][0] = mtx->m[1][2] = mtx->m[1][3] = mtx->m[2][0] =
            mtx->m[2][1] = mtx->m[2][3] = mtx->m[3][0] = mtx->m[3][1] = mtx->m[3][2] = 0.0f;
        mtx->m[3][3] = 1.0f;
    }
}

// Creates rotation matrix about the X axis in mtx (MTXF_NEW) or applies one to mtx (MTXF_APPLY)
void Matrix_RotateX(Matrix* mtx, f32 angle, u8 mode) {
    f32 cs;
    f32 sn;
    f32 ry;
    f32 rz;
    s32 i;

    sn = sinf(angle);
    cs = cosf(angle);
    if (mode == MTXF_APPLY) {
        for (i = 0; i < 4; i++) {
            ry = mtx->m[1][i];
            rz = mtx->m[2][i];

            mtx->m[1][i] = (ry * cs) + (rz * sn);
            mtx->m[2][i] = (rz * cs) - (ry * sn);
        }
    } else {
        mtx->m[1][1] = mtx->m[2][2] = cs;
        mtx->m[1][2] = sn;
        mtx->m[2][1] = -sn;
        mtx->m[0][0] = mtx->m[3][3] = 1.0f;
        mtx->m[0][1] = mtx->m[0][2] = mtx->m[0][3] = mtx->m[1][0] = mtx->m[1][3] = mtx->m[2][0] = mtx->m[2][3] =
            mtx->m[3][0] = mtx->m[3][1] = mtx->m[3][2] = 0.0f;
    }
}

// Creates rotation matrix about the Y axis in mtx (MTXF_NEW) or applies one to mtx (MTXF_APPLY)
void Matrix_RotateY(Matrix* mtx, f32 angle, u8 mode) {
    f32 cs;
    f32 sn;
    f32 rx;
    f32 rz;
    s32 i;

    sn = sinf(angle);
    cs = cosf(angle);
    if (mode == MTXF_APPLY) {
        for (i = 0; i < 4; i++) {
            rx = mtx->m[0][i];
            rz = mtx->m[2][i];

            mtx->m[0][i] = (rx * cs) - (rz * sn);
            mtx->m[2][i] = (rx * sn) + (rz * cs);
        }
    } else {
        mtx->m[0][0] = mtx->m[2][2] = cs;
        mtx->m[0][2] = -sn;
        mtx->m[2][0] = sn;
        mtx->m[1][1] = mtx->m[3][3] = 1.0f;
        mtx->m[0][1] = mtx->m[0][3] = mtx->m[1][0] = mtx->m[1][2] = mtx->m[1][3] = mtx->m[2][1] = mtx->m[2][3] =
            mtx->m[3][0] = mtx->m[3][1] = mtx->m[3][2] = 0.0f;
    }
}

// Creates rotation matrix about the Z axis in mtx (MTXF_NEW) or applies one to mtx (MTXF_APPLY)
void Matrix_RotateZ(Matrix* mtx, f32 angle, u8 mode) {
    f32 cs;
    f32 sn;
    f32 rx;
    f32 ry;
    s32 i;

    sn = sinf(angle);
    cs = cosf(angle);
    if (mode == MTXF_APPLY) {
        for (i = 0; i < 4; i++) {
            rx = mtx->m[0][i];
            ry = mtx->m[1][i];

            mtx->m[0][i] = (rx * cs) + (ry * sn);
            mtx->m[1][i] = (ry * cs) - (rx * sn);
        }
    } else {
        mtx->m[0][0] = mtx->m[1][1] = cs;
        mtx->m[0][1] = sn;
        mtx->m[1][0] = -sn;
        mtx->m[2][2] = mtx->m[3][3] = 1.0f;
        mtx->m[0][2] = mtx->m[0][3] = mtx->m[1][2] = mtx->m[1][3] = mtx->m[2][0] = mtx->m[2][1] = mtx->m[2][3] =
            mtx->m[3][0] = mtx->m[3][1] = mtx->m[3][2] = 0.0f;
    }
}

// Creates rotation matrix about a given vector axis in mtx (MTXF_NEW) or applies one to mtx (MTXF_APPLY).
// The vector specifying the axis does not need to be a unit vector.
void Matrix_RotateAxis(Matrix* mtx, f32 angle, f32 axisX, f32 axisY, f32 axisZ, u8 mode) {
    f32 rx;
    f32 ry;
    f32 rz;
    f32 norm;
    f32 cxx;
    f32 cyx;
    f32 czx;
    f32 cxy;
    f32 cyy;
    f32 czy;
    f32 cxz;
    f32 cyz;
    f32 czz;
    f32 xx;
    f32 yy;
    f32 zz;
    f32 xy;
    f32 yz;
    f32 xz;
    f32 sinA;
    f32 cosA;

    norm = sqrtf((axisX * axisX) + (axisY * axisY) + (axisZ * axisZ));
    if (norm != 0.0f) {
        axisX /= norm;
        axisY /= norm;
        axisZ /= norm;
        sinA = sinf(angle);
        cosA = cosf(angle);
        xx = axisX * axisX;
        yy = axisY * axisY;
        zz = axisZ * axisZ;
        xy = axisX * axisY;
        yz = axisY * axisZ;
        xz = axisX * axisZ;

        if (mode == MTXF_APPLY) {
            cxx = (1.0f - xx) * cosA + xx;
            cyx = (1.0f - cosA) * xy + axisZ * sinA;
            czx = (1.0f - cosA) * xz - axisY * sinA;

            cxy = (1.0f - cosA) * xy - axisZ * sinA;
            cyy = (1.0f - yy) * cosA + yy;
            czy = (1.0f - cosA) * yz + axisX * sinA;

            cxz = (1.0f - cosA) * xz + axisY * sinA;
            cyz = (1.0f - cosA) * yz - axisX * sinA;
            czz = (1.0f - zz) * cosA + zz;

            // loop doesn't seem to work here.
            rx = mtx->m[0][0];
            ry = mtx->m[0][1];
            rz = mtx->m[0][2];
            mtx->m[0][0] = //fipr(rx,ry,rz,0,cxx,cxy,cxz,0);//
            (rx * cxx) + (ry * cxy) + (rz * cxz);
            mtx->m[0][1] = //fipr(rx,ry,rz,0,cyx,cyy,cyz,0);//
            (rx * cyx) + (ry * cyy) + (rz * cyz);
            mtx->m[0][2] = //fipr(rx,ry,rz,0,czx,czy,czz,0);//
            (rx * czx) + (ry * czy) + (rz * czz);

            rx = mtx->m[1][0];
            ry = mtx->m[1][1];
            rz = mtx->m[1][2];
            mtx->m[1][0] = //fipr(rx,ry,rz,0,cxx,cxy,cxz,0);//
            (rx * cxx) + (ry * cxy) + (rz * cxz);
            mtx->m[1][1] = //fipr(rx,ry,rz,0,cyx,cyy,cyz,0);//
            (rx * cyx) + (ry * cyy) + (rz * cyz);
            mtx->m[1][2] = //fipr(rx,ry,rz,0,czx,czy,czz,0);//
            (rx * czx) + (ry * czy) + (rz * czz);

            rx = mtx->m[2][0];
            ry = mtx->m[2][1];
            rz = mtx->m[2][2];
            mtx->m[2][0] = //fipr(rx,ry,rz,0,cxx,cxy,cxz,0);//
            (rx * cxx) + (ry * cxy) + (rz * cxz);
            mtx->m[2][1] = //fipr(rx,ry,rz,0,cyx,cyy,cyz,0);//
            (rx * cyx) + (ry * cyy) + (rz * cyz);
            mtx->m[2][2] = //fipr(rx,ry,rz,0,czx,czy,czz,0);//
            (rx * czx) + (ry * czy) + (rz * czz);
        } else {
            mtx->m[0][0] = (1.0f - xx) * cosA + xx;
            mtx->m[0][1] = (1.0f - cosA) * xy + axisZ * sinA;
            mtx->m[0][2] = (1.0f - cosA) * xz - axisY * sinA;
            mtx->m[0][3] = 0.0f;

            mtx->m[1][0] = (1.0f - cosA) * xy - axisZ * sinA;
            mtx->m[1][1] = (1.0f - yy) * cosA + yy;
            mtx->m[1][2] = (1.0f - cosA) * yz + axisX * sinA;
            mtx->m[1][3] = 0.0f;

            mtx->m[2][0] = (1.0f - cosA) * xz + axisY * sinA;
            mtx->m[2][1] = (1.0f - cosA) * yz - axisX * sinA;
            mtx->m[2][2] = (1.0f - zz) * cosA + zz;
            mtx->m[2][3] = 0.0f;

            mtx->m[3][0] = mtx->m[3][1] = mtx->m[3][2] = 0.0f;
            mtx->m[3][3] = 1.0f;
        }
    }
}

// Converts the current Gfx matrix to a Mtx
void Matrix_ToMtx(Mtx* m) {
    guMtxF2L(gGfxMatrix->m, m->m);
}

#include <kos.h>

void Matrix_LoadOnly(Matrix* mtx) {
    shz_xmtrx_load_4x4_unaligned(mtx);
}


// Applies the transform matrix mtx to the vector src, putting the result in dest
void Matrix_MultVec4f_NoLoad(Vec4f* src, Vec4f* dest) {
    dest->x = src->x;
    dest->y = src->y;
    dest->z = src->z;
    dest->w = src->w;
    mat_trans_single3_nodivw(dest->x, dest->y, dest->z, dest->w);
//    dest->x = //fipr(mtx->m[0][0],mtx->m[1][0],mtx->m[2][0],mtx->m[3][0],src->x,src->y,src->z,1);//
  //  (mtx->m[0][0] * src->x) + (mtx->m[1][0] * src->y) + (mtx->m[2][0] * src->z) + mtx->m[3][0];
    //dest->y = //fipr(mtx->m[0][1],mtx->m[1][1],mtx->m[2][1],mtx->m[3][1],src->x,src->y,src->z,1);//
   // (mtx->m[0][1] * src->x) + (mtx->m[1][1] * src->y) + (mtx->m[2][1] * src->z) + mtx->m[3][1];
    //dest->z = //fipr(mtx->m[0][2],mtx->m[1][2],mtx->m[2][2],mtx->m[3][2],src->x,src->y,src->z,1);//
    //(mtx->m[0][2] * src->x) + (mtx->m[1][2] * src->y) + (mtx->m[2][2] * src->z) + mtx->m[3][2];
}

// Applies the transform matrix mtx to the vector src, putting the result in dest
void Matrix_MultVec3f_NoLoad(Vec3f* src, Vec3f* dest) {
    float w = 1;
    dest->x = src->x;
    dest->y = src->y;
    dest->z = src->z;
    mat_trans_single3_nodivw(dest->x, dest->y, dest->z, w);
//    dest->x = //fipr(mtx->m[0][0],mtx->m[1][0],mtx->m[2][0],mtx->m[3][0],src->x,src->y,src->z,1);//
  //  (mtx->m[0][0] * src->x) + (mtx->m[1][0] * src->y) + (mtx->m[2][0] * src->z) + mtx->m[3][0];
    //dest->y = //fipr(mtx->m[0][1],mtx->m[1][1],mtx->m[2][1],mtx->m[3][1],src->x,src->y,src->z,1);//
   // (mtx->m[0][1] * src->x) + (mtx->m[1][1] * src->y) + (mtx->m[2][1] * src->z) + mtx->m[3][1];
    //dest->z = //fipr(mtx->m[0][2],mtx->m[1][2],mtx->m[2][2],mtx->m[3][2],src->x,src->y,src->z,1);//
    //(mtx->m[0][2] * src->x) + (mtx->m[1][2] * src->y) + (mtx->m[2][2] * src->z) + mtx->m[3][2];
}

// Applies the transform matrix mtx to the vector src, putting the result in dest
void Matrix_MultVec3f(Matrix* mtx, Vec3f* src, Vec3f* dest) {
    shz_xmtrx_load_4x4_unaligned(mtx);
    float w = 1;
    dest->x = src->x;
    dest->y = src->y;
    dest->z = src->z;
    mat_trans_single3_nodivw(dest->x, dest->y, dest->z, w);
//    dest->x = //fipr(mtx->m[0][0],mtx->m[1][0],mtx->m[2][0],mtx->m[3][0],src->x,src->y,src->z,1);//
  //  (mtx->m[0][0] * src->x) + (mtx->m[1][0] * src->y) + (mtx->m[2][0] * src->z) + mtx->m[3][0];
    //dest->y = //fipr(mtx->m[0][1],mtx->m[1][1],mtx->m[2][1],mtx->m[3][1],src->x,src->y,src->z,1);//
   // (mtx->m[0][1] * src->x) + (mtx->m[1][1] * src->y) + (mtx->m[2][1] * src->z) + mtx->m[3][1];
    //dest->z = //fipr(mtx->m[0][2],mtx->m[1][2],mtx->m[2][2],mtx->m[3][2],src->x,src->y,src->z,1);//
    //(mtx->m[0][2] * src->x) + (mtx->m[1][2] * src->y) + (mtx->m[2][2] * src->z) + mtx->m[3][2];
}



void Matrix_MultVec3fNoTranslate_NoLoad(Vec3f* src, Vec3f* dest) {
    float w = 0;
    dest->x = src->x;
    dest->y = src->y;
    dest->z = src->z;
    mat_trans_single3_nodivw(dest->x, dest->y, dest->z, w);

//    dest->x = (mtx->m[0][0] * src->x) + (mtx->m[1][0] * src->y) + (mtx->m[2][0] * src->z);
  //  dest->y = (mtx->m[0][1] * src->x) + (mtx->m[1][1] * src->y) + (mtx->m[2][1] * src->z);
    //dest->z = (mtx->m[0][2] * src->x) + (mtx->m[1][2] * src->y) + (mtx->m[2][2] * src->z);
//    dest->x = fipr(mtx->m[0][0],mtx->m[1][0],mtx->m[2][0],0,src->x,src->y,src->z,0);//(mtx->m[0][0] * src->x) + (mtx->m[1][0] * src->y) + (mtx->m[2][0] * src->z) + mtx->m[3][0];
  //  dest->y = fipr(mtx->m[0][1],mtx->m[1][1],mtx->m[2][1],0,src->x,src->y,src->z,0);//(mtx->m[0][1] * src->x) + (mtx->m[1][1] * src->y) + (mtx->m[2][1] * src->z) + mtx->m[3][1];
    //dest->z = fipr(mtx->m[0][2],mtx->m[1][2],mtx->m[2][2],0,src->x,src->y,src->z,0);//(mtx->m[0][2] * src->x) + (mtx->m[1][2] * src->y) + (mtx->m[2][2] * src->z) + mtx->m[3][2];
}


// Applies the linear part of the transformation matrix mtx to the vector src, ignoring any translation that mtx might
// have. Puts the result in dest.
void Matrix_MultVec3fNoTranslate(Matrix* mtx, Vec3f* src, Vec3f* dest) {
shz_xmtrx_load_4x4_unaligned(mtx);
    float w = 0;
    dest->x = src->x;
    dest->y = src->y;
    dest->z = src->z;
    mat_trans_single3_nodivw(dest->x, dest->y, dest->z, w);

//    dest->x = (mtx->m[0][0] * src->x) + (mtx->m[1][0] * src->y) + (mtx->m[2][0] * src->z);
  //  dest->y = (mtx->m[0][1] * src->x) + (mtx->m[1][1] * src->y) + (mtx->m[2][1] * src->z);
    //dest->z = (mtx->m[0][2] * src->x) + (mtx->m[1][2] * src->y) + (mtx->m[2][2] * src->z);
//    dest->x = fipr(mtx->m[0][0],mtx->m[1][0],mtx->m[2][0],0,src->x,src->y,src->z,0);//(mtx->m[0][0] * src->x) + (mtx->m[1][0] * src->y) + (mtx->m[2][0] * src->z) + mtx->m[3][0];
  //  dest->y = fipr(mtx->m[0][1],mtx->m[1][1],mtx->m[2][1],0,src->x,src->y,src->z,0);//(mtx->m[0][1] * src->x) + (mtx->m[1][1] * src->y) + (mtx->m[2][1] * src->z) + mtx->m[3][1];
    //dest->z = fipr(mtx->m[0][2],mtx->m[1][2],mtx->m[2][2],0,src->x,src->y,src->z,0);//(mtx->m[0][2] * src->x) + (mtx->m[1][2] * src->y) + (mtx->m[2][2] * src->z) + mtx->m[3][2];
}

// Expresses the rotational part of the transform mtx as Tait-Bryan angles, in the yaw-pitch-roll (intrinsic YXZ)
// convention used in worldspace calculations
void Matrix_GetYPRAngles(Matrix* mtx, Vec3f* rot) {
    Matrix invYP;
    Vec3f origin = { 0.0f, 0.0f, 0.0f };
    Vec3f originP;
    Vec3f zHat = { 0.0f, 0.0f, 1.0f };
    Vec3f zHatP;
    Vec3f xHat = { 1.0f, 0.0f, 0.0f };
    Vec3f xHatP;

    Matrix_MultVec3fNoTranslate(mtx, &origin, &originP);
    Matrix_MultVec3fNoTranslate(mtx, &zHat, &zHatP);
    Matrix_MultVec3fNoTranslate(mtx, &xHat, &xHatP);
    zHatP.x -= originP.x;
    zHatP.y -= originP.y;
    zHatP.z -= originP.z;
    xHatP.x -= originP.x;
    xHatP.y -= originP.y;
    xHatP.z -= originP.z;
    rot->y = Math_Atan2F(zHatP.x, zHatP.z);
    rot->x = -Math_Atan2F(zHatP.y, sqrtf(SQ(zHatP.x) + SQ(zHatP.z)));
    Matrix_RotateX(&invYP, -rot->x, MTXF_NEW);
    Matrix_RotateY(&invYP, -rot->y, MTXF_APPLY);
    Matrix_MultVec3fNoTranslate(&invYP, &xHatP, &xHat);
    rot->x *= M_RTOD;
    rot->y *= M_RTOD;
    rot->z = Math_Atan2F(xHat.y, xHat.x) * M_RTOD;
}

// Expresses the rotational part of the transform mtx as Tait-Bryan angles, in the extrinsic XYZ convention used in
// modelspace calculations
void  Matrix_GetXYZAngles(Matrix* mtx, Vec3f* rot) {
    Matrix invYZ;
    Vec3f origin = { 0.0f, 0.0f, 0.0f };
    Vec3f originP;
    Vec3f xHat = { 1.0f, 0.0f, 0.0f };
    Vec3f xHatP;
    Vec3f yHat = { 0.0f, 1.0f, 0.0f };
    Vec3f yHatP;

    Matrix_MultVec3fNoTranslate(mtx, &origin, &originP);
    Matrix_MultVec3fNoTranslate(mtx, &xHat, &xHatP);
    Matrix_MultVec3fNoTranslate(mtx, &yHat, &yHatP);
    xHatP.x -= originP.x;
    xHatP.y -= originP.y;
    xHatP.z -= originP.z;
    yHatP.x -= originP.x;
    yHatP.y -= originP.y;
    yHatP.z -= originP.z;
    rot->z = Math_Atan2F(xHatP.y, xHatP.x);
    rot->y = -Math_Atan2F(xHatP.z, sqrtf(SQ(xHatP.x) + SQ(xHatP.y)));
    Matrix_RotateY(&invYZ, -rot->y, MTXF_NEW);
    Matrix_RotateZ(&invYZ, -rot->z, MTXF_APPLY);
    Matrix_MultVec3fNoTranslate(&invYZ, &yHatP, &yHat);
    rot->x = Math_Atan2F(yHat.z, yHat.y) * M_RTOD;
    rot->y *= M_RTOD;
    rot->z *= M_RTOD;
}

// Creates a look-at matrix from Eye, At, and Up in mtx (MTXF_NEW) or applies one to mtx (MTXF_APPLY).
// A look-at matrix is a rotation-translation matrix that maps y to Up, z to (At - Eye), and translates to Eye
void  Matrix_LookAt(Matrix* mtx, f32 xEye, f32 yEye, f32 zEye, f32 xAt, f32 yAt, f32 zAt, f32 xUp, f32 yUp, f32 zUp,
                   u8 mode) {
    Matrix lookAt;

    guLookAtF(lookAt.m, xEye, yEye, zEye, xAt, yAt, zAt, xUp, yUp, zUp);
    Matrix_Mult(mtx, &lookAt, mode);
}

// Converts the current Gfx matrix to a Mtx and sets it to the display list
void  Matrix_SetGfxMtx(Gfx** gfx) {
    Matrix_ToMtx(gGfxMtx);
    gSPMatrix((*gfx)++, gGfxMtx++, G_MTX_NOPUSH | G_MTX_LOAD | G_MTX_MODELVIEW);
}
