#
# Makefile for installing Sun configuration files.
#
include	${ROOT}/usr/include/make/commondefs

INSTCONFIG= ${INSTALL} -F /etc/config -idb 'config(noupdate)' \
	    -u root -g sys -m 644

default: autofs cachefs nfs rarpd yp ypserv ypmaster automount lockd

include ${COMMONRULES}

install: default
	${INSTCONFIG} autofs automount cachefs nfs lockd
	${INSTCONFIG} rarpd
	${INSTCONFIG} ypmaster ypserv yp
