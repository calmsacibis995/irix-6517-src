include $(ROOT)/usr/include/make/commondefs
LCOPTS=-fullwarn
WOFF=

TARGETS=whereis

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/bsd $(TARGETS)

