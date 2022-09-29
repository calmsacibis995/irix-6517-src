#
#!smake
#
# Makefile for the rpc.lockd subdirectory
#
# $Revision: 1.1 $

include	${ROOT}/usr/include/make/commondefs

LINTFLAGS= -hbax -woff 91,102

CTAGOPTS=-tw

CFILES = util.c

# define NDEBUG to turn off assertions
# define _IRIX for building on IRIX
# define _SUNOS for building on SunOS
#CDEFS = -DNDEBUG
LCDEFS = -D_IRIX
LCOPTS = -g
LCINCS = -I.
LIBS = 

default: ${OBJECTS}

include	${COMMONRULES}

install:	default
