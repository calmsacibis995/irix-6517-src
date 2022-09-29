#!smake
#
# Makefile for kernel internal interfaces
#
# Headers in this directory fall into 1 of the following catagories:
# 1) IRIX ISM internal interfaces only - these aren't installed anywhere.
#
# 2) interfaces for kernel extensions - VFS, streams, device drivers that
#	aren't part of DDI/DKI (which need to be in sys)
#
# 3) headers used for LBOOT, etc but otherwise shouldn't really be visible.
#
# No user<->kernel interfaces should be here. - device driver ioctl, etc.
#	should all be in sys.
#
# Note that these headers are all installed into /usr/include/ksys.
#
#ident "$Revision: 1.24 $"

DEPTH?=..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/ksys
.PATH:$(KPATH)


#
# List of headers that VFS/streams/device driver writers need
#
KMOD_HEADERS=as.h behavior.h ddmap.h fdt.h hwg.h \
	serialio.h cacheops.h uli.h sthread.h i2c.h elsc.h \
	vm_pool.h xthread.h kern_heap.h kqueue.h cell.h \
	cell/handle.h cell/mesg.h cell/service.h cell/tkm.h \
	cell/wp.h cell/object.h cell/relocation.h

#
# List of headers that only system software utilities need
# These are public within SGI for internal development of other ISM's
# but are not shipped to customers.
#
FRIEND_HEADERS=pid.h vfile.h vpag.h vpgrp.h vproc.h

#
# List of headers that are really private to the kernel but are needed
# to compile master.c.  This unfortunately means they must be shipped
# with all systems as if they were public
#
MASTERC_HEADERS=

#
# Any remaining headers not listed in one of the above lists is
# "Private" meaning that it is never used outside of the "irix"
# ISM.  They are not installed in any public include directory.
#

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default $(KCOMMONPREF)install:

headers!
	${INSTALL} -m 444 -F /usr/include/ksys \
		${KMOD_HEADERS} ${FRIEND_HEADERS} ${MASTERC_HEADERS}
