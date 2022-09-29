############################################################################
#
# Makefile for IP27 PROM
#
############################################################################

DEPTH= ..
LLIBDEPTH= ../
TARGDEPTH= ..
include ${DEPTH}/commondefs
include $(PRODUCTDEFS)
include $(CPUARCHDEFS)

DEF_CSTYLE=

GNUMVAL=8
LLWOFF=,1174					# Arg not used remark
DEBUG=-DDEBUG=1
MIPSVERS=-mips4 $(USE_RAGNAROK)

# The flag -OPT:Olimit=4000 is required for parse_yyparse to be optimized.
# However, the optimized version is LARGER than unoptimized, so we'll let
# it go unoptimized.

$(RAGNAROK)RAGFLAGS=
PROM_CFLAGS=-U__INLINE_INTRINSICS -DIOC3_UART $(YESRAGFLAGS)

PERL=/usr/bin/perl

# Revision code for version string
NEWVERS=./ip27vers.sh

SN0_X=\#
SN0XXL_X=\#
SN0PROM_X=\#
SN0XXLPROM_X=\#
$(PRODUCT)_X=

# RELEASE_VER and RELEASE_REV are used to ensure all PROM revisions
# in a system are compatible.  RELEASE_VER should only be incremented
# if a change is introduced that breaks compatibility with the old
# version, other PROMs, or the kernel.

include ../include/release.mk

$(SN0_X)RELEASE_VER=$(RELEASE_VER_SN0)
$(SN0_X)RELEASE_REV=$(RELEASE_REV_SN0)
$(SN0_X)RELEASE_INFO=$(RELEASE_INFO_SN0)

$(SN0PROM_X)RELEASE_VER=$(RELEASE_VER_SN0)
$(SN0PROM_X)RELEASE_REV=$(RELEASE_REV_SN0)
$(SN0PROM_X)RELEASE_INFO=$(RELEASE_INFO_SN0)

$(SN0XXL_X)RELEASE_VER=$(RELEASE_VER_SN0XXL)
$(SN0XXL_X)RELEASE_REV=$(RELEASE_REV_SN0XXL)
$(SN0XXL_X)RELEASE_INFO=$(RELEASE_INFO_SN0XXL)

$(SN0XXLPROM_X)RELEASE_VER=$(RELEASE_VER_SN0XXL)
$(SN0XXLPROM_X)RELEASE_REV=$(RELEASE_REV_SN0XXL)
$(SN0XXLPROM_X)RELEASE_INFO=$(RELEASE_INFO_SN0XXL)

VCOPTS=

#
# Make sure all addresses and sizes are up to date with ip27prom.h.
#

#PROMADDR = -T ffffffffbfc00000
PROMADDR = -T c00000001fc00000
RTLPROMADDR = -T a800000000002000
GATEPROMADDR = -T a800000000002000

#
# Command flag definitions
#

CSTYLE=
LLDOPTS= -rom -non_shared -elf -G $(GNUMVAL) -64 -mips4 $(USE_RAGNAROK) \
	$(IPA_LDOPTS)
LLCDEFS = -D$(CPUBOARD) -DPROM ${SIMFLAGS} ${WORKAROUNDS} \
	-DIP27_CONFIG_${IP27_CONFIG} ${PROM_CFLAGS} $(IPA_COPTS)
ASFLAGS= $(OPTIMIZER) $(CPUARCH_MI_ASOPTS) -DLOCORE ${CDEFS} ${CINCS} \
	${SIMFLAGS} ${WORKAROUNDS} ${DEBUG} -DIP27_CONFIG_${IP27_CONFIG} \
	-MDupdate Makedepend.${PRODUCT}
ASFLAGS_MIPS4= $(OPTIMIZER) $(CPUARCH_MI_ASOPTS4) -DLOCORE ${CDEFS} \
	${CINCS} ${SIMFLAGS} ${WORKAROUNDS} -DDEBUG=1 \
	-DIP27_CONFIG_${IP27_CONFIG} -MDupdate Makedepend.${PRODUCT}

LDV= ${LD} ${VLDFLAGS} ${LLDOPTS} ${ENDIAN}
VCFLAGS=${DEBUG}

TARGETDIR= ${PRODUCT}.O
VPATH= ${TARGETDIR}

#
# The IP27 does not contain an EAROM for the R10000 mode bits.
# Instead, a config-specific start.o file is linked in at offset 0x60
# (in the exception vector area).
#


IP27_CONFIG_LIST = SN0_4MB_97_5MHZ SN0_1MB_90MHZ SN00_1MB_90MHZ SN00_2MB_225MHZ SN00_4MB_270MHZ

ASFILES = entry.s cache.s libasm.s launch.s \
	  podasm.s mdir_asm.s mtest_asm.s tlb.s

CFILES	= main.c pod.c diagval.c \
	  testing.c symbol.c disMips.c bist.c \
	  mdir.c mtest.c memory.c \
	  net.c router.c discover.c partition.c \
	  nasid.c route.c deadlock.c router_war.c \
	  hubuart.c ioc3uart.c libc.c \
	  segldr.c unzip.c \
	  parse_y.tab.c lex.c cmd.c exec.c prom_error.c kdebug.c \
	  rtr_diags.c hub_diags.c ${PRODUCT}io6prom.img.c 

LDIRT= ${OBJECTS} start.o pvers.o disMips.c

STDTARGETS= prom
COMMONPREF= ip27prom

# Allow developers to have a different default target or whatnot
sinclude localdefault

TOOLS	= ${DEPTH}/tools
MYACC	= ${TOOLS}/myacc/myacc
YACCPAR = ${TOOLS}/myacc/yaccpar
SETSYM	= ${TOOLROOT}/usr/sbin/setsym

############################################################################
#  Default Rule:
#

default: directory ${STDTARGETS}

include ${DEPTH}/commonrules
include Makerules

prom: ${PRODUCT}io6prom
	$(MAKE) ${PRODUCT}ip27prom.post IP27_CONFIG=SN0_1MB_90MHZ
	-@rm -f ${TARGETDIR}/${PRODUCT}ip27prom_*

prom.nolink: ${_FORCE}
	$(MAKE) ip27prom.nolink IP27_CONFIG=SN0_4MB_97_5MHZ

dis: ${_FORCE}
	for C in ${IP27_CONFIG_LIST}; do \
	    $(MAKE) ip27prom_$$C.dis IP27_CONFIG=$$C; \
	done

hex: ${_FORCE}
	for C in ${IP27_CONFIG_LIST}; do \
	    $(MAKE) ${PRODUCT}ip27prom_$$C.post IP27_CONFIG=$$C; \
	done
	-@rm -f ${TARGETDIR}/${PRODUCT}ip27prom ${TARGETDIR}/${PRODUCT}ip27prom.*

bist_compile: bist_compile.c
	$(HOST_CC) -n32 -O -s -o bist_compile bist_compile.c

bist.o: bist_data.c

bist_data.c: bist_compile bist_data.src
	./bist_compile < bist_data.src > bist_data.c

disMips.c:
	ln -sf ../symmon/disMips.c .

# Yacc leaves output files even if error, but also leaves a tell-tale tmp file

parse_y.tab.c parse_y.tab.h: parse.y
	-${MYACC} -v -p${YACCPAR} parse.y
	@${PERL} check_yacc

lex.o: parse_y.tab.h

start.o: start.s ${OBJECTS}
	@if test ! -d ${TARGETDIR}; then mkdir ${TARGETDIR}; fi
	$(ASF) start.s -c \
		-DPVERS_VERS=${RELEASE_VER} -DPVERS_REV=${RELEASE_REV} \
		-o $(TARGETDIR)/$@ 

start_${IP27_CONFIG}.o: start.s ${OBJECTS}
	@if test ! -d ${TARGETDIR}; then mkdir ${TARGETDIR}; fi
	$(ASF) start.s -c \
		-DPVERS_VERS=${RELEASE_VER} -DPVERS_REV=${RELEASE_REV} \
		-o $(TARGETDIR)/$@

${PRODUCT}ip27prom: start.o ${OBJECTS} ${LIBKL} pvers.o
	cd ${TARGETDIR}; \
	       ${LDV} ${PROMADDR} start.o \
		      ${OBJECTS} pvers.o $(LIBKL) -e start -o $@

ip27prom.nolink: start.o ${OBJECTS}

${PRODUCT}ip27prom.post: ${PRODUCT}ip27prom
	cd ${TARGETDIR}; \
		#${SETSYM} ${PRODUCT}ip27prom			# Before zero_csum
	${PERL} zero_csum ${TARGETDIR}/${PRODUCT}ip27prom
	${SIZE} -xA ${TARGETDIR}/${PRODUCT}ip27prom
	cd ${TARGETDIR}; \
		${PROMGEN} -o ${PRODUCT}ip27prom.img -v "${RELEASE_VER}.${RELEASE_REV}" -V -N ip27prom -U -E ${PRODUCT}ip27prom
	@${PERL} check_size ${TARGETDIR}/${PRODUCT}ip27prom
	touch ${TARGETDIR}/${PRODUCT}ip27prom.post

${PRODUCT}ip27prom.dis: ${PRODUCT}ip27prom
	cd ${TARGETDIR}; \
		${TOOLROOT}/usr/bin/dis ${PRODUCT}ip27prom > ${PRODUCT}ip27prom.dis

${PRODUCT}ip27prom_${IP27_CONFIG}: start_${IP27_CONFIG}.o ${OBJECTS} ${LIBKL} pvers.o
	cd ${TARGETDIR}; \
	       ${LDV} ${PROMADDR} start_${IP27_CONFIG}.o \
		      ${OBJECTS} pvers.o $(LIBKL) -e start -o $@

${PRODUCT}ip27prom_${IP27_CONFIG}.post: ${PRODUCT}ip27prom_${IP27_CONFIG}
	cd ${TARGETDIR}; \
		#${SETSYM} ${PRODUCT}ip27prom_${IP27_CONFIG}	# Before zero_csum
	${PERL} zero_csum ${TARGETDIR}/${PRODUCT}ip27prom_${IP27_CONFIG}
	${SIZE} -xA ${TARGETDIR}/${PRODUCT}ip27prom_${IP27_CONFIG}
	cd ${TARGETDIR}; \
		${CONVERT} -f intelhex ${PRODUCT}ip27prom_${IP27_CONFIG} \
		> ${PRODUCT}ip27prom_${IP27_CONFIG}.hex
	cd ${TARGETDIR}; \
		${PROMGEN} -o ${PRODUCT}ip27prom_${IP27_CONFIG}.img \
		-V -N ip27prom -U -E ${PRODUCT}ip27prom_${IP27_CONFIG}
	@${PERL} check_size ${TARGETDIR}/${PRODUCT}ip27prom_${IP27_CONFIG}
	touch ${TARGETDIR}/${PRODUCT}ip27prom_${IP27_CONFIG}.post

ip27prom_${IP27_CONFIG}.dis: ip27prom_${IP27_CONFIG}
	cd ${TARGETDIR}; \
		${TOOLROOT}/usr/bin/dis ${PRODUCT}ip27prom_${IP27_CONFIG} > $@

#
# PROM debug version
#

debug:
	$(MAKE) debugprom TARGETDIR=${PRODUCT}-G.O OPTIMIZER=-g

debugprom: start_${IP27_CONFIG}.o ${OBJECTS} pvers.o
	cd ${TARGETDIR}; ${LDV} ${PROMADDR} start_${IP27_CONFIG}.o \
		${OBJECTS} pvers.o $(LIBKL) -e start -o ../$@
	${PERL} zero_csum $@
	@echo Created sablegprom
	${SIZE} -xA $@ ; ${NM} -Bnx $@ | sort > $@.nm

#
# RTL
#

rtl:
	$(MAKE) rtlprom TARGETDIR=${PRODUCT}-RTL.O \
		SIMFLAGS=-DRTL IP27_CONFIG=1MB_100MHZ

rtlprom: start_${IP27_CONFIG}.o ${OBJECTS} pvers.o
	cd ${TARGETDIR}; ${LDV} ${RTLPROMADDR} start_${IP27_CONFIG}.o \
		${OBJECTS} pvers.o $(LIBKL) -e start -o ../$@
	${PERL} zero_csum $@
	@echo Created rtlprom
	${SIZE} -xA $@ ; ${NM} -Bnx $@ | sort > $@.nm

#
# Gate-level
#

gate:
	$(MAKE) gateprom TARGETDIR=${PRODUCT}-GATE.O \
		SIMFLAGS=-DGATE IP27_CONFIG=1MB_100MHZ

gateprom: start_${IP27_CONFIG}.o ${OBJECTS} pvers.o
	cd ${TARGETDIR}; ${LDV} ${GATEPROMADDR} start_${IP27_CONFIG}.o \
		${OBJECTS} pvers.o $(LIBKL) -e start -o ../$@
	${PERL} zero_csum $@
	@echo Created gateprom
	${SIZE} -xA $@ ; ${NM} -Bnx $@ | sort > $@.nm

#
# bin2c utility program
#

bin2c: bin2c.c
	$(HOST_CC) -n32 -O -s -o bin2c bin2c.c

#
# IO6 false test prom
#
#   Use "make ${PRODUCT}io6prom.img" to generate the IO6 PROM image
#   Use "make ioprom.hub0" to generate the PROM file for Sable
#   (add "setenv iopromfile ioprom" to the sable.key file).
#

io6:
	$(MAKE) fake_io6prom TARGETDIR=${PRODUCT}-IO6.O

IO6PROM_ADDR = -T a800000001c00000

IO6PROM_OBJECTS = \
	io6entry.o io6prom.o tlb.o

fake_io6prom: ${IO6PROM_OBJECTS}
	cd ${TARGETDIR}; \
		${LDV} ${IO6PROM_ADDR} ${IO6PROM_OBJECTS} -e entry \
		-o ../fake_io6prom

${PRODUCT}io6prom:
	@if [ -f fake_io6prom ]; then \
	  echo "*** Warning: Found fake_io6prom in IP27prom dir; using it"; \
	  ln -sf fake_io6prom ${PRODUCT}io6prom; \
	else \
	  ln -sf ../IO6prom/${PRODUCT}io6prom ${PRODUCT}io6prom; \
	fi

${PRODUCT}io6prom.img: ${PRODUCT}io6prom ${PRODUCT}mdk
	${PROMGEN} -o $@ -v "${RELEASE_VER}.${RELEASE_REV}" -V -N io6prom -E ${PRODUCT}io6prom \
			    -N mdk -E ${PRODUCT}mdk
	${PROMGEN} -h $@

${PRODUCT}io6prom.img.c: ${PRODUCT}io6prom.img bin2c
	./bin2c -s io6prom < ${PRODUCT}io6prom.img > ${PRODUCT}io6prom.img.c

ioprom.hub0: ${PRODUCT}io6prom.img
	cp ${PRODUCT}io6prom.img ${PRODUCT}ioprom.hub0

#
# MDK false test prom
#
#   Use "make mdk.img" to generate the MDK PROM image
#

${PRODUCT}mdk:
	@if [ ! -f ${PRODUCT}mdk ]; then \
		echo "*** No MDK found in current directory" ; \
		echo "*** Making a place-holder MDK" ; \
		make fake_io6prom ; \
		mv fake_io6prom ${PRODUCT}mdk ; \
	fi

#
# Mini LED flasher PROM
#

LEDPROM_ADDR = -T ffffffffbfc00000

LEDPROM_OBJECTS = ledprom.o

ledprom: ${LEDPROM_OBJECTS}
	cd ${TARGETDIR}; \
	${LDV} ${LEDPROM_ADDR} ${LEDPROM_OBJECTS} -e start -o $@
	cd ${TARGETDIR}; \
		${CONVERT} -f intelhex $@ > $@.hex
	cd ${TARGETDIR}; \
		dis $@ > $@.dis

#
# Default rule for use with SAMPLE.s.
# Gives us (and hardware folks) an easy way to test
# code snippets loaded into IP27prom.
#

.s: cache.o csu.o libasm.o
	$(ASF) $<  -c -o $(TARGETDIR)/$@.o
	cd ${TARGETDIR}; \
		${LDV} -T ffffffffbfc00000 csu.o $@.o cache.o libasm.o \
		-e start -o $@;
	cd ${TARGETDIR}; \
		${CONVERT} -f intelhex $@ > $@.hex
	@echo Created $(TARGETDIR)/$@ and $(TARGETDIR)/$@.hex

#
# Misc
#

directory: ${_FORCE}
	@if test ! -d ${TARGETDIR}; then mkdir ${TARGETDIR}; fi

pvers.c: ${OBJECTS} ${_FORCE}
	${NEWVERS} prom ${RELEASE_VER} ${RELEASE_REV} \
		"${RELEASE_INFO}" "${CFLAGS}" > $@

clobber: clean ${COMMONPREF}clobber
	rm -rf ${PRODUCT}.O
	rm -rf ${PRODUCT}_RTL.O rtlprom rtlprom.nm
	rm -rf ${PRODUCT}_GATE.O gateprom gateprom.nm

uunc: prom
	rm -f ${PRODUCT}ip27prom.uunc ${PRODUCT}ip27prom.img.uunc
	uuencode ${TARGETDIR}/${PRODUCT}ip27prom ${PRODUCT}ip27prom \
		> ${PRODUCT}ip27prom.uunc
	uuencode ${TARGETDIR}/${PRODUCT}ip27prom.img ${PRODUCT}ip27prom.img \
		> ${PRODUCT}ip27prom.img.uunc

install: ${_FORCE}
	case ${PRODUCT} { \
	SN0|SN0XXL) rm -rf ${PRODUCT}ip27prom ${PRODUCT}ip27prom.img; \
		uudecode ${PRODUCT}ip27prom.uunc; \
		uudecode ${PRODUCT}ip27prom.img.uunc; \
		$(INSTALL) -idb 'mach(CPUBOARD=${CPUBOARD}${SUBPRODUCT})' \
			   -m 644 -F /usr/cpu/firmware -src ${PRODUCT}ip27prom.img ip27prom.img;; \
	}

clean:
	if test -d ${TARGETDIR}; then \
		cd ${TARGETDIR}; \
		rm -f ${LDIRT}; \
		for C in ${IP27_CONFIG_LIST}; do \
			rm -f $$C.o; \
			rm -f ${PRODUCT}ip27prom_$$C; \
			rm -f ${PRODUCT}ip27prom_$$C.img; \
			rm -f ${PRODUCT}ip27prom_$$C.hex; \
		done; \
	fi;
	rm -f bin2c bist_compile
