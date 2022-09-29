#!smake
#
# Makefile for SSDB headers
#
include $(ROOT)/usr/include/make/commondefs

INCLDIR = /usr/include
SWHEADER = $(INCLDIR)
SRCDIR = .

COMMONPREF = sss_

$(COMMONPREF)install default: $(TARGETS)

include $(COMMONRULES)

headers:
	$(INSTALL) -F /usr/include/sss -m 444 rgPluginServer.export; \
	$(INSTALL) -F /usr/include -m 444 eventmonapi.h

exports:

clean clobber install:  $(COMMONPREF)$$@

install: default
	$(INSTALL) -F /usr/include -m 444 eventmonapi.h
