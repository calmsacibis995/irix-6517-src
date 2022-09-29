#!smake
#
# Makefile for lib/osi over llc
#
#ident "$$"
LIBDEPTH=..
include ${LIBDEPTH}/Makedefs

OPTIMIZER = -g

NCINCS= -I../llc
PIDLFLAGS= -I../pidlhacks -I../llc -I../tokenring

CFILES=
PIFILES= osi.pi

TARGETS= Marker

default: ${TARGETS}

include ${LIBDEPTH}/Makerules
