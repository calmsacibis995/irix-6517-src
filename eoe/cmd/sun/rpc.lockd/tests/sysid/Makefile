#
#!smake
#
# Makefile for the rpc.lockd subdirectory
#
# $Revision: 1.1 $

include	${ROOT}/usr/include/make/commondefs

LINTFLAGS= -woff 91,102

CTAGOPTS=-tw

TARGETS = sysid_test

CFILES = sysid_test.c ../common/util.c

# define NDEBUG to turn off assertions
# define _IRIX for building on IRIX
# define _SUNOS for building on SunOS
#CFLAGS = -DNDEBUG
LCDEFS = -D_IRIX
LCOPTS = -g
LCINCS = -I../common
LIBS = 

default:	$(TARGETS)

include	${COMMONRULES}

sysid_test:	$(OBJECTS)
	$(CCF) $(LCOPTS) $(LCDEFS) -o $@ $(OBJECTS) $(LDFLAGS) $(LIBS)

install:	default
