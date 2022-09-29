include	$(ROOT)/usr/include/make/commondefs

TARGETS = lint lint1 lint2

default:	$(TARGETS)

install: default
	$(INSTALL) -dir /usr/lib/lint
	$(INSTALL) -F /usr/bin lint
	$(INSTALL) -F /usr/lib/lint lint1
	$(INSTALL) -F /usr/lib/lint lint2

include $(COMMONRULES)

lint1: lint1.uunc
	uudecode lint1.uunc

lint2: lint2.uunc
	uudecode lint2.uunc
