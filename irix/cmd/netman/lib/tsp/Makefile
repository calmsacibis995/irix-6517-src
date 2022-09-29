#!smake
#
# Makefile for lib/tsp, the home of Time Syncronization Protocol modules.
#
LIBDEPTH=..
include ${LIBDEPTH}/Makedefs

# NCINCS to be remove later and pidlflags to be changed
NCINCS= -I../pidlhacks
PIDLFLAGS= -I../pidlhacks

PIFILES= tsp.pi

TARGETS= Marker

default: ${TARGETS}

include ${LIBDEPTH}/Makerules
