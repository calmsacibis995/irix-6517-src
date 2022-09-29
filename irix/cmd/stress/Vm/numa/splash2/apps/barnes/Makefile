#!smake
OBJECT_STYLE=64_M3
SPLASH_ROOT=../..
NUMADEFS = -DNUMA
include $(ROOT)/usr/include/make/commondefs
include $(SPLASH_ROOT)/macros/defs

APPL = barnes
CFILES = code.C code_io.C load.C grav.C getparam.C util.C 
HFILES = code.H defs.H stdinc.H vectmath.H
OBJS = ${CFILES:.C=.o}
SHFILES = runtests.sh 
DATFILES = input.dat
FILES = $(CFILES) $(HFILES) $(SHFILES) $(DATFILES)
CFLAGS = -O2 -mips3 -64 -woff 1116 $(NUMADEFS)
LDFLAGS = -lm
LDIRT = *.c *.h *~ 

TARGETS= $(APPL) ${SHFILES:.sh=} ${DATFILES:.dat=}

default: $(TARGETS)

$(APPL): $(OBJS)
	$(CCF) $(OBJS) -o $(APPL) $(GLDOPTS) $(LDFLAGS)

include $(SPLASH_ROOT)/macros/rules
include $(COMMONRULES)

install:$(TARGETS)
	$(INSTALL) -F /usr/stress/Vm/numa/splash2/apps/$(APPL) '$(TARGETS)'
	$(INSTALL) -F /usr/stress/Vm/numa/splash2/apps/$(APPL) README.barnes 

code.h: code.H defs.h
defs.h: defs.H stdinc.h vectmath.h
stdinc.h: stdinc.H 
vectmath.h : vectmath.H

code.o: code.C code.h defs.h
code_io.o: code_io.C code.h
getparam.o: getparam.C stdinc.h
grav.o: grav.C code.h
load.o: load.C code.h defs.h
util.o: util.C stdinc.h

