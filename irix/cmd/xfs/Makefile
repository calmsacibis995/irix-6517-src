#!smake
#ident	"$Revision: 1.2 $"

include	$(ROOT)/usr/include/make/commondefs

# everything depends on these
FIRST = sim
# mkfs depends on this
SECOND = maxtrres

SUBDIRS	= ${FIRST} ${SECOND} copy db logprint mkfs repair 

default install $(COMMONTARGS) lint debug xfsdebug: $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(_FORCE):

