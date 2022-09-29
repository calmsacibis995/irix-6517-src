#ident "IP30prom/MfgDBGProm.mk: $Revision: 1.9 $"
#
# Makefile for IP30 Mfg DBGPROM
#
# This makefile normally builds the prom code.
#
DEPTH= ..
TARGDEPTH= ../..
include ${DEPTH}/commondefs
include $(PRODUCTDEFS)
include $(ENDIANDEFS)
include $(CPUARCHDEFS)

DEF_CSTYLE=

# flash header revisioning.
RPROM_VERSION=2.3
FPROM_VERSION=2.5

REVISION=$(FPROM_VERSION)

# differentiate between prom building styles.
RACER_X=\#
RACER_RPROM_X=\#
$(PRODUCT)_X=

RPROM_REAL=rprom
RPROM_FAKE=rprom_fake
$(RACER_X)RPROM_REAL=rprom_fake
$(RACER_X)RPROM_FAKE=rprom

# override global opts to use -rom flag
LLDOPTS=-rom -m -non_shared

CONVERT=$(TARGDEPTH)/tools/convert/convert

# XXX prom BSS, want to use mapped space such that we can switch
# between cached and uncached BSS.  we want BSS to be uncached before
# running the cache diagnostic or if the cache diagnostics fails, want
# it to be cached after passing the cache diagnostics.  also want it to
# be uncached inside the ECC handler
BSSADDR=a800000020f00000

# cached prom.
# there are 3 ways to cache the PROM:
#	1: link the PROM using K1 uncached address and then jump to
#	   K0 cached space after the cache diagnostics passes.
#	   this offers little performance gain since function called
#	   through pointer will still be executing in K1 uncached space.
#	2: link the PROM using K0 cached address and then do a jump
#	   after the cache diagnostics passes to start executing in
#	   cached space, need to do a jump since the PROM executes in
#	   K1 compatibility space after coming out of a reset.  this doesn't
#	   work well if the cache diagnostics fails since there's no easy
#	   way to make function called through pointer to run in
#	   uncached space.
#	3: link the PROM using K0 compatibility cached address.  set
#	   the K0 cache algorithm to uncached early on and then switch
#	   to cacheable noncoherent after the cache diagnostic passes.
#	   this do not have the problems described above.

# Cached
TEXTADDR=ffffffff9fc00000
# Uncached
#TEXTADDR=ffffffffbfc00000

# mini-prom addresses (led, death and diag)
MTEXTADDR=ffffffffbfc00000

# dprom load addresses:
DTEXTADDR=a800000020600000
DBSSADDR=a800000020700000

# rprom load addresses:
RTEXTADDR=ffffffff9fc00000

# fprom load addresses:
# use 1fc30100 when we flip the switch for RPROM going 3 segs
# coordinate with sys/RACER/sflash.h
# this is now defined in RACER_RPROM
#
# FTEXTADDR=ffffffff9fc30100
# FTEXTADDR=ffffffff9fc20100
#

LLCDEFS= -D${CPUBOARD} -DBSSADDR=0x$(BSSADDR) -DDBSSADDR=0x$(DBSSADDR) ${CHIPDEFS} $(LLLCDEFS) -DBUILDER=\"$(LOGNAME)\"
LLCOPTS=-use_readonly_const
LDV= ${LD} ${LLDOPTS} ${CPUARCH_MI_LDOPTS} ${VLDFLAGS} ${ENDIAN}

TARGETDIR= ${PRODUCT}.O
VPATH= ${TARGETDIR}

MFG_CFILES=
MFG_CFILES= poncmd.c duart_regs.c utility.c pci_sio.c enet_dma.c enet_diags.c ide_malloc.c RadCspaceTest.c RadUtils.c RadRamTest.c RadStatusTest.c scsi_regs.c gz_errorcodes.c
#
# In the ASFILES list, csu.s must come first for proper load-order.
#
ASFILES= csu.s lmem_conf.s IP30asm.s
CFFILES= Mfg_conf.cf Mfg_fsconf.cf $(PSCFG)_rprom.cf fsconf_rprom.cf
CFILES= IP30.c main.c $(MFG_CFILES) $(TARGETDIR)/Mfg_conf.c $(TARGETDIR)/Mfg_fsconf.c \
        hello_tune.c prad.c

RASFILES= csu_rprom.s
RCFILES=

LLIBDEPTH=../
LIBPSA= $(LIBSK) $(LIBSC)
LIBPON= ../${DEPTH}/ide/${PRODUCT}p.O/libpide.a
LLIBS= ${DEPTH}/$(PRODUCT).O/*.ln

LDIRT= ${OBJECTS} ${DOBJECTS} ${GOBJECTS} vers.[co] pvers.[co] *.map *.nm *.ln \
	deathprom ledprom fakeprom $(ALLCFDIRT) \
	${FOBJECTS} ${ROBJECTS}

#
# To make rprom/IP30prom do 'make IP30proms' from stand/arcs.
STDTARGETS= prom
OPTTARGETS= dprom dprom.DBG fprom.bin
TARGETS= ${STDTARGETS} ${OPTTARGETS}

# allow developers to have a different default target
# usually will be used to make dprom or lowprom the default
sinclude localdefault

# untile we throw the switch for the 3 seg RPROM
# default is the NULL RPROM using the FTEXTADDR, RASFILES, and
# RCFILES. But if $PRODUCT is defined as RACER_RPROM,
# the RPROM is built by setting the real FTEXTADDR, RASFILES, and
# RCFILES values in IP30RPROMdefs file.
# FPROMDIR is also defined to indicate where to pickup the fprom.bin
#

FPROMDIR=.
sinclude ${PRODUCT}_localdefs

default: directory ${TARGETS} 

COMMONPREF=IP30prom
include ${DEPTH}/commonrules
include Makerules

deathprom ledprom: $$@.o elspec.template
	sed -e 's/TEXTADDR/0x${MTEXTADDR}/' -e 's/BSSADDR/0x${BSSADDR}/' \
		elspec.template > ${TARGETDIR}/elspec.$@
	cd ${TARGETDIR}; ${LDV} -elspec elspec.$@ $@.o -o $@ > $@.map; \
	${SIZE} -Ax $@; ${NM} -x -n $@ > $@.nm

fakeprom: $$@.o elspec.template
	sed -e 's/TEXTADDR/0x${TEXTADDR}/' -e 's/BSSADDR/0x${BSSADDR}/' \
		elspec.template > ${TARGETDIR}/elspec.$@
	cd ${TARGETDIR}; ${LDV} -elspec elspec.$@ $@.o -o $@ > $@.map; \
	${SIZE} -Ax $@; ${NM} -x -n $@ > $@.nm

prom: ${OBJECTS} pvers.o ${LIBPON} ${LIBPSA} elspec.template
	sed -e 's/TEXTADDR/0x${TEXTADDR}/' -e 's/BSSADDR/0x${BSSADDR}/' \
		elspec.template > ${TARGETDIR}/elspec.$@
	cd ${TARGETDIR}; ${LDV} -elspec elspec.$@ ${OBJECTS} pvers.o \
	    ${LIBPON} ${LIBPSA} -o $@ > $@.map; \
	    ${SIZE} -Ax $@

dprom: ${DOBJECTS} pvers.o ${LIBPON} ${LIBPSA} elspec.template
	sed -e 's/TEXTADDR/0x${DTEXTADDR}/' -e 's/BSSADDR/0x${DBSSADDR}/' \
		elspec.template > ${TARGETDIR}/elspec.$@
	cd ${TARGETDIR}; ${LDV} -elspec elspec.$@ ${DOBJECTS} pvers.o \
	    ${LIBPON} ${LIBPSA} -o $@ > $@.map; \
	    ${SIZE} -Ax $@

dprom.DBG: $(LIBDEPTH)/lib/libsc/$(ENDIANND)$(CPUARCH_PFIX).O/idbg.o \
	${GOBJECTS} pvers.o ${LIBPON} ${LIBPSA} elspec.template 
	sed -e 's/TEXTADDR/0x${DTEXTADDR}/' -e 's/BSSADDR/0x${DBSSADDR}/' \
		elspec.template > ${TARGETDIR}/elspec.$@
	cd ${TARGETDIR}; ${LDV} -elspec elspec.$@ \
	    ${GOBJECTS} pvers.o \
	    $(LIBDEPTH)/lib/libsc/$(ENDIANND)$(CPUARCH_PFIX).O/idbg.o \
	    ${LIBPON} ${LIBPSA} -o $@ > $@.map; \
	    ${TOOLROOT}/usr/sbin/setsym $@; \
	    ${SIZE} -Ax $@ 

fprom: ${FOBJECTS} pvers.o ${LIBPON} ${LIBPSA} elspec.template
	sed -e 's/TEXTADDR/0x${FTEXTADDR}/' -e 's/BSSADDR/0x${BSSADDR}/' \
		elspec.template > ${TARGETDIR}/elspec.$@
	cd ${TARGETDIR}; ${LDV} -elspec elspec.$@ ${FOBJECTS} pvers.o \
	    ${LIBPON} ${LIBPSA} -o $@ > $@.map; \
	    ${SIZE} -Ax $@ ;

fprom.bin: fprom
	cd ${TARGETDIR}; \
	    ${CONVERT} -f fprom -v ${FPROM_VERSION} fprom > $@

#
# the loader with IPA put everything in stderr, so the LINK Memory Map goes there
# as well (see BUG 428130). Until this is fixed redirect the stderr to *.map
#
drprom: ${DROBJECTS} ${LIBPON} ${LIBPSA} elspec.template
	${NEWVERS} prom "${RELEASE}" "Rev ${REVISION}" ${CPUBOARD} ${VERSCPU} > $(TARGETDIR)/pvers.c
	$(CCF) $(TARGETDIR)/pvers.c -c -o $(TARGETDIR)/pvers.o
	sed -e 's/TEXTADDR/0x${DTEXTADDR}/' -e 's/BSSADDR/0x${DBSSADDR}/' \
		elspec.template > ${TARGETDIR}/elspec.$@
	cd ${TARGETDIR}; ${LDV} -elspec elspec.$@ ${DROBJECTS} pvers.o \
	    ${LIBPON} ${LIBPSA} -o $@  2> $@.map; tail -20 $@.map;\
	    ${SIZE} -Ax $@

$(RPROM_REAL): ${ROBJECTS} ${LIBPON} ${LIBPSA} elspec.template
	${NEWVERS} prom "${RELEASE}" "Rev ${REVISION}" ${CPUBOARD} ${VERSCPU} > $(TARGETDIR)/pvers.c
	$(CCF) $(TARGETDIR)/pvers.c -c -o $(TARGETDIR)/pvers.o
	sed -e 's/TEXTADDR/0x${RTEXTADDR}/' -e 's/BSSADDR/0x${BSSADDR}/' \
		elspec.template > ${TARGETDIR}/elspec.$@
	cd ${TARGETDIR}; ${LDV} -elspec elspec.$@ ${ROBJECTS} pvers.o \
	    ${LIBPON} ${LIBPSA} -o $@ 2> $@.map; tail -20 $@.map;\
	    ${SIZE} -Ax $@;

$(RPROM_FAKE):

IP30prom.bin: rprom ${FPROMDIR}/fprom.bin
	cd ${TARGETDIR}; \
	    ${CONVERT} -f rprom -v ${RPROM_VERSION} ../RACER_RPROM.O/rprom > $@ ; \
	cat ${FPROMDIR}/fprom.bin >> $@

IP30prom.hex: IP30prom.bin
	cd ${TARGETDIR}; ${CONVERT} -p -f intelhex IP30prom.bin > IP30prom.hex

$(LIBPON):
	@echo "";echo "ERROR $(LIBPON) is required"; echo ""; exit 1;

$(LIBPSA):
	@echo "";echo "ERROR $@ is required"; echo""; exit 1;

directory: ${_FORCE}
	@if test ! -d ${TARGETDIR}; then mkdir ${TARGETDIR}; fi

pvers.o: $(TARGETDIR)/pvers.c
	$(CCF) $(TARGETDIR)/pvers.c -c -o $(TARGETDIR)/$@

$(TARGETDIR)/pvers.c: ${FOBJECTS} ${LIBPON} ${LIBPSA}
	${NEWVERS} prom "${RELEASE}" "Rev ${REVISION}" ${CPUBOARD} ${VERSCPU} > $@

$(TARGETDIR)/Mfg_conf.c: Mfg_conf.cf $(GCONF)
	$(GCONF) Mfg_conf.cf $(TARGETDIR)
	@rm -f ${TARGETDIR}/${TARGETDIR} ; ln -s . ${TARGETDIR}/${TARGETDIR}

$(TARGETDIR)/Mfg_fsconf.c: Mfg_fsconf.cf $(GCONF)
	$(GCONF) Mfg_fsconf.cf $(TARGETDIR)
	@rm -f ${TARGETDIR}/${TARGETDIR} ; ln -s . ${TARGETDIR}/${TARGETDIR}

$(TARGETDIR)/IP30conf_rprom.c: IP30conf_rprom.cf $(GCONF)
	$(GCONF) IP30conf_rprom.cf $(TARGETDIR)
	@rm -f ${TARGETDIR}/${TARGETDIR} ; ln -s . ${TARGETDIR}/${TARGETDIR}

$(TARGETDIR)/fsconf_rprom.c: fsconf_rprom.cf $(GCONF)
	$(GCONF) fsconf_rprom.cf $(TARGETDIR)
	@rm -f ${TARGETDIR}/${TARGETDIR} ; ln -s . ${TARGETDIR}/${TARGETDIR}

lint: ${LOBJECTS}
	$(LINT) $(LINTCFLAGS) ${LOBJECTS} ${LLIBS}
	rm -f ${LOBJECTS}

clobber: clean ${COMMONPREF}clobber
	for d in $(EVERYPRODUCT); do rm -rf $$d.O; done
	for d in $(EXTRAPRODUCTS); do rm -rf $$d.O Makedepend.$$d; done

clean:	${COMMONPREF}clean
	@if test -d ${TARGETDIR}; then cd ${TARGETDIR}; rm -f ${LDIRT}; fi

tags:	${COMMONPREF}tags

incdepend:	$(CFFILES:.cf=.c) ${COMMONPREF}incdepend
depend:		$(CFFILES:.cf=.c) ${COMMONPREF}depend

fluff: ${_FORCE}
	${LINT} ${LINTFLAGS} ${CDEFS} ${CINCS} ${CFILES} ${LDLIBS}
