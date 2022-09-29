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
MASTERC_HEADERS= hubstat.h sn0drv.h \
	IP27.h addrs.h arch.h diagval_strs.i\
		hub.h hubpi.h hubmd.h hubio.h hubni.h hubcore.h \
		bte.h sn0.h klhwinit.h memerror.h \
		slotnum.h ip27config.h sn0_fru.h \
		ip27log.h war.h klhwinit_gfx.h IP31.h

#
# List of headers that only system software utilities need
# These are public within SGI for internal development of other ISM's
# but are not shipped to customers.
#

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default $(KCOMMONPREF)install:

headers!
	${INSTALL} -m 444 -F /usr/include/sys/SN/SN0 ${MASTERC_HEADERS}

