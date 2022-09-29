#!smake
#
# Makefile for lib/telnet, the home of the telnet protocol module.
#
#	$Revision: 1.2 $
#
LIBDEPTH=..
include ${LIBDEPTH}/Makedefs

NCINCS= -I../pidlhacks
PIDLFLAGS= -I../pidlhacks

CFILES= telnet.c  # hand crafting required.
#PIFILES= telnet.pi

TARGETS= Marker

default: ${TARGETS}

include ${LIBDEPTH}/Makerules
