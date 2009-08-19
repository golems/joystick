## Makefile for js

VERSION := 0.0.1
PROJECT := libjs

LIBFILES := libjs.so
BINFILES := jsstart

default: $(LIBFILES) $(BINFILES)

include /usr/share/make-common/common.1.mk

$(call LINKBIN, jsstart, jsstart.o, js)
$(call LINKLIB, js, js.o)


clean: 
	rm -vf *.o *.so jsstart

doc:
	doxygen
