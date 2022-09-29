# Generate BIND name server
#
# $Revision: 1.2 $

include	$(ROOT)/usr/include/make/commondefs

SUBDIRS	=rsvpd tools mibs

COMMONPREF=bind

default: all

include $(COMMONRULES)

${COMMONTARGS} all install:
	@for i in $(SUBDIRS) ; do \
		echo "=====\tcd $$i; $(MAKE) $@" ; \
		cd $$i; $(MAKE) $@ ; cd ..; \
	done

exports headers:
