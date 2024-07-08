# Makefile for sdtgen, et al.

ROOT_DIR:=$(shell /usr/bin/dirname $(realpath $(firstword $(MAKEFILE_LIST))))
export ROOT_DIR

SUB_DIRS:=src lib tools example
SUB_DIRS_CLEAN:=$(SUB_DIRS:%=%-clean)

CFLAGS+=-I $(ROOT_DIR)/include -gdwarf-2 -g3 -Wunused-variable -Wshadow -Wuninitialized -Winit-self -Wpointer-arith -Wcast-align
LDFLAGS:=-Wl,-rpath,$(ROOT_DIR)
LDLIBS:=-L$(ROOT_DIR) -lsdt -lm
export CFLAGS LDFLAGS LDLIBS

all: sdtgen packtables tableformat

.PHONY: sdtgen
sdtgen: libsdt.so
	$(MAKE) -C src ../$@

.PHONY: libsdt.so
libsdt.so:
	$(MAKE) -C lib ../$@

.PHONY: packtables tableformat
packtables tableformat: libsdt.so
	$(MAKE) -C tools ../$@

.PHONY: driver
driver: libsdt.so
	$(MAKE) -C example ../$@

# Don't uncomment this entry.  It is for documentation purposes only.
# This entry creates a dependency loop which GNU make doesn't like.
#tables.c: tableformat tables.pak
#	./tableformat $(word $(words $^),$^) $@

tables.pak: packtables tables.dat
	./packtables $(word $(words $^),$^) $@

tables.dat: sdtgen grammars/sdtgen.sdt
	./sdtgen -w $@ $(word $(words $^),$^)

clean: $(SUB_DIRS_CLEAN)
	rm -f libsdt.so *.dat *.pak sdtgen packtables tableformat driver valgrind.txt core

.PHONY: $(SUB_DIRS_CLEAN)
$(SUB_DIRS_CLEAN):
	$(MAKE) -C $(@:%-clean=%) clean
