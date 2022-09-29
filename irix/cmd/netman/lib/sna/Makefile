#!smake
#
# Makefile for lib/tokenring, the home of Tokenring protocol modules.
#
#ident "$$"
LIBDEPTH=..
include ${LIBDEPTH}/Makedefs

OPTIMIZER = -g

NCINCS= -I../llc
PIDLFLAGS= -I../pidlhacks -I../llc -I../tokenring

CFILES=
PIFILES= sna.pi

TARGETS= Marker

default: ${TARGETS}

include ${LIBDEPTH}/Makerules
