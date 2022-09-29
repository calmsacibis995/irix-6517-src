#ident "$Revision: 1.7 $"

include $(ROOT)/usr/include/make/commondefs
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

TARGETS=chroot

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /sbin $(TARGETS)
	${INSTALL} -F /etc -lns ../sbin/$(TARGETS) $(TARGETS)
	$(INSTALL) -F /usr/sbin -lns ../../sbin/$(TARGETS) $(TARGETS)
