#
# Makefile for SVR4 listener
#
#ident	"$Revision: 1.12 $"

include	${ROOT}/usr/include/make/commondefs

LLDLIBS	= -lnsl
LMKDEPFLAGS= $(NULLSUFFIX_MKDEPFLAG)
#LCDEFS	= -DDEBUGMODE -DPORTMAP -D_SVR4_TIRPC
LCDEFS	= -DPORTMAP -D_SVR4_TIRPC
LLDOPTS = -Wl,-T,10000000,-D,10040000 -Wl,-woff,85
CFILES	= listen.c lsdata.c lsdbf.c lslog.c nlsaddr.c nstoa.c
#          nlps_serv.c lssmb.c 
FILES	= _pid _pmtab
ROOTSYS	= -m 644 -u root -g sys -idb "config(noupdate)"
FRIEND_HEADERS = listen.h

TARGETS= listen

default: ${TARGETS}

include ${COMMONRULES}

listen: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

install: default
	${INSTALL} -m 755 -u root -g sys -F /usr/lib/saf -idb svr4net ${TARGETS}
	${INSTALL} -F /etc/saf/tcp ${ROOTSYS} -idb svr4net ${FILES}
	${INSTALL} -F /var/saf/tcp ${ROOTSYS} -idb svr4net log
	${INSTALL} -idb "delhist" -idb svr4net -dir /usr/saf/tcp/log

headers: 
	${INSTALL} -F /usr/include -idb "noship" -m 444 ${FRIEND_HEADERS}
#
