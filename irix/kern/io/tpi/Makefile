#!smake
# Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
#
# Makefile for SGI kernel TPI facilities.
#
#ident	"$Revision: 1.15 $"

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/io/tpi
.PATH:$(KPATH)

HDRFILES= tpisocket.h

TCOOSED = /bin/sed -e s/ticots/ticotsord/g -e s/tco/tcoo/g -e s/TCO/TCOO/g

LIB_NAME= tpisocket.a
LIB_SRCS= tpisocket.c
LIB_OBJS= $(LIB_SRCS:.c=.o)

TPI_SRCS= sockmod.c \
	ticots.c timod.c tirdwr.c \
	tpitcp.c tpiudp.c \
	tpiicmp.c tpirawip.c	

TPI_OBJS= $(TPI_SRCS:.c=.o) ticotsord.o
KDIRT	= $(KPATH)/ticotsord.c

$(LIB_NAME):$(LIB_NAME)($(LIB_OBJS)) MAKELIB

TARGETS	= $(LIB_NAME) $(TPI_OBJS)
.PRECIOUS:$(TARGETS)

LINTNAME= tpisocket
CFILES	= $(LIB_SRCS) $(TPI_SRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${LBOOTPOSTOP} -idb svr4net ${LIB_NAME}
	${KINSTALL} -idb svr4net ${TPI_OBJS}
	${KDEVINSTALL} ${TARGETS}

ticotsord.o: $(KPATH)/ticots.c
	$(TCOOSED) < $(KPATH)/ticots.c > $(KPATH)/ticotsord.c
	$(CC) $(CFLAGS) -DTICOTSORD -c $(KPATH)/ticotsord.c 
	$(LDF) -r $(.TARGET:T) -o $$$$.o && mv $$$$.o $(.TARGET:T) 
	/bin/rm -f $(KPATH)/ticotsord.c


#headers!
#	${INSTALL} -m 444 -F /usr/include/tpi $(HDRFILES)
