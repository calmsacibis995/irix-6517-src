#!smake
#
# Makefile for lib/snmp, the home of SNMP protocol modules.
#
LIBDEPTH=..
include ${LIBDEPTH}/Makedefs

# NCINCS to be remove later and pidlflags to be changed
NCINCS= -I../pidlhacks
LCINCS= -I../pidlhacks -I${ROOT}/usr/include/netman
PIDLFLAGS= -I../pidlhacks

PIFILES= snmp.pi

TARGETS= Marker

default: ${TARGETS}

include ${LIBDEPTH}/Makerules
