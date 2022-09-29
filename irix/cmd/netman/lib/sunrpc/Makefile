#!smake
#
# Makefile for lib/sunrpc, the home of Sun RPC protocol modules.
#
LIBDEPTH=..
include ${LIBDEPTH}/Makedefs

NCINCS= -I../pidlhacks
LCINCS= -I../pidlhacks -I ${ROOT}/usr/include/netman
PIDLFLAGS= -I../pidlhacks

PIFILES= nlm.pi pmap.pi

TARGETS= Marker

default: ${TARGETS}

include ${LIBDEPTH}/Makerules
