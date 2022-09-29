#ident "$Revision: 1.3 $"

include $(ROOT)/usr/include/make/commondefs

TARGETS=true
LINKS=4d 4d60 mips

default:$(TARGETS) $(LINKS)

include $(COMMONRULES)

$(LINKS): true

install: default
	$(INSTALL) -F /usr/bin $(TARGETS)
	$(INSTALL) -ln true -F /usr/bin "$(LINKS)"
