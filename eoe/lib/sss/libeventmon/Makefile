#!smake
#
# Makefile for eventmon API's
#

BASELIB = libeventmon.a
ALTLIBS = libeventmon_n32_M3.a

include $(ROOT)/usr/include/make/librootdefs

TARGETS=libeventmon_n32_M3.so
DSONAMEOPT=-soname libeventmon.so
LLDLIBS= 
LD_NO_UNRESOLVED=
LD_QUICKSTART_INFO=
LDIRT=*.a

default: $(TARGETS)

include $(LIBROOTRULES)

install exports : default
	$(INSTALL) -idb stripdso -F /usr/lib32 -src libeventmon_n32_M3.so libeventmon.so

