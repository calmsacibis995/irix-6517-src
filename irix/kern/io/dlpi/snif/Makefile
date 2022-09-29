#!smake

DEPTH?=../../..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/io/dlpi/snif
.PATH:$(KPATH)

#CVERSION=-cckr
KCDEFS=-DSGI -DFDDI -DTOKENRING -DSINK
KCINCS=-I$(ROOT)/usr/include/sys
#KCOPTS=-r -Wc,-jalr
#KLDFLAGS=-d
#GNUMVAL=0

SNIF_NAME=snif.a
SNIF_CSRCS=snif.c #Space.c
SNIF_OBJS=${SNIF_CSRCS:.c=.o}

$(SNIF_NAME):$(SNIF_NAME)($(SNIF_OBJS)) MAKELIB
#$(SNIF_NAME):$(SNIF_NAME)($(SNIF_OBJS))
#	$(LDF) -r -o $(SNIF_NAME) $(SNIF_OBJS)
#	-rm -f $(SNIF_OBJS)

TARGETS=$(SNIF_NAME)
.PRECIOUS:$(TARGETS)

CFILES=$(SNIF_CSRCS)

KDIRT=$(SNIF_OBJS)

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
