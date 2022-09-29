#!smake

DEPTH?=../../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/io/dlpi/xtimer
.PATH:$(KPATH)

#CVERSION= -cckr
#KCINCS=-I.. -I$(ROOT)/include
#KCOPTS=-r -Wc,-jalr
#KLDFLAGS=-d
#GNUMVAL=0

XTIMER_NAME=xtimer.a
XTIMER_CSRCS=xtimer.c
XTIMER_OBJS=${XTIMER_CSRCS:.c=.o}
$(XTIMER_NAME):$(XTIMER_NAME)($(XTIMER_OBJS)) MAKELIB

#$(XTIMER_NAME):$(XTIMER_NAME)($(XTIMER_OBJS))
#	$(LDF) -r -o $(XTIMER_NAME) $(XTIMER_OBJS)
#	-rm -f $(XTIMER_OBJS)

TARGETS=$(XTIMER_NAME)

#
# Rules
#
include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} -idb DLPI ${TARGETS}
	${KDEVINSTALL} -idb KDEV ${TARGETS}
