#!smake
#
# Makefile for /usr/include/sys/SN header file installation.
#
DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/sys/SN
.PATH:$(KPATH)

#
# List of headers that are really private to the kernel but are needed
# to compile master.c.  This unfortunately means they must be shipped
# with all systems as if they were public
#
HEADERS_SUBDIRS=SN0 SN1
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
FRIEND_HEADERS= addrs.h xtalkaddrs.h xbow.h arch.h \
		agent.h clshm.h clshmd.h intr.h io.h\
		router.h routerdrv.h nvram.h gda.h kldiag.h \
		kldir.h klconfig.h launch.h memsupport.h slotnum.h \
		nmi.h promcfg.h promhdr.h error.h error_info.h error_force.h \
		klkernvars.h intr_public.h vector.h \
		groupintr.h  prom_names.h promlog.h fprom.h \
		war.h hwcntrs.h klpart.h klpartvars.h if_cl.h

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default $(KCOMMONPREF)install:

headers!
	${INSTALL} -m 444 \
		-F /usr/include/sys/SN ${FRIEND_HEADERS}
	${HEADERS_SUBDIRS_MAKERULE}

