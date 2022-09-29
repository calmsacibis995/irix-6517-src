#$Revision: 1.41 $
PRODUCT=4DACE1
DEPTH= ..
LLIBDEPTH=
include ${DEPTH}/commondefs
include $(PRODUCTDEFS)
include $(ENDIANDEFS)

# LLDFLAGS=-e startsc -N -T a8080000 -G 0
LLDFLAGS=-e start -N -T 88100000 -D a8200000 -G 0
LLCFLAGS=-I$(ROOT)/usr/src/uts/mips/sys

LDV= ${LD} ${LLDFLAGS} ${VLDFLAGS} ${ENDIAN}

TARGETS=  \
	hal2_16ramp \
	hal2_1ktone \
	hal2_1ktone_aes \
	hal2_1value \
	hal2_65kramp \
	hal2_acvalue \
	hal2_aesbstx \
	hal2_aesnonaudio \
	hal2_aesnonaudio2 \
	hal2_aesout \
	hal2_aesout_endian \
	hal2_aesout_mono \
	hal2_aesout_quad \
	hal2_aesrxctrl \
	hal2_aestimestamp \
	hal2_aestx \
	hal2_aestxctrl \
	hal2_alive \
	hal2_asyncloop \
	hal2_atten1ktone \
	hal2_bptest \
	hal2_bres1_ctrl1 \
	hal2_bres1_ctrl2 \
	hal2_bres2_ctrl1 \
	hal2_bres2_ctrl2 \
	hal2_bres3_ctrl1 \
	hal2_bres3_ctrl2 \
	hal2_cdcactrl1 \
	hal2_cdcactrl2 \
	hal2_cdcaeasy \
	hal2_cdcbctrl1 \
	hal2_cdcbctrl2 \
	hal2_circuitprove \
	hal2_cyclebres \
	hal2_dmabroken \
	hal2_dmadrive \
	hal2_dmaenable \
	hal2_dmaendian \
	hal2_dual_1ktone \
	hal2_endianloop \
	hal2_endiantimestamp \
	hal2_idrwalk \
	hal2_initaes \
	hal2_isrbusy \
	hal2_isrcodec \
	hal2_isrcreset \
	hal2_isrgreset \
	hal2_isrutime \
	hal2_line2aes \
	hal2_line2aes1 \
	hal2_loopaes \
	hal2_loopisr \
	hal2_loopmic \
	hal2_loopquad \
	hal2_loopthru \
	hal2_loopwalk \
	hal2_mono1ktone \
	hal2_mono2quad \
	hal2_multidma \
	hal2_nonaudio \
	hal2_nonaudio2 \
	hal2_quad1ktone \
	hal2_quad2stereo \
	hal2_quadin \
	hal2_rev \
	hal2_servo \
	hal2_servo2 \
	hal2_setbres \
	hal2_squad1ktone \
	hal2_start1msclock \
	hal2_synth_ctrl \
	hal2_timestamp \
	hal2_timestamp_quad \
	hal2_utime \
	hal2_utime2 \
	hal2_validbit \
	hal2_validtest \
	a2_mdac1ktone \
	a2_mic2codeca \
	hpc3bptest \
	hal2_volume

CFILES=  \
	$(TARGETS:=.c) \
	hal2_unreset.c \
	init_aes.c \
	pbus_dma.c \
	pbus_pio.c

OFILES= $(CFILES:.c=.o)

default: ${TARGETS}

include ${COMMONRULES}

${TARGETS}: $$@.o pbus_pio.o pbus_dma.o hal2_unreset.o init_aes.o ${DLIBSC} ${DLIBSK}
	${LDV} $@.o pbus_pio.o pbus_dma.o hal2_unreset.o init_aes.o ${DEFCFG} ${LIBSK} ${LIBSC} -o $@

links:
	for f in ${TARGETS} ; do rm -f /usr/local/boot/$$f ; ln -sf `pwd`/$$f /usr/local/boot ; done
