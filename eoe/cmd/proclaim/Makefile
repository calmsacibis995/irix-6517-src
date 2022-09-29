#
# Makefile for proclaim client
#
#ident "$Revision: 1.11 $"

include	${ROOT}/usr/include/make/commondefs
sinclude ../Makedefs

TARGETS=proclaim
CHK_FILES=autoconfig_ipaddress.chkconfig proclaim.options
SH_FILES=run-proclaim dhcpcopt

# configuration files that should be discarded if user has one already
NOUPDATE=-idb "${EOE} config(noupdate)"

CFILES = dhclient.c funcs.c dhc0.c ui0.c bitswap.c
OBJECTS = dhclient.o funcs.o dhc0.o ui0.o bitswap.o
# SUBDIRS = man

default:${TARGETS} ${SH_FILES}

include ${COMMONRULES}

install:${SH_FILES} ${TARGETS} ${CHK_FILES}
	${INSTALL} -m 644 -F /etc/config ${NOUPDATE} \
		   -src autoconfig_ipaddress.chkconfig autoconfig_ipaddress
	${INSTALL} -m 644 -F /etc/config ${NOUPDATE} proclaim.options
	${INSTALL} -m 755 -F /etc/init.d ${EOE} -src run-proclaim.sh run-proclaim
	${INSTALL} -F /etc/rc2.d ${EOE} -lns ../init.d/run-proclaim S31proclaim
	${INSTALL} -F /usr/etc ${TCP} ${TARGETS}
	${INSTALL} -m 755 -F /usr/etc ${EOE} -src dhcpcopt.sh dhcpcopt


$(TARGETS): $(OBJECTS)
	$(CCF) $(OBJECTS) $(LDFLAGS) -o $@
