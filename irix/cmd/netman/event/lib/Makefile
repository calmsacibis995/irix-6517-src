#!smake
#
#	Makefile for netvisualyzer event library
#
#	$Revision: 1.4 $
#
EVENTDEPTH=..
include ${EVENTDEPTH}/Makedefs
include ../../Makedefs

NC++INCS = -I ../include
C++FILES=obj_id.c++ event.c++ msg.c++
TARGETS=${LIBEVNAME}

default: ${TARGETS}

include ${COMMONRULES}

${LIBEVNAME}: ${OBJECTS}
	${AR} crl ${LIBEVNAME} $?

install exports: default
	${INSTALL} -F ${LIBINSTPATH} -m 444 -idb ${LIBIDB} ${LIBEVNAME}
