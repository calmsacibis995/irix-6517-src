#!smake
#
# Makefile for CONFIGMON headers
#
include $(ROOT)/usr/include/make/commondefs

INCLDIR  = /usr/include/sss
SWHEADER = $(INCLDIR)
SRCDIR   = .
HEADERS  = configmon.h configmon_api.h dbaccess.h hwconfig.h \
		   swconfig.h sysinfo.h

COMMONPREF = configmon_

$(COMMONPREF)install default: $(TARGETS)

include $(COMMONRULES)

headers: install

exports:

clean clobber install:  $(COMMONPREF)$$@

install: default
	$(INSTALL) -m 444 -F /usr/include/sss $(HEADERS)

