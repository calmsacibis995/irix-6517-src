#!smake
# Makefile for rfind man pages.
#
include ../commondefs

MANPAGES=rfind.1 rfindd.1m fsdump.1m

include ../commonrules

install: ${MANPAGES}
	${INSTALL} -u bin -g bin -F /usr/man/u_man/man1 -idb "rfind.manpages noship" -m 644 ${MANPAGES}
