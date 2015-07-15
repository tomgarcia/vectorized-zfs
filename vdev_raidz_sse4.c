/*
 * CDDL HEADER START
 *
 * The contents of this file are subject to the terms of the
 * Common Development and Distribution License (the "License").
 * You may not use this file except in compliance with the License.
 *
 * You can obtain a copy of the license at usr/src/OPENSOLARIS.LICENSE
 * or http://www.opensolaris.org/os/licensing.
 * See the License for the specific language governing permissions
 * and limitations under the License.
 *
 * When distributing Covered Code, include this CDDL HEADER in each
 * file and include the License file at usr/src/OPENSOLARIS.LICENSE.
 * If applicable, add the following below this CDDL HEADER, with the
 * fields enclosed by brackets "[]" replaced with your own identifying
 * information: Portions Copyright [yyyy] [name of copyright owner]
 *
 * CDDL HEADER END
 */

/*
 * Copyright (c) 2005, 2010, Oracle and/or its affiliates. All rights reserved.
 * Copyright (c) 2012, 2014 by Delphix. All rights reserved.
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "vdev_raidz.h"

void
vdev_raidz_generate_parity_p_sse4(raidz_map_t *rm)
{
    uint64_t *p, *src, pcount, ccount, i;
    int c, remainder;

    pcount = rm->rm_col[VDEV_RAIDZ_P].rc_size / sizeof (src[0]);

    for (c = rm->rm_firstdatacol; c < rm->rm_cols; c++) {
        src = rm->rm_col[c].rc_data;
        p = rm->rm_col[VDEV_RAIDZ_P].rc_data;
        ccount = rm->rm_col[c].rc_size / sizeof (src[0]);

        if (c == rm->rm_firstdatacol) {
            ASSERT(ccount == pcount);
            for (i = 0; i < ccount / 8; i++, src+=8, p+=8) {
                asm("MOVDQU (%[src]), %%xmm0\n"
                    "MOVDQU %%xmm0, (%[p])\n"
                    "MOVDQU 16(%[src]), %%xmm1\n"
                    "MOVDQU %%xmm1, 16(%[p])\n"
                    "MOVDQU 32(%[src]), %%xmm2\n"
                    "MOVDQU %%xmm2, 32(%[p])\n"
                    "MOVDQU 48(%[src]), %%xmm3\n"
                    "MOVDQU %%xmm3, 48(%[p])\n"
                    :
                    : [p] "r" (p), [src] "r" (src)
                    : "xmm0", "xmm1", "xmm2", "xmm3", "memory");
            }
            remainder = ccount % 8;
            for(i = 0; i < remainder / 2; i++, p+=2, src+=2) {
                asm("MOVDQU (%[src]), %%xmm0\n"
                    "MOVDQU %%xmm0, (%[p])\n"
                    :
                    : [p] "r" (p), [src] "r" (src)
                    : "xmm0", "memory");
            }
            remainder %= 2;
            if (remainder > 0) {
                *p = *src;
            }
        } else {
            ASSERT(ccount <= pcount);
            for (i = 0; i < ccount / 8; i++, src+=8, p+=8) {
                asm("MOVDQU (%[src]), %%xmm0\n"
                    "MOVDQU (%[p]), %%xmm1\n"
                    "PXOR %%xmm0, %%xmm1\n"
                    "MOVDQU %%xmm1, (%[p])\n"
                    "MOVDQU 16(%[src]), %%xmm2\n"
                    "MOVDQU 16(%[p]), %%xmm3\n"
                    "PXOR %%xmm2, %%xmm3\n"
                    "MOVDQU %%xmm3, 16(%[p])\n"
                    "MOVDQU 32(%[src]), %%xmm4\n"
                    "MOVDQU 32(%[p]), %%xmm5\n"
                    "PXOR %%xmm4, %%xmm5\n"
                    "MOVDQU %%xmm5, 32(%[p])\n"
                    "MOVDQU 48(%[src]), %%xmm6\n"
                    "MOVDQU 48(%[p]), %%xmm7\n"
                    "PXOR %%xmm6, %%xmm7\n"
                    "MOVDQU %%xmm7, 48(%[p])\n"
                    :
                    : [p] "r" (p), [src] "r" (src)
                    : "xmm0", "xmm1", "xmm2", "xmm3",
                      "xmm4", "xmm5", "xmm6", "xmm7", "memory");
            }
            remainder = ccount % 8;
            for(i = 0; i < remainder / 2; i++, p+=2, src+=2) {
                asm("MOVDQU (%[src]), %%xmm0\n"
                    "MOVDQU (%[p]), %%xmm1\n"
                    "PXOR %%xmm0, %%xmm1\n"
                    "MOVDQU %%xmm1, (%[p])\n"
                    :
                    : [p] "r" (p), [src] "r" (src)
                    : "xmm0", "xmm1", "memory");
            }
            remainder %= 2;
            if (remainder > 0) {
                *p ^= *src;
            }
        }
    }
}

int
vdev_raidz_reconstruct_p_sse4(raidz_map_t *rm, int *tgts, int ntgts)
{
    uint64_t *dst, *src, xcount, ccount, count, i;
    int x = tgts[0];
    int c, remainder;

    ASSERT(ntgts == 1);
    ASSERT(x >= rm->rm_firstdatacol);
    ASSERT(x < rm->rm_cols);

    xcount = rm->rm_col[x].rc_size / sizeof (src[0]);
    ASSERT(xcount <= rm->rm_col[VDEV_RAIDZ_P].rc_size / sizeof (src[0]));
    ASSERT(xcount > 0);

    src = rm->rm_col[VDEV_RAIDZ_P].rc_data;
    dst = rm->rm_col[x].rc_data;
    for (i = 0; i < xcount / 8; i++, src+=8, dst+=8) {
        asm("MOVDQU (%[src]), %%xmm0\n"
            "MOVDQU %%xmm0, (%[dst])\n"
            "MOVDQU 16(%[src]), %%xmm1\n"
            "MOVDQU %%xmm1, 16(%[dst])\n"
            "MOVDQU 32(%[src]), %%xmm2\n"
            "MOVDQU %%xmm2, 32(%[dst])\n"
            "MOVDQU 48(%[src]), %%xmm3\n"
            "MOVDQU %%xmm3, 48(%[dst])"
            :
            : [dst] "r" (dst), [src] "r" (src)
            : "xmm0", "xmm1", "xmm2", "xmm3", "memory");
    }
    remainder = xcount % 8;
    for (i = 0; i < remainder / 2; i++, src+=2, dst+=2) {
        asm("MOVDQU (%[src]), %%xmm0\n"
            "MOVDQU %%xmm0, (%[dst])"
            :
            : [dst] "r" (dst), [src] "r" (src)
            : "xmm0", "memory");
    }
    remainder %= 2;
    if (remainder > 0) {
        *dst = *src;
    }

    for (c = rm->rm_firstdatacol; c < rm->rm_cols; c++) {
        src = rm->rm_col[c].rc_data;
        dst = rm->rm_col[x].rc_data;

        if (c == x)
            continue;

        ccount = rm->rm_col[c].rc_size / sizeof (src[0]);
        count = MIN(ccount, xcount);

        for (i = 0; i < count / 8; i++, src+=8, dst+=8) {
            asm("MOVDQU (%[src]), %%xmm0\n"
                "MOVDQU (%[dst]), %%xmm1\n"
                "PXOR %%xmm0, %%xmm1\n"
                "MOVDQU %%xmm1, (%[dst])\n"
                "MOVDQU 16(%[src]), %%xmm2\n"
                "MOVDQU 16(%[dst]), %%xmm3\n"
                "PXOR %%xmm2, %%xmm3\n"
                "MOVDQU %%xmm3, 16(%[dst])\n"
                "MOVDQU 32(%[src]), %%xmm4\n"
                "MOVDQU 32(%[dst]), %%xmm5\n"
                "PXOR %%xmm4, %%xmm5\n"
                "MOVDQU %%xmm5, 32(%[dst])\n"
                "MOVDQU 48(%[src]), %%xmm6\n"
                "MOVDQU 48(%[dst]), %%xmm7\n"
                "PXOR %%xmm6, %%xmm7\n"
                "MOVDQU %%xmm7, 48(%[dst])\n"
                :
                : [dst] "r" (dst), [src] "r" (src)
                : "xmm0", "xmm1", "xmm2", "xmm3",
                  "xmm4", "xmm5", "xmm6", "xmm7", "memory");
        }
        remainder = count % 8;
        for (i = 0; i < remainder / 2; i++, src+=2, dst+=2) {
            asm("MOVDQU (%[src]), %%xmm0\n"
                "MOVDQU (%[dst]), %%xmm1\n"
                "PXOR %%xmm0, %%xmm1\n"
                "MOVDQU %%xmm1, (%[dst])"
                :
                : [dst] "r" (dst), [src] "r" (src)
                : "xmm0", "xmm1");
        }
        remainder %= 2;
        if (remainder > 0) {
            *dst ^= *src;
        }
    }

    return (1 << VDEV_RAIDZ_P);
}

void
vdev_raidz_generate_parity_pq_sse4(raidz_map_t *rm)
{
    uint64_t *p, *q, *src, pcnt, ccnt, mask, i;
    int c, remainder;

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
            if(ccnt != 0) {
                for (i = 0; i < ccnt / 8; i++, src+=8, p+=8, q+=8) {
                    asm("MOVDQU (%[src]), %%xmm0\n"
                        "MOVDQU %%xmm0, (%[p])\n"
                        "MOVDQU %%xmm0, (%[q])\n"
                        "MOVDQU 16(%[src]), %%xmm1\n"
                        "MOVDQU %%xmm1, 16(%[p])\n"
                        "MOVDQU %%xmm1, 16(%[q])\n"
                        "MOVDQU 32(%[src]), %%xmm2\n"
                        "MOVDQU %%xmm2, 32(%[p])\n"
                        "MOVDQU %%xmm2, 32(%[q])\n"
                        "MOVDQU 48(%[src]), %%xmm3\n"
                        "MOVDQU %%xmm3, 48(%[p])\n"
                        "MOVDQU %%xmm3, 48(%[q])\n"
                        :
                        : [p] "r" (p), [q] "r" (q), [src] "r" (src)
                        : "xmm0", "xmm1", "xmm2", "xmm3", "memory");
                }
                remainder = ccnt % 8;
                for (i = 0; i < remainder / 2; i++, src+=2, p+=2, q+=2) {
                    asm("MOVDQU (%[src]), %%xmm0\n"
                        "MOVDQU %%xmm0, (%[p])\n"
                        "MOVDQU %%xmm0, (%[q])"
                        :
                        : [p] "r" (p), [q] "r" (q), [src] "r" (src)
                        : "xmm0", "memory");
                }
                remainder %= 2;
                if (remainder > 0) {
                    *p = *src;
                    *q = *src;
                }
            } else {
                for (i = 0; i < pcnt / 8; i++, p+=8, q+=8) {
                    asm("PXOR %%xmm0, %%xmm0\n"
                        "MOVDQU %%xmm0, (%[p])\n"
                        "MOVDQU %%xmm0, (%[q])\n"
                        "MOVDQU %%xmm0, 16(%[p])\n"
                        "MOVDQU %%xmm0, 16(%[q])\n"
                        "MOVDQU %%xmm0, 32(%[p])\n"
                        "MOVDQU %%xmm0, 32(%[q])\n"
                        "MOVDQU %%xmm0, 48(%[p])\n"
                        "MOVDQU %%xmm0, 48(%[q])\n"
                        :
                        : [p] "r" (p), [q] "r" (q)
                        : "xmm0", "memory");
                }
                remainder = pcnt % 8;
                for (i = 0; i < remainder / 2; i++, p+=2, q+=2) {
                    asm("PXOR %%xmm0, %%xmm0\n"
                        "MOVDQU %%xmm0, (%[p])\n"
                        "MOVDQU %%xmm0, (%[q])"
                        :
                        : [p] "r" (p), [q] "r" (q)
                        : "xmm0", "memory");
                }
                remainder %= 2;
                if (remainder > 0) {
                    *p = 0;
                    *q = 0;
                }
            }
        } else {
            ASSERT(ccnt <= pcnt);

            /*
            * Apply the algorithm described above by multiplying
            * the previous result and adding in the new value.
            */
            for (i = 0; i < ccnt / 8; i++, src+=8, p+=8, q+=8) {
                asm("MOVDQU (%[src]), %%xmm0\n"
                    "MOVDQU (%[p]), %%xmm1\n"
                    "PXOR %%xmm0, %%xmm1\n"
                    "MOVDQU %%xmm1, (%[p])\n"
                    "MOVDQU (%[q]), %%xmm1\n"
                    "PXOR %%xmm3, %%xmm3\n"
                    "PCMPGTB %%xmm1, %%xmm3\n"
                    "MOVQ $0x1d1d1d1d1d1d1d1d, %%rax\n"
                    "PINSRQ $0, %%rax, %%xmm6\n"
                    "PINSRQ $1, %%rax, %%xmm6\n"
                    "PAND %%xmm6, %%xmm3\n"
                    "PADDB %%xmm1, %%xmm1\n"
                    "PXOR %%xmm3, %%xmm1\n"
                    "PXOR %%xmm0, %%xmm1\n"
                    "MOVDQU %%xmm1, (%[q])\n"
                    "MOVDQU 16(%[src]), %%xmm0\n"
                    "MOVDQU 16(%[p]), %%xmm1\n"
                    "PXOR %%xmm0, %%xmm1\n"
                    "MOVDQU %%xmm1, 16(%[p])\n"
                    "MOVDQU 16(%[q]), %%xmm1\n"
                    "PXOR %%xmm3, %%xmm3\n"
                    "PCMPGTB %%xmm1, %%xmm3\n"
                    "PAND %%xmm6, %%xmm3\n"
                    "PADDB %%xmm1, %%xmm1\n"
                    "PXOR %%xmm3, %%xmm1\n"
                    "PXOR %%xmm0, %%xmm1\n"
                    "MOVDQU %%xmm1, 16(%[q])\n"
                    "MOVDQU 32(%[src]), %%xmm0\n"
                    "MOVDQU 32(%[p]), %%xmm1\n"
                    "PXOR %%xmm0, %%xmm1\n"
                    "MOVDQU %%xmm1, 32(%[p])\n"
                    "MOVDQU 32(%[q]), %%xmm1\n"
                    "PXOR %%xmm3, %%xmm3\n"
                    "PCMPGTB %%xmm1, %%xmm3\n"
                    "PAND %%xmm6, %%xmm3\n"
                    "PADDB %%xmm1, %%xmm1\n"
                    "PXOR %%xmm3, %%xmm1\n"
                    "PXOR %%xmm0, %%xmm1\n"
                    "MOVDQU %%xmm1, 32(%[q])\n"
                    "MOVDQU 48(%[src]), %%xmm0\n"
                    "MOVDQU 48(%[p]), %%xmm1\n"
                    "PXOR %%xmm0, %%xmm1\n"
                    "MOVDQU %%xmm1, 48(%[p])\n"
                    "MOVDQU 48(%[q]), %%xmm1\n"
                    "PXOR %%xmm3, %%xmm3\n"
                    "PCMPGTB %%xmm1, %%xmm3\n"
                    "PAND %%xmm6, %%xmm3\n"
                    "PADDB %%xmm1, %%xmm1\n"
                    "PXOR %%xmm3, %%xmm1\n"
                    "PXOR %%xmm0, %%xmm1\n"
                    "MOVDQU %%xmm1, 48(%[q])\n"
                    :
                    : [p] "r" (p), [q] "r" (q), [src] "r" (src)
                    : "rax", "xmm0", "xmm1", "xmm2",
                      "xmm3", "xmm5", "xmm6", "memory");
            }
            remainder = ccnt % 8;
            for (i = 0; i < remainder / 2; i++, src+=2, p+=2, q+=2) {
                asm("MOVDQU (%[src]), %%xmm0\n"
                    "MOVDQU (%[p]), %%xmm1\n"
                    "PXOR %%xmm0, %%xmm1\n"
                    "MOVDQU %%xmm1, (%[p])\n"
                    "MOVDQU (%[q]), %%xmm1\n"
                    "PXOR %%xmm3, %%xmm3\n"
                    "PCMPGTB %%xmm1, %%xmm3\n"
                    "MOVQ $0x1d1d1d1d1d1d1d1d, %%rax\n"
                    "PINSRQ $0, %%rax, %%xmm4\n"
                    "PINSRQ $1, %%rax, %%xmm4\n"
                    "PAND %%xmm4, %%xmm3\n"
                    "PADDB %%xmm1, %%xmm1\n"
                    "PXOR %%xmm3, %%xmm1\n"
                    "PXOR %%xmm0, %%xmm1\n"
                    "MOVDQU %%xmm1, (%[q])"
                    :
                    : [p] "r" (p), [q] "r" (q), [src] "r" (src)
                    : "rax", "xmm0", "xmm1", "xmm2",
                      "xmm3", "xmm4", "xmm5", "memory");
            }
            remainder %= 2;
            if (remainder > 0) {
                *p ^= *src;
                VDEV_RAIDZ_64MUL_2(*q, mask);
                *q ^= *src;
            }

            /*
            * Treat short columns as though they are full of 0s.
            * Note that there's therefore nothing needed for P.
            */
            for (i = 0; i < (pcnt - ccnt); i++, q++) {
                VDEV_RAIDZ_64MUL_2(*q, mask);
            }
        }
    }
}

int
vdev_raidz_reconstruct_pq_sse4(raidz_map_t *rm, int *tgts, int ntgts)
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

    vdev_raidz_generate_parity_pq_sse4(rm);

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


int
vdev_raidz_reconstruct_q_sse4(raidz_map_t *rm, int *tgts, int ntgts)
{
    uint64_t *dst, *src, xcount, ccount, count, mask, i;
    uint8_t *b;
    int x = tgts[0];
    int c, j, exp, remainder;

    ASSERT(ntgts == 1);

    xcount = rm->rm_col[x].rc_size / sizeof (src[0]);
    ASSERT(xcount <= rm->rm_col[VDEV_RAIDZ_Q].rc_size / sizeof (src[0]));

    for (c = rm->rm_firstdatacol; c < rm->rm_cols; c++) {
        src = rm->rm_col[c].rc_data;
        dst = rm->rm_col[x].rc_data;

        if (c == x)
            ccount = 0;
        else
            ccount = rm->rm_col[c].rc_size / sizeof (src[0]);

        count = MIN(ccount, xcount);

        if (c == rm->rm_firstdatacol) {
            for (i = 0; i < count / 16; i++, src+=16, dst+=16) {
                asm("VMOVDQU (%[src]), %%ymm0\n"
                    "VMOVDQU %%ymm0, (%[dst])\n"
                    "VMOVDQU 32(%[src]), %%ymm1\n"
                    "VMOVDQU %%ymm1, 32(%[dst])\n"
                    "VMOVDQU 64(%[src]), %%ymm2\n"
                    "VMOVDQU %%ymm2, 64(%[dst])\n"
                    "VMOVDQU 96(%[src]), %%ymm3\n"
                    "VMOVDQU %%ymm3, 96(%[dst])\n"
                    :
                    : [dst] "r" (dst), [src] "r" (src)
                    : "ymm0", "ymm1", "ymm2", "ymm3", "memory");
            }
            remainder = count % 16;
            for (i = 0; i < remainder / 4; i++, src+=4, dst+=4) {
                asm("VMOVDQU (%[src]), %%ymm0\n"
                    "VMOVDQU %%ymm0, (%[dst])"
                    :
                    : [dst] "r" (dst), [src] "r" (src)
                    : "ymm0", "memory");
            }
            remainder %= 4;
            for(i = 0; i < remainder; i++, src++, dst++) {
                *dst = *src;
            }
            for (i = 0; i < (xcount - count) / 16; i++, dst+=16) {
                asm("VXORPS %%ymm0, %%ymm0, %%ymm0\n"
                    "VMOVDQU %%ymm0, (%[dst])\n"
                    "VMOVDQU %%ymm0, 32(%[dst])\n"
                    "VMOVDQU %%ymm0, 64(%[dst])\n"
                    "VMOVDQU %%ymm0, 96(%[dst])\n"
                    :
                    : [dst] "r" (dst)
                    : "ymm0", "memory");
            }
            remainder = (xcount - count) % 16;
            for (i = 0; i < remainder / 4; i++, dst+=4) {
                asm("VXORPS %%ymm0, %%ymm0, %%ymm0\n"
                    "VMOVDQU %%ymm0, (%[dst])"
                    :
                    : [dst] "r" (dst)
                    : "ymm0", "memory");
            }
            remainder %= 4;
            for(i = 0; i < remainder; i++, dst++) {
                *dst = 0;
            }
        } else {
            for (i = 0; i < count / 16; i++, src+=16, dst+=16) {
                asm("VMOVDQU (%[src]), %%ymm0\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU (%[dst]), %%ymm1\n"
                    "VEXTRACTF128 $1, %%ymm1, %%xmm2\n"
                    "VXORPS %%xmm3, %%xmm3, %%xmm3\n"
                    "VPCMPGTB %%xmm1, %%xmm3, %%xmm4\n"
                    "VPCMPGTB %%xmm2, %%xmm3, %%xmm5\n"
                    "MOVQ $0x1d1d1d1d1d1d1d1d, %%rax\n"
                    "VPINSRQ $0, %%rax, %%xmm6, %%xmm6\n"
                    "VPINSRQ $1, %%rax, %%xmm6, %%xmm6\n"
                    "VPAND %%xmm6, %%xmm4, %%xmm4\n"
                    "VPAND %%xmm6, %%xmm5, %%xmm5\n"
                    "VINSERTF128 $1, %%xmm5, %%ymm4, %%ymm4\n"
                    "VPADDB %%xmm1, %%xmm1, %%xmm1\n"
                    "VPADDB %%xmm2, %%xmm2, %%xmm2\n"
                    "VINSERTF128 $1, %%xmm2, %%ymm1, %%ymm1\n"
                    "VXORPS %%ymm1, %%ymm4, %%ymm1\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[dst])\n"
                    "VMOVDQU 32(%[src]), %%ymm0\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU 32(%[dst]), %%ymm1\n"
                    "VEXTRACTF128 $1, %%ymm1, %%xmm2\n"
                    "VPCMPGTB %%xmm1, %%xmm3, %%xmm4\n"
                    "VPCMPGTB %%xmm2, %%xmm3, %%xmm5\n"
                    "VPAND %%xmm6, %%xmm4, %%xmm4\n"
                    "VPAND %%xmm6, %%xmm5, %%xmm5\n"
                    "VINSERTF128 $1, %%xmm5, %%ymm4, %%ymm4\n"
                    "VPADDB %%xmm1, %%xmm1, %%xmm1\n"
                    "VPADDB %%xmm2, %%xmm2, %%xmm2\n"
                    "VINSERTF128 $1, %%xmm2, %%ymm1, %%ymm1\n"
                    "VXORPS %%ymm1, %%ymm4, %%ymm1\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 32(%[dst])\n"
                    "VMOVDQU 64(%[src]), %%ymm0\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU 64(%[dst]), %%ymm1\n"
                    "VEXTRACTF128 $1, %%ymm1, %%xmm2\n"
                    "VPCMPGTB %%xmm1, %%xmm3, %%xmm4\n"
                    "VPCMPGTB %%xmm2, %%xmm3, %%xmm5\n"
                    "VPAND %%xmm6, %%xmm4, %%xmm4\n"
                    "VPAND %%xmm6, %%xmm5, %%xmm5\n"
                    "VINSERTF128 $1, %%xmm5, %%ymm4, %%ymm4\n"
                    "VPADDB %%xmm1, %%xmm1, %%xmm1\n"
                    "VPADDB %%xmm2, %%xmm2, %%xmm2\n"
                    "VINSERTF128 $1, %%xmm2, %%ymm1, %%ymm1\n"
                    "VXORPS %%ymm1, %%ymm4, %%ymm1\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 64(%[dst])\n"
                    "VMOVDQU 96(%[src]), %%ymm0\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU 96(%[dst]), %%ymm1\n"
                    "VEXTRACTF128 $1, %%ymm1, %%xmm2\n"
                    "VPCMPGTB %%xmm1, %%xmm3, %%xmm4\n"
                    "VPCMPGTB %%xmm2, %%xmm3, %%xmm5\n"
                    "VPAND %%xmm6, %%xmm4, %%xmm4\n"
                    "VPAND %%xmm6, %%xmm5, %%xmm5\n"
                    "VINSERTF128 $1, %%xmm5, %%ymm4, %%ymm4\n"
                    "VPADDB %%xmm1, %%xmm1, %%xmm1\n"
                    "VPADDB %%xmm2, %%xmm2, %%xmm2\n"
                    "VINSERTF128 $1, %%xmm2, %%ymm1, %%ymm1\n"
                    "VXORPS %%ymm1, %%ymm4, %%ymm1\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 96(%[dst])\n"
                    :
                    : [dst] "r" (dst), [src] "r" (src)
                    : "rax", "ymm0", "ymm1", "xmm2",
                      "xmm3", "ymm4", "xmm5", "xmm6", "memory");
            }
            remainder = count % 16;
            for (i = 0; i < remainder / 4; i++, src+=4, dst+=4) {
                asm("VMOVDQU (%[src]), %%ymm0\n"
                    "VMOVDQU (%[dst]), %%ymm1\n"
                    "VEXTRACTF128 $1, %%ymm1, %%xmm2\n"
                    "VXORPS %%xmm3, %%xmm3, %%xmm3\n"
                    "VPCMPGTB %%xmm1, %%xmm3, %%xmm4\n"
                    "VPCMPGTB %%xmm2, %%xmm3, %%xmm5\n"
                    "MOVQ $0x1d1d1d1d1d1d1d1d, %%rax\n"
                    "VPINSRQ $0, %%rax, %%xmm3, %%xmm3\n"
                    "VPINSRQ $1, %%rax, %%xmm3, %%xmm3\n"
                    "VPAND %%xmm3, %%xmm4, %%xmm4\n"
                    "VPAND %%xmm3, %%xmm5, %%xmm5\n"
                    "VINSERTF128 $1, %%xmm5, %%ymm4, %%ymm4\n"
                    "VPADDB %%xmm1, %%xmm1, %%xmm1\n"
                    "VPADDB %%xmm2, %%xmm2, %%xmm2\n"
                    "VINSERTF128 $1, %%xmm2, %%ymm1, %%ymm1\n"
                    "VXORPS %%ymm1, %%ymm4, %%ymm1\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[dst])"
                    :
                    : [dst] "r" (dst), [src] "r" (src)
                    : "rax", "ymm0", "ymm1", "xmm2",
                      "xmm3", "ymm4", "xmm5", "memory");
            }
            remainder %= 4;
            for(i = 0; i < remainder; i++, src++, dst++) {
                VDEV_RAIDZ_64MUL_2(*dst, mask);
                *dst ^= *src;
            }

            for (i = 0; i < (xcount - count); i++, dst++) {
                VDEV_RAIDZ_64MUL_2(*dst, mask);
            }
        }
    }

    src = rm->rm_col[VDEV_RAIDZ_Q].rc_data;
    dst = rm->rm_col[x].rc_data;
    exp = 255 - (rm->rm_cols - 1 - x);

    for (i = 0; i < xcount; i++, dst++, src++) {
        *dst ^= *src;
        for (j = 0, b = (uint8_t *)dst; j < 8; j++, b++) {
            *b = vdev_raidz_exp2(*b, exp);
        }
    }

    return (1 << VDEV_RAIDZ_Q);
}

void
vdev_raidz_generate_parity_pqr_sse4(raidz_map_t *rm)
{
    uint64_t *p, *q, *r, *src, pcnt, ccnt, mask, i;
    int c, remainder;

    pcnt = rm->rm_col[VDEV_RAIDZ_P].rc_size / sizeof (src[0]);
    ASSERT(rm->rm_col[VDEV_RAIDZ_P].rc_size ==
            rm->rm_col[VDEV_RAIDZ_Q].rc_size);
    ASSERT(rm->rm_col[VDEV_RAIDZ_P].rc_size ==
            rm->rm_col[VDEV_RAIDZ_R].rc_size);

    for (c = rm->rm_firstdatacol; c < rm->rm_cols; c++) {
        src = rm->rm_col[c].rc_data;
        p = rm->rm_col[VDEV_RAIDZ_P].rc_data;
        q = rm->rm_col[VDEV_RAIDZ_Q].rc_data;
        r = rm->rm_col[VDEV_RAIDZ_R].rc_data;

        ccnt = rm->rm_col[c].rc_size / sizeof (src[0]);

        if (c == rm->rm_firstdatacol) {
            ASSERT(ccnt == pcnt || ccnt == 0);
            if(ccnt != 0) {
                for (i = 0; i < ccnt / 16; i++, src+=16, p+=16, q+=16, r+=16) {
                    asm("VMOVDQU (%[src]), %%ymm0\n"
                        "VMOVDQU %%ymm0, (%[p])\n"
                        "VMOVDQU %%ymm0, (%[q])\n"
                        "VMOVDQU %%ymm0, (%[r])\n"
                        "VMOVDQU 32(%[src]), %%ymm1\n"
                        "VMOVDQU %%ymm1, 32(%[p])\n"
                        "VMOVDQU %%ymm1, 32(%[q])\n"
                        "VMOVDQU %%ymm1, 32(%[r])\n"
                        "VMOVDQU 64(%[src]), %%ymm2\n"
                        "VMOVDQU %%ymm2, 64(%[p])\n"
                        "VMOVDQU %%ymm2, 64(%[q])\n"
                        "VMOVDQU %%ymm2, 64(%[r])\n"
                        "VMOVDQU 96(%[src]), %%ymm3\n"
                        "VMOVDQU %%ymm3, 96(%[p])\n"
                        "VMOVDQU %%ymm3, 96(%[q])\n"
                        "VMOVDQU %%ymm3, 96(%[r])\n"
                        :
                        : [p] "r" (p), [q] "r" (q),
                          [r] "r" (r), [src] "r" (src)
                        : "ymm0", "ymm1", "ymm2", "ymm3", "memory");
                }
                remainder = ccnt % 16;
                for (i = 0; i < remainder / 4; i++, src+=4, p+=4, q+=4, r+=4) {
                    asm("VMOVDQU (%[src]), %%ymm0\n"
                        "VMOVDQU %%ymm0, (%[p])\n"
                        "VMOVDQU %%ymm0, (%[q])\n"
                        "VMOVDQU %%ymm0, (%[r])\n"
                        :
                        : [p] "r" (p), [q] "r" (q),
                          [r] "r" (r), [src] "r" (src)
                        : "ymm0", "memory");
                }
                remainder %= 4;
                for(i = 0; i < remainder; i++, p++, src++, q++, r++) {
                    *p = *src;
                    *q = *src;
                    *r = *src;
                }
            } else {
                for (i = 0; i < pcnt / 16; i++, p+=16, q+=16, r+=16) {
                    asm("VXORPS %%ymm0, %%ymm0, %%ymm0\n"
                        "VMOVDQU %%ymm0, (%[p])\n"
                        "VMOVDQU %%ymm0, (%[q])\n"
                        "VMOVDQU %%ymm0, (%[r])\n"
                        "VMOVDQU %%ymm0, 32(%[p])\n"
                        "VMOVDQU %%ymm0, 32(%[q])\n"
                        "VMOVDQU %%ymm0, 32(%[r])\n"
                        "VMOVDQU %%ymm0, 64(%[p])\n"
                        "VMOVDQU %%ymm0, 64(%[q])\n"
                        "VMOVDQU %%ymm0, 64(%[r])\n"
                        "VMOVDQU %%ymm0, 96(%[p])\n"
                        "VMOVDQU %%ymm0, 96(%[q])\n"
                        "VMOVDQU %%ymm0, 96(%[r])\n"
                        :
                        : [p] "r" (p), [q] "r" (q), [r] "r" (r)
                        : "ymm0", "memory");
                }
                remainder = pcnt % 16;
                for (i = 0; i < remainder / 4; i++, p+=4, q+=4, r+=4) {
                    asm("VXORPS %%ymm0, %%ymm0, %%ymm0\n"
                        "VMOVDQU %%ymm0, (%[p])\n"
                        "VMOVDQU %%ymm0, (%[q])\n"
                        "VMOVDQU %%ymm0, (%[r])\n"
                        :
                        : [p] "r" (p), [q] "r" (q), [r] "r" (r)
                        : "ymm0", "memory");
                }
                remainder %= 4;
                for(i = 0; i < remainder; i++, p++, q++, r++) {
                    *p = 0;
                    *q = 0;
                    *r = 0;
                }
            }
        } else {
            ASSERT(ccnt <= pcnt);

            for (i = 0; i < ccnt / 16; i++, src+=16, p+=16, q+=16, r+=16) {
                asm("VMOVDQU (%[src]), %%ymm0\n"
                    "VMOVDQU (%[p]), %%ymm1\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[p])\n"
                    "VMOVDQU (%[q]), %%ymm1\n"
                    "VEXTRACTF128 $1, %%ymm1, %%xmm2\n"
                    "VXORPS %%xmm3, %%xmm3, %%xmm3\n"
                    "VPCMPGTB %%xmm1, %%xmm3, %%xmm4\n"
                    "VPCMPGTB %%xmm2, %%xmm3, %%xmm5\n"
                    "MOVQ $0x1d1d1d1d1d1d1d1d, %%rax\n"
                    "VPINSRQ $0, %%rax, %%xmm6, %%xmm6\n"
                    "VPINSRQ $1, %%rax, %%xmm6, %%xmm6\n"
                    "VPAND %%xmm6, %%xmm4, %%xmm4\n"
                    "VPAND %%xmm6, %%xmm5, %%xmm5\n"
                    "VINSERTF128 $1, %%xmm5, %%ymm4, %%ymm4\n"
                    "VPADDB %%xmm1, %%xmm1, %%xmm1\n"
                    "VPADDB %%xmm2, %%xmm2, %%xmm2\n"
                    "VINSERTF128 $1, %%xmm2, %%ymm1, %%ymm1\n"
                    "VXORPS %%ymm1, %%ymm4, %%ymm1\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[q])\n"
                    "VMOVDQU (%[r]), %%ymm1\n"
                    "VEXTRACTF128 $1, %%ymm1, %%xmm2\n"
                    "VPCMPGTB %%xmm1, %%xmm3, %%xmm4\n"
                    "VPCMPGTB %%xmm2, %%xmm3, %%xmm5\n"
                    "VPAND %%xmm6, %%xmm4, %%xmm4\n"
                    "VPAND %%xmm6, %%xmm5, %%xmm5\n"
                    "VPADDB %%xmm1, %%xmm1, %%xmm1\n"
                    "VPADDB %%xmm2, %%xmm2, %%xmm2\n"
                    "VPXOR %%xmm1, %%xmm4, %%xmm1\n"
                    "VPXOR %%xmm2, %%xmm5, %%xmm2\n"
                    "VPCMPGTB %%xmm1, %%xmm3, %%xmm4\n"
                    "VPCMPGTB %%xmm2, %%xmm3, %%xmm5\n"
                    "VPAND %%xmm6, %%xmm4, %%xmm4\n"
                    "VPAND %%xmm6, %%xmm5, %%xmm5\n"
                    "VINSERTF128 $1, %%xmm5, %%ymm4, %%ymm4\n"
                    "VPADDB %%xmm1, %%xmm1, %%xmm1\n"
                    "VPADDB %%xmm2, %%xmm2, %%xmm2\n"
                    "VINSERTF128 $1, %%xmm2, %%ymm1, %%ymm1\n"
                    "VXORPS %%ymm1, %%ymm4, %%ymm1\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[r])\n"
                    "VMOVDQU 32(%[src]), %%ymm0\n"
                    "VMOVDQU 32(%[p]), %%ymm1\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 32(%[p])\n"
                    "VMOVDQU 32(%[q]), %%ymm1\n"
                    "VEXTRACTF128 $1, %%ymm1, %%xmm2\n"
                    "VPCMPGTB %%xmm1, %%xmm3, %%xmm4\n"
                    "VPCMPGTB %%xmm2, %%xmm3, %%xmm5\n"
                    "VPAND %%xmm6, %%xmm4, %%xmm4\n"
                    "VPAND %%xmm6, %%xmm5, %%xmm5\n"
                    "VINSERTF128 $1, %%xmm5, %%ymm4, %%ymm4\n"
                    "VPADDB %%xmm1, %%xmm1, %%xmm1\n"
                    "VPADDB %%xmm2, %%xmm2, %%xmm2\n"
                    "VINSERTF128 $1, %%xmm2, %%ymm1, %%ymm1\n"
                    "VXORPS %%ymm1, %%ymm4, %%ymm1\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 32(%[q])\n"
                    "VMOVDQU 32(%[r]), %%ymm1\n"
                    "VEXTRACTF128 $1, %%ymm1, %%xmm2\n"
                    "VPCMPGTB %%xmm1, %%xmm3, %%xmm4\n"
                    "VPCMPGTB %%xmm2, %%xmm3, %%xmm5\n"
                    "VPAND %%xmm6, %%xmm4, %%xmm4\n"
                    "VPAND %%xmm6, %%xmm5, %%xmm5\n"
                    "VPADDB %%xmm1, %%xmm1, %%xmm1\n"
                    "VPADDB %%xmm2, %%xmm2, %%xmm2\n"
                    "VPXOR %%xmm1, %%xmm4, %%xmm1\n"
                    "VPXOR %%xmm2, %%xmm5, %%xmm2\n"
                    "VPCMPGTB %%xmm1, %%xmm3, %%xmm4\n"
                    "VPCMPGTB %%xmm2, %%xmm3, %%xmm5\n"
                    "VPAND %%xmm6, %%xmm4, %%xmm4\n"
                    "VPAND %%xmm6, %%xmm5, %%xmm5\n"
                    "VINSERTF128 $1, %%xmm5, %%ymm4, %%ymm4\n"
                    "VPADDB %%xmm1, %%xmm1, %%xmm1\n"
                    "VPADDB %%xmm2, %%xmm2, %%xmm2\n"
                    "VINSERTF128 $1, %%xmm2, %%ymm1, %%ymm1\n"
                    "VXORPS %%ymm1, %%ymm4, %%ymm1\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 32(%[r])\n"
                    "VMOVDQU 64(%[src]), %%ymm0\n"
                    "VMOVDQU 64(%[p]), %%ymm1\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 64(%[p])\n"
                    "VMOVDQU 64(%[q]), %%ymm1\n"
                    "VEXTRACTF128 $1, %%ymm1, %%xmm2\n"
                    "VPCMPGTB %%xmm1, %%xmm3, %%xmm4\n"
                    "VPCMPGTB %%xmm2, %%xmm3, %%xmm5\n"
                    "VPAND %%xmm6, %%xmm4, %%xmm4\n"
                    "VPAND %%xmm6, %%xmm5, %%xmm5\n"
                    "VINSERTF128 $1, %%xmm5, %%ymm4, %%ymm4\n"
                    "VPADDB %%xmm1, %%xmm1, %%xmm1\n"
                    "VPADDB %%xmm2, %%xmm2, %%xmm2\n"
                    "VINSERTF128 $1, %%xmm2, %%ymm1, %%ymm1\n"
                    "VXORPS %%ymm1, %%ymm4, %%ymm1\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 64(%[q])\n"
                    "VMOVDQU 64(%[r]), %%ymm1\n"
                    "VEXTRACTF128 $1, %%ymm1, %%xmm2\n"
                    "VPCMPGTB %%xmm1, %%xmm3, %%xmm4\n"
                    "VPCMPGTB %%xmm2, %%xmm3, %%xmm5\n"
                    "VPAND %%xmm6, %%xmm4, %%xmm4\n"
                    "VPAND %%xmm6, %%xmm5, %%xmm5\n"
                    "VPADDB %%xmm1, %%xmm1, %%xmm1\n"
                    "VPADDB %%xmm2, %%xmm2, %%xmm2\n"
                    "VPXOR %%xmm1, %%xmm4, %%xmm1\n"
                    "VPXOR %%xmm2, %%xmm5, %%xmm2\n"
                    "VPCMPGTB %%xmm1, %%xmm3, %%xmm4\n"
                    "VPCMPGTB %%xmm2, %%xmm3, %%xmm5\n"
                    "VPAND %%xmm6, %%xmm4, %%xmm4\n"
                    "VPAND %%xmm6, %%xmm5, %%xmm5\n"
                    "VINSERTF128 $1, %%xmm5, %%ymm4, %%ymm4\n"
                    "VPADDB %%xmm1, %%xmm1, %%xmm1\n"
                    "VPADDB %%xmm2, %%xmm2, %%xmm2\n"
                    "VINSERTF128 $1, %%xmm2, %%ymm1, %%ymm1\n"
                    "VXORPS %%ymm1, %%ymm4, %%ymm1\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 64(%[r])\n"
                    "VMOVDQU 96(%[src]), %%ymm0\n"
                    "VMOVDQU 96(%[p]), %%ymm1\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 96(%[p])\n"
                    "VMOVDQU 96(%[q]), %%ymm1\n"
                    "VEXTRACTF128 $1, %%ymm1, %%xmm2\n"
                    "VPCMPGTB %%xmm1, %%xmm3, %%xmm4\n"
                    "VPCMPGTB %%xmm2, %%xmm3, %%xmm5\n"
                    "VPAND %%xmm6, %%xmm4, %%xmm4\n"
                    "VPAND %%xmm6, %%xmm5, %%xmm5\n"
                    "VINSERTF128 $1, %%xmm5, %%ymm4, %%ymm4\n"
                    "VPADDB %%xmm1, %%xmm1, %%xmm1\n"
                    "VPADDB %%xmm2, %%xmm2, %%xmm2\n"
                    "VINSERTF128 $1, %%xmm2, %%ymm1, %%ymm1\n"
                    "VXORPS %%ymm1, %%ymm4, %%ymm1\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 96(%[q])\n"
                    "VMOVDQU 96(%[r]), %%ymm1\n"
                    "VEXTRACTF128 $1, %%ymm1, %%xmm2\n"
                    "VPCMPGTB %%xmm1, %%xmm3, %%xmm4\n"
                    "VPCMPGTB %%xmm2, %%xmm3, %%xmm5\n"
                    "VPAND %%xmm6, %%xmm4, %%xmm4\n"
                    "VPAND %%xmm6, %%xmm5, %%xmm5\n"
                    "VPADDB %%xmm1, %%xmm1, %%xmm1\n"
                    "VPADDB %%xmm2, %%xmm2, %%xmm2\n"
                    "VPXOR %%xmm1, %%xmm4, %%xmm1\n"
                    "VPXOR %%xmm2, %%xmm5, %%xmm2\n"
                    "VPCMPGTB %%xmm1, %%xmm3, %%xmm4\n"
                    "VPCMPGTB %%xmm2, %%xmm3, %%xmm5\n"
                    "VPAND %%xmm6, %%xmm4, %%xmm4\n"
                    "VPAND %%xmm6, %%xmm5, %%xmm5\n"
                    "VINSERTF128 $1, %%xmm5, %%ymm4, %%ymm4\n"
                    "VPADDB %%xmm1, %%xmm1, %%xmm1\n"
                    "VPADDB %%xmm2, %%xmm2, %%xmm2\n"
                    "VINSERTF128 $1, %%xmm2, %%ymm1, %%ymm1\n"
                    "VXORPS %%ymm1, %%ymm4, %%ymm1\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 96(%[r])\n"
                    :
                    : [p] "r" (p), [q] "r" (q),
                      [r] "r" (r), [src] "r" (src)
                    : "rax", "ymm0", "ymm1", "xmm2",
                      "xmm3", "ymm4", "xmm5", "xmm6", "memory");
            }
            remainder = ccnt % 16;
            for (i = 0; i < remainder / 4; i++, src+=4, p+=4, q+=4, r+=4) {
                asm("VMOVDQU (%[src]), %%ymm0\n"
                    "VMOVDQU (%[p]), %%ymm1\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[p])\n"
                    "VMOVDQU (%[q]), %%ymm1\n"
                    "VEXTRACTF128 $1, %%ymm1, %%xmm2\n"
                    "VXORPS %%xmm3, %%xmm3, %%xmm3\n"
                    "VPCMPGTB %%xmm1, %%xmm3, %%xmm4\n"
                    "VPCMPGTB %%xmm2, %%xmm3, %%xmm5\n"
                    "MOVQ $0x1d1d1d1d1d1d1d1d, %%rax\n"
                    "VPINSRQ $0, %%rax, %%xmm6, %%xmm6\n"
                    "VPINSRQ $1, %%rax, %%xmm6, %%xmm6\n"
                    "VPAND %%xmm6, %%xmm4, %%xmm4\n"
                    "VPAND %%xmm6, %%xmm5, %%xmm5\n"
                    "VINSERTF128 $1, %%xmm5, %%ymm4, %%ymm4\n"
                    "VPADDB %%xmm1, %%xmm1, %%xmm1\n"
                    "VPADDB %%xmm2, %%xmm2, %%xmm2\n"
                    "VINSERTF128 $1, %%xmm2, %%ymm1, %%ymm1\n"
                    "VXORPS %%ymm1, %%ymm4, %%ymm1\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[q])\n"
                    "VMOVDQU (%[r]), %%ymm1\n"
                    "VEXTRACTF128 $1, %%ymm1, %%xmm2\n"
                    "VPCMPGTB %%xmm1, %%xmm3, %%xmm4\n"
                    "VPCMPGTB %%xmm2, %%xmm3, %%xmm5\n"
                    "VPAND %%xmm6, %%xmm4, %%xmm4\n"
                    "VPAND %%xmm6, %%xmm5, %%xmm5\n"
                    "VPADDB %%xmm1, %%xmm1, %%xmm1\n"
                    "VPADDB %%xmm2, %%xmm2, %%xmm2\n"
                    "VPXOR %%xmm1, %%xmm4, %%xmm1\n"
                    "VPXOR %%xmm2, %%xmm5, %%xmm2\n"
                    "VPCMPGTB %%xmm1, %%xmm3, %%xmm4\n"
                    "VPCMPGTB %%xmm2, %%xmm3, %%xmm5\n"
                    "VPAND %%xmm6, %%xmm4, %%xmm4\n"
                    "VPAND %%xmm6, %%xmm5, %%xmm5\n"
                    "VINSERTF128 $1, %%xmm5, %%ymm4, %%ymm4\n"
                    "VPADDB %%xmm1, %%xmm1, %%xmm1\n"
                    "VPADDB %%xmm2, %%xmm2, %%xmm2\n"
                    "VINSERTF128 $1, %%xmm2, %%ymm1, %%ymm1\n"
                    "VXORPS %%ymm1, %%ymm4, %%ymm1\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[r])\n"
                    :
                    : [p] "r" (p), [q] "r" (q),
                      [r] "r" (r), [src] "r" (src)
                    : "rax", "ymm0", "ymm1", "xmm2",
                      "xmm3", "ymm4", "xmm5", "xmm6", "memory");
            }
            remainder %= 4;
            for(i = 0; i < remainder; i++, p++, src++, q++, r++) {
                *p ^= *src;
                VDEV_RAIDZ_64MUL_2(*q, mask);
                *q ^= *src;
                VDEV_RAIDZ_64MUL_4(*r, mask);
                *r ^= *src;
            }

            /*
             * Treat short columns as though they are full of 0s.
             * Note that there's therefore nothing needed for P.
             */
            for (i = 0; i < (pcnt - ccnt); i++, q++, r++) {
                VDEV_RAIDZ_64MUL_2(*q, mask);
                VDEV_RAIDZ_64MUL_4(*r, mask);
            }
        }
    }
}
