#
# Makefile for installing Documents (in PostScript format)
#

DEPTH=../..
include ${DEPTH}/Makedefs

CLIENT=FrameSchedProgManual.ps

FrameSchedProgManual.ps:	frs_pm4.ps
				cp frs_pm4.ps FrameSchedProgManual.ps

TARGETS= ${CLIENT}
default: ${TARGETS}
	

install: default
	$(INSTALL) -idb "react.sw.frs" -F /usr/react/docs ${CLIENT}

clobber:
	rm -f FrameSchedProgManual.ps
