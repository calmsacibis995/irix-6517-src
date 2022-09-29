#!smake
# Copyright 1994 Silicon Graphics Inc.
#
# Makefile for the Data Management Interfaces.
#
#ident "$Revision: 1.13 $"

DEPTH?=../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/io/dmi
.PATH:$(KPATH)

KCOPTS=	-fullwarn $(XFSCOPTS) -DDMAPI_ON_KUDZU

DMI_NAME=dmi.a
DMI_CSRCS=dmi_attr.c dmi_bulkattr.c dmi_config.c dmi_dmattr.c dmi_event.c \
	dmi_handle.c dmi_hole.c dmi_io.c dmi_mountinfo.c dmi_region.c \
	dmi_register.c dmi_right.c dmi_session.c dmi_sysent.c

DMI_OBJS=${DMI_CSRCS:.c=.o}
$(DMI_NAME):$(DMI_NAME)($(DMI_OBJS)) MAKELIB

DMIIDBG_NAME=dmiidbg.o
DMIIDBG_CSRCS=dmiidbg.c

TARGETS=$(DMI_NAME) $(DMIIDBG_NAME)
.PRECIOUS:$(TARGETS)

LINTNAME=dmi
CFILES=$(DMI_CSRCS) $(DMIIDBG_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${DMI_NAME}
	${KINSTALL} ${DMIIDBG_NAME}
	${KDEVINSTALL} ${TARGETS}

dmiidbg.o: dmiidbg.c
	$(CCF) -c $(JALR) -G 0 $(KPATH)/dmiidbg.c && \
	$(LDF) $(PRODOBJECT) -G 0 -r $(.TARGET:T) -o $$$$.o && \
	mv $$$$.o $(.TARGET:T)
