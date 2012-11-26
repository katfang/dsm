CC=gcc

dsm_objects = libdsm.o network.o pagelocks.o pagedata.o copyset.o
manager_objects = pagedata.o copyset.o network.o manager.o

.PHONY: all
all: dsm_test manager 

.PHONY: test
test: all test_sender

manager: $(manager_objects)
	$(CC) -o manager $(manager_objects) -lpthread

test_sender: test_sender.c sender.o
	$(CC) sender.o test_sender.c -o test_sender

dsm_test: $(dsm_objects)
	$(CC) dsm_test.c $(dsm_objects) -o dsm_test -lrt

.PHONY: clean
clean:
	rm -f *.o dsm_test manager /dev/shm/blah
