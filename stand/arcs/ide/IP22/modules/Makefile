#
# Makefile to make IDE Modules
#

DIAGDEPTH=../..
include $(DIAGDEPTH)/Makedefs

# Set LDADDRS
TLOADADDR=-T $(TEXTSTART) -D $(DATASTART)
LDADDRS=$(TLOADADDR)
LDRELOC= -shared #-call_shared

# blank means shared ...
SHARE_COPT= 

# make sure there is no text/data overlap and bss does not overflow
TEXTSTART= 89000000 
DATASTART= 89A00000 

LLDOPTS=$(CPUARCH_MI_LDOPTS) -e start_module -m $(LDADDRS) #-ignore_unresolved

MODULE= module_cpu
MODULE_CMDS= cpu_diagcmds
MODULE_SCRIPT= $(MODULE)_script

GEN_CFILES= ${MODULE}.c ${MODULE_SCRIPT}_code.c
CFILES= ${GEN_CFILES}

COMMON_OBJS= module.o module_asm.o 

#
# from fforward
#
CACHE_OBJS= cacheasm.M cache_par.M icache.M scache_ecc.M scache.M \
	    cachesubs.M r4600sc.M

MEM_OBJS= addruniq.M altaccess.M binarybit.M blk.M butterfly.M \
	  checker.M diagram.M dmuxfix.M dram2_diag.M dram_diag.M \
	  kh.M kh_dw.M khdouble.M khlow.M khlow_diag.M khparity.M \
	  ldramtest.M march.M marchxy.M mats.M memtest.M memtest2.M \
	  memutil.M misc_s.M movi.M new3bit.M parasm.M parcheck.M \
	  pargenlog.M parity.M parreg.M private.M shift_dram.M \
	  tb_test.M threebit.M walkingbit.M 

TLB_OBJS= global.M mem.M mod.M nocache.M pid.M probe.M purge.M translate.M \
	  valid.M utlb_miss.M tlbtest.M

OBJS= busy.M chg_volt.M duart_lpbk.M eisa_reg.M hpc3_fifo.M hpc3_reg.M int2_mask.M msg_printf.M nvrm_addr.M nvrm_data.M nvrm_id.M nvrm_pins.M passwd_jumper.M plp_lpbk.M plp_mwrite.M plp_write.M rt_clock.M scsi.M scsi_diag.M scsi_xcise.M status.M timer.M wd95a_regs.M

TEST_OBJS= $(CACHE_OBJS) $(MEM_OBJS) $(TLB_OBJS) $(OBJS)

# dirt to clean up ...
LDIRT= $(LLDIRT); \
if [ -d $(TARGETDIR) ]; then cd $(TARGETDIR); rm -f $(OBJECTS); fi
LLDIRT= ${GEN_CFILES} 

# default rule
default: $(MODULE)

# rule for actual module binary
${MODULE}: $(OBJECTS) $(COMMON_OBJS)
	${NEWVERS} ${MODULE} ${RELEASE} "${CPUBOARD} IDE $(@)" > $(@:=_version.c)
	${CCF} -c $(@:=_version.c) -o $(TARGETDIR)/$(@:=_version.o)
	rm -f $(@:=_version.c)
	cd $(TARGETDIR); $(LD) ${LDRELOC} -no_unresolved -wall -o $(@) ${LDOPTS} ${OBJECTS} $(COMMON_OBJS) $(@:=_version.o) $(TEST_OBJS) > $(@).map


# rule for making command .c file
${MODULE}.c: ${MODULE_CMDS}
	nawk -F'	' -f ../../builtins.awk TBL=diagcmds \
		${MODULE_CMDS} > C$$$$.o && mv C$$$$.o $@

# rule for making script .c file
${MODULE_SCRIPT}_code.c: ${MODULE_SCRIPT}
	sed 's/"/\\"/g' $? | sed '/^#/d' | awk -f module_startup.awk > $@

# make debug
print:
	@ echo OBJECTS $(OBJECTS)


# more standard rules
# include $(DIAGDEPTH)/Makerules

# more common rules
include $(DIAGDEPTH)/../commonrules
