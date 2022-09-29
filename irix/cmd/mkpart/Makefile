#!smake
#
# Make file for the Partition config utility
#
# $Revision: 1.9 $
#

# BASEVERSION=32bit

include	$(ROOT)/usr/include/make/commondefs

MKPD_ON=	-idb "mach(CPUBOARD=IP27)"
MKPD_OFF=	-idb "mach(CPUBOARD=IP27)"

HFILES= 	mkpart.h mkpbe.h mkpd.h
CFILES= 	mkpart.c mkpbe.c mkplib.c mkpd.c	\
		mkprou.c

TARGETS= 	mkpart mkpbe mkpd
LCOPTS=		-fullwarn
LLDOPTS=    	-Wl,-I,$(ROOTRLDNAME)

LMKDEPFLAGS= $(NULLSUFFIX_MKDEPFLAG)

MKPART_OBJECTS=	mkpart.o mkplib.o mkprou.o
MKPBE_OBJECTS=	mkpbe.o
MKPD_OBJECTS=	mkpd.o mkplib.o mkprou.o

default: $(TARGETS)

include $(COMMONRULES)

mkpart: $(MKPART_OBJECTS)
	$(CCF) $(MKPART_OBJECTS) $(LDFLAGS) -o $@

mkpbe: $(MKPBE_OBJECTS)
	$(CCF) $(MKPBE_OBJECTS) $(LDFLAGS) -o $@

mkpd: $(MKPD_OBJECTS)
	$(CCF) $(MKPD_OBJECTS) $(LDFLAGS) -o $@

install: default
	$(INSTALL) -idb "std.sw.unix" -idb "mach(CPUBOARD=IP27)" \
                -F /usr/sbin $(TARGETS)
	$(INSTALL) $(MKPD_ON) -m 644 -F /etc/config -src mkpd.on -O mkpd
	$(INSTALL) $(MKPD_OFF) -m 644 -F /etc/config -src mkpd.off -O mkpd.off
	$(INSTALL) $(MKPD_ON) -m 644 -F /etc/config -src mkpart.config -O mkpart.config


