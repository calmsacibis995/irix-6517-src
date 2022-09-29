#
# Makefile for irix/cmd
#
# $Revision: 1.18 $

include	$(ROOT)/usr/include/make/commondefs

#
# Alphabetized, grouped list of subdirectories.
#

# Turn off directories :
#	NONE
#
SUBDIRS= \
	bsd cached clshm cpr diskless dlpi dprof flash flashio flashmmsc \
	hinv icrash lboot linkstat mkmachfile \
	mkpart mmscd netman \
	onlinediag perfex savecore smt sn0log sn0msc snmp \
	stress sysctlrd tokenring xbstat xfs xlv xfsm nvlog

HEADERS_SUBDIRS=netman mkmachfile
EXPORTS_SUBDIRS=netman snmp

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
