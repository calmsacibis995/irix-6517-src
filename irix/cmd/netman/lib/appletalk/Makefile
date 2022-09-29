#!smake
#
# Makefile for lib/appletalk, the home of AppleTalk protocol modules.
#
#ident "$Revision: 1.13 $"
LIBDEPTH=..
include ${LIBDEPTH}/Makedefs

NCINCS= -I../llc -I${DEPTH}/include -I${DEPTH}/include/protocols
LCINCS= -I../llc -I${DEPTH}/include -I${DEPTH}/include/protocols
NCOPTS= -OPT:Olimit 2400
#if ${IRIX} == "SunOS"
NCOPTS=
#endif
PIDLFLAGS= -I../pidlhacks -I../llc -I../tokenring

CFILES= ddp_host.c
PIFILES= aarp.pi adsp.pi aep.pi afp.pi asp.pi atp.pi ddp.pi elap.pi nbp.pi \
        pap.pi rtmp.pi zip.pi
LDIRT+=	atp.h ddp.h elap.h

TARGETS= Marker

default: ${TARGETS}

adsp.c: ddp.c

afp.c: atp.c elap.c

include ${LIBDEPTH}/Makerules
