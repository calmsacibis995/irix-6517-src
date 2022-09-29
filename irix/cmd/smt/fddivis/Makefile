#!smake
#
# Copyright 1989,1990 Silicon Graphics, Inc.  All rights reserved.
#
#	Makefile for FDDIVis
#
#	"$Revision: 1.12 $"

DEPTH=..

# the old tool kit has not been converted
OBJECT_STYLE = 32
include ${DEPTH}/Makedefs

LLDLIBS= ${LIBSMT32} -ltk -lfm -limage -lgl -lX11 -lm

LC++DEFS= -D_LANGUAGE_C_PLUS_PLUS_2_0 -DBACKTRACE ${VISVERS} ${LCDEFS}
LC++INCS= -I${DEPTH}/include -I${INCLDIR}/toolkit -I${INCLDIR}/X11 -I${INCLDIR}/X11/Xm -I${INCLDIR}/Xm

# use -o32 because the old toolkit has not been converted.
LLDOPTS+=-o32

C++FILES= fddivis.c++ resizeview.c++\
	uiwindow.c++ menuview.c++ mviews.c++ \
	mapwindow.c++ mapview.c++ \
	ringwindow.c++ ringview.c++ \
	ring.c++ frame.c++ \
	station.c++ \
	scope.c++ subr.c++ \
	font.c++ fontdump.c++ \
	replaywindow.c++ replayview.c++ empty.c++ \
	statuswindow.c++

CFILES= sig.c

TARGETS= fddivis

default:${TARGETS}

include ${COMMONRULES}

${TARGETS}: ${OBJECTS} ${LIBSMT32}
	${C++F} ${OBJECTS} ${LDOPTS} ${LLDLIBS} -o $@

install: default
	${INSTALL} -F /usr/sbin -idb fddivis.sw.fddivis ${TARGETS}
	${INSTALL} -F /usr/lib  -idb fddivis.sw.fddivis fddivis.help

# recompile if the version number changes
subr.o:	${DEPTH}/Makedefs
