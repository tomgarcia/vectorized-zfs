CFLAGS = -Wall -g -mavx
LDFLAGS = 

all: test

test: test.o vdev_raidz.o mock_raidz.o vdev_raidz_avx.o
	gcc $(CFLAGS) $(LDFLAGS) test.o vdev_raidz.o mock_raidz.o vdev_raidz_avx.o -o test

test.o: test.c vdev_raidz.h mock_raidz.h
	gcc $(CFLAGS) -c test.c

vdev_raidz.o: vdev_raidz.c vdev_raidz.h
	gcc $(CFLAGS) -c vdev_raidz.c

mock_raidz.o: mock_raidz.c mock_raidz.h
	gcc $(CFLAGS) -c mock_raidz.c

vdev_raidz_avx.o: vdev_raidz_avx.c vdev_raidz.h
	gcc $(CFLAGS) -c vdev_raidz_avx.c

clean:
	rm test *.o
