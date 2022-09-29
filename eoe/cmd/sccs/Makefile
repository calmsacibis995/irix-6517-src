#
#ident	"$Revision: 1.8 $"
#

#
# Common Definitions
#
DEPTH=.
include	$(DEPTH)/slocaldefs

SUBDIRS=	lib cmd help.d

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

headers:

exports:

$(_FORCE):
