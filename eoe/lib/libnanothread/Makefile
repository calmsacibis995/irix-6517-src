#!smake
#
# Makefile for libnanothread
#

BASELIB=libnanothread_64.a
ALTLIBS=libnanothread_n32.a

DSOEXPORTS=common.exports
DSOEXPORTS_64=common.exports

include $(ROOT)/usr/include/make/librootdefs
DSONAMEOPT= -soname libnanothread.so
LLDLIBS=-lc

default: $(TARGETS)

include $(LIBROOTRULES)

install: headers exports
exports: default
	$(INST_64) -m 444 -src libnanothread_64.a libnanothread.a
	$(INST_64) -m 444 -src libnanothread_64.so libnanothread.so
	$(INST_N32) -m 444 -src libnanothread_n32.a libnanothread.a
	$(INST_N32) -m 444 -src libnanothread_n32.so libnanothread.so

headers: src/nanothread/nanothread.template.h $(SETUPDIRS)
	cd src/autoheader; ${MAKE} ugenassym
	src/autoheader/ugenassym < src/nanothread/nanothread.template.h > tmp.h
	mv tmp.h nanothread.h
	cd libnanothread_n32/autoheader; ${MAKE} ugenassym
	libnanothread_n32/autoheader/ugenassym < nanothread.h > tmp.h
	mv tmp.h nanothread.h
	$(INSTALL) -F /usr/include -m 444 -src nanothread.h nanothread.h

