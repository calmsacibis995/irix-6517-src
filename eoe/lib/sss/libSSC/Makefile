#!smake
#
# Makefile for console
#

BASELIB = libSSC.a
ALTLIBS = libSSC_n32_M3.a

include $(ROOT)/usr/include/make/librootdefs

TARGETS=libSSC_n32_M3.so
DSONAMEOPT=-soname libSSC.so
LLDLIBS= 
LD_NO_UNRESOLVED=-no_unresolved
LD_QUICKSTART_INFO=
LDIRT=*.a

default: $(TARGETS)

include $(LIBROOTRULES)

install exports : default
	$(INSTALL) -idb stripdso -F /usr/lib32/internal -src libSSC_n32_M3.so libSSC.so

