#!smake

include	$(ROOT)/usr/include/make/commondefs

SUBDIRS	= rtdisk daemon

default lint install debug $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(_FORCE):

