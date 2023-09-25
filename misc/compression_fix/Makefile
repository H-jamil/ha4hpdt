# Compiler
CC = gcc

# Compiler Flags
CFLAGS = -Wall

# Sources
SOURCES = main.c data_generator.c reader.c

# Object Files
OBJ = $(SOURCES:.c=.o)

# Executable
EXECUTABLE = compression_fix

all: $(EXECUTABLE)

$(EXECUTABLE): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread -llz4 -lzmq -lnuma -lhdf5
	rm -f $(OBJ)

.c.o:
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJ) $(EXECUTABLE)
