#!smake
#
# Makefile for lib/runix, the home of rlogin, rsh, rcp protocol modules.
#
LIBDEPTH=..
include ${LIBDEPTH}/Makedefs

NCINCS= -I../pidlhacks
PIDLFLAGS= -I../pidlhacks

CFILES= rlogin.c  # hand crafting required.
#PIFILES= rlogin.pi

TARGETS= Marker

default: ${TARGETS}

include ${LIBDEPTH}/Makerules

