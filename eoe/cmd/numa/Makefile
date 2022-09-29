#
#
# $Revision: 1.3 $

include	$(ROOT)/usr/include/make/commondefs

#
# Alphabetized, grouped list of subdirectories.
#
SUBDIRS= \
	noderun numa_view nstats sn

HEADERS_SUBDIRS=
EXPORTS_SUBDIRS=

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
