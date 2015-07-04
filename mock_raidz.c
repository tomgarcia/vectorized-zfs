#include <stdio.h>
#include <stdlib.h>

#include "mock_raidz.h"
#include "vdev_raidz.h"

raidz_map_t *make_map(size_t num_cols, size_t *sizes, int type)
{
    raidz_map_t *map = malloc(sizeof(raidz_map_t) +
                              (num_cols + type) * sizeof(raidz_col_t));
    map->rm_firstdatacol = type + 1;
    map->rm_cols = map->rm_firstdatacol + num_cols;
    for(int i = map->rm_firstdatacol; i < map->rm_cols; i++) {
        map->rm_col[i] = make_col(sizes[i - map->rm_firstdatacol]);
    }
    for(int i = 0; i < map->rm_firstdatacol; i++) {
        map->rm_col[i].rc_size = map->rm_col[map->rm_firstdatacol].rc_size;
        map->rm_col[i].rc_data = malloc(map->rm_col[i].rc_size);
    }
    return map;
}

raidz_col_t make_col(size_t num_entries)
{
    raidz_col_t col;
    col.rc_size = num_entries * sizeof(uint64_t);
    col.rc_data = malloc(col.rc_size);
    FILE *random = fopen("/dev/urandom", "r");
    fread(col.rc_data, sizeof(uint64_t), num_entries, random);
    fclose(random);
    return col;
}

void raidz_map_free(raidz_map_t *map)
{
    for(int i = 0; i < map->rm_cols; i++) {
        free(map->rm_col[i].rc_data);
    }
    free(map);
}
