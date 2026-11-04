# EXCCD SD card installer (Windows).
#   make                      native build with MinGW (w64devkit)
#   make CROSS=x86_64-w64-mingw32-   cross build on Linux (CI)
#   make test                 PC test of the ZIP / boot-sector core
CROSS ?=
CC = $(CROSS)gcc
WINDRES = $(CROSS)windres
VERSION ?= dev
CFLAGS = -Os -Wall -Wextra -std=c11 -DEXCCD_INSTALLER_VERSION=\"$(VERSION)\"
WINFLAGS = -municode -mwindows -DUNICODE -D_UNICODE -static -s
LIBS = -lcomctl32 -lcomdlg32 -lshell32 -lole32 -luuid

CORE = core.c inflate.c

all: exccd-installer.exe

installer.res.o: installer.rc installer.manifest
	$(WINDRES) installer.rc -O coff -o $@

exccd-installer.exe: installer.c $(CORE) core.h inflate.h installer.res.o
	$(CC) $(CFLAGS) $(WINFLAGS) -o $@ installer.c $(CORE) installer.res.o $(LIBS)

# Host compiler for the test (gcc on Linux, MinGW gcc on Windows).
HOSTCC ?= gcc
test_core: test_core.c $(CORE) core.h inflate.h
	$(HOSTCC) -O2 -Wall -Wextra -o $@ test_core.c $(CORE)

clean:
	rm -f exccd-installer.exe installer.res.o test_core test_core.exe

.PHONY: all clean
