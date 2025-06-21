CC = gcc
CFLAGS = -Wall -Wextra -std=c23 -D_GNU_SOURCE
LDFLAGS = -lreadline -lncurses
TARGET = cmi.bin
SRC = main.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean
