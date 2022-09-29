#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=factor
LCOPTS=-fullwarn

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/bin $(TARGETS)
