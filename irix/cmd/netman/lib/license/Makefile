#!smake
#
#	Makefile for libnetman/license
#
#	$Revision: 1.6 $
#

LIBDEPTH=..
include ${LIBDEPTH}/Makedefs

#if defined(BUILD) && ${BUILD} == "netvis"
LICENSE= -DLICENSE
#else
LICENSE= -ULICENSE
#endif

CFILES= license.c
NCOPTS= -dollar
NCINCS= -I${INCLDIR}/idl/c
NCDEFS= ${LICENSE}

TARGETS= Marker

default: ${TARGETS}

include ${LIBDEPTH}/Makerules
