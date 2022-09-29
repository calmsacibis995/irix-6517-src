#
# Makefile for sysctlrd, the system controller daemon
#
# $Revision: 1.11 $
#
include $(ROOT)/usr/include/make/commondefs

CFGDIR=	    /etc/config
TARGETS=    sysctlrd scmsg
LLDOPTS=    -Wl,-I,$(ROOTRLDNAME)
LCOPTS=     -fullwarn

LMKDEPFLAGS= $(NULLSUFFIX_MKDEPFLAG)

default: $(TARGETS)

sysctlrd: sysctlrd.o inventory.o
	$(CCF) -o sysctlrd sysctlrd.o inventory.o $(LDFLAGS)

include $(COMMONRULES)

install: default
	$(INSTALL) -idb "std.sw.unix" -idb "mach(CPUBOARD=IP19 CPUBOARD=IP21 CPUBOARD=IP25)" \
		-F /usr/etc sysctlrd
	$(INSTALL) -idb "std.sw.unix" -idb "mach(CPUBOARD=IP19 CPUBOARD=IP21 CPUBOARD=IP25)" \
		-F /sbin scmsg
	$(INSTALL) -idb "std.sw.unix" -idb "mach(CPUBOARD=IP19 CPUBOARD=IP21 CPUBOARD=IP25)" \
		-F /etc -lns ../sbin/scmsg scmsg
	$(INSTALL) -idb "std.sw.unix" -idb "config(noupdate)" \
		-idb "mach(CPUBOARD=IP19 CPUBOARD=IP21 CPUBOARD=IP25)" -m 644 -F $(CFGDIR) \
		-src cpumeter.config cpumeter
	$(INSTALL) -idb "std.sw.unix" -idb "config(noupdate)" \
		-idb "mach(CPUBOARD=IP19 CPUBOARD=IP21 CPUBOARD=IP25)" -m 644 -F $(CFGDIR) \
		-src cleanpowerdown.config cleanpowerdown
