#!smake
#
# $Revision: 1.4 $

PCP_SRC_DEPTH = ../..
include	$(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

C++FILES	= pixmaps.c++
LC++DEFS	= '-DPCP_VERSION="$(PCP_VERSION)"'
OBJECTS		= $(C++FILES:S/.c++/.o/g)

default: $(OBJECTS)

include $(COMMONRULES)

install: default

headers:
exports:
