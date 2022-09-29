DIAGDEPTH= ../..
LLDIRT= dsp_diag.lst dsp_diag.lod dsp_diag.lnk\
	audiotst.lst audiotst.lod audiotst.lnk
include ${DIAGDEPTH}/Makedefs

CFILES= dsp_sram.c dsp_diag.c audiosa.c

include ${DIAGDEPTH}/Makerules

dsp_diag.o:	dsp_diag.c dsp_diag.h
	$(CCF) $< -c -o $(TARGETDIR)/$@

audiosa.o:	audiosa.c audiotst.h
	$(CCF) $< -c -o $(TARGETDIR)/$@

# headers checked in already.
#dsp_diag.h:	dsp_diag.asm
#	$(TOOLROOT)/usr/sbin/asm56000 -B -Ldsp_diag.lst dsp_diag.asm
#	$(TOOLROOT)/usr/sbin/lnk56000 -Bdsp_diag.lod dsp_diag.lnk
#	nawk -f M56001.nawk dsp_diag.lod > dsp_diag.h

#audiotst.h:	audiotst.asm
#	$(TOOLROOT)/usr/sbin/asm56000 -B -Laudiotst.lst audiotst.asm
#	$(TOOLROOT)/usr/sbin/lnk56000 -Baudiotst.lod audiotst.lnk
#	nawk -f M56001.nawk audiotst.lod > audiotst.h
