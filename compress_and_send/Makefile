# Compiler
CC = gcc

# Compiler Flags
CFLAGS = -Wall

# Sources
SOURCES = main.c queue.c reader.c sender.c data_generator.c

# Object Files
OBJ = $(SOURCES:.c=.o)

# Executable
EXECUTABLE = compress_and_send

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread -llz4 -lzmq -lnuma -lhdf5
	rm -f $(OBJ)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(EXECUTABLE)
