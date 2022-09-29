#!smake
#
# Makefile for lib/novell, the home of Novell protocol modules.
#
LIBDEPTH=..
include ${LIBDEPTH}/Makedefs

NCINCS= -I../llc -I${ROOT}/usr/include/netman -I${DEPTH}/include/protocols
LCINCS= -I../llc -I${ROOT}/usr/include/netman -I${DEPTH}/include/protocols
PIDLFLAGS= -I../pidlhacks -I../llc -I../tokenring

PIFILES =ipx.pi spx.pi novellrip.pi
LDIRT	+=ipx.h

TARGETS= Marker

default: ${TARGETS}

include ${LIBDEPTH}/Makerules
