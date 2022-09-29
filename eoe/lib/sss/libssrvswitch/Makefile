#!smake
#
# Makefile for eventmon API's
#

BASELIB = libssrvswitch.a
ALTLIBS = libssrvswitch_n32_M3.a
DSOEXPORTS=switcher.export

include $(ROOT)/usr/include/make/librootdefs

TARGETS=libssrvswitch_n32_M3.so
DSONAMEOPT=-soname libssrvswitch.so
LLDLIBS= -lSSC 
LD_NO_UNRESOLVED=
LD_QUICKSTART_INFO=
LDIRT=*.a

default: $(TARGETS)

include $(LIBROOTRULES)

install exports : default
	$(INSTALL) -idb stripdso -F /usr/lib32/internal -src libssrvswitch_n32_M3.so libssrvswitch.so

