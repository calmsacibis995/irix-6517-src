# Makefile for sash
#
# This makes the product independent version of sash.
#
# $Revision: 1.107 $

DEPTH= ..
include $(DEPTH)/commondefs
LLIBDEPTH= ../

include $(PRODUCTDEFS)
include $(ENDIANDEFS)
include $(CPUARCHDEFS)
include $(MIDEFS)

# sash is currently clean, turn on errors for certain warnings.
CPUARCH_DEFS=$(CMPLR_WOFF_ERR)

DEF_CSTYLE=

TARGETDIR=$(ENDIANND)$(CPUARCH_PFIX).O
MKDEPFILE=Makedepend.$(ENDIANND)$(CPUARCH_PFIX)
OBJECTDIR=$(TARGETDIR)
VPATH= $(TARGETDIR)
SASH=sash

LDIRT = $(TARGETDIR)/* $(SASH).DBG $(SASH).defined vers.c tune.[ch]
SETSYM =$(TOOLROOT)/usr/sbin/setsym
LLIBS= $(LIBDEPTH)/lib/libsc/$(ENDIANND)$(CPUDEFS_PFIX).O/llib-lsc.ln

CFFILES= tune.cf
CFILES= main.c tune.c modem_init.c
ASFILES=start.s

SASHLDOPTS=$(CPUARCH_MI_LDOPTS)

64_Q=\#
${CPUARCH_PFIX}_Q=

# A hack for the merged redwood/ucode compilers. -N in the 3.18 and 3.19
# compilers forces -coff, which is unfortunately not supported by
# subsequent tools, such as size, or promcvt.
# The workaround implemented was '-sgi_kernel', which creates a -N type
# output file, but in elf.
# -sgi_kernel is only recognized by the 32 bit ld, so have to special
# case the option.
# The reason that this hack is implemented is that subsequent tools, such
# as size and nm, can not deal with coff files.

LD_N_FLAG=-sgi_kernel
${64_Q}LD_N_FLAG=-N -x

# for 32-bit sash, LD_AUX_CMD creates a coff version of sash
# for 64-bit sash, we stay with elf
LD_AUX_CMD=$(LD) -coff -e startsc -r -32 -o $(SASH) $(SASH).elf; rm $(SASH).elf
SASHBINARY=$(SASH)

${64_Q}LD_AUX_CMD=rm -f $(SASH).stripped; cp $(SASH).elf $(SASH); \
	mv $(SASH).elf $(SASH).stripped; \
	$(STRIP) -sf $(SASH).stripped 2>1 > /dev/null || true
${64_Q}SASHBINARY=$(SASH).stripped

MACHTAG=$(ARCS_MACHTAG)
IDE_MACHTAG=$(IDELD_MACHTAG)
SASHNAME1=sash.ARCS
SASHNAME2=05sashARCS
HACKLINK=-T 80c00000 ${LD_N_FLAG}
LIBSLMAYBE=$(LIBSL)
NOSTRIP=nostrip
NOHIST=nohist

${64_Q}MACHTAG=$(ARCS64_MACHTAG)
${64_Q}IDE_MACHTAG=$(IDELD64_MACHTAG)
${64_Q}SASHNAME1=sash64
${64_Q}SASHNAME2=05sash64
${64_Q}HACKLINK=-T a800000000c00000 ${LD_N_FLAG}
${64_Q}LIBSLMAYBE=
${64_Q}NOSTRIP=

IDB_TAG='std.sw.unix $(MACHTAG) postop("if test X$$instmode = Xnormal ; then /sbin/dvhtool -v creat $$rbase/stand/sash sash $$vhdev && rm -f $$rbase/stand/sash; fi") $(NOSTRIP)'

# Always install sash as ide, even on 64-bit systems.  This lets 'ide fe'
# to work from the command monitor w/o booting sash directly.  This allows us
# to move ide off of the volume header (has always been in /stand) and still
# let the menu on old proms (IP20s and early IP22s) to boot ide via the menu.
#
# We are seperate from the normal install now as some systems (IP27 now) do
# not support ide.
#
IDE_IDB_TAG='std.sw.unix $(IDE_MACHTAG) postop("if test X$$instmode = Xnormal ; then /sbin/dvhtool -v creat $$rbase/stand/ideloader ide $$vhdev && rm -f $$rbase/stand/ideloader; fi") $(NOSTRIP)'

DL_IDB_TAG='DSKLESS_CLT $(MACHTAG) $(NOSTRIP)'

ROBO_IDB_TAG='ROBO_CLT $(NOHIST) $(NOSTRIP)'

IDBG_OBJ= ${DEPTH}/lib/libsc/$(TARGETDIR)/idbg.o

TARGETS= $(SASH)
DTARGETS= $(SASH).defined

default: ${TARGETS}

COMMONPREF= sash

include $(DEPTH)/commonrules

$(TARGETS): $(TARGETDIR) $(DEPLIB) $(TARGETDIR)/vers.o $(DTARGETS)
	cd $(TARGETDIR) ; \
	$(LD) $(SASHLDOPTS) -r -e __start -d -o $@.elf \
		$(OBJECTS) vers.o $(LIBSLMAYBE) $(GLDLIBS); \
	$(LD_AUX_CMD)


install: $(TARGETS)
	@if [ "$(TARGETDIR)" = "EL.O" ];				\
	then							\
		echo "Little Endian sash should not be installed.";	\
		exit 1;						\
	fi
	cd $(TARGETDIR) ; \
	${INSTALL} -idb $(IDB_TAG) -F /stand -src $(SASHBINARY) sash; \
	${INSTALL} -idb $(IDE_IDB_TAG) -F /stand -src $(SASHBINARY) ideloader;\
	${INSTALL} -idb ${DL_IDB_TAG} -F /sbin/stand -src $(SASHBINARY) ${SASHNAME1} ; \
	${INSTALL} -idb ${DL_IDB_TAG} -F /stand -lns ../sbin/stand/${SASHNAME1} sash ; \
	${INSTALL} -idb "sa ${NOSTRIP}" -F / -src $(SASHBINARY) ${SASHNAME2} ; \
	${INSTALL} -idb $(ROBO_IDB_TAG) -F /stand/roboinst -src $(SASHBINARY) ${SASHNAME1}

$(TARGETDIR)/vers.o: $(OBJECTS)
	${NEWVERS} sccs ${RELEASE} "ARCS ${VERSION}" > $(TARGETDIR)/vers.c ; \
	$(CCF) -c $(TARGETDIR)/vers.c -o $@

$(DTARGETS): $(TARGETDIR) $(DEPLIB) $(TARGETDIR)/vers.o
	cd $(TARGETDIR) ; \
	$(LD) $(SASHLDOPTS) -non_shared ${LD_N_FLAG} -o $@ $(OBJECTS) vers.o $(LIBSLMAYBE) $(GLDLIBS)

$(SASH).DBG: $(TARGETDIR) $(DEPLIB) $(TARGETDIR)/vers.o $(DTARGETS)
	cd $(TARGETDIR) ; \
	$(LD) $(SASHLDOPTS) -r -e __start -d -o $@ $(OBJECTS) vers.o ../$(IDBG_OBJ) $(LIBSLMAYBE) $(GLDLIBS)
	$(SETSYM) $(TARGETDIR)/$@

Static="cvstatic.fileset"
static: ${ASFILES} ${CFILES}
	@echo "Making cvstatic data file"
	@echo ${ASFILES} ${CFILES} > ${Static}
	@find '../lib' -name '*.[chs]' -print >> ${Static}
	@echo "-I../include" >> ${Static}
	@echo "-I../../../include" >> ${Static}

$(COMMONTARGS): $(COMMONPREF)$$@

# need an extra rule to clean, clobber and rmtargets for all object directories
depend incdepend fluff tags: $(COMMONPREF)$$@

clean:
	for i in $(EVERYENDIAN); do rm -f $$i.O/*.o; done

rmtargets:
	for i in $(EVERYENDIAN); do rm -f $$i.O/$(TARGETS); done

clobber:
	for i in $(EVERYENDIAN); do rm -rf $$i.O; done

lint: $(LOBJECTS)
	cd $(TARGETDIR) ; \
	$(LINT) $(LINTLDFLAGS) $(LOBJECTS) $(LLIBS)
