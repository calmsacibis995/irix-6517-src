#!smake

DEPTH?=../../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/io/dlpi/llc2
.PATH:$(KPATH)

#CVERSION=-cckr
KCDEFS=-DSGI -DSVR4 -DNO_TRANS_LOOP
#KCOPTS=-r -Wc,-jalr
#KLDFLAGS=-d
#GNUMVAL=0
#LWOFF=,1174,1692

LLC_NAME=llc2.a
LLC_CSRCS=llc2.c llc2serve.c llc2state.c Space.c	\
	  llc2dlpi.c llc2match.c

LLC_OBJS=${LLC_CSRCS:.c=.o}
$(LLC_NAME):$(LLC_NAME)($(LLC_OBJS)) MAKELIB
#$(LLC_NAME):$(LLC_NAME)($(LLC_OBJS))
#	$(LDF) -r -o $(LLC_NAME) $(LLC_OBJS)
#	-rm -f $(LLC_OBJS)

TARGETS=$(LLC_NAME)
.PRECIOUS:$(TARGETS)

CFILES=$(LLC_CSRCS)

KDIRT=$(LLC_OBJS)

#
# Rules
#
include ${DEPTH}/kcommonrules
sinclude klocaldefs

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} -idb DLPI ${TARGETS}
	${KDEVINSTALL} -idb KDEV ${TARGETS}
