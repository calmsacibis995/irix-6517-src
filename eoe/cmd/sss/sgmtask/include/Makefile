#!smake
#
# Makefile for SSDB headers
#
#	"$Revision: 1.1 $"
#
include $(ROOT)/usr/include/make/commondefs

INCLDIR = /usr/include/sss
SWHEADER = $(INCLDIR)
SRCDIR = .
HEADERS = sgmtask.h sgmtaskerr.h subscribe.h

COMMONPREF = sss_

$(COMMONPREF)install default: $(TARGETS)

include $(COMMONRULES)

headers: install

exports:

clean clobber install:  $(COMMONPREF)$$@

install: default
	$(INSTALL) -F $(SWHEADER) -m 444 $(HEADERS)
