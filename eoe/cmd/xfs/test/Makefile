#!smake
#ident	"$Revision: 1.10 $"

include	$(ROOT)/usr/include/make/commondefs

SUBDIRS	= ggd_tst inode rw vnode mmap

default install $(COMMONTARGS) debug xfsdebug lint: $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(_FORCE):

