#
# Makefile for rpc header file installation.
#
include $(ROOT)/usr/include/make/commondefs

PRIVATE_HFILES=	\
	des.h desdata.h softdes.h

HPUT= $(INSTALL) -m 444 -F /usr/include/des

default: install

include $(COMMONRULES)

install:
	$(HPUT) -idb "noship" $(PRIVATE_HFILES)
