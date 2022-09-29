#!smake
#
# Makefile for lib/smtp, the home of smtp protocol modules.
#
LIBDEPTH=..
include ${LIBDEPTH}/Makedefs

# NCINCS to be remove later and pidlflags to be changed
NCINCS= -I../pidlhacks
PIDLFLAGS= -I../pidlhacks

PIFILES= smtp.pi

TARGETS= Marker

default: ${TARGETS}

include ${LIBDEPTH}/Makerules

