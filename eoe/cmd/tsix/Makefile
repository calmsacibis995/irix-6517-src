#!smake
#
# Main makefile for cmd/tsix
#
#ident	"$Revision: 1.2 $"

include	$(ROOT)/usr/include/make/commondefs

SUBDIRS	= rhost satmpd

default install $(COMMONTARGS) lint: $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(_FORCE):
