#!smake
# Makefile for all ssdbserver sources
#
#	"$Revisions$"
include $(ROOT)/usr/include/make/commondefs

SUBDIRS = dbug merge heap regex strings mysys isam client sql data

.PRECIOUS: $(SUBDIRS)

COMMONPREF=SSS

exports: install

default install $(COMMONTARGS):
	@for d in $(SUBDIRS); do \
		echo "++++\tcd\ $$d; $(MAKE) "; \
		cd $$d; $(MAKE) $@; cd ..; \
	done

$(SUBDIRS): $(_FORCE)
	CD $@; $(MAKE)

include $(COMMONRULES)
