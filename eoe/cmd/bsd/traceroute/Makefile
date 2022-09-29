#
# Make the traceroute command
#
#ident $Revision: 1.9 $

include	$(ROOT)/usr/include/make/commondefs

CFILES=	traceroute.c
TARGETS=traceroute

LCOPTS=	-signed -D_BSD_TIME
LMKDEPFLAGS=${NULLSUFFIX_MKDEPFLAG}

default: $(TARGETS)

include $(COMMONRULES)

install: default
	$(INSTALL) -m 4555 -u root -g sys -F /usr/etc $(TARGETS)

traceroute: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@
