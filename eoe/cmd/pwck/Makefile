#ident "$Revision: 1.2 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=pwck

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/sbin $(TARGETS)
	$(INSTALL) -F /etc -lns ../usr/sbin/$(TARGETS) $(TARGETS)
