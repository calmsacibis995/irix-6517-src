#!smake
#
# $Revision: 1.16 $
# par - process activity package
#

include	$(ROOT)/usr/include/make/commondefs

TARGETS=par padc

LCDEFS+=-D_KMEMUSER
LCOPTS+=-fullwarn
LLDLIBS= -lrtmon

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/sbin $(TARGETS)
