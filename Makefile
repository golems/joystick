## Makefile for js

VERSION := 0.0.2
PROJECT := libjs

LIBFILES := libjs.so
BINFILES := jsstart jsd jscat

default: $(LIBFILES) $(BINFILES)

include /usr/share/make-common/common.1.mk

# apparently ach requires this, or at least c99
CFLAGS += --std=gnu99

include/js/js_msg.h: msg/js.lisp
	cd include/js && \
	  sbcl --noinform --noprint --eval "(require 'genmsg)"\
	  --load ../../$< --eval "(quit)"

$(call LINKBIN, jsstart, jsstart.o, js)
$(call LINKBIN, jsd, jsd.o, js ach pthread rt)
$(call LINKBIN, jscat, jscat.o, js ach pthread rt)
$(call LINKLIB, js, js.o)


clean: 
	rm -vf *.o *.so jsstart jsd
	rm -rf .deps

doc:
	doxygen
