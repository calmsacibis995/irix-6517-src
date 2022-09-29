#!smake

#
# FILE: eoe/cmd/miser/cmd/Makefile
#
# DESCRIPTION:
#	Makefile for miser and cpuset commands.
#

include $(ROOT)/usr/include/make/commondefs

SUBDIRS = miser		\
	  cpuset	\
	  miser_jinfo	\
	  miser_kill	\
	  miser_move	\
	  miser_qinfo	\
	  miser_reset	\
	  miser_submit	\
	  $(NULL)

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
