#
# Makefile to make IDE Modules
#

DIAGDEPTH=../../..
include $(DIAGDEPTH)/Makedefs

MODULE= module_cpu
MODULE_CMDS= cpu_diagcmds
MODULE_SCRIPT= $(MODULE)_script

GEN_CFILES= ${MODULE}.c ${MODULE_SCRIPT}_code.c
CFILES= ${GEN_CFILES}

# dirt to clean up ...
LLDIRT= ${GEN_CFILES} 

# Set LDADDRS
TLOADADDR=-T $(TEXTSTART) -D $(DATASTART)
LDADDRS= $(TLOADADDR)
LDRELOC= -shared 

# blank means shared ...
SHARE_COPT= 

# make sure there is no text/data overlap and bss does not overflow
TEXTSTART= A800000030c00000
DATASTART= A800000030d00000
LLDOPTS=$(CPUARCH_MI_LDOPTS) -e start_module -m $(LDADDRS) #-ignore_unresolved

# default rule
# Module IDE module_cpu rule
module_cpu:  ${MODULE}.o ${MODULE_SCRIPT}_code.o
	${NEWVERS} $@ ${RELEASE} "${CPUBOARD} IDE $(@)" > $(@:=_version.c)
	${CCF} -c $(@:=_version.c) -o $(TARGETDIR)/$(@:=_version.o)
	rm -f $(@:=_version.c)
	cd $(TARGETDIR); $(LD) ${LDRELOC} -no_unresolved -wall -o $(@) ${LDOPTS} *.o ../${PRODUCT}modules.O/*.o > $(@).map

# rule for making command .c file
${MODULE}.c: ${MODULE_CMDS}
	nawk -F'	' -f ${DIAGDEPTH}/builtins.awk TBL=diagcmds \
		${MODULE_CMDS} > C$$$$.o && mv C$$$$.o $@

# rule for making script .c file
${MODULE_SCRIPT}_code.c: ${MODULE_SCRIPT}
	sed 's/"/\\"/g' $? | sed '/^#/d' | awk -f ../module_startup.awk > $@

# more common rules
include $(DIAGDEPTH)/../commonrules
