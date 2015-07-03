#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
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
    uint64_t test[] = {400, 5, 3, 2, 5, 6, 5, 2, 1};
    uint64_t test2[] = {1, 4, 2, 1, 8, 3, 6, 9};
    uint64_t test3[] = {2, 200, 5, 6, 400, 37, 29, 606};
    size_t sizes[] = {9, 8, 8};
    uint64_t *input[] = {test, test2, test3};
    int type = VDEV_RAIDZ_P;
    size_t num_cols = sizeof(input) / sizeof(uint64_t*);
    raidz_map_t *map = make_map(input, num_cols, sizes, type);

    uint64_t **input_col = input;
    assert(map->rm_cols == num_cols + (type + 1));
    for(int i = map->rm_firstdatacol; i < map->rm_cols; i++, input_col++) {
        raidz_col_t col = map->rm_col[i];
        uint64_t *output_col = col.rc_data;
        for(int j = 0; j < col.rc_size / sizeof(uint64_t); j++) {
            assert(output_col[j] == (*input_col)[j]);
        }
    }

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
        free(col.rc_data);
        col.rc_data = malloc(col.rc_size);
        int targets[] = {i};
        r(map, targets, 1);
        for(int j = 0; j < col.rc_size / sizeof(uint64_t); j++) {
            assert(copy[j] == ((uint64_t*)col.rc_data)[j]);
        }
    }
}
