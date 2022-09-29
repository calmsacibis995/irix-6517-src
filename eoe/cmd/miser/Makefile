#!smake

#
# FILE: eoe/cmd/miser/Makefile
#
# DESCRIPTION:
# 	Makefile for miser and cpuset commands and libraries. 
#

include ${ROOT}/usr/include/make/commondefs

SUBDIRS	= lib cmd man

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
