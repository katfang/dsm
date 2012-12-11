# Sorry guys I messed everything up and ruined everything.  I'll fix it some day

CC=gcc -ggdb 

dsm_objects = libdsm.o network.o pagelocks.o 
manager_objects = pagedata.o copyset.o network.o manager.o pagelocks.o

.PHONY: all
all: dsm_test manager strassen_test

manager: pagelocks.h pagedata.h network.h copyset.h $(manager_objects)
	$(CC) -o manager $(manager_objects) -lpthread

dsm_test: dsm_test.c libdsm.h network.h pagelocks.h pagedata.h copyset.h  $(dsm_objects)
	$(CC) -pthread dsm_test.c $(dsm_objects) -o dsm_test -lrt

strassen_test: scheduler.c strassen.c strassen_test.c
	$(CC) strassen_test.c strassen.c scheduler.c -o strassen_test

.PHONY: clean
clean:
	rm -f *.o dsm_test dsm_read_test faulthandler manager strassen_test /dev/shm/blah
