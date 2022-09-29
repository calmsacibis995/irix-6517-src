#!smake
# Makefile for /etc/rc script to start rfindd.
#
include ../commondefs

default: rfindd

include ../commonrules

install: default
	${INSTALL} ${USR_GRP} -F /etc/init.d -idb "config(update)" -m 755 rfindd
	${INSTALL} -F /etc/rc2.d -lns ../init.d/rfindd S98rfindd
	${INSTALL} -F /etc/rc0.d -lns ../init.d/rfindd K32rfindd
