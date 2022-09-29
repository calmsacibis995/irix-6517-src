#!smake
#
# Makefile for lib/tokenring, the home of Tokenring protocol modules.
#
# $Revision: 1.3 $
#
LIBDEPTH=..
include ${LIBDEPTH}/Makedefs

# The order of the following line is very important because of another llc.h
LCINCS= -I../pidlhacks -I${ROOT}/usr/include/netman -I${ROOT}/usr/include/netman/protocols
NLCINCS= -I../pidlhacks -I${ROOT}/usr/include/netman -I${ROOT}/usr/include/netman/protocols
PIDLFLAGS= -I../pidlhacks

CFILES= tokenring.c tokenmac.c # hand craft required
#PIFILES= tokenllc.pi tokenring.pi tokenmac.pi

TARGETS= Marker

default: ${TARGETS}

# XXX Copy ${MAKERULES} here until .pi files are built.
#
# Make rules for libnetman and its subdirectories.
#
include ${COMMONRULES}

Marker: ${OBJECTS} ${PIFILES:.pi=.o}
	${AR} rl ${LIBPATH} $?
	@touch $@

${LIBNAME} ${LIBPATH}: Marker

.PRECIOUS: ${LIBNAME} ${LIBPATH}

depend: ${DEPFILES}
