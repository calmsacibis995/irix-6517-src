#!smake
#
# $Id: Makefile,v 1.4 1997/10/24 06:26:20 markgw Exp $
#

PCP_SRC_DEPTH = ../..
include $(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

WARNOFF =

C++FILES = VkPCPtimer.c++

LC++DEFS  = $(DEBUG) -fullwarn $(WARNOFF) $(PCP_INC_PATH)
LC++OPTS = -I$(ROOT)/usr/include/Vk -woff 3262

default: $(C++FILES) $(OBJECTS)

include $(COMMONRULES)

install: default
