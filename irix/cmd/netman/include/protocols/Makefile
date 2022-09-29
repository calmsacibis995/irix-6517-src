#!smake
#
#	Makefile for netvis/include/protocols
#
#	$Revision: 1.2 $
#
include ../../Makedefs
include $(ROOT)/usr/include/make/commondefs

INST= $(INSTALL) -F ${HEADERSPATH}/protocols -m 444

IFILES= arp.h bootp.h byteorder.h ddp_host.h decnet.h \
	decnet_nsp.h dnhost.h ether.h fddi.h hello.h ip.h mac.h nfs.h \
	smt.h sunrpc.h xtp.h

default exports: install

include $(COMMONRULES)

headers install: $(_FORCE)
	$(INST) $(IFILES)
