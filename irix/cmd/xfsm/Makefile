#
# "$Revision: 1.11 $"

include $(ROOT)/usr/include/make/commondefs

SUBDIRS = filetype lib xfsmd lib/tcl tcl

default lint debug $(COMMONTARGS): $(_FORCE)
	$(SUBDIRS_MAKERULE)

install:	$(_FORCE)
		$(SUBDIRS_MAKERULE)
		$(INSTALL) -m 755 -u root -g sys -dir /var/xfsm
		$(INSTALL) -m 755 -u root -g sys -dir /var/xfsm/templates
		$(INSTALL) -m 755 -u root -g sys -dir /var/xfsm/templates/DISK
		$(INSTALL) -m 755 -u root -g sys -dir /var/xfsm/templates/FS
		$(INSTALL) -m 755 -u root -g sys -dir /var/xfsm/templates/VOL

$(_FORCE):
