#!smake
#
# Makefile for SSDB API's
#

BASELIB = libsgmtask.a
ALTLIBS = libsgmtask_n32_M3.a

include $(ROOT)/usr/include/make/librootdefs

LIBDIR = /usr/lib32
TARGETS=libsgmtask_n32_M3.so
DSONAMEOPT=-soname libsgmtask.so
LLDLIBS= -L/usr/lib32/internal -lssdb
LCINCS=-I$(ROOT)/usr/include -I$(ROOT)/usr/include/sss -I../../include -I ../../../include
LD_NO_UNRESOLVED=
LD_QUICKSTART_INFO=
LDIRT=*.a

default: $(TARGETS)

include $(LIBROOTRULES)

install exports: default
	$(INSTALL) -idb SSS_EOE -F /usr/lib32/internal -src libsgmtask_n32_M3.so libsgmtask.so
