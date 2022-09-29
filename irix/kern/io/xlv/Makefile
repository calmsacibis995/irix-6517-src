#!smake
# Copyright 1993, Silicon Graphics Inc., Mountain View, CA. 
#
# Makefile for the XLV driver core
#
#ident "$Revision: 1.26 $"

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/io/xlv
.PATH:$(KPATH)

KCOPTS=	-fullwarn $(XFSCOPTS)

XLV_NAME=xlv.a
XLV_CSRCS= xlv.c xlv_attr.c xlv_icrash.c xlv_ktab.c xlv_lock.c xlv_merge.c \
	xlv_root.c xlv_mem.c xlv_xlate.c xlvd_failover.c
XLV_OBJS=${XLV_CSRCS:.c=.o}
$(XLV_NAME):$(XLV_NAME)($(XLV_OBJS)) MAKELIB

XLV_PLEX_NAME=xlv_plex.a
XLV_PLEX_CSRCS=plex_xlvd.c plex_klabd.c plex_block_map.c plex_cpy.c
XLV_PLEX_OBJS=${XLV_PLEX_CSRCS:.c=.o}
$(XLV_PLEX_NAME):$(XLV_PLEX_NAME)($(XLV_PLEX_OBJS)) MAKELIB

XLVIDBG_NAME=xlvidbg.o
XLVIDBG_CSRCS=xlvidbg.c

TARGETS=$(XLV_NAME) $(XLV_PLEX_NAME) $(XLVIDBG_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=xlv
CFILES=$(XLV_CSRCS) $(XLV_PLEX_CSRCS) $(XLVIDBG_CSRCS)
HFILES=xlv_ioctx.h xlv_procs.h xlv_xlate.h

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${XLV_NAME}
	${KINSTALL} ${XLV_PLEX_NAME}
	${KINSTALL} ${XLVIDBG_NAME}
	${KDEVINSTALL} ${TARGETS}

xlvidbg.o: xlvidbg.c
	$(CCF) -c $(JALR) -G 0 $(KPATH)/xlvidbg.c && \
	$(LDF) $(PRODOBJECT) -G 0 -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)

xlv_icrash.o: xlv_icrash.c
	$(CC) $(CFLAGS) -woff 1174 -g3 -c $(.IMPSRC) && \
	$(LDF) -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)
