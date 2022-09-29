DIAGDEPTH= ../..
LLDIRT= slot_dsp.h slot_dsp.lst slot_dsp.lod slot_dsp.lnk
include ${DIAGDEPTH}/Makedefs

CFILES=  slot_test.c \
		 gio_int_logic.c \
		 pbus_logic.c \
		 pbus_int.c \
		 gio_master.c

HFILES=  slot_dsp.h \
		 slot_hpc_dma.h

include ${DIAGDEPTH}/Makerules

gio_master_logic.o:	gio_master_logic.c slot_hpc_dma.h
	$(CCF) $< -c -o $(TARGETDIR)/$@

pbus_int_logic.o:	pbus_int_logic.c slot_dsp.h
	$(CCF) $< -c -o $(TARGETDIR)/$@

slot_dsp.h:	slot_dsp.asm
	$(TOOLROOT)/usr/sbin/asm56000 -B -Lslot_dsp.lst slot_dsp.asm
	$(TOOLROOT)/usr/sbin/lnk56000 -Bslot_dsp.lod slot_dsp.lnk
	$(TOOLROOT)/usr/bin/nawk -f M56001.nawk slot_dsp.lod > slot_dsp.h
