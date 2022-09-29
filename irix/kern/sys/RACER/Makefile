#!smake
#
# Makefile for /usr/include/sys/RACER header file installation.
#
DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/sys/RACER
.PATH:$(KPATH)

#
# List of headers that are really private to the kernel but are needed
# to compile master.c.  This unfortunately means they must be shipped
# with all systems as if they were public
#
MASTERC_HEADERS= \
	IP30.h	\
	IP30addrs.h \
	racermp.h

#
# List of headers that only system software utilities need
# These are public within SGI for internal development of other ISM's
# but are not shipped to customers.
#
FRIEND_HEADERS=	\
    heart.h	\
    IP30nvram.h \
    sflash.h	\
    gda.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default $(KCOMMONPREF)install:

headers!
	${INSTALL} -m 444 -F /usr/include/sys/RACER ${MASTERC_HEADERS}
	${INSTALL} -m 444 -F /usr/include/sys/RACER ${FRIEND_HEADERS}

morestuff: .heart.h_ok heart.html

# Do we want to force a heart_check? When?
#
.heart.h_ok: heart.h heart_check.c
	@rm -f $@
	cd $(BOOTAREA) ;\
	unset RLS_ID_OBJECT; $(CC) $(CINCS) $(CDEFS) \
		$(CVERSION) $(MKDEPOPT) $(KPATH)/heart_check.c \
		-o heart_check
	$(BOOTAREA)/heart_check
	rm -f $(BOOTAREA)/heart_check
	touch .heart.h_ok

# This is pretty useful. Do we want to do it regularly?
#
heart.html: heart.h heart_html.c
	@rm -f $@
	cd $(BOOTAREA) ;\
	unset RLS_ID_OBJECT; $(CC) $(CINCS) $(CDEFS) \
		$(CVERSION) $(MKDEPOPT) $(KPATH)/heart_html.c \
		-o heart_html
	$(BOOTAREA)/heart_html > heart.html
	rm -f $(BOOTAREA)/heart_html
