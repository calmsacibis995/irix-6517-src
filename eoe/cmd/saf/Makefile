#
# Makefile for SVR4 Service Access Controller (sac)
#
#ident	"$Revision: 1.11 $"

include	${ROOT}/usr/include/make/commondefs

LLDLIBS	= -lnsl
LLDOPTS = -Wl,-T,10000000,-D,10040000 -Wl,-woff,85
LMKDEPFLAGS= $(NULLSUFFIX_MKDEPFLAG)
LCDEFS	= -DPORTMAP -D_SVR4_TIRPC
CFILES	= sac.c readtab.c global.c log.c misc.c util.c
ROOTSYS	= -u root -g sys
ROOTSYSSU = -u root -g sys -idb "config(suggest)"
ROOTSYSNO = -u root -g sys -idb "config(noupdate)"
TARGETS	= sac

default: ${TARGETS}

include ${COMMONRULES}

sac: $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@

install: default
	${INSTALL} -F /usr/lib/saf -m 755 ${ROOTSYS}   -idb svr4net ${TARGETS}
	${INSTALL} -F /etc/saf     -m 600 ${ROOTSYS}   -idb svr4net _cmdpipe 
	${INSTALL} -F /etc/saf     -m 644 ${ROOTSYSSU} -idb svr4net _sactab _sysconfig
	${INSTALL} -F /var/saf     -m 644 ${ROOTSYSNO} -idb svr4net _log
	${INSTALL} -idb "delhist" -idb svr4net -dir /usr/saf/_log 
