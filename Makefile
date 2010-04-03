# Project Name
PROJECT := joystick

# Project Version 
VERSION := 0.1

# Binary Files
BINFILES :=  snachd jachd jach_listen_and_print

# Library files
SHAREDLIBS := js_smm

.PHONY: default clean

all: default

include /usr/share/make-common/common.1.mk

CFLAGS += -O0 -Wno-conversion
CFLAGS += --std=gnu99

default: $(LIBFILES) $(BINFILES)

## BUILDING LIBRARIES: call with  $(call LINKLIB, name_of_lib, list of object files)
$(call LINKLIB, js_smm, js_smm.o)

## BUILDING BINARIES: call with $(call LINKBIN, name_of_binary, object files, shared libs, static libs)
$(call LINKBIN, snachd, snachd.o js_smm.o, spnav X11 rt somatic ach somatic_pb-c protobuf-c stdc++ blas lapack)
$(call LINKBIN, jachd, jachd.o js_smm.o js.o, rt somatic ach somatic_pb-c protobuf-c stdc++ blas lapack)
$(call LINKBIN, jach_listen_and_print, jach_listen_and_print.o js_smm.o, rt somatic ach somatic_pb-c protobuf-c stdc++ blas lapack)

clean:
	rm -fr *.o $(BINFILES) $(LIBFILES) *.o .dep debian *.deb *.lzma
