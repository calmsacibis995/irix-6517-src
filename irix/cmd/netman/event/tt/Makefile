#!smake
#
# Makefile for the ToolTalk ptypes and any other ToolTalk stuff that is
# part of the NetVisualyzer event stuff.
#
# THIS IS NOT PART OF THE BUILD PROCESS. Only used on my developement
# system. But there must be an exit op for inst equivalent to the
# tt_type_comp line. And there must be an install target to put all the
# files in the right places to make the tooltalk data base.
#
# $Revision: 1.10 $

TTDEPTH=..
EVINC=${TTDEPTH}/include
include ../Makedefs
# TARGETS only for local development
TARGETS=/usr/people/probins/.tt/types.xdr
TTFILES=nveventd_types
INCFILE=ttEventdOps.h
TTINCFILE= ${EVINC}/${INCFILE}
MODE=-m 444
DIROWN=-m 755 -u root -g sys
INSTDIR= /etc/tt
INSTFILE=/dev/null
IDB_TAG = netvis.sw.display
EXITOP = exitop("(cd ${NV_TARGET_LIB};tt_type_comp -s -d system ${TTFILES}; :) >/dev/null 2>&1")

# This is for building during development
#default: ${TARGETS}

default:


include ../Makerules
	
#for building during development -- local for testing only
#${TARGETS}: ${TTFILES}
#	ln -s ${TTINCFILE} .
#	tt_type_comp  -d user ${TTFILES} 
	
#The actual install on a target system by inst.
install: 
	${INSTALL}  -idb ${IDB_TAG} ${MODE} -F ${NV_INSTALL_EVENT} -src ${TTINCFILE} \
	 	   -O ${INCFILE}
	${INSTALL} -lns $(NV_INSTALL_EVENT)/${INCFILE} -idb $(NV_DISP_IDB_SW_LINKS) \
	-F $(NV_TARGET_LIB) ${INCFILE}
	
	${INSTALL} -idb ${IDB_TAG} ${MODE} -F $(NV_INSTALL_EVENT) ${TTFILES}
	${INSTALL}  -lns $(NV_INSTALL_EVENT)/${TTFILES}  -idb $(NV_DISP_IDB_SW_LINKS) \
	-F $(NV_TARGET_LIB) ${TTFILES}
				
	$(INSTALL) -idb '$(IDB_TAG) $(EXITOP)' ${DIROWN} -dir ${INSTDIR}
