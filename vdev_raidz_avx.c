#include <stdint.h>
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
                asm("VMOVDQU %1, %%ymm0\n"
                    "VMOVDQU %%ymm0, %0"
                    : "=m" (*p)
                    : "m" (*src)
                    : "ymm0");
            }
            int remainder = ccount % 4;
            if (remainder != 0) {
                src -= (4 - remainder);
                p -= (4 - remainder);
                asm("VMOVDQU %1, %%ymm0\n"
                    "VMOVDQU %%ymm0, %0"
                    : "=m" (*p)
                    : "m" (*src)
                    : "ymm0");
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
            if (remainder != 0) {
                src -= (4 - remainder);
                p -= (4 - remainder);
                asm("VMOVDQU %0, %%ymm0\n"
                    "VMOVDQU %1, %%ymm1\n"
                    "VXORPS %%ymm1, %%ymm0, %%ymm0\n"
                    "VMOVDQU %%ymm0, %0"
                    : "+m" (*p)
                    : "m" (*src)
                    : "ymm0", "ymm1");
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
