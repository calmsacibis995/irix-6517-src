#!smake

BASEVERSION=32bit
ALTVERSIONS=64bit n32bit

include $(ROOT)/usr/include/make/cmdcommondefs

COMMANDS=libstress.a

32bitinstall 64bitinstall n32bitinstall default:$(TARGETS)

include $(CMDCOMMONRULES)

libstress.a: libstress.a(libstress.o) MAKELIB

