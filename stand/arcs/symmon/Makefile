#ident "symmon/Makefile: $Revision: 1.167 $"
#
# Makefile for symmon, the standalone debugger
#
DEPTH= ..
include ${DEPTH}/commondefs
include $(PRODUCTDEFS)
include $(ENDIANDEFS)
include $(CPUARCHDEFS)

DEF_CSTYLE=

IP1932_X=\#
IP19MK_X=\#
IP19_X=\#
IP20_X=\#
IP21_X=\#
IP21Q_X=\#
IP22_Y=\#
IP25_X=\#
IP26_X=\#
IP27_X=\#
IP27XXL_X=\#
IP27H1_X=\#
IP27SABLE_X=\#
IP28_X=\#
IP30_X=\#
IP32_X=\#
$(CPUBOARD)$(SUBPRODUCT)_X=
$(CPUBOARD)_Y=

include $(DEPTH)/netdbxdefs
$(NETDBX)SCFG=$(CPUBOARD)conf.nr

TARGETDIR= $(PRODUCT).O
OBJECTDIR=$(TARGETDIR)
SYMMONBINARY=symmon

ELFTOCOFF=echo "\telf2coff $$@" && \
	$(ELF2COFF) $$@ $$@.coff && \
	mv $$@ $$@.elf && mv $$@.coff $$@

# Since symmon is loaded relocatable on SN0, undefined symbols are not
# reported by ld and silently end up NULL.  CHECKSYM looks for such symbols.

UNDEFSYM=${TOOLROOT}/usr/bin/nm -uh symmon | \
	sed -e /_gp/d -e /ftext/d -e /_end/d

CHECKSYM=if [ "`${UNDEFSYM}`" != "" ]; then \
	echo "Undefined symbols:"; ${UNDEFSYM}; exit 1; fi

SUPERSTRIP=echo "\tsuper stripping $$@.stripped"; \
	cp $$@ $$@.stripped; $(STRIP) -fs $$@.stripped 2>1 > /dev/null || true

# NOTE: the size is the max size symmon can be to fit; and depends on
# the LOADADDR and the DBGLOADADDR in master.d/system.gen
# (in order for the math to work, the DBGLOADADDR here is given as KSEG1)
# The addresses must have hex digits in upper case for the size check
# to work

$(IP19MK_X)DBGLOADADDR=A800000000180000
$(IP19MK_X)LOADADDR=A800000000054000
$(IP19MK_X)POSTLDOP=${SUPERSTRIP}
$(IP19MK_X)SYMMONBINARY=symmon.stripped
$(IP1932_X)DBGLOADADDR=80100000
$(IP1932_X)LOADADDR=80034000
$(IP19_X)DBGLOADADDR=A800000000180000
$(IP19_X)LOADADDR=A800000000054000
$(IP19_X)POSTLDOP=${SUPERSTRIP}
$(IP19_X)SYMMONBINARY=symmon.stripped

$(IP20_X)DBGLOADADDR=A8062000
$(IP20_X)LOADADDR=A8007000
$(IP20_X)POSTLDOP=$(ELFTOCOFF)
$(IP20_X)NOSTRIP=nostrip

$(IP21_X)DBGLOADADDR=A800000000100000
$(IP21_X)LOADADDR=A800000000034000
$(IP21_X)POSTLDOP=${SUPERSTRIP}
$(IP21_X)SYMMONBINARY=symmon.stripped
$(IP21Q_X)DBGLOADADDR=A800000000100000
$(IP21Q_X)LOADADDR=A800000000034000

$(IP22_Y)$(NETDBX)DBGLOADADDR=A8089000
$(IP22_Y)$(NONETDBX)DBGLOADADDR=A8070000
$(IP22_Y)LOADADDR=A8007000
$(IP22_Y)POSTLDOP=$(ELFTOCOFF)
$(IP22_Y)NOSTRIP=nostrip

$(IP26_X)$(NETDBX)DBGLOADADDR=98000000080a0000
$(IP26_X)$(NONETDBX)DBGLOADADDR=9800000008070000
$(IP26_X)LOADADDR=9800000008007000
$(IP26_X)POSTLDOP=${SUPERSTRIP}
$(IP26_X)SYMMONBINARY=symmon.stripped

# actual loadaddr is passed by kernel to arcs_load, 'cos symmon is compiled 
# relocatable for SN0. Look at sn0addrs.h for SYMMON_LOADADDR
$(IP27_X)DBGLOADADDR=A800000000300000
$(IP27_X)LOADADDR=A800000000034000
$(IP27_X)POSTLDOP=${SUPERSTRIP}; ${CHECKSYM}
$(IP27_X)SYMMONBINARY=symmon.stripped

$(IP27XXL_X)DBGLOADADDR=A800000000360000
$(IP27XXL_X)LOADADDR=A800000000034000
$(IP27XXL_X)POSTLDOP=${SUPERSTRIP}; ${CHECKSYM}
$(IP27XXL_X)SYMMONBINARY=symmon.stripped

$(IP27H1_X)DBGLOADADDR=A800000000300000
$(IP27H1_X)LOADADDR=A800000000034000
$(IP27H1_X)POSTLDOP=${SUPERSTRIP}
$(IP27H1_X)SYMMONBINARY=symmon.stripped

$(IP27SABLE_X)DBGLOADADDR=A800000000190000
$(IP27SABLE_X)LOADADDR=A800000000034000
$(IP27SABLE_X)POSTLDOP=${SUPERSTRIP}
$(IP27SABLE_X)SYMMONBINARY=symmon.stripped

$(IP28_X)$(NETDBX)DBGLOADADDR=980000002009C000
$(IP28_X)$(NONETDBX)DBGLOADADDR=9800000020080000
$(IP28_X)LOADADDR=9800000020007000
$(IP28_X)POSTLDOP=${SUPERSTRIP}
$(IP28_X)SYMMONBINARY=symmon.stripped
$(IP28_X)LIBSC=../$(CPULIBSC)
$(IP28_X)DLIBSC=$(DCPULIBSC)

$(IP30_X)$(NETDBX)DBGLOADADDR=A80000002009C000
$(IP30_X)$(NONETDBX)DBGLOADADDR=A800000020080000
$(IP30_X)LOADADDR=A80000002000A000
$(IP30_X)POSTLDOP=${SUPERSTRIP}
$(IP30_X)SYMMONBINARY=symmon.stripped

$(IP32_X)$(NONETDBX)DBGLOADADDR=A0069000
$(IP32_X)LOADADDR=A0007000
$(IP32_X)POSTLDOP=
$(IP32_X)NOSTRIP=nostrip

$(IP25_X)DBGLOADADDR=9000000000140000
$(IP25_X)LOADADDR=9000000000054000
$(IP25_X)POSTLDOP=${SUPERSTRIP}
$(IP25_X)SYMMONBINARY=symmon.stripped

$(IP19MK_X)CPUC=EVEREST.c hook.c
$(IP1932_X)CPUC=EVEREST.c hook.c
$(IP19_X)CPUC=EVEREST.c hook.c
$(IP21_X)CPUC=EVEREST.c
$(IP21Q_X)CPUC=EVEREST.c
$(IP20_X)CPUC=IP20.c
$(IP22_Y)CPUC=IP22.c
$(IP25_X)CPUC=EVEREST.c hook.c
$(IP26_X)CPUC=IP26.c
$(IP27_X)CPUC=SN0.c hook.c
$(IP27XXL_X)CPUC=SN0.c hook.c
$(IP27H1_X)CPUC=SN0.c hook.c
$(IP27SABLE_X)CPUC=SN0.c hook.c
$(IP28_X)CPUC=IP28.c
$(IP30_X)CPUC=IP30.c hook.c
$(IP32_X)CPUC=IP32.c

MODETAG=MODE=32bit
$(IP19_X)MODETAG=MODE=64bit
$(IP1932_X)MODETAG=MODE=32bit
$(IP19MK_X)MODETAG=MODE=64bit
$(IP21_X)MODETAG=MODE=64bit
$(IP21Q_X)MODETAG=MODE=64bit
$(IP25_X)MODETAG=MODE=64bit
$(IP26_X)MODETAG=MODE=64bit
$(IP27_X)MODETAG=MODE=64bit
$(IP27XXL_X)MODETAG=MODE=64bit
$(IP27H1_X)MODETAG=MODE=64bit
$(IP27SABLE_X)MODETAG=MODE=64bit
$(IP28_X)MODETAG=MODE=64bit
$(IP30_X)MODETAG=MODE=64bit

64_Q=\#
${CPUARCH_PFIX}_Q=

# uncomment LD_MAP if you want a load map.  This is mostly useless,
# and is here only for debugging.
# LD_MAP = -m > $@.map

# uncomment this for debugging, but don't do it ordinarily, it
# just wastes time and disk space.
# NAMELIST = ${NM} -Bx $(TARGETDIR)/$@ > $(TARGETDIR)/$@.nm &

ASFILES= csu.s faultasm.s
CFFILES= ${SCFG}.cf fsconf.cf
CFILES= ${CPUC} brkpt.c btrace.c calc.c commands.c coord.c dbgmon.c disMips.c \
	fault.c idbg.c machdep.c mp.c netprotocol.c parser.c salink.c sym.c \
	protocol.c rdebug.c ${CFFILES:.cf=.c} htport.c 
HFILES= idbg.h protocol.h protoio.h

LDIRT= $(TARGETDIR)/vers.? $(TARGETDIR)/*.nm $(TARGETDIR)/*.map *.ln \
	${ALLCFDIRT} htport.c
VPATH= $(OBJECTDIR)

LLCDEFS=-D${CPUBOARD} ${SIMFLAGS} ${NETDBXFLAGS}

LLIBDEPTH=../
LLDOPTS=-e start -T $(LOADADDR) -non_shared -N
LDEPLIB=$(DLIBSK)
LLDLIBS=$(LIBSK)
$(IP27_X)LLDOPTS = -e start -r -d
$(IP27_X)LLDLIBS=$(LIBSK) $(LIBKL)
$(IP27XXL_X)LLDOPTS = -e start -r -d
$(IP27XXL_X)LLDLIBS=$(LIBSK) $(LIBKL)
$(IP27H1_X)LLDLIBS=$(LIBSK) $(LIBKL)
$(IP27SABLE_X)LLDLIBS=$(LIBSK) $(LIBKL)

IDB_TAG='std.sw.debug mach(CPUBOARD=${CPUBOARD}${SUBPRODUCT} ${MODETAG}) \
	postop("if test X$$instmode = Xnormal ; then /sbin/dvhtool -v creat $$rbase/stand/symmon symmon $$vhdev && rm -f $$rbase/stand/symmon; fi") ${NOSTRIP}'
DL_IDB_TAG='DSKLESS_CLT mach(CPUBOARD=${CPUBOARD}${SUBPRODUCT} ${MODETAG}) ${NOSTRIP}'

TARGETS= symmon

default: ${TARGETS}

COMMONPREF= symmon

include ${DEPTH}/commonrules
MKDEPRULE= $(EVERYPRODUCT_MKDEPRULE)

install: default
	${INSTALL} -idb ${IDB_TAG} -F /stand -src $(TARGETDIR)/$(SYMMONBINARY) symmon
	${INSTALL} -idb ${DL_IDB_TAG} -F /sbin/stand -src $(TARGETDIR)/$(SYMMONBINARY) symmon.${CPUBOARD}${SUBPRODUCT}
	${INSTALL} -idb ${DL_IDB_TAG} -F /stand -lns ../sbin/stand/symmon.${CPUBOARD}${SUBPRODUCT} symmon

incdepend: ${CFFILES:.cf=.c} ${COMMONPREF}incdepend
depend: ${CFFILES:.cf=.c} ${COMMONPREF}depend

# NOTE: the size check is here, because a jump in size indicates that someone
# wasn't careful about a change in lib, and a large chunk of
# extra code has been dragged in.
symmon: $(TARGETDIR) $(OBJECTS)	$(DEPLIB)
	${NEWVERS} symmon ${RELEASE} "${CPUBOARD}" > vers.c ;\
	$(CCF) -c vers.c -o $(TARGETDIR)/vers.o; \
	cd $(TARGETDIR); \
	${LD} ${CPUARCH_MI_LDOPTS} ${LLDOPTS} -o $@ \
	  ${OBJECTS} vers.o ${LLDLIBS} ${GLDLIBS} $(LD_MAP); \
	  $(NAMELIST)
	@$(MAKE) chksize
	@cd $(TARGETDIR); $(POSTLDOP)

sablemon:
	make TARGETDIR=$(PRODUCT)S.O SIMFLAGS=-DSABLE symmon

# need to build our own htport.c 
HTPORTC=../lib/libsk/graphics/htport.c
htport.c: ${HTPORTC} Makefile
	rm -f htport.c
	echo "#define _TP_KERNEL" > htport.c
	cat ${HTPORTC} >> htport.c

# 64-bit compiles will use RAGNAROK options, 32-bit compiles use ucode.
#
SYMMON_SIZE=${TOOLROOT}/usr/bin/size -B symmon | sed -e 1d -e 's/^[0-9]*[ 	][0-9]*[ 	][0-9]*[^ ]//' -e 's/[ 	].*$$//'
$(64_Q)SYMMON_SIZE=$${TOOLROOT}/usr/bin/size -B symmon | sed '1d' | awk '{printf "%s ", $$$$(NF-1)}'
$(64_Q)SIZE=${TOOLROOT}/usr/bin/size -Ax

# we move symmon symmon.LARGE just so it can't get put into a
# product image if someone isn't watching the logs.
chksize:
	@cd $(TARGETDIR); left=`( ${SYMMON_SIZE}; \
	echo 16i ${DBGLOADADDR} ${LOADADDR} - - p q) | dc`; \
	if [ "$$left" -gt 0 ]; \
	then \
		echo symmon.LARGE for ${CPUBOARD} is too large by $$left; \
		${SIZE} symmon;\
		mv symmon symmon.LARGE; \
		exit 1; \
	fi

tags: $(COMMONPREF)tags

clean: lclobber $(COMMONPREF)clean
clobber: lclobber
rmtargets: lrmtargets

lclobber: ${COMMONPREF}clobber
	for d in $(EVERYPRODUCT) $(EXTRAPRODUCTS); do rm -rf $$d.O; done

lrmtargets:
	for d in $(EVERYPRODUCT) $(EXTRAPRODUCTS); do rm -rf $$d.O/$(TARGETS); done

lint: ${LOBJECTS}
	$(LINT) $(LINTCFLAGS) ${LOBJECTS} ${LLIBS}
	rm -f ${LOBJECTS}

symtalk: symtalk.c
	cc -o symtalk symtalk.c -lgen
