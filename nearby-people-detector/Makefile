CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=gnu11
TARGET = build/people_detector
SOURCE = src/main.c

all: $(TARGET)

$(TARGET): $(SOURCE)
	mkdir -p build
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET)

run: $(TARGET)
	./$(TARGET)

clean:
	rm -rf build

.PHONY: all run clean
