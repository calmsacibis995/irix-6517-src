#!smake
#
#ident	"$Revision: 1.9 $"
#

#
# Common Definitions
#
DEPTH=..
include	$(DEPTH)/slocaldefs

SUBDIRS=	cassi comobj mpwlib

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
