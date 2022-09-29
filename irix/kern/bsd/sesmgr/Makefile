#!smake
#
# Makefile for SGI's socket implementation.
#
# $Revision: 1.9 $

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/bsd/sesmgr
.PATH:$(KPATH)

KCINCS=-I$(TOP)/bsd
KCOPTS=-signed -common

HDRFILES=sesmgr.h t6api_private.h t6attrs.h t6rhdb.h t6samp.h t6satmp.h \
	ip_secopts.h sm_private.h sesmgr_samp.h t6refcnt.h sesmgr_if.h

SESMGR_NAME=sesmgr.a
SESMGR_CSRCS=sesmgr.c sesmgr_ip.c sesmgr_samp.c \
		sesmgr_satmp.c sesmgr_so.c sesmgr_syscalls.c \
		sesmgr_t6api.c t6rhdb.c sesmgr_trace.c
SESMGR_OBJS=$(SESMGR_CSRCS:.c=.o)

IPSO_NAME=ip_sec.a
IPSO_CSRCS=ip_sec_tx.c ip_sec_rx.c ip_sec_cache.c
IPSO_OBJS=$(IPSO_CSRCS:.c=.o)
$(IPSO_NAME):$(IPSO_NAME)($(IPSO_OBJS)) MAKELIB

$(SESMGR_NAME):$(SESMGR_NAME)($(SESMGR_OBJS)) MAKELIB

TARGETS=$(SESMGR_NAME) $(IPSO_NAME)

LINTNAME=sesmgr
CFILES=$(SESMGR_CSRCS) $(IPSO_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${TARGETS}
	${KDEVINSTALL} ${TARGETS}

headers!
	${INSTALL} -m 444 -F /usr/include/sys $(HDRFILES)
