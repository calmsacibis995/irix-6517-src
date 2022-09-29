#!smake
#
# $Id: Makefile,v 1.6 1999/04/30 01:44:04 kenmcd Exp $
#

PCP_SRC_DEPTH = ../..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

WARNOFF  =

C++FILES = ArchiveTimeForm.c++ ArchiveTimeFormUI.c++ VkPCParchiveTime.c++ \
	impl.c++ clients.c++ ArchiveBounds.c++ ArchiveBoundsForm.c++

OBJECTS = $(C++FILES:S/.c++/.o/g)

LC++DEFS  = $(DEBUG) -fullwarn $(WARNOFF) $(PCP_INC_PATH) -I../pixmaps -I../common
LC++OPTS = -nostdinc -woff 3262

default: $(C++FILES) $(OBJECTS)

include $(COMMONRULES)

install: default
