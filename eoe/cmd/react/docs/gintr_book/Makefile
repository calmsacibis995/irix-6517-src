#
# Makefile for installing Documents (in PostScript format)
#

DEPTH=../..
include ${DEPTH}/Makedefs

CLIENT=GroupIntrKernelInterface.ps

GroupIntrKernelInterface.ps:	groupintr.ps
				cp groupintr.ps GroupIntrKernelInterface.ps

TARGETS= ${CLIENT}
default: ${TARGETS}
	

install: default
	$(INSTALL) -idb "react.sw.frs" -F /usr/react/docs ${CLIENT}

clobber:
	rm -f GroupIntrKernelInterface.ps
