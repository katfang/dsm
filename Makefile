CC=gcc

all: dsm_test.o master 

test: all test_sender

master: pagedata.o sender.o master.o 
	$(CC) pagedata.o sender.o master.o -o master -lpthread

test_sender: test_sender.c sender.o sender.h messages.h
	$(CC) sender.o test_sender.c -o test_sender

dsm_test.o: dsm_test.c libdsm.so
	$(CC) dsm_test.c -o dsm_test -L. -ldsm -lrt

libdsm.o: libdsm.c libdsm.h
	$(CC) -Wall -fPIC -DPIC -c libdsm.c

libdsm.so: libdsm.o
	ld -shared -o libdsm.so libdsm.o -ldl

sender.o: sender.c sender.h messages.h
	$(CC) -c sender.c

pagelocks.o: pagelocks.c pagelocks.h
	$(CC) -c pagelocks.c

pagedata.o: pagedata.c pagedata.h
	$(CC) -c pagedata.c

master.o: master.c sender.h pagedata.h messages.h
	$(CC) master.c -c -lpthread

clean:
	rm -f *.o *.so dsm_test /dev/shm/blah
