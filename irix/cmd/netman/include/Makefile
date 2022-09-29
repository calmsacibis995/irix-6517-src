#!smake
#!smake
#
#	Makefile for netvis/include/protocols
#
#	$Revision: 1.4 $
#
include ../Makedefs
include $(ROOT)/usr/include/make/commondefs
HEADERS_SUBDIRS= protocols net sys

INST= $(INSTALL) -F ${HEADERSPATH} -m 444

IFILES= address.h addrlist.h cache.h datastream.h debug.h enum.h exception.h \
	expr.h heap.h histogram.h index.h license.h macros.h netlook.h \
	packetbuf.h packetview.h protocol.h protodefs.h protoid.h \
	protoindex.h protostack.h scope.h snoopd_rpc.h snooper.h \
	snoopstream.h strings.h

default exports: install

include $(COMMONRULES)

headers install: $(_FORCE)
	$(INST) $(IFILES)
	$(HEADERS_SUBDIRS_MAKERULE)
