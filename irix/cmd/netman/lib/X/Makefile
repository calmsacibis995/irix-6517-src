#!smake
#
# Makefile for lib/x, the home of X protocol modules.
#
LIBDEPTH=..
include ${LIBDEPTH}/Makedefs

# NCINCS to be remove later and pidlflags to be changed
NCINCS= -I../pidlhacks
PIDLFLAGS= -I../pidlhacks

PIFILES= x11.pi

TARGETS= Marker

default: ${TARGETS}

include ${LIBDEPTH}/Makerules
