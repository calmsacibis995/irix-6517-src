#!smake
#
# Makefile for pcp/src/pmdas
#
# $Revision: 2.22 $

PCP_SRC_DEPTH = ..
include	$(ROOT)/usr/include/make/commondefs
include $(PCP_SRC_DEPTH)/include/commondefs

SUBDIRS =

#if $(ISM_NAME) == "pcp"
SUBDIRS	+= cisco environ summary news simple sample trivial txmon \
	  shping trace hotproc mailq sendmail
#endif

#if $(BEFORE_IRIX6_5) || $(ISM_NAME) == "irix"
SUBDIRS	+= pmcd proc
#endif
#if ! $(BEFORE_IRIX6_5) || $(ISM_NAME) == "irix"
SUBDIRS += espping
#endif

LDIRT	=

HEADERS_SUBDIRS=
EXPORTS_SUBDIRS=

default:
	$(SUBDIRS_MAKERULE)

install:
	$(SUBDIRS_MAKERULE)

clobber clean rmtargets fluff:
	rm -f ${LDIRT}
	$(SUBDIRS_MAKERULE)

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; PWD=`pwd`; export PWD; $(MAKE)

$(_FORCE):
