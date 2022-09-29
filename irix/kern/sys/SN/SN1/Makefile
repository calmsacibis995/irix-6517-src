#!smake
#
# Makefile for /usr/include/sys/SN header file installation.
#
DEPTH?=../../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/sys/SN/SN0
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
# Unfortunately, every one of these are marked as EOE in the checked
# in idb, and *HAVE* been getting shipped.  Since we are fixing the
# spec file to not put things marked "noship" into EOE, I am taking
# the noship out of the install rule for these, so we keep shipping them.
# This may be the wrong thing; if so, it can be put back, and EOE removed
# from the checked in idb.  Olson, 11/97
#
FRIEND_HEADERS= IP33.h addrs.h arch.h \
		snac.h snacpi.h snacpiregs.h \
	        snacmd.h snacmdregs.h snacio.h snacioregs.h \
	        snacni.h snacniregs.h snaclb.h snaclbregs.h \
		slotnum.h war.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default $(KCOMMONPREF)install:

headers!
	${INSTALL} -m 444 \
		-F /usr/include/sys/SN/SN1 ${FRIEND_HEADERS}
#	${INSTALL} -m 444 -F /usr/include/sys/SN/SN1 ${MASTERC_HEADERS}

