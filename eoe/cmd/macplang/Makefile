#
# Makefile for macplang(1).
#
#ident	"$Revision: 1.4 $"
include	$(ROOT)/usr/include/make/commondefs

TARGETS=macplang
LLDLIBS=-lmls

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
