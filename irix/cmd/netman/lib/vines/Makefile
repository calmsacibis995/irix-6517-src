#!smake
#
# Makefile for lib/vines, the home of Banyan Vines protocol modules.
#
LIBDEPTH=..
include ${LIBDEPTH}/Makedefs

NCINCS=${LIBNET_INCS}
NCOPTS= -OPT:Olimit 1700
#if ${IRIX} == "SunOS"
NCOPTS=
#endif

PIDLFLAGS= -I../pidlhacks -I../llc -I../tokenring
PIFILES= vines.pi

TARGETS= Marker

default: ${TARGETS}

include ${LIBDEPTH}/Makerules
