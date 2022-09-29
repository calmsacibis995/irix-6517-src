#ident "$Revision: 1.4 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=chkconfig
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /sbin $(TARGETS)
	${INSTALL} -F /etc -lns ../sbin/$(TARGETS) $(TARGETS)
