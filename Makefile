# Project Name
PROJECT := snachd

# Project Version 
VERSION := 0.1

# Binary Files
BINFILES :=  snachd snachd_test

# Library files
SHAREDLIBS := 

.PHONY: default clean

all: default

include /usr/share/make-common/common.1.mk

CFLAGS += -O0 -Wno-conversion
CFLAGS += --std=gnu99

default: $(LIBFILES) $(BINFILES)

## BUILDING LIBRARIES: call with  $(call LINKLIB, name_of_lib, list of object files)


## BUILDING BINARIES: call with $(call LINKBIN, name_of_binary, object files, shared libs, static libs)
$(call LINKBIN, snachd, snachd.o, spnav X11 rt somatic ach somatic_pb-c protobuf-c stdc++ blas lapack)
$(call LINKBIN, snachd_test, snachd_test.o, spnav X11 rt somatic ach somatic_pb-c protobuf-c stdc++ blas lapack)

clean:
	rm -fr *.o $(BINFILES) $(LIBFILES) *.o .dep debian *.deb *.lzma
