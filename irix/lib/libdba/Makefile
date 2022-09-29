#!smake
#
# Makefile for libdba.
#

BASELIB=libdba.a
ALTLIBS=libdba_64_M4.a libdba_n32_M4.a libdba_64_M3.a libdba_n32_M3.a

DSOEXPORTS=common.exports
DSOEXPORTS_32_ABI=common.exports
DSOEXPORTS_64_ABI=common.exports

LDIRT=so_locations

include $(ROOT)/usr/include/make/librootdefs

# don't turn on -no_unresolved for these libraries
#libc
LD_NO_UNRESOLVED=
#libplace:
#LLDLIBS= -lc

INSOPT=-m 444 -idb "nostrip"
INSOPT4=-idb "mach(CPUARCH=R4000)"
INSOPT4_64=-idb "mach(CPUARCH=R4000 MODE=64bit)"
INSOPT8=-idb "mach(CPUARCH=R10000)"
INSOPT8_64=-idb "mach(CPUARCH=R10000 MODE=64bit)"

DSONAMEOPT=-soname libdba.so

#libdplace
#LLDOPTS= -init __init_dba

default: $(TARGETS)

include $(LIBROOTRULES)

exports install! default
	$(INST_32_M2)	$(INSOPT) -idb 'exitop("if [ -c $$rbase/dev/kaio ] ; then rm -f $$rbase/dev/kaio; fi; if [ ! -l $$rbase/dev/kaio ] ; then ln -s /hw/kaio $$rbase/dev/kaio; fi")' -src libdba.so	libdba.so 
	$(INST_N32_M3)	$(INSOPT) -src libdba_n32_M3.so	libdba.so
	$(INST_64_M3)	$(INSOPT) -src libdba_64_M3.so	libdba.so
	$(INST_N32_M4)	$(INSOPT) -src libdba_n32_M4.so	libdba.so
	$(INST_64_M4)	$(INSOPT) -src libdba_64_M4.so	libdba.so
	$(I_N32_M3) (cd libdba_n32_M3; $(INST_N32) $(INSOPT4) -lns mips3/libdba.so libdba.so)
	$(I_64_M3) (cd libdba_64_M3; $(INST_64) $(INSOPT4) -lns mips3/libdba.so libdba.so)
	$(I_N32_M4) (cd libdba_n32_M4; $(INST_N32) $(INSOPT8) -lns mips4/libdba.so libdba.so)
	$(I_64_M4) (cd libdba_64_M4; $(INST_64) $(INSOPT8_64) -lns mips4/libdba.so libdba.so)
	


