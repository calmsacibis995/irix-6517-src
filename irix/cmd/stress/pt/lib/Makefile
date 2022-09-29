#!smake

BASEVERSION=32bit
ALTVERSIONS=64bit n32bit

LCINCS		= -I.

include $(ROOT)/usr/include/make/cmdcommondefs

COMMANDS=libTst.a

32bitinstall 64bitinstall n32bitinstall default:$(TARGETS)

include $(CMDCOMMONRULES)

libTst.a: libTst.a(Tio.o) libTst.a(Tmain.o) libTst.a(Tbnch.o) MAKELIB

