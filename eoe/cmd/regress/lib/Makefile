#
# Makefile for cmd/regress/lib/*.sh
#
#	"$Revisions$"

include $(ROOT)/usr/include/make/commondefs

SHFILES = error.sh info.sh warning.sh
TARGETS = error info warning

IDB_TAG_SW = noship.sw.regress

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -F /usr/regress/lib -idb $(IDB_TAG_SW) $(TARGETS)
