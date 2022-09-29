#!smake
#
# Makefile for lib/netbios, the home of netbios protocol modules.
#
LIBDEPTH=..
include ${LIBDEPTH}/Makedefs

# NCINCS to be remove later and pidlflags to be changed
NCINCS= -I../pidlhacks -I../llc
PIDLFLAGS= -I../pidlhacks -I../llc -I../tokenring

PIFILES= netbios.pi

TARGETS= Marker

default: ${TARGETS}

include ${LIBDEPTH}/Makerules

