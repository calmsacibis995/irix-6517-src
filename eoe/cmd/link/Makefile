#ident "$Revision: 1.3 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=link

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/sbin $(TARGETS)
	$(INSTALL) -F /etc -lns ../usr/sbin/$(TARGETS) $(TARGETS)
