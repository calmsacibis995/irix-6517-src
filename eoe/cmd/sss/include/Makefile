#!smake
#
# Makefile for SSDB headers
#
#	"$Revision: 1.6 $"
#
include $(ROOT)/usr/include/make/commondefs

INCLDIR = /usr/include/sss
SWHEADER = $(INCLDIR)
SRCDIR = .
HEADERS = ssdbapi.h ssdberr.h semapi.h common.h common_events.h \
	  seh_errors.h dsm_errors.h semapisrv.h sgmdefs.h seh_archive.h \
	  common_ssdb.h

COMMONPREF = sss_

$(COMMONPREF)install default: $(TARGETS)

include $(COMMONRULES)

headers: install

exports:

clean clobber install:  $(COMMONPREF)$$@

install: default
	$(INSTALL) -F $(SWHEADER) -m 444 $(HEADERS)
