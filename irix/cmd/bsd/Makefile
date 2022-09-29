#!smake
#
# Main makefile for cmd/bsd
#
#ident	"$Revision: 1.2 $"

include	$(ROOT)/usr/include/make/commondefs

SUBDIRS	= epfirm netstat eg

LDLIBS	= 
LCDEFS	= -D_BSD_TIME -D_SHAREII
LCOPTS	= -signed
LLDOPTS = -Wl,-T,10000000,-D,10040000 -ignore_unresolved
LDDOPTS = -Wl,-T,7FC00000,-D,7FC40000 -ignore_unresolved

default: $(TARGETS)
	$(SUBDIRS_MAKERULE)

COMMONPREF=bsd_
include $(COMMONRULES)

install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	@$(ECHO) "====\t$@"; cd $@; $(MAKE)
