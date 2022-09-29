#
# Makefile for sss
#
#       "$Revision: 1.5 $"
#
include $(ROOT)/usr/include/make/commondefs

HEADERS_SUBDIRS=include libconfigmon
EXPORTS_SUBDIRS=libeventmon libconfigmon libSSC libssrvswitch
SUBDIRS=include $(EXPORTS_SUBDIRS)

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)


$(_FORCE):
