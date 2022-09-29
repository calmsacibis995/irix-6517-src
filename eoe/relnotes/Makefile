#
# Makefile for irix/relnotes
#

include $(ROOT)/usr/include/make/commondefs

SUBDIRS=dba nfs plexing fddivis fddixpress pcp_eoe trix

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):

