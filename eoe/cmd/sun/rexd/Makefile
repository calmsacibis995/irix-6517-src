#
# Makefile for rexd, the Remote EXecution Daemon.
#
include ${ROOT}/usr/include/make/commondefs

CVERSION=-cckr

LCINCS=-I${INCLDIR}/rpcsvc
LLDLIBS= -lrpcsvc

HFILES= rexioctl.h
CFILES= rexd.c mount_nfs.c on.c where.c where_main.c unix_login.c sgi.c
TARGETS= where on rpc.rexd

default: ${TARGETS}

include ${COMMONRULES}

rpc.rexd: rexd.o mount_nfs.o unix_login.o sgi.o
	${CCF} -o $@ rexd.o mount_nfs.o unix_login.o sgi.o $(LDFLAGS)

on: on.o where.o sgi.o
	${CCF} -o $@ on.o where.o sgi.o $(LDOPTS) -ltermcap $(LDLIBS)

where: where_main.o where.o
	${CCF} -o $@ where_main.o where.o $(LDFLAGS)

install: default
	# Install "on" execute-only so mortals can't copy it (for security)
	${INSTALL} -F /usr/bin -m 711 -u root -g sys on
	${INSTALL} -F /usr/bin -m 755 -u root -g sys where
	${INSTALL} -F /usr/etc -m 755 -u root -g sys  rpc.rexd
