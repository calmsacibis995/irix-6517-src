#!smake
#
# Makefile for /usr/include/sys/RACER header file installation.
#
DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/sys/PCI
.PATH:$(KPATH)

#
# List of headers that are really private to the kernel but are needed
# to compile master.c.  This unfortunately means they must be shipped
# with all systems as if they were public
#
MASTERC_HEADERS=

#
# List of headers that only system software utilities need
# These are public within SGI for internal development of other ISM's
# but are not shipped to customers.
#
FRIEND_HEADERS=

#
# List of "Public" header files.  These are headers included by user
# code or used by device driver/VFS/Stream module writers
#

PUBLIC_HEADERS=	\
	pciba.h		\
	pciio.h		\
	PCI_defs.h	\
	bridge.h	\
	pcibr.h		\
	ioc3.h		\
	linc.h		\
	univ_vmestr.h	\
	univ_vmereg.h	\
	usrpci.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default $(KCOMMONPREF)install:

headers!
#empty	${INSTALL} -idb "noship" -m 444 \
#empty		-F /usr/include/sys/PCI ${FRIEND_HEADERS}
# When we get some header files we want to export, put them here
#	${INSTALL} -m 444 -F /usr/include/sys/PCI ${MASTERC_HEADERS}
	${INSTALL} -m 444 -F /usr/include/sys/PCI ${PUBLIC_HEADERS}

morestuff: .bridge.h_ok bridge.html

# Do we want to force a bridge_check? When?
#
.bridge.h_ok: bridge.h bridge_check.c
	@rm -f $@
	cd $(BOOTAREA) ;\
	unset RLS_ID_OBJECT; $(CC) $(CINCS) $(CDEFS) \
		$(CVERSION) $(MKDEPOPT) $(KPATH)/bridge_check.c \
		-o bridge_check
	$(BOOTAREA)/bridge_check
	rm -f $(BOOTAREA)/bridge_check
	touch .bridge.h_ok

# This is pretty useful. Do we want to do it regularly?
#
bridge.html: bridge.h bridge_html.c
	@rm -f $@
	cd $(BOOTAREA) ;\
	unset RLS_ID_OBJECT; $(CC) $(CINCS) $(CDEFS) \
		$(CVERSION) $(MKDEPOPT) $(KPATH)/bridge_html.c \
		-o bridge_html
	$(BOOTAREA)/bridge_html > bridge.html
	rm -f $(BOOTAREA)/bridge_html
