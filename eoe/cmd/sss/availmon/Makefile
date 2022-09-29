#
# Makefile for eoe/cmd/sss/availmon
#
# $Revision: 1.5 $

include $(ROOT)/usr/include/make/commondefs

#
# Sub-directories
#
SUBDIRS=amfilter amparse amreceive amsyslog amuudecode amconfig \
	amtime1970 scripts amconvert amformat stubs

HEADERS_SUBDIRS=include

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
