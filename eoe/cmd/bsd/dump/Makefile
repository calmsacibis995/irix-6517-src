#!smake
#
# Makefile for dump
#
# ident "$Revision: 1.12 $"
#
#	dump.h			header file
#	dumpitime.c		reads /etc/dumpdates
#	dumpmain.c		driver
#	dumpoptr.c		operator interface
#	dumptape.c		handles the mag tape and opening/closing
#	dumptraverse.c		traverses the file system
#	unctime.c		undo ctime
#	dumpdir.c		dump directories in bsd format
#
#	DEBUG			use local directory to find ddate and dumpdates
#	TDEBUG			trace out the process forking

include	${ROOT}/usr/include/make/commondefs

CFILES	= dumpitime.c dumpmain.c dumpoptr.c dumprmt.c dumptape.c \
		dumptraverse.c unctime.c dumpdir.c
OBJS	= ${CFILES:.c=.o}
TARGETS	= dump

LCDEFS	= -D_BSD_SIGNALS -D_DUMP_
LCOPTS	= -fullwarn
LLDLIBS	= -ldisk
LDIRT	= rdump
WOFF	=

default: ${TARGETS} rdump

include ${COMMONRULES}

install: default
	${INSTALL} -F /usr/sbin ${TARGETS}
	${INSTALL} -F /usr/sbin -ln dump rdump 
	${INSTALL} -F /etc -lns ../usr/sbin/dump dump 
	${INSTALL} -F /etc -lns ../usr/sbin/rdump rdump 
	${INSTALL} -F /sbin -lns ../usr/sbin/dump dump 
	${INSTALL} -F /sbin -lns ../usr/sbin/rdump rdump 

dump: ${OBJS}
	${CCF} -o $@ ${OBJS} ${LDOPTS} ${LLDLIBS}

rdump: dump
	rm -f rdump
	ln dump rdump

$(OBJS): ${@:.o=.c}
	${CCF} -c ${@:.o=.c}
