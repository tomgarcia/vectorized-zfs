#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "vdev_raidz.h"
#include "mock_raidz.h"

#define TEST_PRINT(s...) if (!is_profiling) { \
                           printf(s);\
                       }

static char is_profiling = 1;

typedef struct {
    char *name;
    void (*generate)(raidz_map_t *rm);
    int (*reconstruct)(raidz_map_t *rm, int *tgts, int ntgts);
} parity;

typedef void (*generate_pqr)(raidz_map_t *rm);

void test_parity_p(parity p, raidz_map_t *map);
void test_reconstruct_q(parity p, raidz_map_t *map);
void test_parity_pq(parity p, raidz_map_t *map);
void test_parity_pqr(generate_pqr generate, raidz_map_t *map);

int main(int argc, char **argv)
{
    int seconds = atoi(argv[1]);
    if (seconds == 0) {
        is_profiling = 0;
    }
    size_t num_cols = argc - 2;
    size_t *sizes = malloc(sizeof(size_t) * num_cols);
    for (int i = 2; i < argc; i++) {
        sizes[i-2] = atoi(argv[i]);
    }
    parity avx = {"RAID-Z1 AVX",
                  vdev_raidz_generate_parity_p_avx,
                  vdev_raidz_reconstruct_p_avx};
    parity sse4 = {"RAID-Z1 SSE4",
                  vdev_raidz_generate_parity_p_sse4,
                  vdev_raidz_reconstruct_p_sse4};
    parity standard = {"RAID-Z1 Standard",
                       vdev_raidz_generate_parity_p,
                       vdev_raidz_reconstruct_p};
    parity standard_pq = {"RAID-Z2 Standard",
                       vdev_raidz_generate_parity_pq,
                       vdev_raidz_reconstruct_pq};
    parity avx_pq = {"RAID-Z2 AVX",
                       vdev_raidz_generate_parity_pq_avx,
                       vdev_raidz_reconstruct_pq_avx};
    parity sse4_pq = {"RAID-Z2 SSE4",
                       vdev_raidz_generate_parity_pq_sse4,
                       vdev_raidz_reconstruct_pq_sse4};
    parity standard_q = {"RAID-Z2 Standard (Q)",
                       vdev_raidz_generate_parity_pq,
                       vdev_raidz_reconstruct_q};
    parity avx_q = {"RAID-Z2 AVX (Q)",
                       vdev_raidz_generate_parity_pq_avx,
                       vdev_raidz_reconstruct_q_avx};
    parity sse4_q = {"RAID-Z2 SSE4 (Q)",
                       vdev_raidz_generate_parity_pq_sse4,
                       vdev_raidz_reconstruct_q_sse4};

    parity parities_p[] = {standard, avx, sse4};
    parity parities_pq[] = {standard_pq, avx_pq, sse4_pq};
    parity parities_q[] = {standard_q, avx_q, sse4_q};
    time_t start = time(NULL);
    do {
        raidz_map_t *map_p = make_map(num_cols, sizes, VDEV_RAIDZ_P);
        raidz_map_t *map_pq = make_map(num_cols, sizes, VDEV_RAIDZ_Q);
        raidz_map_t *map_pqr = make_map(num_cols, sizes, VDEV_RAIDZ_R);

        for(int i = 0; i < sizeof(parities_p) / sizeof(parity); i++) {
            parity p = parities_p[i];
            TEST_PRINT("Testing %s\n", p.name);
            test_parity_p(p, map_p);
            TEST_PRINT("%s works!\n", p.name);
        }
        TEST_PRINT("\n");
        for(int i = 0; i < sizeof(parities_pq) / sizeof(parity); i++) {
            parity p = parities_pq[i];
            TEST_PRINT("Testing %s\n", p.name);
            test_parity_pq(p, map_pq);
            TEST_PRINT("%s works!\n", p.name);
        }
        TEST_PRINT("\n");
        for(int i = 0; i < sizeof(parities_q) / sizeof(parity); i++) {
            parity p = parities_q[i];
            TEST_PRINT("Testing %s\n", p.name);
            test_reconstruct_q(p, map_pq);
            TEST_PRINT("%s works!\n", p.name);
        }
        TEST_PRINT("\n");
        TEST_PRINT("Testing RAIDZ3 AVX Generation\n");
        test_parity_pqr(vdev_raidz_generate_parity_pqr_avx, map_pqr);
        TEST_PRINT("RAIDZ3 AVX Generation Works!\n");
        TEST_PRINT("Testing RAIDZ3 SSE4 Generation\n");
        test_parity_pqr(vdev_raidz_generate_parity_pqr_sse4, map_pqr);
        TEST_PRINT("RAIDZ3 SSE4 Generation Works!\n");
        raidz_map_free(map_p);
        raidz_map_free(map_pq);
        raidz_map_free(map_pqr);
    } while (difftime(time(NULL), start) < seconds);
    free(sizes);
    return 0;
}

void test_parity_p(parity p, raidz_map_t *map)
{
    p.generate(map);
    for(int i = map->rm_firstdatacol; i < map->rm_cols; i++) {
        raidz_col_t col = map->rm_col[i];
        uint64_t *copy = malloc(col.rc_size);
        memcpy(copy, col.rc_data, col.rc_size);
        memset(col.rc_data, 0, col.rc_size);
        int targets[] = {i};
        p.reconstruct(map, targets, 1);
        for(int j = 0; j < col.rc_size / sizeof(uint64_t); j++) {
            assert(copy[j] == ((uint64_t*)col.rc_data)[j]);
        }
        free(copy);
    }
}

void test_reconstruct_q(parity p, raidz_map_t *map)
{
    p.generate(map);
    for(int i = map->rm_firstdatacol; i < map->rm_cols; i++) {
        raidz_col_t col = map->rm_col[i];
        uint64_t *copy = malloc(col.rc_size);
        memcpy(copy, col.rc_data, col.rc_size);
        memset(col.rc_data, 0, col.rc_size);
        int targets[] = {i};
        p.reconstruct(map, targets, 1);
        for(int j = 0; j < col.rc_size / sizeof(uint64_t); j++) {
            assert(copy[j] == ((uint64_t*)col.rc_data)[j]);
        }
        free(copy);
    }
}

void test_parity_pq(parity p, raidz_map_t *map)
{
    p.generate(map);
    for(int i = map->rm_firstdatacol; i < map->rm_cols; i++) {
        for(int j = map->rm_firstdatacol; j < i; j++) {
            raidz_col_t col1 = map->rm_col[i];
            uint64_t *copy1 = malloc(col1.rc_size);
            memcpy(copy1, col1.rc_data, col1.rc_size);
            memset(col1.rc_data, 0, col1.rc_size);
            raidz_col_t col2 = map->rm_col[j];
            uint64_t *copy2 = malloc(col2.rc_size);
            memcpy(copy2, col2.rc_data, col2.rc_size);
            memset(col2.rc_data, 0, col2.rc_size);
            int targets[] = {j, i};
            p.reconstruct(map, targets, 2);
            for(int k = 0; k < col1.rc_size / sizeof(uint64_t); k++) {
                assert(copy1[k] == ((uint64_t*)col1.rc_data)[k]);
            }
            for(int k = 0; k < col2.rc_size / sizeof(uint64_t); k++) {
                assert(copy2[k] == ((uint64_t*)col2.rc_data)[k]);
            }
            free(copy1);
            free(copy2);
        }
    }
}

void test_parity_pqr(generate_pqr generate, raidz_map_t *map)
{
    vdev_raidz_generate_parity_pqr(map);
    uint64_t *p = (uint64_t *) map->rm_col[VDEV_RAIDZ_P].rc_data;
    uint64_t psize = map->rm_col[VDEV_RAIDZ_P].rc_size;
    uint64_t *pcopy = malloc(psize);
    memcpy(pcopy, p, psize);
    uint64_t *q = (uint64_t *) map->rm_col[VDEV_RAIDZ_Q].rc_data;
    uint64_t qsize = map->rm_col[VDEV_RAIDZ_Q].rc_size;
    uint64_t *qcopy = malloc(qsize);
    memcpy(qcopy, q, qsize);
    uint64_t *r = (uint64_t *) map->rm_col[VDEV_RAIDZ_R].rc_data;
    uint64_t rsize = map->rm_col[VDEV_RAIDZ_R].rc_size;
    uint64_t *rcopy = malloc(rsize);
    memcpy(rcopy, r, rsize);
    generate(map);
    for(int i = 0; i < psize / sizeof(uint64_t); i++) {
        assert(p[i] == pcopy[i]);
    }
    for(int i = 0; i < rsize / sizeof(uint64_t); i++) {
        assert(r[i] == rcopy[i]);
    }
    for(int i = 0; i < qsize / sizeof(uint64_t); i++) {
        assert(q[i] == qcopy[i]);
    }
    free(pcopy);
    free(qcopy);
    free(rcopy);
}
