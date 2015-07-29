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
vdev_raidz_generate_parity_p_avx2(raidz_map_t *rm)
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
                asm("VMOVDQU (%[src]), %%ymm0\n"
                    "VMOVDQU %%ymm0, (%[p])\n"
                    "VMOVDQU 16(%[src]), %%ymm1\n"
                    "VMOVDQU %%ymm1, 16(%[p])\n"
                    "VMOVDQU 32(%[src]), %%ymm2\n"
                    "VMOVDQU %%ymm2, 32(%[p])\n"
                    "VMOVDQU 48(%[src]), %%ymm3\n"
                    "VMOVDQU %%ymm3, 48(%[p])\n"
                    :
                    : [p] "r" (p), [src] "r" (src)
                    : "ymm0", "ymm1", "ymm2", "ymm3", "memory");
            }
            remainder = ccount % 8;
            for(i = 0; i < remainder / 2; i++, p+=2, src+=2) {
                asm("VMOVDQU (%[src]), %%ymm0\n"
                    "VMOVDQU %%ymm0, (%[p])\n"
                    :
                    : [p] "r" (p), [src] "r" (src)
                    : "ymm0", "memory");
            }
            remainder %= 2;
            if (remainder > 0) {
                *p = *src;
            }
        } else {
            ASSERT(ccount <= pcount);
            for (i = 0; i < ccount / 8; i++, src+=8, p+=8) {
                asm("VMOVDQU (%[src]), %%ymm0\n"
                    "VMOVDQU (%[p]), %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[p])\n"
                    "VMOVDQU 16(%[src]), %%ymm2\n"
                    "VMOVDQU 16(%[p]), %%ymm3\n"
                    "VPXOR %%ymm2, %%ymm3, %%ymm3\n"
                    "VMOVDQU %%ymm3, 16(%[p])\n"
                    "VMOVDQU 32(%[src]), %%ymm4\n"
                    "VMOVDQU 32(%[p]), %%ymm5\n"
                    "VPXOR %%ymm4, %%ymm5, %%ymm5\n"
                    "VMOVDQU %%ymm5, 32(%[p])\n"
                    "VMOVDQU 48(%[src]), %%ymm6\n"
                    "VMOVDQU 48(%[p]), %%ymm7\n"
                    "VPXOR %%ymm6, %%ymm7, %%ymm7\n"
                    "VMOVDQU %%ymm7, 48(%[p])\n"
                    :
                    : [p] "r" (p), [src] "r" (src)
                    : "ymm0", "ymm1", "ymm2", "ymm3",
                      "ymm4", "ymm5", "ymm6", "ymm7", "memory");
            }
            remainder = ccount % 8;
            for(i = 0; i < remainder / 2; i++, p+=2, src+=2) {
                asm("VMOVDQU (%[src]), %%ymm0\n"
                    "VMOVDQU (%[p]), %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[p])\n"
                    :
                    : [p] "r" (p), [src] "r" (src)
                    : "ymm0", "ymm1", "memory");
            }
            remainder %= 2;
            if (remainder > 0) {
                *p ^= *src;
            }
        }
    }
}

int
vdev_raidz_reconstruct_p_avx2(raidz_map_t *rm, int *tgts, int ntgts)
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
        asm("VMOVDQU (%[src]), %%ymm0\n"
            "VMOVDQU %%ymm0, (%[dst])\n"
            "VMOVDQU 16(%[src]), %%ymm1\n"
            "VMOVDQU %%ymm1, 16(%[dst])\n"
            "VMOVDQU 32(%[src]), %%ymm2\n"
            "VMOVDQU %%ymm2, 32(%[dst])\n"
            "VMOVDQU 48(%[src]), %%ymm3\n"
            "VMOVDQU %%ymm3, 48(%[dst])"
            :
            : [dst] "r" (dst), [src] "r" (src)
            : "ymm0", "ymm1", "ymm2", "ymm3", "memory");
    }
    remainder = xcount % 8;
    for (i = 0; i < remainder / 2; i++, src+=2, dst+=2) {
        asm("VMOVDQU (%[src]), %%ymm0\n"
            "VMOVDQU %%ymm0, (%[dst])"
            :
            : [dst] "r" (dst), [src] "r" (src)
            : "ymm0", "memory");
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
            asm("VMOVDQU (%[src]), %%ymm0\n"
                "VMOVDQU (%[dst]), %%ymm1\n"
                "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                "VMOVDQU %%ymm1, (%[dst])\n"
                "VMOVDQU 16(%[src]), %%ymm2\n"
                "VMOVDQU 16(%[dst]), %%ymm3\n"
                "VPXOR %%ymm2, %%ymm3, %%ymm3\n"
                "VMOVDQU %%ymm3, 16(%[dst])\n"
                "VMOVDQU 32(%[src]), %%ymm4\n"
                "VMOVDQU 32(%[dst]), %%ymm5\n"
                "VPXOR %%ymm4, %%ymm5, %%ymm5\n"
                "VMOVDQU %%ymm5, 32(%[dst])\n"
                "VMOVDQU 48(%[src]), %%ymm6\n"
                "VMOVDQU 48(%[dst]), %%ymm7\n"
                "VPXOR %%ymm6, %%ymm7, %%ymm7\n"
                "VMOVDQU %%ymm7, 48(%[dst])\n"
                :
                : [dst] "r" (dst), [src] "r" (src)
                : "ymm0", "ymm1", "ymm2", "ymm3",
                  "ymm4", "ymm5", "ymm6", "ymm7", "memory");
        }
        remainder = count % 8;
        for (i = 0; i < remainder / 2; i++, src+=2, dst+=2) {
            asm("VMOVDQU (%[src]), %%ymm0\n"
                "VMOVDQU (%[dst]), %%ymm1\n"
                "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                "VMOVDQU %%ymm1, (%[dst])"
                :
                : [dst] "r" (dst), [src] "r" (src)
                : "ymm0", "ymm1");
        }
        remainder %= 2;
        if (remainder > 0) {
            *dst ^= *src;
        }
    }

    return (1 << VDEV_RAIDZ_P);
}

void
vdev_raidz_generate_parity_pq_avx2(raidz_map_t *rm)
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
                    asm("VMOVDQU (%[src]), %%ymm0\n"
                        "VMOVDQU %%ymm0, (%[p])\n"
                        "VMOVDQU %%ymm0, (%[q])\n"
                        "VMOVDQU 16(%[src]), %%ymm1\n"
                        "VMOVDQU %%ymm1, 16(%[p])\n"
                        "VMOVDQU %%ymm1, 16(%[q])\n"
                        "VMOVDQU 32(%[src]), %%ymm2\n"
                        "VMOVDQU %%ymm2, 32(%[p])\n"
                        "VMOVDQU %%ymm2, 32(%[q])\n"
                        "VMOVDQU 48(%[src]), %%ymm3\n"
                        "VMOVDQU %%ymm3, 48(%[p])\n"
                        "VMOVDQU %%ymm3, 48(%[q])\n"
                        :
                        : [p] "r" (p), [q] "r" (q), [src] "r" (src)
                        : "ymm0", "ymm1", "ymm2", "ymm3", "memory");
                }
                remainder = ccnt % 8;
                for (i = 0; i < remainder / 2; i++, src+=2, p+=2, q+=2) {
                    asm("VMOVDQU (%[src]), %%ymm0\n"
                        "VMOVDQU %%ymm0, (%[p])\n"
                        "VMOVDQU %%ymm0, (%[q])"
                        :
                        : [p] "r" (p), [q] "r" (q), [src] "r" (src)
                        : "ymm0", "memory");
                }
                remainder %= 2;
                if (remainder > 0) {
                    *p = *src;
                    *q = *src;
                }
            } else {
                for (i = 0; i < pcnt / 8; i++, p+=8, q+=8) {
                    asm("VPXOR %%ymm0, %%ymm0, %%ymm0\n"
                        "VMOVDQU %%ymm0, (%[p])\n"
                        "VMOVDQU %%ymm0, (%[q])\n"
                        "VMOVDQU %%ymm0, 16(%[p])\n"
                        "VMOVDQU %%ymm0, 16(%[q])\n"
                        "VMOVDQU %%ymm0, 32(%[p])\n"
                        "VMOVDQU %%ymm0, 32(%[q])\n"
                        "VMOVDQU %%ymm0, 48(%[p])\n"
                        "VMOVDQU %%ymm0, 48(%[q])\n"
                        :
                        : [p] "r" (p), [q] "r" (q)
                        : "ymm0", "memory");
                }
                remainder = pcnt % 8;
                for (i = 0; i < remainder / 2; i++, p+=2, q+=2) {
                    asm("VPXOR %%ymm0, %%ymm0, %%ymm0\n"
                        "VMOVDQU %%ymm0, (%[p])\n"
                        "VMOVDQU %%ymm0, (%[q])"
                        :
                        : [p] "r" (p), [q] "r" (q)
                        : "ymm0", "memory");
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
                asm("VMOVDQU (%[src]), %%ymm0\n"
                    "VMOVDQU (%[p]), %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[p])\n"
                    "VMOVDQU (%[q]), %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "MOVQ $0x1d1d1d1d1d1d1d1d, %%rax\n"
                    "VPINSRQ $0, %%rax, %%xmm6, %%xmm6\n"
                    "VPINSRQ $1, %%rax, %%xmm6, %%xmm6\n"
                    "VINSERTF128 $1, %%xmm6, %%ymm6, %%ymm6\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[q])\n"
                    "VMOVDQU 16(%[src]), %%ymm0\n"
                    "VMOVDQU 16(%[p]), %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 16(%[p])\n"
                    "VMOVDQU 16(%[q]), %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 16(%[q])\n"
                    "VMOVDQU 32(%[src]), %%ymm0\n"
                    "VMOVDQU 32(%[p]), %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 32(%[p])\n"
                    "VMOVDQU 32(%[q]), %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 32(%[q])\n"
                    "VMOVDQU 48(%[src]), %%ymm0\n"
                    "VMOVDQU 48(%[p]), %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 48(%[p])\n"
                    "VMOVDQU 48(%[q]), %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 48(%[q])\n"
                    :
                    : [p] "r" (p), [q] "r" (q), [src] "r" (src)
                    : "rax", "ymm0", "ymm1", "ymm2",
                      "ymm3", "ymm5", "ymm6", "memory");
            }
            remainder = ccnt % 8;
            for (i = 0; i < remainder / 2; i++, src+=2, p+=2, q+=2) {
                asm("VMOVDQU (%[src]), %%ymm0\n"
                    "VMOVDQU (%[p]), %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[p])\n"
                    "VMOVDQU (%[q]), %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "MOVQ $0x1d1d1d1d1d1d1d1d, %%rax\n"
                    "VPINSRQ $0, %%rax, %%xmm4, %%xmm4\n"
                    "VPINSRQ $1, %%rax, %%xmm4, %%xmm4\n"
                    "VINSERTF128 $1, %%xmm4, %%ymm4, %%ymm4\n"
                    "VPAND %%ymm4, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[q])"
                    :
                    : [p] "r" (p), [q] "r" (q), [src] "r" (src)
                    : "rax", "ymm0", "ymm1", "ymm2",
                      "ymm3", "ymm4", "ymm5", "memory");
            }
            remainder %= 2;
            if (remainder > 0) {
                *p ^= *src;
                VDEV_RAIDZ_64MUL_2(*q, mask);
                *q ^= *src;
                q++;
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
vdev_raidz_reconstruct_pq_avx2(raidz_map_t *rm, int *tgts, int ntgts)
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

    vdev_raidz_generate_parity_pq_avx2(rm);

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
vdev_raidz_reconstruct_q_avx2(raidz_map_t *rm, int *tgts, int ntgts)
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
            for (i = 0; i < count / 8; i++, src+=8, dst+=8) {
                asm("VMOVDQU (%[src]), %%ymm0\n"
                    "VMOVDQU %%ymm0, (%[dst])\n"
                    "VMOVDQU 16(%[src]), %%ymm1\n"
                    "VMOVDQU %%ymm1, 16(%[dst])\n"
                    "VMOVDQU 32(%[src]), %%ymm2\n"
                    "VMOVDQU %%ymm2, 32(%[dst])\n"
                    "VMOVDQU 48(%[src]), %%ymm3\n"
                    "VMOVDQU %%ymm3, 48(%[dst])\n"
                    :
                    : [dst] "r" (dst), [src] "r" (src)
                    : "ymm0", "ymm1", "ymm2", "ymm3", "memory");
            }
            remainder = count % 8;
            for (i = 0; i < remainder / 2; i++, src+=2, dst+=2) {
                asm("VMOVDQU (%[src]), %%ymm0\n"
                    "VMOVDQU %%ymm0, (%[dst])"
                    :
                    : [dst] "r" (dst), [src] "r" (src)
                    : "ymm0", "memory");
            }
            remainder %= 2;
            if (remainder > 0) {
                *dst = *src;
                dst++;
            }
            for (i = 0; i < (xcount - count) / 8; i++, dst+=8) {
                asm("VPXOR %%ymm0, %%ymm0, %%ymm0\n"
                    "VMOVDQU %%ymm0, (%[dst])\n"
                    "VMOVDQU %%ymm0, 16(%[dst])\n"
                    "VMOVDQU %%ymm0, 32(%[dst])\n"
                    "VMOVDQU %%ymm0, 48(%[dst])\n"
                    :
                    : [dst] "r" (dst)
                    : "ymm0", "memory");
            }
            remainder = (xcount - count) % 8;
            for (i = 0; i < remainder / 2; i++, dst+=2) {
                asm("VPXOR %%ymm0, %%ymm0, %%ymm0\n"
                    "VMOVDQU %%ymm0, (%[dst])"
                    :
                    : [dst] "r" (dst)
                    : "ymm0", "memory");
            }
            remainder %= 2;
            if (remainder > 0) {
                *dst = 0;
            }
        } else {
            for (i = 0; i < count / 8; i++, src+=8, dst+=8) {
                asm("VMOVDQU (%[src]), %%ymm0\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU (%[dst]), %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "MOVQ $0x1d1d1d1d1d1d1d1d, %%rax\n"
                    "VPINSRQ $0, %%rax, %%xmm6, %%xmm6\n"
                    "VPINSRQ $1, %%rax, %%xmm6, %%xmm6\n"
                    "VINSERTF128 $1, %%xmm6, %%ymm6, %%ymm6\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[dst])\n"
                    "VMOVDQU 16(%[src]), %%ymm0\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU 16(%[dst]), %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 16(%[dst])\n"
                    "VMOVDQU 32(%[src]), %%ymm0\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU 32(%[dst]), %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 32(%[dst])\n"
                    "VMOVDQU 48(%[src]), %%ymm0\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU 48(%[dst]), %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 48(%[dst])\n"
                    :
                    : [dst] "r" (dst), [src] "r" (src)
                    : "rax", "ymm0", "ymm1", "ymm2",
                      "ymm3", "ymm5", "ymm6", "memory");
            }
            remainder = count % 8;
            for (i = 0; i < remainder / 2; i++, src+=2, dst+=2) {
                asm("VMOVDQU (%[src]), %%ymm0\n"
                    "VMOVDQU (%[dst]), %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "MOVQ $0x1d1d1d1d1d1d1d1d, %%rax\n"
                    "VPINSRQ $0, %%rax, %%xmm4, %%xmm4\n"
                    "VPINSRQ $1, %%rax, %%xmm4, %%xmm4\n"
                    "VINSERTF128 $1, %%xmm4, %%ymm4, %%ymm4\n"
                    "VPAND %%ymm3, %%ymm4, %%ymm4\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm4, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[dst])"
                    :
                    : [dst] "r" (dst), [src] "r" (src)
                    : "rax", "ymm0", "ymm1", "ymm2",
                      "ymm3", "ymm4", "ymm5", "memory");
            }
            remainder %= 2;
            if (remainder > 0) {
                VDEV_RAIDZ_64MUL_2(*dst, mask);
                *dst ^= *src;
                dst++;
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
vdev_raidz_generate_parity_pqr_avx2(raidz_map_t *rm)
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
                for (i = 0; i < ccnt / 8; i++, src+=8, p+=8, q+=8, r+=8) {
                    asm("VMOVDQU (%[src]), %%ymm0\n"
                        "VMOVDQU %%ymm0, (%[p])\n"
                        "VMOVDQU %%ymm0, (%[q])\n"
                        "VMOVDQU %%ymm0, (%[r])\n"
                        "VMOVDQU 16(%[src]), %%ymm1\n"
                        "VMOVDQU %%ymm1, 16(%[p])\n"
                        "VMOVDQU %%ymm1, 16(%[q])\n"
                        "VMOVDQU %%ymm1, 16(%[r])\n"
                        "VMOVDQU 32(%[src]), %%ymm2\n"
                        "VMOVDQU %%ymm2, 32(%[p])\n"
                        "VMOVDQU %%ymm2, 32(%[q])\n"
                        "VMOVDQU %%ymm2, 32(%[r])\n"
                        "VMOVDQU 48(%[src]), %%ymm3\n"
                        "VMOVDQU %%ymm3, 48(%[p])\n"
                        "VMOVDQU %%ymm3, 48(%[q])\n"
                        "VMOVDQU %%ymm3, 48(%[r])\n"
                        :
                        : [p] "r" (p), [q] "r" (q),
                          [r] "r" (r), [src] "r" (src)
                        : "ymm0", "ymm1", "ymm2", "ymm3", "memory");
                }
                remainder = ccnt % 8;
                for (i = 0; i < remainder / 2; i++, src+=2, p+=2, q+=2, r+=2) {
                    asm("VMOVDQU (%[src]), %%ymm0\n"
                        "VMOVDQU %%ymm0, (%[p])\n"
                        "VMOVDQU %%ymm0, (%[q])\n"
                        "VMOVDQU %%ymm0, (%[r])\n"
                        :
                        : [p] "r" (p), [q] "r" (q),
                          [r] "r" (r), [src] "r" (src)
                        : "ymm0", "memory");
                }
                remainder %= 2;
                if (remainder > 0) {
                    *p = *src;
                    *q = *src;
                    *r = *src;
                }
            } else {
                for (i = 0; i < pcnt / 8; i++, p+=8, q+=8, r+=8) {
                    asm("VPXOR %%ymm0, %%ymm0, %%ymm0\n"
                        "VMOVDQU %%ymm0, (%[p])\n"
                        "VMOVDQU %%ymm0, (%[q])\n"
                        "VMOVDQU %%ymm0, (%[r])\n"
                        "VMOVDQU %%ymm0, 16(%[p])\n"
                        "VMOVDQU %%ymm0, 16(%[q])\n"
                        "VMOVDQU %%ymm0, 16(%[r])\n"
                        "VMOVDQU %%ymm0, 32(%[p])\n"
                        "VMOVDQU %%ymm0, 32(%[q])\n"
                        "VMOVDQU %%ymm0, 32(%[r])\n"
                        "VMOVDQU %%ymm0, 48(%[p])\n"
                        "VMOVDQU %%ymm0, 48(%[q])\n"
                        "VMOVDQU %%ymm0, 48(%[r])\n"
                        :
                        : [p] "r" (p), [q] "r" (q), [r] "r" (r)
                        : "ymm0", "memory");
                }
                remainder = pcnt % 8;
                for (i = 0; i < remainder / 2; i++, p+=2, q+=2, r+=2) {
                    asm("VPXOR %%ymm0, %%ymm0, %%ymm0\n"
                        "VMOVDQU %%ymm0, (%[p])\n"
                        "VMOVDQU %%ymm0, (%[q])\n"
                        "VMOVDQU %%ymm0, (%[r])\n"
                        :
                        : [p] "r" (p), [q] "r" (q), [r] "r" (r)
                        : "ymm0", "memory");
                }
                remainder %= 2;
                if (remainder > 0) {
                    *p = 0;
                    *q = 0;
                    *r = 0;
                }
            }
        } else {
            ASSERT(ccnt <= pcnt);

            for (i = 0; i < ccnt / 8; i++, src+=8, p+=8, q+=8, r+=8) {
                asm("VMOVDQU (%[src]), %%ymm0\n"
                    "VMOVDQU (%[p]), %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[p])\n"
                    "VMOVDQU (%[q]), %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "MOVQ $0x1d1d1d1d1d1d1d1d, %%rax\n"
                    "VPINSRQ $0, %%rax, %%xmm6, %%xmm6\n"
                    "VPINSRQ $1, %%rax, %%xmm6, %%xmm6\n"
                    "VINSERTF128 $1, %%xmm6, %%ymm6, %%ymm6\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[q])\n"
                    "VMOVDQU (%[r]), %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[r])\n"
                    "VMOVDQU 16(%[src]), %%ymm0\n"
                    "VMOVDQU 16(%[p]), %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 16(%[p])\n"
                    "VMOVDQU 16(%[q]), %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 16(%[q])\n"
                    "VMOVDQU 16(%[r]), %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 16(%[r])\n"
                    "VMOVDQU 32(%[src]), %%ymm0\n"
                    "VMOVDQU 32(%[p]), %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 32(%[p])\n"
                    "VMOVDQU 32(%[q]), %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 32(%[q])\n"
                    "VMOVDQU 32(%[r]), %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 32(%[r])\n"
                    "VMOVDQU 48(%[src]), %%ymm0\n"
                    "VMOVDQU 48(%[p]), %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 48(%[p])\n"
                    "VMOVDQU 48(%[q]), %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 48(%[q])\n"
                    "VMOVDQU 48(%[r]), %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, 48(%[r])\n"
                    :
                    : [p] "r" (p), [q] "r" (q),
                      [r] "r" (r), [src] "r" (src)
                    : "rax", "ymm0", "ymm1",
                      "ymm3", "ymm6", "memory");
            }
            remainder = ccnt % 8;
            for (i = 0; i < remainder / 2; i++, src+=2, p+=2, q+=2, r+=2) {
                asm("VMOVDQU (%[src]), %%ymm0\n"
                    "VMOVDQU (%[p]), %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[p])\n"
                    "VMOVDQU (%[q]), %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "MOVQ $0x1d1d1d1d1d1d1d1d, %%rax\n"
                    "VPINSRQ $0, %%rax, %%xmm6, %%xmm6\n"
                    "VPINSRQ $1, %%rax, %%xmm6, %%xmm6\n"
                    "VINSERTF128 $1, %%xmm6, %%ymm6, %%ymm6\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[q])\n"
                    "VMOVDQU (%[r]), %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm3, %%ymm3\n"
                    "VPCMPGTB %%ymm1, %%ymm3, %%ymm3\n"
                    "VPAND %%ymm6, %%ymm3, %%ymm3\n"
                    "VPADDB %%ymm1, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm3, %%ymm1, %%ymm1\n"
                    "VPXOR %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[r])\n"
                    :
                    : [p] "r" (p), [q] "r" (q),
                      [r] "r" (r), [src] "r" (src)
                    : "rax", "ymm0", "ymm1",
                      "ymm3", "ymm6", "memory");
            }
            remainder %= 2;
            if (remainder > 0) {
                *p ^= *src;
                VDEV_RAIDZ_64MUL_2(*q, mask);
                *q ^= *src;
                VDEV_RAIDZ_64MUL_4(*r, mask);
                *r ^= *src;
                q++;
                r++;
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
