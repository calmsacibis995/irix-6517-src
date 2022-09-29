#
# Makefile for standalone version of ide
#
#ident "stand/arcs/ide/Makefile:  $Revision: 1.158 $"

DIAGDEPTH=.
LLIBDEPTH=../
include Makedefs
include ${ENDIANDEFS}

DEFAULT_SCRIPT= shell_
FIELD_SCRIPT= field_
RELEASEIDE=field.ide
OPTIONIDE=ip22option.ide

IP20MFGIDE=ip20.ide 
IP22MFGIDE=ip22option.ide
IP26MFGIDE=ip26config.ide
IP28MFGIDE=ip28config.ide

GEN_CFILES= ide_builtins.c
SHELL_CFILES= ${CPUBOARD}_diags.c
SHELL_OBJECTS= ${SHELL_CFILES:.c=.o}
FIELD_CFILES= ${CPUBOARD}_field_diags.c
FIELD_OBJECTS= ${FIELD_CFILES:.c=.o}
OPT_CFILES= ${CPUBOARD}_opt_diags.c
OPT_OBJECTS= ${OPT_CFILES:.c=.o}
CORE_CFILES= ${CPUBOARD}_core_diags.c
CORE_OBJECTS= ${CORE_CFILES:.c=.o}
CFILES= $(GEN_CFILES)

LLDIRT= $(GEN_CFILES) $(TARGETDIR)/*.map $(TARGETDIR)/*.nm  \
	${OPT_CFILES} ${SHELL_CFILES} ${FIELD_CFILES} ${CORE_CFILES} \
	*.ide.*.c 

LTARGETS= ${DEFAULT_SCRIPT:_=.ide}
LLDLIBS=libidecmn.a libide.a $(LIBSK) 
LDEPLIB= $(TARGETDIR)/libidecmn.a $(TARGETDIR)/libide.a $(DLIBSK) 
ELFTOCOFF=echo "\telf2coff $$@" && $(ELF2COFF) $$@ $$@.coff &&mv $$@ $$@.elf && mv $$@.coff $$@

CPUS=IP19 IP21 IP25 IP20 IP22 IP26 IP28 IP30 IP32

IP19_X = \#
IP20_X = \#
IP21_X = \#
IP25_X = \#
IP22_X = \#
IP26_X = \#
IP28_X = \#
IP30_X = \#
IP32_X = \#

IP22_64Y = \#

# To activate Modular IDE builds, "setenv IDE_TYPE MODULAR" before make ...
IP22_MODULAR_Z = \#

$(CPUBOARD)_X=
$(CPUBOARD)_$(COMPILATION_MODEL)Y=
$(CPUBOARD)_$(IDE_TYPE)_Z=

# Let LDADDRS
TLOADADDR=-T $(TEXTSTART) -D $(DATASTART)
LDADDRS=$(TLOADADDR)
LDRELOC=-non_shared

MFGSCRIPT= xxx

# make sure there is no text/data overlap and bss does not overflow
$(IP19_X)TEXTSTART= A0100000
$(IP19_X)LOADADDR = A0100000
$(IP19_X)DATASTART= A0200000
$(IP19_X)TOPCHECK = AFFFFFFF
$(IP19_X)ARCHDIR =
$(IP19_X)IDE_POSTOP=

$(IP20_X)TEXTSTART= 88500000
$(IP20_X)LOADADDR = A8500000
$(IP20_X)DATASTART= A85A5000
$(IP20_X)TOPCHECK = A8740000
$(IP20_X)ARCHDIR = fforward isddevices
$(IP20_X)IDE_POSTOP=
$(IP20_X)POSTLDOP =$(ELFTOCOFF)
$(IP20_X)USEELFTOCOFF=yes
$(IP20_X)TLOADADDR=-elspec ../elspec32.IP20
$(IP20_X)ELSPECDEP=elspec32.IP20
$(IP20_X)NOSTRIP = nostrip

$(IP21_X)TEXTSTART= a800000002000000
$(IP21_X)LOADADDR = 9000000002000000
$(IP21_X)DATASTART= 9800000002403000
$(IP21_X)TOPCHECK = 9800000002740000
$(IP21_X)ARCHDIR = 
$(IP21_X)IDE_POSTOP=
$(IP21_X)POSTLDOP = cp $$@ $$@.stripped; \
	$(STRIP) -s $@.stripped 2>1 > /dev/null || true
$(IP21_X)RELEASEIDE=field.ide.stripped

$(IP22_X)TEXTSTART= 88400000
$(IP22_X)LOADADDR = A8400000
$(IP22_X)DATASTART= A8560000
$(IP22_X)TOPCHECK = A8740000
$(IP22_X)ARCHDIR = fforward isddevices
$(IP22_X)IDE_POSTOP=
$(IP22_X)POSTLDOP =$(ELFTOCOFF)
$(IP22_X)USEELFTOCOFF=yes
$(IP22_X)NOSTRIP = nostrip
$(IP22_X)TLOADADDR=-elspec ../elspec32.IP22
$(IP22_X)ELSPECDEP=elspec32.IP22
$(IP22_MODULAR_Z)TEXTSTART= 88800000
$(IP22_MODULAR_Z)DATASTART= 88A00000

$(IP25_X)TEXTSTART= a800000002000000
$(IP25_X)LOADADDR = 9000000002000000
$(IP25_X)DATASTART= 9800000002403000
$(IP25_X)TOPCHECK = 9800000002740000
$(IP25_X)ARCHDIR = 
$(IP25_X)IDE_POSTOP=
$(IP25_X)POSTLDOP = cp $$@ $$@.stripped; \
	$(STRIP) -s $@.stripped 2>1 > /dev/null || true
$(IP25_X)RELEASEIDE=field.ide.stripped

# ldram depends on ide being in 8-11MB range
$(IP26_X)TEXTSTART= A800000008800000
$(IP26_X)LOADADDR = 9000000008800000
$(IP26_X)DATASTART= 9000000008933000
$(IP26_X)TOPCHECK = 9000000008B00000
$(IP26_X)IDE_POSTOP=
$(IP26_X)POSTLDOP = cp $$@ $$@.stripped; \
	$(STRIP) -s $@.stripped 2>1 > /dev/null || true
$(IP26_X)ARCHDIR = fforward isddevices
$(IP26_X)RELEASEIDE=field.ide.stripped

# ldram depends on ide being in 8-11MB range
$(IP28_X)TEXTSTART= a800000020800000
$(IP28_X)LOADADDR = 9000000020800000
$(IP28_X)DATASTART= 9000000020980000
$(IP28_X)TOPCHECK = 9000000020C00000
$(IP28_X)IDE_POSTOP=
$(IP28_X)POSTLDOP = cp $$@ $$@.stripped; \
	$(STRIP) -s $@.stripped 2>1 > /dev/null || true
$(IP28_X)ARCHDIR = fforward isddevices IP26/ecc
$(IP28_X)RELEASEIDE=field.ide.stripped
$(IP28_X)LIBSC=$(CPULIBSC)
$(IP28_X)DLIBSC=$(DCPULIBSC)

$(IP30_X)TEXTSTART= A800000020800000
$(IP30_X)LOADADDR = A800000020800000
$(IP30_X)DATASTART= A800000020A00000
$(IP30_X)TOPCHECK = A800000020C00000
$(IP30_X)IDE_POSTOP=

$(IP30_X)POSTLDOP = cp $$@ $$@.stripped; \
	$(STRIP) -s $@.stripped 2>1 > /dev/null || true
$(IP30_X)ARCHDIR = godzilla isddevices
$(IP30_X)RELEASEIDE=field.ide.stripped
$(IP30_X)TLOADADDR=-T $(TEXTSTART)

$(IP32_X)TEXTSTART= A00A0000
$(IP32_X)LOADADDR = A00A0000
$(IP32_X)DATASTART= A0200000
$(IP32_X)TOPCHECK = A03F0000
$(IP32_X)IDE_POSTOP=
$(IP32_X)POSTLDOP = cp $$@ $$@.stripped; \
	$(STRIP) -s $@.stripped 2>1 > /dev/null || true
$(IP32_X)ARCHDIR = 
$(IP32_X)RELEASEIDE=field.ide.stripped


$(IP19_X)LLDOPTS=$(CPUARCH_MI_LDOPTS) -e start -m $(LDADDRS)
$(IP20_X)LLDOPTS=$(CPUARCH_MI_LDOPTS) -e start -m $(LDADDRS)
$(IP21_X)LLDOPTS=$(CPUARCH_MI_LDOPTS) -e start -m $(LDADDRS)
$(IP25_X)LLDOPTS=$(CPUARCH_MI_LDOPTS) -e start -m $(LDADDRS)
$(IP22_X)LLDOPTS=$(CPUARCH_MI_LDOPTS) -e start -m $(LDADDRS)
$(IP26_X)LLDOPTS=$(CPUARCH_MI_LDOPTS) -e start -m $(LDADDRS)
$(IP28_X)LLDOPTS=$(CPUARCH_MI_LDOPTS) -e start -m $(LDADDRS)
$(IP30_X)LLDOPTS=$(CPUARCH_MI_LDOPTS) -e start -m $(LDADDRS)
$(IP32_X)LLDOPTS=$(CPUARCH_MI_LDOPTS) -e start -m $(LDADDRS)

# additional commands for various cpus
$(IP20_X)AUXCMDS=IP20/gr2_diagcmds IP20/lg1_diagcmds IP20/vid_diagcmds
$(IP20_X)FIELD_AUXCMDS=IP20/gr2_field_diagcmds IP20/lg1_field_diagcmds IP20/vid_diagcmds

$(IP22_X)AUXCMDS=IP22/gr2_diagcmds IP22/a2_diagcmds IP22/ng1_diagcmds IP22/ng1_mfg_diagcmds IP22/vid_diagcmds IP22/comp_diagcmds IP22/vino_diagcmds
$(IP22_X)FIELD_AUXCMDS=IP22/gr2_diagcmds IP22/mgras_field_diagcmds IP22/a2_field_diagcmds IP22/ng1_diagcmds
$(IP22_X)OPT_AUXCMDS=IP22/gr2_diagcmds IP22/mgras_diagcmds IP22/a2_diagcmds IP22/ng1_diagcmds IP22/ng1_mfg_diagcmds IP22/mco_diagcmds IP22/mgv_diagcmds IP22/comp_diagcmds IP22/cosmo2_diagcmds IP22/vid_diagcmds IP22/vino_diagcmds 

$(IP26_X)AUXCMDS= IP26/gr2_diagcmds IP26/vid_diagcmds
$(IP26_X)FIELD_AUXCMDS= IP26/gr2_diagcmds

$(IP28_X)AUXCMDS= IP28/gr2_diagcmds IP28/vid_diagcmds
$(IP28_X)FIELD_AUXCMDS= IP28/gr2_diagcmds IP28/mgras_field_diagcmds IP28/a2_field_diagcmds

$(IP30_X)AUXCMDS= IP30/mgras_field_diagcmds
$(IP30_X)FIELD_AUXCMDS= IP30/mgras_field_diagcmds

$(IP32_X)AUXCMDS= IP32/crm_graphics_diagcmds
$(IP32_X)FIELD_AUXCMDS= 
sinclude IP32/mace_include

OPT_SCRIPT=xxx

$(IP19_X)MFGSCRIPT= xxx
$(IP20_X)MFGSCRIPT= ip20_ 
$(IP21_X)MFGSCRIPT= xxx
$(IP25_X)MFGSCRIPT= xxx
$(IP22_X)MFGSCRIPT= ip24_ ip24asrs_ 
$(IP26_X)MFGSCRIPT= ip26config_ 
$(IP28_X)MFGSCRIPT= ip28config_
$(IP30_X)MFGSCRIPT= xxx
$(IP32_X)MFGSCRIPT= ip32mfg_

$(IP19_X)OPT_SCRIPT= xxx
$(IP25_X)OPT_SCRIPT= xxx
$(IP20_X)OPT_SCRIPT= xxx
$(IP21_X)OPT_SCRIPT= xxx
$(IP22_X)OPT_SCRIPT= ip22option_
$(IP26_X)OPT_SCRIPT= xxx
$(IP28_X)OPT_SCRIPT= xxx
$(IP30_X)OPT_SCRIPT= xxx
$(IP32_X)OPT_SCRIPT= xxx

$(IP19_X)EXTRATARGETS=
$(IP20_X)EXTRATARGETS= mfgstuff
$(IP21_X)EXTRATARGETS=
# IP22 mfgstuff removed due to ip24 ld problem 12/16/96 -- jeffs
$(IP22_X)EXTRATARGETS=
$(IP25_X)EXTRATARGETS=
$(IP26_X)EXTRATARGETS= mfgstuff
$(IP28_X)EXTRATARGETS=
$(IP30_X)EXTRATARGETS= 
$(IP32_X)EXTRATARGETS= 

$(IP19_X)FIELD_CPUCMDS = ${CPUBOARD}/cpu_diagcmds
$(IP20_X)FIELD_CPUCMDS = ${CPUBOARD}/cpu_field_diagcmds
$(IP21_X)FIELD_CPUCMDS = ${CPUBOARD}/ide_diagcmds
$(IP25_X)FIELD_CPUCMDS = ${CPUBOARD}/ide_diagcmds
$(IP22_X)FIELD_CPUCMDS = ${CPUBOARD}/cpu_field_diagcmds
$(IP26_X)FIELD_CPUCMDS = ${CPUBOARD}/cpu_field_diagcmds
$(IP28_X)FIELD_CPUCMDS = ${CPUBOARD}/cpu_field_diagcmds
$(IP30_X)FIELD_CPUCMDS = ${CPUBOARD}/cpu_field_diagcmds
$(IP32_X)FIELD_CPUCMDS = ${CPUBOARD}/cpu_field_diagcmds

$(IP19_X)MFG_CPUCMDS = ${CPUBOARD}/cpu_diagcmds
$(IP20_X)MFG_CPUCMDS = ${CPUBOARD}/cpu_field_diagcmds
$(IP21_X)MFG_CPUCMDS = ${CPUBOARD}/ide_diagcmds
$(IP25_X)MFG_CPUCMDS = ${CPUBOARD}/ide_diagcmds
$(IP22_X)MFG_CPUCMDS = ${CPUBOARD}/cpu_diagcmds
$(IP26_X)MFG_CPUCMDS = ${CPUBOARD}/cpu_diagcmds
$(IP28_X)MFG_CPUCMDS = ${CPUBOARD}/cpu_diagcmds
$(IP30_X)MFG_CPUCMDS = ${CPUBOARD}/cpu_diagcmds
$(IP32_X)MFG_CPUCMDS = ${CPUBOARD}/cpu_diagcmds

$(IP22_X)MFG_OPTCMDS = ${CPUBOARD}/opt_diagcmds
# $(IP26_X)MFG_OPTCMDS = ${CPUBOARD}/opt_diagcmds
# $(IP28_X)MFG_OPTCMDS = ${CPUBOARD}/opt_diagcmds
# $(IP30_X)MFG_OPTCMDS = ${CPUBOARD}/opt_diagcmds

$(IP22_X)CORE_CMDS = $(CPUBOARD)/core_diagcmds
$(IP30_X)CORE_CMDS = $(CPUBOARD)/core_diagcmds

# need to run 'size' on elf version
SHELL_IDE=shell.ide.elf
FIELD_IDE=field.ide.elf
OPT_IDE=ip22option.ide.elf
$(USEELFTOCOFF)SHELL_IDE=shell.ide
$(USEELFTOCOFF)FIELD_IDE=field.ide
$(USEELFTOCOFF)OPT_IDE=ip22option.ide

SETSYM =$(TOOLROOT)/usr/sbin/setsym
IDBG_OBJ= ${DEPTH}/lib/libsc/$(ENDIANND)$(CPUARCH_PFIX).O/idbg.o

# Macro for 32/64-bit differentiation.
64_Q=\#
${CPUARCH_PFIX}_Q=

# we want the default rule from Makerules
COMMONPREF=ide

default: makelink common ${CPUBOARD} ${ARCHDIR} $(COMMONPREF)default ides chksize

ides: ${DEFAULT_SCRIPT:_=.ide} ${DEFAULT_SCRIPT:_=.ide}.DBG ${FIELD_SCRIPT:_=.ide} ${FIELD_SCRIPT:_=.ide}.DBG 

# Modular IDE
modular: core modules

# core parts of Modular IDE
core: core_msg common_core elf core_cpuboard core_archdir link_core_ide

core_msg:
	@echo "-----\tBuilding IDE Core"

common_core: ${_FORCE}
	@echo "=====\tcd common; ${MAKE} LIBTYPE=core default"; \
	( cd common; ${MAKE} LIBTYPE=core default );

elf: ${_FORCE}
	@echo "=====\tcd $@; ${MAKE} LIBTYPE=core default"; \
	( cd $@; ${MAKE} LIBTYPE=core default );

core_cpuboard:
	@echo "=====\tcd ${CPUBOARD}; ${MAKE} LIBTYPE=core core"; \
	( cd ${CPUBOARD}; ${MAKE} LIBTYPE=core core );	

core_archdir:
	@echo "=====\tcd ${ARCHDIR}; ${MAKE} LIBTYPE=core core"; \
	( cd ${ARCHDIR}; ${MAKE} LIBTYPE=core core );	

link_core_ide:
	${MAKE} LIBTYPE=core core.ide	

# module pieces of Modular IDE
modules: module_msg module module_cpu

module_msg:
	@echo "-----\tBuilding IDE Modules"

module: ${_FORCE}
	@echo "=====\tcd $@; ${MAKE} LIBTYPE=modules default"; \
	( cd $@; ${MAKE} LIBTYPE=modules default );	

# module_cpu module pieces here
module_cpu: module_cpu_msg module_archdir module_cpudir 
	@echo "=====\tcd ${CPUBOARD}/modules/$@; ${MAKE} SHARE_COPT= SHARE_ASOPT=-KPIC LIBTYPE=module_cpu module_cpu"; \
	( cd ${CPUBOARD}/modules/$@; ${MAKE} SHARE_COPT= SHARE_ASOPT=-KPIC LIBTYPE=module_cpu module_cpu );	

module_cpu_msg:
	@echo "------\tBuilding IDE module_cpu Module"

module_archdir:
	@echo "=====\tcd ${ARCHDIR}; ${MAKE} LIBTYPE=module_cpu _module_cpu"; \
	( cd ${ARCHDIR}; ${MAKE} LIBTYPE=module_cpu _module_cpu );

module_cpudir:
	@echo "=====\tcd ${CPUBOARD}; ${MAKE} LIBTYPE=module_cpu _module_cpu"; \
	( cd ${CPUBOARD}; ${MAKE} LIBTYPE=module_cpu _module_cpu );

makelink:
	@for i in $(CPUS); do \
		(case $$i { \
		IP19) if [ ! -l IP19 ] ; then ln -sf EVEREST IP19 ; fi ;;\
		IP21) if [ ! -l IP21 ] ; then ln -sf EVEREST IP21 ; fi ;;\
		IP25) if [ ! -l IP25 ] ; then ln -sf EVEREST IP25 ; fi ;;\
		*)    ;; }; ); \
	done

.PATH: $(TARGETDIR)

include Makerules
MKDEPRULE= $(EVERYPRODUCT_MKDEPRULE)

$(COMMONTARGS): $(COMMONPREF)$$@

install: default $(EXTRATARGETS)
	cd ${TARGETDIR}; \
	${INSTALL} -idb 'std.sw.unix diag.sw.diag mach(CPUBOARD=${CPUBOARD}) ${IDE_POSTOP} $(NOSTRIP)' \
	-src $(RELEASEIDE) -F /usr/stand ide; \
	${INSTALL} -idb 'DSKLESS_CLT mach(CPUBOARD=${CPUBOARD}) $(NOSTRIP)' -src $(RELEASEIDE) -F /sbin/stand ide.$(CPUBOARD); \
	${INSTALL} -idb 'DSKLESS_CLT mach(CPUBOARD=${CPUBOARD}) $(NOSTRIP)' -F /usr/stand -lns ../sbin/stand/ide.$(CPUBOARD) ide; \
	${INSTALL} -idb "sa $(NOSTRIP)" -src $(RELEASEIDE) -F / 40ide.$(CPUBOARD)

ide_builtins.c: builtins.awk builtins
	nawk -F'	' -f builtins.awk TBL=builtins \
		builtins > B$$$$.o && mv B$$$$.o $@

${CPUBOARD}_diags.c: ${MFG_CPUCMDS} ${AUXCMDS} Makefile
	nawk -F'	' -f builtins.awk TBL=diagcmds \
		${MFG_CPUCMDS} ${AUXCMDS} > C$$$$.o && mv C$$$$.o $@

${CPUBOARD}_opt_diags.c: ${MFG_OPTCMDS} ${OPT_AUXCMDS} Makefile
	nawk -F'	' -f builtins.awk TBL=diagcmds \
	${CPUBOARD}/opt_diagcmds ${OPT_AUXCMDS} > C$$$$.o && mv C$$$$.o $@

${CPUBOARD}_field_diags.c: ${FIELD_CPUCMDS} ${FIELD_AUXCMDS} Makefile
	nawk -F'	' -f builtins.awk TBL=diagcmds \
		${FIELD_CPUCMDS} ${FIELD_AUXCMDS} > C$$$$.o && mv C$$$$.o $@

${CPUBOARD}_core_diags.c: ${CORE_CMDS}
	nawk -F'	' -f builtins.awk TBL=diagcmds \
		${CORE_CMDS} > C$$$$.o && mv C$$$$.o $@

# for directories w/o specific 
${CPUBOARD}/field.${CPUBOARD}:
	(cd ${CPUBOARD}; ln -s field field.${CPUBOARD})
${CPUBOARD}/shell.${CPUBOARD}:
	(cd ${CPUBOARD}; ln -s shell shell.${CPUBOARD})

# because of the dependencies, and the awk scripts having no suffix,
# these can't use ${DEFAULT_SCRIPT}
field.ide.${CPUBOARD}.c: ${CPUBOARD}/field.${CPUBOARD}
	sed 's/"/\\"/g' $? | sed '/^#/d' | awk -f startup.awk > $@
shell.ide.${CPUBOARD}.c: ${CPUBOARD}/shell.${CPUBOARD}
	sed 's/"/\\"/g' $? | sed '/^#/d' | awk -f startup.awk > $@
core.ide.${CPUBOARD}.c: ${CPUBOARD}/core.${CPUBOARD}
	sed 's/"/\\"/g' $? | sed '/^#/d' | awk -f startup.awk > $@
ip20.ide.IP20.c: IP20/ip20
	sed 's/"/\\"/g' $? | sed '/^#/d' | awk -f startup.awk > $@
ip22config.ide.IP22.c: IP22/ip22config
	sed 's/"/\\"/g' $? | sed '/^#/d' | awk -f startup.awk > $@
ip22option.ide.IP22.c: IP22/ip22option
	sed 's/"/\\"/g' $? | sed '/^#/d' | awk -f startup.awk > $@
ip26config.ide.IP26.c: IP26/ip26config
	sed 's/"/\\"/g' $? | sed '/^#/d' | awk -f startup.awk > $@
ip26final.ide.IP26.c: IP26/ip26final
	sed 's/"/\\"/g' $? | sed '/^#/d' | awk -f startup.awk > $@
ip26asrs.ide.IP26.c: IP26/ip26asrs
	sed 's/"/\\"/g' $? | sed '/^#/d' | awk -f startup.awk > $@
ip28final.ide.IP28.c: IP28/ip28final
	sed 's/"/\\"/g' $? | sed '/^#/d' | awk -f startup.awk > $@
ip28asrs.ide.IP28.c: IP28/ip28asrs
	sed 's/"/\\"/g' $? | sed '/^#/d' | awk -f startup.awk > $@
ip28config.ide.IP28.c: IP28/ip28config
	sed 's/"/\\"/g' $? | sed '/^#/d' | awk -f startup.awk > $@
gfx.ide.${CPUBOARD}.c: ${CPUBOARD}/gfx
	sed 's/"/\\"/g' $? | sed '/^#/d' | awk -f startup.awk > $@
oven.ide.${CPUBOARD}.c: ${CPUBOARD}/oven
	sed 's/"/\\"/g' $? | sed '/^#/d' | awk -f startup.awk > $@
final.ide.${CPUBOARD}.c: ${CPUBOARD}/final
	sed 's/"/\\"/g' $? | sed '/^#/d' | awk -f startup.awk > $@
ip24.ide.${CPUBOARD}.c: ${CPUBOARD}/ip24
	sed 's/"/\\"/g' $? | sed '/^#/d' | awk -f startup.awk > $@
ip24asrs.ide.${CPUBOARD}.c: ${CPUBOARD}/ip24asrs
	sed 's/"/\\"/g' $? | sed '/^#/d' | awk -f startup.awk > $@
ip32mfg.ide.${CPUBOARD}.c: ${CPUBOARD}/ip32mfg
	sed 's/"/\\"/g' $? | sed '/^#/d' | awk -f startup.awk > $@

mfgstuff: ${MFGSCRIPT:_=.ide}

mfgoptstuff: ${OPT_SCRIPT:_=.ide}

${DEFAULT_SCRIPT:_=.ide}: $(TARGETDIR) ${OBJECTS} ${SHELL_OBJECTS} ${DEFCFG} \
			  ${DEPLIB} ${TARGETDIR}/ide_gram.h $$@.${CPUBOARD}.o\
			  ${ELSPECDEP}
	${CCF} -DMFG_USED -c ./common/ide_alloc.c -o $(TARGETDIR)/ide_alloc.o
	${NEWVERS} ide ${RELEASE} "${CPUBOARD} IDE $(@:.ide=)" > $(@:.ide=v.c)
	${CCF} -c $(@:.ide=v.c) -o $(TARGETDIR)/$(@:.ide=v.o)
	rm -f $(@:.ide=v.c)
	cd ${TARGETDIR}; ${LD} ${LDRELOC} -o $(@) ${LDOPTS} ${OBJECTS} ${SHELL_OBJECTS} ../${DEFCFG} \
	${@:.ide=v.o} $@.${CPUBOARD}.o ${LDLIBS} ${ENDIAN} > ${@):.ide=.map}; \
	${NM} -Bn $@ > ${@:.ide=.nm}; $(POSTLDOP)

${FIELD_SCRIPT:_=.ide}: $(TARGETDIR) ${OBJECTS} ${FIELD_OBJECTS} ${DEFCFG} \
			${DEPLIB} ${TARGETDIR}/ide_gram.h $$@.${CPUBOARD}.o\
			${ELSPECDEP}
	${NEWVERS} ide ${RELEASE} "${CPUBOARD} IDE $(@:.ide=)" > $(@:.ide=v.c)
	${CCF} -c $(@:.ide=v.c) -o $(TARGETDIR)/$(@:.ide=v.o)
	rm -f $(@:.ide=v.c)
	cd ${TARGETDIR}; ${LD} ${LDRELOC} -o $(@) ${LDOPTS} ${OBJECTS} ${FIELD_OBJECTS} ../${DEFCFG} \
	${@:.ide=v.o} $@.${CPUBOARD}.o ${LDLIBS} ${ENDIAN} > ${@):.ide=.map}; \
	${NM} -Bn $@ > ${@:.ide=.nm}; $(POSTLDOP)

shell.ide.DBG: ${DEFAULT_SCRIPT:_=.ide}
	cd ${TARGETDIR}; ${LD} ${LDRELOC} -o $@ ${LDOPTS} ${OBJECTS} ${SHELL_OBJECTS} ../${DEFCFG}  ../$(IDBG_OBJ) \
	shellv.o shell.ide.${CPUBOARD}.o ${LDLIBS} ${ENDIAN} > shell.DBG.map; \
	${SETSYM} $@

field.ide.DBG: ${FIELD_SCRIPT:_=.ide}
	cd ${TARGETDIR}; ${LD} ${LDRELOC} -o $@ ${LDOPTS} ${OBJECTS} ${FIELD_OBJECTS} ../${DEFCFG}  ../$(IDBG_OBJ) \
	fieldv.o field.ide.${CPUBOARD}.o ${LDLIBS} ${ENDIAN} > field.DBG.map; \
	${SETSYM} $@

${MFGSCRIPT:_=.ide}: $(TARGETDIR) ${OBJECTS} ${SHELL_OBJECTS} ${DEFCFG} \
		     ${DEPLIB} ${TARGETDIR}/ide_gram.h $$@.${CPUBOARD}.o\
		     ${ELSPECDEP}
	${NEWVERS} ide ${RELEASE} "${CPUBOARD} IDE $(@:.ide=)" > $(@:.ide=v.c)
	${CCF} -c $(@:.ide=v.c) -o $(TARGETDIR)/$(@:.ide=v.o)
	rm -f $(@:.ide=v.c)
	cd ${TARGETDIR}; ${LD} ${LDRELOC} -o $(@) ${LDOPTS} ${OBJECTS} ${SHELL_OBJECTS} ../${DEFCFG} \
	${@:.ide=v.o} $@.${CPUBOARD}.o ${LDLIBS} ${ENDIAN} > ${@):.ide=.map}; \
	${NM} -Bn $@ > ${@:.ide=.nm}; $(POSTLDOP)

${OPT_SCRIPT:_=.ide}: $(TARGETDIR) ${OBJECTS} ${OPT_OBJECTS} ${DEFCFG} \
		${DEPLIB} ${TARGETDIR}/ide_gram.h $$@.${CPUBOARD}.o \
		${ELSPECDEP}
	${CCF} -DMFG_USED -c ./common/ide_alloc.c -o $(TARGETDIR)/ide_alloc.o
	${NEWVERS} ide ${RELEASE} "${CPUBOARD} IDE $(@:.ide=)" > $(@:.ide=v.c)
	${CCF} -c $(@:.ide=v.c) -o $(TARGETDIR)/$(@:.ide=v.o)
	rm -f $(@:.ide=v.c)
	cd ${TARGETDIR}; ${LD} ${LDRELOC} -o $(@) ${LDOPTS} ${OBJECTS} ${OPT_OBJECTS} ../${DEFCFG} \
	${@:.ide=v.o} $@.${CPUBOARD}.o ${LDLIBS} ${ENDIAN} > ${@):.ide=.map}; \
	${NM} -Bn $@ > ${@:.ide=.nm}; $(POSTLDOP)

# elspec rules.
#
# We run the elspec file through cpp because an elspec incompatibility was
# introduced between the 7.2 and 7.3 compiler versions.  The new 7.3 ld
# requires that a new keyword, "default_gprel", be present in the file while
# the 7.2 ld will barf if it sees it.  We use the cpp predefined symbol
# _COMPILER_VERSION >= 730 to detect this and automatically select to add the
# new keyword or not.  Unfortunately this is somewhat complicated by the fact
# that the binaries we're generating in these situations are often O32 and the
# O32 compiler doesn't define _COMPILER_VERSION -- and if it did it would
# certainly have a value less than 730!  Since our only real desire is to
# detect whether we're using a 7.2- or a 7.3-based compiler TOOLROOT, we just
# run the elspec file through cpp with -n32 in order to force the N32 compiler
# flags to be used which will define _COMPILER_VERSION for us with the right
# values in either TOOLROOT.
# 
elspec32.$(CPUBOARD): Makefile elspec32.template
	LC_TEXTSTART=`echo ${TEXTSTART} | tr '[A-F]' '[a-f]'`; \
	LC_DATASTART=`echo ${DATASTART} | tr '[A-F]' '[a-f]'`; \
	$(CC) -n32 -E elspec32.template \
	| sed -e "s/TEXTADDR/0x$$LC_TEXTSTART/" \
	    -e "s/DATAADDR/0x$$LC_DATASTART/" > $@

# core.ide rule
CORELIBS= libidecmn.a libidecore.a $(LIBSK) $(LIBSC) libideelf.a
CORE_DEPLIB= $(TARGETDIR)/libidecmn.a $(TARGETDIR)/libidecore.a \
             $(DLIBSK) $(DLIBSC) $(TARGETDIR)/libideelf.a
CORE_LDOPTS= -all -woff 15
CORE_UNDEFINED= -U
core.ide: $(TARGETDIR) ${OBJECTS} $(CORE_OBJECTS) $(CORE_OBJECTS_MISC) \
	  ${DEFCFG} ${CORE_DEPLIB} $$@.${CPUBOARD}.o 
	${NEWVERS} ide ${RELEASE} "${CPUBOARD} IDE $(@:.ide=)" > $(@:.ide=v.c)
	${CCF} -c $(@:.ide=v.c) -o $(TARGETDIR)/$(@:.ide=v.o)
	rm -f $(@:.ide=v.c)
	cd ${TARGETDIR}; \
	${LD} ${LDRELOC} -o $(@) ${LDOPTS} ${CORE_LDOPTS} $(CORE_UNDEFINED) \
	      ${OBJECTS} $(CORE_OBJECTS) $(CORE_OBJECTS_MISC) \
              ../${DEFCFG} ${@:.ide=v.o} $@.${CPUBOARD}.o ${CORELIBS} \
              ${ENDIAN} > ${@):.ide=.map}; \
	${NM} -Bn $@ > ${@:.ide=.nm}

common $(CPUBOARD) cpuboard $(ARCHDIR): ${_FORCE}
	@echo "=====\tcd $@; ${MAKE} default"; \
	( cd $@; ${MAKE} default );

dprom prom: ${_FORCE}
	@for i in $(CPUBOARD) $(ARCHDIR); do \
		echo "=====\tcd $$i; $(MAKE) $@"; \
		( cd $$i; $(MAKE) $@ ); \
	done

tags:
	@for i in common $(CPUS) $(ARCHDIR); do \
		echo "=====\tcd $$i; $(MAKE) $@"; \
		( cd $$i; $(MAKE) $@ ); \
	done

$(COMMONPREF)rmtargets: $(COMMONPREF)lrmtargets

# local rmtargets rule needed to remove the targets in the PRODUCT dir
# descend and do clobber so that non-object files get cleaned up.
$(COMMONPREF)lrmtargets:
	rm -f $(CPUBOARD)_diags.c; \
	rm -rf $(PRODUCT).O $(PRODUCT)p.O; \
	for i in common $(CPUBOARD) $(ARCHDIR); do \
	echo "====\tcd $$i; make clobber";\
	(cd $$i;$(MAKE) clobber);\
	done;\

# make sure ide fits the memory map:
chksize: chkshelltext chkshellbss chkfieldtext chkfieldbss

SIZE_CMD=${SIZE} -B
#GET_TEXT=sed -e 1d | awk '{print $$1}'
#GET_SIZE= sed -e 1d -e 's/^[0-9]*[ 	][0-9]*[ 	][0-9]*[^ ]//' -e 's/[	 ].*$$//'
#GET_TEXT=awk '{print $$1}'
GET_TEXT=sed -e 1d | awk '{printf "%s ", $$1 }'
GET_SIZE=sed -e 1d | awk '{printf "%s ", $$(NF-1)}'

chkshelltext:
	@cd ${TARGETDIR}; left=`( ${SIZE_CMD} ${SHELL_IDE} | ${GET_TEXT}; \
	echo 16i ${LOADADDR} + ${DATASTART} - p q ) | dc`; \
	if [ "$$left" -gt 0 ]; \
	then \
		echo shell.ide.LARGE for ${CPUBOARD} text/data overlap by $$left; \
		mv shell.ide shell.ide.LARGE; \
		exit 1; \
	fi

chkshellbss:
	@cd ${TARGETDIR}; left=`( ${SIZE_CMD} ${SHELL_IDE} | ${GET_SIZE}; \
	echo 16i ${TOPCHECK} ${LOADADDR} - - p q ) | dc`; \
	if [ "$$left" -gt 0 ]; \
	then \
		echo shell.ide.LARGE for ${CPUBOARD} is too large by $$left; \
		mv shell.ide shell.ide.LARGE; \
		exit 1; \
	fi

chkfieldtext:
	@cd ${TARGETDIR}; left=`( ${SIZE_CMD} $(FIELD_IDE) | ${GET_TEXT}; \
	echo 16i ${LOADADDR} + ${DATASTART} - p q ) | dc`; \
	if [ "$$left" -gt 0 ]; \
	then \
		echo field.ide.LARGE for ${CPUBOARD} text/data overlap by $$left; \
		mv field.ide field.ide.LARGE; \
		exit 1; \
	fi

chkfieldbss:
	@cd ${TARGETDIR}; left=`( ${SIZE_CMD} $(FIELD_IDE) | ${GET_SIZE}; \
	echo 16i ${TOPCHECK} ${LOADADDR} - - p q ) | dc`; \
	if [ "$$left" -gt 0 ]; \
	then \
		echo field.ide.LARGE for ${CPUBOARD} is too large by $$left; \
		mv field.ide field.ide.LARGE; \
		exit 1; \
	fi

chkopttext:
	@cd ${TARGETDIR}; left=`( ${SIZE_CMD} ${OPT_IDE} | ${GET_TEXT}; \
	echo 16i ${LOADADDR} + ${DATASTART} - p q ) | dc`; \
	if [ "$$left" -gt 0 ]; \
	then \
		echo ip22option.ide.LARGE for ${CPUBOARD} text/data overlap by $$left; \
		mv ip22option.ide ip22option.ide.LARGE; \
		exit 1; \
	fi

chkoptbss:
	@cd ${TARGETDIR}; left=`( ${SIZE_CMD} ${OPT_IDE} | ${GET_SIZE}; \
	echo 16i ${TOPCHECK} ${LOADADDR} - - p q ) | dc`; \
	if [ "$$left" -gt 0 ]; \
	then \
		echo ip22option.ide.LARGE for ${CPUBOARD} is too large by $$left; \
		mv ip22option.ide ip22option.ide.LARGE; \
		exit 1; \
	fi

