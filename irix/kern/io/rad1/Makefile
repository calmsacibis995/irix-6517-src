#!smake
# $Revision: 1.2 $

DEPTH?=../..
include $(ROOT)/usr/include/make/kcommondefs
include $(ROOT)/usr/include/make/gfxkcommondefs

# use kcommrules KINSTALL
#KINSTALL=$(INSTALL) -F $(SHIP_CPUINSTDIR) -m 444 -idb "nostrip mach(CPUBOARD=IP27)"
KPATH=$(TOP)/io/rad1
.PATH:$(KPATH)


LCOPTS += -woff 1412,1413,1498,1174,1552,1196


RAD1_NAME=rad1pci.a
RAD1_CSRCS=rad1drv.c rad1_init.c rad1_tpinit.c rad1_tport.c rad1data.c

RAD1_OBJS=$(RAD1_CSRCS:.c=.o)
$(RAD1_NAME):$(RAD1_NAME)($(RAD1_OBJS)) MAKELIB

TARGETS=$(RAD1_NAME)

LINTNAME=rad1pci
CFILES=$(RAD1_CSRCS)

#
# Rules
#
include $(ROOT)/usr/include/make/kcommonrules

$(KCOMMONPREF)default:$(TARGETS)
	${KLINKINSTALL} ${TARGETS}

$(KCOMMONPREF)install: $(KCOMMONPREF)default
	${KINSTALL} ${TARGETS}
	${KDEVINSTALL} ${TARGETS}

#
#		
