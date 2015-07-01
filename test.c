#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>

#include "vdev_raidz.h"

raidz_col_t make_col(uint64_t *data, size_t col_entries);
raidz_map_t *make_map(uint64_t **data, size_t num_cols, size_t *sizes, int type);

int main()
{
    uint64_t test[] = {5, 3, 2, 5, 1, 47};
    uint64_t test2[] = {0, 4, 2, 1};
    uint64_t test3[] = {0, 4, 2, 1};
    size_t sizes[] = {6, 4, 4};
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
    return 0;
}

raidz_col_t make_col(uint64_t *data, size_t num_entries)
{
    raidz_col_t col;
    col.rc_size = num_entries * sizeof(uint64_t);
    col.rc_data = malloc(col.rc_size);
    uint64_t *ptr = col.rc_data;
    for(int i = 0; i < num_entries; i++) {
        ptr[i] = data[i];
    }
    return col;
}

raidz_map_t *make_map(uint64_t **data, size_t num_cols, size_t *sizes, int type)
{
    raidz_col_t *cols = malloc(num_cols * sizeof(raidz_col_t));
    for(int i = 0; i < num_cols; i++) {
        raidz_col_t col = make_col(data[i], sizes[i]);
        cols[i] = col;
    }
    raidz_map_t *map = malloc(sizeof(raidz_map_t) +
                              (num_cols + type) * sizeof(raidz_col_t));
    map->rm_firstdatacol = type + 1;
    map->rm_cols = map->rm_firstdatacol + num_cols;
    for(int i = map->rm_firstdatacol; i < map->rm_cols; i++, cols++) {
        map->rm_col[i] = *cols;
    }
    return map;
}
