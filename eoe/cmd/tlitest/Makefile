#!smake
#
# Main makefile for TLI test programs
#

include	$(ROOT)/usr/include/make/commondefs


CFILES	= sendpkt.c recvpkt.c accept.c connect.c netdir.c

TARGETS	= ${CFILES:.c=}

FILES = 

# When appropriate (4.4 port?) restore -lbsd and let stand BSD setpgrp etc.
LLDLIBS = -lsocket -lnsl
#LLDLIBS = -lnsl
#LLDLIBS = -lsocket -lnsl -ldl
#LLDLIBS = -lsocket -lnsl -ldl $(ROOT)/usr/lib/tcpip.so
#LLDLIBS = -lsocket -lnsl $(ROOT)/usr/lib/tcpip.so
#LLDLIBS = -lsocket -lnsl -ldl $(ROOT)/usr/lib/tcpip.so

LLDOPTS = -Wl,-transitive_link

#LCOPTS	= -g
LCOPTS	= -call_shared -g

default: $(TARGETS) 

include $(COMMONRULES)

install: default
	$(INSTALL) -F /sbin $(TARGETS)
