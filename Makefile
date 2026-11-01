CC ?= gcc
OBS_INCLUDE ?= ../obs-studio/libobs
OBS_LIBDIR ?= ../obs-studio/build/libobs
TARGET ?= obs-beauty-camera.dll
SRC ?= src/plugin.c
CFLAGS ?= -O2 -Wall -Wextra
LIBS ?= -lobs -lshell32 -ladvapi32

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) -shared $(CFLAGS) -s -static-libgcc -o $@ $< \
		-I$(OBS_INCLUDE) -L$(OBS_LIBDIR) $(LIBS)

clean:
	rm -f $(TARGET)
