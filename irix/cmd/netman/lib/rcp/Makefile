#!smake
#
# Makefile for lib/rcp, the home of rcp protocol modules.
#
LIBDEPTH=..
include ${LIBDEPTH}/Makedefs

NCINCS= -I../pidlhacks
PIDLFLAGS= -I../pidlhacks

CFILES= rcp.c # hand crafting required.
#PIFILES= rcp.pi

TARGETS= Marker

default: ${TARGETS}

include ${LIBDEPTH}/Makerules

