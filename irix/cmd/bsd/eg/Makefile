#!smake
#
# Makefile for gigether cmds
#
# $Revision: 1.1 $

include $(ROOT)/usr/include/make/commondefs

TARGETS= egconfig egcmd

default: $(TARGETS)

include $(COMMONRULES)

install: default 
	$(INSTALL) -f /usr/etc $(TARGETS)

