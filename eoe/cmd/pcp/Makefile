#!smake
#
# Makefile for irix/cmd/pcp
#
# $Revision: 1.27 $

include	$(ROOT)/usr/include/make/commondefs

SUBDIRS = include buildtools libpcp_omc libpcp_inv \
	pmbrand newhelp pmnscomp pmns pmcd pmem pminfo \
	pmlogger pmpost pmdas pmval pmstore pmkstat \
	pmlaunch pmdbg pmerr pmobjstyle desktop \
	oview_layout oview pmtime pmrun magic pmafm \
	pmlc pmprobe pmdumplog dkprobe pmcdctl autofsd-probe \
	hipprobe pmdate pmhostname pmie pmlock pmlogconf pmieconf

HEADERS_SUBDIRS=include libpcp_omc libpcp_inv
EXPORTS_SUBDIRS=buildtools libpcp_omc libpcp_inv

default install $(COMMONTARGS) lint debug: $(_FORCE)
	$(SUBDIRS_MAKERULE)

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

$(_FORCE):

