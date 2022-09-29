#
#ident	"$Revision: 1.1 $"
include	$(ROOT)/usr/include/make/commondefs

DIRS = misc port

COMMONPREF=vi

all: default

include $(COMMONRULES)

default $(COMMONTARGS) install:
	@for i in $(DIRS); do \
		$(ECHO) "====\tcd $$i; make $@"; \
		cd $$i; $(MAKE) $@; cd ..; \
	done
