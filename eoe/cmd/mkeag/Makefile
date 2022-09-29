#
# Makefile for mkeag(1m).
#
#ident	"$Revision: 1.2 $"
include	$(ROOT)/usr/include/make/commondefs

TARGETS=mkeag eagls eagid eagch eagedit

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
