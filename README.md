Vectorized RAIDZ
================

This is a test suite for vectorized versions of ZFS RAIDZ functions,
designed to run in userspace.

When running the test suite, the first parameter is how many seconds you
want the test to run for; use 0 seconds if you simply want to test the code,
but use a longer time if you plan to profile. Print statements are turned off
when profiling (time > 0). The remaining parameters are the column sizes for
the raidz_map_t, measured in 64bit blocks. The actual contents of the columns
are randomly generated on each iteration. For example, if you wanted a two
minute test on maps with 4 columns, 1000 blocks each, you would run:

    ./test 120 1000 1000 1000 1000
