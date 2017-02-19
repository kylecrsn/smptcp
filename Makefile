TARGET = smptcp
CC = gcc
CFLAGS = -g -Wall
LDFLAGS = -L. -lmptcp
LIBS = -pthread

.PRECIOUS: $(TARGET) $(OBJECTS)
OBJECTS = $(patsubst %.c,%.o,$(wildcard *.c))
HEADERS = $(wildcard *.h)

.PHONY: default all clean
default: $(TARGET)
all: default
clean:
	-rm -f *.o $(TARGET)

%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -Wall $(LIBS) $(LDFLAGS) -o $@