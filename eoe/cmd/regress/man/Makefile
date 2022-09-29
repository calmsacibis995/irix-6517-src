#
# Makefile for cmd/man
#
#       "$Revisions$"

include $(ROOT)/usr/include/make/commondefs

SUBDIRS = man1 man5

.PRECIOUS: $(SUBDIRS)

default install $(COMMONTARGS):
	@for d in $(SUBDIRS); do \
		echo "====\tcd $$d; $(MAKE) $@"; \
		cd $$d; $(MAKE) $@; cd ..; \
	done

include $(COMMONRULES)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)
