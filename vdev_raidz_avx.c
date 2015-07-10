#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "vdev_raidz.h"

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
                asm("VMOVDQU %[src], %%ymm0\n"
                    "VMOVDQU %%ymm0, %[p]"
                    : [p] "=m" (*p)
                    : [src] "m" (*src)
                    : "ymm0");
            }
            int remainder = ccount % 4;
            for(i = 0; i < remainder; i++, p++, src++) {
                *p = *src;
            }
        } else {
            ASSERT(ccount <= pcount);
            for (i = 0; i < ccount / 4; i++, src+=4, p+=4) {
                asm("VMOVDQU %[p], %%ymm0\n"
                    "VMOVDQU %[src], %%ymm1\n"
                    "VXORPS %%ymm1, %%ymm0, %%ymm0\n"
                    "VMOVDQU %%ymm0, %[p]"
                    : [p] "+m" (*p)
                    : [src] "m" (*src)
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
        asm("VMOVDQU %[src], %%ymm0\n"
            "VMOVDQU %%ymm0, %[dst]"
            : [dst] "=m" (*dst)
            : [src] "m" (*src)
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
            asm("VMOVDQU %[dst], %%ymm0\n"
                "VMOVDQU %[src], %%ymm1\n"
                "VXORPS %%ymm1, %%ymm0, %%ymm0\n"
                "VMOVDQU %%ymm0, %[dst]"
                : [dst] "+m" (*dst)
                : [src] "m" (*src)
                : "ymm0", "ymm1");
        }
        int remainder = count % 4;
        for(i = 0; i < remainder; i++, dst++, src++) {
            *dst ^= *src;
        }
    }

    return (1 << VDEV_RAIDZ_P);
}

void
vdev_raidz_generate_parity_pq_avx(raidz_map_t *rm)
{
    uint64_t *p, *q, *src, pcnt, ccnt, mask, i;
    int c;

    pcnt = rm->rm_col[VDEV_RAIDZ_P].rc_size / sizeof (src[0]);
    ASSERT(rm->rm_col[VDEV_RAIDZ_P].rc_size ==
    rm->rm_col[VDEV_RAIDZ_Q].rc_size);

    for (c = rm->rm_firstdatacol; c < rm->rm_cols; c++) {
        src = rm->rm_col[c].rc_data;
        p = rm->rm_col[VDEV_RAIDZ_P].rc_data;
        q = rm->rm_col[VDEV_RAIDZ_Q].rc_data;

        ccnt = rm->rm_col[c].rc_size / sizeof (src[0]);

        if (c == rm->rm_firstdatacol) {
            ASSERT(ccnt == pcnt || ccnt == 0);
            for (i = 0; i < ccnt / 4; i++, src+=4, p+=4, q+=4) {
                asm("VMOVDQU %[src], %%ymm0\n"
                    "VMOVDQU %%ymm0, %[p]\n"
                    "VMOVDQU %%ymm0, %[q]"
                    : [p] "=m" (*p), [q] "=m" (*q)
                    : [src] "m" (*src)
                    : "ymm0");
            }
            int remainder = ccnt % 4;
            for(int j = 0; j < remainder; j++, p++, src++, q++) {
                *p = *src;
                *q = *src;
            }
            for (; i < pcnt / 4; i++, src+=4, p+=4, q+=4) {
                asm("VXORPS %%ymm0, %%ymm0, %%ymm0\n"
                    "VMOVDQU %%ymm0, %[p]\n"
                    "VMOVDQU %%ymm0, %[q]"
                    : [p] "=m" (*p), [q] "=m" (*q)
                    :
                    : "ymm0");
            }
            remainder = (pcnt - ccnt) % 4;
            for(i = 0; i < remainder; i++, p++, q++) {
                *p = 0;
                *q = 0;
            }
        } else {
            ASSERT(ccnt <= pcnt);

            /*
            * Apply the algorithm described above by multiplying
            * the previous result and adding in the new value.
            */
            for (i = 0; i < ccnt; i++, src++, p++, q++) {
                *p ^= *src;

                VDEV_RAIDZ_64MUL_2(*q, mask);
                *q ^= *src;
            }

            /*
            * Treat short columns as though they are full of 0s.
            * Note that there's therefore nothing needed for P.
            */
            for (; i < pcnt; i++, q++) {
                VDEV_RAIDZ_64MUL_2(*q, mask);
            }
        }
    }
}

int
vdev_raidz_reconstruct_pq_avx(raidz_map_t *rm, int *tgts, int ntgts)
{
    uint8_t *p, *q, *pxy, *qxy, *xd, *yd, tmp, a, b, aexp, bexp;
    void *pdata, *qdata;
    uint64_t xsize, ysize, i;
    int x = tgts[0];
    int y = tgts[1];

    ASSERT(ntgts == 2);
    ASSERT(x < y);
    ASSERT(x >= rm->rm_firstdatacol);
    ASSERT(y < rm->rm_cols);

    ASSERT(rm->rm_col[x].rc_size >= rm->rm_col[y].rc_size);

    /*
    * Move the parity data aside -- we're going to compute parity as
    * though columns x and y were full of zeros -- Pxy and Qxy. We want to
    * reuse the parity generation mechanism without trashing the actual
    * parity so we make those columns appear to be full of zeros by
    * setting their lengths to zero.
    */
    pdata = rm->rm_col[VDEV_RAIDZ_P].rc_data;
    qdata = rm->rm_col[VDEV_RAIDZ_Q].rc_data;
    xsize = rm->rm_col[x].rc_size;
    ysize = rm->rm_col[y].rc_size;

    rm->rm_col[VDEV_RAIDZ_P].rc_data =
    malloc(rm->rm_col[VDEV_RAIDZ_P].rc_size);
    rm->rm_col[VDEV_RAIDZ_Q].rc_data =
    malloc(rm->rm_col[VDEV_RAIDZ_Q].rc_size);
    rm->rm_col[x].rc_size = 0;
    rm->rm_col[y].rc_size = 0;

    vdev_raidz_generate_parity_pq(rm);

    rm->rm_col[x].rc_size = xsize;
    rm->rm_col[y].rc_size = ysize;

    p = pdata;
    q = qdata;
    pxy = rm->rm_col[VDEV_RAIDZ_P].rc_data;
    qxy = rm->rm_col[VDEV_RAIDZ_Q].rc_data;
    xd = rm->rm_col[x].rc_data;
    yd = rm->rm_col[y].rc_data;

    /*
    * We now have:
    *  Pxy = P + D_x + D_y
    *  Qxy = Q + 2^(ndevs - 1 - x) * D_x + 2^(ndevs - 1 - y) * D_y
    *
    * We can then solve for D_x:
    *  D_x = A * (P + Pxy) + B * (Q + Qxy)
    * where
    *  A = 2^(x - y) * (2^(x - y) + 1)^-1
    *  B = 2^(ndevs - 1 - x) * (2^(x - y) + 1)^-1
    *
    * With D_x in hand, we can easily solve for D_y:
    *  D_y = P + Pxy + D_x
    */

    a = vdev_raidz_pow2[255 + x - y];
    b = vdev_raidz_pow2[255 - (rm->rm_cols - 1 - x)];
    tmp = 255 - vdev_raidz_log2[a ^ 1];

    aexp = vdev_raidz_log2[vdev_raidz_exp2(a, tmp)];
    bexp = vdev_raidz_log2[vdev_raidz_exp2(b, tmp)];

    for (i = 0; i < xsize; i++, p++, q++, pxy++, qxy++, xd++, yd++) {
        *xd = vdev_raidz_exp2(*p ^ *pxy, aexp) ^
        vdev_raidz_exp2(*q ^ *qxy, bexp);

        if (i < ysize)
            *yd = *p ^ *pxy ^ *xd;
    }

    free(rm->rm_col[VDEV_RAIDZ_P].rc_data);
    free(rm->rm_col[VDEV_RAIDZ_Q].rc_data);

    /*
    * Restore the saved parity data.
    */
    rm->rm_col[VDEV_RAIDZ_P].rc_data = pdata;
    rm->rm_col[VDEV_RAIDZ_Q].rc_data = qdata;

    return ((1 << VDEV_RAIDZ_P) | (1 << VDEV_RAIDZ_Q));
}
