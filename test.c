#include <stdio.h>
#include <time.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "vdev_raidz.h"
#include "mock_raidz.h"

#define TEST_PRINT(s) if (!is_profiling) { \
                           puts((s));\
                       }

static char is_profiling = 1;

typedef void (*generator)(raidz_map_t *rm);
typedef int (*reconstructor)(raidz_map_t *rm, int *tgts, int ntgts);

void test_parity(generator g, reconstructor r, raidz_map_t *map);

int main(int argc, char **argv)
{
    int seconds = 0;
    if (argc > 1) {
        seconds = atoi(argv[1]);
    }
    if (seconds == 0) {
        is_profiling = 0;
    }
    time_t start = time(NULL);
    do {
        // All columns must be <= the first column
        // (ask ZFS maintainers why)
        size_t sizes[] = {1000, 1000, 1000};
        int type = VDEV_RAIDZ_P;
        size_t num_cols = sizeof(sizes) / sizeof(size_t);
        raidz_map_t *map = make_map(num_cols, sizes, type);

        TEST_PRINT("Testing standard RAID-Z1");
        test_parity(vdev_raidz_generate_parity_p,
                    vdev_raidz_reconstruct_p,
                    map);
        TEST_PRINT("Standard RAID-Z1 works!");
        TEST_PRINT("Testing AVX RAID-Z1");
        test_parity(vdev_raidz_generate_parity_p_avx,
                    vdev_raidz_reconstruct_p_avx,
                    map);
        TEST_PRINT("AVX RAID-Z1 works!");
        raidz_map_free(map);
    } while (difftime(time(NULL), start) < seconds);
    return 0;
}

void test_parity(generator g, reconstructor r, raidz_map_t *map)
{
    g(map);
    for(int i = map->rm_firstdatacol; i < map->rm_cols; i++) {
        raidz_col_t col = map->rm_col[i];
        uint64_t *copy = malloc(col.rc_size);
        memcpy(copy, col.rc_data, col.rc_size);
        memset(col.rc_data, 0, col.rc_size);
        int targets[] = {i};
        r(map, targets, 1);
        for(int j = 0; j < col.rc_size / sizeof(uint64_t); j++) {
            assert(copy[j] == ((uint64_t*)col.rc_data)[j]);
        }
        free(copy);
    }
}
