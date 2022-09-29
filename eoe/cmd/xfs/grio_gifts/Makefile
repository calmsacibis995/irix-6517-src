#!smake
#
# Makefile for grio_gifts
#
#ident	"$Revision: 1.9 $"

include	$(ROOT)/usr/include/make/commondefs

CFILES = rtcp.c grioread.c

TARGETS= rtcp grioread


debug:	$(TARGETS)

default: $(TARGETS)

include $(COMMONRULES)

grioread: grioread.o
	$(CCF) -o $@ grioread.o $(LDFLAGS) -lgrio 

rtcp: rtcp.o
	$(CCF) -o $@ rtcp.o $(LDFLAGS) -lgen


SOURCEDIR=/usr/share/src/xfs/grio
install: default
	$(INSTALL) -dir $(SOURCEDIR)
	$(INSTALL) -m 444 -F $(SOURCEDIR) README Makefile rtcp.c grioread.c
