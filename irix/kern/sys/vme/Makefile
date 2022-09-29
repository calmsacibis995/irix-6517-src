#!smake
#
# Makefile for /usr/include/sys/vme header file installation.
#

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/sys/vme
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
	universe.h      \
	usrvme.h


#
# List of "Public" header files.  These are headers included by user
# code or used by device driver/VFS/Stream module writers
#
PUBLIC_HEADERS=	\
	vmeio.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default $(KCOMMONPREF)install:

headers!
	${INSTALL} -m 444 -F /usr/include/sys/vme ${PUBLIC_HEADERS}
	${INSTALL} -idb "noship" -m 444 -F /usr/include/sys/vme ${FRIEND_HEADERS}


