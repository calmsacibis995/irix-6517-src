#
# Makefile for installing Documents (in PostScript format)
#

DEPTH=../..
include ${DEPTH}/Makedefs

CLIENT=TechnicalReport.ps

TechnicalReport.ps:	techrep.ps
			cp techrep.ps TechnicalReport.ps

TARGETS= ${CLIENT}
default: ${TARGETS}
	

install: default
	$(INSTALL) -idb "react.sw.frs" -F /usr/react/docs ${CLIENT}

clobber:
	rm -f TechnicalReport.ps
