#!smake
#
# "$Revision: 1.4 $"

include $(ROOT)/usr/include/make/commondefs

SUBDIRS = tlib xdk xfs xlv

default lint debug install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(_FORCE):
