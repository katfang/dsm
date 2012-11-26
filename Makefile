CC=gcc

.PHONY: all
all: dsm_test manager 

.PHONY: test
test: all test_sender

manager: pagedata.o sender.o copyset.o server.o manager.o
	$(CC) pagedata.o sender.o copyset.o server.o manager.o -o manager -lpthread

test_sender: test_sender.c sender.o
	$(CC) sender.o test_sender.c -o test_sender

dsm_test: dsm_test.c libdsm.o sender.o pagelocks.o pagedata.o copyset.o
	$(CC) dsm_test.c libdsm.o sender.o pagelocks.o pagedata.o copyset.o -o dsm_test -lrt

libdsm.o: libdsm.c libdsm.h 
	$(CC) -fPIC -DPIC -c libdsm.c

libdsm.so: sender.o pagedata.o libdsm.o pagelocks.o copyset.o
	ld -shared -o libdsm.so sender.o pagedata.o pagelocks.o copyset.o libdsm.o -ldl

sender.o: sender.c sender.h
	$(CC) -fPIC -DPIC -c sender.c

pagelocks.o: 
	$(CC) -fPIC -DPIC -c pagelocks.c

pagedata.o: 
	$(CC) -fPIC -DPIC -c pagedata.c

copyset.o: 
	$(CC) -fPIC -DPIC -c copyset.c

server.o: 
	$(CC) -c server.c -lpthread

manager.o: 
	$(CC) manager.c -c -lpthread

.PHONY: clean
clean:
	rm -f *.o *.so dsm_test /dev/shm/blah
