all: sender

sender: sender.o queue.o
	gcc sender.o queue.o -o sender -lhdf5 -lpthread -llz4 -lzmq -lnuma

sender.o: sender.c queue.h
	gcc -c sender.c -o sender.o

queue.o: queue.c queue.h
	gcc -c queue.c -o queue.o

clean:
	rm *.o sender
