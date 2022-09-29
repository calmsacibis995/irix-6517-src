#!smake
OBJECT_STYLE=64_M3
SPLASH_ROOT=../../..
NUMADEFS = -DNUMA
include $(ROOT)/usr/include/make/commondefs
include $(SPLASH_ROOT)/macros/defs

APPL = ocean

CFILES = main.C jacobcalc.C jacobcalc2.C laplacalc.C \
	   slave1.C slave2.C multi.C linkup.C subblock.C 
HFILES = decs.H
OBJS = ${CFILES:.C=.o}
SHFILES = runtests.sh 
DATFILES = 
FILES = $(CFILES) $(HFILES) $(SHFILES) $(DATFILES)
CFLAGS = -O2 -mips3 -64 $(NUMADEFS)
LDFLAGS = -lm 
LDIRT = *.c *.h *~

TARGETS= $(APPL) ${SHFILES:.sh=} ${DATFILES:.dat=}

default: $(TARGETS)

$(APPL): $(OBJS)
	$(CCF) $(OBJS) -o $(APPL) $(GLDOPTS) $(LDFLAGS)


include $(SPLASH_ROOT)/macros/rules
include $(COMMONRULES)

install:$(TARGETS)
	$(INSTALL) -F /usr/stress/Vm/numa/splash2/apps/$(APPL)/contiguous_partitions '$(TARGETS)'

decs.h: decs.H
jacobcalc.c: decs.h
linkup.c: decs.h
slave1.c: decs.h
jacobcalc2.c: decs.h
main.c: decs.h
slave2.c: decs.h
laplacalc.c: decs.h
multi.c: decs.h
subblock.c: decs.h

