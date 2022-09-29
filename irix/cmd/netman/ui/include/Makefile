#!smake
#
#	Makefile for netvis/ui/include
#
#	$Revision: 1.1 $
#
DEPTH=../..
include ${DEPTH}/Makedefs

IFILES= dialog.h helpWin.h prompt.h tooloptions.h \
	tuBitmapTile.h tuBitmaps.h tuDialogBox.h

default exports:

headers install: ${_FORCE}
	${INSTALL} -m 0444 -F ${HEADERSPATH}/ui -idb ${HEADERSIDB} ${IFILES}

include ${MAKERULES}
