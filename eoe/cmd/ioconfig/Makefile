#!smake
#ident "$Revision: 1.10 $"

BASEVERSION=n32bit
include $(ROOT)/usr/include/make/cmdcommondefs

COMMANDS=ioconfig
HEADERS=ioconfig.h error.h
CFILES=error.c attr.c ioconfig.c
MISC=README
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)

default:$(TARGETS)

include $(CMDCOMMONRULES)

ioconfig: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

n32bitinstall: default
	${INSTALL} -m 755 -F /sbin ${COMMANDS}
	${INSTALL} -u root -g sys -dir /var/sysgen/ioconfig
	${INSTALL} -m 644 -u root -g sys -F /var/sysgen/ioconfig "$(MISC)"
	$(INSTALL) -F /var/sysgen/ioconfig -idb "mach(CPUBOARD=IP27)" \
		-m 444 -src ifcl.ioconfig ifcl
	$(INSTALL) -F /var/sysgen/ioconfig -idb "mach(CPUBOARD=IP27)" \
		-m 444 -src vme.ioconfig vme
