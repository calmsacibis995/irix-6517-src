#
# Makefile for memory accounting commands
#
# There are six commands: memusage mkinodetbl flushmem prgdump rgaccum solocal
# only memusage is actually machine specific.
#
# $Revision: 1.6 $
#
include $(ROOT)/usr/include/make/commondefs
include $(PRODUCTDEFS)

#LLDLIBS=-lelf
#LCDEFS=-D$(CPUARCH) -D$(CPUBOARD)

#TARGETS=memusage mkinodetbl flushmem prgdump rgaccum solocal
TARGETS=mkinodetbl flushmem prgdump rgaccum solocal

default: $(TARGETS)

include $(COMMONRULES)
