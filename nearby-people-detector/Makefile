CC      = gcc
CFLAGS  = -Wall -Wextra -Wpedantic -O2 -std=gnu11 -Isrc
BUILD   = build
TARGET  = $(BUILD)/people_detector
TESTBIN = $(BUILD)/test_runner

# GIO (GDBus, for BlueZ) and libnm (NetworkManager)
PKGS       = gio-2.0 libnm
PKG_CFLAGS = $(subst -I,-isystem ,$(shell pkg-config --cflags $(PKGS)))
PKG_LIBS   = $(shell pkg-config --libs $(PKGS))

HEADERS   = $(wildcard src/*.h)
PURE_OBJS = $(BUILD)/bt_table.o $(BUILD)/lan.o $(BUILD)/estimator.o
LIB_OBJS  = $(BUILD)/bt_dbus.o $(BUILD)/wifi_nm.o

all: $(TARGET)

$(TARGET): $(BUILD)/main.o $(PURE_OBJS) $(LIB_OBJS)
	$(CC) $(CFLAGS) $^ $(PKG_LIBS) -o $@

# unit tests use only the pure C modules: no GLib, no hardware
$(TESTBIN): tests/test_runner.c $(PURE_OBJS)
	$(CC) $(CFLAGS) $^ -o $@

$(BUILD)/%.o: src/%.c $(HEADERS) | $(BUILD)
	$(CC) $(CFLAGS) -c $< -o $@

# only these two files include GLib / NetworkManager headers
$(BUILD)/bt_dbus.o $(BUILD)/wifi_nm.o: CFLAGS += $(PKG_CFLAGS)

$(BUILD):
	mkdir -p $(BUILD)

run: $(TARGET)
	./$(TARGET)

test: $(TESTBIN)
	./$(TESTBIN)

clean:
	rm -rf $(BUILD)

.PHONY: all run test clean
