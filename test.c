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

void test_parity_p(parity p, raidz_map_t *map);
void test_parity_pq(parity p, raidz_map_t *map);

int main(int argc, char **argv)
{
    int seconds = 0;
    size_t num_cols = argc - 2;
    size_t *sizes = malloc(sizeof(size_t) * num_cols);
    for (int i = 1; i < argc - 1; i++) {
        sizes[i-1] = atoi(argv[i]);
    }
    seconds = atoi(argv[argc-1]);
    if (seconds == 0) {
        is_profiling = 0;
    }
    parity avx = {"RAID-Z1 AVX",
                  vdev_raidz_generate_parity_p_avx,
                  vdev_raidz_reconstruct_p_avx};
    parity standard = {"RAID-Z1 Standard",
                       vdev_raidz_generate_parity_p,
                       vdev_raidz_reconstruct_p};
    parity standard_pq = {"RAID-Z2 Standard",
                       vdev_raidz_generate_parity_pq,
                       vdev_raidz_reconstruct_pq};
    parity avx_pq = {"RAID-Z2 AVX",
                       vdev_raidz_generate_parity_pq_avx,
                       vdev_raidz_reconstruct_pq_avx};

    parity parities[] = {standard_pq, avx_pq};
    int type = VDEV_RAIDZ_Q;
    time_t start = time(NULL);
    do {
        raidz_map_t *map = make_map(num_cols, sizes, type);

        for(int i = 0; i < sizeof(parities) / sizeof(parity); i++) {
            parity p = parities[i];
            TEST_PRINT("Testing %s\n", p.name);
            test_parity_pq(p, map);
            TEST_PRINT("%s works!\n", p.name);
        }
        raidz_map_free(map);
    } while (difftime(time(NULL), start) < seconds);
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
