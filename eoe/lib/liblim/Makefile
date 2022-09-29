#!smake
#
# Makefile for liblim.
#
#ident	"$Id: Makefile,v 1.2 1997/05/12 23:47:14 rbeck Exp $"

BASELIB=liblim_32.a
ALTLIBS=liblim_64.a

DSOEXPORTS=common.exports
DSOEXPORTS_32=common.exports
DSOEXPORTS_64=common.exports

include	$(ROOT)/usr/include/make/librootdefs
LLDLIBS=-lc
DSONAMEOPT=-soname liblim.so

default: $(TARGETS)

include $(LIBROOTRULES)

exports install: default
	$(INST_32) -m 444 -src liblim_32.a liblim.a
	$(INST_32) -m 444 -src liblim_32.so liblim.so
	$(INST_64) -m 444 -src liblim_64.a liblim.a
	$(INST_64) -m 444 -src liblim_64.so liblim.so
