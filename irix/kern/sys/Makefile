#!smake
#
# Makefile for header file installation.
#
# Note - this directory should be used for kernel<->user interface headers
# only! Kernel internal and kernel extension headers should be in ksys.
#
#ident "$Revision: 1.293 $"

DEPTH?=..
include ${DEPTH}/kcommondefs
KPATH=$(TOP)/sys
.PATH:$(KPATH)

#
# List of subdirectories to run makes in.
#
HEADERS_SUBDIRS=EVEREST RACER PCI GIO xtalk SN arcs vme snet

#
# List of "Public" header files.  These are headers included by user
# code or used by device driver/VFS/Stream module writers
#

PUBLIC_HEADERS= acct.h acl.h alp.h attributes.h \
	bsd_types.h \
	cachectl.h capability.h cellkernstats.h cdefs.h cdl.h \
	cpumask.h cred.h crime.h crtctl.h \
	dbacc.h ddi.h dir.h dirent.h dkio.h dksc.h dlpi.h dmi.h dsreq.h \
	eag.h ecpplpreg.h ei.h eisa.h elog.h endian.h \
	errno.h euc.h eucioctl.h extacct.h \
	fcntl.h file.h filio.h flock.h fpregdef.h fpu.h fsctl.h fsid.h fstyp.h \
	frs.h grio.h giobus.h handle.h if_mtr.h \
	inf_label.h inst.h invent.h ioccom.h ioerror.h ioctl.h iotlb.h ipc.h \
	isdn_irix.h \
	mac.h mace.h mac_label.h major.h mload.h mman.h mode.h mount.h \
	kbdm.h ksync.h ksynch.h kthread.h ktserialio.h kusharena.h \
	lio.h lock.h log.h lpage.h lvtab.h \
	mace.h mac_label.h major.h mload.h mman.h mode.h \
	mount.h mpconf.h msg.h mtio.h \
	netconfig.h ns16550.h \
	open.h page.h param.h pci_intf.h plp.h pmo.h poll.h pwioctl.h prctl.h \
	prio.h procset.h profil.h pthread.h ptimers.h ptrace.h quota.h \
	ram.h regdef.h resource.h rtmon.h \
	sad.h sat.h sat_compat.h schedctl.h select.h sem.h \
	serialio.h sg.h sgigsc.h \
	shm.h siginfo.h signal.h sigevent.h slotnum.h smfd.h so_dac.h \
	sockio.h sockmod.h softfp.h \
	stat.h statfs.h statvfs.h stdnames.h stermio.h stream.h strids.h \
	strlog.h strmdep.h strmp.h stropts.h strstat.h strtty.h swap.h \
	syscall.h sysget.h sysinfo.h \
	syslog.h sysmips.h sysmp.h syssgi.h systeminfo.h \
	threadcfg.h \
	termio.h termios.h \
	ticlts.h ticots.h ticotsord.h \
	tihdr.h time.h timeb.h times.h timespec.h timod.h tiuser.h \
	ttydev.h ttold.h types.h timers.h \
	uadmin.h ucontext.h uio.h uli.h ulimit.h umfs.h usioctl.h unistd.h usrbus.h ustat.h \
	utime.h utsname.h utssys.h uuid.h vde_if.h vmereg.h wait.h wr_eventP.h \
	xbmon.h xlv_ioctl.h xti.h z8530.h wr_eventP.h \
	ckpt.h clksupport.h disksar.h dp8573clk.h ds1216clk.h ds1286.h ds17287.h hal2.h \
	i8254clock.h iograph.h loaddrs.h numa.h pckm.h rad.h rmap.h \
	tpucom.h tpudrv.h tpureg.h tpuerrno.h gsnreg.h \
	scsi_stats.h stty_ld.h systm.h xlate.h

#
# List of headers that VFS/streams/device driver writers need
#
KMOD_HEADERS=alenlist.h asm.h buf.h conf.h edt.h groupintr.h \
	fs_subr.h graph.h hwgraph.h kmem.h kucontext.h map.h mkdev.h \
	nodemask.h par.h pathname.h pio.h pvnode.h repl_vnode.h sema.h tile.h \
	vfs.h vnode.h mon.h unc.h uncintf.h \

#
# List of headers that only system software utilities need
# These are public within SGI for internal development of other ISM's
# but are not shipped to customers.
#
FRIEND_HEADERS= beast.h clock.h \
	dmi_kern.h dump.h \
	ecc.h \
	hlock.h hwperfmacros.h \
	idbgactlog.h idbgentry.h imon.h i8251uart.h \
	ISPcode2.17.h \
	kernnetls.h kernflexlm.h kfcntl.h ktrace.h \
	ds1687clk.h nic.h \
	parity.h partition.h prf.h psw.h \
	snif.h splock.h \
	tserialio_pvt.h \
	tsd.h vdma.h vsocket.h \
	watch.h \
	pbus.h mpzduart.h usrdma.h \
	uart16550.h traplog.h ad1843.h\
	xlv_tab.h xlv_attr.h xlv_labd.h xlv_stat.h xlv_vh.h \
	psema_cntl.h usync.h mmci.h sema_private.h vnode_private.h

#
# List of headers that are really private to the kernel but are needed
# to compile master.c.  This unfortunately means they must be shipped
# with all systems as if they were public
# The PUBLIC_HEADERS and KMOD_HEADERS are also required
#

MASTERC_HEADERS= arsess.h atomic_ops.h avl.h callo.h calloinfo.h cmn_err.h cpu.h \
	debug.h dmamap.h dnlc.h driver.h dvh.h fault.h \
	hwperftypes.h idbg.h immu.h iobuf.h iobus.h \
	kabi.h kopt.h ksa.h ksignal.h kstat.h \
	ktime.h ktypes.h \
	map.h mapped_kernel.h mips_addrspace.h \
	pcb.h pda.h pfdat.h proc.h  uthread.h reg.h sbd.h \
	scsi.h fcal.h failover.h wd93.h wd95a.h wd95a_struct.h adp78.h scip.h fcadp.h \
	strsubr.h sysmacros.h \
	R10k.h tirdwr.h tuneable.h tpsc.h tfp.h \
	var.h xl.h xlv_base.h xlv_lock.h \
	IP20.h IP22.h IP26.h \
	IP32.h IP32flash.h IPMHSIM.h IPMHSIMaddrs.h MHSIM.h \
	IP20addrs.h IP22addrs.h IP26addrs.h IP28addrs.h \
	IP20nvram.h IP22nvram.h \
	mc.h hpc1.h hpc3.h hpc3plpreg.h ql.h ql_firmware.h strtbl.h \
	shareIIstubs.h


#
# Any remaining headers not listed in one of the above lists is
# "Private" meaning that it is never used outside of the "irix"
# ISM.  They are not installed in any public include directory.
#

include ${DEPTH}/kcommonrules

$(KCOMMONPREF)default $(KCOMMONPREF)install:

headers!
	${INSTALL} -m 444 -F /usr/include/sys ${PUBLIC_HEADERS} \
		${KMOD_HEADERS} ${MASTERC_HEADERS}
	${INSTALL} -idb "noship" -m 444 \
		-F /usr/include/sys ${FRIEND_HEADERS}
	${HEADERS_SUBDIRS_MAKERULE}
