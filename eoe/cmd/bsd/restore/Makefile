#!smake
#
# Makefile for restore
#
# ident "$Revision: 1.15 $"
#
#	DEBUG			use local directory to find ddate and dumpdates
#	TDEBUG			trace out the process forking

include	${ROOT}/usr/include/make/commondefs

CFILES	= main.c interactive.c restore.c dirs.c symtab.c tape.c \
		utilities.c
OBJS	= ${CFILES:.c=.o}
TARGETS	= restore

LCDEFS=-D_BSD_SIGNALS -D_RESTORE_
LCINCS=-I../dump
LCOPTS=-fullwarn
LLDOPTS=-Wl,-I,$(ROOTRLDNAME)
LDIRT	= rrestore
WOFF	=

all default: ${TARGETS} rrestore

include ${COMMONRULES}

install: default
	${INSTALL} -F /sbin ${TARGETS}
	${INSTALL} -F /sbin -ln restore rrestore 
	${INSTALL} -F /etc -lns ../sbin/restore restore 
	${INSTALL} -F /etc -lns ../sbin/rrestore rrestore 

restore: ${OBJS} dumprmt.o
	${CCF} -o $@ ${OBJS} dumprmt.o ${LDOPTS} ${LLDLIBS}

rrestore: restore
	rm -f rrestore
	ln restore rrestore

$(OBJS): ${@:.o=.c}
	${CCF} -c ${@:.o=.c}

dumprmt.o: ../dump/dumprmt.c
	${CCF} -c -I../restore ../dump/dumprmt.c
