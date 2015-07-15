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

#include "vdev_raidz.h"

const uint8_t vdev_raidz_pow2[256] = {
    0x01, 0x02, 0x04, 0x08, 0x10, 0x20, 0x40, 0x80,
    0x1d, 0x3a, 0x74, 0xe8, 0xcd, 0x87, 0x13, 0x26,
    0x4c, 0x98, 0x2d, 0x5a, 0xb4, 0x75, 0xea, 0xc9,
    0x8f, 0x03, 0x06, 0x0c, 0x18, 0x30, 0x60, 0xc0,
    0x9d, 0x27, 0x4e, 0x9c, 0x25, 0x4a, 0x94, 0x35,
    0x6a, 0xd4, 0xb5, 0x77, 0xee, 0xc1, 0x9f, 0x23,
    0x46, 0x8c, 0x05, 0x0a, 0x14, 0x28, 0x50, 0xa0,
    0x5d, 0xba, 0x69, 0xd2, 0xb9, 0x6f, 0xde, 0xa1,
    0x5f, 0xbe, 0x61, 0xc2, 0x99, 0x2f, 0x5e, 0xbc,
    0x65, 0xca, 0x89, 0x0f, 0x1e, 0x3c, 0x78, 0xf0,
    0xfd, 0xe7, 0xd3, 0xbb, 0x6b, 0xd6, 0xb1, 0x7f,
    0xfe, 0xe1, 0xdf, 0xa3, 0x5b, 0xb6, 0x71, 0xe2,
    0xd9, 0xaf, 0x43, 0x86, 0x11, 0x22, 0x44, 0x88,
    0x0d, 0x1a, 0x34, 0x68, 0xd0, 0xbd, 0x67, 0xce,
    0x81, 0x1f, 0x3e, 0x7c, 0xf8, 0xed, 0xc7, 0x93,
    0x3b, 0x76, 0xec, 0xc5, 0x97, 0x33, 0x66, 0xcc,
    0x85, 0x17, 0x2e, 0x5c, 0xb8, 0x6d, 0xda, 0xa9,
    0x4f, 0x9e, 0x21, 0x42, 0x84, 0x15, 0x2a, 0x54,
    0xa8, 0x4d, 0x9a, 0x29, 0x52, 0xa4, 0x55, 0xaa,
    0x49, 0x92, 0x39, 0x72, 0xe4, 0xd5, 0xb7, 0x73,
    0xe6, 0xd1, 0xbf, 0x63, 0xc6, 0x91, 0x3f, 0x7e,
    0xfc, 0xe5, 0xd7, 0xb3, 0x7b, 0xf6, 0xf1, 0xff,
    0xe3, 0xdb, 0xab, 0x4b, 0x96, 0x31, 0x62, 0xc4,
    0x95, 0x37, 0x6e, 0xdc, 0xa5, 0x57, 0xae, 0x41,
    0x82, 0x19, 0x32, 0x64, 0xc8, 0x8d, 0x07, 0x0e,
    0x1c, 0x38, 0x70, 0xe0, 0xdd, 0xa7, 0x53, 0xa6,
    0x51, 0xa2, 0x59, 0xb2, 0x79, 0xf2, 0xf9, 0xef,
    0xc3, 0x9b, 0x2b, 0x56, 0xac, 0x45, 0x8a, 0x09,
    0x12, 0x24, 0x48, 0x90, 0x3d, 0x7a, 0xf4, 0xf5,
    0xf7, 0xf3, 0xfb, 0xeb, 0xcb, 0x8b, 0x0b, 0x16,
    0x2c, 0x58, 0xb0, 0x7d, 0xfa, 0xe9, 0xcf, 0x83,
    0x1b, 0x36, 0x6c, 0xd8, 0xad, 0x47, 0x8e, 0x01
};
/* Logs of 2 in the Galois field defined above. */
const uint8_t vdev_raidz_log2[256] = {
    0x00, 0x00, 0x01, 0x19, 0x02, 0x32, 0x1a, 0xc6,
    0x03, 0xdf, 0x33, 0xee, 0x1b, 0x68, 0xc7, 0x4b,
    0x04, 0x64, 0xe0, 0x0e, 0x34, 0x8d, 0xef, 0x81,
    0x1c, 0xc1, 0x69, 0xf8, 0xc8, 0x08, 0x4c, 0x71,
    0x05, 0x8a, 0x65, 0x2f, 0xe1, 0x24, 0x0f, 0x21,
    0x35, 0x93, 0x8e, 0xda, 0xf0, 0x12, 0x82, 0x45,
    0x1d, 0xb5, 0xc2, 0x7d, 0x6a, 0x27, 0xf9, 0xb9,
    0xc9, 0x9a, 0x09, 0x78, 0x4d, 0xe4, 0x72, 0xa6,
    0x06, 0xbf, 0x8b, 0x62, 0x66, 0xdd, 0x30, 0xfd,
    0xe2, 0x98, 0x25, 0xb3, 0x10, 0x91, 0x22, 0x88,
    0x36, 0xd0, 0x94, 0xce, 0x8f, 0x96, 0xdb, 0xbd,
    0xf1, 0xd2, 0x13, 0x5c, 0x83, 0x38, 0x46, 0x40,
    0x1e, 0x42, 0xb6, 0xa3, 0xc3, 0x48, 0x7e, 0x6e,
    0x6b, 0x3a, 0x28, 0x54, 0xfa, 0x85, 0xba, 0x3d,
    0xca, 0x5e, 0x9b, 0x9f, 0x0a, 0x15, 0x79, 0x2b,
    0x4e, 0xd4, 0xe5, 0xac, 0x73, 0xf3, 0xa7, 0x57,
    0x07, 0x70, 0xc0, 0xf7, 0x8c, 0x80, 0x63, 0x0d,
    0x67, 0x4a, 0xde, 0xed, 0x31, 0xc5, 0xfe, 0x18,
    0xe3, 0xa5, 0x99, 0x77, 0x26, 0xb8, 0xb4, 0x7c,
    0x11, 0x44, 0x92, 0xd9, 0x23, 0x20, 0x89, 0x2e,
    0x37, 0x3f, 0xd1, 0x5b, 0x95, 0xbc, 0xcf, 0xcd,
    0x90, 0x87, 0x97, 0xb2, 0xdc, 0xfc, 0xbe, 0x61,
    0xf2, 0x56, 0xd3, 0xab, 0x14, 0x2a, 0x5d, 0x9e,
    0x84, 0x3c, 0x39, 0x53, 0x47, 0x6d, 0x41, 0xa2,
    0x1f, 0x2d, 0x43, 0xd8, 0xb7, 0x7b, 0xa4, 0x76,
    0xc4, 0x17, 0x49, 0xec, 0x7f, 0x0c, 0x6f, 0xf6,
    0x6c, 0xa1, 0x3b, 0x52, 0x29, 0x9d, 0x55, 0xaa,
    0xfb, 0x60, 0x86, 0xb1, 0xbb, 0xcc, 0x3e, 0x5a,
    0xcb, 0x59, 0x5f, 0xb0, 0x9c, 0xa9, 0xa0, 0x51,
    0x0b, 0xf5, 0x16, 0xeb, 0x7a, 0x75, 0x2c, 0xd7,
    0x4f, 0xae, 0xd5, 0xe9, 0xe6, 0xe7, 0xad, 0xe8,
    0x74, 0xd6, 0xf4, 0xea, 0xa8, 0x50, 0x58, 0xaf,
};

uint8_t
vdev_raidz_exp2(uint8_t a, int exp)
{
    if (a == 0)
        return (0);

    ASSERT(exp >= 0);
    ASSERT(vdev_raidz_log2[a] > 0 || a == 1);

    exp += vdev_raidz_log2[a];
    if (exp > 255)
        exp -= 255;

    return (vdev_raidz_pow2[exp]);
}

void
vdev_raidz_generate_parity_p(raidz_map_t *rm)
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
            for (i = 0; i < ccount; i++, src++, p++) {
                *p = *src;
            }
        } else {
            ASSERT(ccount <= pcount);
            for (i = 0; i < ccount; i++, src++, p++) {
                *p ^= *src;
            }
        }
    }
}

int
vdev_raidz_reconstruct_p(raidz_map_t *rm, int *tgts, int ntgts)
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
    for (i = 0; i < xcount; i++, dst++, src++) {
        *dst = *src;
    }

    for (c = rm->rm_firstdatacol; c < rm->rm_cols; c++) {
        src = rm->rm_col[c].rc_data;
        dst = rm->rm_col[x].rc_data;

        if (c == x)
            continue;

        ccount = rm->rm_col[c].rc_size / sizeof (src[0]);
        count = MIN(ccount, xcount);

        for (i = 0; i < count; i++, dst++, src++) {
            *dst ^= *src;
        }
    }

    return (1 << VDEV_RAIDZ_P);
}

void
vdev_raidz_generate_parity_pq(raidz_map_t *rm)
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
            for (i = 0; i < ccnt; i++, src++, p++, q++) {
                *p = *src;
                *q = *src;
            }
            for (; i < pcnt; i++, src++, p++, q++) {
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
vdev_raidz_reconstruct_pq(raidz_map_t *rm, int *tgts, int ntgts)
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

int
vdev_raidz_reconstruct_q(raidz_map_t *rm, int *tgts, int ntgts)
{
    uint64_t *dst, *src, xcount, ccount, count, mask, i;
    uint8_t *b;
    int x = tgts[0];
    int c, j, exp;

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
            for (i = 0; i < count; i++, dst++, src++) {
                *dst = *src;
            }
            for (; i < xcount; i++, dst++) {
                *dst = 0;
            }

        } else {
            for (i = 0; i < count; i++, dst++, src++) {
                VDEV_RAIDZ_64MUL_2(*dst, mask);
                *dst ^= *src;
            }

            for (; i < xcount; i++, dst++) {
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
vdev_raidz_generate_parity_pqr(raidz_map_t *rm)
{
    uint64_t *p, *q, *r, *src, pcnt, ccnt, mask, i;
    int c;

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
            for (i = 0; i < ccnt; i++, src++, p++, q++, r++) {
                *p = *src;
                *q = *src;
                *r = *src;
            }
            for (; i < pcnt; i++, src++, p++, q++, r++) {
                *p = 0;
                *q = 0;
                *r = 0;
            }
        } else {
            ASSERT(ccnt <= pcnt);

            /*
             * Apply the algorithm described above by multiplying
             * the previous result and adding in the new value.
             */
            for (i = 0; i < ccnt; i++, src++, p++, q++, r++) {
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
            for (; i < pcnt; i++, q++, r++) {
                VDEV_RAIDZ_64MUL_2(*q, mask);
                VDEV_RAIDZ_64MUL_4(*r, mask);
            }
        }
    }
}
