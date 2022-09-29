#!smake
#
# Makefile for miscellaneous network routines.
#
# $Revision: 1.34 $

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/bsd/misc
.PATH:$(KPATH)

KCINCS	=-I$(TOP)/bsd
KCOPTS	=-signed -common

BSD_NAME=bsd.a
BSD_CSRCS=bitswap.c ether.c in_cksum.c kern_mbuf.c lance.c mclance.c \
	remque.c hashing.c bsd_icrash.c md5.c
BSD_OBJS=$(BSD_CSRCS:.c=.o)
$(BSD_NAME):$(BSD_NAME)($(BSD_OBJS)) MAKELIB

BIDBG_NAME=bsdidbg.o
BIDBG_CSRCS=bsdidbg.c
BIDBG_OBJS=$(BIDBG_CSRCS:.c=.o)

FDDI_NAME=fddi.a
FDDI_CSRCS=smt.c pcm.c mpcm.c
FDDI_OBJS=$(FDDI_CSRCS:.c=.o)
$(FDDI_NAME):$(FDDI_NAME)($(FDDI_OBJS)) MAKELIB

TARGETS=$(BSD_NAME) $(FDDI_NAME) ${BIDBG_NAME}

LINTNAME=bsdmisc
CFILES=$(BSD_CSRCS) $(FDDI_CSRCS) ${BIDBG_CSRCS}

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}
#
# BSD_NAME gets installed at top level
#
$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} -idb "fddi.sw.fddi nostrip" $(FDDI_NAME)
	${KINSTALL} $(BIDBG_NAME)
	${KDEVINSTALL} $(FDDI_NAME)
	${KDEVINSTALL} $(BIDBG_NAME)

# Always optimize the checksum routine to reduce the performance difference
#   between debugging and optimized kernels.
in_cksum.o: in_cksum.c
	$(CC) $(CVERSION) $(LCFLAGS) $(GCFLAGS) $(KPATH)/in_cksum.c -c

headers!
	${INSTALL} -m 444 -F /usr/include/sys seeq.h
	${INSTALL} -m 444 -F /usr/include/sys/misc ether.h

bsd_icrash.o: bsd_icrash.c
	$(CC) $(CFLAGS) -woff 1174 -g3 -c $(.IMPSRC) && \
	$(LDF) -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)

bsdidbg.o: bsdidbg.c
	$(CCF) -c $(JALR) -G 0 $*.c
