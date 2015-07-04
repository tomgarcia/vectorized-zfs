#ifndef _VDEV_RAIDZ_H
#define _VDEV_RAIDZ_H

#include <stdint.h>
#include <assert.h>

#define VDEV_RAIDZ_P        0
#define VDEV_RAIDZ_Q        1
#define VDEV_RAIDZ_R        2

#define ASSERT assert
#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef struct raidz_col {
    uint64_t rc_devidx;     /* child device index for I/O */
    uint64_t rc_offset;     /* device offset */
    uint64_t rc_size;       /* I/O size */
    void *rc_data;          /* I/O data */
    void *rc_gdata;         /* used to store the "good" version */
    int rc_error;           /* I/O error for this device */
    uint8_t rc_tried;       /* Did we attempt this I/O column? */
    uint8_t rc_skipped;     /* Did we skip this I/O column? */
} raidz_col_t;

typedef struct raidz_map {
    uint64_t rm_cols;       /* Regular column count */
    uint64_t rm_scols;      /* Count including skipped columns */
    uint64_t rm_bigcols;        /* Number of oversized columns */
    uint64_t rm_asize;      /* Actual total I/O size */
    uint64_t rm_missingdata;    /* Count of missing data devices */
    uint64_t rm_missingparity;  /* Count of missing parity devices */
    uint64_t rm_firstdatacol;   /* First data column/parity count */
    uint64_t rm_nskip;      /* Skipped sectors for padding */
    uint64_t rm_skipstart;      /* Column index of padding start */
    void *rm_datacopy;      /* rm_asize-buffer of copied data */
    uintptr_t rm_reports;       /* # of referencing checksum reports */
    uint8_t rm_freed;       /* map no longer has referencing ZIO */
    uint8_t rm_ecksuminjected;  /* checksum error was injected */
    raidz_col_t rm_col[1];      /* Flexible array of I/O columns */
} raidz_map_t;

void vdev_raidz_generate_parity_p(raidz_map_t *rm);
int vdev_raidz_reconstruct_p(raidz_map_t *rm, int *tgts, int ntgts);

void vdev_raidz_generate_parity_pq(raidz_map_t *rm);
int vdev_raidz_reconstruct_pq(raidz_map_t *rm, int *tgts, int ntgts);

void vdev_raidz_generate_parity_p_avx(raidz_map_t *rm);
int vdev_raidz_reconstruct_p_avx(raidz_map_t *rm, int *tgts, int ntgts);

void vdev_raidz_generate_parity_p_avx_v2(raidz_map_t *rm);
int vdev_raidz_reconstruct_p_avx_v2(raidz_map_t *rm, int *tgts, int ntgts);
#endif
