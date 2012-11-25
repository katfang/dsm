CC=gcc

all: dsm_test.o master.o

dsm_test.o: dsm_test.c libdsm.so
	$(CC) dsm_test.c -o dsm_test -L. -ldsm -lrt

libdsm.o: libdsm.c libdsm.h
	$(CC) -Wall -fPIC -DPIC -c libdsm.c

libdsm.so: libdsm.o
	ld -shared -o libdsm.so libdsm.o -ldl

master.o: master.c sender.h messages.h
	gcc master.c -o master -lpthread

clean:
	rm -f *.o *.so dsm_test /dev/shm/blah
