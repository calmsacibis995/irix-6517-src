#!smake
#
# Makefile for lib/decnet, the home of DECnet protocol modules.
#
LIBDEPTH=..
include ${LIBDEPTH}/Makedefs

NCINCS=${LIBNET_INCS}
LCINCS=${LIBNET_INCS}
NCOPTS= -OPT:Olimit 1700
#if ${IRIX} == "SunOS"
NCOPTS=
#endif

PIDLFLAGS= -I../pidlhacks -I../llc -I../tokenring
PIFILES= lat.pi

TARGETS= Marker

default: ${TARGETS}

include ${LIBDEPTH}/Makerules
