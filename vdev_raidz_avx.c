#include <stdint.h>
#include <stdio.h>

#include "vdev_raidz.h"

void
vdev_raidz_generate_parity_p_avx_v2(raidz_map_t *rm)
{
    uint64_t *p, *src, pcount, ccount, i;
    int c;

    pcount = rm->rm_col[VDEV_RAIDZ_P].rc_size / sizeof (src[0]);

    for (c = rm->rm_firstdatacol; c < rm->rm_cols; c++) {
        src = rm->rm_col[c].rc_data;
        p = rm->rm_col[VDEV_RAIDZ_P].rc_data;
        ccount = rm->rm_col[c].rc_size / sizeof (src[0]);

        if (c == rm->rm_firstdatacol) {
            ASSERT(ccount == pcount);
            for (i = 0; i < ccount / 4; i++, src+=4, p+=4) {
                asm("VMOVDQU %1, %%ymm0\n"
                    "VMOVDQU %%ymm0, %0"
                    : "=m" (*p)
                    : "m" (*src)
                    : "ymm0");
            }
            int remainder = ccount % 4;
            if (remainder >= 2) {
                asm("MOVDQU %1, %%xmm0\n"
                    "MOVDQU %%xmm0, %0"
                    : "=m" (*p)
                    : "m" (*src)
                    : "xmm0");
                p += 2;
                src += 2;
                remainder -= 2;
            }
            if (remainder) {
                *p = *src;
            }
        } else {
            ASSERT(ccount <= pcount);
            for (i = 0; i < ccount / 4; i++, src+=4, p+=4) {
                asm("VMOVDQU %0, %%ymm0\n"
                    "VMOVDQU %1, %%ymm1\n"
                    "VXORPS %%ymm1, %%ymm0, %%ymm0\n"
                    "VMOVDQU %%ymm0, %0"
                    : "+m" (*p)
                    : "m" (*src)
                    : "ymm0", "ymm1");
            }
            int remainder = ccount % 4;
            if (remainder >= 2) {
                asm("MOVDQU %0, %%xmm0\n"
                    "MOVDQU %1, %%xmm1\n"
                    "PXOR %%xmm1, %%xmm0\n"
                    "MOVDQU %%xmm0, %0"
                    : "+m" (*p)
                    : "m" (*src)
                    : "xmm0", "xmm1");
                p += 2;
                src += 2;
                remainder -= 2;
            }
            if (remainder) {
                *p ^= *src;
            }
        }
    }
}

int
vdev_raidz_reconstruct_p_avx_v2(raidz_map_t *rm, int *tgts, int ntgts)
{
    uint64_t *dst, *src, xcount, ccount, count, i;
    int x = tgts[0];
    int c;

    ASSERT(ntgts == 1);
    ASSERT(x >= rm->rm_firstdatacol);
    ASSERT(x < rm->rm_cols);

    xcount = rm->rm_col[x].rc_size / sizeof (src[0]);
    ASSERT(xcount <= rm->rm_col[VDEV_RAIDZ_P].rc_size / sizeof (src[0]));
    ASSERT(xcount > 0);

    src = rm->rm_col[VDEV_RAIDZ_P].rc_data;
    dst = rm->rm_col[x].rc_data;
    for (i = 0; i < xcount / 4; i++, src+=4, dst+=4) {
        asm("VMOVDQU %1, %%ymm0\n"
            "VMOVDQU %%ymm0, %0"
            : "=m" (*dst)
            : "m" (*src)
            : "ymm0");
    }
    int remainder = xcount % 4;
    if (remainder >= 2) {
        asm("MOVDQU %1, %%xmm0\n"
            "MOVDQU %%xmm0, %0"
            : "=m" (*dst)
            : "m" (*src)
            : "xmm0");
        dst += 2;
        src += 2;
        remainder -= 2;
    }
    if (remainder) {
        *dst = *src;
    }

    for (c = rm->rm_firstdatacol; c < rm->rm_cols; c++) {
        src = rm->rm_col[c].rc_data;
        dst = rm->rm_col[x].rc_data;

        if (c == x)
            continue;

        ccount = rm->rm_col[c].rc_size / sizeof (src[0]);
        count = MIN(ccount, xcount);

        for (i = 0; i < count / 4; i++, src+=4, dst+=4) {
            asm("VMOVDQU %0, %%ymm0\n"
                "VMOVDQU %1, %%ymm1\n"
                "VXORPS %%ymm1, %%ymm0, %%ymm0\n"
                "VMOVDQU %%ymm0, %0"
                : "+m" (*dst)
                : "m" (*src)
                : "ymm0", "ymm1");
        }
        int remainder = count % 4;
        if (remainder >= 2) {
            asm("MOVDQU %0, %%xmm0\n"
                "MOVDQU %1, %%xmm1\n"
                "PXOR %%xmm1, %%xmm0\n"
                "MOVDQU %%xmm0, %0"
                : "+m" (*dst)
                : "m" (*src)
                : "xmm0", "xmm1");
            dst += 2;
            src += 2;
            remainder -= 2;
        }
        if (remainder) {
            *dst ^= *src;
        }
    }

    return (1 << VDEV_RAIDZ_P);
}

void
vdev_raidz_generate_parity_p_avx(raidz_map_t *rm)
{
    uint64_t *p, *src, pcount, ccount, i;
    int c;

    pcount = rm->rm_col[VDEV_RAIDZ_P].rc_size / sizeof (src[0]);

    for (c = rm->rm_firstdatacol; c < rm->rm_cols; c++) {
        src = rm->rm_col[c].rc_data;
        p = rm->rm_col[VDEV_RAIDZ_P].rc_data;
        ccount = rm->rm_col[c].rc_size / sizeof (src[0]);

        if (c == rm->rm_firstdatacol) {
            ASSERT(ccount == pcount);
            for (i = 0; i < ccount / 4; i++, src+=4, p+=4) {
                asm("VMOVDQU %1, %%ymm0\n"
                    "VMOVDQU %%ymm0, %0"
                    : "=m" (*p)
                    : "m" (*src)
                    : "ymm0");
            }
            int remainder = ccount % 4;
            for(i = 0; i < remainder; i++, p++, src++) {
                *p = *src;
            }
        } else {
            ASSERT(ccount <= pcount);
            for (i = 0; i < ccount / 4; i++, src+=4, p+=4) {
                asm("VMOVDQU %0, %%ymm0\n"
                    "VMOVDQU %1, %%ymm1\n"
                    "VXORPS %%ymm1, %%ymm0, %%ymm0\n"
                    "VMOVDQU %%ymm0, %0"
                    : "+m" (*p)
                    : "m" (*src)
                    : "ymm0", "ymm1");
            }
            int remainder = ccount % 4;
            for(i = 0; i < remainder; i++, p++, src++) {
                *p ^= *src;
            }
        }
    }
}

int
vdev_raidz_reconstruct_p_avx(raidz_map_t *rm, int *tgts, int ntgts)
{
    uint64_t *dst, *src, xcount, ccount, count, i;
    int x = tgts[0];
    int c;

    ASSERT(ntgts == 1);
    ASSERT(x >= rm->rm_firstdatacol);
    ASSERT(x < rm->rm_cols);

    xcount = rm->rm_col[x].rc_size / sizeof (src[0]);
    ASSERT(xcount <= rm->rm_col[VDEV_RAIDZ_P].rc_size / sizeof (src[0]));
    ASSERT(xcount > 0);

    src = rm->rm_col[VDEV_RAIDZ_P].rc_data;
    dst = rm->rm_col[x].rc_data;
    for (i = 0; i < xcount / 4; i++, src+=4, dst+=4) {
        asm("VMOVDQU %1, %%ymm0\n"
            "VMOVDQU %%ymm0, %0"
            : "=m" (*dst)
            : "m" (*src)
            : "ymm0");
    }
    int remainder = xcount % 4;
        for(i = 0; i < remainder; i++, dst++, src++) {
            *dst = *src;
        }

    for (c = rm->rm_firstdatacol; c < rm->rm_cols; c++) {
        src = rm->rm_col[c].rc_data;
        dst = rm->rm_col[x].rc_data;

        if (c == x)
            continue;

        ccount = rm->rm_col[c].rc_size / sizeof (src[0]);
        count = MIN(ccount, xcount);

        for (i = 0; i < count / 4; i++, src+=4, dst+=4) {
            asm("VMOVDQU %0, %%ymm0\n"
                "VMOVDQU %1, %%ymm1\n"
                "VXORPS %%ymm1, %%ymm0, %%ymm0\n"
                "VMOVDQU %%ymm0, %0"
                : "+m" (*dst)
                : "m" (*src)
                : "ymm0", "ymm1");
        }
        int remainder = count % 4;
        for(i = 0; i < remainder; i++, dst++, src++) {
            *dst ^= *src;
        }
    }

    return (1 << VDEV_RAIDZ_P);
}
