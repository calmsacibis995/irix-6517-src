#ident "$Revision: 1.6 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=mpadmin
LCOPTS=-fullwarn

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/sbin $(TARGETS)
