#!smake
#
# Makefile for lib/ftp, the home of ftp protocol modules.
#
LIBDEPTH=..
include ${LIBDEPTH}/Makedefs

# NCINCS to be remove later and pidlflags to be changed
NCINCS= -I../pidlhacks
PIDLFLAGS= -I../pidlhacks

CFILES= ftp.c # hand crafting required.
#PIFILES= ftp.pi

TARGETS= Marker

default: ${TARGETS}

include ${LIBDEPTH}/Makerules

