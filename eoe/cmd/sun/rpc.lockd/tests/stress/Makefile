#
#!smake
#
# Makefile for the rpc.lockd subdirectory
#
# $Revision: 1.1 $

include	${ROOT}/usr/include/make/commondefs

LINTFLAGS= -woff 91,102

CTAGOPTS=-tw

TARGETS = lock_stress

CFILES= lock_stress.c print.c ../common/util.c

MONITOR_OBJS = lock_monitor.o
COMMON_OBJS = ../common/util.o print.c

# define NDEBUG to turn off assertions
# define _IRIX for building on IRIX 5.*
# define _SUNOS for building on SunOS
#CFLAGS = -DNDEBUG
LCDEFS = -D_IRIX
LCOPTS = -g
LCINCS = -I../common
LIBS = 

LDIRT = lock_monitor

default:	$(TARGETS)

include	${COMMONRULES}

lock_stress:	$(OBJECTS)
	$(CCF) $(LCOPTS) $(LCCDEFS) -o lock_stress $(OBJECTS) $(LDFLAGS) $(LIBS)

lock_monitor:	$(MONITOR_OBJS) $(COMMON_OBJS)
	$(CCF) $(LCFLAGS) -o lock_monitor $(MONITOR_OBJS) $(COMMON_OBJS) $(LIBS)

install:	default
