#!smake
#
# Makefile for event server test programs
#
# "$Revision: 1.4 $"
#
EVENTDEPTH=..
include ${EVENTDEPTH}/Makedefs

NC++INCS= -I../include 
XLDLIBS= 
NLDLIBS= -ltt
C++FILES= evuser.c++ 
TARGETS= evuser

default: ${TARGETS} ${EVLIBRARY}

evuser: $@.o ${EVLIBRARY}
	${C++F} $@.o ${EVLIBRARY} ${LDFLAGS} -o $@
	@${SIZE} -B $@

include ${EVENTDEPTH}/Makerules

install: 
