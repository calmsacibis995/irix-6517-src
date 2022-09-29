#!smake

include	$(ROOT)/usr/include/make/commondefs

SUBDIRS=dlpitest snetd lltune

default:
	$(SUBDIRS_MAKERULE)

COMMONPREF=dlpi_
include $(COMMONRULES)

install $(COMMONTARGS): $(COMMONPREF)$$@
	$(SUBDIRS_MAKERULE)

$(COMMONPREF)install: default
	$(INSTALL) -idb 'exitop("$$rbase/var/opt/snet/instdir/instscript")' \
		-idb DLPI -F /var/opt/snet/instdir instscript

$(SUBDIRS): $(_FORCE)
	cd $@; $(MAKE)

$(_FORCE):
