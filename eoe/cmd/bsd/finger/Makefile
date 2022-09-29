#!smake
#
# Make the finger command and finger deamon
#
# $Revision: 1.9 $

include $(ROOT)/usr/include/make/commondefs

LMKDEPFLAGS= $(NULLSUFFIX_MKDEPFLAG)

CFILES= finger.c fingerd.c
TARGETS= $(CFILES:.c=)

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bsd finger
	$(INSTALL) -F /usr/etc fingerd
