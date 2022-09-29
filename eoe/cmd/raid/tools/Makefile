#
# Makefile for the RAID administrative utility
#

include	$(ROOT)/usr/include/make/commondefs

TARGETS	= startstop modepage writelong write markdown inq
CFILES	= startstop.c modepage.c writelong.c write.c markdown.c inq.c
LCFLAGS	= -g -DDEBUG
LLDOPTS	= -g
LLDLIBS = -lds

default: $(TARGETS)

include $(COMMONRULES)
