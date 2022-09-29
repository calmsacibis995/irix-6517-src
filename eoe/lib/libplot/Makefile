#!smake
#ident "$Revision: 1.4 $"

include $(ROOT)/usr/include/make/commondefs

SUBDIRS= plot vt0

default exports install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(_FORCE):
