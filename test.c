#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#include "vdev_raidz.h"
#include "mock_raidz.h"

typedef void (*generator)(raidz_map_t *rm);
typedef int (*reconstructor)(raidz_map_t *rm, int *tgts, int ntgts);

void test_parity(generator g, reconstructor r, raidz_map_t *map);

int main()
{
    // All columns must be <= the first column
    // (ask ZFS maintainers why)
    size_t sizes[] = {9, 9, 9};
    int type = VDEV_RAIDZ_P;
    size_t num_cols = sizeof(sizes) / sizeof(size_t);
    raidz_map_t *map = make_map(num_cols, sizes, type);

    test_parity(vdev_raidz_generate_parity_p,
                vdev_raidz_reconstruct_p,
                map);
    test_parity(vdev_raidz_generate_parity_p_avx,
                vdev_raidz_reconstruct_p_avx,
                map);
    return 0;
}

void test_parity(generator g, reconstructor r, raidz_map_t *map)
{
    g(map);
    for(int i = map->rm_firstdatacol; i < map->rm_cols; i++) {
        raidz_col_t col = map->rm_col[i];
        uint64_t *copy = malloc(col.rc_size);
        for(int j = 0; j < col.rc_size / sizeof(uint64_t); j++) {
            copy[j] = ((uint64_t*)col.rc_data)[j];
        }
        memset(col.rc_data, 0, col.rc_size);
        int targets[] = {i};
        r(map, targets, 1);
        for(int j = 0; j < col.rc_size / sizeof(uint64_t); j++) {
            assert(copy[j] == ((uint64_t*)col.rc_data)[j]);
        }
        free(copy);
    }
}
