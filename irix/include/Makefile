#
# Makefile for irix/include, to install header files under /usr/include.
#
include $(ROOT)/usr/include/make/commondefs

#
# List of subdirectories to run makes in.
#
COMMONPREF=inc

SUBDIRS=make rpc 
PUBLIC_HFILES= sgidefs.h

default:

include $(COMMONRULES)

headers: install

exports:

$(COMMONPREF)install: $(_FORCE)
	$(INSTALL) -m 444 -F /usr/include $(PUBLIC_HFILES)

install $(COMMONTARGS): $(COMMONPREF)$$@ $(_FORCE)
	$(SUBDIRS_MAKERULE)
