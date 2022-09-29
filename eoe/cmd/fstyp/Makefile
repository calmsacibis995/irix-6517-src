#ident "$Revision: 1.4 $"

include $(ROOT)/usr/include/make/commondefs

SHFILES=fstyp.sh
TARGETS=$(SHFILES:.sh=)

default:$(TARGETS)

include $(COMMONRULES)

install: default
	${INSTALL} -F /usr/sbin $(TARGETS)
	$(INSTALL) -F /etc -lns ../usr/sbin/$(TARGETS) $(TARGETS)
