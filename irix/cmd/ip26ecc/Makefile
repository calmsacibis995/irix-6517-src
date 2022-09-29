#!smake
# Makefile for ecc
#

include $(ROOT)/usr/include/make/commondefs

LCOPTS+= -fullwarn
TARGETS = ecc
OBJECTS = ecc.o
LDFLAGS = -lelf

.c.o:
	$(CCF) $< -c -o $*.o

default: ${TARGETS}

ecc: $(OBJECTS)
