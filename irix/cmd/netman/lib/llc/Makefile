#!smake
#
# Makefile for lib/llc
#
#ident "$$"
LIBDEPTH=..
include ${LIBDEPTH}/Makedefs

OPTIMIZER = -g

# The order of the following line is very important because of another llc.h
LCINCS= ${LIBNET_INCS}
NCINCS= ${LIBNET_INCS}
PIDLFLAGS= -I../pidlhacks -I../tokenring

PIFILES= llc.pi
LDIRT	+=llc.h

TARGETS= Marker

default: ${TARGETS}

include ${LIBDEPTH}/Makerules
