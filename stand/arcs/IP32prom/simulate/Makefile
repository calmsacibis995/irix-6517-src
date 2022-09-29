#
# Makefile for Moosehead power-on self-test code
#
DEPTH= ../..
include $(DEPTH)/commondefs
include $(PRODUCTDEFS)
include $(ENDIANDEFS)
include $(CPUARCHDEFS)

LDIRT= *.map
LCINCS=	-I$(DEPTH)/IP32/include -I$(DEPTH)/include
LLDOPTS= -m 

LDV=	ld $(CPUARCH_MI_LDOPTS) $(VLDFLAGS) $(LLDOPTS) $(ENDIAN)

LIBMSK=	$(DEPTH)/IP32/lib/libmsk/libmsk.a
LIBS=	$(LIBMSK) $(LIBSK) $(LIBSC) $(LIBMSK)

FORCE_MIPS2= ../tools/force_mips2

COMMONPREF= sim

TARGETS= uprom fw.hex SIM_MACE


FWOBJS=\
	../fw/*.o
SLOBJS=\
	../sloader/*.o
POSTOBJS=\
	../post/*.o

CFILES=\
	IRIX.c\
	SIM.c\
	SIM_k0_k1.c\
	SIM_flash.c\
	SIM_mac110.c\
	SIM_mace_serial.c\
	SIM_rtc.c

EXTOBJS= $(FWOBJS) $(SLOBJS) $(POSTOBJS)
ALLOBJS= $(EXTOBJS) $(OBJECTS)


default $(COMMONTARGS): $(COMMONPREF)$$@

simdefault: $(TARGETS)

uprom:	$(ALLOBJS) $(LIBS)
	$(CCF) $(OBJECTS) $(EXTOBJS) $(LIBS) -o $@ -m > $@.map
	$(FORCE_MIPS2) $@
	size -x $@

fw.hex: ../fw/fw
	echo "SLD" > fw.hex
	cat ../fw/fw.hex >> fw.hex

SIM_MACE: SIM_MACE.c
	$(CC) -g -xansi SIM_MACE.c -o $@
include $(DEPTH)/commonrules








