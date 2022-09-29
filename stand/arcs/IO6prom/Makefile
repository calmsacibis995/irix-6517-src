#
# Makefile for the SN0 and SN0XXL IO SYSTEM PROM
#
# This Makefile builds the code which resides in the 
# flash EPROM of the SN0 BASE IO Board. Some TBD portions of this
# code will have to be shared with the SPEEDRACER product. The code 
# implicitly assumes that it is being 
# called by the IP27 PROM and relies on memory
# and certain data structure provided by these PROMS. 
# In addition it assumes that shared memory and all the distributed
# PIO stuff is in place.
# TBD - Arcs related stuff will remain as it is.
# TBD - The user interface is the same as older proms but for
#       the addition of new cmds and refined displays.
#

PRODUCT?=SN0
DEPTH= ..
TARGDEPTH= ../..
LLCINCS=-I../IP27prom

SN0_X=\#
SN0XXL_X=\#
SN0PROM_X=\#
SN0XXLPROM_X=\#
$(PRODUCT)_X=

$(SN0_X)TUNEFILES=io6tune
$(SN0PROM_X)TUNEFILES=io6tune
$(SN0XXL_X)TUNEFILES=io6XXLtune
$(SN0XXLPROM_X)TUNEFILES=io6XXLtune

include ${DEPTH}/commondefs
include $(PRODUCTDEFS)
include $(CPUARCHDEFS)
include $(ENDIANDEFS)

DEF_CSTYLE=

# Revsion code for version string
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

NEWVERS=./io6vers.sh

LDV= ${LD} ${CPUARCH_MI_LDOPTS} -non_shared -N ${VLDFLAGS} ${LLDOPTS} ${ENDIAN}

# -m generates long load map on stderr
#LLDOPTS=-m

# Prom Load Addresses
#	RPROMADDRS specifies the text and bss segment bases for the
#	  real prom.
#	DPROMADDRS specifies the text and bss segment bases for the
#	  debugging version of the PROM.  We want to load debugging 
#	  versions of the PROM, so we have to keep it out of the
#	  way of the primary PROM
#	SPROMADDRS specifies the text and bss segment bases for the
#	  sable version of the prom. Sable only supports 8 mb of main
#	  memory, so we have to load the prom at a lower address.
#
RPROMADDRS=C000000011C00000
DPROMADDRS=A800000000800000
SPROMADDRS=A800000001E00000
UPROMADDRS=A800000001E00000

# Bring back the Prom loading address to be unmapped address. 
# There is some problem in PROM drivers to work in mapped mode.
# RPROMADDRS=A800000001E00000

PROMADDRS   = ${RPROMADDRS}

#
# Command flag definitions
#
LLCOPTS =
LLCDEFS = -D$(CPUBOARD) ${SIMFLAGS} -DPROM -DNO_tpsc

TARGETDIR= ${PRODUCT}.O
VPATH= ${TARGETDIR}

#
# In the ASFILES list, sysasm.s must come first for proper load-order.
#
ASFILES= sysasm.s tlb.s
CFFILES= io6conf.cf fsconf.cf $(TUNEFILES).cf
CFILES= main.c io6conf.c sysinit.c fsconf.c flashprom.c inventory.c \
        io6prommisc_cmd.c sysctlr.c directory.c fake_prom.c traverse_nodes.c \
	flaship27.c flashio6.c $(TUNEFILES).c diag_cmds.c cmds.c salink.c \
	ed.c ed1.c checkclk.c

LLIBDEPTH=../
LIBPSA=  $(LIBSK) $(LIBKL) $(LIBSK) $(LIBSC) 
LLIBS= ${DEPTH}/$(PRODUCT).O/*.ln

LDIRT= ${OBJECTS} ${DOBJECTS} vers.[co] ${PRODUCT}pvers.[co] *.map *.nm *.ln \
	$(ALLCFDIRT)

STDTARGETS= prom prom.img
OPTTARGETS= dprom
TARGETS= ${STDTARGETS} ${OPTTARGETS}

# allow developers to have a different default target
# usually will be used to make dprom or lowprom the defaut
sinclude localdefault

SETSYM  = ${TOOLROOT}/usr/sbin/setsym

default: directory ${STDTARGETS} 
	@echo ==========================================================
	@echo NOTE: PRODUCT is currently set to ${PRODUCT}.
	@echo SN0 or SN0XXL product does *not* include graphics - this
	@echo is what is linked into the CPU prom.  Only SN0PROM and
	@echo SN0XXLPROM product has full graphics support.
	@echo When checking in an IO6prom image, make sure the product
	@echo is set to SN0PROM or SN0XXLPROM.

COMMONPREF= IO6prom
include ${DEPTH}/commonrules
include Makerules

prom: ${PRODUCT}io6prom

prom.nolink: ${OBJECTS}

${PRODUCT}io6prom ${PRODUCT}io6dprom ${PRODUCT}io6sprom ${PRODUCT}io6uprom: ${OBJECTS} \
		${PRODUCT}pvers.o ${LIBPSA}
	cd ${TARGETDIR}; ${LDV} -T ${PROMADDRS} ${OBJECTS} ${PRODUCT}pvers.o \
		 ${LIBPSA} -e entry -o ../$@
	${SIZE} -Ax $@; ${NM} -Bnx -n $@ | sort > $@.nm

dprom:
	make PROMADDRS=${DPROMADDRS} ${PRODUCT}io6dprom

sprom:
	make TARGETDIR=${PRODUCT}.O SIMFLAGS=-DSABLE \
		PROMADDRS=${SPROMADDRS} ${PRODUCT}io6sprom

uprom:
	make TARGETDIR=${PRODUCT}.O SIMFLAGS=-DSABLE \
		PROMADDRS=${UPROMADDRS} ${PRODUCT}io6uprom

# Create an image file .img, which can be used by the 
# flashio6 command to write it to the PROM on the IO6.
# This image file contains headers and code in a
# compressed format. For now, it just contains one section
# The promgen command needs to be used for multiple sections.

prom.img: ${PRODUCT}io6prom.img

${PRODUCT}io6prom.img: ${PRODUCT}io6prom
	${PROMGEN} -o $@ -v "${RELEASE_VER}.${RELEASE_REV}" -V -N io6prom \
		-E ${PRODUCT}io6prom
	${PROMGEN} -h $@

# For uncompressed images use the following in place of the
# above command line 
#       ${PROMGEN} -o $@ -V -U -N io6prom -E ${PRODUCT}io6prom

$(LIBPSA):
	@echo "";echo "ERROR $@ are required; build them first"; echo""; exit 1

directory: ${_FORCE}
	@if test ! -d ${TARGETDIR}; then mkdir ${TARGETDIR}; fi

${PRODUCT}pvers.c: ${OBJECTS} ${LIBIDE} ${LIBPSA} ${_FORCE}
	${NEWVERS} prom ${RELEASE_VER} ${RELEASE_REV} \
		"${RELEASE_INFO}" "${CFLAGS}" > $@

uunc: ${PRODUCT}io6prom.img
	@if [ ${PRODUCT} != "SN0PROM" ]; then \
	    if [ ${PRODUCT} != "SN0XXLPROM" ]; then \
		echo WARNING: Your PRODUCT is set to ${PRODUCT}.; \
		echo Binaries must be checked in with \
		     PRODUCT of SN0PROM or SN0XXLPROM.; \
	    fi; \
	fi
	rm -f ${PRODUCT}io6prom.uunc ${PRODUCT}io6prom.img.uunc
	uuencode ${PRODUCT}io6prom ${PRODUCT}io6prom > ${PRODUCT}io6prom.uunc
	uuencode ${PRODUCT}io6prom.img ${PRODUCT}io6prom.img > ${PRODUCT}io6prom.img.uunc

install: ${_FORCE}
	case ${PRODUCT} { \
	SN0PROM|SN0XXLPROM) rm -rf ${PRODUCT}io6prom ${PRODUCT}io6prom.img; \
		uudecode ${PRODUCT}io6prom.uunc; \
		uudecode ${PRODUCT}io6prom.img.uunc; \
		$(INSTALL) -idb 'mach(CPUBOARD=${CPUBOARD}${SUBPRODUCT})' \
			   -m 644 -F /usr/cpu/firmware -src ${PRODUCT}io6prom.img io6prom.img;; \
	}

lint: ${LOBJECTS}
	$(LINT) $(LINTCFLAGS) ${LOBJECTS} ${LLIBS}
	rm -f ${LOBJECTS}

clobber: clean ${COMMONPREF}clobber
	rm -f ${PRODUCT}io6prom.bin ${PRODUCT}io6dprom.bin ${PRODUCT}io6sprom.bin
	rm -f ${PRODUCT}io6prom.map ${PRODUCT}io6dprom.map ${PRODUCT}io6sprom.map
	rm -f ${PRODUCT}io6prom.nm  ${PRODUCT}io6dprom.nm  ${PRODUCT}io6sprom.nm
	rm -f ${PRODUCT}io6prom     ${PRODUCT}io6dprom     ${PRODUCT}io6sprom
	rm -f ${PRODUCT}io6prom.img
	rm -rf ${PRODUCT}.O      ${PRODUCT}S.O
	rm -f io6prom.bin
	for d in $(EVERYPRODUCT); do rm -rf $$d.O $$dS.O; done

clean:	${COMMONPREF}clean
	@if test -d ${TARGETDIR}; then cd ${TARGETDIR}; rm -f ${LDIRT}; fi

tags:	${COMMONPREF}tags

incdepend:	$(CFFILES:.cf=.c) ${COMMONPREF}incdepend
depend:		$(CFFILES:.cf=.c) ${COMMONPREF}depend

fluff: ${_FORCE}
	${LINT} ${LINTFLAGS} ${CDEFS} ${CINCS} ${CFILES} ${LDLIBS}
