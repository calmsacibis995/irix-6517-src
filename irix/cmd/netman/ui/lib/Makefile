#!smake
#
#	Makefile for NetVisualyzer UI library
#
#	$Revision: 1.4 $
#
DEPTH=../..
include ${DEPTH}/Makedefs

C++FILES= tuDialogBox.c++ tuBitmapTile.c++ \
	  dialog.c++ prompt.c++ tooloptions.c++ helpWin.c++
NC++OPTS = +a1 +w +p -float -Wf,-XNl8192
NC++INCS= -I.

TARGETS= ${LIBUINAME}

default: ${TARGETS}

include ${COMMONRULES}

${LIBUINAME}: ${OBJECTS}
	${AR} crl ${LIBUINAME} ${OBJECTS}

install exports: default
	${INSTALL} -F ${LIBINSTPATH} -m 444 -idb ${LIBIDB} ${LIBUINAME}
