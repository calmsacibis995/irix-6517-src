#!smake
#
# Makefile for kernel synchronization routines
#
# $Revision: 1.10 $

DEPTH?=../..

include ${DEPTH}/kcommondefs

# Compile ksync sources multiple times to generate packages implementing
# debug, metered and named ksync objects.
#
TLINKDIR_MAKERULE= \
	for opt in metered named statistics; do \
		tlink -r -x '^statistics' -x '^metered' -x '^named' -x '^Make.*' . $$opt; \
		echo "\tcd $$opt; $(MAKE) $${RULE:=$@}"; \
		cd $$opt; \
		ln -sf ../Makefile.$$opt ./Makefile; \
		${MAKE} $${RULE:=$@}; \
		cd ..; \
	done
KDIRT+=$(KPATH)/statistics $(KPATH)/metered $(KPATH)/named

KPATH=$(TOP)/os/ksync
.PATH:$(KPATH)
#ifnmake (clobber)
SUBDIRS_DEP+=doassym
#endif

KSYNC_NAME=ksync.a
KSYNC_CSRCS=mutex.c sema.c 
KSYNC_ASRCS=mrlock.s
KSYNC_OBJS=$(KSYNC_CSRCS:.c=.o) $(KSYNC_ASRCS:.s=.o)
$(KSYNC_NAME):$(KSYNC_NAME)($(KSYNC_OBJS)) MAKELIB

TARGETS=$(KSYNC_NAME)

LINTNAME=ksync
CFILES=$(KSYNC_CSRCS)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${KSYNC_NAME}
	${KDEVINSTALL} ${KSYNC_NAME}

headers!
	${INSTALL} -m 444 -F /usr/include/sys klstat.h
