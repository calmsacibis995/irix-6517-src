#!smake

#
# FILE: eoe/cmd/miser/lib/libcpuset/src/Makefile
#
# DESCRIPTION:
#	Makefile for libcpuset
#

include $(ROOT)/usr/include/make/commondefs

#DEBUG	= -g

TARGETS	= ../libcpuset.a
CFILES	= cmd.c 
OFILES	= $(CFILES:.c=.o)

LCFLAGS	= $(DEBUG) 
LDLIBS	= $(TARGETS)

include $(COMMONRULES)

all:     default
default: $(TARGETS)
install: default

$(TARGETS): $(OFILES)
	$(AR) c$(ARFLAGS) $(TARGETS) $(OFILES)
