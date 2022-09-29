#!smake
# Make the ftp demon
# $Revision: 2.23 $

include	$(ROOT)/usr/include/make/commondefs

FILEPERM= -m 644 -u root -g sys
NOUPDATE= $(FILEPERM) -idb "config(noupdate)"
CFILES	= ftpd.c popen.c vers.c uio.c
YFILES	= ftpcmd.y
LDIRT	= vers.c ftpcmd.c

TARGETS	= ftpd

LCOPTS	= -woff 1116 -signed
LCDEFS	= -D_BSD_SIGNALS -Dvfork=fork -D_SHAREII -DDCE -DAFS
LLDOPTS = -Wl,-T,10000000,-D,10040000,-woff,85,-woff,134 -ignore_unresolved
LLDLIBS	= -lcrypt -liaf -lpthread

default:$(TARGETS)

install:default
	$(INSTALL) -F /usr/etc $(TARGETS)
	$(INSTALL) -F /etc $(NOUPDATE) ftpusers

include $(COMMONRULES)

ftpd:	$(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

vers.c:	ftpd.c ftpcmd.y newvers.sh
	sh newvers.sh
