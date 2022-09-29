#!smake
#
# Makefile for lib/xns, the home of XNS protocol modules.
#
LIBDEPTH=..
include ${LIBDEPTH}/Makedefs

NCINCS= -I../llc -I../novell -I${DEPTH}/include/protocols
PIDLFLAGS= -I../pidlhacks -I../llc -I../novell -I../tokenring

PIFILES	=echo.pi pep.pi xnsrip.pi error.pi idp.pi spp.pi
LDIRT	+=idp.h

TARGETS= Marker

default: ${TARGETS}

echo.c: idp.c 

include ${LIBDEPTH}/Makerules
