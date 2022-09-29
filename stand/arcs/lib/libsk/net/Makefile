# Makefile libsk/net
#
# This directory contains the standalone networking support.
#
# $Revision: 1.15 $

SKDEPTH= ..
include $(SKDEPTH)/libskdefs
include $(DEPTH)/netdbxdefs

#
# Set up to build machine dependent objects
LLCDEFS= -D$(CPUBOARD) ${NETDBXFLAGS}
#
IP32R4000_X=\#
IP32R10000_X=\#
$(CPUBOARD)$(CPUARCH)_X=

$(IP32R4000_X)LLCDEFS= -D$(CPUBOARD) -mips3

OBJECTDIR=$(TARGETDIR)
LIBPATH=$(LIBNAME)

CFILES= arp.c if.c if_ec2.c if_ee.c if_ef.c mbuf.c socket.c udpip.c
$(IP32R4000_X)CFILES= arp.c if.c mbuf.c socket.c udpip.c mace_ec.c
$(IP32R10000_X)CFILES= arp.c if.c mbuf.c socket.c udpip.c mace_ec.c

include $(LIBSKRULES)
MKDEPRULE= $(EVERYPRODUCT_MKDEPRULE)
