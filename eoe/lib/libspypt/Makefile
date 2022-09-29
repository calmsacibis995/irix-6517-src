#!smake
OBJECT_STYLE	= 32_M2
BASELIB		= libspypt_32.a
ALTLIBS		= libspypt_n32_M3.a libspypt_64.a

include $(ROOT)/usr/include/make/librootdefs

DSONAMEOPT	= -soname libspypt.so
LLDLIBS		= -lc
HDRINST		= $(INSTALL) -F $(INSTINC) -m 444 -idb "noship"

default: $(TARGETS)

include $(LIBROOTRULES)

install!headers exports

exports!default libo32 libn32 libn64

headers!
	$(HDRINST) -src abi/ptABI.h	pt/ptABI.h
	$(HDRINST) -src abi/ptABID.h	pt/ptABID.h
	$(HDRINST) -src inc/spyThread.h	pt/spyThread.h
	$(HDRINST) -src inc/spyBase.h	pt/spyBase.h
	$(HDRINST) -src inc/spyCmd.h	pt/spyCmd.h

libo32!	libspypt_32.so
	$(INST_32) -m 444 -src libspypt_32.so internal/libspypt.so

libn32!	libspypt_n32_M3.so
	$(INST_N32) -m 444 -src libspypt_n32_M3.so internal/libspypt.so

libn64!	libspypt_64.so
	$(INST_64) -m 444 -src libspypt_64.so internal/libspypt.so
