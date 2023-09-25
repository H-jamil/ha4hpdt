# Compiler
CC = gcc

# Compiler Flags
CFLAGS = -Wall

# Sources
SOURCES = main.c queue.c receiver.c decompression.c

# Object Files
OBJ = $(SOURCES:.c=.o)

# Executable
EXECUTABLE = receive_and_decompress

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread -llz4 -lzmq -lnuma -lhdf5
	rm -f $(OBJ)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(EXECUTABLE)
