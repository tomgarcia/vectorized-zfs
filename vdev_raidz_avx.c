#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>

#include "vdev_raidz.h"

void
vdev_raidz_generate_parity_p_avx(raidz_map_t *rm)
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
            for (i = 0; i < ccount / 16; i++, src+=16, p+=16) {
                asm("VMOVDQU (%[src]), %%ymm0\n"
                    "VMOVDQU %%ymm0, (%[p])\n"
                    "VMOVDQU 32(%[src]), %%ymm1\n"
                    "VMOVDQU %%ymm1, 32(%[p])\n"
                    "VMOVDQU 64(%[src]), %%ymm2\n"
                    "VMOVDQU %%ymm2, 64(%[p])\n"
                    "VMOVDQU 96(%[src]), %%ymm3\n"
                    "VMOVDQU %%ymm3, 96(%[p])\n"
                    :
                    : [p] "r" (p), [src] "r" (src)
                    : "ymm0", "ymm1", "ymm2", "ymm3", "memory");
            }
            remainder = ccount % 16;
            for(i = 0; i < remainder / 4; i++, p+=4, src+=4) {
                asm("VMOVDQU (%[src]), %%ymm0\n"
                    "VMOVDQU %%ymm0, (%[p])\n"
                    :
                    : [p] "r" (p), [src] "r" (src)
                    : "ymm0", "memory");
            }
            remainder %= 4;
            for(i = 0; i < remainder; i++, p++, src++) {
                *p = *src;
            }
        } else {
            ASSERT(ccount <= pcount);
            for (i = 0; i < ccount / 16; i++, src+=16, p+=16) {
                asm("VMOVDQU (%[src]), %%ymm0\n"
                    "VMOVDQU (%[p]), %%ymm1\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[p])\n"
                    "VMOVDQU 32(%[src]), %%ymm2\n"
                    "VMOVDQU 32(%[p]), %%ymm3\n"
                    "VXORPS %%ymm2, %%ymm3, %%ymm3\n"
                    "VMOVDQU %%ymm3, 32(%[p])\n"
                    "VMOVDQU 64(%[src]), %%ymm4\n"
                    "VMOVDQU 64(%[p]), %%ymm5\n"
                    "VXORPS %%ymm4, %%ymm5, %%ymm5\n"
                    "VMOVDQU %%ymm5, 64(%[p])\n"
                    "VMOVDQU 96(%[src]), %%ymm6\n"
                    "VMOVDQU 96(%[p]), %%ymm7\n"
                    "VXORPS %%ymm6, %%ymm7, %%ymm7\n"
                    "VMOVDQU %%ymm7, 96(%[p])\n"
                    :
                    : [p] "r" (p), [src] "r" (src)
                    : "ymm0", "ymm1", "ymm2", "ymm3",
                      "ymm4", "ymm5", "ymm6", "ymm7", "memory");
            }
            remainder = ccount % 16;
            for(i = 0; i < remainder / 4; i++, p+=4, src+=4) {
                asm("VMOVDQU (%[src]), %%ymm0\n"
                    "VMOVDQU (%[p]), %%ymm1\n"
                    "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                    "VMOVDQU %%ymm1, (%[p])\n"
                    :
                    : [p] "r" (p), [src] "r" (src)
                    : "ymm0", "ymm1", "memory");
            }
            remainder %= 4;
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
    int c, remainder;

    ASSERT(ntgts == 1);
    ASSERT(x >= rm->rm_firstdatacol);
    ASSERT(x < rm->rm_cols);

    xcount = rm->rm_col[x].rc_size / sizeof (src[0]);
    ASSERT(xcount <= rm->rm_col[VDEV_RAIDZ_P].rc_size / sizeof (src[0]));
    ASSERT(xcount > 0);

    src = rm->rm_col[VDEV_RAIDZ_P].rc_data;
    dst = rm->rm_col[x].rc_data;
    for (i = 0; i < xcount / 16; i++, src+=16, dst+=16) {
        asm("VMOVDQU (%[src]), %%ymm0\n"
            "VMOVDQU %%ymm0, (%[dst])\n"
            "VMOVDQU 32(%[src]), %%ymm1\n"
            "VMOVDQU %%ymm1, 32(%[dst])\n"
            "VMOVDQU 64(%[src]), %%ymm2\n"
            "VMOVDQU %%ymm2, 64(%[dst])\n"
            "VMOVDQU 96(%[src]), %%ymm3\n"
            "VMOVDQU %%ymm3, 96(%[dst])"
            :
            : [dst] "r" (dst), [src] "r" (src)
            : "ymm0", "ymm1", "ymm2", "ymm3", "memory");
    }
    remainder = xcount % 16;
    for (i = 0; i < remainder / 4; i++, src+=4, dst+=4) {
        asm("VMOVDQU (%[src]), %%ymm0\n"
            "VMOVDQU %%ymm0, (%[dst])"
            :
            : [dst] "r" (dst), [src] "r" (src)
            : "ymm0", "memory");
    }
    remainder %= 4;
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

        for (i = 0; i < count / 16; i++, src+=16, dst+=16) {
            asm("VMOVDQU (%[src]), %%ymm0\n"
                "VMOVDQU (%[dst]), %%ymm1\n"
                "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                "VMOVDQU %%ymm1, (%[dst])\n"
                "VMOVDQU 32(%[src]), %%ymm2\n"
                "VMOVDQU 32(%[dst]), %%ymm3\n"
                "VXORPS %%ymm2, %%ymm3, %%ymm3\n"
                "VMOVDQU %%ymm3, 32(%[dst])\n"
                "VMOVDQU 64(%[src]), %%ymm4\n"
                "VMOVDQU 64(%[dst]), %%ymm5\n"
                "VXORPS %%ymm4, %%ymm5, %%ymm5\n"
                "VMOVDQU %%ymm5, 64(%[dst])\n"
                "VMOVDQU 96(%[src]), %%ymm6\n"
                "VMOVDQU 96(%[dst]), %%ymm7\n"
                "VXORPS %%ymm6, %%ymm7, %%ymm7\n"
                "VMOVDQU %%ymm7, 96(%[dst])\n"
                :
                : [dst] "r" (dst), [src] "r" (src)
                : "ymm0", "ymm1", "ymm2", "ymm3",
                  "ymm4", "ymm5", "ymm6", "ymm7", "memory");
        }
        remainder = count % 16;
        for (i = 0; i < remainder / 4; i++, src+=4, dst+=4) {
            asm("VMOVDQU (%[src]), %%ymm0\n"
                "VMOVDQU (%[dst]), %%ymm1\n"
                "VXORPS %%ymm0, %%ymm1, %%ymm1\n"
                "VMOVDQU %%ymm1, (%[dst])"
                :
                : [dst] "r" (dst), [src] "r" (src)
                : "ymm0", "ymm1");
        }
        remainder %= 4;
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
                for (i = 0; i < ccnt / 16; i++, src+=16, p+=16, q+=16) {
                    asm("VMOVDQU (%[src]), %%ymm0\n"
                        "VMOVDQU %%ymm0, (%[p])\n"
                        "VMOVDQU %%ymm0, (%[q])\n"
                        "VMOVDQU 32(%[src]), %%ymm1\n"
                        "VMOVDQU %%ymm1, 32(%[p])\n"
                        "VMOVDQU %%ymm1, 32(%[q])\n"
                        "VMOVDQU 64(%[src]), %%ymm2\n"
                        "VMOVDQU %%ymm2, 64(%[p])\n"
                        "VMOVDQU %%ymm2, 64(%[q])\n"
                        "VMOVDQU 96(%[src]), %%ymm3\n"
                        "VMOVDQU %%ymm3, 96(%[p])\n"
                        "VMOVDQU %%ymm3, 96(%[q])\n"
                        :
                        : [p] "r" (p), [q] "r" (q), [src] "r" (src)
                        : "ymm0", "ymm1", "ymm2", "ymm3", "memory");
                }
                remainder = ccnt % 16;
                for (i = 0; i < remainder / 4; i++, src+=4, p+=4, q+=4) {
                    asm("VMOVDQU (%[src]), %%ymm0\n"
                        "VMOVDQU %%ymm0, (%[p])\n"
                        "VMOVDQU %%ymm0, (%[q])"
                        :
                        : [p] "r" (p), [q] "r" (q), [src] "r" (src)
                        : "ymm0", "memory");
                }
                remainder %= 4;
                for(i = 0; i < remainder; i++, p++, src++, q++) {
                    *p = *src;
                    *q = *src;
                }
            } else {
                for (i = 0; i < pcnt / 16; i++, p+=16, q+=16) {
                    asm("VXORPS %%ymm0, %%ymm0, %%ymm0\n"
                        "VMOVDQU %%ymm0, (%[p])\n"
                        "VMOVDQU %%ymm0, (%[q])\n"
                        "VMOVDQU %%ymm0, 32(%[p])\n"
                        "VMOVDQU %%ymm0, 32(%[q])\n"
                        "VMOVDQU %%ymm0, 64(%[p])\n"
                        "VMOVDQU %%ymm0, 64(%[q])\n"
                        "VMOVDQU %%ymm0, 96(%[p])\n"
                        "VMOVDQU %%ymm0, 96(%[q])\n"
                        :
                        : [p] "r" (p), [q] "r" (q)
                        : "ymm0", "memory");
                }
                remainder = pcnt % 16;
                for (i = 0; i < remainder / 4; i++, p+=4, q+=4) {
                    asm("VXORPS %%ymm0, %%ymm0, %%ymm0\n"
                        "VMOVDQU %%ymm0, (%[p])\n"
                        "VMOVDQU %%ymm0, (%[q])"
                        :
                        : [p] "r" (p), [q] "r" (q)
                        : "ymm0", "memory");
                }
                remainder %= 4;
                for(i = 0; i < remainder; i++, p++, q++) {
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
            for (i = 0; i < ccnt / 16; i++, src+=16, p+=16, q+=16) {
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
                    :
                    : [p] "r" (p), [q] "r" (q), [src] "r" (src)
                    : "rax", "ymm0", "ymm1", "xmm2",
                      "xmm3", "ymm4", "xmm5", "xmm6", "memory");
            }
            remainder = ccnt % 16;
            for (i = 0; i < remainder / 4; i++, src+=4, p+=4, q+=4) {
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
                    "VMOVDQU %%ymm1, (%[q])"
                    :
                    : [p] "r" (p), [q] "r" (q), [src] "r" (src)
                    : "rax", "ymm0", "ymm1", "xmm2",
                      "xmm3", "ymm4", "xmm5", "memory");
            }
            remainder %= 4;
            for(i = 0; i < remainder; i++, p++, src++, q++) {
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

    vdev_raidz_generate_parity_pq_avx(rm);

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
vdev_raidz_reconstruct_q_avx(raidz_map_t *rm, int *tgts, int ntgts)
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
