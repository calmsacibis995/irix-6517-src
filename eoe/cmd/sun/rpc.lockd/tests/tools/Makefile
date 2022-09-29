#
#!smake
#
# Makefile for the tests/tools subdirectory
#
# $Revision: 1.1 $

include	${ROOT}/usr/include/make/commondefs

LINTFLAGS= -woff 91,102

CTAGOPTS=-tw

SHOWLOCKS_FILES = showlocks.c
SHOWLOCKS_OBJS = ${SHOWLOCKS_FILES:.c=.o}

RELEASE_FILES = release.c
RELEASE_OBJS = ${RELEASE_FILES:.c=.o}

CFILES = $(RELEASE_FILES) $(SHOWLOCKS_FILES)

#
# PROGRAMS is a list of programs to be built
#
TARGETS = showlocks release

# define NDEBUG to turn off assertions
# define _IRIX for building on IRIX
# define _SUNOS for building on SunOS
#CFLAGS = -DNDEBUG
LCDEFS = -D_IRIX
LCOPTS = -g
LCINCS = -I../common
LIBS = 

default: ${TARGETS}

include	${COMMONRULES}

showlocks:	$(SHOWLOCKS_FILES)
	$(CCF) $(LCOPTS) $(LCDEFS) $(LDFLAGS) -o $@ $(SHOWLOCKS_FILES) $(LIBS)

release:	$(RELEASE_FILES)
	$(CCF) $(LCOPTS) $(LCDEFS) -o $@ $(RELEASE_FILES) $(LDFLAGS) $(LIBS)

install:	default
