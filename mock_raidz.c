#include <stdlib.h>

#include "mock_raidz.h"
#include "vdev_raidz.h"

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
    map->rm_col[VDEV_RAIDZ_P].rc_size = map->rm_col[map->rm_firstdatacol].rc_size;
    map->rm_col[VDEV_RAIDZ_P].rc_data = malloc(map->rm_col[VDEV_RAIDZ_P].rc_size);
    return map;
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
