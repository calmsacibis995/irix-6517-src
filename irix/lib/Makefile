#
# Makefile for irix/lib
#
# $Revision: 1.5 $

include	$(ROOT)/usr/include/make/commondefs

# The first line contains an explicit build ordering, for purposes of
# transitive .so symbol resolution.
#
# NOTE : only add libs here if need by another isms.
# Otherwise add below to SUBDIRS.
#
# IMPORTANT: The ordering of libds and libdisk is important since
# libdisk depends on libds being there.
# 
# NOTE: libclshm has not been added to the SUBDIRS list because this
# library is not currently released.  Add it to the SUBDIRS list 
# below to have it made generally.

EXPORTS_SUBDIRS=libc \
	libbsd \
	libcpr \
	libfetchop \
	libgrio \
	librsvp \
	librestart \
	libtoolroot \
	klib

HEADERS_SUBDIRS=libc \
		libcpr \
		librsvp \
		klib

SUBDIRS=$(EXPORTS_SUBDIRS) \
	libirixpmda \
	libperfex 

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
