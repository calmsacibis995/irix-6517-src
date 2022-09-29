#!smake
#
# Makefile for frameview
#

include $(ROOT)/usr/include/make/commondefs


LLDLIBS=  -lGLw -lGL -lGLU -lXm -lXt -lX11 -lm -lSgm -lrtmon
OPTIMIZER = -g

CFILES= dialog.c file.c gr-cb.c gr-general.c gr-menu.c gr-setup.c graph-kern-cb.c graph-kern.c graph-kfunct-cb.c graph-kfunct.c graph-main-cb.c graph-main.c graph-proc-cb.c graph-proc.c graph-raw-cb.c graph-raw.c help-text.c init.c kf_colors.c kf_main.c logo.c main.c master-cb.c master.c no-frame.c stats.c

SFILES= atomicDec.s atomicInc.s 

CLIENTS= frameview

TARGETS= ${CLIENTS}

default: ${TARGETS}

frameview:	${CFILES:.c=.o} ${SFILES:.s=.o}
		$(CCF) -o frameview ${CFILES:.c=.o} ${SFILES:.s=.o} $(LDOPTS) $(LLDLIBS)

include ${COMMONRULES}


install:	default
		$(INSTALL) -F /usr/react/bin '$(TARGETS)'



