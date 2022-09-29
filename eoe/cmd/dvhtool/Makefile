#ident "$Revision: 1.6 $"

include $(ROOT)/usr/include/make/commondefs
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)
LCOPTS=-fullwarn

TARGETS=dvhtool
LLDLIBS=-ldisk

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /sbin $(TARGETS)
	${INSTALL} -F /etc -lns ../sbin/$(TARGETS) $(TARGETS)
