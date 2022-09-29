#ident "$Revision: 1.1 $"

include ${ROOT}/usr/include/make/commondefs

TARGETS=mesgstats

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/sbin $(TARGETS)
