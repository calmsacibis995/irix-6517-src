#ident	"$Revision: 1.16 $"

include	$(ROOT)/usr/include/make/commondefs

SCRIPTS =Backup Restore List_tape Add_disk

default:$(SCRIPTS)

include $(COMMONRULES)

install: default
	$(INSTALL) -u root -g sys -F /usr/sbin -m 755 "$(SCRIPTS)"
