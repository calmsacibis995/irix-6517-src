#
# Makefile for eoe/lib
#
# $Revision$

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
EXPORTS_SUBDIRS=libcurses libnsl \
	libadm \
	libattr \
	libcmd libcomp libcrypt \
	libcvt libdb \
	libds libdisk libdl libdm \
	libfam libfrs \
	libgen \
	libiaf \
	libl liblim \
	libmalloc \
	libmdbm \
	libmediaclient \
	libmediad \
	libmutex \
	libnanothread \
	libnls \
	libpcp libpcp_dev libpcp_pmda \
	libpkg \
	libpthread \
	librmt librpcsvc librt librtmon \
	libsatmp \
	libspypt \
	libsun \
	libtserialio \
	libuli \
	libusio \
	libw \
	libxnet \
	liby \
	ns \
	sss

HEADERS_SUBDIRS=\
		libcurses \
		libdb \
		libds \
		libgen \
		libl \
		libmediad \
		libnanothread \
		libnsl \
		libpthread \
		librtmon \
		libsocket \
		libspypt \
		libtserialio \
		libw \
		libxnet \
		liby \
		sss

SUBDIRS=$(EXPORTS_SUBDIRS) libcursesBSD terminfo \
	libudma \
	libmutex \
	libplot \
	libsocket \
	nametoaddr\
	libabi \
	libdlook \
	libdplace \
	libdprof \
	ns

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
