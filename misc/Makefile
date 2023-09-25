CC=gcc
CFLAGS=-I /home/beams/MJAMIL/hdf5/include
LDFLAGS=-L /home/beams/MJAMIL/hdf5/lib -lhdf5 -lpthread -llz4 -lzmq -lnuma

SOURCES=$(wildcard *.c)
TARGETS=$(SOURCES:.c=)

.PHONY: all clean

all: $(TARGETS)

%: %.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGETS)
