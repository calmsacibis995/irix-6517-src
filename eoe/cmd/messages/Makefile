#ident "$Revision: 1.18 $"

.MAKEOPTS:-P

include $(ROOT)/usr/include/make/commondefs

COMMONPREF=msgs
TARGETS=mkinclmsg
HEADERS_SUBDIRS=uxlibc uxlibxnet uxsgicore uxsgicsh uxsgidesktop uxsgish
SUBDIRS= uxawk uxcore uxcore.abi uxdfm uxed.abi \
	uxlibc uxlibxnet uxmesg uxnsu uxsysadm uxue uxue.abi uxsgicore \
	uxsgicsh uxsgidesktop uxsgiimpr uxepu uxsgish


default: $(TARGETS)
	$(SUBDIRS_MAKERULE)

include $(COMMONRULES)

install $(COMMONTARGS) : $(COMMONPREF)$$@
	$(SUBDIRS_MAKERULE)

headers: $(TARGETS)
	$(HEADERS_SUBDIRS_MAKERULE)

mkinclmsg: mkinclmsg.c
	$(HOST_CC) -O -o mkinclmsg mkinclmsg.c

$(COMMONPREF)install: mkinclmsg
