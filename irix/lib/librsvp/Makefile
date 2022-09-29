#!smake
#ident "$Revision: 1.3 $

BASELIB=librsvp_32.a
ALTLIBS=librsvp_64.a librsvp_n32.a

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

include $(ROOT)/usr/include/make/librootdefs

DSONAMEOPT=-soname librsvp.so
LLDLIBS=-lc

PUBLIC_HEADERS=rapi_lib.h rapi_err.h

DSOVERSION=sgi2.0

default: $(TARGETS)

include ${LIBROOTRULES}

install:headers exports

exports:default
	$(INST_32) -m 444 -src librsvp_32.so librsvp.so
	$(INST_32) -m 444 -src compat/librsvp_32.so.1 librsvp.so.1
	$(INST_64) -m 444 -src librsvp_64.so librsvp.so
	$(INST_64) -m 444 -src compat/librsvp_64.so.1 librsvp.so.1
	$(INST_N32) -m 444 -src librsvp_n32.so librsvp.so
	$(INST_N32) -m 444 -src compat/librsvp_n32.so.1 librsvp.so.1

headers:
	(cd inc; make headers)










