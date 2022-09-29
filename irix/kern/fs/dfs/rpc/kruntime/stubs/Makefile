#!smake
#
# Makefile for OSF Distributed File System
#
# $Revision: 1.2 $
DEPTH?=../../../../..
include ${DEPTH}/kcommondefs
include ${DEPTH}/fs/dfs/dfscommondefs
KPATH=$(TOP)/fs/dfs/rpc/kruntime/stubs
.PATH:$(KPATH)

KCINCS=		-I$(KPATH)/.. \
		-I$(KPATH)/../../sys_idl \
		-I$(ROOT)/usr/include \
		-I$(ROOT)/usr/include/dcedfs

KCDEFS +=	-UNCK -DINET -DPROT_NCADG -DAUTH_KRB -DAUTH_KRB_DG -DNAF_IP

LIB_NAME=	libknckstub.a

CFILES=		conv_cstub.c \
		convc_cstub.c \
		convc_sstub.c \
		ep_cstub.c \
		mgmt_cstub.c \
		mgmt_sstub.c

OBJS=		$(CFILES:.c=.o)
TARGETS=	$(LIB_NAME)
LIB_FILES=	$(OBJS)

$(LIB_NAME):$(LIB_NAME)($(LIB_FILES)) MAKELIB

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)

$(KCOMMONPREF)install:$(KCOMMONPREF)default

headers!

