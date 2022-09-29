#
# Makefile for mmscd, the system controller daemon
#

include $(ROOT)/usr/include/make/commondefs

CFGDIR = /etc/config
TARGETS = mmscd
LLDOPTS= -Wl,-I,$(ROOTRLDNAME) -g
LCOPTS=-fullwarn
LMKDEPFLAGS= $(NULLSUFFIX_MKDEPFLAG)

CFILES = main.c cmd.c mmsc.c load.c mem.c double_time.c error.c

default: $(TARGETS)

mmscd: $(OBJECTS)
	$(CCF) -o mmscd $(OBJECTS) $(LDFLAGS) -lm

include $(COMMONRULES)

install: default
	$(INSTALL) -idb "std.sw.unix" -idb "mach(CPUBOARD=IP27)" \
		-F /usr/etc mmscd
	$(INSTALL) -idb "std.sw.unix" -idb "config(noupdate)" \
		-idb "mach(CPUBOARD=IP27)" -m 644 -F $(CFGDIR) \
		-src mmscd.config mmscd
	$(INSTALL) -idb "std.sw.unix" -idb "config(noupdate)" \
		-idb "mach(CPUBOARD=IP27)" -m 644 -F $(CFGDIR) \
		-src cpumeter.config cpumeter
	$(INSTALL) -idb "std.sw.unix" -idb "config(noupdate)" \
		-idb "mach(CPUBOARD=IP27)" -m 644 -F $(CFGDIR) \
		-src cleanpowerdown.config cleanpowerdown

#
# Curses-based load graph for testing and adjusting load.c
#

loadtest: loadtest.c load.c double_time.c
	cc -g -o loadtest loadtest.c load.c double_time.c error.c -lcurses -lm
