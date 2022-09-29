#
include	$(ROOT)/usr/include/make/commondefs

TARGETS=join
LLDLIBS=-lw

default:$(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
