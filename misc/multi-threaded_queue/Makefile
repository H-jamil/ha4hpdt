all: main

main: main.o queue.o
	gcc main.o queue.o -o main -lhdf5 -lpthread -llz4 -lzmq -lnuma

main.o: main.c queue.h
	gcc -c main.c -o main.o

queue.o: queue.c queue.h
	gcc -c queue.c -o queue.o

clean:
	rm *.o main
