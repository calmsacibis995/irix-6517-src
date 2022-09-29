#!smake
#!smake
#
#	Makefile for netvis/include/protocols
#
#	$Revision: 1.2 $
#
include ../../Makedefs
include $(ROOT)/usr/include/make/commondefs

INST= $(INSTALL) -F ${HEADERSPATH}/net -m 444

IFILES= raw.h

default exports: install

include $(COMMONRULES)

headers install: $(_FORCE)
	$(INST) $(IFILES)
