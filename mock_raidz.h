#ifndef _MOCK_RAIDZ
#define _MOCK_RAIDZ

#include <stdint.h>
#include <stddef.h>

#include "vdev_raidz.h"

raidz_col_t make_col(size_t col_entries);
raidz_map_t *make_map(size_t num_cols, size_t *sizes, int type);

#endif
