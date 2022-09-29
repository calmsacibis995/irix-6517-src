#!smake
#
# Makefile for /usr/include/sys/EVEREST header file installation.
#
DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/sys/EVEREST
.PATH:$(KPATH)

#
# List of headers that are really private to the kernel but are needed
# to compile master.c.  This unfortunately means they must be shipped
# with all systems as if they were public
#
MASTERC_HEADERS= addrs.h everest.h IP19.h IP19addrs.h \
	IP21.h IP21addrs.h IP25.h IP25addrs.h \
	ccsync.h dang.h epc.h evaddrmacros.h evconfig.h \
	evdiag.h everror.h evintr.h evmp.h fchip.h gda.h \
	groupintr.h io4.h mc3.h nvram.h promhdr.h \
	s1chip.h sysctlr.h vmecc.h wd95a.h

#
# List of headers that only system software utilities need
# These are public within SGI for internal development of other ISM's
# but are not shipped to customers.
#
FRIEND_HEADERS=

DIAGVAL_STRS=	diagval_strs.i

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default $(KCOMMONPREF)install:

headers!
	${INSTALL} -m 444 -F /usr/include/sys/EVEREST ${MASTERC_HEADERS}
	${INSTALL} -idb "noship" -m 444 \
		-F /usr/include/sys/EVEREST ${DIAGVAL_STRS}
