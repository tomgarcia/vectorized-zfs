CFLAGS = -Wall -g
LDFLAGS = 

all: test vdev_raidz_sse4.o

test: test.o vdev_raidz.o mock_raidz.o vdev_raidz_avx.o vdev_raidz_sse4.o
	gcc $(CFLAGS) $(LDFLAGS) test.o vdev_raidz.o mock_raidz.o vdev_raidz_avx.o vdev_raidz_sse4.o -o test

test.o: test.c vdev_raidz.h mock_raidz.h
	gcc $(CFLAGS) -c test.c

vdev_raidz.o: vdev_raidz.c vdev_raidz.h
	gcc $(CFLAGS) -c vdev_raidz.c

mock_raidz.o: mock_raidz.c mock_raidz.h
	gcc $(CFLAGS) -c mock_raidz.c

vdev_raidz_avx.o: vdev_raidz_avx.c vdev_raidz.h
	gcc $(CFLAGS) -c vdev_raidz_avx.c

vdev_raidz_sse4.o: vdev_raidz_sse4.c vdev_raidz.h
	gcc $(CFLAGS) -c vdev_raidz_sse4.c

clean:
	rm test *.o
