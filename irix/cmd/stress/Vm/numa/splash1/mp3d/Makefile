#!smake
OBJECT_STYLE=64_M3
SPLASH_ROOT=../
NUMADEFS = -DNUMA
include $(ROOT)/usr/include/make/commondefs
include $(SPLASH_ROOT)/macros/defs

APPL = mp3d
CFILES = adv.C mp3d.C setup.C
HFILES = common.H
OBJS = ${CFILES:.C=.o}
SHFILES = runtests.sh 
DATFILES = empty.geom input.0 input.5 input.50 test.geom
FILES = $(CFILES) $(HFILES) $(SHFILES) $(DATFILES)
CFLAGS = -O2 -mips3 -64 -woff 1116 $(NUMADEFS)
LDFLAGS = -lm
LDIRT = *.c *.h *~ 

TARGETS= $(APPL) ${SHFILES:.sh=}

default: $(TARGETS)

$(APPL): $(OBJS)
	$(CCF) $(OBJS) -o $(APPL) $(GLDOPTS) $(LDFLAGS)

include $(SPLASH_ROOT)/macros/rules
include $(COMMONRULES)

install:$(TARGETS)
	$(INSTALL) -F /usr/stress/Vm/numa/splash1/$(APPL) '$(TARGETS)'
	$(INSTALL) -F /usr/stress/Vm/numa/splash1/$(APPL) $(DATFILES)	

adv.o: adv.C common.h
mp3d.o: mp3d.C common.h
setup.o: setup.C common.h