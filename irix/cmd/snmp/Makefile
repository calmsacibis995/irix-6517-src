#
# Makefile for irix/cmd/snmp
#
# $Revision: 1.5 $

include	$(ROOT)/usr/include/make/commondefs

#
# Alphabetized, grouped list of subdirectories.
#

# Turn off directories :
#	NONE
#
#SUBDIRS= libsnmp libagent sgiagent hpagent peer
SUBDIRS= include libsnmp libagent sgiagent peer
HEADERS_SUBDIRS= include libagent
EXPORTS_SUBDIRS= libsnmp libagent

default install $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

headers: $(_FORCE)
	$(HEADERS_SUBDIRS_MAKERULE)

exports: $(_FORCE)
	$(EXPORTS_SUBDIRS_MAKERULE)

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):

