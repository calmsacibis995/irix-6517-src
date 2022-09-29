# Generate BIND name server
#
# $Revision: 1.7 $

include	$(ROOT)/usr/include/make/commondefs

SUBDIRS	=named nslookup master

COMMONPREF=bind

default: all

include $(COMMONRULES)

${COMMONTARGS} all install:
	@for i in $(SUBDIRS) ; do \
		echo "=====\tcd $$i; $(MAKE) $@" ; \
		cd $$i; $(MAKE) $@ ; cd ..; \
	done
