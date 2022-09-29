#!smake
#!smake
#
#	Makefile for netvis/include/protocols
#
#	$Revision: 1.2 $
#
include ../../Makedefs
include $(ROOT)/usr/include/make/commondefs

INST= $(INSTALL) -F ${HEADERSPATH}/sys -m 444

IFILES= cfeiioctl.h fddi.h llc.h smt.h

default exports: install

include $(COMMONRULES)

headers install: $(_FORCE)
	$(INST) $(IFILES)
