#!smake
# Copyright 1992 Silicon Graphics, Inc.  All rights reserved.
#
# Makefile for SGI kernel checkpoint/restart facilities.
#
#ident	"$Revision: 1.11 $"

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/os/cpr
.PATH:$(KPATH)

HDRFILES= ckpt_sys.h	\
	ckpt_procfs.h

LIB_NAME= cpr.a
#if defined(CPUBOARD) && ($(CPUBOARD) == "IP19" || $(CPUBOARD) == "IP21" || \
	 		  $(CPUBOARD) == "IP22" || \
	 		  $(CPUBOARD) == "IP25" || $(CPUBOARD) == "IP27" || \
	 		  $(CPUBOARD) == "IP20" || $(CPUBOARD) == "IP26" || \
			  $(CPUBOARD) == "IP28" || $(CPUBOARD) == "IP30" || \
			  $(CPUBOARD) == "IP32" || $(CPUBOARD) == "IP33")
LIB_SRCS= 	\
	ckpt_procfs.c	\
	ckpt_sys.c	\
	ckpt_util.c

LIB_ASRCS=
#else
LIB_SRCS=
LIB_ASRCS=
#endif
IO_OBJS=${IO_CSRCS:.c=.o} ${IO_ASRCS:.s=.o}
LIB_OBJS= $(LIB_SRCS:.c=.o) ${LIB_ASRCS:.s=.o}

$(LIB_NAME):$(LIB_NAME)($(LIB_OBJS)) MAKELIB

TARGETS	= $(LIB_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME= ckpt
CFILES	= $(LIB_SRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

# This default rule is always invoked in BOOTAREA
$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install:	$(KCOMMONPREF)default
	${KINSTALL} ${TARGETS}
	${KDEVINSTALL} ${TARGETS}

#	install header files under ${ROOT}/usr/include
headers!
	${INSTALL} -m 444 -F /usr/include/sys ${HDRFILES}

