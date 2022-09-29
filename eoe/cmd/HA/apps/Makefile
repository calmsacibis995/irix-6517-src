#!smake
#ident	"$Revision: 1.1 $"

include	$(ROOT)/usr/include/make/commondefs

SUBDIRS	= nfs

default install $(COMMONTARGS) lint debug : $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(_FORCE):

