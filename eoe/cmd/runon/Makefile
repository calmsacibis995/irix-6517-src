#ident "$Revision: 1.4 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=runon

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/sbin $(TARGETS)
