#!smake
#
# Makefile for /usr/include/sys/xtalk header file installation.
#
DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/sys/xtalk
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
FRIEND_HEADERS=	\
    xbow.h	\
    xswitch.h	\
    xtalk.h	\
    xwidget.h	\
    xbow_info.h \
    xg.h	\
    hq4.h 

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default $(KCOMMONPREF)install:

headers!
	#${INSTALL} -m 444 -F /usr/include/sys/xtalk ${MASTERC_HEADERS}
	${INSTALL} -m 444 -F /usr/include/sys/xtalk ${FRIEND_HEADERS}

morestuff: .xbow.h_ok xbow.html

# Do we want to force a xbow_check? When?
#
.xbow.h_ok: xbow.h xbow_check.c
	@rm -f $@
	cd $(BOOTAREA) ;\
	unset RLS_ID_OBJECT; $(CC) $(CINCS) $(CDEFS) \
		$(CVERSION) $(MKDEPOPT) $(KPATH)/xbow_check.c \
		-o xbow_check
	$(BOOTAREA)/xbow_check
	rm -f $(BOOTAREA)/xbow_check
	touch .xbow.h_ok

# This is pretty useful. Do we want to do it regularly?
#
xbow.html: xbow.h xbow_html.c
	@rm -f $@
	cd $(BOOTAREA) ;\
	unset RLS_ID_OBJECT; $(CC) $(CINCS) $(CDEFS) \
		$(CVERSION) $(MKDEPOPT) $(KPATH)/xbow_html.c \
		-o xbow_html
	$(BOOTAREA)/xbow_html > xbow.html
	rm -f $(BOOTAREA)/xbow_html
