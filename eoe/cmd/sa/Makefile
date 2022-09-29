#!smake
#ident	"$Revision: 1.35 $"
#
# System performance monitoring and administration.

BASEVERSION=32bit
ALTVERSIONS=64bit
WANTPARALLEL=yes-please

include	${ROOT}/usr/include/make/cmdcommondefs

LCOPTS=-fullwarn -diag_error 1-10000
WOFF=

# not doing sag yet

#if $(VERSION) == "32bit"
COMMANDS=sadc sar timex sa1 sa2 perf
#endif

#if $(VERSION) == "64bit"
COMMANDS=sadc sar
#endif

INSDIR= /usr/lib/sa
INS= ${INSTALL}
INSCON= ${INSTALL} -idb "config(suggest)"
INSNOUP= ${INSTALL} -idb "config(noupdate)"

CFILES= sadc.c saga.c sagb.c sar.c timex.c
HFILES= sa.h saghdr.h
SHFILES= perf.sh sa1.sh sa2.sh 

LLDLIBS+= $(.TARGET:Msadc:S/sadc/-ldisk/)

default: ${TARGETS}

include ${CMDCOMMONRULES}

saga:	saga.o sagb.o
	${CCF} saga.o sagb.o -o sag ${LDFLAGS}

32bitinstall: default
	${INS} -m 775 -u adm -g sys -dir /var/adm/sa
	${INS} -m 775 -u adm -g sys -idb "delhist" -dir /usr/adm/sa
	${INS} -m 755 -u sys -g sys -dir ${INSDIR}
	${INS} -m 2755 -g sys -f ${INSDIR} ${IDB_TAG32} sadc
	${INS} -F /usr/bin ${IDB_TAG32} sar 
	${INS} -F ${INSDIR} "sa2 sa1"
	${INS} -o -F /usr/bin timex
	${INSCON} -u root -g sys -m 744 -F /etc/init.d perf
	${INS} -ln ../init.d/perf -F /etc/rc2.d S21perf
	${INSCON} -u root -g sys -m 644 -F /var/spool/cron/crontabs sys
	${INSCON} -u root -g sys -m 644 -idb "delhist" -dir /usr/spool/cron/crontabs/sys
	${INSNOUP} -u root -g sys -m 644 -F /etc/config -src sar.config sar

64bitinstall: default
	${INS} -m 2755 -g sys -f ${INSDIR} ${IDB_TAG64} sadc
	${INS} -F /usr/bin ${IDB_TAG64} sar 
