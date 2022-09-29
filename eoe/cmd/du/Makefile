#ident "$Revision: 1.10 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=du
CFILES= du.c

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/bin $(TARGETS)
