#!smake
#ident	"$Revision: 1.43 $"

include	$(ROOT)/usr/include/make/commondefs

# everything depends on these

SUBDIRS	= bmap bstat dump estimate fsr fstyp ggd grio \
	grio_bandwidth grio_gifts growfs imap stress

default install $(COMMONTARGS) lint debug xfsdebug: $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(_FORCE):

