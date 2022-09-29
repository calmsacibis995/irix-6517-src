/**************************************************************************
 *									  *
 *	 	Copyright (C) 1986-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded	instructions, statements, and computer programs	 contain  *
 *  unpublished	 proprietary  information of Silicon Graphics, Inc., and  *
 *  are	protected by Federal copyright law.  They  may	not be disclosed  *
 *  to	third  parties	or copied or duplicated	in any form, in	whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident	"$Revision: 3.1227 $"

#include <bstring.h>
#include <ctype.h>
#include <limits.h>
#include <sys/types.h>
#include <ksys/as.h>
#include <sys/reg.h>
#include <sys/systm.h>
#include <sys/param.h>
#include <ksys/vpgrp.h>
#include <ksys/vproc.h>
#include <os/proc/vpgrp_private.h>
#include <os/proc/vproc_private.h>
#include <sys/sbd.h>
#include <ksys/xthread.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/sysmacros.h>
#include <sys/pcb.h>
#include <sys/signal.h>
#include <sys/flock.h>
#include <sys/sema.h>
#include <sys/sema_private.h>
#include <sys/swap.h>
#include <sys/proc.h>
#include "os/proc/pproc_private.h"
#include <ksys/vsession.h>
#include <sys/arsess.h>
#include <ksys/vpag.h>
#include "os/pagg/ppagg_private.h"
#include <ksys/exception.h>
#include <sys/sysinfo.h>
#include <sys/map.h>
#include <ksys/vfile.h>
#include <ksys/fdt_private.h>
#include <sys/vfs.h>
#include <sys/vnode_private.h>
#include <sys/poll.h>
#include <sys/quota.h>
#include <sys/cmn_err.h>
#include <ksys/cred.h>
#include <sys/debug.h>
#include <sys/pda.h>
#include <sys/splock.h>
#include <sys/conf.h>
#include <sys/lock.h>
#include <sys/callo.h>
#include <sys/calloinfo.h>
#include <sys/var.h>
#include <sys/edt.h>
#include <sys/runq.h>
#include <sys/rt.h>
#include <sys/q.h>
#include <sys/idbg.h>
#include <sys/idbgentry.h>
#include <sys/inst.h>
#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/nodepda.h>
#include <sys/kmem.h>
#include <sys/sysmp.h>
#include <sys/syssgi.h>
#include <sys/cachectl.h>
#include <sys/dirent.h>
#include <sys/iobuf.h>
#include <sys/scsi.h>
#include <sys/iograph.h>
#include <sys/dvh.h>
#include <sys/dkio.h>
#include <sys/dksc.h>
#include <sys/failover.h>
#include <ksys/behavior.h>
#include <ksys/kern_heap.h>
#include <elf.h>		/* must be included before efs.h */
#include <sys/fs/efs.h>
#include <sys/fs/efs_inode.h>
#include <fs/specfs/spec_atnode.h>
#include <fs/specfs/spec_csnode.h>
#include <fs/specfs/spec_lsnode.h>
#include <procfs/prdata.h>
#include <procfs/prsystm.h>
#include <sys/dnlc.h>
#include <net/if.h>
#include <sys/strsubr.h>
#include <sys/strstat.h>
#include <sys/strmp.h>

#include <sys/getpages.h>
#include <sys/page.h>
#include <sys/prctl.h>
#include <sys/scsi.h>
#include <sys/ioerror.h>
#if IP20 || IP22 || IP26 || IP28 || EVEREST
#define IDBG_WD93	1
#include <sys/wd93.h>
#endif
#include <sys/schedctl.h>

#ifdef SPLMETER
#include <sys/splmeter.h>
#include <sys/invent.h>
#endif
#include <sys/watch.h>
#include <fifofs/fifonode.h>
#include <namefs/namenode.h>
#include <sys/arcs/debug_block.h>
#include <sys/arcs/spb.h>
#include <sys/eag.h>
#include <sys/mac_label.h>
#include <sys/sat.h>
#include <sym.h>
#include <sys/statvfs.h>
#include <sys/mload.h>
#include <sys/mloadpvt.h>
#include <sys/ktrace.h>
#include <ksys/sthread.h>
#include <ksys/isthread.h>
#if EVEREST
#include <sys/scip.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/evintr_struct.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/EVEREST/everror.h>
#include <sys/EVEREST/mc3.h>
#endif /* EVEREST */
#if defined(DEBUG) && defined(ACTLOG)
#include <sys/idbgactlog.h>
#endif /* DEBUG && ACTLOG */
#include <string.h>
#include <sys/atomic_ops.h>
#include <sys/kabi.h>
#include <sys/uuid.h>
#include <ksys/pid.h>
#include <ksys/childlist.h>
#include <sys/traplog.h>
#ifdef ULI
#include <ksys/uli.h>
#endif
#if defined(CELL)
#include <ksys/cell.h>
#include <fs/cfs/dvn.h>
#endif
#include <sys/rtmon.h>
#include <os/tstamp.h>

#include <sys/nodepda.h>

#ifdef	NUMA_REPLICATION
#include <sys/repl_vnode.h>
#endif	/* NUMA_REPLICATION */

#include <sys/numa.h>

#ifdef R10000
#include <sys/hwperftypes.h>
#include <sys/hwperfmacros.h>
#endif 

#include <ksys/vm_pool.h>

#ifdef DEBUG
	extern struct vfile *activefiles;
#endif

#include <sys/space.h>

#ifdef MCCHIP
#include <ksys/cacheops.h>
#endif
# define LONG_LONG long long

extern void dprintf(char *, ...);
extern char *dnlc_v2name(struct vnode *vp);
/*
 * Internal debug routines.  These routines must be invoked by the debug
 * monitor in response to table	dump commands.	Each routine must be
 * re-entrant, but is called with the same C environment (though not
 * on a	kernel stack) that the kernel uses.
 */
/*
 * Addendum: The above is true when invoked from the debugger; these routines
 *		can also be invoked by a user, causing output directly on the
 *		console, and running in	a working kernel environment.
 */

static char	*islock(void *);
static void	sleepon(kthread_t *);
char	*re_comp(const char *);
int	re_exec(const char *);
static void	glob2regex(char *, const char *);
static void	strtoargs(char *buf, int *ac, char **ap);
#define SZOFPSINT_HEX	sizeof(__psint_t)/4  /* Num of chars in __psint_t */
char	*padnum(__psint_t, int, int);
char	*padnum64(__int64_t, int, int);
static char	*pad_utrace_qual(__uint64_t);
static void 	idbg_dumphisto(k_histelem_t *, char *, int);
__psunsigned_t	fetch_kaddr(char *s);

void	idbg_help();
void	idbg_dstats(uint64_t);
void	idbg_proc(__psint_t);
void	idbg_uthread(__psint_t);
void	idbg_fbuf(__psint_t);
void	idbg_shdump(__psint_t);
void	idbg_semdump(sema_t *);
void	idbg_semctl(__psint_t, char *);
void	idbg_chklock(void *);
void	idbg_lockmeter(k_splockmeter_t *);
void	idbg_avlnode(avlnode_t *);
void	idbg_pda(__psint_t);
void	idbg_upage(proc_t *);
void	idbg_fdt(fdt_t *);
void	idbg_uarea(__psint_t);
void	idbg_runq(__psint_t);
void	idbg_eframe(__psint_t);
void	idbg_semchain(__psint_t);
void	idbg_ubtrace(__psint_t *);
void	idbg_etrace(__psint_t *);
void	idbg_dnlc(__psint_t);
void	idbg_ncache(__psint_t);
vnode_t	*idbg_vnode(__psint_t);
void 	idbg_vnbhv(__psint_t);
void 	idbg_bhv(__psint_t);
void	idbg_retarget(void *p, void *a2, int argc, char **argv);
void 	idbg_bhvh(__psint_t);
void	idbg_extents(__psint_t);
void	idbg_fifolist(__psint_t);
void	idbg_namelist(__psint_t);
void	idbg_vlist(__psint_t);
void 	idbg_blktrace(daddr_t blk);
void 	idbg_vbuftrace(vnode_t *vp);
void	idbg_page_counter(__psint_t);
void	idbg_page_counters_set(void);
void	idbg_pollhead(struct pollhead *);
void	idbg_polldat(struct polldat *);
void	idbg_pchain(__psint_t);
void	idbg_pgrp(__psint_t);
void	idbg_plist(__psint_t);
void	idbg_plfdt(__psint_t);
void	idbg_proxy(proc_proxy_t *);
void	idbg_ptree(__psint_t);
void	idbg_nschain(char *);
void	idbg_slpproc(__psint_t);
void	idbg_minfo(int);
void	idbg_printputbuf(__psint_t);
void	idbg_symtab(struct dbstbl **, char **);
void	idbg_symsize(int *);
void	idbg_hotlocks(int, int);
void	idbg_coldlocks(int);
void	idbg_map();
#if SN
void	idbg_patch_instruction(__int32_t *, __psint_t, int, char **);
#endif
void	idbg_bitmap(__psint_t);
void	idbg_chbtmp(__psint_t);
void	idbg_semalist(__psint_t);
void	idbg_shaddr(__psint_t);
void	idbg_string();
void	idbg_hd(unsigned *, __psint_t);
void	idbg_file(__psint_t);
void	idbg_vfile(__psint_t);
void	idbg_filock(__psint_t);
void	idbg_flock(__psint_t);
void	idbg_sleeplcks(__psint_t);
void	idbg_vfs(__psint_t);
void	idbg_vnmap(__psunsigned_t);
void	idbg_callo(__psint_t);
void	idbg_calloinfo(__psint_t);
void    idbg_ptimer(__psint_t);
void	idbg_softfp(__psint_t);
void	idbg_sysinfo(int);
void	idbg_kill(__psint_t);
void	idbg_nfs(struct vfs *);
void	idbg_exportfs(__psint_t);
void	idbg_efs(struct efs *);
void	idbg_tracemasks(struct pr_tracemasks *);
void	idbg_qbuf(__psint_t);
void	idbg_vbuf(__psint_t);
void	idbg_unstablebuf(void);
void	idbg_kaddr(__psint_t, char *);

#ifdef SPLMETER
void	idbg_splmeter();
#endif

void	idbg_prilist();
void	idbg_watch(__psint_t);
void	idbg_quota(__psint_t);
void	idbg_dqb();
void	idbg_pwait(__psint_t);
void	idbg_mrlock(mrlock_t *);
void	idbg_mrlockfull(mrlock_t *);
void	idbg_mslock(mslock_t *);
#ifdef SEMAMETER
extern void idbg_mrlocks(__psint_t);
#endif
void	idbg_call(__psint_t);
void	idbg_zone(__psint_t);
void	idbg_pglst(__psint_t);
void	idbg_swap(__psint_t);
void	idbg_panic(__psint_t);
void	idbg_nodepda(__psint_t);
void	idbg_setpri(__psint_t, char *);

void	idbg_utrace_find(__psint_t,__psint_t);
void	idbg_utrace_mask(uint64_t);
void	idbg_signal(__psint_t);
void	idbg_ptrace(__psint_t);
void	idbg_mask();
#ifdef IDBG_WD93
void	idbg_wd93();
#endif
void	idbg_scsi(struct scsi_request *);
void	idbg_dksoftc(struct dksoftc *);
void	idbg_mutexdump(mutex_t *);
void	idbg_svdump(sv_t *);
void	idbg_bmapval(struct bmapval *);
void	idbg_doadump(void);
void	idbg_uio(uio_t *);
void	idbg_findbuf(int);
void	idbg_pagg(__psint_t);
void	idbg_pagtree(__psint_t);
void	idbg_paglist(__psint_t);
void    idbg_pagfind(__psint_t);
void    idbg_klist(__psint_t);
void    idbg_slist(__psint_t);
void    idbg_xlist(__psint_t);
void	idbg_kthread(__psint_t);
void	idbg_sthread(__psint_t);
void	idbg_tstamp(const tstamp_obj_t*);
void	idbg_xthread(__psint_t);

struct	opena;
void	idbg_open(struct opena *);
struct	creata;
void	idbg_creat(struct creata *);
struct	mkdira;
void	idbg_mkdir(struct mkdira *);
struct	linka;
void	idbg_link(struct linka *);
struct	renamea;
void	idbg_rename(struct renamea *);
struct	symlinka;
void	idbg_symlink(struct symlinka *);
struct	unlinka;
void	idbg_unlink(struct unlinka *);
struct	rmdira;
void	idbg_rmdir(struct rmdira *);
struct	xstatarg;
void	idbg_xstat(struct xstatarg *);

#ifdef DEBUG_BUFTRACE
void	idbg_abuftrace(int);
void	idbg_sbuftrace(buf_t *);
void	idbg_buftrace(buf_t *);
void	idbg_bufcpu(__psint_t);
#endif
void	idbg_dumpsym(char **);

#ifdef VNODE_TRACING
void	idbg_vntrace(vnode_t *);
#endif
#ifdef _STRQ_TRACING
void	idbg_qtrace(queue_t *);
#endif

void	idbg_fifo(__psint_t);
void	idbg_prfifo(struct fifonode *, int);
void	idbg_prnamenode(struct namenode *, int);
void	idbg_prinode(struct inode *, vtype_t, int);
void	idbg_prnode(__psint_t);
void	idbg_prnodefree(__psint_t);
void	idbg_prrnode(bhv_desc_t *, vtype_t, int);

extern	void idbg_pratnode(struct atnode *);
extern	void idbg_prcsnode(struct csnode *);
extern	void idbg_prlsnode(struct lsnode *);

static void	do_efs(register struct efs *, int);

#if !IPMHSIM && !IP32
void	dkstatpr(void);
#endif

void	idbg_pty(__psint_t);
void	idbg_dport(__psint_t);
void	idbg_dportrset(__psint_t);
#if DEBUG
void	idbg_dportact(__psint_t);
#endif /* DEBUG */

#if defined(SIO_DUMMY) && defined(DEBUG)
void	idbg_dummyportact(__psint_t);
#endif

#if SN || MULTIKERNEL

void	idbg_partition(void);
void	idbg_xpr(void *);
void	idbg_xp(void);
void	idbg_xpm(__psint_t x);

#endif /* SN || MULTIKERNEL */

#if SN0
void	idbg_stop(void *);
void	idbg_hwstate(void);
void	idbg_nmidump(int);
#if defined(DEBUG)
void	idbg_intrmap(__psint_t);
#endif
#endif


#if IP30
void	idbg_stop(void *);
#endif

#if SN0 && DEBUG && !defined(SABLE)
extern void	idbg_prconbuf(void *);
#ifndef IOC3_PIO_MODE
extern void	idbg_ioc3dump(void *);
#endif
#endif

#if ULI && DEBUG
void	idbg_uliact(int);
void	idbg_curuli(int);
#endif

#if I2CUART_DEBUG
void	idbg_i2cuart(__psint_t, __psint_t);
#endif

#if EVEREST
void	idbg_splcnt(__psint_t, __psint_t, __psint_t);
void	idbg_scip(__psint_t);
void	idbg_stop(void *);
void	idbg_nmidump(int);
void	dump_hwstate(__psint_t);
void	idbg_memecc(void);
void	idbg_decode_addr(caddr_t);
#if DEBUG
void	idbg_dportact(__psint_t);
#ifdef ULI
void	idbg_uliact(int);
void	idbg_curuli(int);
#endif
#endif /* DEBUG */
#ifdef I2CUART_DEBUG
void	idbg_i2cuart(__psint_t, __psint_t);
#endif
#endif /* EVEREST */

/*
 * These funcs are used to print interrupt table info.
 */
#if MCCHIP || EVEREST || IP30
void	idbg_intvec(__psint_t);
#endif /* MCCHIP || EVEREST */
void	intvec_tinfo(thd_int_t *);

#if defined(DEBUG) || defined(DEBUG_CONPOLL)
void	idbg_conpoll(int);
#endif /* DEBUG || DEBUG_CONPOLL */

#if TRAPLOG_DEBUG && (_PAGESZ == 16384)
void	idbg_traplog(int argc,char **argv);
void dumptraplog(cpuid_t);
#endif /* TRAPLOG_DEBUG */

#if defined(DEBUG) && defined(ACTLOG)
void	idbg_actlog();
void	idbg_actlogcpu();
void	idbg_actlogany();
#endif /* DEBUG && ACTLOG */
void	idbg_tlbsync(void);

extern int idbgmaxfuncs;
extern int kheap_initialized;

#define	IDBGMAXSTATFUNCS	400

static dbgoff_t *idbglaststatic;
static dbgoff_t	idbgtab[IDBGMAXSTATFUNCS];
static lock_t idbg_addlock;

static dbglist_t dbglist;
static int n_dbglist;

static int print_filocks = 0;

/* function prototypes */
static int convsp_start(struct uthread_s *ut);
static void convsp_end(void);
static unsigned *convsp(unsigned *, unsigned *);
static void addrinit(void);
static void leafinit(void);
static void print_maclabel(char *, mac_label *);
static void tinyproc(proc_t *);
static void  dumpssleep(ssleep_t *);
static void idbg_priscan(prilist_t *);
static void dumppdabstats(pda_t *);
static void dumppdaistats(pda_t *);
void   prvfs(struct vfs *, int);
static void  dump_callo(struct callout *);
static void scanchain(int);
void _prsymoff(void *, char *, char *);
char *_padstr(char *, int);
char *_padstrn(char *, int, int);
static void bufstuff(void);
void do_pdas(void (*)(pda_t *), int);
void dumpRunq(__psint_t);
void dumppda(pda_t *);
static void dumpminfostats(register pda_t *);
static void dumpsysinfostats(register pda_t *);
void dumphotlock(int, int);
void idbgplog(semahist_t *);
int ROUNDPC(void *, int);
#ifdef IDBG_WD93
void wd93_debug_chip(wd93dev_t *);
void wd93_debug_pflags(int, char **);
#endif
#define KID_IS_UTHREAD 1
#define KID_IS_STHREAD 2
#define KID_IS_ITHREAD 3
#define KID_IS_XTHREAD 3
#define KID_IS_UNKNOWN -1
static void printid(kthread_t *, uint64_t, char *);
static int find_kthreadid(uint64_t id, int verbose, __psunsigned_t *);
static void showpagtree(proc_t *, proc_t *, int);
static void showptree(proc_t *, int);
static void shortvpag(vpagg_t *);
#if R10000 && (EVEREST || IP28 || IP30 || IP32 || SN)
static void idbg_cache_errors(void);
#endif
#if defined(SN0) || defined(IP30)
static void idbg_ioerror(__psunsigned_t);
#endif 
#if R10000
void	idbg_print_cpumon(cpu_mon_t *);
void	idbg_cpumon(__psint_t);
#endif /* R10000 */
#ifdef ITHREAD_LATENCY
void	idbg_print_latstats(__psint_t);
void	idbg_zero_latstats(__psint_t);
#endif /* ITHREAD_LATENCY */


int	ec0_up(void), ec0_down(void), getrbase(void);

char *lookup_kname(register void *, register void *);

/*
 * record state of who (symmon/user) invoked the idbg function so we
 * can route info to the appr. place
 * NOTE: this allows only one user at a time
 */
#define	IDBGSYMMON	1
#define	IDBGUSER	2
int idbginvoke = IDBGSYMMON;
static caddr_t idbgoutbuf = NULL;/* pointer to output buffer/single threader */
static caddr_t idbginbuf = NULL;
static caddr_t idbguaddr;	/* address into user's space */
static label_t idbgjmp;

char *noupage = "<NO_UPAGE>";

extern struct dbstbl dbstab[];

extern vfsops_t spec_vfsops;

extern vfsops_t efs_vfsops, prvfsops, *afs_vfsopsp, xfs_vfsops;
extern vnodeops_t *nfs3_getvnodeops(void);

extern char nametab[];
extern int symmax;

extern struct reg_desc sr_desc[], cause_desc[], cache_err_desc[];
extern struct reg_desc s_taglo_desc[], p_taglo_desc[];
#if R4000 && R10000
extern struct reg_desc r10k_sr_desc[], r10k_s_taglo_desc[];
#endif /* R4000 && R10000 */

void  _idbg_setup(void);
int  _idbg_copytab(caddr_t *, int, rval_t *);
void _idbg_tablesize(rval_t *);
int  _idbg_dofunc(struct idbgcomm *, rval_t *);
int  _idbg_error(void);
void _idbg_switch(int, int);
int  _idbg_addfunc(char *, void (*)());
void _idbg_delfunc(void (*)());
void _qprintf(char *, va_list);
void _printflags(uint64_t, char **, char *);
void __prvn(vnode_t *, int);
void _idbg_wd93(__psint_t);
void _idbg_late_setup(void);
int  _idbg_unload(void);
void _idbg_addfssw(idbgfssw_t *);
void _idbg_delfssw(idbgfssw_t *);
extern char *kjstate_to_string(int);
extern int (*idbg_prafsnodep)(bhv_desc_t *);
extern int (*idbg_afsvfslistp)(void);

idbgfunc_t idbgdefault = {/*no stubs*/0, _idbg_setup,
	_idbg_copytab, _idbg_tablesize, _idbg_dofunc, _idbg_error,
	_idbg_switch, _idbg_addfunc, _idbg_delfunc, _qprintf, __prvn,
#ifdef IDBG_WD93
	_idbg_wd93, 
#else
	0,
#endif
	_idbg_late_setup, _idbg_unload, _idbg_addfssw, _idbg_delfssw,
	_printflags, _padstr, idbg_mrlock, _prsymoff};

idbgfssw_t *idbgfssws;
unsigned *sp_kvaddr;
#if EXTKSTKSIZE == 1
/* kvaddr for kernel stack extension page */
unsigned *extsp_kvaddr;
#endif

#define VD	(void (*)())

/*
 * This structure defines the "built-in" functions that enabled in
 * this file.
 */
struct idbgcommfunc {
	char	*comm_name;
	void	(*comm_func)();
	int	comm_argstype;
	char	*comm_descrip;
};

#define	INTEGER_ARGS	0		/* Standard (int) args */
#define STRING_ARGS	1		/* String (char *) args */

/*
 * Older versions of symmon only allow 100 entry points.  This list
 * contains the "most popular" functions.
 */
struct idbgcommfunc idbgBasicBuiltIn[] = {
    "??",	VD idbg_help,
    	INTEGER_ARGS,	"Help (print this list)",
    "avlnode",	VD idbg_avlnode,
    	INTEGER_ARGS,	"Print avlnode structure",
    "bitmap",	VD idbg_bitmap,
    	INTEGER_ARGS,	"Print bit map structure",
    "chbtmp",	VD idbg_chbtmp,
    	INTEGER_ARGS,	"Check bit map structure",
    "buf",	VD idbg_fbuf,
    	INTEGER_ARGS,	"Print data buffers",
#if R10000 && (EVEREST || IP28 || IP30 || IP32 || SN)
    "cache",	VD idbg_cache_errors,
        INTEGER_ARGS,	"Dump logged cache errors",
#endif
    "call",	VD 0,
    	INTEGER_ARGS,	"Call arbitrary function",
    "callo",	VD idbg_callo,
    	INTEGER_ARGS,	"Print callout table",
    "cinfo",	VD idbg_calloinfo,
    	INTEGER_ARGS,	"Print calloutinfo table",
#if !SABLE && !IPMHSIM && !IP32
    "dkstatpr",	VD dkstatpr,
    	INTEGER_ARGS,	"Print dksc block traces",
#endif
    "dksoftc",	VD idbg_dksoftc,
    	INTEGER_ARGS,	"Print dksc struct",
    "dnlc",	VD idbg_dnlc,
    	INTEGER_ARGS,	"Dump/search dnlc entries",
    "dport",	VD idbg_dport,
    	INTEGER_ARGS,	"Print data about duart port",
#ifdef DEBUG
    "dportact",	VD idbg_dportact,
    	INTEGER_ARGS,	"Print data about duart port activity",
#endif
#ifdef I2CUART_DEBUG
    "i2cuart",	VD idbg_i2cuart,
	INTEGER_ARGS,	"get/put data to simulated i2c uart",
#endif
    "dportrset",VD idbg_dportrset,
    	INTEGER_ARGS,	"Reset duart port counters",
    "dquotab",	VD idbg_dqb,
    	INTEGER_ARGS,	"Print quota entries",
    "dsym",	VD idbg_dumpsym,
    	INTEGER_ARGS,	"Dump memory with symbols",
    "ebt",	VD idbg_etrace,
    	INTEGER_ARGS,	"Exception frame backtrace",
    "ec0_down",	VD ec0_down,
	INTEGER_ARGS, "Stop Ethernet Interface (ec0/et0)",
    "ec0_up",	VD ec0_up,
	INTEGER_ARGS, "Start Ethernet Interface  (ec0/et0)",
    "getrbase",	VD getrbase, 
	INTEGER_ARGS, "Get Receive Base for Ethernet Interface (ec0/et0)",
    "eframe",	VD idbg_eframe,
    	INTEGER_ARGS,	"Exception frame dump",
    "efs",	VD idbg_efs,
    	INTEGER_ARGS,	"Print efs structures",
    "extents",	VD idbg_extents,
    	INTEGER_ARGS,	"Efs vnode extents",
    "fdt",    VD idbg_fdt,
	INTEGER_ARGS,   "Print file descriptor table",
    "fifo",	VD idbg_fifo,
    	INTEGER_ARGS,	"Print an specified fifonode entry",
    "fifolist",	VD idbg_fifolist,
    	INTEGER_ARGS,	"Print allocated fifonode entries",
    "file",	VD idbg_file,
    	INTEGER_ARGS,	"Print open files and file table",
    "filock",	VD idbg_filock,
    	INTEGER_ARGS,	"Print file locks",
    "findbuf",	VD idbg_findbuf,
    	INTEGER_ARGS,	"Print bufs matching given blkno",
    "flock",	VD idbg_flock,
    	INTEGER_ARGS,	"Print file lock data",
    "hd",	VD idbg_hd,
    	INTEGER_ARGS,	"Print a hex dump",
    "help",	VD idbg_help,
    	INTEGER_ARGS,	"Help (print this list)",
    "kill",	VD idbg_kill,
    	INTEGER_ARGS,	"Kill a process",
    "ktlbfix",	VD ktlbfix,
    	INTEGER_ARGS,	"Ktlbfix",
    "lockmeter",VD idbg_lockmeter,
    	INTEGER_ARGS,	"Dump a lockmeter",
    "lock",	VD idbg_chklock,
    	INTEGER_ARGS,	"Dump a lock",
    "map",	VD idbg_map,
    	INTEGER_ARGS,	"Print map structure",
    "minfo",    VD idbg_minfo,
	INTEGER_ARGS,   "Minfo numbers",
    "mrlock",	VD idbg_mrlock,
    	INTEGER_ARGS,	"Print the multi-reader lock",
    "mrlockfull",	VD idbg_mrlockfull,
    	INTEGER_ARGS,	"Print the multi-reader lock w/timing",
    "mslock",	VD idbg_mslock,
    	INTEGER_ARGS,	"Print the multi-reader/multi-writer switch",
    "mutex",	VD idbg_mutexdump,
    	INTEGER_ARGS,	"Dump a mutex",
    "namelist",	VD idbg_namelist,
    	INTEGER_ARGS,	"Print allocated namenode entries",
    "ncache",	VD idbg_ncache,
    	INTEGER_ARGS,	"dump dnlc namecache entry",
    "nodepda",	VD idbg_nodepda,
    	INTEGER_ARGS,	"Print nodepda info",
    "pb",	VD idbg_printputbuf,
    	INTEGER_ARGS,	"Print out putbuf",
    "pchain",	VD idbg_pchain,
    	INTEGER_ARGS,	"Parent child sibling scanner",
    "pctr",	VD idbg_page_counter,
    	INTEGER_ARGS,	"Print out page counter",
    "pctrset",	VD idbg_page_counters_set,
    	INTEGER_ARGS,	"Print out set page counter",
    "pda",	VD idbg_pda,
    	INTEGER_ARGS,	"Private area dumps",
    "pglst",	VD idbg_pglst,
    	INTEGER_ARGS,	"Print page list",
    "pgrp",	VD idbg_pgrp,
    	INTEGER_ARGS,	"Print process group",
    "pgwait",	VD idbg_pwait,
    	INTEGER_ARGS,	"Print page wait structure",
#if SN
    "pi",	VD idbg_patch_instruction,
    	INTEGER_ARGS,	"Patch instruction ",
#endif
    "plist",	VD idbg_plist,
    	INTEGER_ARGS,	"Process active/free list scanners",
    "plfdt",	VD idbg_plfdt,
    	INTEGER_ARGS,	"Process file descriptor table info",
    "ptree",	VD idbg_ptree,
        INTEGER_ARGS,	"Print tree of children for a process",
    "polldat",	VD idbg_polldat,
    	INTEGER_ARGS,	"Print polldat",
    "pollhead",	VD idbg_pollhead,
    	INTEGER_ARGS,	"Print pollhead",
    "prilist",	VD idbg_prilist,
    	INTEGER_ARGS,	"Dump the sorted priority table",
    "prnode",	VD idbg_prnode,
    	INTEGER_ARGS,	"Print /proc vnode info",
    "prnodefree",VD idbg_prnodefree,
	INTEGER_ARGS,   "Print prnode free list",
    "dstats", 	VD idbg_dstats,
    	INTEGER_ARGS,	"Dump debug stats",
    "proc", 	VD idbg_proc,
    	INTEGER_ARGS,	"Process entry parser",
    "proxy", 	VD idbg_proxy,
    	INTEGER_ARGS,	"proc-proxy parser",
    "ptimer",   VD idbg_ptimer,
        INTEGER_ARGS,   "dump Posix timer",
    "ptrace",	VD idbg_ptrace,
    	INTEGER_ARGS,	"Print debugger info",
    "pty",	VD idbg_pty,
    	INTEGER_ARGS,	"Print pty info",
    "quota",	VD idbg_quota,
    	INTEGER_ARGS,	"Print quota entries via mount table",
    "retarget",	VD idbg_retarget,
    	INTEGER_ARGS,	"Retarget xthreads/sthreads ",
    "runq",	VD idbg_runq,
    	INTEGER_ARGS,	"Run queue dump",
    "scsi",	VD idbg_scsi,
    	INTEGER_ARGS,	"Print scsi info",
    "sema",	VD idbg_semdump,
    	INTEGER_ARGS,	"Dump a semaphore",
    "shaddr",	VD idbg_shaddr,
    	INTEGER_ARGS,	"Print shared address blocks",
    "signal",	VD idbg_signal,
    	INTEGER_ARGS,	"Print proc signal info",
    "sleeplk",	VD idbg_sleeplcks,
    	INTEGER_ARGS,	"Print blocked file locks",
    "slpproc",	VD idbg_slpproc,
    	INTEGER_ARGS,	"Sleeping process list",
    "string",	VD idbg_string,
    	INTEGER_ARGS,	"Print a string",
    "sv",	VD idbg_svdump,
    	INTEGER_ARGS,	"Dump a sync variable",
    "swap",	VD idbg_swap,
    	INTEGER_ARGS,	"Print swap table",
    "symsize",	VD idbg_symsize,
    	INTEGER_ARGS,	"Dbgmon size of dbstab",
    "symtab",	VD idbg_symtab,
    	INTEGER_ARGS,	"Dbgmon address of dbstab/nametab",
    "sysinfo",	VD idbg_sysinfo,
    	INTEGER_ARGS,	"Print sysinfo struct",
    "tracemasks",	VD idbg_tracemasks,
    	INTEGER_ARGS,	"Print pr_tracemasks (procfs) struct",
    "tstamp",	VD idbg_tstamp,
	INTEGER_ARGS,	"Dump shared timestamp descriptor",
    "ubt",	VD idbg_ubtrace,
    	INTEGER_ARGS,	"User process backtrace",
    "uio",	VD idbg_uio,
    	INTEGER_ARGS,	"Print uio structure",
    "upage",    VD idbg_upage,
	INTEGER_ARGS,   "User area dump (page addr)",
    "user",	VD idbg_uarea,
    	INTEGER_ARGS,	"User area dump",
    "uthread", 	VD idbg_uthread,
    	INTEGER_ARGS,	"dump uthread",
    "utrace",	VD idbg_utrace,
	INTEGER_ARGS,	"Merge and dump timestamp trace entries",
    "utfind",	VD idbg_utrace_find,
	INTEGER_ARGS,	"Find utrace entry",
    "utmask",	VD idbg_utrace_mask,
	INTEGER_ARGS,	"Set utrace mask",
    "vbuf",	VD idbg_vbuf,
    	INTEGER_ARGS,	"Print chunk bucket headers",
    "vfile",	VD idbg_vfile,
    	INTEGER_ARGS,	"Print file table entries for given vnode",
    "vfs",	VD idbg_vfs,
    	INTEGER_ARGS,	"Print mount entries",
    "vlist",	VD idbg_vlist,
    	INTEGER_ARGS,	"Print mounted/free vnode entries",
    "vnode",	VD idbg_vnode,
    	INTEGER_ARGS,	"Dump vnode",
    "vnbhv",	VD idbg_vnbhv,
    	INTEGER_ARGS,	"Dump vnode behavior descriptors",
    "vnmap",	VD idbg_vnmap,
    	INTEGER_ARGS,	"Dump vnode address map structure",
    "bhvh",	VD idbg_bhvh,
    	INTEGER_ARGS,	"Dump behavior head and descriptors",
    "bhv",	VD idbg_bhv,
    	INTEGER_ARGS,	"Dump behavior descriptors",
#ifdef _STRQ_TRACING
    "qtrace",	VD idbg_qtrace,
    	INTEGER_ARGS,	"Queue tracing",
#endif
#ifdef VNODE_TRACING
    "vntrace",	VD idbg_vntrace,
    	INTEGER_ARGS,	"Vnode tracing",
#endif
    "watch",	VD idbg_watch,
    	INTEGER_ARGS,	"Dump watchpoint list",
#if IDBG_WD93
    "wd93",	VD idbg_wd93,
    	INTEGER_ARGS,	"Print wd93 info",
#endif
    "zone",	VD idbg_zone,
    	INTEGER_ARGS,	"Print zone struct(s)",
#if defined(SIO_DUMMY) && defined(DEBUG)
    "dummy",	VD idbg_dummyportact,
    	INTEGER_ARGS,	"Print data about dummy port activity",
#endif
#if defined(ULI) && defined(DEBUG)
    "uliact",	VD idbg_uliact,
    	INTEGER_ARGS,	"Print data about user level interrupt activity",
    "curuli",   VD idbg_curuli,
        INTEGER_ARGS,   "Dump current ULI",
#endif
#if TRAPLOG_DEBUG && (_PAGESZ == 16384)
    "traplog",   VD idbg_traplog,
        STRING_ARGS,   "Dump current CPU traplog",
#endif /* TRAPLOG_DEBUG */
    "tlbsync",	VD idbg_tlbsync,
    	INTEGER_ARGS,	"tlbsync debug info",
    0,		0,	0,			0
};

struct idbgcommfunc idbgExtendedBuiltIn[] = {
    "bmap",	VD idbg_bmapval,
    	INTEGER_ARGS,	"Print bmapval structure",
    "doadump",  VD idbg_doadump,
	INTEGER_ARGS,	"Do a kernel dump to swap space",
    "kaddr",	VD idbg_kaddr,
    	STRING_ARGS,	"Return the address of a kernel symbol",
    "slist",    VD idbg_slist,
        INTEGER_ARGS,   "Print list of sthreads",
    "ilist",    VD idbg_xlist,
        INTEGER_ARGS,   "Print list of ithreads",
    "ithread",    VD idbg_xthread,
        INTEGER_ARGS,   "Print ithread struct",
    "xlist",    VD idbg_xlist,
        INTEGER_ARGS,   "Print list of xthreads",
    "klist",    VD idbg_klist,
        INTEGER_ARGS,   "Map kthreadid to proc/sthread/ithread",
    "sthread",    VD idbg_sthread,
        INTEGER_ARGS,   "Print sthread struct",
    "xthread",    VD idbg_xthread,
        INTEGER_ARGS,   "Print xthread struct",
    "kthread",    VD idbg_kthread,
        INTEGER_ARGS,   "Print kthread struct",
    "setpri",	VD idbg_setpri,
    	STRING_ARGS,	"Adjust kthread priority",
    "softfp", 	VD idbg_softfp,
    	INTEGER_ARGS,	"Print fp exception info",
#if defined (R10000)
    "cpumon",	VD   idbg_cpumon,
        INTEGER_ARGS, "Show hwperf cpu monitor info",
#endif /* R10000 */
#ifdef ITHREAD_LATENCY
    "latency",    VD idbg_print_latstats,
        INTEGER_ARGS,   "Print preemption latency stats",
    "zerolatstats",    VD idbg_zero_latstats,
        INTEGER_ARGS,   "Reset preemption latency stats",
#endif
#if MCCHIP || EVEREST || IP30
    "intvec",    VD idbg_intvec,
        INTEGER_ARGS,   "Print interrupt vector tables",
#endif /* MCCHIP || EVEREST */
#if defined(DEBUG) || defined(DEBUG_CONPOLL)
    "conpoll",	 VD idbg_conpoll,
        INTEGER_ARGS,   "enable/disable dbg console poll from fastclock",
#endif /* DEBUG || DEBUG_CONPOLL */
    0,		0,	0,			0
};

struct idbgcommfunc idbgBufferBuiltIn[] = {
#ifdef DEBUG_BUFTRACE
    "abuftrace",VD idbg_abuftrace,
    	INTEGER_ARGS,	"Print global buffer trace info",
    "bufcpu",	VD idbg_bufcpu,
    	INTEGER_ARGS,	"Print buffer trace info for cpu",
    "buftrace",	VD idbg_buftrace,
    	INTEGER_ARGS,	"Print trace info for single buf",
    "sbuftrace",VD idbg_sbuftrace,
    	INTEGER_ARGS,	"Print single buf trace info / global",
    "vbuftrace",VD idbg_vbuftrace,
    	INTEGER_ARGS,	"Print vnode buf trace info / global",
    "blktrace",VD idbg_blktrace,
    	INTEGER_ARGS,	"Print blk trace info / global",
#endif
    "ubuf",VD idbg_unstablebuf,
	INTEGER_ARGS,	"Print B_NFS_UNSTABLE buffers",	
    0,		0,	0,			0
};


struct idbgcommfunc idbgSystemCallArgs[] = {
    "creat",	VD idbg_creat,
    	INTEGER_ARGS,	"Print creat call args",
    "link",	VD idbg_link,
    	INTEGER_ARGS,	"Print link call args",
    "mkdir",	VD idbg_mkdir,
    	INTEGER_ARGS,	"Print mkdir call args",
    "open",	VD idbg_open,
    	INTEGER_ARGS,	"Print open call args",
    "rename",	VD idbg_rename,
    	INTEGER_ARGS,	"Print rename call args",
    "rmdir",	VD idbg_rmdir,
    	INTEGER_ARGS,	"Print rmdir call args",
    "symlink",	VD idbg_symlink,
    	INTEGER_ARGS,	"Print symlink call args",
    "unlink",	VD idbg_unlink,
    	INTEGER_ARGS,	"Print unlink call args",
    "xstat",	VD idbg_xstat,
    	INTEGER_ARGS,	"Print xstat call args",
    0,		0,	0,			0
};

struct idbgcommfunc idbgSemaCtl[] = {
    "nschain",	VD idbg_nschain,
    	INTEGER_ARGS,	"Chain on semaphore name",
    "schain",	VD idbg_semchain,
    	INTEGER_ARGS,	"Dump a semaphore chain",
    "semctl",	VD idbg_semctl,
    	STRING_ARGS,	"Semaphore control",
    "semlist",	VD idbg_semalist,
    	INTEGER_ARGS,	"Dump semaphore list",
    "semlog",	VD idbg_shdump,
    	INTEGER_ARGS,	"Semaphore log",
    0,		0,	0,			0
};

struct idbgcommfunc idbgSpinCtl[] = {
    "hotlk",	VD idbg_hotlocks,
    	INTEGER_ARGS,	"Find hot locks",
    "coldlk",	VD idbg_coldlocks,
    	INTEGER_ARGS,	"Find cold locks",
    0,		0,	0,			0
};

struct idbgcommfunc idbgProcAgg[] = {
    "pagg",  VD idbg_pagg,
	INTEGER_ARGS,	"Print process aggregate info",
    "pagtree",	VD idbg_pagtree,
    	INTEGER_ARGS,	"Print list of processes in a process aggregate",
    "paglist",	VD idbg_paglist,
        INTEGER_ARGS,   "Process aggregate active list scanner",
    "pagfind",	VD idbg_pagfind,
	INTEGER_ARGS,	"Find all processes in a process aggregate",
    0,		0,	0,			0
};

#ifdef SN0
struct idbgcommfunc idbgSN0[] = {
    "stop",	VD idbg_stop,
    	INTEGER_ARGS,	"Stop other processors",
    "error_dump", VD idbg_hwstate,
        INTEGER_ARGS,   "Dump the error hardware state",
    "nmidump",	VD idbg_nmidump,
	INTEGER_ARGS,	"Adjust nmi behavior",
#if DEBUG
    "prconbuf",	VD idbg_prconbuf,
    	INTEGER_ARGS,	"print console log",
#ifndef IOC3_PIO_MODE
    "ioc3dump",	VD idbg_ioc3dump,
    	INTEGER_ARGS,	"print ioc3 info",
#endif
#endif

    "part", 	VD idbg_partition, 
        INTEGER_ARGS,	"print partition information",
    "xpr",	VD idbg_xpr,
        INTEGER_ARGS, 	"print xpc ring data",
    "xmap",	VD idbg_xp,
        INTEGER_ARGS,   "Print xpc partition map",
    "xpm", 	VD idbg_xpm,
        INTEGER_ARGS, 	"Print xpc message",

    "ioerror", VD idbg_ioerror, 
	INTEGER_ARGS,   "print ioerror struacture",
#if defined(DEBUG)
    "intrmap", VD idbg_intrmap,
        INTEGER_ARGS,   "print device - target cpu map",
#endif
    0,		0,	0,			0
};
#endif /* SN0 */

#ifdef EVEREST
struct idbgcommfunc idbgEverest[] = {
    "hwstate",	VD dump_hwstate,
    	INTEGER_ARGS,	"Dump error state",
    "mc3decode",VD idbg_decode_addr,
    	INTEGER_ARGS,	"Resolve phys addr to slot/leaf/bank",
    "memecc",	VD idbg_memecc,
    	INTEGER_ARGS,	"Dump memory ecc error state",
    "scip",	VD idbg_scip,
    	INTEGER_ARGS,	"Print scip chip info",
    "stop",	VD idbg_stop,
    	INTEGER_ARGS,	"Stop other processors",
    "nmidump",	VD idbg_nmidump,
	INTEGER_ARGS,	"Adjust nmi behavior",
#if MULTIKERNEL
    "part", 	VD idbg_partition, 
        INTEGER_ARGS,	"print partition information",
    "xpr",	VD idbg_xpr,
        INTEGER_ARGS, 	"print xpc ring data",
    "xmap",	VD idbg_xp,
        INTEGER_ARGS,   "Print xpc partition map",
    "xpm", 	VD idbg_xpm,
        INTEGER_ARGS, 	"Print xpc message",
#endif /* MULTIKERNEL */
    0,		0,	0,			0
};
#endif

#ifdef IP30
struct idbgcommfunc idbgRacer[] = {
    "stop",	VD idbg_stop,
	INTEGER_ARGS,	"Stop other processors",
    "ioerror",	VD idbg_ioerror, 
	INTEGER_ARGS,   "print ioerror structure",
    0,		0,	0,			0
};
#endif

#if defined(DEBUG) && defined(ACTLOG)
struct idbgcommfunc idbgActLog[] = {
    "actlog",	VD idbg_actlog,
    	INTEGER_ARGS,	"Print activity log data",
    "actlogcpu",VD idbg_actlogcpu,
    	INTEGER_ARGS,	"Limit activity logging to single cpu",
    "actlogany",VD idbg_actlogany,
    	INTEGER_ARGS,	"Log activity from any CPU",
    0,		0,	0,			0
};
#endif /* DEBUG && ACTLOG */

#ifdef SPLMETER
struct idbgcommfunc idbgSplmeter[] = {
    "spl",	VD idbg_splmeter,
    	INTEGER_ARGS,	"Dump the spl metering",
    0,		0,	0,			0
};
#endif

struct idbgcommtbl {
	struct idbgcommfunc	*tbl;
	char			*name;	
};

struct idbgcommtbl idbgCommTbl[] = {
	idbgBasicBuiltIn,	"Basic Built In Functions",
	idbgExtendedBuiltIn,	"Extended Built In Functions",
	idbgBufferBuiltIn,	"Buffer Functions",
	idbgSystemCallArgs,	"System Call Args",
	idbgSemaCtl,		"Semaphore Control Functions",
	idbgSpinCtl,		"Spinlock Control Functions",
	idbgProcAgg,		"Process Aggregate Functions",
#ifdef EVEREST
	idbgEverest,		"EVEREST",
#endif
#ifdef SN0
	idbgSN0,		"SN0",
#endif
#ifdef IP30
	idbgRacer,		"RACER",
#endif
#if defined(DEBUG) && defined(ACTLOG)
	idbgActLog,		"ACTLOG",
#endif
#ifdef SPLMETER
	idbgSplmeter,		"SPLMETER",
#endif
	0,			0
};

static char name_buf[256];
static char *np;

/*
 * idbg_setup - init idbg table
 */
void
_idbg_setup(void)
{
	dbgoff_t *dp;
	struct idbgcommtbl *tbl;
	struct idbgcommfunc *comm;

	dp = idbgtab;
	n_dbglist = 0;
	np = name_buf;

	if (SPB->DebugBlock) {
		db_t *d = (db_t *) SPB->DebugBlock;
		d->db_idbgbase = (__scunsigned_t)dp;
	}

	/* Set up environment record */
	dp->s_type = DO_ENV;
	dp->s_gp = getgp();
	np[0] = '\0';	/* Give first record a '\0' to point to */
	dp->ks_name = np;
	np++;
	dp++;

	/* Call a arb function */
	/* leave call first so it shows up in limited symmon lists */
	dp->s_func = (void (*)(void *, void *))idbg_call;
	strcpy(np, "call");
	dp->ks_name = np;
	np += strlen(np) + 1;
	dp++;
	
	/*
	 * Set up the call elements.
	 */
	for (tbl = idbgCommTbl; tbl->tbl; tbl++) {
		for (comm = tbl->tbl; comm->comm_name; comm++) {
			if (!comm->comm_func)
				continue;
			if (dp >= &idbgtab[IDBGMAXSTATFUNCS - 1])
				goto full;
			dp->s_func = comm->comm_func;
			dp->ks_name = comm->comm_name;
			dp->s_type = comm->comm_argstype;
			dp++;
		}
	}

 full:
	/*
	 * End the static table and set up the head node for 
	 * the dynamic allocated list.
	 */

	dp->s_type = DO_END;
	dp->s_head = &dbglist;
	dp->ks_name = NULL;
	dp->s_head->next = NULL;
	idbglaststatic = dp;
	spinlock_init(&idbg_addlock, "idbg_addlock");

	/*
	 * Do any dump setup required.
	 */
	addrinit();
	/* determine LEAF functions */
	leafinit();
}

/*
 * Perform initialization for idbg routines.
 * idbg_setup is called very early on.  _idbg_late_setup is called after
 * kernel memory allocators and such are initialized.
 */
void
_idbg_late_setup(void)
{
	/*
	 * Allocate a page of kvspace for use by ubt command.
	 */
#if (_MIPS_SIM != _ABI64) || defined(R4000)	/* r4k needs it to avoid VCE */
	if (!sp_kvaddr)
		sp_kvaddr = (unsigned *)kvalloc(1, VM_VACOLOR, 
						colorof(KSTACKPAGE));
#endif
#if EXTKSTKSIZE == 1
	/*
	 * Allocate a page of kvspace for use by ubt command for kernel stack
	 * extension page.
	 */
	if (!extsp_kvaddr)
		extsp_kvaddr = (unsigned *)kvalloc(1, VM_VACOLOR, 
						colorof(KSTACKPAGE-NBPC));
#endif
}

int
_idbg_unload(void)
{
	/* Free the kvspace allocated for use by ubt command */
#if (_MIPS_SIM != _ABI64) || defined(R4000)	/* r4k needs it to avoid VCE */
	if (sp_kvaddr){
		kvfree(sp_kvaddr, 1);
		sp_kvaddr = 0;
	}
#endif
#if EXTKSTKSIZE == 1
	/* Free the kvspace allocated for stack extension */
	if (extsp_kvaddr){
		kvfree(extsp_kvaddr, 1);
		extsp_kvaddr = 0;
	}
#endif
	spinlock_destroy(&idbg_addlock);
	return	0;
}

/*
 * Add filesystem switch to the list.
 */
void
_idbg_addfssw(idbgfssw_t *i)
{
	i->next = idbgfssws;
	idbgfssws = i;
}

/*
 * Remove filesystem switch from the list.
 */
void
_idbg_delfssw(idbgfssw_t *i)
{
	idbgfssw_t	**p;

	for (p = &idbgfssws; *p; p = &(*p)->next) {
		if ((*p)->next == i) {
			(*p)->next = i->next;
			return;
		}
	}
}

/*
 * Find switch entry given vfs.
 */
idbgfssw_t *
idbg_findfssw_vfs(vfs_t *vfsp, bhv_desc_t **bdp)
{
	idbgfssw_t	*rval;

	for (rval = idbgfssws; rval; rval = rval->next) {
		*bdp = bhv_lookup_unlocked(VFS_BHVHEAD(vfsp), rval->vfsops);
		if (*bdp)
			return rval;
	}

	return NULL;
}

/*
 * Find switch entry given vnode.
 */
idbgfssw_t *
idbg_findfssw_vnbhv(bhv_desc_t *bdp)
{
	idbgfssw_t	*rval;

	for (rval = idbgfssws; rval; rval = rval->next)
		if ((void *)rval->vnops == BHV_OPS(bdp))
			return rval;
	return NULL;
}

/*
 * add a function to the table
 */
int
_idbg_addfunc(char *name, void (*func)())
{
	dbgoff_t *dp;
	dbglist_t *dl;
	int s;

	if (n_dbglist >= idbgmaxfuncs)
		return 1;

	s = mutex_spinlock(&idbg_addlock);

	/* Compare name to the static table functions, looking for dups */
        for (dp = idbgtab + 1; dp < idbglaststatic; dp++)
		if (!strcmp(name, dp->ks_name)) {
			mutex_spinunlock(&idbg_addlock, s);
			return 1;
		}

	/*
	 * If it's too early to call kern_malloc, place the function
	 * in the static idbgtab table.
	 */

	if (!kheap_initialized) {
		if (dp >= &idbgtab[IDBGMAXSTATFUNCS - 1])  {
			mutex_spinunlock(&idbg_addlock, s);
			return 1;
		}

       		dp->s_type = INTEGER_ARGS;
        	dp->s_func = func;
        	strcpy(np, name);
		dp->ks_name = np;
		np += strlen(name) + 1;
        	dp++;
		dp->s_type = DO_END;
		dp->s_head = &dbglist;
		dp->ks_name = NULL;
		idbglaststatic = dp;
		mutex_spinunlock(&idbg_addlock, s);
		return 0;
	}

	/* Compare name to the dynamic list functions, looking for dups */
	for (dl = &dbglist; dl->next; dl = dl->next)
                if (!strcmp(name, dl->next->dp.ks_name)) {
			mutex_spinunlock(&idbg_addlock, s);
                        return 1;
		}

	dl->next = (dbglist_t *)kern_malloc(sizeof (dbglist_t));
	dl = dl->next;
	dl->dp.s_type = INTEGER_ARGS;
	dl->dp.s_func = func;
	dl->dp.ks_name = (char *)kern_malloc(strlen(name) + 1);
	strcpy(dl->dp.ks_name, name);
	dl->next = NULL;
	n_dbglist++;
	mutex_spinunlock(&idbg_addlock, s);
	return 0;
}

/*
 * find a function in the table
 */
int
idbg_findfunc(char *name, void (**func)())
{
	dbgoff_t *dp;
	dbglist_t *dl;
	int s;

	if (n_dbglist >= idbgmaxfuncs)
		return 1;

	s = mutex_spinlock(&idbg_addlock);

        for (dp = idbgtab + 1; dp < idbglaststatic; dp++)
		if (!strcmp(name, dp->ks_name)) {
			*func = dp->s_func;
			mutex_spinunlock(&idbg_addlock, s);
			return 1;
		}

	for (dl = &dbglist; dl->next; dl = dl->next)
                if (!strcmp(name, dl->next->dp.ks_name)) {
			*func = dl->next->dp.s_func;
			mutex_spinunlock(&idbg_addlock, s);
                        return 1;
		}

	mutex_spinunlock(&idbg_addlock, s);
	return 0;
}

/*
 * remove a function from the table
 */
void
_idbg_delfunc(void (*func)())
{
	dbglist_t *dbgnode, *dl;
	int s;

	s = mutex_spinlock(&idbg_addlock);
	for (dl = &dbglist; dl->next; dl = dl->next) {
		if (dl->next->dp.s_func == func) {
			dbgnode = dl->next;
			dl->next = dl->next->next;
			kern_free(dbgnode->dp.ks_name);
			kern_free(dbgnode);
			n_dbglist--;
			break;
		}
	}
	mutex_spinunlock(&idbg_addlock, s);
}

/*
 * Invoked from user mode to copy out the function table.
 */
int
_idbg_copytab(caddr_t *uaddr, int len, rval_t *rvp)
{
	int alen;
	dbglist_t *dl;
	dbgoff_t *copyoutbuf, *dp, *u_dp;

	if (!(copyoutbuf = (dbgoff_t *)kern_malloc(len)))
		return ENOMEM;

	u_dp = copyoutbuf;
	alen = sizeof (dbgoff_t);
	
	for (dp = idbgtab; dp->s_type != DO_END; dp++) {
		if (alen >= len)
			break;
		u_dp->s_type = dp->s_type;
		u_dp->s_func = dp->s_func;
		strncpy(u_dp->us_name, dp->ks_name, sizeof (u_dp->us_name));
		u_dp->us_name[sizeof (u_dp->us_name) - 1] = '\0';
		alen += sizeof (dbgoff_t);
		u_dp++;
	}

	for (dl = dbglist.next; dl; dl = dl->next) {
		if (alen >= len)
			break;
		u_dp->s_type = dl->dp.s_type;
		u_dp->s_func = dl->dp.s_func;
		strncpy(u_dp->us_name, dl->dp.ks_name, sizeof (u_dp->us_name));
		u_dp->us_name[sizeof (u_dp->us_name) - 1] = '\0';
		alen += sizeof (dbgoff_t);
		u_dp++;
	}

	if (alen <= len) {
		u_dp->s_type = idbglaststatic->s_type;
		u_dp++;
	}

	if (copyout((caddr_t) copyoutbuf, uaddr, alen)) {
		kern_free(copyoutbuf);
		return EFAULT;
	}

	rvp->r_val1 = alen;

	kern_free(copyoutbuf);
	return 0;
}

/*
 * Return the number of entries to allocate for a function table.
 */
void
_idbg_tablesize(rval_t *rvp)
{
	rvp->r_val1 = idbglaststatic - idbgtab + 1 + n_dbglist;
	return;
}

/*
 * Invoked from user mode to invoke an entry point.
 * NOTE: this works for only ONE user AT A TIME
 */
int
_idbg_dofunc(struct idbgcomm *s, rval_t *rvp)
{
	struct idbgcomm ic;
	struct apair {
		caddr_t addr;
		unsigned len;
	} ap;
	__psint_t args[5];
	int i, x;
	dbgoff_t dp;
	int error = 0;
	int resid;
	int ix;
	__int64_t i64;
	__int32_t i32;
	int numstatfuncs;
	dbglist_t *dl;
	/*REFERENCED (!MP)*/
	void *old_rtpin;

	/* make sure no-one else doing this */
	if (idbgoutbuf) {
		/* already allocated */
		return EBUSY;
	}

	/* copy in users args */
	if (copyin((caddr_t) s, &ic, sizeof(ic)))
		return EFAULT;

	numstatfuncs = idbglaststatic - idbgtab - 1;

	args[0] = ic.i_arg;
	args[1] = args[2] = args[3] = args[4] = 0;
	/* make sure valid function */
	x = ic.i_func;
	if (x < 1 || x > numstatfuncs + n_dbglist || ic.i_argcnt > 4)
		return EINVAL;
	
	if (x <= numstatfuncs)
		dp = idbgtab[x];
	else {
		dl = dbglist.next;
		for (i = 0; i < x - numstatfuncs - 1 && dl; i++)
			dl = dl->next;
		dp = dl->dp;
	}

	if (ic.i_argcnt > 0) {
		idbginbuf = kern_malloc(1024);
		resid = 1024;
	}

	/* now alloc mem for arg(s) itself if need-be */
	for (i = 0, ix = 0; i < ic.i_argcnt; i++) {
		/*
		 * it's a weird command with its own arg - we'll have to
		 * handle this separately
		 */
		if (copyin(ic.i_argp, &ap, sizeof(ap))) {
			error = EFAULT;
			goto errout;
		}
		ic.i_argp += sizeof(ap);
		if (ap.len > resid) {
			error = E2BIG;
			goto errout;
		}
		switch (dp.s_type) {
		case INTEGER_ARGS:
			switch (ap.len) {
			case sizeof(__int32_t):
				if (copyin(ap.addr, &i32, ap.len)) {
					error = EFAULT;
					goto errout;
				}
				args[i+1] = (__psint_t)(__psunsigned_t)i32;
				break;
			case sizeof(__int64_t):
				if (copyin(ap.addr, &i64, ap.len)) {
					error = EFAULT;
					goto errout;
				}
				args[i+1] = (__psint_t)i64;
				break;
			}
			break;
		case STRING_ARGS:
			if (copyin(ap.addr, &idbginbuf[ix], ap.len)) {
				error = EFAULT;
				goto errout;
			}
			args[i+1] = (__psint_t)&idbginbuf[ix];
			break;
		}
		resid -= ap.len;
		ix += ap.len;
	}

	/* alloc output buffer */
	idbgoutbuf = kern_malloc(1024);
	idbginvoke = IDBGUSER;
	idbguaddr = ic.i_uaddr;

	/* Some idbg functions, like idbg_ubtrace, need to completely execute
	 * on one cpu without being migrated (in this case due to calls to
	 * unmaptlb() which does not perform the intended function if we
	 * mgirate).  So pin the thread.
	 */
	old_rtpin = rt_pin_thread();
	/* protect function from errors! */
	if (setjmp(idbgjmp)) {
		/* store fault info back in comm arg */
		ic.i_badvaddr = curuthread->ut_flt_badvaddr;
		ic.i_cause = curuthread->ut_flt_cause;
		error = copyout(&ic, (caddr_t) s, sizeof(ic)) ? EFAULT : 0;
		rt_unpin_thread(old_rtpin);
		goto errout;
	}
	curthreadp->k_nofault = NF_IDBG;
	/* some functions (ubt) are strange and take bizarre args -
	 * handle that here by special casing
	 */
	if (dp.s_func == (void (*)(void *, void *))idbg_ubtrace) {
		/* if three args then assume user knows whats going on
		 * else assume 1 arg and stuff a -1 into arg 0
		 */
		if (ic.i_argcnt != (3-1)) {
			/* assume gave proc index */
			args[1] = args[0];
			args[0] = -1;
		}
		(*(void (*)(__psint_t []))dp.s_func)(args);
	} else {
		(*(void (*)(__psint_t, __psint_t, __psint_t, __psint_t))
		 dp.s_func)(args[0],
		 args[1], args[2], args[3]);
	}
	curthreadp->k_nofault = 0;
	rt_unpin_thread(old_rtpin);

errout:
	if (idbgoutbuf)
		kern_free(idbgoutbuf);
	if (idbginbuf)
		kern_free(idbginbuf);
	idbgoutbuf = idbginbuf = NULL;
	idbginvoke = IDBGSYMMON;
	rvp->r_val1 = idbguaddr - ic.i_uaddr;
	return error;
}

/*
 * output routine - if from symmon then dump to dprintf
 * else to user supplied buffer
 */
#define ARGS args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],args[8],args[9],args[10],args[11],args[12],args[13],args[14],args[15]

/* VARARGS1 */
void
_qprintf(char *fmt, va_list ap)
{
	__psint_t args[16];
	int len;
	if (idbginvoke == IDBGSYMMON) {
		aptoargs(fmt, ap, args);
		dprintf(fmt, ARGS);
	} else if (idbgoutbuf) {
		vsprintf(idbgoutbuf, fmt, ap);
		len = strlen(idbgoutbuf);
		curthreadp->k_nofault = 0;

		/* copy out to user for now we ignore length arg */
		if (copyout(idbgoutbuf, idbguaddr, len)) {
			/* EFAULT */
			longjmp(idbgjmp, 1);
		}
		curthreadp->k_nofault = NF_IDBG;
		idbguaddr += len;
	} else {
		/* something ain't right */
		cmn_err(CE_CONT, "idbg:invoker is user but no buffer\n");
		idbginvoke = IDBGSYMMON;
	}
	return;
}

/*
 * idbg_error -	address errors come here
 */
int
_idbg_error(void)
{
	curthreadp->k_nofault = 0;
	longjmp(idbgjmp, 1);

	/*NOTREACHED*/
	return(0);
}

/*
 * Invoked from user mode to change an OS switch.
 */
char *tbl_snames[] = {
	0,
	"syscalltrace",
	"doass",
	0,
};
#ifdef DEBUG
extern int syscalltrace;
#endif
extern int doass;

void
_idbg_switch(int snum, int onoff)
{
	register int i;

	switch (snum) {
	case 0:
		qprintf("AVAILABLE IDBG SWITCHES:\n");
		for (i = 1; tbl_snames[i]; i++)
			qprintf("%s(%d) ", tbl_snames[i], i);
		break;
#ifdef DEBUG
	case 1:
		syscalltrace = (onoff == 0 ? 0 : 1);
		break;
#endif
	case 3:
		doass = (onoff == 0 ? 0 : 1);
		break;
	default:
		qprintf("IDBG SWITCH - unknown switch %d\n", snum);
		break;
	}
	return;
}

/* useful address dumping routines for use with idbg_dofunc from user land */
void
idbg_string(addr)
{
	qprintf("%s\n", addr);
}

/*
 * print flags
 */
void
_printflags(register uint64_t flags,
	register char **strings,
	register char *name)
{
	register uint64_t mask = 1;

	if (name)
		qprintf("%s 0x%llx <", name, flags);
	while (flags != 0 && *strings) {
		if (mask & flags) {
			qprintf("%s ", *strings);
			flags &= ~mask;
		}
		mask <<= 1;
		strings++;
	}
	if (name)
		qprintf("> ");
	return;
}

void
idbg_hd(register unsigned *addr, register __psint_t len)
{
	register int i;
	register unsigned j;

	if (idbginvoke != IDBGUSER) {
		qprintf("hd only valid for user level idbg\n");
		return;
	}
	
	if (!len)
		return;

	/* we really can't copyout the whole thing since we don't know if
	 * addr/len are valid
	 */
	for (i = 0; i < len; i++) {
		j = *addr++;

		curthreadp->k_nofault = 0;
		/* copy out to user for now we ignore length arg */
		if (suword(idbguaddr, j)) {
			/* EFAULT */
			longjmp(idbgjmp, 1);
		}
		curthreadp->k_nofault = NF_IDBG;
		idbguaddr += sizeof(*addr);
	}
	return;
}

/*
 * These are a set of routines used to print out trace buffers
 * constructed using the routines in os/ktrace.c.
 */

/*
 * Return the number of entries in the trace buffer.
 */
int
ktrace_nentries(
	ktrace_t	*ktp)
{
	if (ktp == NULL) {
		return 0;
	}

	return (ktp->kt_rollover ? ktp->kt_nentries : ktp->kt_index);
}

/*
 * ktrace_first()
 *
 * This is used to find the start of the trace buffer.
 * In conjunction with ktrace_next() it can be used to
 * iterate through the entire trace buffer.  This code does
 * not do any locking because it is assumed that it is called
 * from the debugger.
 *
 * The caller must pass in a pointer to a ktrace_snap
 * structure in which we will keep some state used to
 * iterate through the buffer.  This state must not touched
 * by any code outside of this module.
 */
ktrace_entry_t *
ktrace_first(ktrace_t	*ktp, ktrace_snap_t	*ktsp)
{
	ktrace_entry_t	*ktep;
	int		index;
	int		nentries;

	if (ktp->kt_rollover)
		index = ktp->kt_index;
	else
		index = 0;

	ktsp->ks_start = index;
	ktep = &(ktp->kt_entries[index]);
	
	nentries = ktrace_nentries(ktp);
	index++;
	if (index < nentries) {
		ktsp->ks_index = index;
	} else {
		ktsp->ks_index = 0;
		if (index > nentries)
			ktep = NULL;
	}
	return ktep;
}


/*
 * ktrace_next()
 *
 * This is used to iterate through the entries of the given
 * trace buffer.  The caller must pass in the ktrace_snap_t
 * structure initialized by ktrace_first().  The return value
 * will be either a pointer to the next ktrace_entry or NULL
 * if all of the entries have been traversed.
 */
ktrace_entry_t *
ktrace_next(
	ktrace_t	*ktp,
	ktrace_snap_t	*ktsp)
{
	int		index;
	ktrace_entry_t	*ktep;

	index = ktsp->ks_index;
	if (index == ktsp->ks_start) {
		ktep = NULL;
	} else {
		ktep = &ktp->kt_entries[index];
	}

	index++;
	if (index == ktrace_nentries(ktp)) {
		ktsp->ks_index = 0;
	} else {
		ktsp->ks_index = index;
	}

	return ktep;
}

/*
 * ktrace_skip()
 *
 * Skip the next "count" entries and return the entry after that.
 * Return NULL if this causes us to iterate past the beginning again.
 */
ktrace_entry_t *
ktrace_skip(
	ktrace_t	*ktp,
	int		count,	    
	ktrace_snap_t	*ktsp)
{
	int		index;
	int		new_index;
	ktrace_entry_t	*ktep;
	int		nentries = ktrace_nentries(ktp);

	index = ktsp->ks_index;
	new_index = index + count;
	while (new_index >= nentries) {
		new_index -= nentries;
	}
	if (index == ktsp->ks_start) {
		/*
		 * We've iterated around to the start, so we're done.
		 */
		ktep = NULL;
	} else if ((new_index < index) && (index < ktsp->ks_index)) {
		/*
		 * We've skipped past the start again, so we're done.
		 */
		ktep = NULL;
		ktsp->ks_index = ktsp->ks_start;
	} else {
		ktep = &(ktp->kt_entries[new_index]);
		new_index++;
		if (new_index == nentries) {
			ktsp->ks_index = 0;
		} else {
			ktsp->ks_index = new_index;
		}
	}
	return ktep;
}


/*
 * Proc table entry dump.
 */
char *tab_pstat[] = {
	"INVALID",		/* 0 */
	"",			/* SRUN -- don't bother to print anything */
	"ZOMBIE",
	"unused",	
	"unused",	
	"unused",	
	0,
	0,
	0,
	0
};

char *tab_pflag[] = {
	"statelock",	/* 0x00000001 */
	"trace",	/* 0x00000002 */
	"ignored",	/* 0x00000004 */
	"bblst",	/* 0x00000008 */
	"parswtch",	/* 0x00000010 */
	"proctr",	/* 0x00000020 */
	"jstop",	/* 0x00000040 */
	"propen",	/* 0x00000080 */
	"parsys",	/* 0x00000100 */
	"parinh",	/* 0x00000200 */
	"parpriv",	/* 0x00000400 */
	"pgjcl",	/* 0x00000800 */
	"wsrch",	/* 0x00001000 */
	"ckpt",		/* 0x00002000 */
	"0x00004000",	/* 0x00004000 */
	"0x00008000",	/* 0x00008000 */
	"noctty",	/* 0x00010000 */
	"fdt",		/* 0x00020000 */
	"proffast",	/* 0x00040000 */
	"prof32",	/* 0x00080000 */
	"prof",		/* 0x00100000 */
	"cexit",	/* 0x00200000 */
	"execed",	/* 0x00400000 */
	"corepid",	/* 0x00800000 */
	"0x01000000",	/* 0x01000000 */
	"pardis",	/* 0x02000000 */
	"0x04000000",	/* 0x04000000 */
	"0x08000000",	/* 0x08000000 */
	"prprotect",	/* 0x10000000 */
	"exit",		/* 0x20000000 */
	"abortsig",	/* 0x40000000 */
	"0x80000000",	/* 0x80000000 */
	0
};

char *tab_policy[] = {
	"SCHED_FIFO",	/* 1 */
	"SCHED_RR",	/* 2 */
	"SCHED_TS",	/* 3 */
	"SCHED_NP"	/* 4 */
};

char *whytab[] = {
	"UNK",		/* 0 */
	"REQUESTED",	/* 1 */
	"SIGNALLED",	/* 2 */
	"SYSENTRY",	/* 3 */
	"SYSEXIT",	/* 4 */
	"FAULTED",	/* 5 */
	"JOBCONTROL",	/* 6 */
	"CHECKPOINT",	/* 7 */
};
char *tab_abi[] = {
	"obsolete4",	/* 1 */
	"IRIX5",	/* 2 */
	"IRIX5_64",	/* 4 */
	"NEW 32",	/* 8 */
	0
};

char *tab_ktimer[] = {
	"USR_RUN",	
	"SYS_RUN",
	"INT_RUN",
	"BIO_WAIT",
	"MEM_WAIT",
	"SELECT_WAIT",
	"JCL_WAIT",	
	"RUNQ_WAIT",
	"SLEEP_WAIT",
	"STRMON_WAIT",
	"PHYSIO_WAIT"
};

/*
 * print the type of timer that 'timer' points at
 */
void
printtimers(kthread_t *kt)
{
	ktimerpkg_t *kp;
	unsigned int i;

	if (!kt)
		return;
	kp = &kt->k_timers;

	qprintf("  Current process timer: ");
	if (kp->kp_curtimer >= MAX_PROCTIMER)
		qprintf("INVALID: %d\n", kp->kp_curtimer);
	else
		qprintf("%s\n", tab_ktimer[kp->kp_curtimer]);
	for (i = 0; i < MAX_PROCTIMER; i++) {
		qprintf("    %14s %20ld", tab_ktimer[i], kp->kp_timer[i]);
		if (i & 1)
			qprintf("\n");
	}
	if (i & 1)
		qprintf("\n");
	qprintf("    %14s  ", "Timer Started:");
#if defined(CLOCK_CTIME_IS_ABSOLUTE)
	qprintf("rtc %lu (0x%lx)\n", kp->kp_snap, kp->kp_snap);
#else
	qprintf("time %u, rtc %u\n", kp->kp_snap.secs, kp->kp_snap.rtc);
#endif
}

char *tab_events[] = {
	"POLLIN",	/* 0x00000001 */
	"POLLPRI",	/* 0x00000002 */
	"POLLOUT",	/* 0x00000004 */
	"POLLERR",	/* 0x00000008 */
	"POLLHUP",	/* 0x00000010 */	
	"POLLNVAL",	/* 0x00000020 */	
	"POLLRDNORM",	/* 0x00000040 */	
	"POLLRDBAND",	/* 0x00000080 */	
	"POLLWRBAND",	/* 0x00000100 */	
	0
};

/*
 * Get the name of a kthread
 */
char *
idbg_kthreadname(kthread_t *kt)
{
	char	*name;
	if (KT_ISUTHREAD(kt)) {
		name = UT_TO_PROC(KT_TO_UT(kt))->p_comm;
	} else if (KT_ISSTHREAD(kt)) {
		name = KT_TO_ST(kt)->st_name;
	} else if (KT_ISXTHREAD(kt)) {
		name = KT_TO_XT(kt)->xt_name;
	} else 
		name = "?";
	return name;
}

/*
 * kp polldat addr - dump information about a polldat
 */
void
idbg_polldat(struct polldat *p)
{
	qprintf("polldat 0x%x:", (__psunsigned_t)p);
	_printflags(p->pd_events, tab_events, "events");
#if CELL_IRIX
	qprintf("pid %d, utid %d, headp 0x%x, rotor %d\n",
		p->pd_pid, p->pd_tid, 
		p->pd_headp, p->pd_rotorhint);
#else
	qprintf("headp 0x%x, rotor %d\n", p->pd_headp, p->pd_rotorhint);
#endif
}

/*
 * kp pollhead addr - dump information about a pollhead
 */
void
idbg_pollhead(struct pollhead *p)
{
	struct polldat *dat;

	qprintf("pollhead 0x%x:\n", p);
	qprintf("  gen 0x%x\n", POLLGEN(p));

	for (dat = p->ph_list; dat; dat = dat->pd_next)
		idbg_polldat(dat);

	for (dat = p->ph_next; dat; dat = dat->pd_next)
		idbg_polldat(dat);
}

/*
 * Print out the list of commands.
 */
void
idbg_help()
{
	struct idbgcommfunc *comm;
	struct idbgcommtbl *tbl;
	dbglist_t *dl;
	int i = 0;

	/*
	 * First print out the ones that we know about.
	 */
	for (tbl = idbgCommTbl; tbl->tbl; tbl++) {
		qprintf("%s:\n", tbl->name);
		for (comm = tbl->tbl; comm->comm_name; comm++) {
			if (i++ >= IDBGMAXSTATFUNCS - 2)
				goto done;
			qprintf("%s %s\n",
				_padstr(comm->comm_name, 16),
				comm->comm_descrip);
		}
	}
	
 done:
	/*
	 * Now, the ones we don't know about.
	 */
	qprintf("Dynamically attached (no description):\n");

	for (dl = dbglist.next; dl; dl = dl->next)
		qprintf("%s\n", _padstr(dl->dp.ks_name, 16));
}


char *tab_prxyflags[] = {
	"lock",		/* 0x00000001 */
	"exit",		/* 0x00000002 */
	"exec",		/* 0x00000004 */
	"sproc",	/* 0x00000008 */
	"spipe",	/* 0x00000010 */
	"uservme",	/* 0x00000020 */
	"wait",		/* 0x00000040 */
	"jstop",	/* 0x00000080 */
	"jpoll",	/* 0x00000100 */
	"pwait",	/* 0x00000200 */
	"pstop",	/* 0x00000400 */
	"jsarmed",	/* 0x00000800 */
	"jstopped",	/* 0x00001000 */
	"0x2000",	/* 0x00002000 */
	"0x4000",	/* 0x00004000 */
	"0x8000",	/* 0x00008000 */
	"lonewt",	/* 0x00010000 */
	0
};

char *tab_prxyshmask[] = {
	"sproc",	/* 0x00000001 */
	"sfds",		/* 0x00000002 */
	"sdir",		/* 0x00000004 */
	"sumask",	/* 0x00000008 */
	"sulimit",	/* 0x00000010 */
	"sid",		/* 0x00000020 */
	"saddr",	/* 0x00000040 */
	"pthreads",	/* 0x00000080 */
	"0x0100",	/* 0x00000100 */
	"0x0200",	/* 0x00000200 */
	"0x0400",	/* 0x00000400 */
	"0x0800",	/* 0x00000800 */
	"0x1000",	/* 0x00001000 */
	"0x2000",	/* 0x00002000 */
	"0x4000",	/* 0x00004000 */
	"0x8000",	/* 0x00008000 */
	"0x10000",	/* 0x00010000 */
	"0x20000",	/* 0x00020000 */
	"0x40000",	/* 0x00040000 */
	"0x80000",	/* 0x00080000 */
	"0x100000",	/* 0x00100000 */
	"0x200000",	/* 0x00200000 */
	"0x400000",	/* 0x00400000 */
	"0x800000",	/* 0x00800000 */
	"block",	/* 0x01000000 */
	"nolibc",	/* 0x02000000 */
	"UNKNOWN",	/* 0x04000000 */
	"UNKNOWN",	/* 0x08000000 */
	"UNKNOWN",	/* 0x10000000 */
	"UNKNOWN",	/* 0x20000000 */
	"UNKNOWN",	/* 0x40000000 */
	"UNKNOWN",	/* 0x80000000 */
	0
};

void
idbg_proxy(proc_proxy_t *prxy)
{
	qprintf(" proxy 0x%x syscall 0x%x &ru 0x%x &acct 0x%x\n",
		prxy, prxy->prxy_syscall, &prxy->prxy_ru, &prxy->prxy_exit_acct);

	qprintf("  flags: ");
	_printflags((unsigned)prxy->prxy_flags, tab_prxyflags, 0);

	qprintf("; held %d; shmask: ", prxy->prxy_hold);
	_printflags((unsigned)prxy->prxy_shmask, tab_prxyshmask, 0);

	qprintf("\n  syscall 0x%x abi ", prxy->prxy_syscall);
	if (prxy->prxy_abi)
		_printflags((u_int)prxy->prxy_abi, tab_abi, 0);
	else
		qprintf("(none) ");
#ifdef R10000
	qprintf("cpumon 0x%x", prxy->prxy_cpumon);
#endif
	qprintf("\n  &uthread lock 0x%x threadp 0x%x, #thrds %d\n",
		&prxy->prxy_thrdlock, prxy->prxy_threads,
		prxy->prxy_nthreads);
	
	qprintf("  tramp 0x%x oldctxt 0x%x, siglb\n",
		prxy->prxy_sigtramp, prxy->prxy_oldcontext, prxy->prxy_siglb);
	qprintf("  ssflags 0x%x sigsp 0x%x spsize 0x%x jscount %d\n",
		(int)prxy->prxy_ssflags, prxy->prxy_sigsp,
		(int)prxy->prxy_spsize, prxy->prxy_jscount);

	if (prxy->prxy_fp.pfp_fpflags ||
	    prxy->prxy_fp.pfp_nofpefrom ||
	    prxy->prxy_fp.pfp_nofpeto) {
		qprintf("  fpflags 0x%x nofpefrom 0x%x nofpeto 0x%x\n",
			prxy->prxy_fp.pfp_fpflags,
			prxy->prxy_fp.pfp_nofpefrom,
			prxy->prxy_fp.pfp_nofpeto);
	}
}
	
void
pcred(cred_t *crp)
{
	int i;

	if (crp == NULL)
		return;
	qprintf(" cred @ 0x%lx cap 0x%x/0x%x/0x%x ref %d\n",
		crp,
		crp->cr_cap.cap_effective,
		crp->cr_cap.cap_permitted,
		crp->cr_cap.cap_inheritable,
		crp->cr_ref);
	qprintf("  uid %d gid %d ruid %d rgid %d suid %d sgid %d\n",
		crp->cr_uid, crp->cr_gid, crp->cr_ruid, crp->cr_rgid,
		crp->cr_suid, crp->cr_sgid);
	if (crp->cr_ngroups != 0) {
		qprintf("  mgrps:");
		for (i = 0; i < crp->cr_ngroups; i++) {
			qprintf(" %d", crp->cr_groups[i]);
		}
		qprintf("\n");
	}
	if (mac_enabled)
		print_maclabel("  MAC label ", crp->cr_mac);
}

/*
 * Given a generic pointer, find the uthread pointer might imply.
 * Call pfunc on that uthread -- if the `uthread' pointer turns out
 * to be a proc address, and 'all' is set, apply pfunc to all the
 * uthreads in the process; otherwise apply it to the first uthread.
 * If 'must' is set then return 0 if not a valid uthread can be found.
 */
int
idbg_doto_uthread(__psint_t p1, void (*pfunc)(uthread_t *), int all, int must)
{
	uthread_t *ut;
	proc_t *pp;

	if (p1 == -1L) {
		ut = curuthread;

		if (ut == NULL) {
			qprintf("no current uthread\n");
			return 0;
		}
	} else if (p1 < 0L) {
		ut = (uthread_t *) p1;
		if (!uthreadp_is_valid(ut)) {
			if (must) {
				qprintf("0x%x is not a valid uthread address\n", ut);
				return 0;
			}
			/*
			 * Otherwise, call function with p1 anyway.
			 */
		}
	} else {
		if ((pp = idbg_pid_to_proc(p1)) == NULL) {
			qprintf("%d is not an active pid\n", (pid_t)p1);
			return 0;
		}

		for (ut = pp->p_proxy.prxy_threads; ut; ut = ut->ut_next) {
			(*pfunc)(ut);
			if (!all)
				break;
		}
		return 1;
	}

	(*pfunc)(ut);
	return 1;
}

char *tab_utflags[] = {
	"0x00000001",	/* 0x00000001 */ /* Was UT_SLEEP */
	"sright",	/* 0x00000002 */ 
	"prstopx",	/* 0x00000004 */ 
	"sxbrk",	/* 0x00000008 */
	"stop",		/* 0x00000010 */
	"prstopj",	/* 0x00000020 */
	"step",		/* 0x00000040 */
	"wsys",		/* 0x00000080 */
	"sigsuspend",	/* 0x00000100 */
	"pthread",	/* 0x00000200 */
	"in-kernel",	/* 0x00000400 */
	"trwait",	/* 0x00000800 */
	"inactive",	/* 0x00001000 */
	"NULL3",	/* 0x00002000 */
	"eventpr",	/* 0x00004000 */
	"holdjs",	/* 0x00008000 */
	"NULL1",	/* 0x00010000 */
	"clrhalt",	/* 0x00020000 */
	"no-hang",	/* 0x00040000 */
	"isolate",	/* 0x00080000 */
	"mustrunlck",	/* 0x00100000 */
	"nomruninh",	/* 0x00200000 */
	"oweupc",	/* 0x00400000 */
	"fixade",	/* 0x00800000 */
	"NULL2",	/* 0x01000000 */ 
	"sysabort",	/* 0x02000000 */
	"block",	/* 0x04000000 */
	"ptpscope",	/* 0x08000000 */
	"blkonentry",	/* 0x10000000 */
	"ckpt",		/* 0x20000000 */
	"reloadfp",	/* 0x40000000 */
	"ptstep",	/* 0x80000000 */
	0
};

char *tab_utupdate[] = {
	"upd-dirs",	/* 0x00000001 */
	"upd-uid",	/* 0x00000002 */
	"upd-ulimit",	/* 0x00000004 */
	"upd-umask",	/* 0x00000008 */
	"upd-cred",	/* 0x00000010 */
	"upd-sig",	/* 0x00000020 */
	"upd-sigvec",	/* 0x00000040 */
	"0x0080",	/* 0x00000080 */
	"0x0100",	/* 0x00000100 */
	"0x0200",	/* 0x00000200 */
	"0x0400",	/* 0x00000400 */
	"0x0800",	/* 0x00000800 */
	"0x1000",	/* 0x00001000 */
	"0x2000",	/* 0x00002000 */
	"0x4000",	/* 0x00004000 */
	"0x8000",	/* 0x00008000 */
	"0x010000",	/* 0x00010000 */
	"0x020000",	/* 0x00020000 */
	"0x040000",	/* 0x00040000 */
	"0x080000",	/* 0x00080000 */
	"0x00100000",	/* 0x00100000 */
	"0x00200000",	/* 0x00200000 */
	"0x00400000",	/* 0x00400000 */
	"0x00800000",	/* 0x00800000 */
	"0x01000000",	/* 0x01000000 */
	"0x02000000",	/* 0x02000000 */
	"0x04000000",	/* 0x04000000 */
	"0x08000000",	/* 0x08000000 */
	"0x10000000",	/* 0x10000000 */
	"0x20000000",	/* 0x20000000 */
	"0x40000000",	/* 0x40000000 */
	"upd-lock",	/* 0x80000000 */
	0
};

void
idbg_do_uthread(register uthread_t *ut)
{
#ifdef NUMA_BASE
	int	j;
#endif
	kthread_t *kt = UT_TO_KT(ut);
	exception_t *up = ut->ut_exception;
#ifndef NO_WIRED_SEGMENTS
	int ti;
	pde_t *pde;
#endif
	qprintf(" uthread 0x%x [%d] for vproc 0x%x\n",
		ut, ut->ut_id, ut->ut_vproc);
	qprintf("  proc 0x%x, proxy 0x%x\n",
		ut->ut_proc, ut->ut_pproxy);
	qprintf("  prev 0x%x, next 0x%x flags: ",
		ut->ut_prev, ut->ut_next);

	_printflags((unsigned)ut->ut_flags, tab_utflags, 0);
	qprintf("; upd-flags: ");
	_printflags((unsigned)ut->ut_update, tab_utupdate, 0);

	qprintf("\n  mustrun %d sonproc %d ticks %d\n",
		kt->k_mustrun, kt->k_sonproc, ut->ut_rticks);
	qprintf("  syscallno %d, argp 0x%x satmask 0x%x\n",
		ut->ut_syscallno, ut->ut_scallargs, ut->ut_satmask);

	qprintf(" gbinding %d \n", ut->ut_gbinding);
	qprintf("  polldat 0x%x fdinuse %d msz %d mmax %d many 0x%x\n",
		ut->ut_polldat,
		ut->ut_fdinuse-1, 
		ut->ut_fdmanysz, ut->ut_fdmanymax, ut->ut_fdmany);

	qprintf("  cdir 0x%x rdir 0x%x cmask 0%o cred 0x%x\n",
		ut->ut_cdir, ut->ut_rdir, ut->ut_cmask, ut->ut_cred);

	qprintf("  code %d whystop %d whatstop %d\n",
		ut->ut_code, ut->ut_whystop, ut->ut_whatstop);

	qprintf("  ukstk: pde @ 0x%x pfn 0x%x\n",
		&ut->ut_kstkpgs[KSTKIDX], ut->ut_kstkpgs[KSTKIDX].pte.pg_pfn);

	qprintf("  asid: vas 0x%x pasid 0x%x gen %ld prda 0x%x\n",
		ut->ut_asid.as_obj, ut->ut_asid.as_pasid, ut->ut_asid.as_gen,
		ut->ut_prda);

	qprintf("  watch 0x%x &ut_as 0x%x\n",
		ut->ut_watch, &ut->ut_as);

	qprintf("  pri %d tslice %d runcond (0x%x) %s\n",
		kt_pri(UT_TO_KT(ut)),
		ut->ut_tslice,
		UT_TO_KT(ut)->k_runcond,
		UT_TO_KT(ut)->k_runcond & RQF_GFX ? "GFX" : "");

	qprintf(" gang state %d gang binding %d \n", ut->ut_gstate,
						ut->ut_gbinding);
	printtimers(kt);

#if defined (R10000)
        qprintf(" cpumon 0x%x", ut->ut_cpumon);
#endif
	qprintf(" tfaults %d vfaults %d ufaults %d kfaults %d\n",
		ut->ut_acct.ua_tfaults, ut->ut_acct.ua_vfaults,
		ut->ut_acct.ua_ufaults, ut->ut_acct.ua_kfaults);
	dumpssleep(&ut->ut_pblock);

	print_ssthread(kt);

	qprintf("  exception 0x%x &eframe 0x%x\n",
		up, &up->u_eframe);

#if !NO_WIRED_SEGMENTS
	qprintf("  nexttlb %d pde: ", up->u_nexttlb);
#if !R4000 && !R10000
	pde = up->u_ubptbl;
	for (ti = 0; ti < (NWIREDENTRIES-TLBWIREDBASE); ti++, pde++)
#else
	pde = &up->u_ubptbl[0].pde_low;
	for (ti = 0; ti < (NWIREDENTRIES-TLBWIREDBASE) * 2; ti++, pde++)
#endif
	{
		if (pde->pgi)
			qprintf("%x [pfn 0x%x] ", pde->pgi, pde->pte.pg_pfn);
		else
			qprintf("%x ", pde->pgi);
	}
	qprintf("\n");

	qprintf("  thi: ");
	for (ti = 0; ti < (NWIREDENTRIES-TLBWIREDBASE); ti++)
		qprintf("%x ", up->u_tlbhi_tbl[ti]);
	qprintf("\n");
	qprintf("  sharena 0x%x maxrsaid %d npgs %d locore %d runable %d\n",
		ut->ut_sharena, ut->ut_maxrsaid, ut->ut_rsa_npgs,
		ut->ut_rsa_locore, ut->ut_rsa_runable);
	if (ut->ut_prda)
		qprintf("  t_nid %d\n", ut->ut_prda->sys_prda.prda_sys.t_nid);
#endif	/* !NO_WIRED_SEGMENTS */

#ifdef _SHAREII
	qprintf("  ShareII shadThread 0x%p\n", ut->ut_shareT);
#endif /*_SHAREII*/

	if (sat_enabled) {
		qprintf("  satid %d\n", ut->ut_sat_proc->sat_uid);
		qprintf("  sat_cwd 0x%x sat_root 0x%x\n",
		    ut->ut_sat_proc->sat_cwd, ut->ut_sat_proc->sat_root);
		qprintf("  sat_pn 0x%x sat_tokens 0x%x\n",
		    ut->ut_sat_proc->sat_pn, ut->ut_sat_proc->sat_tokens);
		qprintf("  sat_subsys %d sat_suflag 0x%x\n",
		    ut->ut_sat_proc->sat_subsysnum,ut->ut_sat_proc->sat_suflag);
		qprintf("  sat_event %d sat_cap 0x%x\n",
		    ut->ut_sat_proc->sat_event, ut->ut_sat_proc->sat_cap);
	}
        /*
         * Memory affinity
         */
#ifdef NUMA_BASE
	qprintf("mld: 0x%llx, affnode %d\n", UT_TO_KT(ut)->k_mldlink, UT_TO_KT(ut)->k_affnode);
	qprintf("maffbv: [");
	for (j=0; j<CNODEMASK_SIZE; j++)
		qprintf(" 0x%llx ", CNODEMASK_WORD(UT_TO_KT(ut)->k_maffbv, j));
	qprintf("],  nodemask: [");
	for (j=0; j<CNODEMASK_SIZE; j++)
		qprintf(" 0x%llx ", CNODEMASK_WORD(UT_TO_KT(ut)->k_nodemask, j));
	qprintf("]\n");
#endif /* NUMA_BASE */        
#if JUMP_WAR
	qprintf("Jump_war: set %u, max_wired %u\n",
		ut->ut_jump_war_set, ut->ut_max_jump_war_wired);
#endif	/* JUMP_WAR */

	/*
	 * Frame scheduler
	 */
	qprintf("FRS: lock 0x%x flags %x refs %d frs 0x%x\n",
		&ut->ut_frslock, ut->ut_frsflags, ut->ut_frsrefs, ut->ut_frs);
	/*
	 * recursive filesystem locking
	 */
	qprintf("vnlock = 0x%x\n", (int) ut->ut_vnlock);
}

void
idbg_uthread(__psint_t p1)
{
	(void) idbg_doto_uthread(p1, idbg_do_uthread, 1, 0);
}

/*
++
++	kp proc <arg> - dump information about a process
++		arg == -1 - use curprocp
++		arg < 0 - use addr as address of proc struct
++		arg >= 0 - use arg as pid to lookup
++
*/
void
idbg_proc(__psint_t p1)
{
   proc_t *pp;
   uthread_t *ut;
   long tmp;

	if (p1 == -1L) {
		pp = curprocp;
		if (pp == NULL) {
			qprintf("no current process\n");
			return;
		}
	} else if (p1 < 0L) {
		pp = (proc_t *) p1;
		/* check for validity but let them print it anyway */
		if (!procp_is_valid(pp)) {
			qprintf("WARNING:0x%x is not a valid proc address\n",
				pp);
		}
	} else if ((pp = idbg_pid_to_proc(p1)) == NULL) {
		qprintf("%d is not an active pid\n", (pid_t)p1);
		return;
	}

	shortproc(pp, 2);

	qprintf(" &bitlock 0x%x nice %d job 0x%x\n",
		&pp->p_bitlock, pp->p_proxy.prxy_sched.prs_nice,
		pp->p_proxy.prxy_sched.prs_job);
	qprintf(" childlist 0x%x:", pp->p_childpids);
	{
	child_pidlist_t	*c;
	for (c = pp->p_childpids; c; c = c->cp_next)
		qprintf(" %d", c->cp_pid);
	}
	qprintf("\n");
	pcred(pp->p_cred);

	qprintf(" p_trace 0x%x p_pstrace 0x%x p_trmasks 0x%x\n",
		pp->p_trace, pp->p_pstrace, pp->p_trmasks);
	qprintf(" p_ptimers 0x%x\n", pp->p_ptimers);
	qprintf(" p_childlock @0x%x; waitv @0x%x (waiter: 0x%x)\n",
		&pp->p_childlock,
		&pp->p_wait, SV_WAITER(&pp->p_wait));
	qprintf("slink [0x%x]\n", pp->p_slink);
	tmp = pp->p_exitsig;
	qprintf(" shmask (0x%x)%s%s%s%s%s%s%s%s"
		"shaddr 0x%x exitsig %d &itimer 0x%x\n",
		pp->p_proxy.prxy_shmask,
		pp->p_proxy.prxy_shmask & PR_SPROC ? "PROC " : "",
		pp->p_proxy.prxy_shmask & PR_SFDS ? "FDS " : "",
		pp->p_proxy.prxy_shmask & PR_SDIR ? "DIR " : "",
		pp->p_proxy.prxy_shmask & PR_SUMASK ? "UMASK " : "",
		pp->p_proxy.prxy_shmask & PR_SULIMIT ? "ULIM " : "",
		pp->p_proxy.prxy_shmask & PR_SID ? "ID " : "",
		pp->p_proxy.prxy_shmask & PR_SADDR	? "ADDR " : "",
		pp->p_proxy.prxy_shmask == 0 ? "<none>" : "",
		pp->p_shaddr, tmp, &pp->p_ii);

	qprintf(" pgflink 0x%x pgblink 0x%x",
		pp->p_pgflink, pp->p_pgblink);
	qprintf(" pgid %d vpgrp 0x%x sid %d\n",
		pp->p_pgid, pp->p_vpgrp, pp->p_sid);
        qprintf(" &vpagg 0x%x ",pp->p_vpagg);
        if (pp->p_vpagg != 0) {
	        qprintf(" @ 0x%x handle %lld prid %lld\n",
		        pp->p_vpagg,
			VPAG_GETPAGGID(pp->p_vpagg), VPAG_GETPRID(pp->p_vpagg));
	}
	qprintf(" gang 0x%x\n",
		pp->p_shaddr ? pp->p_shaddr->s_gdb : NULL);
#ifdef _SHAREII
	qprintf("\tShareII shadProc 0x%p\n", pp->p_shareP);
#endif /*_SHAREII*/

	for (ut = pp->p_proxy.prxy_threads; ut; ut = ut->ut_next) {
		idbg_do_uthread(ut);
	}
}

typedef	struct {
	caddr_t start;
	caddr_t end;
	char *type;
} sleepon_t;
extern sema_t slpsvs[];
extern sema_t *slpsv_end;
sleepon_t sleepon_tbl[] = {
	{ (caddr_t) &slpsvs[0], (caddr_t) &slpsv_end, "sleepsv" },
	{ 0, 0, 0 }
};

static void
addrinit(void)
{
}

static void
sleepon(kthread_t *kt)
{
	register int i;
	caddr_t wchan = kt->k_wchan;
	uint_t flags = kt->k_flags;
	char *name;

	qprintf("wchan 0x%x ", wchan);
	if (wchan == (caddr_t) 0)
		return;
	name = wchanname(wchan, flags);

	for (i = 0; sleepon_tbl[i].type != NULL; i++)
		if (wchan >= sleepon_tbl[i].start &&
		    wchan < sleepon_tbl[i].end)
			qprintf("(%s)", sleepon_tbl[i].type);
	if (flags & KT_WMUTEX) {
		mutex_t *mp = (mutex_t *)wchan;
		mmeter_t *kp = mutexmeter(mp);
		qprintf("(mutex [%s] -- owner: ", name);
		if (kp)
			qprintf("thread 0x%x) ", mutex_owner(mp));
		else
			qprintf("unknown)");
	} else if (flags & KT_WSV) {
		qprintf("(sv [%s]) ", name);
	} else if (flags & KT_WMRLOCK) {
		qprintf("(mrlock [%s]) ", name);
	} else if (flags & KT_WSEMA) {
		sema_t *sp = (sema_t *)wchan;
		k_semameter_t *kp = smeter(sp);
		qprintf("(sema [%s]", name);
		if (kp)
			if (kp->s_thread) {
				qprintf(" -- owner:");
				printid(kp->s_thread, kp->s_id, "thread");
			}
		qprintf(") ");
	}
}

static void
dumpssleep(register ssleep_t *st)
{
	qprintf(
	"  ssleep: wait %d slp %d sv 0x%x (q: 0x%x)\n",
		st->s_waitcnt, st->s_slpcnt,
		&st->s_wait, SV_WAITER(&st->s_wait));
}


/*
++	kp pchain addr - dump parent-child-sibling chain for a process
++		addr < 0 - use addr as address of proc struct
++		addr >= 0 - use addr as proc slot number
*/
void
idbg_pchain(__psint_t p1)
{
	proc_t *pp;
	child_pidlist_t *cpid;

	if (p1 < 0L) {
		pp = (proc_t *) p1;
		if (!procp_is_valid(pp)) {
			qprintf("WARNING:0x%x is not a valid proc address\n", pp);
		}
	} else if ((pp = idbg_pid_to_proc(p1)) == NULL) {
		qprintf("%d is not an active pid\n", (pid_t)p1);
		return;
	}

	qprintf("child pid list for proc 0x%x (pid %d):\n", pp, pp->p_pid);
	for (cpid = pp->p_childpids; cpid != NULL; cpid = cpid->cp_next)
		qprintf("   child pid %d, wcode %d, wdata %d, utime 0x%x\n"
			"     stime 0x%x, pgid %d, xstat 0x%x next 0x%x\n",
			cpid->cp_pid, cpid->cp_wcode, cpid->cp_wdata,
			hzto(&cpid->cp_utime), hzto(&cpid->cp_stime),
			cpid->cp_pgid, cpid->cp_xstat, cpid->cp_next);

}

static int
issthread(sthread_t *x)
{
	extern sthread_t sthreadlist;
	sthread_t *st;

	for (st = sthreadlist.st_next; st != &sthreadlist; st = st->st_next)
		if (x == st)
			return 1;
	return 0;
}

static int
isxthread(xthread_t *x)
{
	extern xthread_t xthreadlist;
	xthread_t *xt;

	for (xt = xthreadlist.xt_next; xt != &xthreadlist; xt = xt->xt_next)
		if (x == xt)
			return 1;
	return 0;
}

/*
 * kthread schedstates
 */
char *tab_kthreadstate[] = {
	"RUNNING",		/* 0 */
	"RUNNABLE",
	"BLOCKED",
	"DEFUNCT",
	"ONCPUQ",
	"LATENT",
	"IDLE",
	0,
	0,
	0
};

static void
kstack(kthread_t *kt)
{
	int *sp;
	caddr_t stktop = kt->k_stack + kt->k_stacksize;
	int stkfree;

	for (sp = (int *)(kt->k_stack); (caddr_t)sp < stktop; sp++) {
		if (*sp)
			break;
	}
	stkfree = (caddr_t)sp - kt->k_stack;
	qprintf("kthread 0x%x", kt);
	qprintf(" stklen %d", kt->k_stacksize);
	qprintf(" stkfree %d", stkfree);
	if (stkfree < 16)
		qprintf(" OVERFLOW\n");
	else if (stkfree < 1024)
		qprintf(" LOW\n");
	else
		qprintf("\n");
}

/*
 * Dump list of sthreads
 */
/* ARGSUSED */
void
idbg_slist(__psint_t x)
{
	extern sthread_t sthreadlist;
	sthread_t *st;
	kthread_t *kt;

	for (st = sthreadlist.st_next; st != &sthreadlist; st = st->st_next) {
		kt = ST_TO_KT(st);
		if (x == 1) {
			kstack(kt);
			continue;
		}

		qprintf("0x%x [%d/%s] onrq %d oncpu %d pri(%d) ",
			st,
			kidtopid(kt->k_id), st->st_name,
			kt->k_onrq, kt->k_sonproc, kt->k_pri);
		if (kt->k_wchan)
			sleepon(kt);
		qprintf("\n");
	}
}

/*
 * Dump list of xthreads
 */
/* ARGSUSED */
void
idbg_xlist(__psint_t x)
{
	extern xthread_t xthreadlist;
	xthread_t *xt;
	kthread_t *kt;

	for (xt = xthreadlist.xt_next; xt != &xthreadlist; xt = xt->xt_next) {
		kt = XT_TO_KT(xt);
		if (x == 1) {
			kstack(kt);
			continue;
		}

		qprintf("0x%x [%d/%s] onrq %d oncpu %d pri(%d) ",
			xt,
			kidtopid(kt->k_id), xt->xt_name,
			kt->k_onrq, kt->k_sonproc, kt->k_pri);
		if (kt->k_wchan)
			sleepon(kt);
		qprintf("\n");
	}
}

/*
 * Dump kthread lists and map from thread id to owning process or sthread
 */
void
idbg_klist(__psint_t x)
{
	__psunsigned_t dummy;
	if (x == 0) {
		qprintf("no free pool of kthreads yet!\n");
	} else if (x < 0) {
		idbg_slist(1);
		idbg_xlist(1);
	} else {
		/* look for id */
		find_kthreadid(x, 1, &dummy);
	}
}

struct ut_test {
	uint64_t	kid;
	uthread_t	*ut;
};

int
kid_is_uthread(
	proc_t	*p,
	void	*arg,
	int	ctl)
{
	struct ut_test	*uarg = (struct ut_test *)arg;
	uthread_t	*ut;

	if (ctl == 1) {
		for (ut = p->p_proxy.prxy_threads; ut; ut = ut->ut_next) {
			if (UT_TO_KT(ut)->k_id == uarg->kid) {
				uarg->ut = ut;
				return(KID_IS_UTHREAD);
			}
		}
	}
	return(0);
}


/*
 * Search through process and sthread lists and try to find a match to the
 * passed id.
 * Returns:
 *  KID_IS_UTHREAD
 *  KID_IS_STHREAD
 *  KID_IS_ITHREAD
 *  KID_IS_XTHREAD
 *  KID_IS_UNKNOWN
 * The result field has either a proc slot or a sthread address.
 */
static int
find_kthreadid(uint64_t id, int verbose, __psunsigned_t *result)
{
	int i;
	sthread_t *st;
	xthread_t *xt;
	uthread_t *ut;
	struct ut_test arg;
	extern sthread_t sthreadlist;
	extern xthread_t xthreadlist;

	if (verbose)
	    qprintf("threadid %lld", id);
	/* look through processes first */
	arg.kid = id;
	i = idbg_procscan(kid_is_uthread, &arg);
	if (i == KID_IS_UTHREAD) {
		ut = arg.ut;
		if (verbose)
			qprintf(" is at uthread addr 0x%x\n", ut);
		*result = (__psunsigned_t)ut;
		return(KID_IS_UTHREAD);
	}
		
	/*
	 * now check sthreads
	 */
	for (st = sthreadlist.st_next; st != &sthreadlist; st = st->st_next) {
		if (ST_TO_KT(st)->k_id == id) {
			if (verbose)
			    qprintf(" is sthread @ 0x%x\n", st);
			*result = (__psunsigned_t)st;
			return KID_IS_STHREAD;
		}
	}

	/*
	 * now check xthreads
	 */
	for (xt = xthreadlist.xt_next; xt != &xthreadlist; xt = xt->xt_next) {
		if (XT_TO_KT(xt)->k_id == id) {
			if (verbose)
			    qprintf(" is xthread @ 0x%x\n", xt);
			*result = (__psunsigned_t)xt;
			return KID_IS_XTHREAD;
		}
	}

	if (verbose)
	    qprintf(" not active\n");
	return KID_IS_UNKNOWN;
}

char	*tab_kflags[] = {
	"STK_MALLOC",	/* 0x00000001 */
	"W-MUTEX",	/* 0x00000002 */
	"LOCK",		/* 0x00000004 */
	"WACC",		/* 0x00000008 */
	"STARVE",	/* 0x00000010 */
	"W-SV",		/* 0x00000020 */
	"WSVQ",		/* 0x00000040 */
	"INDIRECTQ",	/* 0x00000080 */
	"W-SEMA",	/* 0x00000100 */
	"W-MRLOCK",	/* 0x00000200 */
	"W-ISEMA",	/* 0x00000400 */
	"WUPD",		/* 0x00000800 */
	"PS",	 	/* 0x00001000 */
	"BIND",		/* 0x00002000 */
	"NPRMPT",	/* 0x00004000 */
	"BPS", 		/* 0x00008000 */
	"NBPRMPT",	/* 0x00010000 */
	"PSMR",		/* 0x00020000 */
	"HOLD",		/* 0x00040000 */
	"PSPIN",	/* 0x00080000 */
	"INTERRUPTED",	/* 0x00100000 */
	"SLEEP",	/* 0x00200000 */
	"NWAKE",	/* 0x00400000 */
	"LTWAIT",	/* 0x00800000 */
	"BHVINTR",	/* 0x01000000 */
	"SERVER",       /* 0x02000000 */
	"NOAFF",        /* 0x04000000 */
	"MUTEX_INHERIT",/* 0x08000000 */
	0
};

void
idbg_kthread(__psint_t x)
{
	kthread_t *kt;
	int *sp;
	mri_t *mrip;
	mria_t *mriap;

	if (x == -1L) {
		kt = private.p_curkthread;
		if (kt == NULL) {
			qprintf("no current kthread\n");
			return;
		}
	} else
		kt = (kthread_t *)x;

	qprintf("kthread @0x%x id %lld\n", kt, kt->k_id);
	qprintf(" bitlock/flags @0x%x:", &kt->k_flags);
	printflags((unsigned)kt->k_flags, tab_kflags, 0);
	qprintf("\n");
	qprintf(" type %d ops 0x%x\n", kt->k_type, kt->k_ops);
	qprintf(" sonproc %d onrq %d cpuset %d link 0x%x\n",
	     kt->k_sonproc, kt->k_onrq, kt->k_cpuset, kt->k_link);
	qprintf(" mustrun %d binding %d lastrun %d\n",
	     kt->k_mustrun, kt->k_binding, kt->k_lastrun);
#ifdef NUMA_BASE
	{
	int	j;
	qprintf(" mld: 0x%llx, affnode %d\n", kt->k_mldlink, kt->k_affnode);
	qprintf(" maffbv: [");
	for (j=0; j<CNODEMASK_SIZE; j++)
		qprintf(" 0x%llx ", CNODEMASK_WORD(kt->k_maffbv, j));
	qprintf("],  nodemask: [");
	for (j=0; j<CNODEMASK_SIZE; j++)
		qprintf(" 0x%llx ", CNODEMASK_WORD(kt->k_nodemask, j));
	qprintf("]\n");
	}
#endif
	qprintf(" flink 0x%x blink 0x%x\n", kt->k_flink, kt->k_blink);
	qprintf(" rflink 0x%x rblink 0x%x\n", kt->k_rflink, kt->k_rblink);
	qprintf(" ");
	sleepon(kt);
	qprintf("w2chan 0x%x qkey %d\n", kt->k_w2chan, kt->k_qkey);
	qprintf(" prtn %d inherit 0x%x pri %d basepri %d copri %d sqself %d\n",
		kt->k_prtn, kt->k_inherit, kt->k_pri, kt->k_basepri,
		kt->k_copri, kt->k_sqself);
	qprintf(" stk 0x%x stklen %d",
		kt->k_stack, kt->k_stacksize);
	for (sp = (int *)(kt->k_stack);
			(caddr_t)sp < (kt->k_stack + kt->k_stacksize); sp++)
		if (*sp)
			break;
	qprintf(" stkfree %d\n", (caddr_t)sp - kt->k_stack);
	qprintf(" monitor 0x%x eframe 0x%x nofault %d timewait @0x%x\n",
		kt->k_monitor, kt->k_eframe, kt->k_nofault,
		&kt->k_timewait);
	qprintf(" activemonpp 0x%x\n", kt->k_activemonpp);
	printtimers(kt);
	qprintf(" indirectwait 0x%x binding %d\n",
		kt->k_indirectwait, kt->k_binding);

	qprintf(" preempted %d \n", kt->k_preempt);
#if CELL
	qprintf(" k_bla @0x%x\n", &kt->k_bla);
#endif
	qprintf(" Held mrlocks k_mria 0x%x", &kt->k_mria);
#ifdef DEBUG
	qprintf(" Total Held %d\n", kt->k_mria.mria_count);
#else
	qprintf("\n");
#endif
	for (mrip = kt->k_mria.mria_mril;
		mrip < &kt->k_mria.mria_mril[MRI_MAXLINKS]; mrip++)
	{
		if (mrip->mri_mrlock)
			qprintf("     mrlock 0x%x count %d\n",
				mrip->mri_mrlock, mrip->mri_count);
	}

	for (mriap = kt->k_mria.mria_next; mriap; mriap = mriap->mria_next) {
		qprintf(" OVERFLOW mria 0x%x", mriap);
#ifdef DEBUG
		qprintf(" Held in mria %d\n", mriap->mria_count);
#else
		qprintf("\n");
#endif
		for (mrip = mriap->mria_mril;
			mrip < &mriap->mria_mril[MRI_MAXLINKS]; mrip++)
		{
			if (mrip->mri_mrlock)
				qprintf("     mrlock 0x%x count %d\n",
					mrip->mri_mrlock, mrip->mri_count);
		}
	}
}

void
idbg_sthread(__psint_t x)
{
	kthread_t *kt;
	sthread_t *st;

	if (x == -1L) {
		kt = private.p_curkthread;
		if (kt == NULL || !KT_ISSTHREAD(kt)) {
			qprintf("no current sthread\n");
			return;
		}
		st = KT_TO_ST(kt);
	} else {
		st = (sthread_t *)x;
		kt = ST_TO_KT(st);
	}

	if (!KT_ISSTHREAD(kt)) {
		qprintf("not an sthread\n");
		return;
	}
	qprintf("sthread @0x%x [%s] ktid %lld\n", st, st->st_name, kt->k_id);
	qprintf(" cred 0x%x next 0x%x prev 0x%x\n",
		st->st_cred, st->st_next, st->st_prev);
	if (ST_TO_KT(st)->k_wchan)
		sleepon(ST_TO_KT(st));
	qprintf("\n");
	printtimers(kt);
}

void
idbg_xthread(__psint_t x)
{
	kthread_t *kt;
	xthread_t *xt;

	if (x == -1L) {
		kt = private.p_curkthread;
		if (kt == NULL || !KT_ISXTHREAD(kt)) {
			qprintf("no current xthread\n");
			return;
		}
		xt = KT_TO_XT(kt);
	} else {
		xt = (xthread_t *)x;
		kt = XT_TO_KT(xt);
	}

	if (!KT_ISXTHREAD(kt)) {
		qprintf("not an xthread\n");
		return;
	}
	qprintf("xthread @0x%x [%s] ktid %lld", xt, KT_TO_XT(kt)->xt_name, kt->k_id);
	qprintf(" cred 0x%x next 0x%x prev 0x%x\n",
		xt->xt_cred, xt->xt_next, xt->xt_prev);
	if (XT_TO_KT(xt)->k_wchan)
		sleepon(XT_TO_KT(xt));
	qprintf("\n");
	qprintf(" restart function 0x%lx restart argument 0x%lx arg2 0x%lx\n",
		xt->xt_func, xt->xt_arg, xt->xt_arg2);
	qprintf(" restart stack ptr 0x%lx\n", xt->xt_sp);
#ifdef ITHREAD_LATENCY
	if (xt->xt_latstats != NULL)
		xthread_print_latstats(xt->xt_latstats);
#endif
#if CELL
	if (xt->xt_info)
		qprintf(" info: 0x%lx \n", xt->xt_info);
#endif
}

/* ARGSUSED */
int
print_proc(
	proc_t	*p,
	void	*arg,
	int	ctl)
{
	if (ctl == 1)
		shortproc(p, 0);
	return(0);
}

/*
 * Dump the active and free process lists
++
++	kp plist which - dump active or free process list
++		which = -1 (no args)  - dump active process list
++		which > 0 lookup pid <which>
 */
void
idbg_plist(__psint_t x)
{
	proc_t *pp;

	if (x < 0) {
		qprintf("active process list:\n");
		idbg_procscan(print_proc, 0);
	} else {
		/* see if pid is active */
		pp = idbg_pid_to_proc(x);

		if (pp != NULL)
			qprintf("pid %d in proc 0x%x\n", x, pp);
		else
			qprintf("pid %d is not active\n", (pid_t)x);
	}
}

/* ARGSUSED */
int
print_proc_fdt(
	proc_t	*p,
	void	*arg,
	int	ctl)
{
	if (ctl == 1) {
		qprintf("proc 0x%x pid %d ", p, p->p_pid);
		qprintf("\"%s\" ", p->p_comm);
		if (p->p_fdt != NULL)
			qprintf("fdt 0x%x nofiles %d\n",
				p->p_fdt, 
				p->p_fdt->fd_nofiles);
		else
		      qprintf("fdt NULL (sproc)\n");
	}
	return(0);
}

/*
 * Process file descriptor table info.
++
++	kp plfdt which - process fdt info
++		which = -1 (no args)  - dump active process list
++		which > 0 lookup pid <which>
 */
void
idbg_plfdt(__psint_t x)
{
	if (x < 0) {
		qprintf("active process list:\n");
		idbg_procscan(print_proc_fdt, 0);
	} else {
		proc_t *p;

		/* see if pid is active */
		p = idbg_pid_to_proc(x);
		if (p != NULL)
			print_proc_fdt(p, 0, 1);
		else
			qprintf("pid %d is not active\n", (pid_t)x);
	}
}


/*
 * Print info about specified process then recurse through its children
 */
static void
showptree(proc_t *parent, int level)
{
	int i;
	proc_t *child;
	child_pidlist_t *cpid;

	for (i = 0;  i < level;  ++i)
		qprintf("  ");
	tinyproc(parent);

	for (cpid = parent->p_childpids; cpid; cpid = cpid->cp_next) {
		if (child = idbg_pid_to_proc(cpid->cp_pid))
			showptree(child, level+1);
	}
}

/*
 * Dump	a list of all children for a given process
 *	kp ptree <which>
 *		<which> < 0: specifies address of parent proc
 *		<which> > 0: specifies PID of parent proc
 *
 */
void
idbg_ptree(__psint_t arg)
{
	proc_t *p;

	if (arg < 0) {
		p = (proc_t *) arg;
	}
	else if (arg > 0) {

		p = idbg_pid_to_proc((pid_t) arg);
		if (p == NULL) {
			qprintf("pid %d is not active\n", (pid_t)arg);
			return;
		}
	}
	else {
		qprintf("must specify parent process\n");
		return;
	}

	showptree(p, 1);
}


/*
 * Dump	the process group
 *	kp pgrp
 */
void
idbg_pgrp(__psint_t x)
{
	proc_t *pp;
	struct pgrp *pg = (struct pgrp *) x;
	qprintf("pgrp 0x%x:\n", x);
	qprintf("    memcnt %d jclcnt %d sigseq %d\n",
		 pg->pg_memcnt, pg->pg_jclcnt, pg->pg_sigseq);
	qprintf("    &lockmode 0x%x &lock 0x%x\n",
		 &pg->pg_lockmode, &pg->pg_lock);
	pp = pg->pg_chain;
	while (pp != 0) {
		if (!procp_is_valid(pp)) {
			qprintf("WARNING:invalid process at 0x%x\n", pp);
		}
		shortproc(pp, 0);
		pp = pp->p_pgflink;
	}
}

/* ARGSUSED */
int
gdbg_proc_print(proc_t *p, void *arg, int ctl)
{
	int	gfxthds=0, v=(__psint_t)arg;

	if (ctl == 1) {
		uthread_t *ut;

		for (ut = p->p_proxy.prxy_threads; ut; ut = ut->ut_next) {
			if (UT_TO_KT(ut)->k_runcond & RQF_GFX) {
				gfxthds++;
				break;
			}
		}
		if (gfxthds) {
			if (gfxthds>1) qprintf("GFX %d> ",gfxthds);
			else           qprintf("GFX> ");
			shortproc(p, v);
		}
	}
	return(0);
}

/* ARGSUSED */
int
gdbg_owner_print(proc_t *p, void *arg, int ctl)
{
	if (ctl == 1) {
		uthread_t *ut, *owner;

		owner = (uthread_t *)arg;
		for (ut = p->p_proxy.prxy_threads; ut; ut = ut->ut_next) {
			if (UT_TO_KT(ut)->k_runcond & RQF_GFX) {
				if (owner == ut) {
				    shortproc(p, 0);
				    return(1);
				}
			}
		}
	}
	return(0);
}

char *tab_asflags[] = {
	"SUSPEND",	/* 0x0001 */
	"NONEW",	/* 0x0002 */
	0
};

/*
 * Dump process aggregate info
 *	kp pagg <which>
 *		where <which> < -1 - use addr as address of vpagg struct
 *			      = -1 - dump pagg containing current process
 *			      >= 0 - dump pagg for specified handle
 */
void
idbg_pagg(__psint_t arg)
{
	vpagg_t	*vpag;
	extern vpagg_t *vpag_lookup_nolock(paggid_t);
	ppag_t *ppag;

	if (arg == -1L) {
		proc_t *pp;

		pp = curprocp;
		if (pp == NULL) {
			qprintf("no current process\n");
			return;
		}
		vpag = pp->p_vpagg;
	}
	else if (arg >= 0) {
		vpag = vpag_lookup_nolock((ash_t) arg);
	} else {
		vpag = (vpagg_t *) arg;
	}

	if (vpag == 0) {
		qprintf("Process aggregate not available\n");
		return;
	}

	qprintf("Process aggregate: ");
	qprintf("addr 0x%x   refcnt %d\n",
		vpag, vpag->vpag_refcnt);
	ppag = BHV_TO_PPAG(vpag->vpag_bhvh.bh_first);
	qprintf("    handle 0x%llx   pid %d\n",
		vpag->vpag_paggid,
		ppag->ppag_pid);
	qprintf("  vm pool 0x%x rss %d smem %d rmem %d rss_limit %d rssmax %d\n", 
			ppag->ppag_vm_resource.vm_pool, 
			ppag->ppag_vm_resource.rss, 
			ppag->ppag_vm_resource.smem, 
			ppag->ppag_vm_resource.rmem, 
			ppag->ppag_vm_resource.rss_limit,
			ppag->ppag_vm_resource.maxrss); 
	qprintf("    prid %lld   nice %d\n",
		ppag->ppag_prid,
		ppag->ppag_nice);
	qprintf("    start %x   ticks %x", ppag->ppag_start, ppag->ppag_ticks);
	qprintf("\n    next 0x%x   prev 0x%x\n", vpag->vpag_next, vpag->vpag_prev);
	_printflags((unsigned) ppag->ppag_flag, tab_asflags, "    flags");
	qprintf("\n        service provider info (length %d):\n",
		ppag->ppag_spilen);
	if (ppag->ppag_spi == NULL) {
		qprintf("            <none/default>\n");
	}
	else {
		int i ;

		for (i = 0;  i < ppag->ppag_spilen;  ++i) {
			if (i % 16 == 0) {
				if (i > 0) {
					qprintf("\n");
				}
				qprintf("            ");
			}
			else if (i % 4  == 0) {
				qprintf(" ");
			}

			qprintf("%02x", ppag->ppag_spi[i]);
		}
		qprintf("\n");
	}
	qprintf("        user time:       %llx\n", ppag->ppag_timers.ac_utime);
	qprintf("        system time:     %llx\n", ppag->ppag_timers.ac_stime);
	qprintf("        block I/O wait:  %llx\n", ppag->ppag_timers.ac_bwtime);
	qprintf("        raw I/O wait:    %llx\n", ppag->ppag_timers.ac_rwtime);
	qprintf("        run queue wait:  %llx\n", ppag->ppag_timers.ac_qwtime);
	qprintf("        mem usage:       %llx\n", ppag->ppag_counts.ac_mem);
	qprintf("        swaps:           %llx\n", ppag->ppag_counts.ac_swaps);
	qprintf("        bytes read:      %llx\n", ppag->ppag_counts.ac_chr);
	qprintf("        bytes written:   %llx\n", ppag->ppag_counts.ac_chw);
	qprintf("        blocks read:     %llx\n", ppag->ppag_counts.ac_br);
	qprintf("        blocks written:  %llx\n", ppag->ppag_counts.ac_bw);
	qprintf("        read syscalls:   %llx\n", ppag->ppag_counts.ac_br);
	qprintf("        write syscalls:  %llx\n", ppag->ppag_counts.ac_bw);
	qprintf("        disk usage:      %llx\n", ppag->ppag_counts.ac_disk);
}

/*
 * Print info about specified process then recurse through any children
 * in the same process aggregate
 */
static void
showpagtree(proc_t *root, proc_t *parent, int level)
{
	int i;
	proc_t *child;
	child_pidlist_t *cpid;

	if (parent->p_vpagg == root->p_vpagg) {
		for (i = 0;  i < level;  ++i)
			qprintf("  ");
		tinyproc(parent);
	}

	for (cpid = parent->p_childpids; cpid; cpid = cpid->cp_next) {
		if (child = idbg_pid_to_proc(cpid->cp_pid))
			showptree(child, level+1);
	}
}

/*
 * Dump	a list of all processes in  process aggregate
 *	kp pagtree <which>
 *		<which> < 0: specifies address of pagg struct
 *		<which> > 0: specifies process aggregate handle
 *
 */
void
idbg_pagtree(__psint_t arg)
{
	vpagg_t *vpag;
	proc_t *p;
	static arsess_t as;	/* struct is too big for stack. idbg cant allocate/free */
	int rele = 0;

	if (arg < 0) {
		vpag = (vpagg_t *) arg;
	}
	else if (arg > 0) {
		vpag = VPAG_LOOKUP((paggid_t) arg);
		rele = 1;
	}
	else {
		qprintf("must specify process aggregate\n");
		return;
	}

	VPAG_EXTRACT_ARSESS_INFO(vpag, &as);
	if(rele)
		VPAG_RELE(vpag);
	p = idbg_pid_to_proc(as.as_pid);
	if (p == NULL) {
		qprintf("Can't find top proc in process aggregate %lld (pid %d)\n",
			as.as_handle, as.as_pid);
		return;
	}

	showpagtree(p, p, 1);
}

/*
 * Dump the active and process aggregate lists
++
++	kp paglist <which>
++		which = -1 (no args)  - dump active process aggregate list
++		which > 0 - list the process aggregate session for the specified *PID*
 */
void
idbg_paglist(__psint_t arg)
{
	vpagg_t *vpag;
	extern vpagg_t *vpagactlist;	/* active arsess list */

	if (arg > 0) {
		proc_t * p;

		if ((p = idbg_pid_to_proc((pid_t) arg)) == NULL)
			return;
		vpag = p->p_vpagg;

		if (vpag)
			shortvpag(vpag);
		else
		    qprintf("Process aggregate handle %d not found\n", arg);
		return;
	}

	if (arg < 0) {
		qprintf("active process aggregate list:\n");
		vpag = vpagactlist;
	} else 	
		return;

	while (vpag != NULL) {
		shortvpag(vpag);
		vpag = vpag->vpag_next;
	}
}


int
proc_pagfind(
	proc_t	*p,
	void	*arg,
	int	ctl)
{
	vpagg_t *vpag;

	if (ctl == 1) {
		vpag = (vpagg_t *) arg;
		if (p->p_vpagg == vpag)
			tinyproc(p);
	}
	return(0);
}

/*
 * Search for all processes in a specified process aggregate
++
++	kp pagfind <which>
++		which < 0 - <which> specifies the address of a pagg
++		which > 0 - <which> specifies an paggid
 */
void
idbg_pagfind(__psint_t arg)
{
	vpagg_t *vpag;
	int rele = 0;

	if (arg < 0) {
		vpag = (vpagg_t *) arg;
	}
	else if (arg > 0) {
		vpag = VPAG_LOOKUP((paggid_t) arg);
		rele = 1;
	}
	else {
		qprintf("must specify a process aggregate\n");
		return;
	}

	idbg_procscan(proc_pagfind, vpag);
	if(rele)
		VPAG_RELE(vpag);
	return;
}

static void
shortuthread(uthread_t *ut)
{
	qprintf("   thrd 0x%x [%d], flags: ", ut, ut->ut_id);
#ifdef NUMA_BASE
	qprintf(" affnode: %d, lastrun: %d ", UT_TO_KT(ut)->k_affnode, UT_TO_KT(ut)->k_lastrun);
#endif
	_printflags((unsigned)ut->ut_flags, tab_utflags, 0);
	_printflags((unsigned)ut->ut_update, tab_utupdate, 0);
	if (UT_TO_KT(ut)->k_wchan) {
		qprintf("\n      ");
		sleepon(UT_TO_KT(ut));
		qprintf("\n");
	} else {
		qprintf(" [no wchan]\n");
	}
}

void
shortproc(proc_t *pp, int ohboy)
{
	uthread_t *ut;

	if (ohboy) {
		qprintf("proc: addr 0x%x vproc 0x%x\n", pp, PROC_TO_VPROC(pp));
		qprintf(" pid %d ppid %d uid %d pgid %d",
			pp->p_pid, pp->p_ppid,
			pp->p_cred ? pp->p_cred->cr_uid : -1, pp->p_pgid);
	} else {
		qprintf("\"%s\" [%d] @ 0x%x", pp->p_comm, pp->p_pid, pp);
	}
	qprintf(" flags: ");
	_printflags((unsigned)pp->p_flag, tab_pflag, 0);
	qprintf(" %s\n", tab_pstat[pp->p_stat]);

	if (ohboy)
		idbg_proxy(&pp->p_proxy);

	if (pp->p_stat == NULL)
		return;

	if (ohboy < 2)
		for (ut = pp->p_proxy.prxy_threads; ut; ut = ut->ut_next)
			shortuthread(ut);
}

static void
tinyproc(proc_t *p)
{
	qprintf("Pid %d ", p->p_pid);
	qprintf("Cmd \"%s\" Args \"%s\"\n",
		    p->p_comm, p->p_psargs);
}

static void
shortvpag(vpagg_t *vpag)
{
	if (vpag == 0) {
		qprintf("<Vpagg missing>\n");
		return;
	}

	qprintf("Vpagg paggid %lld", vpag->vpag_paggid);
	qprintf("  prid %lld  at 0x%x", VPAG_GETPRID(vpag), vpag);
	qprintf("\n");
}


char *tab_wflag[] = {
	"wstep",	/* 0x00000001 */
	"winsys",	/* 0x00000002 */
	"wsetstep",	/* 0x00000004 */
	"wintsys",	/* 0x00000008 */
	"wign",		/* 0x00000010 */
	"",
	"",
	"",
	0
};

/*
 * SVR4 ABI signal list
 */

char *_sys_signames[NSIG] = {
		0,
/*  1 */	"HUP",
/*  2 */	"INT",
/*  3 */	"QUIT",
/*  4 */	"ILL",
/*  5 */	"TRAP",
/*  6 */	"ABRT",
/*  7 */	"EMT",
/*  8 */	"FPE",
/*  9 */	"KILL",
/* 10 */	"BUS",
/* 11 */	"SEGV",
/* 12 */	"SYS",
/* 13 */	"PIPE",
/* 14 */	"ALRM",
/* 15 */	"TERM",
/* 16 */	"USR1",
/* 17 */	"USR2",
/* 18 */	"CLD",
/* 19 */	"PWR",
/* 20 */	"WINCH",
/* 21 */	"URG",
/* 22 */	"POLL",
/* 23 */	"STOP",
/* 24 */	"TSTP",
/* 25 */	"CONT",
/* 26 */	"TTIN",
/* 27 */	"TTOU",
/* 28 */	"VTALRM",
/* 29 */	"PROF",
/* 30 */	"XCPU",
/* 31 */	"XFSZ",
/* 32 */	"USR32",
#ifdef CKPT
/* 33 */	"CKPT",
/* 34 */	"RESTART",
#else
/* 33 */	"USR33",
/* 34 */	"USR34",
#endif
/* 35 */	"USR35",
/* 36 */	"USR36",
/* 37 */	"USR37",
/* 38 */	"USR38",
/* 39 */	"USR39",
/* 40 */	"USR40",
/* 41 */	"USR41",
/* 42 */	"USR42",
/* 43 */	"USR43",
/* 44 */	"USR44",
/* 45 */	"USR45",
/* 46 */	"USR46",
/* 47 */	"USR47",
/* 48 */	"USR48",
/* 49 */	"RTMIN",
/* 50 */	"RT2",
/* 51 */	"RT3",
/* 52 */	"RT4",
/* 53 */	"RT5",
/* 54 */	"RT6",
/* 55 */	"RT7",
/* 56 */	"RT8",
/* 57 */	"RT9",
/* 58 */	"RT10",
/* 59 */	"RT11",
/* 60 */	"RT12",
/* 61 */	"RT13",
/* 62 */	"RT14",
/* 63 */	"RT15",
/* 64 */	"RTMAX"
};

static void
prksigset(char *str, k_sigset_t *ks)
{
	int i, j = 5;

#if	(_MIPS_SIM != _ABIO32)
	qprintf("  %s 0x%x: 0x%llx", str, ks, *ks);
#else
	qprintf("  %s 0x%x: 0x%x 0x%x", str, ks, ks->sigbits[0], ks->sigbits[1]);
#endif
	for (i = 1; i < NSIG; i++) {
		if (sigismember(ks, i)) {
			if ((j++)%12 == 0)
				qprintf("\n\t");
			qprintf(" %s", _sys_signames[i]);
		}
	}
	qprintf("\n");
}

void
idbg_do_signal(struct uthread_s *ut)
{
	/*
	 * Print out proc stuff if this is the first uthread.
	 */
	if (!ut->ut_prev) {
		proc_t *pp = UT_TO_PROC(ut);
		proc_proxy_t *prxy = ut->ut_pproxy;

		qprintf(" proc 0x%x,", pp);
		qprintf("   ptrace 0x%x,", pp->p_trace);
		prksigset(" traced: ", &pp->p_sigtrace);

		qprintf("  sigvec structure @ 0x%x, mrlock @ 0x%x flags 0x%x\n",
			&pp->p_sigvec, &pp->p_sigvec.sv_lock,
			pp->p_sigvec.sv_flags);

		prksigset(" sv_sig", &pp->p_sigvec.sv_sig);
		qprintf("   &hndlr 0x%x &sigmasks 0x%x sigqueue 0x%x\n",
			pp->p_sigvec.sv_hndlr, pp->p_sigvec.sv_sigmasks,
			pp->p_sigvec.sv_sigqueue);
		prksigset(" ignore ", &pp->p_sigvec.sv_sigign);
		prksigset(" catch  ", &pp->p_sigvec.sv_sigcatch);
		prksigset(" hold   ", &pp->p_sigvec.sv_sighold);
		prksigset(" sainfo ", &pp->p_sigvec.sv_sainfo);
		prksigset(" restrt ", &pp->p_sigvec.sv_sigrestart);
		prksigset(" ndefer ", &pp->p_sigvec.sv_signodefer);
		prksigset(" rsthnd ", &pp->p_sigvec.sv_sigresethand);

		prksigset(" onstk  ", &prxy->prxy_sigonstack);
		qprintf("  tramp 0x%x oldctxt 0x%x, siglb\n",
			prxy->prxy_sigtramp, prxy->prxy_oldcontext,
			prxy->prxy_siglb);
		qprintf("  ssflags 0x%x sigsp 0x%x spsize 0x%x\n",
			(int)prxy->prxy_ssflags, prxy->prxy_sigsp,
			(int)prxy->prxy_spsize);
	}

	qprintf("  ut 0x%x cursig %d curinfo 0x%x ut_sigqueue 0x%x\n",
		ut, ut->ut_cursig, ut->ut_curinfo, ut->ut_sigqueue);
	prksigset(" sigwait ", &ut->ut_sigwait);
	prksigset(" ut_sig  ", &ut->ut_sig);
	prksigset(" ut_held ", ut->ut_sighold);
	prksigset(" suspmsk ", &ut->ut_suspmask);
}

/*
++
++	kp signal <arg> - dump signal information about a process/thread
++		arg < 0 - use arg as address of uthread struct
++
*/
void
idbg_signal(__psint_t p1)
{
	(void) idbg_doto_uthread(p1, idbg_do_signal, 1, 0);
}

void
idbg_ptrace(__psint_t p1)
{
	int fstype;
	struct vnode *vp = (struct vnode *)p1;

	(void) idbg_vnode((__psint_t)vp);
	if (vp->v_vfsp) {
		fstype = vp->v_vfsp->vfs_fstype;
		if ((vfssw[fstype].vsw_name) &&
		    ((strcmp(vfssw[fstype].vsw_name, "dbg") == 0) ||
				(strcmp(vfssw[fstype].vsw_name, "proc") == 0)))
			(void) idbg_prnode((__psint_t)BHVTOPRNODE(vp->v_fbhv));
	}
}

char *tab_prtypes[] = {
        "PR_PROCDIR",
        "PR_PSDIR",
        0
};

void
idbg_prnode(__psint_t p1)
{
	struct prnode *pnp = (struct prnode *)p1;
	prnodetype_t prtype = pnp->pr_type;
	struct pollhead *php;

	if (pnp != NULL) {
		qprintf("  pr_free 0x%x pr_next 0x%x pr_vnode 0x%x pr_proc 0x%x\n",
			pnp->pr_free, pnp->pr_next,
			pnp->pr_vnode, pnp->pr_proc);
		qprintf("  pr_tpid %d pr_mode 0x%x pr_opens %d pr_writers %d pr_lock 0x%x\n",
			pnp->pr_tpid, pnp->pr_mode, pnp->pr_opens,
			pnp->pr_writers, &pnp->pr_lock);
		qprintf("  pr_pflags %s %s pr_type %s\n",
			pnp->pr_pflags & PREXCL ? "PREXCL" : "",
			pnp->pr_pflags & PRINVAL ? "PRINVAL" : "",
			tab_prtypes[prtype]);
		if (php = pnp->pr_pollhead) {
			qprintf("  &pr_pollhead 0x%x, ph_list 0x%x, "
				"ph_gen %d\n",
				php, php->ph_list, POLLGEN(php));
		}
		if (pnp->pr_next) {
			qprintf("\n");
			idbg_ptrace((__psint_t)pnp->pr_next->pr_vnode);
		}
	}
}


void
idbg_prnodefree(__psint_t p)
{
	struct prnode *pnp;
	int            cnt = 0;

	extern zone_t *procfs_trzone;
	extern lock_t  procfs_lock;
	extern struct  prnode *procfs_freelist,procfs_root,procfs_pinfo;
	extern int     procfs_nfree;
	extern int     procfs_refcnt;
#ifdef PRCLEAR
	extern int     prnode_pidmismatch_cnt;
#endif

	if (p != -1L) {
		qprintf("usage: prnodefree\n");
		return;
	}

	qprintf("  procfs_trzone 0x%x procfs_lock 0x%x\n",
			procfs_trzone,procfs_lock);
	qprintf("  procfs_freelist 0x%x procfs_nfree %d procfs_refcnt %d\n",
			procfs_freelist,procfs_nfree,procfs_refcnt);
	qprintf("  procfs_root 0x%x   procfs_pinfo 0x%x\n",
			procfs_root,procfs_pinfo);

	for (pnp = procfs_freelist ; pnp != NULL ; pnp = pnp->pr_free,cnt++) {
		qprintf("[%d] prnode %x\n",cnt,pnp);
		qprintf("  pr_free 0x%x pr_next 0x%x",
				pnp->pr_free,pnp->pr_next);
		qprintf("  pr_tpid %d pr_lock 0x%x\n",
				pnp->pr_tpid,pnp->pr_lock);
#ifdef PRCLEAR
		qprintf("  prnode_pidmismatch_cnt %d\n",
			prnode_pidmismatch_cnt);
		if (pnp->pr_proc != NULL || pnp->pr_listproc != NULL)
			qprintf("  pr_proc 0x%x pr_listproc 0x%x\n",
				pnp->pr_proc,pnp->pr_listproc);
#else
		if (pnp->pr_proc != NULL)
			qprintf("  pr_proc 0x%x \n",pnp->pr_proc);
#endif
#ifdef PRNODE_DEBUG
		if (pnp->pr_magic != PRNODE_MAGIC)
			qprintf("invalid MAGIC\n");
		qprintf("  magic %x stat %x\n",pnp->pr_magic,pnp->pr_stat);
#endif
	}

	if (cnt != procfs_nfree)
		qprintf("WARNING: cnt %d procfs_nfree %d\n",cnt,procfs_nfree);
}
/*
++
++	kp watch arg
++		arg == -1 : prints watchpoint structure for curproc
++		arg < 0  : prints watchpoint structure at addr
++		arg >= 0 : prints watchpoint structure for proc with pid <arg>
++
*/
void
idbg_watch(__psint_t p1)
{
	register pwatch_t *pw;
	register watch_t *wp;
	proc_t *pp;

	if (p1 == -1L) {
		if (curprocp == NULL) {
			qprintf("no current process\n");
			return;
		}
		p1 = (__psint_t)curuthread->ut_watch;
	}
	if (p1 < 0) {
		pw = (pwatch_t *)p1;
	} else {
		if ((pp = idbg_pid_to_proc((pid_t)p1)) == NULL) {
			qprintf("%d is not an active pid\n", (pid_t)p1);
			return;
		}
		if ((pw = pp->p_proxy.prxy_threads->ut_watch) == NULL) {
			qprintf(
			"proc 0x%x (pid %d) does not have a watchpoint block\n",
				pp, p1);
			return;
		}
	}
	qprintf("watchpoint list @ 0x%x skip 0x%x skip2 0x%x flags: ",
			pw, pw->pw_skipaddr, pw->pw_skipaddr2);
	_printflags(pw->pw_flag, tab_wflag, 0);
	qprintf("\n");
	qprintf("   curmode 0x%x curaddr 0x%x cursize 0x%x skippc 0x%x\n",
			pw->pw_curmode, pw->pw_curaddr,
			pw->pw_cursize, pw->pw_skippc);
	qprintf("   firstsys mode 0x%x addr 0x%x size 0x%x\n",
			pw->pw_firstsys.w_mode,
			pw->pw_firstsys.w_vaddr,
			pw->pw_firstsys.w_length);
	for (wp = pw->pw_list; wp; wp = wp->w_next) {
		qprintf("\tvaddr 0x%x len 0x%x mode 0x%x\n",
			wp->w_vaddr, wp->w_length, wp->w_mode);
	}
}

int
print_sleepers(
	proc_t	*p,
	void	*arg,
	int	ctl)
{
	int		proc_printed = 0;
	uthread_t	*ut;
	int		pmask;
	sema_t		*sp;

	if (ctl == 1) {
		pmask = (__psint_t)arg;
		for (ut = p->p_proxy.prxy_threads; ut; ut = ut->ut_next) {
			kthread_t *kt;

			kt = UT_TO_KT(ut);

			if (!(kt->k_flags & KT_SLEEP))
				continue;

			sp = (sema_t *)kt->k_wchan;
			if ((pmask & 0x002) &&
			    sp == (sema_t *)&p->p_wait)
				continue;

			if ((pmask & 0x008) &&
			    (sp >= &slpsvs[0]) &&
			    (sp < slpsv_end))
				continue;

			if (proc_printed == 0) {
				proc_printed = 1;
				qprintf("proc 0x%x[%d] (\"%s\")\n",
					p, p->p_pid, p->p_comm);
			}
			qprintf("   thread 0x%x [%d] ",
				ut, ut->ut_id);
			sleepon(kt);

			if (kt->k_w2chan)
				qprintf(" w2chan 0x%x\n", kt->k_w2chan);
			else
			if (ut->ut_flags & UT_SXBRK)
				qprintf(" -- sxbrk\n");
			else
				qprintf("\n");
		}
	}
	return(0);
}

/*
++
++	kp slpproc
++		prints summary of all processes whose p_stat == SSLEEP
++		x < -1 -> use abs(x) as a mask of processes to ignore
++			0x002 - ignore processes sleeping in wait()
++			0x004 - ignore procs w/o upages;
++			0x008 - ignore proces on a 'sleep' sema
++
*/
void
idbg_slpproc(__psint_t x)
{
	__psint_t pmask;

	if (x < -1)
		pmask = -x;
	else
		pmask = 0;

	idbg_procscan(print_sleepers, (void *)pmask);
}

/*
++	kp prilist
++		Dump the sorted priority table used during process
++		paging.
*/

void
idbg_prilist()
{
	extern	prilist_t *prilist;

	qprintf("Sorted paging priority list (first to last scanned)\n");
	idbg_priscan(prilist);
}

static void
idbg_priscan(register prilist_t *mp)
{
	if (mp->right != 0)
		idbg_priscan(mp->right);
	qprintf("  0x%x pri %d state %d slptime %d\n",
			mp->vas, mp->sargs.as_shakepage_pri,
			mp->sargs.as_shakepage_state,
			mp->sargs.as_shakepage_slptime);
	if (mp->left != 0)
		idbg_priscan(mp->left);
}

/*
++
++	kp pb [0]
++	       -1: (default) prints entire printbuf
++		   (cmn_err w/ '!' as beginning)
++		0: clears out printbuf
++	       >0: print last n characters
*/
void
idbg_printputbuf(__psint_t x)
{
	extern char *putbuf;
	extern int putbufsz;
	extern int putbufndx;
	if (x == 0) {
		/* clear out putbuf */
		bzero(putbuf, putbufsz);
		putbufndx = 0;
	} else
		printputbuf(x, qprintf);
}

static void
dumpminfostats(register pda_t *pd)
{
	register struct minfo *mi = &pd->ksaptr->mi;

	qprintf("Minfo dump for cpu %d\n", pd->p_cpuid);
	qprintf("  freemem[1] %d freemem[0] %d freeswap %d palloc %d \n",
		mi->freemem[1], (unsigned long) (mi->freemem[0] & 0xffffffff),
		mi->freeswap, mi->palloc);
	qprintf("  freedpgs %d unmodsw %d unmodfl %d\n",
		mi->freedpgs, mi->unmodsw, mi->unmodfl);
	qprintf("  vfault %d demand %d cache %d swap %d file %d\n",
		mi->vfault, mi->demand, mi->cache, mi->swap, mi->file);
	qprintf("  rfault %d pfault %d steal %d cw %d\n",
		mi->rfault, mi->pfault, mi->steal, mi->cw);
	qprintf("  tfault %d tlbpids %d tlbsync %d tlbflush %d tvirt %d\n",
		mi->tfault, mi->tlbpids, mi->tlbsync, mi->tlbflush, mi->tvirt);
	qprintf("  heapmem %d hovhd %d hunused %d halloc %d hfree %d\n",
		mi->heapmem, mi->hovhd, mi->hunused, mi->halloc, mi->hfree);
	qprintf("  bsdnet %d\n", mi->bsdnet);
#if _MIPS_SIM != _ABI64
	qprintf("  iclean %u sfault %lld\n",
		mi->iclean, mi->sfault);
#else
	qprintf("  iclean %u sfault %lu\n",
		mi->iclean, mi->sfault);
#endif
}

extern int bufmem, pdcount, chunkpages, dchunkpages;
extern unsigned int scachemem, pmapmem;

/*
 * Sum up resource utilization fields in minfo (fields like the freemem,
 * pagesfreed) etc. It does not sum up the statistical count fields like
 * vfault etc..
 * Also print global stats.
 */
static void
minfo_summary(void)
{
	struct minfo *mi;
	int	i;
	long	total_freemem[2], total_freeswap, total_freedpages;
	long	total_heapmem, total_hovhd, total_hunused;
	long	total_halloc, total_hfree;
	long	total_bsdnet;

	total_freemem[0] = total_freemem[1] = total_freeswap = 0;
	total_freedpages = total_heapmem = total_hovhd = total_hunused = 0;
	total_halloc = total_hfree = total_bsdnet = 0;

	qprintf("pdcount %d chunkpages %d dchunkpages %d bufmem %d\n",
		pdcount, chunkpages, dchunkpages, bufmem);
        qprintf("global relaxed freemem %d global real freemem %d\n",
                GLOBAL_FREEMEM(), GLOBAL_FREEMEM_GET());
	qprintf("pmapmem %d scachemem %d\n", pmapmem, scachemem);
	qprintf("availrmem %d availsmem %d\n",
		GLOBAL_AVAILRMEM(), GLOBAL_AVAILSMEM());

	qprintf("minfo resource usage summary\n");
	for (i = 0; i < maxcpus; i++) {
		 if (pdaindr[i].CpuId == -1)
			continue;
		mi = &pdaindr[i].pda->ksaptr->mi;
		total_freemem[0] += mi->freemem[0];
		total_freemem[1] += mi->freemem[1];
		total_freeswap   += mi->freeswap;
		total_freedpages += mi->freedpgs;
		total_heapmem    += mi->heapmem;
		total_hovhd      += mi->hovhd;
		total_hunused    += mi->hunused;
		total_halloc     += mi->halloc;
		total_hfree      += mi->hfree;
		total_bsdnet	 += mi->bsdnet;
	}

	qprintf("  freemem[0] %d freemem[1] %d freeswap %d freedpages %d\n", 
		total_freemem[0], total_freemem[1], total_freeswap,
		total_freedpages);
	qprintf("  heapmem %d hovhd %d hunused %d halloc %d hfree %d\n",
		total_heapmem, total_hovhd, total_hunused, total_halloc, 
		total_hfree);
	qprintf("  bsdnet %d\n", total_bsdnet);
		
}

/*
++
++	kp minfo
++		dumps out minfo struct for each active processor
++
*/
void
idbg_minfo(int x)
{
	if (x == -2) {
		minfo_summary();
		return;
	}
	do_pdas(dumpminfostats, x);
	if ((x < 0) || (x > maxcpus)) {
		minfo_summary();
	}
}

static void
dumpsysinfostats(register pda_t *pd)
{
	register struct sysinfo *si = &pd->ksaptr->si;

	qprintf("Sysinfo dump for cpu %d\n", pd->p_cpuid);
	qprintf("  bread %d bwrite %d lread %d lwrite %d\n",
		si->bread, si->bwrite, si->lread, si->lwrite);
	qprintf("  phread %d phwrite %d swapin %d swapout %d\n",
		si->phread, si->phwrite, si->swapin, si->swapout);
	qprintf("  bswapin %d bswapout %d pswapout %d\n",
		si->bswapin, si->bswapout, si->pswapout);
	qprintf("  syscall %d sysread %d syswrite %d\n",
		si->syscall, si->sysread, si->syswrite);
	qprintf("  pswitch %d kswitch %d kpreempt %d\n",
		si->pswitch, si->kswitch, si->kpreempt);
	qprintf("  sysfork %d sysexec %d runque %d runocc %d\n",
		si->sysfork, si->sysexec, si->runque, si->runocc);
	qprintf("  swpque %d swpocc %d iget %d dirblk %d\n",
		si->swpque, si->swpocc, si->iget, si->dirblk);
	qprintf("  readch %d writech %d rcvint %d xmtint %d\n",
		si->readch, si->writech, si->rcvint, si->xmtint);
	qprintf("  mdmint %d rawch %d canch %d outch %d\n",
		si->mdmint, si->rawch, si->canch, si->outch);
	qprintf("  msg %d sema %d pnpfault %d wrtfault %d\n",
		si->msg, si->sema, si->pnpfault, si->wrtfault);
	qprintf("  ptc %d pts %d  intrs %d  vmeintrs %d\n",
		si->ptc, si->pts, si->intr_svcd, si->vmeintr_svcd);
}

/*
++
++	kp sysinfo
++		dumps out sysinfo struct for each active processor
++
*/
void
idbg_sysinfo(int x)
{
	do_pdas(dumpsysinfostats, x);
}

/*
 * Buffer header dump.
 */
char	*tab_bflags[] = {
	"read",		/* 0x0000000000000001 */
	"done",		/* 0x0000000000000002 */
	"error",	/* 0x0000000000000004 */
	"busy",		/* 0x0000000000000008 */
	"phys",		/* 0x0000000000000010 */
	"RESERVED_1",	/* 0x0000000000000020 */
	"wanted",	/* 0x0000000000000040 */
	"age",		/* 0x0000000000000080 */
	"async",	/* 0x0000000000000100 */
	"delwri",	/* 0x0000000000000200 */
	"open",		/* 0x0000000000000400 */
	"stale",	/* 0x0000000000000800 */
	"leader",	/* 0x0000000000001000 */
	"format",	/* 0x0000000000002000 */
	"pageio",	/* 0x0000000000004000 */
	"mapped",	/* 0x0000000000008000 */
	"swap",		/* 0x0000000000010000 */
	"bdflush",	/* 0x0000000000020000 */
	"relse",	/* 0x0000000000040000 */
	"alenlist",	/* 0x0000000000080000 */
	"partial",	/* 0x0000000000100000 */
	"uncached",	/* 0x0000000000200000 */
	"inactive",	/* 0x0000000000400000 */
	"bflush",	/* 0x0000000000800000 */
	"found",	/* 0x0000000001000000 */
	"wait",		/* 0x0000000002000000 */
	"wake",		/* 0x0000000004000000 */
	"hold",		/* 0x0000000008000000 */
	"delalloc",	/* 0x0000000010000000 */
	"mapuser",	/* 0x0000000020000000 */
	"dacct",	/* 0x0000000040000000 */
	"vdbuf",	/* 0x0000000080000000 */
	"gr_buf",	/* 0x0000000100000000 */
	"UNK",		/* 0x0000000200000000 */
	"gr_isd",	/* 0x0000000400000000 */
	"UNK",		/* 0x0000000800000000 */
	"xlv_hbuf",	/* 0x0000001000000000 */
	"xlvd_buf",	/* 0x0000002000000000 */
	"xlvd_fail",	/* 0x0000004000000000 */
	"xlv_ioc",	/* 0x0000008000000000 */
	"xlv_ack",	/* 0x0000010000000000 */
	"shuffled",	/* 0x0000020000000000 */
	"prio_val",	/* 0x0000040000000000 */
	"unused",	/* 0x0000080000000000 */
	"xfs_shutdown",	/* 0x0000100000000000 */
	"nfs_awrite",	/* 0x0000200000000000 */
	"nfs_unstable",	/* 0x0000400000000000 */
	"nfs_retry",	/* 0x0000800000000000 */
	"uninitial",	/* 0x0001000000000000 */
	"0x8000000000000000",
			/* 0x8000000000000000 */
	0
};

/*
 * gbuf - print out a buffer header
 */
static void
idbg_gbuf(buf_t *bp, int idx, buf_t *bufhead)
{
	qprintf("buf: addr 0x%x [%d]", bp, idx);
	qprintf("   ");
	if (bp->b_flags == 0)
		qprintf("warning: no flags set ");
	else
		_printflags((unsigned)bp->b_flags, tab_bflags, "flags");

	qprintf("\n");

	if (bp->b_vp)
	qprintf(
	"     vtree: parent 0x%x [%d] back 0x%x [%d] forw 0x%x [%d] bal %d\n",
		bp->b_parent, (bp->b_parent) ? bp->b_parent - bufhead : 0,
		bp->b_back, (bp->b_back) ? bp->b_back - bufhead : 0,
		bp->b_forw, (bp->b_forw) ? bp->b_forw - bufhead : 0,
		bp->b_balance);
	else
	qprintf(
	"     hash chain: back 0x%x [%d] forw 0x%x [%d]\n",
		bp->b_back, (bp->b_back) ? bp->b_back - bufhead : 0,
		bp->b_forw, (bp->b_forw) ? bp->b_forw - bufhead : 0);

	qprintf(
	"     free chain: listid %d av_forw 0x%x [%d] av_back 0x%x [%d]\n",
		bp->b_listid,
		bp->av_forw, (bp->av_forw) ? bp->av_forw - bufhead : 0,
		bp->av_back, (bp->av_back) ? bp->av_back - bufhead : 0);
	qprintf(
	"     dev 0x%x blkno 0x%x vp/proc 0x%x offset 0x%llx\n",
		bp->b_edev, bp->b_blkno, bp->b_vp, bp->b_offset);
	if (bp->b_target)
	qprintf(
	"     b_target 0x%x (dev 0x%x specvp 0x%x bdevsw 0x%x)\n",
		bp->b_target, bp->b_target->dev, bp->b_target->specvp,
		bp->b_target->bdevsw);
	qprintf(
	"     dev chain: back 0x%x [%d] forw 0x%x [%d]\n",
		bp->bd_back, (bp->bd_back) ? bp->bd_back - bufhead : 0,
		bp->bd_forw, (bp->bd_forw) ? bp->bd_forw - bufhead : 0);
	qprintf(
	"     ref %d error %d resid 0x%x start 0x%x\n",
		bp->b_ref, bp->b_error, bp->b_resid, bp->b_start);
	qprintf(
	"     vchain: b_dforw 0x%x [%d] b_dback 0x%x [%d]\n",
		bp->b_dforw, (bp->b_dforw) ? bp->b_dforw - bufhead : 0,
		bp->b_dback, (bp->b_dback) ? bp->b_dback - bufhead : 0);
	qprintf(
        "     bcount 0x%x bufsize 0x%x addr 0x%x pgs 0x%x remain 0x%x\n",
		bp->b_bcount, bp->b_bufsize, bp->b_un.b_addr,
		bp->b_pages, bp->b_remain);
	qprintf(
	"     &iodone 0x%x (%d) &lock 0x%x (%d) sort 0x%x\n",
		&bp->b_iodonesema, valusema(&bp->b_iodonesema),
		&bp->b_lock, valusema(&bp->b_lock),
		bp->b_sort);
	qprintf(
	"     iodonefunc 0x%x brelse 0x%x bdstrat 0x%x private 0x%x\n",
		bp->b_iodone, bp->b_relse, bp->b_bdstrat, bp->b_private);
	qprintf(
	"     fspriv 0x%x fspriv2 0x%x fspriv3 0x%x pincount 0x%x pinwaiter 0x%x\n",
		bp->b_fsprivate, bp->b_fsprivate2, bp->b_fsprivate3, bp->b_pincount,
		bp->b_pin_waiter);
	qprintf(
	"     ref 0x%x balance 0x%x dforw 0x%x dback 0x%x\n",
		bp->b_ref, bp->b_balance, bp->b_dforw, bp->b_dback);
	qprintf(
	"     parent 0x%x bvtype 0x%x ", bp->b_parent, bp->b_bvtype);
	if (BUF_IS_IOSPL(bp)) qprintf("iopri 0x%x\n", bp->b_iopri);
#ifdef DEBUG_BUFTRACE
	qprintf("trace buf 0x%x\n", bp->b_trace);
#endif
	qprintf("\n");
}

extern int	dchunkbufs, delallocleft;
extern int	delalloc_waiters;
extern int	bemptycnt;
#ifdef DEBUG_BUFTRACE
extern int	delalloc_dbufs, delalloc_cbufs;
#endif

/*
 * print out bio control locks & semas
 */
static void
bufstuff(void)
{
	int i, nbytes;
	buf_t *bp;

	/* go through all buffers and compute how much memory they are
	 * consuming
	 */
	nbytes = 0;
	for (i = 0, bp = global_buf_table; i < v.v_buf; i++, bp++) {
		nbytes += bp->b_bufsize;
	}

	qprintf(
	"     Total memory used by buffers %d bytes\n", nbytes);
	/* print out bfreelists */
	for (i = 0; i <= bfreelistmask; i++) {
		qprintf("bfreelist[%d]:\n", i);
		idbg_gbuf(&bfreelist[i], 0, global_buf_table);
	}

	/* print out those amazing stats */
	do_pdas(dumppdabstats, -1);

	qprintf("Chunk and delalloc buffer stats ((all numbers in decimal)\n");
	qprintf(
	"     chunkpages %d dchunkpages %d dchunkbufs %d\n",
		chunkpages, dchunkpages, dchunkbufs);
	qprintf(
   	"     delallocleft %d delalloc_waiters %d\n",
		delallocleft, delalloc_waiters);
#ifdef DEBUG_BUFTRACE
	qprintf(
   	"     delalloc_dbufs %d delalloc_cbufs %d\n",
		delalloc_dbufs, delalloc_cbufs);
#endif
	qprintf(
	"     bemptycnt %d\n", bemptycnt);
}

/*
 * dumppdabstats - dump stats for getblks that are stored in the pda
 */
static void
dumppdabstats(register pda_t *pd)
{
	qprintf("Getblk stats for cpu %d (all numbers in decimal)\n",
		pd->p_cpuid);
	qprintf(
	"     getblks %d getblockmiss %d getfound %d getbchg %d\n",
		pd->ksaptr->p_getblkstats.getblks,
		pd->ksaptr->p_getblkstats.getblockmiss,
		pd->ksaptr->p_getblkstats.getfound,
		pd->ksaptr->p_getblkstats.getbchg);
	qprintf(
	"     getfree %d getfreeempty %d getfreehmiss %d getfreealllck %d getfreeref %d\n",
		pd->ksaptr->p_getblkstats.getfree,
		pd->ksaptr->p_getblkstats.getfreeempty,
		pd->ksaptr->p_getblkstats.getfreehmiss,
		pd->ksaptr->p_getblkstats.getfreealllck,
		pd->ksaptr->p_getblkstats.getfreeref);

	qprintf(
	"     getloops %d getfreedelwri %d getfreerelse %d flush %d flushloops %d\n",
		pd->ksaptr->p_getblkstats.getloops,
		pd->ksaptr->p_getblkstats.getfreedelwri,
		pd->ksaptr->p_getblkstats.getfreerelse,
		pd->ksaptr->p_getblkstats.flush,
		pd->ksaptr->p_getblkstats.flushloops);
	qprintf(
	"     getoverlap %d clusters %d clustered %d\n",
		pd->ksaptr->p_getblkstats.getoverlap,
		pd->ksaptr->p_getblkstats.clusters,
		pd->ksaptr->p_getblkstats.clustered);
	qprintf(
	"     getfrag %d getpatch %d trimmed %d inserts %d irotates %d deletes %d\n",
		pd->ksaptr->p_getblkstats.getfrag,
		pd->ksaptr->p_getblkstats.getpatch,
		pd->ksaptr->p_getblkstats.trimmed,
		pd->ksaptr->p_getblkstats.inserts,
		pd->ksaptr->p_getblkstats.irotates,
		pd->ksaptr->p_getblkstats.deletes);
	qprintf(
	"     drotates %d decomms %d flush_decomms %d delrsv %d delrsvfree %d\n",
		pd->ksaptr->p_getblkstats.drotates,
		pd->ksaptr->p_getblkstats.decomms,
		pd->ksaptr->p_getblkstats.flush_decomms,
		pd->ksaptr->p_getblkstats.delrsv,
		pd->ksaptr->p_getblkstats.delrsvfree);
	qprintf(
	"     delrsvclean %d delrsvdirty %d delrsvwait %d\n",
		pd->ksaptr->p_getblkstats.delrsvclean,
		pd->ksaptr->p_getblkstats.delrsvdirty,
		pd->ksaptr->p_getblkstats.delrsvwait);
	qprintf(
	"     sync_commits %d commits %d getfreecommit %d inactive %d active %d\n",
		pd->ksaptr->p_getblkstats.sync_commits,
		pd->ksaptr->p_getblkstats.commits,
		pd->ksaptr->p_getblkstats.getfreecommit,
		pd->ksaptr->p_getblkstats.inactive,
		pd->ksaptr->p_getblkstats.active);
}

/*
 * print out a regular buffer header
++
++	kp buf addr
++		addr < 0  : prints buf at address addr
++		addr >= 0 : prints buf[addr]
++		addr == -1: prints buf headers/stats
 */
void idbg_fbuf(__psint_t p1)
{
   int idx;
   buf_t *bp;

	if (p1 < 0) {
		if (p1 == -1L) {
			bufstuff();
			return;
		}
		bp = (buf_t *) p1;
		if ((__psint_t) bp == (__psint_t) &bfreelist[0]) {
			qprintf("0x%x is the buffer freelist header\n", bp);
			bufstuff();
			return;
		}
		if ((__psint_t) bp == (__psint_t) &global_buf_hash[0]) {
			buf_t *dp;
			int cnt, m, mi;

			qprintf("0x%x is the buffer hash header\n", bp);
			for (idx = 0, m = -1; idx < v.v_hbuf; idx++) {
				dp = (struct buf *)&global_buf_hash[idx];
				for (bp = dp->b_forw, cnt = 0;
				     bp != dp;
				     bp = bp->b_forw) {
					cnt++;
				}
				qprintf("0x%x[%d]:%d  ", dp, idx, cnt);
                                if (cnt > m) {
                                        m = cnt;
                                        mi = idx;
                                }
				if (idx % 3 == 2)
					qprintf("\n");
			}
			qprintf("\nmax entry [%d]:%d\n", mi, m);
			return;
		}
		idx = bp - global_buf_table;
		if (idx < 0 || idx >= v.v_buf) {
			qprintf("0x%x is not a standard buffer header\n", p1);
		}
	}
	else {
		if (p1 < 0 || p1 >= v.v_buf) {
			qprintf("0x%x not a valid file buffer #\n", p1);
			return;
		}
		bp = &global_buf_table[p1];
		idx = p1;
	}
	idbg_gbuf(bp, idx, global_buf_table);
}

void
idbg_vbuf(__psint_t p1)
{
	vnode_t *vp;
	register buf_t *blast, *bnext, *bp;
	off_t offset = 0;
	off_t end;

	vp = (vnode_t *)p1;

	if (idbg_vnode(p1) == 0)
		return;

	blast = bnext = vp->v_buf;

	while (bnext) {

		end = bnext->b_offset + BTOBBT(bnext->b_bcount);

		if (end <= offset) {
			if ((bp = bnext->b_forw) && bp != blast) {
				blast = bnext;
				bnext = bp;
			} else {
				blast = bnext;
				bnext = bnext->b_parent;
			}
			continue;
		}

		blast = bnext;
		if (bp = bnext->b_back) {
			if (bp->b_offset + BTOBBT(bp->b_bcount) > offset) {
				bnext = bp;
				continue;
			}
		}

		bp = bnext;
		bnext = bnext->b_forw;
		if (!bnext)
			bnext = bp->b_parent;

		qprintf(" buf 0x%x, [0x%x,0x%x], p: 0x%x b: 0x%x f: 0x%x bal: %d\n",
			bp, (scoff_t)bp->b_offset, BTOBBT(bp->b_bcount),
			bp->b_parent, bp->b_back, bp->b_forw, bp->b_balance);

		offset = end;
	}
}

/*
 * Find any buffers which match the given block number in their
 * blkno fields.
 */
void
idbg_findbuf(int blkno)
{
	buf_t	*bp;
	int	index;
	int	found;

	bp = global_buf_table;
	index = 0;
	found = 0;

	while (index < v.v_buf) {
		if (bp->b_blkno == blkno) {
			idbg_gbuf(bp, index, global_buf_table);
			qprintf("\n");
			found++;
		}
		index++;
		bp++;
	}
	if (!found) {
		qprintf("No buffer with b_blkno == %x found.\n",
			blkno);
	}
}

#ifdef EVEREST
static void
prprct(int num, int den)
{
    int fraction;

    /* print out a fixed point decimal value for num/den */
    /* kernel printf doesn't seem to support integer field widths.. */
    fraction = (int)((long long)num * 10000 / (long long)den) % 10000;
    qprintf("%d.%s%s%s%d (%d/%d)", num / den,
	    fraction > 999 ? "" : "0",
	    fraction > 99 ? "" : "0",
	    fraction > 9 ? "" : "0",
	    fraction, num, den);
}

void idbg_splcnt(__psint_t func, __psint_t arg0, __psint_t arg1)
{
    switch(func) {

      case 0: {
	  /*
	   * print out the system-wide average of how often a spinlock
	   * is obtained on the first try
	   */
	  struct k_splockmeter *spm;
	  int num_inuse = 0, gotfirst = 0, num = 0;
	  
	  for (spm = lockmeter_chain; spm; spm = next_lockmeter(spm)) {
	      if (spm->m_gotfirst_num > 0) {
		  num_inuse++;
		  gotfirst += spm->m_gotfirst;
		  num += spm->m_gotfirst_num;
	      }
	  }
	  
	  qprintf("%d spinlocks used since last reset\n", num_inuse);
	  qprintf("\tpercentage which were obtained on first try: ");
	  prprct(gotfirst, num);
	  qprintf("\n");
	  break;
      }
      case 1: {
	  struct k_splockmeter *spm;

	  /* print out any spinlock which was obtained on the first try
	   * on average less often than arg0/arg1
	   */
	  qprintf("Locks that were obtained on the first try less often than "
		  "%d/%d:\n", arg0, arg1);
	  for (spm = lockmeter_chain; spm; spm = next_lockmeter(spm)) {
	      if (spm->m_gotfirst_num > 100 &&
		  spm->m_gotfirst * arg1 < arg0 * spm->m_gotfirst_num) {
		  qprintf("Lock meter 0x%x, lock_t 0x%x,\n\tname \"%s\"",
			  spm, spm->m_addr, spm->m_name);
		  qprintf("\n\tlast locked from ");
		  _prsymoff((void *)spm->m_elapsedpc,NULL,NULL);
		  qprintf("\n\taverage contention ");
		  prprct(spm->m_gotfirst, spm->m_gotfirst_num);
		  qprintf("\n");
	      }
	  }
	  break;
      }
      case 2: {
	  /*
	   * print out average splock contention queue depth system-wide
	   */
	  struct k_splockmeter *spm;
	  int total = 0, num = 0, num_inuse = 0;
	  
	  for (spm = lockmeter_chain; spm; spm = next_lockmeter(spm)) {
	      if (spm->m_racers_num > 0) {
		  num_inuse++;
		  total += spm->m_racers_total;
		  num += spm->m_racers_num;
	      }
	  }
	  
	  qprintf("%d spinlocks contended for since last reset\n", num_inuse);
	  qprintf("\taverage contention: ");
	  prprct(total, num);
	  qprintf("\n");
	  break;
      }
      case 3: {
	  struct k_splockmeter *spm;

	  /* print out any spinlock which has an average contention 
	   * greater than a fraction defined by arg0/arg1
	   */
	  qprintf("Locks with an average contention greater than %d/%d:\n",
		  arg0, arg1);
	  for (spm = lockmeter_chain; spm; spm = next_lockmeter(spm)) {
	      if (spm->m_racers_num > 100 &&
		  spm->m_racers_total * arg1 > arg0 * spm->m_racers_num) {
		  qprintf("Lock meter 0x%x, lock_t 0x%x,\n\tname \"%s\"",
			  spm, spm->m_addr, spm->m_name);
		  qprintf("\n\tlast locked from ");
		  _prsymoff((void *)spm->m_elapsedpc,NULL,NULL);
		  qprintf("\n\taverage contention ");
		  prprct(spm->m_racers_total, spm->m_racers_num);
		  qprintf("\n\tthis lock obtained on first try ");
		  prprct(spm->m_gotfirst, spm->m_gotfirst_num);
		  qprintf("\n");
	      }
	  }
	  break;
      }
      case 99: {
	  struct k_splockmeter *spm;
	  
	  /* clear all stats */
	  qprintf("clearing all lock contention info\n");
	  for (spm = lockmeter_chain; spm; spm = next_lockmeter(spm)) {
	      spm->m_racers_max = spm->m_racers_num = 
	      spm->m_racers_total = spm->m_gotfirst = 
	      spm->m_gotfirst_num = 0;
	  }
	  break;
      }
      default:
	qprintf("unknown option %d\n", func);
	return;
    }
}
#endif


#ifdef EVEREST

/*
++
++	kp decode arg
++		prints slot, leaf, bank of physical address
++
*/
void idbg_decode_addr(caddr_t address)
{
	qprintf("  0x%x decodes to ", address);
	mc3_decode_addr(qprintf, 0, (__psint_t)address);

}

#endif /* EVEREST */

/*
++
++	kp dport arg
++		arg >= 0 : prints dport structure for port # <arg>.
++		arg < 0  : prints dport structure at address <arg>.
++
*/

void
idbg_dport(__psint_t p1)
{
	extern void dump_dport_info(__psint_t);
	dump_dport_info(p1);
}

/*
++
++	dportrset arg
++		arg >= 0 : resets error counts in dport structure
++			   for port # <arg>.
++		arg < 0  : resets error counts in dport structure
++			   at address <arg>.
++
*/

void
idbg_dportrset(__psint_t p1)
{
	extern void reset_dport_info(__psint_t);
	reset_dport_info(p1);
}

#if defined(SIO_DUMMY) && defined(DEBUG)
void
idbg_dummyportact(__psint_t p)
{
    extern void dump_dummyport_act(__psint_t);
    dump_dummyport_act(p);
}
#endif

#if DEBUG == 1

void
idbg_dportact(__psint_t p1)
{
	extern void dump_dport_actlog(__psint_t);
	dump_dport_actlog(p1);
}

#ifdef I2CUART_DEBUG
void
idbg_i2cuart(__psint_t arg0, __psint_t arg1)
{
    extern void sio_i2c_idbg_output(void);
    extern void sio_i2c_idbg_input(int);

    qprintf("%d %d\n", arg0, arg1);
    if (arg0 == 0) {
	qprintf("getting output:\n");
	sio_i2c_idbg_output();
	qprintf("\n");
    }
    else if (arg0 == 1) {
	qprintf("putting %d chars of input\n", arg1);
	sio_i2c_idbg_input((int)arg1);
    }
}
#endif

#ifdef ULI

/*ARGSUSED*/
void idbg_curuli(int b)
{
    struct uli *uli = private.p_curuli;

    if (uli == 0)
	qprintf("no curuli\n");
    else {
	qprintf("uli jmpbuf:\n");
	qprintf("\ts0/s1/s2/s3: 0x%llx, 0x%llx, 0x%llx, 0x%llx\n",
		uli->jmpbuf[JB_S0],
		uli->jmpbuf[JB_S1],
		uli->jmpbuf[JB_S2],
		uli->jmpbuf[JB_S3]);
	qprintf("\ts4/s5/s6/s7: 0x%llx, 0x%llx, 0x%llx, 0x%llx\n",
		uli->jmpbuf[JB_S4],
		uli->jmpbuf[JB_S5],
		uli->jmpbuf[JB_S6],
		uli->jmpbuf[JB_S7]);
	qprintf("\tsp/fp/pc/sr/cel: 0x%llx, 0x%llx, 0x%llx, 0x%llx, 0x%x\n",
		uli->jmpbuf[JB_SP],
		uli->jmpbuf[JB_FP],
		uli->jmpbuf[JB_PC],
		uli->jmpbuf[JB_SR],
		(unchar)uli->jmpbuf[JB_CEL]);
	qprintf("uthread 0x%x\n", uli->uli_uthread);
	qprintf("gp/sp/pc: 0x%x, 0x%x, 0x%x\n", uli->gp, uli->sp, uli->pc);
	qprintf("saved intstk 0x%x\n", uli->saved_intstack);
	qprintf("saved curuthread 0x%x\n", uli->uli_saved_curuthread);
	qprintf("saved asids 0x%x\n", uli->saved_asids);
	qprintf("new asids 0x%x\n", uli->new_asids);
	qprintf("uli cpu 0x%x\n", uli->ulicpu);
    }
}

void idbg_uliact(int cpu)
{
    extern int uli_during_swtch, initsgtbl_uli, real_invalid,
               bogus_invalid;
    extern int intuliframe, nested_int_during_uli;
    extern int nested_return_to_uli, defer_resched, exc_keruliframe;
    extern int segtblrace, real_nested_int_during_uli;
    extern int nested_uli;
    extern int availpids(void), pidflushinterval(void);
    int flag = 0;
    cpu_cookie_t was_running;

    if (cpu != cpuid()) {
	flag = 1;
	was_running = setmustrun(cpu);
    }
    qprintf("pids %d interval %d\n", availpids(), pidflushinterval());
    qprintf("ULIs during swtch %d\n", uli_during_swtch);
    qprintf("initsegtbl %d\n", initsgtbl_uli);
    qprintf("inval pdes in uli vfault: %d\n", real_invalid);
    qprintf("bogus inval pdes in uli vfault: %d\n", bogus_invalid);
    qprintf("intuliframe %d\n", intuliframe);
    qprintf("nested int during uli %d\n", nested_int_during_uli);
    qprintf("real nested int during uli %d\n", real_nested_int_during_uli);
    qprintf("nested uli %d\n", nested_uli);
    qprintf("nested return to uli %d\n", nested_return_to_uli);
    qprintf("defer resched %d\n", defer_resched);
    qprintf("keruliframe %d\n", exc_keruliframe);
    qprintf("segtblrace %d\n", segtblrace);
    if (flag)
	restoremustrun(was_running);
}
#endif

#endif /* DEBUG */

void
idbg_pty(__psint_t n)
{
	extern void dump_pty_info(__psint_t);

	dump_pty_info(n);
}

#if defined(DEBUG) && defined(ACTLOG)

extern lock_t actlog_lock;
extern int actlog_log_cpu;

void
idbg_actlogcpu(__psint_t p1)
{
	actlog_log_cpu = p1;
}

void
idbg_actlogany()
{
	actlog_log_cpu = LOG_ANY_CPU;
}

void
idbg_actlog(__psint_t p1)
{
	int i, j, act;
	struct actlogentry *aep;

	if (actlog_log_cpu == LOG_ANY_CPU) {
		qprintf("log_cpu 0x%x (ANY) lock 0x%x\n",
			&actlog_log_cpu, actlog_lock);
	} else {
		qprintf("log_cpu 0x%x (%d) lock 0x%x\n",
			&actlog_log_cpu, actlog_log_cpu, actlog_lock);
	}
	for (i = 0, j = sysactlog.al_wind % ACTLOG_SIZE;
	     i < ACTLOG_SIZE;
	     i++, j = ++j % ACTLOG_SIZE) {
		aep = &sysactlog.al_log[j];
		act = aep->ae_act;
		qprintf(
"actlog[%d]: %s sec %d usec %d cpu %d 0x%x 0x%x 0x%x\n",
			j,
			act == AL_NULL ?		"EMPTY     " :
			act == AL_INTR_IN ?		"INTR IN   " :
			act == AL_INTR_OUT ?		"INTR OUT  " :
			act == AL_INTR_BAD ?		"INTR BAD  " :
			act == AL_GETRUNQ ?		"GETRUNQ   " :
			act == AL_SYSCALL ?		"SYSCALL   " :
			act == AL_PMON ?		"PMON      " :
			act == AL_PMON_COMP ?		"PMON COMP " :
			act == AL_PMON_TRIP ?		"PMON TRIP " :
			act == AL_VMON ?		"VMON      " :
			act == AL_VMON_TRIP ?		"VMON TRIP " :
			act == AL_QMON ?		"QMON      " :
			act == AL_QMON_Q ?		"QMON Q    " :
			act == AL_AMON ?		"AMON      " :
			act == AL_RMON ?		"RMON      " :
			act == AL_MON_RUNQ ?		"MON RUNQ  " :
			act == AL_STRSERV ?		"STR SERV  " :
			act == AL_MARK0 ?		"MARK 0    " :
			act == AL_MARK1 ?		"MARK 1    " :
			act == AL_MARK2 ?		"MARK 2    " :
			act == AL_MARK3 ?		"MARK 3    " :
			act == AL_MARK4 ?		"MARK 4    " :
			act == AL_MARK5 ?		"MARK 5    " :
			act == AL_MARK6 ?		"MARK 6    " :
			act == AL_MARK7 ?		"MARK 7    " :
			act == AL_MARK8 ?		"MARK 8    " :
			act == AL_MARK9 ?		"MARK 9    " :
			act == AL_STROPEN ?		"STR OPEN  " :
			act == AL_STRCLOSE ?		"STR CLOSE " :
							"?!?!?!?!? ",
			aep->ae_sec,
			aep->ae_usec,
			aep->ae_cpu,
			aep->ae_info1,
			aep->ae_info2,
			aep->ae_info3);
	}
}

#endif /* DEBUG && ACTLOG */

/*
++
++	kp shaddr arg
++		arg == -1 : prints shaddr structure for curproc
++		arg < 0  : prints shaddr structure at addr <arg>
++		arg >= 0 : prints shaddr structure for proc with pid <arg>
++
*/
void
idbg_do_shaddr(shaddr_t *sa)
{
	register proc_t *p;
	register int i;

	qprintf(
"Shared address block at 0x%x refcnt %d &fupdsema 0x%x(0x%x)\n",
		sa, sa->s_refcnt,
		&sa->s_fupdsema, mutex_owner(&sa->s_fupdsema));
	qprintf(
"  plink [0x%x] ",
		sa->s_plink);

	if (sa->s_fdt == NULL)
		qprintf("no file descriptor table\n");
	else
		qprintf("&fdt 0x%x\n", sa->s_fdt);

	qprintf(
"  &detachlock &detached 0x%x\n",
		&sa->s_detachsem, &sa->s_detached);
	qprintf(
"  cdir 0x%x rdir 0x%x umask 0%o ulimit 0x%x uid %d gid %d\n",
		sa->s_cdir, sa->s_rdir, sa->s_cmask, sa->s_limit);
	qprintf(
"  cred @ 0x%x [ref %d] uid %d gid %d\n",
		sa->s_cred, sa->s_cred->cr_ref,
		sa->s_cred->cr_uid, sa->s_cred->cr_gid);
	if (sa->s_cred->cr_ngroups != 0) {
		qprintf("  mgrps:");
		for (i = 0; i < sa->s_cred->cr_ngroups; i++) {
			qprintf(" %d",sa->s_cred->cr_groups[i]);
		}
		qprintf("\n");
	}
	if (mac_enabled)
		print_maclabel("  MAC label ", sa->s_cred->cr_mac);
	qprintf(
"  rupdlock 0x%x listlock 0x%x\n",
		sa->s_rupdlock, sa->s_listlock);
	switch(sa->s_sched) {
	case SGS_FREE:
		qprintf("  schedmode SGS_FREE (%d)", sa->s_sched);
		break;
	case SGS_SINGLE:
		qprintf("  schedmode SGS_SINGLE (%d)", sa->s_sched);
		break;
	case SGS_EQUAL:
		qprintf("  schedmode SGS_EQUAL (%d)", sa->s_sched);
		break;
	case SGS_GANG:
		qprintf("  schedmode SGS_GANG (%d)", sa->s_sched);
		break;
	default:
		qprintf("  schedmode INVALID (%d)", sa->s_sched);
		break;
	}
	qprintf(", master (pid) %d\n",
		sa->s_master);

	qprintf(" GDB 0x%x \n", sa->s_gdb);
	for (p = sa->s_plink; p; p = p->p_slink)
		shortproc(p, 1);
}

void
idbg_do_ut_shaddr(uthread_t *ut)
{
	register proc_t *pp = UT_TO_PROC(ut);

	if (!IS_SPROC(ut->ut_pproxy)) {
		qprintf("proc %d does not have a shared address block\n", pp);
		return;
	}

	idbg_do_shaddr(pp->p_shaddr);
}
void
idbg_shaddr(__psint_t p1)
{
	if (p1 < 0 && p1 != -1L) {
		idbg_do_shaddr((shaddr_t *)p1);
	} else {
		(void) idbg_doto_uthread(p1, idbg_do_ut_shaddr, 0, 0);
	}
}

/*
 * Vnode descriptor dump.
 * This table is a string version of all the flags defined in sys/vnode.h.
 */
char *tab_vflags[] = {
	/* local only flags */
	"VDUP",		/* 0x01 */
	"VINACT",	/* 0x02 */
	"VRECLM",	/* 0x04 */
	"VLOCK",	/* 0x08 */
	"VEVICT",	/* 0x10 */
	"VWAIT",	/* 0x20 */
	"VFLUSH",	/* 0x40 */
	"VGONE",	/* 0x80 */
	"VREMAPPING",	/* 0x100 */
	"VMOUNTING",	/* 0x200 */
	"VLOCKHOLD",	/* 0x400 */
	"VDFSCONVERTED",/* 0x800 */
	"VROOTMOUNTOK",	/* 0x1000 */
	"VINACTIVE_TEARDOWN", /* 0x2000 */
	"VSEMAPHORE",	/* 0x4000 */
	"VUSYNC",	/* 0x8000 */
	/* invalid (unused) flags */
	"INVALID",	/* 0x10000 */
	"INVALID",	/* 0x20000 */
	"INVALID",	/* 0x40000 */
	"INVALID",	/* 0x80000 */
	/* single system image flags */
	"VROOT",	/* 0x100000 */
	"VNOSWAP", 	/* 0x200000 */
	"VISSWAP", 	/* 0x400000 */
	"VREPLICABLE",	/* 0x800000 */
	"VNOTREPLICABLE",/* 0x1000000 */
	"VDOCMP",	/* 0x2000000 */
	"VSHARE",	/* 0x4000000 */
	"VFRLOCKS",	/* 0x8000000 */
	"VENF_LOCKING",	/* 0x10000000 */
	/* invalid (unused) flags */
	"INVALID",	/* 0x20000000 */
	"INVALID",	/* 0x40000000 */
	"INVALID",	/* 0x80000000 */
	0
};


char *tab_vtypes[] = {
	"VNON",
	"VREG",
	"VDIR",
	"VBLK",
	"VCHR",
	"VLNK",
	"VFIFO",
	"VBAD",
	"VSOCK",
	0
};

extern vnodeops_t spec_at_vnodeops;
extern vnodeops_t spec_cs_vnodeops;
extern vnodeops_t spec_vnodeops;
#ifdef	CELL_IRIX
extern vnodeops_t dsvn_ops;
#endif	/* CELL_IRIX */

extern vnodeops_t fifo_vnodeops, nm_vnodeops;
extern vnodeops_t efs_vnodeops, nfs_vnodeops, *afs_vnodeopsp;
extern vnodeops_t xfs_vnodeops;

/*
++
++	kp vnode addr
++		addr < 0  : prints vnode at address addr
++		addr >= 0 : prints v_number == addr
++		addr == -1: prints vnode stats
++
*/
vnode_t *
idbg_vnode(__psint_t p1)
{
	vnode_t *vp;
	vtype_t vtype;
	idbgfssw_t *i;

	if (p1 == -1L) {
		/* dump stats */
		do_pdas(dumppdaistats, -1);
		return 0;
	}
	if (p1 >= 0) {
		struct inode *ip;
		struct vfs *vfsp;
		bhv_desc_t *vfs_bdp;
		int found = 0;
		/*
		 * Loop through all mounted file systems' inode list
		 */
		for (vfsp = rootvfs; vfsp && !found; vfsp = vfsp->vfs_next) {
			if (i = idbg_findfssw_vfs(vfsp, &vfs_bdp)) {
				if (vp = i->vnode_find(vfsp, (vnumber_t)p1)) {
					found++;
					break;
				}
			} else if (vfs_bdp = bhv_lookup(VFS_BHVHEAD(vfsp), &efs_vfsops)) {
				for (ip = bhvtom(vfs_bdp)->m_inodes; ip; ip = ip->i_mnext)
					if (itov(ip)->v_number == p1) {
						vp = itov(ip);
						found++;
						break;
					}
			} else {
				continue;
			}
		}
		if (!found) {
			qprintf("vnode number %d not found on efs, or xfs filesystem\n", p1);
			return 0;
		}
	} else
		vp = (vnode_t *)p1;

	vtype = vp->v_type;
	qprintf("vnode 0x%x name %s number %d (0x%x) count %d\n",
		vp, dnlc_v2name(vp), vp->v_number, vp->v_number, vp->v_count);
	qprintf("type %s ", tab_vtypes[vtype]);
	_printflags((unsigned)vp->v_flag, tab_vflags, "flags");
	qprintf("\n");
	qprintf("vfsmountedhere 0x%x vfsp 0x%x\n",
		vp->v_vfsmountedhere, vp->v_vfsp);
	/* print behavior info */
	idbg_bhvh((__psint_t)VN_BHV_HEAD(vp));

	qprintf(
	"   stream 0x%x rdev %d,%d next 0x%x prev 0x%x\n",
		vp->v_stream, emajor(vp->v_rdev),
		minor(vp->v_rdev), vp->v_next, vp->v_prev);
	qprintf(
	"   filocks 0x%x &filocksem 0x%x intpcount %d vpcache 0x%x vplock 0x%x\n",
		vp->v_filocks, &vp->v_filocksem, vp->v_intpcount, 
			&vp->v_pcache, &vp->v_pcacheflag);
	qprintf(
	"   vbuf 0x%x dbuf %d dpages %x pgcnt %d mreg 0x%x \n",
		vp->v_buf, vp->v_dbuf, vp->v_dpages,
		vp->v_pgcnt, vp->v_mreg);
	qprintf(
	"   gen %d hashp 0x%x hashn 0x%x namecap 0x%x listid %d dgen %d\n",
		vp->v_bufgen, vp->v_hashp, vp->v_hashn, vp->v_namecap,
		vp->v_listid, vp->v_dpages_gen);

	/*
	 * file system dependent stuff
	 */
	idbg_vnbhv((__psint_t)(vp->v_fbhv));
	return vp;
}

static int
vnops_to_vfstype(
	vnodeops_t	*vnops)
{
	int	i;
	extern int vfsmax;

	for (i = 0; i < vfsmax; i++) {
		if (vfssw[i].vsw_vnodeops == vnops) {
			return i;
		}
	}
	return 0;
}

void
idbg_bhvh(__psint_t x)
{
	bhv_head_t	*bhvh;

	if (x < 0) {
		bhvh = (bhv_head_t *)x;
#ifdef BHV_SYNCH
		qprintf("Behavior head at 0x%x mrlock 0x%x\n",
				bhvh, &bhvh->bh_mrlock);
#ifdef DEBUG
		bhv_print_ucallout(bhvh);
#endif /* DEBUG */
#else
		qprintf("Behavior head at 0x%x\n", bhvh);
#endif
		if (bhvh->bh_first == NULL)
			qprintf("NULL behavior chain\n");
		else
			idbg_bhv((__psint_t)bhvh->bh_first);
	}
}

void
idbg_bhv(__psint_t x)
{
	bhv_desc_t *bdp;

	if (x < 0) {
		bdp = (bhv_desc_t *)x;
		qprintf("Printing bhv descriptors starting at 0x%x\n", bdp);
		do {
			qprintf(" ops 0x%x vobj 0x%x pdata 0x%x next 0x%x\n",
                                bdp->bd_ops, bdp->bd_vobj, bdp->bd_pdata,
                                bdp->bd_next);
			bdp = bdp->bd_next;
		} while (bdp);
	}
}
			

void
idbg_vnbhv(__psint_t x)
{
	bhv_desc_t	*bdp;
	int		fstype;
	char		*fstype_name;
	vtype_t		vtype;
	idbgfssw_t	*i;

	if (x < 0 ) {
		bdp = (bhv_desc_t *)x;
		vtype = BHV_TO_VNODE(bdp)->v_type;
		qprintf("\n");
		qprintf("Printing file system-dependent data\n");
		do {
			qprintf("ops 0x%x vobj 0x%x pdata 0x%x next 0x%x\n",
				bdp->bd_ops, bdp->bd_vobj, bdp->bd_pdata,
				bdp->bd_next);

			fstype_name = "?";

			if (bdp->bd_ops == &spec_at_vnodeops) {
				fstype = vnops_to_vfstype(&spec_vnodeops);
				fstype_name = "spec_at";
			} else if (bdp->bd_ops == &spec_cs_vnodeops) {
				fstype = vnops_to_vfstype(&spec_vnodeops);
				fstype_name = "spec_cs";
			} else if (bdp->bd_ops == &spec_vnodeops) {
				fstype = vnops_to_vfstype(&spec_vnodeops);
				fstype_name = "spec_ls";
#ifdef	CELL_IRIX
			} else if (bdp->bd_ops == &dsvn_ops) {
				fstype = vnops_to_vfstype(&dsvn_ops);
				fstype_name = "dsvn";
#endif	/* CELL_IRIX */
			} else {
				fstype = vnops_to_vfstype(bdp->bd_ops);
				fstype_name = vfssw[fstype].vsw_name;

				if (fstype_name == NULL)
					fstype_name = "?";
			}

			qprintf("fstype %d (%s) data 0x%x\n",
				fstype, fstype_name, bdp->bd_pdata);

			if (bdp->bd_pdata == NULL &&
			    bdp->bd_ops != afs_vnodeopsp)
				return;

			if (vtype == VFIFO) {
				if (bdp->bd_ops == &fifo_vnodeops) {
					struct fifonode *fnp = BHVTOF(bdp);
					idbg_prfifo(fnp, 0);
					return;
#ifndef SABLE
				/* SABLE (& miniroot) don't load namefs */
				} else if (bdp->bd_ops == &nm_vnodeops) {
					struct namenode *nmp = BHVTONM(bdp);
					idbg_prnamenode(nmp, 0);
					return;
#endif	/* SABLE */
				}
			}
			if (i = idbg_findfssw_vnbhv(bdp)) {
				i->vnode_data_print(bdp->bd_pdata);
			} else if (bdp->bd_ops == &efs_vnodeops) {
				struct inode *ip = bhvtoi(bdp);

				idbg_prinode(ip, vtype, 0);
			} else if (bdp->bd_ops == &nfs_vnodeops ||
				   bdp->bd_ops == nfs3_getvnodeops()) {
				idbg_prrnode(bdp, vtype, 0);
			} else if (bdp->bd_ops == &spec_at_vnodeops) {

				idbg_pratnode(BHV_TO_ATP(bdp));

			} else if (bdp->bd_ops == &spec_cs_vnodeops) {

				idbg_prcsnode(BHV_TO_CSP(bdp));

			} else if (bdp->bd_ops == &spec_vnodeops) {

				idbg_prlsnode(BHV_TO_LSP(bdp));

			} else if (bdp->bd_ops == afs_vnodeopsp &&
				   idbg_prafsnodep) {
				(*idbg_prafsnodep)(bdp);
			}

			bdp = bdp->bd_next;
		} while (bdp);
	}
	return;
}

void
idbg_extents(__psint_t x)
{
	vnode_t		*vp;
	int		vtype;
	bhv_desc_t	*bdp;

	vp = idbg_vnode(x);
	if (vp == 0)
		return;

	vtype = vp->v_type;

	bdp = vn_bhv_lookup_unlocked(VN_BHV_HEAD(vp), &efs_vnodeops);
	if (bdp == NULL) {
		return;
	}
	if (vtype == VREG || vtype == VDIR || vtype == VLNK) {
		struct inode *ip = bhvtoi(bdp);
		register int i;
		int numextents;
		extent *ex;

		/* indirect extents not yet read in */
		if ((ip->i_flags & IINCORE) == 0) {
			ex = ip->i_extents;
			numextents = ex->ex_offset;
			qprintf("  indir extents: (bn len foffset):\n");
		} else {
			numextents = ip->i_numextents;
			qprintf("  extents: (bn len foffset):\n");
		}

		ex = &ip->i_extents[0];
		for (i = 0; i < numextents; i++, ex++) {
			if ((i%4) == 0)
				qprintf(" ");
			qprintf("%x %x %x",
				ex->ex_bn, ex->ex_length,
				ex->ex_offset);
			if ((i%4) == 3)
				qprintf("\n");
			else
				qprintf(" ");
		}
		if ((i%4) != 0)
			qprintf("\n");
	}
}

/*
 * Print the contents of a fifonode
 */
void
idbg_prfifo(struct fifonode *fnp, int header)
{
	if (header)
		qprintf("fifo node: addr 0x%x\n", fnp);

	qprintf(
	"    fn_vnode 0x%x fn_mate 0x%x fn_realvp 0x%x fn_ino %d fn_open %d\n",
			FTOV(fnp),
			fnp->fn_mate,
			fnp->fn_realvp,
			fnp->fn_ino,
			fnp->fn_open);

	if (fnp->fn_lockp) { /* valid lock node info */
		qprintf(
		"    fn_lockp 0x%x &lock 0x%x (owner 0x%x) refcnt %d\n",
				fnp->fn_lockp,
				&(fnp->fn_lockp->fn_lock),
				mutex_owner(&(fnp->fn_lockp->fn_lock)),
				fnp->fn_lockp->fn_refcnt);
	} else { /* empty */	
		qprintf(
		"    fn_lockp NULL\n");
	}

	qprintf(
	"    &empty 0x%x (q: 0x%x) &full 0x%x (q: 0x%x) &ioctl 0x%x (q: 0x%x) unique 0x%x\n",
			&fnp->fn_empty, SV_WAITER(&fnp->fn_empty),
			&fnp->fn_full, SV_WAITER(&fnp->fn_full),
			&fnp->fn_ioctl, SV_WAITER(&fnp->fn_ioctl),
			fnp->fn_unique);
        qprintf(
                "    &openwait 0x%x (q: 0x%x) &rwait 0x%x (q: 0x%x) &wwait 0x%x (q: 0x%x)\n",
                        &fnp->fn_openwait, SV_WAITER(&fnp->fn_openwait),
                        &fnp->fn_rwait, SV_WAITER(&fnp->fn_rwait),
                        &fnp->fn_wwait, SV_WAITER(&fnp->fn_wwait));

        qprintf(
        "    &wcnt 0x%x (%d) &rcnt 0x%x (%d)\n",
                        &fnp->fn_numw, fnp->fn_numw,
                        &fnp->fn_numr, fnp->fn_numr);

	qprintf(
	"    flags %s%s%s%s%s%s%s%s%s%s%s%s\n",
			fnp->fn_flag & ISPIPE ? "ISPIPE " : "",
			fnp->fn_flag & FIFOLOCK ? "FIFOLOCK " : "",
			fnp->fn_flag & FIFOSEND ? "FIFOSEND " : "",
			fnp->fn_flag & FIFOWRITE ? "FIFOWRITE " : "",
			fnp->fn_flag & FIFOWANT ? "FIFOWANT " : "",
			fnp->fn_flag & FIFOREAD ? "FIFOREAD " : "",
			fnp->fn_flag & FIFOPASS ? "FIFOPASS " : "",
			fnp->fn_flag & FIFOCLOSING ? "FIFOCLOSING " : "",
			fnp->fn_flag & FIFODONTLOCK ? "FIFODONTLOCK " : "",
                        fnp->fn_flag & FIFOWCLOSED ? "FIFOWCLOSED " : "",
                        fnp->fn_flag & FIFOWOPEN ? "FIFOWOPEN " : "",
                        fnp->fn_flag & FIFOWCLOSE ? "FIFOWCLOSE " : ""
			);
	qprintf(
	"    atime %d mtime %d ctime %d\n",
			fnp->fn_atime, fnp->fn_mtime, fnp->fn_ctime);
	qprintf(
	"    nextp 0x%x backp 0x%x\n",
			fnp->fn_nextp, fnp->fn_backp);
}

/*
 * efs inode flags
 */
char	*tab_iflags[] = {
	"IUPD",		/* 0x0001 */
	"IACC",		/* 0x0002 */
	"ICHG",		/* 0x0004 */
	"ISYN",		/* 0x0008 */
	"INODELAY",	/* 0x0010 */
	"IRWLOCK",	/* 0x0020 */
	"ITRUNC",	/* 0x0040 */
	"IINCORE",	/* 0x0080 */
	"IMOD",		/* 0x0100 */
	"0x0200",	/* 0x0200 */
	"0x0400",	/* 0x0400 */
	"0x0800",	/* 0x0800 */
	0
};

/*
 * Print the contents of an inode
 */
void
idbg_prinode(struct inode *ip, vtype_t vtype, int header)
{
	if (header)
		qprintf("inode: addr 0x%x\n", ip);

	if (vtype == VREG || vtype == VDIR || vtype == VLNK) {
		qprintf(
	"     inumber %d dev %x rdev %x\n",
			ip->i_number,
			ip->i_dev,
			ip->i_rdev);
		qprintf(
	"     hash 0x%x next 0x%x prevp 0x%x\n",
			ip->i_hash,
			ip->i_next,
			ip->i_prevp);
		qprintf(
	"     mount 0x%x mnext 0x%x mprevp 0x%x\n",
			ip->i_mount,
			ip->i_mnext,
			ip->i_mprevp);
		_printflags((unsigned)ip->i_flags, tab_iflags, "flags");
		qprintf("vers %s\n",
			ip->i_version == EFS_IVER_EFS ? "EFS" :
			ip->i_version == EFS_IVER_AFSSPEC ? "AFSSPEC" :
			ip->i_version == EFS_IVER_AFSINO ? "AFSINO" : "UNK");
		qprintf(
	"     lockid %lld locktrips %d lock 0x%x (owner: 0x%x)\n",
			ip->i_lockid,
			ip->i_locktrips,
			&ip->i_lock, mutex_owner(&ip->i_lock));
		qprintf(
	"     nextbn %x reada %x dquot 0x%x vcode %d afs 0x%x\n",
			ip->i_nextbn,
			ip->i_reada,
			ip->i_dquot,
			ip->i_vcode,
			ip->i_afs);
		qprintf(
	"     mode %o nlink %d uid %d gid %d size %d\n",
			ip->i_mode, 
			ip->i_nlink, 
			ip->i_uid, 
			ip->i_gid, 
			ip->i_size); 
		qprintf(
	"     atime %d mtime %d ctime %d gen %d\n",
			ip->i_atime, 
			ip->i_mtime, 
			ip->i_ctime, 
			ip->i_gen); 
		qprintf(
	"     numindirs %d &indir 0x%x indirbytes %d directbytes %d\n",
			ip->i_numindirs,
			ip->i_indir,
			ip->i_indirbytes,
			ip->i_directbytes);

		qprintf(
	"     numextents %d &extents 0x%x blocks %d\n",
			ip->i_numextents,
			ip->i_extents,
			ip->i_blocks);
	      }
}

/*
 * Print the contents of a namenode
 */
void
idbg_prnamenode(struct namenode *nmp, int header)
{
	if (header)
		qprintf("name node: addr 0x%x\n", nmp);

		qprintf(
"     nm_vnode 0x%x nm_filevp 0x%x nm_filep 0x%x nm_mountptvp 0x%x\n",
			NMTOV(nmp),
			nmp->nm_filevp,
			nmp->nm_filep,
			nmp->nm_mountpt);

		qprintf(
"     &nm_lock 0x%x (0x%x) nm_nextp 0x%x nm_backp 0x%x\n",
			&nmp->nm_lock,
			mutex_owner(&nmp->nm_lock),
			nmp->nm_nextp,
			nmp->nm_backp);

		qprintf(
"     va_mask 0x%x va_type %d va_mode 0x%x va_uid %d va_gid %d\n",
			nmp->nm_vattr.va_mask,
			nmp->nm_vattr.va_type,
			nmp->nm_vattr.va_mode,
			nmp->nm_vattr.va_uid,
			nmp->nm_vattr.va_gid);

		qprintf(
"     va_fsid 0x%x va_nodeid 0x%x va_nlink %d va_size %d\n",
			nmp->nm_vattr.va_fsid,
			nmp->nm_vattr.va_nodeid,
			nmp->nm_vattr.va_nlink,
			(int)nmp->nm_vattr.va_size);

		qprintf(
"     va_atime 0x%x 0x%x va_mtime 0x%x 0x%x va_ctime 0x%x 0x%x\n",
			nmp->nm_vattr.va_atime.tv_sec,
			nmp->nm_vattr.va_atime.tv_nsec,
			nmp->nm_vattr.va_mtime.tv_sec,
			nmp->nm_vattr.va_mtime.tv_nsec,
			nmp->nm_vattr.va_ctime.tv_sec,
			nmp->nm_vattr.va_ctime.tv_nsec);

		qprintf(
"     va_rdev 0x%x va_blksize %d va_nblocks %d va_vcode %d\n",
			nmp->nm_vattr.va_rdev,
			nmp->nm_vattr.va_blksize,
			nmp->nm_vattr.va_nblocks,
			nmp->nm_vattr.va_vcode);
}

/*
 * Print the contents of an lsnode
 */
void
idbg_prlsnode(struct lsnode *lsp)
{
	vnode_t		*vp;


	vp = LSP_TO_VP(lsp);

	qprintf("  dev 0x%x(%d/%d, %d) <%s>", lsp->ls_dev,
					      major(lsp->ls_dev),
					      emajor(lsp->ls_dev),
					      minor(lsp->ls_dev),
					      tab_vtypes[vp->v_type]);

#ifdef	CELL_IRIX

	if (vp == lsp->ls_fsvp) {
		qprintf(" vp==fsvp 0x%x{%d} cell %d\n",
				vp, vp->v_count,
				lsp->ls2cs_handle.sh_obj.h_service.s_cell);
	} else {
		if (lsp->ls_fsvp)
			qprintf(" vp 0x%x{%d} fsvp 0x%x{%d} cell %d\n",
				vp, vp->v_count,
				lsp->ls_fsvp, lsp->ls_fsvp->v_count,
				lsp->ls2cs_handle.sh_obj.h_service.s_cell);
		else
			qprintf(" vp 0x%x{%d} fsvp 0x0 cell %d\n",
				vp, vp->v_count,
				lsp->ls2cs_handle.sh_obj.h_service.s_cell);
	}

#else	/* ! CELL_IRIX */

	if (vp == lsp->ls_fsvp) {
		qprintf(" vp==fsvp 0x%x{%d}\n", vp, vp->v_count);
	} else {
		if (lsp->ls_fsvp)
			qprintf(" vp 0x%x{%d} fsvp 0x%x{%d}\n",
					vp, vp->v_count,
					lsp->ls_fsvp, lsp->ls_fsvp->v_count);
		else
			qprintf(" vp 0x%x{%d} fsvp 0x0\n",
					vp, vp->v_count);
	}

#endif	/* ! CELL_IRIX */

	qprintf(
    "  opencnt %d gen %d ops 0x%x fsid %d flags %s%s%s5s%s%s%s%s%s%s%s%s%s\n",
		lsp->ls_opencnt, lsp->ls_gen,
		lsp->ls2cs_ops,
		lsp->ls_fsid,
		lsp->ls_flag & SACC		? "SACC "    : "",
		lsp->ls_flag & SCHG		? "SCHG "    : "",
		lsp->ls_flag & SUPD		? "SUPD "    : "",
		lsp->ls_flag & SCOMMON		? "SCOM "    : "",
		lsp->ls_flag & SPASS		? "SPASS "   : "",
		lsp->ls_flag & SMOUNTED		? "SMOUNT "  : "",
		lsp->ls_flag & SLINKREMOVED	? "SLNKRM "  : "",
		lsp->ls_flag & SINACTIVE	? "SINACTV " : "",
		lsp->ls_flag & SWANTCLOSE	? "SWNTCLS " : "",
		lsp->ls_flag & SCLOSING		? "SCLOSE "  : "",
		lsp->ls_flag & SWAITING		? "SWAIT "   : "",
		lsp->ls_flag & SATTR		? "SATTR "   : "",
		lsp->ls_flag & SSTREAM		? "STREAM "  : "");

#ifdef	SPECFS_DEBUG
	qprintf("  cycle %d", lsp->ls_cycle);
	qprintf("  line# %d", lsp->ls_line);
#endif	/* SPECFS_DEBUG */

#ifdef	CELL_IRIX
	qprintf(
	"  com_hndl.h_objid %d/%d/0x%x cvp 0x%x\n\n",
			SPEC_HANDLE_TO_SERVICE(lsp->ls2cs_handle).s_cell,
			SPEC_HANDLE_TO_SERVICE(lsp->ls2cs_handle).s_svcnum,
			SPEC_HANDLE_TO_OBJID(lsp->ls2cs_handle),
			SPEC_HANDLE_TO_OBJID(lsp->ls_cvp_handle));
#else	/* ! CELL_IRIX */
	qprintf(
	"  com_hdl.objid 0x%x cvp 0x%x\n\n",
				SPEC_HANDLE_TO_OBJID(lsp->ls2cs_handle),
				SPEC_HANDLE_TO_OBJID(lsp->ls_cvp_handle));
#endif	/* ! CELL_IRIX */
}


/*
 * Print the contents of a atnode
 */
void
idbg_pratnode(struct atnode *atp)
{
	vnode_t		*vp;


	vp = ATP_TO_VP(atp);

	qprintf("  dev 0x%x(%d/%d, %d) <%s> avp 0x%x{%d}",
						atp->at_dev,
						major(atp->at_dev),
						emajor(atp->at_dev),
						minor(atp->at_dev),
						tab_vtypes[vp->v_type],
						vp, vp->v_count);
	if (atp->at_fsvp)
		qprintf(" fsvp 0x%x{%d}\n",
				atp->at_fsvp, atp->at_fsvp->v_count);
	else
		qprintf(" fsvp 0x0\n");

	qprintf("  uid %d gid %d proj %d fsid 0x%x size %d mode 0%o\n",
			atp->at_uid, atp->at_gid, atp->at_projid,
			atp->at_fsid, atp->at_size, atp->at_mode);
	
	qprintf( " atime 0x%x mtime 0x%x ctime 0x%x flags 0x%x",
			atp->at_atime, atp->at_mtime, atp->at_ctime,
			atp->at_flag);

	qprintf("\n\n");
}


/*
 * Print the contents of a csnode
 */
void
idbg_prcsnode(struct csnode *csp)
{
	vnode_t		*vp;


	vp = CSP_TO_VP(csp);

	qprintf("  dev 0x%x(%d/%d, %d) <%s> cvp 0x%x{%d}\n",
						csp->cs_dev,
						major(csp->cs_dev),
						emajor(csp->cs_dev),
						minor(csp->cs_dev),
						tab_vtypes[vp->v_type],
						vp, vp->v_count);

	qprintf(
	    "  opencnt %d mapcnt %d gen %d flags %s%s%s%s%s%s%s%s%s%s%s%s%s",
			csp->cs_opencnt, csp->cs_mapcnt, csp->cs_gen,
			csp->cs_flag & SACC		? "SACC "    : "",
			csp->cs_flag & SCHG		? "SCHG "    : "",
			csp->cs_flag & SUPD		? "SUPD "    : "",
			csp->cs_flag & SCOMMON		? "SCOM "    : "",
			csp->cs_flag & SPASS		? "SPASS "   : "",
			csp->cs_flag & SMOUNTED		? "SMOUNT "  : "",
			csp->cs_flag & SLINKREMOVED	? "SLNKRM "  : "",
			csp->cs_flag & SINACTIVE	? "SINACTV " : "",
			csp->cs_flag & SWANTCLOSE	? "SWNTCLS " : "",
			csp->cs_flag & SCLOSING		? "SCLOSE "  : "",
			csp->cs_flag & SWAITING		? "SWAIT "   : "",
			csp->cs_flag & SATTR		? "SATTR "   : "",
			csp->cs_flag & SSTREAM		? "STREAM "  : "");

	if (csp->cs_close_flag)
		qprintf("  closeflags %s%s",
		    csp->cs_close_flag & SCLOSING	? "SCLOSE "  : "",
		    csp->cs_close_flag & SWAITING	? "SWAIT "   : "");

	qprintf("\n\n");
}


/*
 * Print 1 line about a vnode
 * all = 0: ignore vnodes with counts == 0
 * all = 1: ignore VNON types (but include count == 0)
 * all = 2: print 'em all
 * all = 3: (all = 0) + print vp->v_filocks 
 */
static void
prvn(vnode_t *vp, int all)
{
	vtype_t vtype;

	if (all == 0 && vp->v_count == 0)
		return;
	vtype = vp->v_type;
	if (all == 1 && vtype == VNON)
		return;
	qprintf("0x%x: #%d name %s type %s cnt %d ",
		vp, vp->v_number, dnlc_v2name(vp),
		tab_vtypes[vtype], vp->v_count);
	_printflags((unsigned)vp->v_flag, tab_vflags, "flags");
	qprintf("rdev 0x%x ", vp->v_rdev);

	if (!print_filocks) {
		qprintf("\n");
	} else {
		qprintf("filocks 0x%x\n", vp->v_filocks);
	}
}

/* external interface */
void
__prvn(vnode_t *vp, int all)
{
	prvn(vp, all);
}

#ifdef DEBUG

/* ARGSUSED */
void
print_spec_csnodes_matching_vfs(struct vfs *vfsp)
{
	int		i;
	int		header = 0;
	struct csnode	*csp;

	for (i = 0; i < DCSPECTBSIZE; i++) {
		for (csp = spec_csnode_table[i]; csp; csp = csp->cs_next) {
			struct vnode *vp = CSP_TO_VP(csp);

			if (vp->v_vfsp == vfsp) {
				if (!header) {
					qprintf(
					    "---------- csnodes ---------\n");
					header++;
				}

				_prvn(CSP_TO_VP(csp), 2);
			}
		}
	}
}


void
print_spec_lsnodes_matching_vfs(struct vfs *vfsp)
{
	int		i;
	int		header = 0;
	struct lsnode	*lsp;

	for (i = 0; i < DLSPECTBSIZE; i++) {
		for (lsp = spec_lsnode_table[i];
				lsp;
					lsp = lsp->ls_next) {

			struct vnode *vp = LSP_TO_VP(lsp);

			if (vp->v_vfsp == vfsp) {

				if (!header) {
					qprintf(
					    "---------- lsnodes ---------\n");
					header++;
				}

				_prvn(LSP_TO_VP(lsp), 2);
			}
		}
	}
}

#endif


/*
 * print all vnodes for a vfs
 */
static void
prvfsvn(struct vfs * vfsp, int all)
{
	int	i;
	idbgfssw_t *p;
	bhv_desc_t *bdp;

	if (p = idbg_findfssw_vfs(vfsp, &bdp)) {
		p->vfs_vnodes_print(vfsp, all);
	} else {
		bdp = bhv_base_unlocked(VFS_BHVHEAD(vfsp));
		if ((vfsops_t *)BHV_OPS(bdp) == &efs_vfsops) {
			struct inode *ip;
			if (bhvtom(bdp) == NULL) {
				qprintf("No EFS superblock!\n");
				return;
			}
			for (ip = bhvtom(bdp)->m_inodes; ip; ip = ip->i_mnext)
				prvn(itov(ip), all);
		} else if ((vfsops_t *)BHV_OPS(bdp) == &prvfsops) {
			if (BHV_PDATA(bdp) == NULL) {
				qprintf("No private proc data\n");
				return;
			}
			/* Need more */
		} else if (afs_vfsopsp && (vfsops_t *)BHV_OPS(bdp) == afs_vfsopsp) {
			(*idbg_afsvfslistp)();
		} else if ((vfsops_t *)BHV_OPS(bdp) == &spec_vfsops) {
			       struct lsnode *lsp;
			       struct csnode *csp;

			for (i = 0; i < DCSPECTBSIZE; i++) {
				/* qprintf("spec_csnode_table[%d]:\n", i); */
				for (csp = spec_csnode_table[i];
						csp; csp = csp->cs_next)

					prvn(CSP_TO_VP(csp), all);
			}

			for (i = 0; i < DLSPECTBSIZE; i++) {
				/* qprintf("spec_lsnode_table[%d]:\n", i); */
				for (lsp = spec_lsnode_table[i];
						lsp; lsp = lsp->ls_next)

					prvn(LSP_TO_VP(lsp), all);
			}
		} else {
			qprintf("Unknown vfs ops 0x%x\n", BHV_OPS(bdp));
		}
	}
}

/*
++	kp fifo addr
++	    addr => prints address as a fifo node entry
*/
void
idbg_fifo(__psint_t x)
{
	if (x != -1L && x < 0) {
		idbg_prfifo((struct fifonode *)x, 1);
	}
	return;
}

/*
++	kp fifolist addr
++	    addr < 0  : prints summary of the fifo node
++	    addr == 1 : prints summary of all fifo nodes on fifoallocmon list
++	    addr == 99: prints help
*/
void
idbg_fifolist(__psint_t x)
{
	register struct fifonode *fnode;
	extern struct fifonode *fifoalloc;

	if (x != -1L && x < 0) {
		idbg_prfifo((struct fifonode *)x, 1);
	}
	else {
		if (x == 1) {
			qprintf("All fifo nodes on fifoallocmon list\n");
			for (fnode = fifoalloc; fnode; fnode = fnode->fn_nextp)
				idbg_prfifo(fnode, 1);
		}
		else if (x == 99) {
			/* print help */
			qprintf("USAGE: kp fifolist <arg>\n");
			qprintf("%s\n%s\n%s\n",
		"       addr < 0  : prints info on the fifo node",
		"       addr == 1 : prints summary of all allocate fifo nodes",
		"       addr == 99: prints help"
			);
		}
	}
}

/*
++	kp namelist addr
++	    addr < 0  : prints summary of the name node
++	    addr == 1 : prints summary of all name nodes on namealloc list
++	    addr == 99: prints help
*/
/* ARGSUSED */
void
idbg_namelist(__psint_t x)
{
#ifndef SABLE
	register struct namenode *tnode;
	extern struct namenode *namealloc;

	if (x != -1L && x < 0) {
		idbg_prnamenode((struct namenode *)x, 1);
	} else {
		if (x == 1) {
			qprintf("All name nodes on namealloc list\n");
			for (tnode = namealloc; tnode; tnode = tnode->nm_nextp)
				idbg_prnamenode(tnode, 1);
		} else if (x == 99) {
			/* print help */
			qprintf("USAGE: kp namelist <arg>\n");
			qprintf("%s\n%s\n%s\n",
		"       addr < 0  : prints info on the name node",
		"       addr == 1 : prints summary of all allocated name nodes",
		"       addr == 99: prints help"
			);
		}
	}
#endif
}

/*
++
++	kp vlist addr
++	    addr < 0  : prints summary of all vnodes for vfs
++				including those on free list
++	    addr == 1 : prints summary vnodes on vfreelist
++	    addr == 99: prints help
++	    addr == 3 : turn on printing vp->v_filocks
++	    addr == 4 : turn off printing vp->v_filocks
++	    addr == -1: prints summary of all mounted vnodes with refcnt > 0
++	    no addr   : prints summary of all mounted vnodes with refcnt > 0
++
*/
void
idbg_vlist(__psint_t x)
{
	register struct vfs	*vfsp;
	register vfreelist_t	*vfp;
	register vnode_t	*vp;
	register int		i;

	if (x != -1 && x < 0) {
		vfsp = (struct vfs *) x;
		qprintf("Active vnodes for vfs @ 0x%x dev 0x%x fstype %s\n",
			vfsp, vfsp->vfs_dev,
			vfssw[vfsp->vfs_fstype].vsw_name ?
			vfssw[vfsp->vfs_fstype].vsw_name : "<null>");
		prvfsvn(vfsp, 1);
		qprintf("\nFree vnodes for vfs @ 0x%x\n", vfsp);
		vfp = vfreelist;
		do {
			for (i = vfp->vf_lsize, vp = vfp->vf_freelist.vl_next;
			     --i >= 0 && vp != (struct vnode *)vfp;
			     vp = vp->v_next) {
				if (vp->v_vfsp == vfsp)
					prvn(vp, 1);
			}
			vfp = vfp->vf_next;
		} while (vfp != vfreelist);
	} else if (x == 1) {
		/* print all vnodes on vfreelists */
		qprintf("All vnodes on vfreelists\n");
		vfp = vfreelist;
		do {
			for (i = vfp->vf_lsize, vp = vfp->vf_freelist.vl_next;
			     --i >= 0 && vp != (struct vnode *)vfp;
			     vp = vp->v_next)
				prvn(vp, 2);
			vfp = vfp->vf_next;
		} while (vfp != vfreelist);
	} else
	if (x == 99) {
		/* print help */
		qprintf(
"USAGE: kp vlist <arg>\n");
		qprintf("%s\n%s\n%s\n%s\n%s\n%s\n%s\n%s\n",
"	addr < 0  : prints summary of all vnodes for vfs",
"			including those on free lists",
"	addr == 1 : prints summary vnodes on vfreelists",
"	addr == 3 : turn on printing vp->v_filocks",
"	addr == 4 : turn off printing vp->v_filocks",
"	addr == -1: prints summary of all mounted vnodes with refcnt > 0",
"	no addr   : prints summary of all mounted vnodes with refcnt > 0"
			);
	} else if (x == 3) {
		print_filocks = 1;
		qprintf("Will print vp->v_filocks\n");
	} else if (x == 4) {
		print_filocks = 0;
		qprintf("Will NOT print vp->v_filocks\n");
	} else {	/* -1 */
		for (vfsp = rootvfs; vfsp; vfsp = vfsp->vfs_next) {
			qprintf("\nActive vnodes for vfs @ 0x%x dev 0x%x fstype %s\n",
				vfsp, vfsp->vfs_dev,
				vfssw[vfsp->vfs_fstype].vsw_name ?
				vfssw[vfsp->vfs_fstype].vsw_name : "<null>");
			prvfsvn(vfsp, 0);
		}
	}
}

/*
 * dumppdaistats - dump stats for igets that are stored in the pda
 */
static void
dumppdaistats(register pda_t *pd)
{
	qprintf("Iget stats for cpu %d (all numbers in decimal)\n",
		pd->p_cpuid);
	qprintf("  attempts %d found %d frecycle %d missed %d dup %d\n",
		pd->ksaptr->p_igetstats.ig_attempts,
		pd->ksaptr->p_igetstats.ig_found,
		pd->ksaptr->p_igetstats.ig_frecycle,
		pd->ksaptr->p_igetstats.ig_missed,
		pd->ksaptr->p_igetstats.ig_dup);
	qprintf("  reclaims %d itobp %d itobpf %d\n",
		pd->ksaptr->p_igetstats.ig_reclaims,
		pd->ksaptr->p_igetstats.ig_itobp,
		pd->ksaptr->p_igetstats.ig_itobpf);
    qprintf("  iupdat %d iupacc %d iupupd %d iupchg %d iupmod %d iupunk %d\n",
		pd->ksaptr->p_igetstats.ig_iupdat,
		pd->ksaptr->p_igetstats.ig_iupacc,
		pd->ksaptr->p_igetstats.ig_iupupd,
		pd->ksaptr->p_igetstats.ig_iupchg,
		pd->ksaptr->p_igetstats.ig_iupmod,
		pd->ksaptr->p_igetstats.ig_iupunk);
	qprintf("  iallocrd %d iallocrdf %d ialloccoll %d\n",
		pd->ksaptr->p_igetstats.ig_iallocrd,
		pd->ksaptr->p_igetstats.ig_iallocrdf,
		pd->ksaptr->p_igetstats.ig_ialloccoll);
	qprintf("  bmaprd %d bmapfbm %d bmapfbc %d\n",
		pd->ksaptr->p_igetstats.ig_bmaprd,
		pd->ksaptr->p_igetstats.ig_bmapfbm,
		pd->ksaptr->p_igetstats.ig_bmapfbc);
	qprintf("  dirupd %d truncs %d icreat %d attrchg %d\n",
		pd->ksaptr->p_igetstats.ig_dirupd,
		pd->ksaptr->p_igetstats.ig_truncs,
		pd->ksaptr->p_igetstats.ig_icreat,
		pd->ksaptr->p_igetstats.ig_attrchg);
}

static void
do_efs(register struct efs *fs, int summary)
{
	char fname[sizeof fs->fs_fname + 1];
	char fpack[sizeof fs->fs_fpack + 1];
	short dirty;
	register struct cg *cg;
	register int i;

	if (IS_EFS_MAGIC(fs->fs_magic))
		qprintf(" efs superblock 0x%x:", fs);
	else {
		qprintf(" bad magic number -- 0x%x not efs super block\n", fs);
		return;
	}
	fname[sizeof fs->fs_fname] = '\0';
	fpack[sizeof fs->fs_fpack] = '\0';
	qprintf(" fs name \"%s\" pack \"%s\" on dev 0x%x",
		strncpy(fname, fs->fs_fname, sizeof fs->fs_fname),
		strncpy(fpack, fs->fs_fpack, sizeof fs->fs_fpack),
		fs->fs_dev);
	dirty = fs->fs_dirty;
	qprintf(" %s%s%s%s\n",
		dirty ?
		 dirty == EFS_ACTIVEDIRT ? "ACTIVEDIRT " :
		  dirty == EFS_ACTIVE ? "ACTIVE " :
		   dirty == EFS_DIRTY ? "DIRTY " : "HUH?" :
		 "",
		fs->fs_readonly ? "RDONLY " : "",
		fs->fs_fmod ? "MODIFIED " : "",
		fs->fs_corrupted ? "CORRUPTED " : "");
	qprintf(
"  fssize %x firstcg %x cgrotor %x cgfsize %x cgisize %x free blks %x\n",
		fs->fs_size, fs->fs_firstcg, fs->fs_cgrotor, fs->fs_cgfsize,
		fs->fs_cgisize, fs->fs_tfree);
	qprintf(
"  sec/trk %x lbshift %x ncg %x ipcg %x free inodes %x\n",
		fs->fs_sectors, fs->fs_lbshift, fs->fs_ncg,
		fs->fs_ipcg, fs->fs_tinode);
	qprintf(
"  minfree %x mindirfree %x ino/chk %x bb/inochk %x last inode %x\n",
		fs->fs_minfree, fs->fs_mindirfree,
		fs->fs_inopchunk, fs->fs_inopchunkbb,
		fs->fs_lastinum);
	qprintf(
"  time %x lock 0x%x bmapsize %x bmapblock %x replsb %x\n",
		fs->fs_time, fs->fs_lock, fs->fs_bmsize,
		fs->fs_bmblock, fs->fs_replsb);

	if (summary)
		return;

	cg = &fs->fs_cgs[0];
	for (i = 0; i < fs->fs_ncg; i++, cg++) {
		qprintf("  cg %x @ %x: ", i, cg);
		qprintf("1stbn %x 1stdbn %x dfree %x 1stdfree %x\n",
			cg->cg_firstbn, cg->cg_firstdbn,
			cg->cg_dfree, cg->cg_firstdfree);
		qprintf("   firsti %x lowi %x lasti %x gen# %x%s",
			cg->cg_firsti, cg->cg_lowi,
			cg->cg_firsti + fs->fs_ipcg - 1, cg->cg_gen,
			cg->cg_lowi >= cg->cg_firsti + fs->fs_ipcg ?
				" (DEPLETED)\n" : "\n");
	}
}

/*
++
++	kp efs addr
++		addr : address of EFS superblock
++
*/
void
idbg_efs(struct efs *fs)
{
	do_efs(fs, 0);
}

/*
 * kp quota number
 * number == -1 prints out quota information of all mount entries with quotas
 * number >= 0 prints out information for uid number for all mount entries
 */
void
idbg_quota(__psint_t x)
{
	struct vfs *vfsp;
	struct mount *mp;
	int usrid, printed = 0;
	struct dquot *dqp;
	bhv_desc_t *bdp;


	if ((caddr_t)v.ve_dquot == NULL) {
	    qprintf("quota subsystem not initialized or not included\n");
	    return;
	}
	for (vfsp = rootvfs; vfsp; vfsp = vfsp->vfs_next) {
	    bdp = bhv_base_unlocked(VFS_BHVHEAD(vfsp));
	    mp = bhvtom(bdp);
	    if (x == -1L)
		    usrid = -1;
	    else
		    usrid = x;
	    if (((vfsops_t *)BHV_OPS(bdp) == &efs_vfsops) && (mp->m_flags & (M_QENABLED | M_QACTIVE))) {
		for (dqp = dquot; dqp < (struct dquot *)v.ve_dquot; dqp++) {
		    if ((dqp->dq_mp == mp) &&
			((usrid == -1) || (dqp->dq_uid == usrid))) {
			printed++;
			qprintf("quota at 0x%x [%d] dev 0x%x flags %s\n",
				dqp, dqp - dquot, dqp->dq_mp->m_devvp->v_rdev,
				dqp->dq_flags & DQ_MOD ? "DQ_MOD " : "");
			qprintf("\trefcnt %d, uid %d, offset %lld\n",
				dqp->dq_cnt, dqp->dq_uid, dqp->dq_off);
			qprintf("\tdisk %d %d %d, inodes %d %d %d\n",
				dqp->dq_bhardlimit, dqp->dq_bsoftlimit,
				dqp->dq_curblocks, dqp->dq_fhardlimit,
				dqp->dq_fsoftlimit, dqp->dq_curfiles);
			qprintf("\tbtime(hrs) %d, ftime(hrs) %d\n",
			    dqp->dq_btimelimit/3600, dqp->dq_ftimelimit/3600);
			qprintf("\tdq_sema 0x%x\n", &dqp->dq_sema);
		    }
		}
	    }
	}
	if (!printed)
	    qprintf("No relevant quota information available\n");
}

/*
 * print out a struct dqblk
 *	kp dquotab addr - print disk quota info
 */
void
idbg_dqb(dqp)
struct dquot *dqp;
{
	struct vfs *vfsp;
	struct mount *mp;
	struct inode *ip;

	if ((caddr_t)v.ve_dquot == NULL) {
	    qprintf("quota subsystem not initialized or not included\n");
	    return;
	}
	if ((dqp == NULL) || (dqp == (struct dquot *) -1L)) {
		for (vfsp = rootvfs; vfsp; vfsp = vfsp->vfs_next) {
			bhv_desc_t *bdp = bhv_base_unlocked(VFS_BHVHEAD(vfsp));
			if ((vfsops_t *)BHV_OPS(bdp) == &efs_vfsops) {
				mp = bhvtom(bdp);
				qprintf("Scanning mount 0x%x for i_dquot\n", mp);
				for (ip = mp->m_inodes; ip != NULL; ip = ip->i_mnext) {
					if (ip->i_dquot != NULL)
						qprintf("inode 0x%x, i_dquot 0x%x\n", ip, ip->i_dquot);
				}
			}
		}
		return;
	}

	qprintf("quota at 0x%x [%d] dev 0x%x\n",
		dqp, dqp - dquot,
		dqp->dq_mp ? dqp->dq_mp->m_devvp->v_rdev : 0);
	qprintf("\thash list forw 0x%x, back 0x%x\n",
		dqp->dq_forw, dqp->dq_back);
	qprintf("\tfree list forw 0x%x, back 0x%x\n",
		dqp->dq_freef, dqp->dq_freeb);
	qprintf("\tflags %s\n",
		dqp->dq_flags & DQ_MOD ? "DQ_MOD " : "");
	qprintf("\trefcnt %d, uid %d, offset %lld\n",
		dqp->dq_cnt, dqp->dq_uid, dqp->dq_off);
	qprintf("\tdisk %d %d %d, inodes %d %d %d\n",
		dqp->dq_bhardlimit, dqp->dq_bsoftlimit,
		dqp->dq_curblocks, dqp->dq_fhardlimit,
		dqp->dq_fsoftlimit, dqp->dq_curfiles);
	qprintf("\tbtime(hrs) %d, ftime(hrs) %d\n",
		dqp->dq_btimelimit/3600, dqp->dq_ftimelimit/3600);
	qprintf("\tdq_sema 0x%x\n", &dqp->dq_sema);

}

/*
++
++	kp vfs addr
++		addr == -1: prints out summary of all valid mount points
++		addr < 0  : prints out mount entry pointed to by addr
++		addr > 0  : prints out vfs matching addr as vfs_dev
++
*/
void
idbg_vfs(__psint_t x)
{
	register struct vfs *vfsp;

	if (x == -1L) {
		/* go through all of mount table, and print summary */
		for (vfsp = rootvfs; vfsp; vfsp = vfsp->vfs_next)
			prvfs(vfsp, 1);
	} else if (x < 0) {
		/* use as address */
		prvfs((struct vfs *)x, 0);
	} else {
		/* go through mount table looking for match */
		vfsp = vfs_devsearch_nolock(x, VFS_FSTYPE_ANY);
		if (vfsp)
			prvfs(vfsp, 0);
		else
			qprintf("%d invalid mount address\n", x);
	}
}

void
prvfs(register struct vfs *vfsp, int summary)
{
	bhv_desc_t *bdp;
	idbgfssw_t *i;
	static char *tab_vfsflags[] = {
		"rdonly",		/* 0x00001 */
		"nosuid",		/* 0x00002 */
		"nodev",		/* 0x00004 */
		"grpid",		/* 0x00008 */
		"remount",		/* 0x00010 */
		"notrunc",		/* 0x00020 */
		"unlinkable",		/* 0x00040 */
		"badblock",		/* 0x00080 */
		"mlock",		/* 0x00100 */
		"mwait",		/* 0x00200 */
		"mwant",		/* 0x00400 */
		"cellular",		/* 0x00800 */
		"local",		/* 0x01000 */
		"offline",		/* 0x02000 */
		"dmi",			/* 0x04000 */
		"cellroot",		/* 0x08000 */
		"doxattr",		/* 0x10000 */
		"defxattr",		/* 0x20000 */
		NULL
	};

	qprintf("vfs at 0x%x dev 0x%x bcount %d bsize %d\n",
		vfsp, vfsp->vfs_dev, vfsp->vfs_bcount, vfsp->vfs_bsize);
	qprintf("  fstype %s fsid %d/%d \n",
		vfssw[vfsp->vfs_fstype].vsw_name ?
			vfssw[vfsp->vfs_fstype].vsw_name : "<null>",
		vfsp->vfs_fsid.val[0], vfsp->vfs_fsid.val[1]);
	_printflags(vfsp->vfs_flag, tab_vfsflags, "  flags");
	qprintf("\n");
	qprintf("  busycnt %d  wait 0x%x dcount %d\n",
		vfsp->vfs_busycnt, &vfsp->vfs_wait, vfsp->vfs_dcount);
	qprintf("  vnodecovered 0x%x nsubmounts %d vfsbh_first 0x%x",
		vfsp->vfs_vnodecovered, vfsp->vfs_nsubmounts, vfsp->vfs_bh.bh_first);
#ifdef BHV_SYNCH
	qprintf(" vfsbh_mrlock 0x%x", &vfsp->vfs_bh.bh_mrlock); 
#endif
	qprintf("\n");
	
	if (mac_enabled) {
		mac_vfs_t *mvp = vfsp->vfs_mac;
	
		if (mvp != NULL) {
			print_maclabel("  default ", mvp->mv_default);
			print_maclabel("  ipmac   ", mvp->mv_ipmac);
		}
	}

	if (summary)
		return;

	qprintf("  next 0x%x prevp 0x%x\n", vfsp->vfs_next, vfsp->vfs_prevp);
	bdp = bhv_base_unlocked(VFS_BHVHEAD(vfsp));
	if (i = idbg_findfssw_vfs(vfsp, &bdp)) {
		if (BHV_PDATA(bdp) == NULL) {
			qprintf("  No %s private data\n", i->name);
			return;
		}
		i->vfs_data_print(BHV_PDATA(bdp));
	} else {
		if ((vfsops_t *)BHV_OPS(bdp) == &efs_vfsops) {
			if (bhvtom(bdp) == NULL) {
				qprintf("  No EFS superblock!\n");
				return;
			}
			do_efs(bhvtoefs(bdp), 1);
		} else {
			/* does nothing if not an NFS vfs */
			idbg_nfs(vfsp);
		}
	}
}

int num_idbgvnmaps = 1;

void
idbg_vnmap(__psunsigned_t addr)
{
	int i;
	vnmap_t *map = (vnmap_t *)addr;
	static char *vnmap_flags[] = {
		"FOUND",	/* 0x0001 */
		"AUTOGROW",	/* 0x0002 */
		"OVERLAP",	/* 0x0004 */
		0
	};

	for (i = 0; i < num_idbgvnmaps; i++) {
		qprintf("vaddr 0x%llx, len %lld, ",
			(uint64_t) map->vnmap_vaddr,
			(int64_t) map->vnmap_len);
		_printflags(map->vnmap_flags, vnmap_flags, "flags");
		qprintf("\n  overlap file offset %lld, len %lld, vaddr 0x%x\n",
			map->vnmap_ovoffset * BBSIZE,
			(int64_t) map->vnmap_ovlen,
			(uint64_t) map->vnmap_ovvaddr);
	}
}

/*
 * Print out file modes.
 */
static void
print_fmode(int fmode)
{
	static char	*fmodes[] = {
		"RDONLY",	/* 0x0001 */
		"WRONLY",	/* 0x0002 */
		"RDWR",		/* 0x0004 */
		"NDELAY",	/* 0x0008 */
		"SYNC",		/* 0x0010 */
		"0x020",	/* 0x0020 */
		"0x040",	/* 0x0040 */
		"NONBLOCK",	/* 0x0080 */
		"CREAT",	/* 0x0100 */
		"TRUNC",	/* 0x0200 */
		"EXCL",		/* 0x0400 */
		"NOCTTY",	/* 0x0800 */
		"ASYNC",	/* 0x1000 */
		"0x2000",	/* 0x2000 */
		"0x4000",	/* 0x4000 */
		"DIRECT",	/* 0x8000 */
		0
		};

	qprintf("mode ");
	_printflags(fmode, fmodes,"mode");
	qprintf("\n");
}


void
idbg_bmapval(struct bmapval *bmapp)
{
	static char *eof_flags[] = {
		"EOF",		/* 0x01 */
		"HOLE",		/* 0x02 */
		"DELAY",	/* 0x04 */
		"!FLUSHOV",	/* 0x08 */
		"READAHEAD",	/* 0x10 */
		"UNWRITTEN",	/* 0x20 */
		0
		};

	qprintf("blkno 0x%x offset %llx length 0x%x bsize 0x%x\n",
		bmapp->bn, bmapp->offset,
		bmapp->length, bmapp->bsize);
	qprintf("pbsize 0x%x pboff 0x%x pbdev 0x%x eof ",
		bmapp->pbsize, bmapp->pboff, bmapp->pbdev);
	_printflags((uint)(bmapp->eof), eof_flags, "eof");
	qprintf("\n");
}

void
idbg_doadump(void)
{
    extern void _symmon_dump(void);
    _symmon_dump();
}

void
idbg_uio(uio_t *uiop)
{
	char	*seg;
	int	i;
	iovec_t	*iovp;

	switch (uiop->uio_segflg) {
	case UIO_NOSPACE:
		seg = "NOSPACE";
		break;
	case UIO_USERSPACE:
		seg = "USER";
		break;
	case UIO_SYSSPACE:
		seg = "SYS";
		break;
	case UIO_USERISPACE:
		seg = "USERI";
		break;
	default:
		seg = "UNKNOWN";
		break;
	}
	qprintf("offset %llx segment %s\n", uiop->uio_offset, seg);
	print_fmode(((int)uiop->uio_fmode) & 0x0000ffff);
	qprintf("limit %llx resid 0x%x pmp 0x%x niovecs %d\n",
		uiop->uio_limit, uiop->uio_resid,
		uiop->uio_pmp, uiop->uio_iovcnt);
	qprintf("readiolog %u, writeiolog %u \n",
		uiop->uio_readiolog,
		uiop->uio_writeiolog);

	iovp = uiop->uio_iov;
	i = 0;
	while (i < uiop->uio_iovcnt) {
		qprintf("iovec [%d] base 0x%x len %x\n",
			i, iovp->iov_base, iovp->iov_len);
		i++;
		iovp++;
	}
}
			
/*
 * Print out the arguments to an open call.
 */
struct opena {
	char		*fname;
	sysarg_t	fmode;
	usysarg_t	cmode;
};

void
idbg_open(struct opena *uap)
{
	qprintf("file %s create mode O%o\n", uap->fname, uap->cmode);
	print_fmode(uap->fmode);
}

/*
 * Print the args to a creat call.
 */
struct creata {
	char		*fname;
	sysarg_t	cmode;
};

void
idbg_creat(struct creata *uap)
{
	qprintf("file %s create mode 0%o\n", uap->fname, uap->cmode);
}

/*
 * Print the args to a mkdir call.
 */
struct mkdira {
	char		*dname;
	usysarg_t	dmode;
};

void
idbg_mkdir(struct mkdira *uap)
{
	qprintf("dir %s create mode 0%o\n", uap->dname, uap->dmode);
}

/*
 * Print the args to a link call.
 */
struct linka {
	char	*from;
	char	*to;
};

void
idbg_link(struct linka *uap)
{
	qprintf("from %s to %s\n", uap->from, uap->to);
}

/*
 * Print the args to a rename call.
 */
struct renamea {
	char	*from;
	char	*to;
};

void
idbg_rename(struct renamea *uap)
{
	qprintf("from %s to %s\n", uap->from, uap->to);
}

/*
 * Print the args to a symlink call.
 */
struct symlinka {
	char	*target;
	char	*linkname;
};

void
idbg_symlink(struct symlinka *uap)
{
	qprintf("target %s linkname %s\n", uap->target, uap->linkname);
}

/*
 * Print the args to an unlink call.
 */
struct unlinka {
	char	*fname;
};

void
idbg_unlink(struct unlinka *uap)
{
	qprintf("fname %s\n", uap->fname);
}

/*
 * Print the args to a rmdir call.
 */
struct rmdira {
	char	*dname;
};

void
idbg_rmdir(struct rmdira *uap)
{
	qprintf("dir name %s\n", uap->dname);
}

/*
 * Print the args to a stat call.
 */
struct xstatarg {
	sysarg_t		version;
	char			*fname;
	struct irix5_stat	*sb;
};

void
idbg_xstat(struct xstatarg *uap)
{
	qprintf("file %s version %d sb 0x%x\n",
		uap->fname, uap->version, uap->sb);
}

#ifdef DEBUG_BUFTRACE
void
buf_trace_entry(
	ktrace_entry_t	*ktep)
{		  
	qprintf("%s ra 0x%x cpu %d bp 0x%x flags ",
		(char *)(ktep->val[0]), ktep->val[9], ktep->val[1],
		ktep->val[2]);
	if (ktep->val[3] != 0) {
		_printflags((ulong)(ktep->val[3]), tab_bflags,"bflags");
		qprintf("\n");
	} else {
		qprintf("NO FLAGS SET\n");
	}
	qprintf("offset 0x%x%x bcount 0x%x bufsize 0x%x blkno 0x%x\n",
		ktep->val[4], ktep->val[5], ktep->val[6],
		ktep->val[7], ktep->val[8]);
	qprintf("forw 0x%x back 0x%x vp 0x%x\n",
		ktep->val[10], ktep->val[11], ktep->val[12]);
	qprintf("dforw 0x%x dback 0x%x lock %d\n",
		ktep->val[13], ktep->val[14], ktep->val[15]);
}

/*
 * Print out the last "count" entries in the chunk cache trace buffer.
 * The "a" is for "all" buffers.
 */
void
idbg_abuftrace(int count)
{
	ktrace_entry_t	*ktep;
	ktrace_snap_t	kts;
	int		nentries;
	int		skip_entries;
	extern ktrace_t	*buf_trace_buf;

	if (buf_trace_buf == NULL) {
		qprintf("The buffer trace buffer is not initialized\n");
		return;
	}
	nentries = ktrace_nentries(buf_trace_buf);
	if (count == -1) {
		count = nentries;
	}
	if ((count <= 0) || (count > nentries)) {
		qprintf("Invalid count.  There are %d entries.\n", nentries);
		return;
	}

	ktep = ktrace_first(buf_trace_buf, &kts);
	if (count != nentries) {
		/*
		 * Skip the total minus the number to look at minus one
		 * for the entry returned by ktrace_first().
		 */
		skip_entries = nentries - count - 1;
		ktep = ktrace_skip(buf_trace_buf, skip_entries, &kts);
		if (ktep == NULL) {
			qprintf("Skipped them all\n");
			return;
		}
	}
	while (ktep != NULL) {
		qprintf("\n");
		buf_trace_entry(ktep);
		ktep = ktrace_next(buf_trace_buf, &kts);
	}
}
			
/*
 * Print out all the entries in the chunk trace buf corresponding
 * to the given buffer.  The "s" is for "single" buffer.
 */
void
idbg_sbuftrace(buf_t *bp)
{
	ktrace_entry_t	*ktep;
	ktrace_snap_t	kts;
	extern ktrace_t	*buf_trace_buf;

	if (buf_trace_buf == NULL) {
		qprintf("The buffer trace buffer is not initialized\n");
		return;
	}

	qprintf("sbuftrace bp 0x%x\n", bp);
	ktep = ktrace_first(buf_trace_buf, &kts);
	while (ktep != NULL) {
		if ((buf_t*)(ktep->val[2]) == bp) {
			qprintf("\n");
			buf_trace_entry(ktep);
		}

		ktep = ktrace_next(buf_trace_buf, &kts);
	}
}

/*
 * Print out all the entries in the chunk trace buf corresponding
 * to the given vnode.
 */
void
idbg_vbuftrace(vnode_t *vp)
{
	ktrace_entry_t	*ktep;
	ktrace_snap_t	kts;
	extern ktrace_t	*buf_trace_buf;

	if (buf_trace_buf == NULL) {
		qprintf("The buffer trace buffer is not initialized\n");
		return;
	}

	qprintf("vbuftrace vnode 0x%x\n", vp);
	ktep = ktrace_first(buf_trace_buf, &kts);
	while (ktep != NULL) {
		if ((vnode_t*)(ktep->val[12]) == vp) {
			qprintf("\n");
			buf_trace_entry(ktep);
		}

		ktep = ktrace_next(buf_trace_buf, &kts);
	}
}

/*
 * Print out all the entries in the chunk trace buf corresponding
 * to the given block.
 */
void
idbg_blktrace(daddr_t blk)
{
	ktrace_entry_t	*ktep;
	ktrace_snap_t	kts;
	extern ktrace_t	*buf_trace_buf;

	if (buf_trace_buf == NULL) {
		qprintf("The buffer trace buffer is not initialized\n");
		return;
	}

	qprintf("blktrace blk 0x%x\n", blk);
	ktep = ktrace_first(buf_trace_buf, &kts);
	while (ktep != NULL) {
		if ((daddr_t)(ktep->val[8]) == blk) {
			qprintf("\n");
			buf_trace_entry(ktep);
		}

		ktep = ktrace_next(buf_trace_buf, &kts);
	}
}

/*
 * Print out the trace buffer attached to the given buffer.
 */
void
idbg_buftrace(buf_t *bp)
{
	ktrace_entry_t	*ktep;
	ktrace_snap_t	kts;

	if (bp->b_trace == NULL) {
		qprintf("The buffer trace buffer is not initialized\n");
		return;
	}

	qprintf("buftrace bp 0x%x\n", bp);
	ktep = ktrace_first(bp->b_trace, &kts);
	while (ktep != NULL) {
		qprintf("\n");
		buf_trace_entry(ktep);
		ktep = ktrace_next(bp->b_trace, &kts);
	}

}

/*
 * Print out all the entries in the chunk trace buf corresponding
 * to the given cpu.
 */
void
idbg_bufcpu(__psint_t cpuid)
{
	ktrace_entry_t	*ktep;
	ktrace_snap_t	kts;
	extern ktrace_t	*buf_trace_buf;

	if (buf_trace_buf == NULL) {
		qprintf("The chunk trace buffer is not initialized\n");
		return;
	}

	if ((cpuid < 0) || (cpuid > 36)) {
		qprintf("Invalid cpu id %d\n", cpuid);
	}

	qprintf("chunkcpu cpu %d\n", cpuid);
	ktep = ktrace_first(buf_trace_buf, &kts);
	while (ktep != NULL) {
		if ((__psint_t)(ktep->val[1]) == cpuid) {
			qprintf("\n");
			buf_trace_entry(ktep);
		}

		ktep = ktrace_next(buf_trace_buf, &kts);
	}
}
#endif /* DEBUG_BUFTRACE */

/*
 * Dump out memory and show symbols that show up in the memory.
 * This is useful for manually recreating stack traces.
 */
void
idbg_dumpsym(char **addr)
{
	int	i;
	int	j;

	for (i = 0; i < 25; i++) {
		qprintf("0x%x: ", addr);
		for (j = 0; j < 4; j++) {
			_prsymoff(*addr, NULL, NULL);
			qprintf(" ");
			addr++;
		}
		qprintf("\n");
	}
}

#ifdef VNODE_TRACING
/*
 * Print a vnode trace entry.
 */
static int
vn_trace_pr_entry(ktrace_entry_t *ktep)
{
	if ((__psint_t)ktep->val[0] == 0)
		return 0;
	switch ((__psint_t)ktep->val[0]) {
	case VNODE_KTRACE_ENTRY:
		qprintf("entry to %s v_count %d",
			(char *)ktep->val[1], (__psint_t)ktep->val[3]);
		break;
	case VNODE_KTRACE_HOLD:
		if ((__psint_t)ktep->val[3] != 1)
			qprintf("hold @%s:%d v_count %d => %d",
				(char *)ktep->val[1], (__psint_t)ktep->val[2],
				(__psint_t)ktep->val[3] - 1,
				(__psint_t)ktep->val[3]);
		else
			qprintf("get @%s:%d",
				(char *)ktep->val[1], (__psint_t)ktep->val[2]);
		break;
	case VNODE_KTRACE_REF:
		qprintf("ref @%s:%d v_count %d",
			(char *)ktep->val[1], (__psint_t)ktep->val[2],
			(__psint_t)ktep->val[3]);
		break;
	case VNODE_KTRACE_RELE:
		if ((__psint_t)ktep->val[3] != 1)
			qprintf("rele @%s:%d v_count %d => %d",
				(char *)ktep->val[1], (__psint_t)ktep->val[2],
				(__psint_t)ktep->val[3],
				(__psint_t)ktep->val[3] - 1);
		else
			qprintf("free @%s:%d",
				(char *)ktep->val[1], (__psint_t)ktep->val[2]);
		break;
	default:
		qprintf("unknown vntrace record\n");
		return 1;
	}
	qprintf(" ra=");
	_prsymoff((void *)(inst_t *)ktep->val[4], NULL, NULL);
	qprintf("  cpu=%d proc=%x ",
		(__psint_t)ktep->val[6],
		(proc_t *)ktep->val[7]);
	_printflags((__psunsigned_t)ktep->val[5], tab_vflags, "flag");
	return 1;
}

/*
 * Print out the trace buffer attached to the given vnode.
 */
void
idbg_vntrace(vnode_t *vp)
{
	ktrace_entry_t	*ktep;
	ktrace_snap_t	kts;

	if (vp->v_trace == NULL) {
		qprintf("The vnode trace buffer is not initialized\n");
		return;
	}

	qprintf("vntrace vp 0x%x\n", vp);
	ktep = ktrace_first(vp->v_trace, &kts);
	while (ktep != NULL) {
		if (vn_trace_pr_entry(ktep))
			qprintf("\n");
		ktep = ktrace_next(vp->v_trace, &kts);
	}
}
#endif /* VNODE_TRACING */

/*
++
++	kp callo n
++		n != -1: prints current callout table for cpu n
++		n == -1: prints free list
++
*/

#if !defined(CLOCK_CTIME_IS_ABSOLUTE)
extern callout_info_t *fastcatodo;
#endif /* CLOCK_CTIME_IS_ABSOLUTE */

void
idbg_callo(__psint_t x)
{
	extern int fastclock;
	register int s;
	extern zone_t *co_reserve_zone;

	s = splprof(); /* so we get good results for user land */
	if (x == -1L) {
		qprintf("Please specify a cpu\n");
	} else {
		qprintf("Free zone for cpu %d is at 0x%x\n", x, CI_FREE_ZONE(&CALLTODO(x)));
		qprintf("Callout todo list for cpu %d\n",x);
#if defined(CLOCK_CTIME_IS_ABSOLUTE)
		qprintf("Current time is %lld\n",absolute_rtc_current_time());
#endif
		dump_callo(CI_TODO_NEXT(&CALLTODO(x)));
		qprintf("Callout pending list for cpu %d\n",x);
		dump_callo(CI_PENDING_NEXT(&CALLTODO(x)));
#if !defined(CLOCK_CTIME_IS_ABSOLUTE)
		if (fastclock) {
			qprintf("Fast callout todo list\n");
			dump_callo(CI_TODO_NEXT(fastcatodo));
			qprintf("Fast callout pending list\n");
			dump_callo(CI_PENDING_NEXT(fastcatodo));
		}
#endif /* !CLOCK_CTIME_IS_ABSOLUTE */
	}
	splx(s);
}

static void
dump_callo(register struct callout *p1)
{
	__psunsigned_t offst;

	for (; p1; p1 = p1->c_next) {
		qprintf(" func:0x%x %s", p1->c_func,
			fetch_kname((void *)p1->c_func, &offst));
		qprintf(" (%x, %x, %x, %x)\n",
				p1->c_arg, p1->c_arg1,
				p1->c_arg2, p1->c_arg3);
		qprintf("   id 0x%x cid 0x%x cpuid %d flags 0x%x time %lld ",
			p1->c_id, p1->c_cid, p1->c_cpuid,
			p1->c_flags, p1->c_time);
		qprintf("owner %d pri %d next %x\n", p1->c_ownercpu, p1->c_pl, p1->c_next);
	}
}

static void dump_callout_info(callout_info_t *cip);

void
idbg_calloinfo(__psint_t x)
{
        callout_info_t *cip;

        if (x == -1L) {
		for (x = 0; x < maxcpus; x++) {
			cip = &CALLTODO(x);
			qprintf("Callout info @ 0x%x for cpu %d\n",
				cip, CI_CPU(cip));
			dump_callout_info(cip);
		}
#if !defined(CLOCK_CTIME_IS_ABSOLUTE)
		qprintf("Fast callout info @ 0x%x\n", fastcatodo,
			CI_CPU(fastcatodo));
		dump_callout_info(fastcatodo);
#endif /* CLOCK_CTIME_IS_ABSOLUTE */

        } else {
                cip = &CALLTODO(x);
		qprintf("Callout info @ 0x%x for cpu %d\n", cip, CI_CPU(cip));
		dump_callout_info(cip);
	}
}

static void
dump_callout_info(callout_info_t *cip)
{

	struct callout *p1;
	ci_itinfo_t *citp;

	qprintf(" todo_lock at 0x%x  free_zone at 0x%x\n", &cip->ci_listlock, cip->ci_free_zone);
	qprintf(" flags 0x%x cpu %d sema 0x%x\n", cip->ci_flags,
		CI_CPU(cip), CI_SEMA(cip));
	qprintf(" ithrd_cnt %d\n", cip->ci_ithrd_cnt);
	p1 = CI_TODO_NEXT(cip);
	qprintf(" todo @ 0x%x base time %lld\n", CI_TODO(cip),
		(p1) ? p1->c_time : 0);
	qprintf(" pending @ 0x%x\n", CI_PENDING(cip));
	for (citp = cip->ci_ithrdinfo;
	     citp < &cip->ci_ithrdinfo[CA_ITHRDS_PER_LIST];
	     citp++)
		qprintf("  ithrd @ 0x%x sync @ 0x%x flags 0x%x toid 0x%x\n",
			citp->cit_ithread, &citp->cit_sync, citp->cit_flags,
			citp->cit_toid);
}

/*
++
++      kp ptimer n
++              n is pid of proc to look at
++
*/
void
idbg_ptimer(__psint_t x)
{
        struct proc *p;
        struct ptimer_info *pinfo, *rpinfo;
        int i;

        p = idbg_pid_to_proc(x);
        rpinfo = p->p_ptimers;
        qprintf("ptimer: proc slot %d addr of p_ptimer 0x%lx\n",x,rpinfo);
        if (rpinfo == NULL) {
                qprintf("No posix timers for this process \n");
                return;
        }
        qprintf("slot       next_timeout      interval_tick  next_toid overrun_cnt type signo      value\n");
        for (i = 0; i < _SGI_POSIX_TIMER_MAX; i++) {
                if (rpinfo[i].clock_type) {
                        pinfo = &rpinfo[i];
			qprintf("%4d %18lld %18lld %10ld %11d %4d %5d %18d\n",
                                i, pinfo->next_timeout, pinfo->interval_tick,
                                pinfo->next_toid,pinfo->overrun_cnt,
				pinfo->clock_type, pinfo->signo, pinfo->value);
                }
        }

}

/*
++
++	kp file arg
++		arg == -1: prints out all valid file descriptors
++		arg == -2: print all file desc that have msgcounts
++		arg > 0  : prints out open files for proc with pid <arg>
++		arg < 0  : prints out file structure pointed to by arg
++		arg == 0 : prints freelist (obsolete)
++
*/

static int printfp(vfile_t *, int, int);

void
idbg_file(__psint_t x)
{
	fdt_t  *fdt;
	vfile_t *fp;
	int i;
	proc_t *p;

	if (x == -1L || x == -2L) {
		/* go through open files */
		if (x == -2L)
			qprintf("Open files with msgcounts\n");
		else
			qprintf("System open files\n");
#ifdef DEBUG
		for (fp = activefiles, i = 0; fp != NULL; fp = fp->vf_next, i++)
			printfp(fp, i, x == -1L ? 0 : 1);
		if (x == -1L)
			qprintf("%d active files\n", i);
#endif
	} else if (x < 0) {
		/* print out fp pointed to by addr */
		printfp((vfile_t *)x, 0, 0);
	} else if (x == 0) {
		qprintf("System file table freelist is obsolete\n");
	} else {
		/* print out a process' open file table */
		if ((p = idbg_pid_to_proc(x)) == NULL) {
			qprintf("0x%x is not an active proc\n", x);
			return;
		}

		if (ISSHDFD(&p->p_proxy))
			fdt = p->p_shaddr->s_fdt;
		else
			fdt = p->p_fdt;

		for (i = 0; i < fdt->fd_nofiles; i++) {
			fp = fdt_get_idbg(fdt, i);
			if (fp != NULL)
				printfp(fp, i, 0);
		}
	}
}

int
print_procfdt(
	proc_t	*p,
	void	*arg,
	int	ctl)
{
	vnode_t		*vp;
	vfile_t		*fp;
	fdt_t		*fdt;
	int		i;

	if (ctl == 1) {
		vp = arg;
		if (ISSHDFD(&p->p_proxy))
			fdt = p->p_shaddr->s_fdt;
		else
			fdt = p->p_fdt;
		if (fdt == NULL)
			return 0;

		for (i = 0; i < fdt->fd_nofiles; i++) {
			fp = fdt_get_idbg(fdt, i);
			if (fp != NULL && VF_DATA_EQU(fp, vp)) {
				qprintf("proc:0x%x  ", p);
				printfp(fp, i, 0);
			}
		}
	}
	return(0);
}

/*
 * Find and print out the file structs that point at the given vnode.
 * We find them via the file table so that we'll find them all, and then
 * we find them via the proc active list so that we can print which proc has
 * the reference to the vnode.
 */
void
idbg_vfile(__psint_t ptr)
{
	vnode_t			*vp;

	vp = (vnode_t *)ptr;
	if (vp == (vnode_t*)-1L) {
		qprintf("Usage: vfile <vnode address>\n");
		return;
	}

	/*
	 * Go through open files and print out any that match the
	 * given vnode.
	 */
	qprintf("Results of search by file struct list:\n");
#ifdef DEBUG
	{
	vfile_t			*fp;
	int			i;
	for (i = 0, fp = activefiles; fp != NULL; fp = fp->vf_next) {
		if (VF_DATA_EQU(fp, vp)) {
			printfp(fp, i, 0);
			i++;
		}
	}
	}
#endif

	qprintf("\nResults of search by proc active list\n");
	idbg_procscan(print_procfdt, vp);
}


char *
ltype_to_str(int ltype)
{
	static char *str;
	static char badbuf[11];

	switch (ltype) {
		case F_RDLCK:
			str = "F_RDLCK";
			break;
		case F_WRLCK:
			str = "F_WRLCK";
			break;
		case F_UNLCK:
			str = "F_UNLCK";
			break;
		default:
			str = badbuf;
			sprintf(badbuf, "0x%x", ltype);
	}
	return(str);
}

/*
++
++	kp filock addr
++		addr < 0  : prints out filock structure pointed to by addr
++
*/

/*ARGSUSED*/
void
idbg_filock(__psint_t addr)
{
	if (addr < 0) {
		/* addr is the head of a filock queue */
		struct filock *tmp;

		for (tmp = (struct filock *)addr; tmp; tmp = tmp->next) {
			qprintf("0x%x: %s %lld,%lld sysid 0x%x pid %d wflg %d\n",
				tmp, ltype_to_str(tmp->set.l_type), tmp->set.l_start,
				tmp->set.l_end, tmp->set.l_sysid, tmp->set.l_pid,
				tmp->stat.wakeflg);
		}
	}
}

void
idbg_sleeplcks(__psint_t addr)
{
	struct filock *tmp;

	if (addr < 0) {
		/* addr is the head of a filock queue */
		for (tmp = (struct filock *)addr; tmp; tmp = tmp->next) {
			qprintf(
				"0x%x: %s %lld,%lld sysid 0x%x pid %d blker 0x%p vp 0x%p%s\n",
				tmp, ltype_to_str(tmp->set.l_type), tmp->set.l_start,
				tmp->set.l_end, tmp->set.l_sysid, tmp->set.l_pid,
				tmp->stat.blk.blocked, tmp->stat.blk.vp,
				tmp->stat.blk.cancel ? " cancelled" : "");
		}
	}
}

void
idbg_flock(__psint_t addr)
{
	flock_t *tmp = (flock_t *)addr;

	if (addr < 0) {
		qprintf("0x%x: %s %ld,%ld sysid 0x%x pid %d\n", tmp,
			ltype_to_str(tmp->l_type), tmp->l_start,
			tmp->l_end, tmp->l_sysid, tmp->l_pid);
	}
}

char	*tab_fflags[] = {
	"RD",		/* 0x0001 */
	"WR",		/* 0x0002 */
	"NDELAY",	/* 0x0004 */
	"APPEND",	/* 0x0008 */
	"SYNC",		/* 0x0010 */
	"UNK",		/* 0x0020 */
	"UNK",		/* 0x0040 */
	"NONBLOCK",	/* 0x0080 */
	"UNK",		/* 0x0100 */
	"UNK",		/* 0x0200 */
	"INPROGRESS",	/* 0x0400 */
	"GRIO/NOCTTY",	/* 0x0800 */
	"UNK",		/* 0x1000 */
	"DEFER",	/* 0x2000 */
	"PRIO",		/* 0x4000 */
	"DIRECT",	/* 0x8000 */
};

static int
printfp(register vfile_t *fp, int slot, int flag)
{
	/*REFERENCED*/
	off_t	offset;

	if (flag == 1 && fp->vf_msgcount == 0)
		return 0;
	qprintf("%d:0x%x cnt %d vno/vso 0x%x ",
		slot, fp, fp->vf_count, fp->__vf_data__);
	_printflags((unsigned)fp->vf_flag, tab_fflags, "flags");
	if (VF_IS_VNODE(fp)) {
		vnode_t *vp = VF_TO_VNODE(fp);
		if (vp != NULL)
			qprintf("<%s>\n", tab_vtypes[vp->v_type]);
	} else
		qprintf("\n");

	if (flag == 1)
		qprintf("	msgc %d\n", fp->vf_msgcount);
	else {
		off_t	offset;
		if (!VF_IS_VNODE(fp))
			qprintf("	no offset - is a socket\n");
		else if ((fp->vf_bh.bh_first != NULL) &&
			 (BHV_POSITION(fp->vf_bh.bh_first) == BHV_POSITION_BASE)) {
			VFILE_GETOFFSET(fp, &offset);
			qprintf("	offset %llx\n", offset);
		} else
			qprintf("	offset distributed\n");
	}
#ifdef CKPT
	qprintf("	parent dir id %d\n", fp->vf_ckpt);
#endif
	qprintf("	iopri %d\n", fp->vf_iopri);
	/* print behavior stuff */
	idbg_bhvh((__psint_t)&fp->vf_bh);

	qprintf("\n");
	return 0;
}

/*
++
++	kp map map-address
++		Print map structure array
++		Assumes that addr is beginning of map array.
++
*/
void
idbg_map(p)
struct map *p;
{
	struct map *q;

	qprintf("map size 0x%x, map lock 0x%x, mapwant sema 0x%x\n",
		mapsize(p), maplock(p), mapout(p));
	for (q = mapstart(p); q->m_size; q++) {
		qprintf("size 0x%x addr 0x%x\n", q->m_size, q->m_addr);
	}
}

/*
++
++	kp map map-address
++		Print map structure array
++		Assumes that addr is beginning of map array.
++
*/
void
idbg_dobitmap(struct bitmap *bp, char *name)
{
	register int i;

	qprintf(" bitmap @ 0x%x, name is \"%s\", map @ 0x%x\n",
		bp, name, bp->m_map);
	qprintf("   unit0 0x%x size 0x%x count 0x%x ",
		bp->m_unit0, bp->m_size, bp->m_count);
	if (bp->m_lowbit == -1) {
		qprintf(" high 0x%x\n   rotors:", bp->m_highbit);
		for (i = 0; i < SPTROTORS; i++)
			qprintf(" 0x%x", bp->m_startb[i]);
		qprintf("\n");
	} else {
		qprintf("low 0x%x rotor 0x%x high 0x%x\n",
			bp->m_lowbit, bp->m_rotor, bp->m_highbit);
	}
}

void
idbg_chbtmp(__psint_t x)
{
	struct bitmap	*bm;
	int		size;
	int		count;
	int		cmpcnt;

	if (x < 0L) {
		bm = (struct bitmap *)x;
		size = bm->m_size;
		count = bm->m_count;
		cmpcnt = bfcount(bm->m_map, 0, size);
		qprintf("Computed count 0x%x\n", cmpcnt);
		if (cmpcnt != count) 
			qprintf("ALERT : Computed != Registered\n");
	} else {
		qprintf("usage: chbtmp <addr-of-bitmap-struct>\n");
	}
}

void
idbg_bitmap(__psint_t x)
{
	struct sysbitmap *p;
#ifdef R4000
	int nmaps;
	char buf[30];
#endif

	if (x < 0) {
		if (x == -1L) {
all:
			p = &sptbitmap;
		} else
			p = (struct sysbitmap *)x;

		qprintf("sysbitmap @0x%x gen %d &sema 0x%x waiters %d &sv 0x%x\n",
			p, p->m_gen, &p->sptmaplock, p->spt_wait.waiters,
			&p->spt_wait.wait);
		idbg_dobitmap(&p->sptmap, "sptmap");
		idbg_dobitmap(&p->temp_sptmap, "tempmap");
		idbg_dobitmap(&p->aged_sptmap, "agedmap");
		idbg_dobitmap(&p->stale_sptmap, "stalemap");
#ifdef R4000
		for (nmaps = 0; nmaps < CACHECOLORSIZE; nmaps++) {
			sprintf(buf, "clean color %d", nmaps);
			idbg_dobitmap(p->sptmap.m_color[nmaps], buf);
			sprintf(buf, "temp color %d", nmaps);
			idbg_dobitmap(p->temp_sptmap.m_color[nmaps], buf);
			sprintf(buf, "aged color %d", nmaps);
			idbg_dobitmap(p->aged_sptmap.m_color[nmaps], buf);
			sprintf(buf, "stale color %d", nmaps);
			idbg_dobitmap(p->stale_sptmap.m_color[nmaps], buf);
		}
#endif
		return;
	}
	if (x == 0)
		idbg_dobitmap(&sptbitmap.sptmap, "sptmap");
	else if (x == 1)
		idbg_dobitmap(&sptbitmap.aged_sptmap, "agedmap");
	else if (x == 2)
		idbg_dobitmap(&sptbitmap.stale_sptmap, "stalemap");
	else if (x == 3)
		idbg_dobitmap(&sptbitmap.temp_sptmap, "tempmap");
	else
		goto all;
}

void
idbg_svdump(sv_t *svp)
{
	register kthread_t *kt;
	svmeter_t *kp = svmeter(svp);
	char *name = wchanname(svp, KT_WSV);

	qprintf(" sv 0x%x [%s] %s ", svp, name,
		sv_islocked(svp) ? "locked" : "unlocked");

	switch (svp->sv_queue & SV_TYPE) {
	case SV_FIFO :
		qprintf("type: fifo\n");
		break;
	case SV_LIFO :
		qprintf("type: lifo\n");
		break;
	case SV_PRIO :
		qprintf("type: prio\n");
		break;
	case SV_KEYED :
		qprintf("type: keyed\n");
		break;
	}

	if (SV_WAITER(svp)) {
		qprintf(" waiters: ");
		for (kt = SV_WAITER(svp); kt; kt = kt->k_flink) {
			qprintf(" 0x%x", kt);
			if (kt->k_flink == SV_WAITER(svp))
				break;
		}
	} else
		qprintf(" no waiters: ");

	qprintf("\n");

	if (kp) {
		qprintf(
"  wait %d waitsig %d waitcnt %d maxwaitcnt %d\n",
			kp->sm_wait, kp->sm_waitsig,
			kp->sm_nwait, kp->sm_maxnwait);
		qprintf(
"  signals %d signalled %d broadcasts %d broadcasted %d\n",
			kp->sm_signal, kp->sm_signalled,
			kp->sm_broadcast, kp->sm_broadcasted);
		qprintf(
"  wsync %d wsyncd %d unsyncv %d unsyncvd %d\n",
			kp->sm_wsyncv, kp->sm_wsyncvd,
			kp->sm_unsyncv, kp->sm_unsyncvd);
		printid(kp->sm_thread, kp->sm_id, "  last waker");
		qprintf(" from ");
		_prsymoff((void *)kp->sm_lastpc,NULL,NULL); 
		qprintf("\n");
	}
}

/*
++
++	kp pgwait P
++		P is absolute page wait address
++		or a page wait struct index
++
*/
void
idbg_pwait(__psint_t p1)
{
	sv_t *pwtsv;

	if (p1 < 0) {
		pwtsv = (sv_t *)p1;
		if (pwtsv < &pfdwtsv[0] || pwtsv > &pfdwtsv[PWAITMASK]) {
			qprintf("arg 0x%x is not a page wait syncvar\n", p1);
			return;
		}
	} else {
		if (p1 > PWAITMASK) {
			qprintf("arg 0x%x is not a page wait syncvar\n", p1);
			return;
		}
		pwtsv = &pfdwtsv[p1];
	}

	qprintf(" page wait struct %d: \n", pwtsv - pfdwtsv);
	idbg_svdump(pwtsv);
}


/* ARGSUSED */
void
idbg_page_counter(__psint_t p1)
{
#ifdef EVEREST
	page_counter_t *pcp;
	int		counter;
	int		i;
	int		j;
	__uint64_t	val;

	if (!io4ia_war || !io4ia_userdma_war) {
		qprintf("The page counters are not enabled\n");
		return;
	}

	if (p1 < 0) {
		pcp = (page_counter_t *)p1;
	} else {
		pcp = &page_counters[p1];
	}

	counter = 0;
	for (i = 0; i < PC_NUM_WORDS; i++) {
		for (j = 0; j < PC_COUNTERS_PER_WORD; j++) {
			val = pcp->pc_words[i];
			val >>= (j * 2);
			val &= 3ULL;
			qprintf("%d:%d ", counter, (int)val);
			if (j == 15) {
				qprintf("\n");
			}
			counter++;
		}
		qprintf("\n");
	}
#endif
}

void
idbg_page_counters_set(void)
{
#ifdef EVEREST
	int		i;
	int		j;
	page_counter_t	*pcp;
	int		set;
	int		num_set;

	if (!io4ia_war || !io4ia_userdma_war) {
		qprintf("The page counters are not enabled\n");
		return;
	}

	for (i = 0; i < num_page_counters; i++) {
		pcp = &(page_counters[i]);
		set = 0;
		for (j = 0; j < PC_NUM_WORDS; j++) {
			if (pcp->pc_words[j] != 0ULL) {
				set = 1;
			}
		}
		if (set) {
			qprintf("%d ", i);
			num_set++;
			if ((num_set % 8) == 0) {
				qprintf("\n");
			}
		}
	}
#endif
}

/*
 * Semaphore history log.
++
++	kp semlog n - dump semaphore history log (last n entries)
++
 */
extern semahist_t *semahist;
extern unsigned int histpos;
extern unsigned int histmax;
unsigned shdump_lastts;

void
idbg_shdump(__psint_t n)
{
   register int i;
   register semahist_t *sp;
   extern int dispseq;
   sema_t *pp;

	qprintf("semaphore history log dump\n");
	qprintf("current sequence number is %d\n", dispseq);
	shdump_lastts = 0;
	if (n == -1L) {
		pp = (sema_t *)0;
		n = histmax;
	} else if (n < 0) {
		pp = (sema_t *)n;
		n = histmax;
	} else {
		pp = 0;
		n = (histmax > n ? n : histmax);
	}
	for (i = histpos-1; n > 0 && i >= 0; i--, n--) {
		sp = &semahist[i];
		if (pp && (sp->h_sid != pp))
			continue;
		idbgplog(sp);
	}
	for (i = histmax-1; n > 0 && i >= histpos; i--, n--) {
		sp = &semahist[i];
		if (pp && (sp->h_sid != pp))
			continue;
		idbgplog(sp);
	}
}

/*
++
++	kp semchain addr - dump semaphore history for individual semaphore
++			addr < 0 use addr as address of semaphore
++			addr > 0 use addr as index into history buffer
++
 */
void
idbg_semchain(__psint_t x)
{
   register int	i;
   register int	n;

	if (x < 0) {
		n = histmax;
		for (i = histpos-1; n > 0 && i >= 0; i--, n--)
			if ((__psint_t) semahist[i].h_sid == x) {
				scanchain(i);
				return;
			}
		for (i = histmax-1; n > 0 && i >= histpos; i--, n--)
			if ((__psint_t) semahist[i].h_sid == x) {
				scanchain(i);
				return;
			}
		qprintf("semaphore 0x%x not found in log\n", x);
	}
	else {
		if (x > histmax) {
			qprintf("invalid log index %d\n", x);
			return;
		}
		scanchain(x);
	}
}

/*
++
++	kp nschain s - dump semaphore history for individual semaphore
++			s - name of semaphore
++
 */
void
idbg_nschain(char *s)
{
	register int i;
	register int n;
	k_semameter_t *kp;
	char *t;
	char *val;

	if (s == 0 || *s == '\0') {
		qprintf("named semaphore chain needs a string pointer\n");
		return;
	}
	n = histmax;
	for (i = histpos-1; n > 0 && i >= 0; i--, n--) {
		t = s;
		kp = smeter(semahist[i].h_sid);
		if (!kp)
			continue;
		val = kp->s_base.name;
		if (*val == '\0')
			continue;
		while (*t != '\0' && *val != 0) {
			if (*t == *val)
				val++;
			t++;
		}
		if (*val == '\0') {
			scanchain(i);
			return;
		}
	}
	for (i = histmax-1; n > 0 && i >= histpos; i--, n--) {
		t = s;
		kp = smeter(semahist[i].h_sid);
		if (!kp)
			continue;
		val = kp->s_base.name;
		if (*val == '\0')
			continue;
		while (*t != '\0' && *val != 0) {
			if (*t == *val)
				val++;
			t++;
		}
		if (*val == '\0') {
			scanchain(i);
			return;
		}
	}
	qprintf("semaphore name containing \"%s\" not found in log\n", s);
}

static void
scanchain(register int x)
{
	register semahist_t *hp;
	int cnt;

	hp = &semahist[x];
	qprintf("semaphore chain for 0x%x, starting at index %d\n",
		hp->h_sid, x);
	shdump_lastts = 0;
	cnt = 0;
	do {
		idbgplog(hp);
		hp = hp->h_blink;
	} while (hp != 0 && cnt++ < histmax &&
		(__psint_t) hp >= (__psint_t) semahist &&
		(__psint_t) hp < (__psint_t) &semahist[histmax] &&
		hp->h_sid == semahist[x].h_sid);
}

char *tbl_shop[] = {
	"",
	"psema",
	"vsema",
	"cpsema",
	"cvsema",
	"wsema",
	"unsema",
	"init",
	"free",
	"",
	"",
};

char *tbl_subop[] = {
	"",
	"hit",
	"wait",
	"noslp",
	"wake",
	"miss",
	"awake",
	"presig",
	"postsig",
	"remov",
	"",
};

void
idbgplog(semahist_t *hp)
{
	char *name = wchanname(hp->h_sid, KT_WSEMA);
	unsigned int elapsed;

	qprintf("%s(%s) sem 0x%x [%s] ",
		tbl_shop[hp->h_op], tbl_subop[hp->h_subop],
		hp->h_sid, name);
	printid(hp->h_thread, hp->h_id, "thread");
	qprintf(" cpu %d callpc 0x%x\n", hp->h_cpuid, hp->h_callpc);

	qprintf("  blink 0x%x seq %d ", hp->h_blink, hp->h_seq);
	printid(hp->h_woken, hp->h_wid, "woken");

	if (shdump_lastts != 0)
		elapsed = shdump_lastts - hp->h_ts;
	else
		elapsed = 0;
	shdump_lastts = hp->h_ts;
	qprintf("  delta %d usec\n", (elapsed*timer_unit)/1000);
}

/*
 * This is the set of semaphore control functions that we are defining.
 */
static int semctl_apply(char *, void (*)(), int, char **);
static void semctl_describe(sema_t *, int, char **);
static void semctl_ctlmeter(int, char **);
static void semctl_defhist(int, char **);
static void semctl_defkhist(int, char **);
static void semctl_defkhlen(int, char **);
static void semctl_defmeter(int, char **);
static void semctl_help(int, char **);
static void semctl_histmax(int, char **);
static void semctl_sema(int, char **);
static void semctl_semlog(int, char **);
static void semctl_what(int, char **);

static struct idbgcommfunc idbgSemCtlFunc[] = {
    "?",	VD semctl_help,
    	STRING_ARGS,	"Help (print this list)",
    "??",	VD semctl_help,
    	STRING_ARGS,	"Help (print this list)",
    "clrkhist", VD semctl_ctlmeter,
    	STRING_ARGS,	"Clear semaphore khist",
    "clrmeter", VD semctl_ctlmeter,
    	STRING_ARGS,	"Clear semaphore meter",
    "defhist",	VD semctl_defhist,
    	STRING_ARGS,	"Set default history",
    "defkhist",	VD semctl_defkhist,
    	STRING_ARGS,	"Set default khist",
    "defkhlen",	VD semctl_defkhlen,
    	STRING_ARGS,	"Set default khistlen",
    "defmeter",	VD semctl_defmeter,
    	STRING_ARGS,	"Set default metering",
    "help",	VD semctl_help,
    	STRING_ARGS,	"Help (print this list)",
    "histmax",	VD semctl_histmax,
    	STRING_ARGS,	"Set length of sema hist",
    "histoff",	VD semctl_ctlmeter,
    	STRING_ARGS,	"Turn semaphore hist OFF",
    "histon",	VD semctl_ctlmeter,
    	STRING_ARGS,	"Turn semaphore hist ON",
    "howmany",	VD semctl_ctlmeter,
    	STRING_ARGS,	"Return number of semas that match",
    "khist",	VD semctl_ctlmeter,
    	STRING_ARGS,	"Enable/disable khist",
    "khistarg", VD semctl_ctlmeter,
    	STRING_ARGS,	"Setup khist (policy, base, offset)",
    "khisthot", VD semctl_ctlmeter,
    	STRING_ARGS,	"Return khist usage data on semas",
    "sema",	VD semctl_sema,
    	STRING_ARGS,	"Dump semaphore struct",
    "semlog",	VD semctl_semlog,
    	STRING_ARGS,	"Dump semaphore log",
    "what",	VD semctl_what,
    	STRING_ARGS,	"Print list of semas matching expr",
    0,		0,	0,			0
};

static void semctl_ctlmeter_aux(sema_t *, int, char **);

#define KHIST_HOLD	1
#define KHIST_WAIT	2
#define KHIST_COUNT	3

/*
 * This function is the entry point to controlling semaphore mechanism
 * behavior.
 */
/* ARGSUSED */
void
idbg_semctl(__psint_t arg0, char *arg1)
{
	struct idbgcommfunc *comm;
	char *av[8];
	int ac = 8;

	if (idbginvoke != IDBGUSER) {
		qprintf("semctl only valid for user level idbg\n");
		return;
	}

	if (!arg1) {
		qprintf("Usage (TBD)\n");
		return;
	}
	
	strtoargs(arg1, &ac, av);
	if (ac == 0) {
		qprintf("Usage (TBD)\n");
		return;
	}
	for (comm = idbgSemCtlFunc; comm->comm_name; comm++) {
		if (strcmp(av[0], comm->comm_name))
			continue;
		(*comm->comm_func)(ac, av);
		return;
	}
	
	qprintf("Unknown control '%s'\n", av[0]);
	return;
}

static int
semctl_apply(char *str, void (*func)(), int ac, char **av)
{
	mpkqueuehead_t *bp;
	int bi;
	sema_t *sp;
	char buf[1024];
	int applied;

	if (isdigit(*str)) {
		sp = (sema_t *)(__psint_t)strtoull(str, (char **)NULL, 0);
		(*func)(sp, ac, av);
		return -1;	/* Avoid printing "matched" string */
	}
	
	(void)glob2regex(buf, str);
	if (re_comp(buf)) {
		qprintf("Error in reg expr '%s'\n", str);
		return -1;
	}

	/*
	 * XXX This is not protected by any lock -- there
	 * is some question as to whether we can wrap the
	 * loop with a lock or must give and release the lock
	 * since we don't know what (*func) will do.
	 */
	applied = 0;
	for (bi = 0, bp = semainfolist; bi < semabuckets; bi++, bp++) {
		kqueue_t *lp;
		for (lp = kqueue_first(&bp->head);
		     lp != kqueue_end(&bp->head);
		     lp = kqueue_next(lp)) {
			wchaninfo_t *winfo = baseof(wchaninfo_t, list, lp);
			if (winfo->name[0] && re_exec(winfo->name)) {
				(*func)((sema_t *)winfo->wchan, ac, av);
				applied++;
			}
		}
	}

	return applied;
}

/* ARGSUSED */
static void
semctl_describe(sema_t *sp, int ac, char **av)
{
	qprintf("semaphore 0x%s [%s]\n",
		padnum((__psint_t)sp, 16, SZOFPSINT_HEX),
		_padstr(wchanname(sp, KT_WSEMA), 16));
}

static void
semctl_ctlmeter(int ac, char **av)
{ 
	int result;
	unsigned int base;
	unsigned int offset;
	unsigned short length;
	unsigned short policy;
	unsigned short type;
	int matched = 0;
	int showmatch = 0;
	int error;

	if (ac < 2) {
		qprintf("Missing expr\n");
		return;
	}

	/*
	 * Parse as needed.
	 */	
	if (!strcmp(av[0], "khist")) {
		switch (ac) {
		case 4:
			if (strcmp(av[2], "on")) {
				qprintf("Arguments: on|off [length]\n");
				return;
			}
			length = (unsigned short)strtoull(av[3],
							(char **)NULL, 0);
			if (length > 256) {
				qprintf("Max length is 256\n");
				return;
			}
			av[3] = (char *)&length;
			
			/*
			 * Check to see if we support sema khist
			 */
			if ((error = semakhistinit(0, 0)) != 0) {
				switch (error) {
				case ENXIO:
					qprintf("Sema khist not supported in "
						"this kernel\n");
					return;
				default:
					qprintf("Error %d trying to initialize"
						"khist", error);
					return;
				}
			}
			break;
		case 3: if (!strcmp(av[0], "off"))
				break;
			/* fall thru */
		default:
			qprintf("Arguments: on|off [length]\n");
			return;
		}
	} else if (!strcmp(av[0], "khistarg")) {
		if (ac != 6) {
	             khistusage:
			qprintf("Arguments: hold|wait|count add|mult ");
			qprintf("<base> <offset>\n");
			return;
		}
		if (!strcmp(av[2], "hold"))
			type = KHIST_HOLD;
		else if (!strcmp(av[2], "wait"))
			type = KHIST_WAIT;
		else if (!strcmp(av[2], "count"))
			type = KHIST_COUNT;
		else
			goto khistusage;
		if (!strcmp(av[3], "add"))
			policy = HP_ADDOFF;
		else if (!strcmp(av[3], "mult"))
			policy = HP_MULOFF;
		else
			goto khistusage;
		base = (unsigned int)strtoull(av[4], (char **)NULL, 0);
		offset = (unsigned int)strtoull(av[5], (char **)NULL, 0);
		av[2] = (char *)&type;
		av[3] = (char *)&policy;
		av[4] = (char *)&base;
		av[5] = (char *)&offset;
	} else if (!strcmp(av[0], "khisthot")) {
		if (ac != 4 && ac != 5) {
	             khisthotusage:
			qprintf("Arguments: hold|wait|count <value> ");
			qprintf("[short|long|verylong]\n");
			return;
		}
		if (!strcmp(av[2], "hold"))
			type = KHIST_HOLD;
		else if (!strcmp(av[2], "wait"))
			type = KHIST_WAIT;
		else if (!strcmp(av[2], "count"))
			type = KHIST_COUNT;
		else
			goto khisthotusage;
		base = (unsigned int)strtoull(av[3], (char **)NULL, 0);
		if (ac == 5) {
			if (!strcmp(av[4], "verylong")) {
				av[4] = (char *)-1L;
			} else if (!strcmp(av[4], "long")) {
				av[4] = 0;
			} else if (!strcmp(av[4], "short")) {
				av[4] = (char *)&matched;
				showmatch = 1;
			}
		} else {
			av[4] = (char *)&matched;
			showmatch = 1;
		}
		av[2] = (char *)&type;
		av[3] = (char *)&base;
	}
	
	/*
	 * Apply operation to semaphores.
	 */
	result = semctl_apply(av[1], semctl_ctlmeter_aux, ac, av);
	if (result >= 0) {
		if (showmatch) {
			qprintf("[%d/%d sema%s matched]\n",
				matched, result, ((result == 1) ? "" : "s"));
		} else {
			qprintf("[%d sema%s matched]\n",
				result, ((result == 1) ? "" : "s"));
		}
	}
	return;
}

static void
semctl_ctlmeter_aux(sema_t *sp, int ac, char **av)
{
	register k_semameter_t *km = smeter(sp);
	register int s;

	/*
	 * Perform operation based on the type of argument that
	 * we're given.
	 */
	if (!strcmp(av[0], "clrkhist")) {
		semakhistclr(sp);
	} else if (!strcmp(av[0], "clrmeter")) {
		/*
		 * Clear the history information.
		 */
		if (!km)
			return;
		/* If structure changes, this bzero must also change */
		s = sema_lock(sp);
		bzero((char *)&km->s_data, sizeof(struct k_semameter_data));
		sema_unlock(sp, s);
	} else if (!strcmp(av[0], "histoff")) {
		semahistoff(sp);
	} else if (!strcmp(av[0], "histon")) {
		semahiston(sp);
	} else if (!strcmp(av[0], "howmany")) {
		/* Do nothing */
	} else if (!strcmp(av[0], "khist")) {
		unsigned short length;
		switch (ac) {
		case 4:	/* on */
			length = *(unsigned short *)av[3];
			semakhistinit(sp, length);
			break;
		case 3: /* off */
			/* XXX */
			break;
		}
	} else if (!strcmp(av[0], "khistarg")) {
		unsigned short type = *(unsigned short *)av[2];
		unsigned short policy = *(unsigned short *)av[3];
		unsigned int base = *(unsigned int *)av[4];
		unsigned int offset = *(unsigned int *)av[5];
		k_histo_t *kp;
		k_histelem_t *khp;
		if (!(kp = HMETER(km))) {
			return;
		}
		switch (type) {
		case KHIST_HOLD:
			khp = &kp->h_hold;
			base = (base * 1000) / timer_unit;
			offset = (offset * 1000) / timer_unit;
			break;
		case KHIST_WAIT:
			khp = &kp->h_waiting;
			base = (base * 1000) / timer_unit;
			offset = (offset * 1000) / timer_unit;
			break;
		case KHIST_COUNT:
			khp = &kp->h_count;
			break;
		}
		s = sema_lock(sp);
		khp->he_policy = policy;
		khp->he_base = base;
		khp->he_offset = offset;
		CLRHISTOLOG(khp);
		kp->h_ts = 0;
		kp->h_pc = 0;
		sema_unlock(sp, s);
	} else if (!strcmp(av[0], "khisthot")) {
		unsigned short type = *(unsigned short *)av[2];
		unsigned int base = *(unsigned int *)av[3];
		k_histo_t *kp;
		k_histelem_t *khp;
		int timed = 1;
		if (!(kp = HMETER(km))) {
			return;
		}
		switch (type) {
		case KHIST_HOLD:
			khp = &kp->h_hold;
			base = (base * 1000) / timer_unit;
			break;
		case KHIST_WAIT:
			khp = &kp->h_waiting;
			base = (base * 1000) / timer_unit;
			break;
		case KHIST_COUNT:
			khp = &kp->h_count;
			timed = 1;
			break;
		}
		if (!khp->he_updates)
			return;
		if ((int)(khp->he_total/khp->he_updates) > base) {
			switch ((__psint_t)av[4]) {
			case 0:	/* long */
				semctl_describe(sp, 0, 0);
				break;
			case -1: /* verylong */
				semctl_describe(sp, 0, 0);
				idbg_dumphisto(khp, 0, timed);
				break;
			default: /* short */
				*((unsigned int *)av[4]) += 1;
				break;
			}
		}
	}
	
	return;
}

static void
semctl_defhist(int ac, char **av)
{
	extern unsigned short semadefhist;
	
	if (ac > 2) {
		qprintf("Argument: on|off\n");
		return;
	}
	
	if (ac == 2) {
		if (!strcmp("on", av[1]) || !strcmp("ON", av[1]))
			semadefhist &= ~SEMA_NOHIST;
		else if (!strcmp("off", av[1]) || !strcmp("OFF", av[1]))
			semadefhist |= SEMA_NOHIST;
		else {
			qprintf("Allowed values are 'on' and 'off'\n");
		}
	}

	qprintf("Semaphore history default is '%s'\n",
			(semadefhist & SEMA_NOHIST) ? "off" : "on");
	return;
}


static void
semctl_defkhist(int ac, char **av)
{
	if (ac > 2) {
		qprintf("Argument: on|off\n");
		return;
	}
	
	if (ac == 2) {
		if (!strcmp("on", av[1]) || !strcmp("ON", av[1]))
			semadefkhist = 1;
		else if (!strcmp("off", av[1]) || !strcmp("OFF", av[1]))
			semadefkhist = 0;
		else {
			qprintf("Allowed values are 'on' and 'off'\n");
		}
	}

	qprintf("Semaphore khist default is '%s'\n",
			(semadefkhist) ? "on" : "off");
	return;
}

static void
semctl_defkhlen(int ac, char **av)
{
	if (ac > 2) {
		qprintf("Argument: <value>\n");
		return;
	}
	
	if (ac == 2) {
		unsigned short len;
		len = (unsigned short)strtoull(av[1],
						(char **)NULL, 0);
		if (len > 256) {
			qprintf("Maximum value set here is 256\n");
			return;
		}
		semadefkhistlen = len;
	}

	qprintf("Semaphore khist length is %d\n", semadefkhistlen);
	return;
}

static void
semctl_defmeter(int ac, char **av)
{
	if (ac > 2) {
		qprintf("Argument: on|off\n");
		return;
	}

	if (ac == 2) {	
		if (!strcmp("on", av[1]) || !strcmp("ON", av[1]))
			defaultsemameter = 1;
		else if (!strcmp("off", av[1]) || !strcmp("OFF", av[1]))
			defaultsemameter = 0;
		else {
			qprintf("Allowed values are 'on' and 'off'\n");
		}
	}

	qprintf("Semaphore metering default is '%s'\n",
			defaultsemameter ? "on" : "off");
	return;
}

/* ARGSUSED */
static void
semctl_help(int ac, char **av)
{
	struct idbgcommfunc *comm;
	
	for (comm = idbgSemCtlFunc; comm->comm_name; comm++) {
		qprintf("%s %s\n",
			_padstr(comm->comm_name, 16),
			comm->comm_descrip);
	}
}

static void
semctl_histmax(int ac, char **av)
{
	int sz;
	int error;

	if (ac != 2) {
		qprintf("Missing value\n");
		return;
	}
	
	sz = (int)strtoull(av[1], (char **)NULL, 0);
	if (!(error = rslog_size(sz))) {
		qprintf("Semaphore history size now %d\n", sz);
	} else {
		qprintf("Error %d attempting to resize to %d\n", error, sz);
	}
		
	return;
}

static void
semctl_sema(int ac, char **av)
{
	if (ac != 2) {
		qprintf("Missing expr\n");
		return;
	}
	(void)semctl_apply(av[1], idbg_semdump, ac, av);
	return;
}

static void
semctl_semlog(int ac, char **av)
{
	extern int dispseq;
	mpkqueuehead_t *bp;
	int bi;
	kqueue_t *lp;
	sema_t *sp;
	semahist_t *hp;
	char buf[1024];
	int i;
	char *re = "*";
	int n = histmax;
	int ix;
	int always = 0;
	sema_t *cachesp = 0;

	switch (ac) {
	default:
	case 3:
		n = (int)strtoull(av[2], (char **)NULL, 0);
		/* fall thru */
	case 2:
		re = av[1];
		break;
	case 1:
		always = 1;
		break;
	}

	if (isdigit(*re)) {
		sp = (sema_t *)(__psint_t)strtoull(re, (char **)NULL, 0);
	} else if (!always) {
		sp = 0;
		(void)glob2regex(buf, re);
		if (re_comp(buf)) {
			qprintf("Error in reg expr '%s'\n", re);
			return;
		}
	}
	
	qprintf("semaphore history log dump\n");
	qprintf("current sequence number is %d\n", dispseq);
	shdump_lastts = 0;

	for (ix = 0, i = histpos-1; n > 0 && i >= 0; i--, ix++) {
		hp = &semahist[i];

		if (always) {
			qprintf("[%d] ", ix);
			idbgplog(hp);
			n--;
			continue;
		}

		if (sp) {
			if (hp->h_sid == sp) {
				qprintf("[%d] ", ix);
				idbgplog(hp);
				n--;
			}
			continue;
		}
		
		/* Yuck!  This (value of cachesp) should be protected. */
		if (hp->h_sid == cachesp) {
			qprintf("[%d] ", ix);
			idbgplog(hp);
			n--;
			continue;
		}

		/* Yuck! Don't know if sema has been freed. */
		for (bi = 0, bp = semainfolist; bi < semabuckets; bi++, bp++) {
		    for (lp = kqueue_first(&bp->head);
			 lp != kqueue_end(&bp->head);
			 lp = kqueue_next(lp)) {
			wchaninfo_t *winfo = baseof(wchaninfo_t, list, lp);
			if (winfo->name[0]
			    && (sema_t *)winfo->wchan == hp->h_sid
			    && re_exec(winfo->name)) {
				qprintf("[%d] ", ix);
				cachesp = (sema_t *)winfo->wchan;
				idbgplog(hp);
				n--;
				break;
			}
		    }
		}
	}

	for (i = histmax-1; n > 0 && i >= histpos; i--, ix++) {
		hp = &semahist[i];

		if (always) {
			qprintf("[%d] ", ix);
			idbgplog(hp);
			n--;
			continue;
		}

		if (sp) {
			if (hp->h_sid == sp) {
				qprintf("[%d] ", ix);
				idbgplog(hp);
				n--;
			}
			continue;
		}
		
		/* Yuck!  This (value of cachesp) should be protected. */
		if (hp->h_sid == cachesp) {
			qprintf("[%d] ", ix);
			idbgplog(hp);
			n--;
			continue;
		}

		/* Yuck! Don't know if sema has been freed. */
		for (bi = 0, bp = semainfolist; bi < semabuckets; bi++, bp++) {
		    for (lp = kqueue_first(&bp->head);
			 lp != kqueue_end(&bp->head);
			 lp = kqueue_next(lp)) {
			wchaninfo_t *winfo = baseof(wchaninfo_t, list, lp);
			if (winfo->name[0]
			    && (sema_t *)winfo->wchan == hp->h_sid
			    && re_exec(winfo->name)) {
				qprintf("[%d] ", ix);
				cachesp = (sema_t *)winfo->wchan;
				idbgplog(hp);
				n--;
				break;
			}
		    }
		}
	}
}

static void
semctl_what(int ac, char **av)
{
	if (ac != 2) {
		qprintf("Missing expr\n");
		return;
	}
	(void)semctl_apply(av[1], semctl_describe, ac, av);
	return;
}

static void
printid(kthread_t *kt, uint64_t id, char *str)
{
	int rv;
	__psunsigned_t res;

	qprintf("%s [%llu (", str, id);
	if ((rv = find_kthreadid(id, 0, &res)) == KID_IS_UTHREAD) {
		qprintf("uthread @ 0x%lx[%d]",
			res, UT_TO_PROC(((uthread_t *)res))->p_pid);
	} else if (rv == KID_IS_STHREAD)
		qprintf("sthread @ 0x%lx", res);
	else if (rv == KID_IS_ITHREAD)
		qprintf("ithread @ 0x%lx", res);
	else
		qprintf("eh? @ 0x%lx", kt);
	qprintf(")]");
}

/*
 * Lock	and semaphore dumps.
++
++	kp sema s - dump semaphore
++
 */
char *tab_semflg[] = {
	"nohist",
	"queue",
	"locked",
	"metered",
	"mutex",
	0
};

#define H(x)	((k_histo_t *)(x))
int dumphisto = 1;

void
idbg_semdump(sema_t *sp)
{
	k_semameter_t *kp = smeter(sp);
	register kthread_t *kt;
	char *name = wchanname(sp, KT_WSEMA);

	qprintf("semaphore @ 0x%x [%s] count %d flags:", sp, name,
		sp->s_un.s_st.count);
	if (sp->s_un.s_st.flags & SEMA_LOCK)
		qprintf(" locked");
	if (sp->s_un.s_st.flags & SEMA_NOHIST)
		qprintf(" nohist");
	if (kp)
		qprintf(" metered");
	qprintf("\n  wq:");

	for (kt = sp->s_queue; kt; kt = kt->k_flink) {
		qprintf(" 0x%x", kt);
		if (kt->k_flink == sp->s_queue)
			break;
	}

	if (kp)
		qprintf("; meter @ 0x%x\n", kp);
	else
		qprintf("; no meter info\n");

	if (kp) {
		qprintf(
"  psema %d phits %d vsema %d vnoslp %d wsema %d wnoslp %d unsema %d\n",
			kp->s_psema, kp->s_phits, kp->s_vsema, kp->s_vnoslp,
			kp->s_wsema, kp->s_wnoslp, kp->s_unsema);
		qprintf(
"  cpsema %d cphits %d cvsema %d cvnoslp %d nwait %d maxnwait %d\n",
			kp->s_cpsema, kp->s_cphits, kp->s_cvsema, kp->s_cvnoslp,
			kp->s_nwait, kp->s_maxnwait);
		printid(kp->s_thread, kp->s_id, "  last owner");
		qprintf(" lastpc ");
		_prsymoff((void *)kp->s_lastpc, NULL, NULL); 
		qprintf("\n");
		qprintf(
"  next 0x%x prev 0x%x blink 0x%x histo 0x%x\n",
			kp->s_base.list.kq_next, kp->s_base.list.kq_prev,
			kp->s_blink, kp->s_histo);
		if (dumphisto && kp->s_histo) {
			idbg_dumphisto(&H(kp->s_histo)->h_hold, "hold", 1);
			idbg_dumphisto(&H(kp->s_histo)->h_waiting,
								"waiting", 1);
			idbg_dumphisto(&H(kp->s_histo)->h_count, "count", 0);
		}
	}
}


void
idbg_mutexdump(mutex_t *mp)
{
	mmeter_t *kp = mutexmeter(mp);
	kthread_t *kt = mutex_owner(mp);
	char *name = wchanname(mp, KT_WMUTEX);

	qprintf("mutex @ 0x%x [%s] owner 0x%x (%lld)\n ", mp, name,
		kt, kt ? kt->k_id : 0);

	if (mp->m_bits & MUTEX_LOCKBIT)
		qprintf(" %s", "locked");

	qprintf(" wq:");
	for (kt = mp->m_queue; kt; kt = kt->k_flink) {
		qprintf(" 0x%x", kt);
		if (kt->k_flink == mp->m_queue)
			break;
	}

	if (kp)
		qprintf("; meter @ 0x%x\n", kp);
	else
		qprintf("; (no meter info)\n");

	if (kp) {
		qprintf(
"  locks %d lockhits %d trylock %d trylockhit %d\n",
			kp->mm_lock, kp->mm_lockhit,
			kp->mm_trylock, kp->mm_tryhit);
		qprintf(
"  unlocks %d unlock-noslp %d nwait %d maxnwait %d\n",
			kp->mm_unlock, kp->mm_unoslp,
			kp->mm_nwait, kp->mm_maxnwait);
		printid(kp->mm_thread, kp->mm_id, "  last owner");
		qprintf(" lastpc ");
		_prsymoff((void *)kp->mm_lastpc,NULL,NULL); 
		qprintf("\n");
		qprintf("Hold times: Max (%d us) Total (%d us)\n",kp->mm_holdtime,kp->mm_totalholdtime);
		qprintf("Wait times: Max (%d us) Total (%d us)\n",kp->mm_waittime,kp->mm_totalwaittime);
	}
}

static char *histoPolicy[] = {
	"none",
	"add",
	"mul",
	0
};

static void
idbg_dumphisto(k_histelem_t *khp, char *str, int timed)
{
	if (str)
		qprintf("  %s:\n", str);
	if (timed)
		qprintf("    policy '%s', length %d, base %d usec, offset %d usec\n",
			histoPolicy[khp->he_policy],
			khp->he_length, (khp->he_base*timer_unit)/1000,
			(khp->he_offset*timer_unit)/1000);
	else
		qprintf("    policy '%s', length %d, base %d, offset %d\n",
			histoPolicy[khp->he_policy],
			khp->he_length, khp->he_base, khp->he_offset);
	if (timed) {
		/* do integer division */
		int left, right;
		left = (int)(((khp->he_total*timer_unit)/1000)/1000000);
		right = (int)(((khp->he_total*timer_unit)/1000)%1000000);
		qprintf("    total %d.%s sec, updates %d",
			left, padnum(right, 10, 6),
			(int)khp->he_updates);
	} else
		qprintf("    total %d, updates %d",
			(int)khp->he_total, (int)khp->he_updates);
	if (khp->he_updates > 0) {
		if (timed) 
			qprintf(", avg %d usec\n",
				(int)((khp->he_total*timer_unit) /
				(1000*khp->he_updates)));
		else {
			/* do integer division */
			int left, right;
			left = ((khp->he_total*1000)/khp->he_updates)/1000;
			right = ((khp->he_total*1000)/khp->he_updates)%1000;
			qprintf(", avg %d.%s\n",
				left, padnum(right, 10, 3));
		}
	} else {
		qprintf("\n");
	}
	if (timed)
		qprintf("    pc 0x%x, value %d usec, data @ 0x%x\n",
			khp->he_pc, (khp->he_value*timer_unit)/1000,
			khp->he_data);
	else
		qprintf("    pc 0x%x, value %d, data @ 0x%x\n",
			khp->he_pc, khp->he_value, khp->he_data);
	return;
}


/*
 * Semaphore list.
++
++	kp semlist n - print list of semaphores which have metering
++			n - # entries to print
++
 */
extern struct syncinfo syncinfo;

void
idbg_semalist(__psint_t n)
{
	mpkqueuehead_t *bp;
	int bi;

	/*
	 * We break up the following printf because of maximum argument
	 * constraints (16) and because printing more than ARG registers
	 * 32-bit quantities on a 64-bit kernel generally yields garbage
	 * since the 64-bit kernel printf treats %d as referring to 64-bit
	 * quantities.
	 */
	qprintf("lock type  #current  #initialized\n"
		"---------------------------------\n"
		"mutex      %8d  %12d\n"
		"semaphore  %8d  %12d\n",
		syncinfo.mutex.current,     syncinfo.mutex.initialized,
		syncinfo.semaphore.current, syncinfo.semaphore.initialized);
	qprintf("mrlock     %8d  %12d\n"
		"sv         %8d  %12d\n",
		syncinfo.mrlock.current,    syncinfo.mrlock.initialized,
		syncinfo.sv.current,        syncinfo.sv.initialized);
	qprintf("spinlock   %8d  %12d\n"
		"bitlock    %8d  %12d\n",
		syncinfo.spinlock.current,  syncinfo.spinlock.initialized,
		syncinfo.bitlock.current,   syncinfo.bitlock.initialized);
	qprintf("64bitlock  %8d  %12d\n"
		"mrsplock   %8d  %12d\n"
		"\n",
		syncinfo.bitlock64.current, syncinfo.bitlock64.initialized,
		syncinfo.mrsplock.current,  syncinfo.mrsplock.initialized);

	for (bi = 0, bp = semainfolist; bi < semabuckets; bi++, bp++) {
		kqueue_t *lp;
		for (lp = kqueue_first(&bp->head);
		     lp != kqueue_end(&bp->head);
		     lp = kqueue_next(lp)) {
			wchaninfo_t *winfo = baseof(wchaninfo_t, list, lp);
			if (--n < 0)
				return;
			if (semametered) {
				qprintf("sem 0x%x [%s] meter @0x%x ",
					winfo->wchan, winfo->name, winfo);
				printid(((k_semameter_t *)winfo)->s_thread,
					((k_semameter_t *)winfo)->s_id,
					"thread");
				qprintf("\n");
			} else
				qprintf("sem 0x%x [%s]\n",
					winfo->wchan, winfo->name);
		}
	}
}


/*
 * Lock information dump
++
++	kp lock l - print information about a spinlock
++			l - lock index or lock address
++
 */
void
idbg_chklock(void *l)
{
	register struct k_splockmeter *spm;

	spm = lockmeter_find(l);
	if (!spm) {
		qprintf("no metering info for lock at 0x%x\n", l);
		return;
	}

	qprintf("lock @ 0x%x type %s meter info @ 0x%x\n",
		spm->m_addr,
		spm->m_type == SPINTYPE_SPIN ? "spinlock" :
		spm->m_type == SPINTYPE_BIT ? "bitlock" :
		spm->m_type == SPINTYPE_64BIT ? "64bitlock" : "???",
		spm);
	qprintf("\tname \"%s\" is %s\n",
		spm->m_name, islock(spm->m_addr));
	qprintf("\t[inuse %d owner %d wait %d lcnt %d ucnt %d]\n",
		spm->m_inuse, spm->m_owner, spm->m_wait,
		spm->m_lcnt, spm->m_ucnt);
	qprintf("\tlast lock: ");
	_prsymoff((void *)spm->m_lastlock,NULL,NULL);
	qprintf("\tlast unlock: ");
	_prsymoff((void *)spm->m_lastunlock,NULL,"\n");
	qprintf("\theld: ");
	_prsymoff((void *)spm->m_elapsedpc,NULL,NULL); 
	qprintf(" (%d us) ",
		(uint)(((LONG_LONG)spm->m_elapsed*timer_unit)/1000));
	qprintf("\twaited: ");
	_prsymoff((void *)spm->m_waitpc,NULL,NULL); 
	qprintf(" (%d us)\n",
		(int)(((LONG_LONG)spm->m_waittime*timer_unit)/1000));  
	qprintf(" Total held: (%lld us)\t\tTotal wait: (%lld us)\n",
		(uint64_t)(((LONG_LONG)spm->m_totalheld*timer_unit)/1000),
		(uint64_t)(((LONG_LONG)spm->m_totalwait*timer_unit)/1000));  
}

void
idbg_avlnode(avlnode_t *avl)
{
	qprintf("avl @ 0x%x: balance %s, next 0x%x\n",
		avl, avl->avl_balance == AVL_BALANCE ? "even" :
			(avl->avl_balance == AVL_BACK ? "back" : "forw"),
		avl->avl_nextino);
	qprintf("  parent 0x%x back 0x%x forw 0x%x\n",
		avl->avl_parent, avl->avl_back, avl->avl_forw);
}

void
idbg_lockmeter(k_splockmeter_t *spm)
{
	qprintf("lock @ 0x%x, type %s meter info @ 0x%x\n",
		spm->m_addr,
		spm->m_type == SPINTYPE_SPIN ? "spinlock" :
		spm->m_type == SPINTYPE_BIT ? "bitlock" :
		spm->m_type == SPINTYPE_64BIT ? "64bitlock" : "???",
		spm);
	qprintf("\tname \"%s\" is %s\n",
		spm->m_name, islock(spm->m_addr));
	idbg_avlnode(&spm->m_avlnode);
	qprintf("\t[inuse %d owner %d wait %d lcnt %d ucnt %d]\n",
		spm->m_inuse, spm->m_owner, spm->m_wait,
		spm->m_lcnt, spm->m_ucnt);
	qprintf("\tlast lock: ");
	_prsymoff((void *)spm->m_lastlock,NULL,NULL);
	qprintf("\tlast unlock: ");
	_prsymoff((void *)spm->m_lastunlock,NULL,"\n");
	qprintf("\theld: ");
	_prsymoff((void *)spm->m_elapsedpc,NULL,NULL); 
	qprintf(" (%d us) ",
		(uint)(((LONG_LONG)spm->m_elapsed*timer_unit)/1000));
	qprintf("\twaited: ");
	_prsymoff((void *)spm->m_waitpc,NULL,NULL); 
	qprintf(" (%d us)\n",
		(int)(((LONG_LONG)spm->m_waittime*timer_unit)/1000));  
	qprintf(" Total held: (%lld us)\t\tTotal wait: (%lld us)\n",
		(uint64_t)(((LONG_LONG)spm->m_totalheld*timer_unit)/1000),
		(uint64_t)(((LONG_LONG)spm->m_totalwait*timer_unit)/1000));  
	qprintf(" ts %u, no. current waiters: %d\n",
		spm->m_ts, spm->m_racers);
			
}

static char *
islock(void *lck)
{
	register struct k_splockmeter *spm;

	spm = lockmeter_find(lck);
	if (!spm)
		goto no_lock;

	if (spm->m_owner == OWNER_NONE) {
		if (spm->m_ucnt != spm->m_lcnt)
			return("Finconsistent");
		else
			return("free");
	} else {
		if (spm->m_ucnt != spm->m_lcnt - 1)
				return("Linconsistent");
			else
				return("locked");
	}

no_lock:
	return("???");
}

/*
++
++	kp hotlk [threshold]
++			where threshold is expressed in tenths of a percent.
++			The default threshold is 20 (i.e. 2 percent).
++		prints list of spin locks on which spsema sleeps more often
++		than the specified threshold.
*/
#define HOTPERCENT 20
void
idbg_hotlocks(int op, int n)
{
	dumphotlock(op,n);
}

int idbg_locks_default = 10;
int idbg_total_default = 1000000;

/*
 * op = 0	dump hot lock with wait percent > n/10
 * op = 1	dump hot lock with m_elapsed > n
 * op = 2	dump hot lock with m_waittime > n
 * op = 4       dump hot lock with m_totalwait > 1000000 (for now -- Humper)
 * op = 5       dump hot lock with m_totalheld > 1000000 (for now -- Humper)
 * op = 99	clear hot lock data
 */

void
dumphotlock(int op, int n)
{
	register int percent;
	register int hot = HOTPERCENT;	/* default to wait % */
	register int wait = 0;
	register int numhot = 0;
	register struct k_splockmeter *spm;
	char *nptr;
	__psunsigned_t offst;
	if (op == 0) {
		if (n > 0) hot = n;
	qprintf("Hot spinlocks (spins > %d.%d percent of the time):\n",
		hot/10, hot%10);
	} else {
		wait = n;
		if (wait == 0)
		  {
		    if (op == 4 || op == 5) 
		      wait = idbg_total_default;
		    else wait = idbg_locks_default;
		  }
		switch(op) {
		    case 1:
			qprintf("Hot spinlocks (spins > %d us):\n",wait);
			break;
		    case 2:
			qprintf("Hot spinlocks (wait > %d us):\n",wait);
			break;
		    case 4:
			qprintf("Hot locks (total wait > %d us):\n",wait);
			break;
		    case 5:
			qprintf("Hot locks (total held > %d us):\n",wait);
			break;
		    case 99:
			qprintf("Reset data\n");
			break;
		    default:
			qprintf("Unknown option.\n");
			return;
		}
	}
	
	for (spm = lockmeter_chain; spm; spm = next_lockmeter(spm)) {
		if (!spm->m_inuse)
			continue;
		if (spm->m_lcnt == 0)
			continue;
		if (op == 99) {
			/* reset the counts */
			spm->m_wait = 0;
			spm->m_lcnt = spm->m_ucnt = 0;
			spm->m_elapsed = 0;
			spm->m_waittime = 0;
			spm->m_totalheld = 0;
			spm->m_totalwait = 0;
			continue;
		}
		percent = ((LONG_LONG)spm->m_wait * 1000) / spm->m_lcnt;
		if (op == 0 && percent <= hot)
			continue;
		else if (op == 1 && 
			 (((LONG_LONG)spm->m_elapsed*timer_unit)/1000)<wait)
			continue;
		else if (op == 2 && 
			 (((LONG_LONG)spm->m_waittime*timer_unit)/1000)<wait)
			continue;
		else if (op == 4 && 
			 (((LONG_LONG)spm->m_totalwait*timer_unit)/1000)<wait)
			continue;
		else if (op == 5 && 
			 (((LONG_LONG)spm->m_totalheld*timer_unit)/1000)<wait)
			continue;
		if (++numhot > 200) {
			qprintf("Breaking loop ... \n");
			break;
		}
    qprintf("Lock meter @ 0x%x, lock_t 0x%x, name \"%s\", lcnt %d, wait %d\n",
			spm, spm->m_addr, spm->m_name,
			spm->m_lcnt, spm->m_wait);
		qprintf("\twait %d.%d%% (%d),\n\theld: ",
			percent/10, percent%10, percent);
		_prsymoff((void *)spm->m_elapsedpc,NULL,NULL); 
		qprintf(" (cpu=%d, spl func=", spm->m_elapsedcpu);

		if (nptr = lookup_kname((void*)spm->m_splevel, &offst))
		    qprintf("%s", nptr);
		else
		    qprintf("0x%x", spm->m_splevel);
		
		qprintf("),\n\treleased by ");
		_prsymoff((void *)spm->m_elapsedpcfree,NULL,NULL); 

		qprintf(" (%d us),\n\twaited: ",
			(int)(((LONG_LONG)spm->m_elapsed*timer_unit)/1000));
		_prsymoff((void *)spm->m_waitpc,NULL,NULL); 
		qprintf(" (cpu %d) (%d us)\n", spm->m_waitcpu,
			(int)(((LONG_LONG)spm->m_waittime*timer_unit)/1000));
		qprintf(" total wait time: (%lld us)\n", 
			(uint64_t)(((LONG_LONG)spm->m_totalwait*timer_unit)/1000));
		qprintf(" total held time: (%lld us)\n",
			(uint64_t)(((LONG_LONG)spm->m_totalheld*timer_unit)/1000));
	}
	
	/* Print out mrlocks that are considered to be "hot" 
	   Note that the interface to the mrlocks function was designed
	   so that this would be a simple function call -- code reuse 
	   is your *friend*.  */

#if SEMAMETER
	idbg_mrlocks(op);
#endif
}

/*
++
++	kp coldlk [threshold]
++		prints list of spin locks that have been locked less than
++		threshold times per hour on average.  The default threshold
++		is 5.  Will print a maximum of 200 entries.
++
*/
#define COLDCOUNT 5 /* accesses per hour of system uptime */

void
idbg_coldlocks(int n)
{
	register int hours;
	register int locksperhour;
	register int numcold = 0;
	register int cold = COLDCOUNT;
	register struct k_splockmeter *spm;

	if (n > 0)
		cold = n;

	hours = lbolt / HZ;		/* seconds since boot */
	hours = (hours + 3599) / 3600;	/* hours since boot (rounded up) */

	qprintf("Cold spinlocks (locked less than %d times per hour):\n",
		cold);

	for (spm = lockmeter_chain; spm; spm = next_lockmeter(spm)) {
		if (!spm->m_inuse)
			continue;
		if ((locksperhour = spm->m_lcnt / hours) > cold)
			continue;
		if (numcold++ > 200)
			return;
		qprintf(
"lock meter @ 0x%x, lock_t 0x%x, name \"%s\", lcnt %d, wait %d, lcks/hr %d\n",
			spm, spm->m_addr, spm->m_name,
			spm->m_lcnt, spm->m_wait, locksperhour);
	}
	qprintf("total number of cold locks is %d\n", numcold);
}


/*
 * multi-reader lock dump
++
++	kp mrlock <addr> - dump multi-reader lock
++
 */
void
idbg_mrlock(mrlock_t *mrp)
{
	k_mrmeter_t *kmp = mrmeter(mrp);
	kthread_t *kt;
	char *name = wchanname(mrp, KT_WMRLOCK);
	int i, n;

	qprintf("mrlock: 0x%x [%s], lbits 0x%llx qbits 0x%llx (TYPE:%s%s%s%s%s%s):",
		mrp, name, mrp->mr_lbits, mrp->mr_qbits,
		mrp->mr_lbits & MR_BARRIER ? " barrier" : "",
		mrp->mr_qbits & MR_DBLTRIPPABLE ? " dbltrip" : "",
		mrp->mr_qbits & MR_BEHAVIOR ? " behavior" : "",
		mrp->mr_qbits & MR_ALLOW_EQUAL_PRI ? " alloweqpri" : "",
		mrp->mr_qbits & MR_V ? " MR_V" : "",
		mrp->mr_qbits & MR_QLOCK ? " QLOCK" : "");
	if (mrp->mr_lbits & MR_ACC)
		qprintf(" %d acc", mrp->mr_lbits & MR_ACC);
	if (mrp->mr_lbits & MR_UPD)
		qprintf(" update");
	if (mrp->mr_lbits & MR_WAIT)
		qprintf(", %d waiters",
			(mrp->mr_lbits & MR_WAIT) >> MR_WAITSHFT);
	qprintf("\n");

	if (kmp) {
		qprintf(" meter @ 0x%x:", kmp);
		qprintf(" l/u %d/%d acc; %d/%d upd\n",
			kmp->mr_acclocks, kmp->mr_accunlocks,
			kmp->mr_updlocks, kmp->mr_updunlocks);

		if (mrp->mr_lbits & (MR_ACC|MR_UPD)) {
			mr_log_t *logp;
			mr_entry_t *mre;
			int i;

			qprintf(" current users:\n");
			for (logp = &kmp->mr_log; logp; logp = logp->mrl_next) {
				for (i = 0, mre = &logp->mrl_entries[0];
				     i < MRE_ENTRIES; i++, mre++) {
					if (!(mre->mre_use))
						continue;
					qprintf("  thread %lld %s%s%sfrom pc: ",
						mre->mre_kid,
						mre->mre_use & MRE_ACCESS ?
						"acc " : "",
						mre->mre_use & MRE_UPDATE ?
						"upd " : "",
						mre->mre_use & MRE_PENDING ?
						"wanted " : "");
					_prsymoff((void *)mre->mre_pc,
							NULL, NULL); 
					qprintf("\n");
				}
			}
		}

		if (mrp->mr_pcnt || mrlock_peek_hpri_waiter(mrp))
			qprintf(" sleep queue [%sPcnt %d] (nwait %d, maxwait %d):",
				mrp->mr_qbits & MR_V ? "V " : "",
				mrp->mr_pcnt, kmp->mr_nwait, kmp->mr_maxnwait);
	} else {
		if (mrp->mr_pcnt || mrlock_peek_hpri_waiter(mrp))
			qprintf(" sleep queue [%sPcnt %d]:",
				mrp->mr_qbits & MR_V ? "V " : "", mrp->mr_pcnt);
	}

#ifdef DEBUG
	qprintf("Waiters (hpri = %d)\n", mrp->mr_holders.pq_pri);
#else
	qprintf("Waiters:\n");
#endif
	for (i = 0; i < NSCHEDCLASS; i++) {
		qprintf("CLASS[%d]: ", -i);
		for (n=0, kt = mrp->mr_waiters.pq_pq[i]; kt; kt = kt->k_flink) {
			if (n++) qprintf("           ");
			qprintf("kthread 0x%x %s,  mpri %d, bpri %d, kpri %d %s\n",
				kt, (kt->k_flags&KT_WUPD) ? "update" : "access",
				kt->k_mrqpri, kt->k_basepri, kt->k_pri, idbg_kthreadname(kt));
			if (kt->k_flink == mrp->mr_waiters.pq_pq[i])
				break;
		}
		if (!n) qprintf("\n");
	}

#ifdef DEBUG
	qprintf("Holders (lpri = %d nholders = %d)\n",
		mrp->mr_holders.pq_pri, mrp->mr_nholders);
#else
	qprintf("Holders:\n");
#endif
	for (i = 0; i < NSCHEDCLASS; i++) {
		mri_t *mrip;

		qprintf("CLASS[%d]: ", -i);
		for (n=0, mrip = mrp->mr_holders.pq_pq[i]; mrip; mrip = mrip->mri_flink) {
			kthread_t *kt = *(kthread_t **) MRI_TO_MRIA_KT_P(mrip);
			if (n++) qprintf("           ");
			qprintf("kthread 0x%x mpri %d, bpri %d, kpri %d %s\n",
				kt, mrip->mri_pri, kt->k_basepri, kt->k_pri, idbg_kthreadname(kt));
			if (mrip->mri_flink == mrp->mr_holders.pq_pq[i])
				break;
		}
		if (!n) qprintf("\n");
	}
}

char *tab_msmode[] = {
	"update",	/* 0x00000001 */
	"access",	/* 0x00000002 */
	"waiters",	/* 0x00000004 */
};

/*
 * multi-reader/multi-writer toggled lock dump
++
++	kp mslock <addr> - dump multi-reader/multi-writer lock
++
 */
void
idbg_mslock(mslock_t *msp)
{
	qprintf("mrlock: @ 0x%x count %d ", msp, msp->ms_cnt);
	if (msp->ms_mode) {
		_printflags((unsigned)msp->ms_mode, tab_msmode, "mode");
		qprintf("\n");
	} else {
		qprintf("free\n");
	}
	if (msp->ms_mode & MS_WAITERS) {
		idbg_svdump(&msp->ms_sv);
	}
}

void
idbg_mrlockfull(mrlock_t *mrp)
{
	k_mrmeter_t *kmp = mrmeter(mrp);
	char *name = wchanname(mrp, KT_WMRLOCK);
	kthread_t *kt;
	int i;

	qprintf("mrlock: 0x%x [%s], lbits 0x%llx qbits 0x%llx (TYPE:%s%s%s%s%s%s):",
		mrp, name, mrp->mr_lbits, mrp->mr_qbits,
		mrp->mr_lbits & MR_BARRIER ? " barrier" : "",
		mrp->mr_qbits & MR_DBLTRIPPABLE ? " dbltrip" : "",
		mrp->mr_qbits & MR_BEHAVIOR ? " behavior" : "",
		mrp->mr_qbits & MR_ALLOW_EQUAL_PRI ? " alloweqpri" : "",
		mrp->mr_qbits & MR_V ? " MR_V" : "",
		mrp->mr_qbits & MR_QLOCK ? " QLOCK" : "");
	if (mrp->mr_lbits & MR_ACC)
		qprintf(" %d acc", mrp->mr_lbits & MR_ACC);
	if (mrp->mr_lbits & MR_UPD)
		qprintf(" update");
	if (mrp->mr_lbits & MR_WAIT)
		qprintf(", %d waiters",
			(mrp->mr_lbits & MR_WAIT) >> MR_WAITSHFT);
	qprintf("\n");

	if (kmp) {
		qprintf(" meter @ 0x%x:", kmp);
		qprintf(" l/u %d/%d acc; %d/%d upd\n",
			kmp->mr_acclocks, kmp->mr_accunlocks,
			kmp->mr_updlocks, kmp->mr_updunlocks);

		/* Print timing information */
		qprintf ("\n******************* Writer Wait Timing ***************");
		qprintf("\n\tTotal time writers waited for lock: (%d us)",
			(int)(((LONG_LONG)kmp->mr_totalwwaittime*timer_unit)/1000));
		qprintf("\n\tMax time a writer waited for lock: (%d us)",
			(int)(((LONG_LONG)kmp->mr_wwaittime*timer_unit)/1000));
		qprintf("\n\tWriter who waited (cpu %d): ",
			kmp->mr_wwaitcpu);
		_prsymoff((void *)kmp->mr_wwaitpc,NULL,NULL);
		qprintf ("\n******************* Reader Wait Timing ***************");
		qprintf("\n\tTotal time readers waited for lock: (%d us)",
			(int)(((LONG_LONG)kmp->mr_totalrwaittime*timer_unit)/1000));
		qprintf("\n\tMax time a reader waited for lock: (%d us)",
			(int)(((LONG_LONG)kmp->mr_rwaittime*timer_unit)/1000));
		qprintf("\n\tReader who waited (cpu %d): ",
			kmp->mr_rwaitcpu);
		_prsymoff((void *)kmp->mr_rwaitpc,NULL,NULL);
		qprintf ("\n******************* Writer Hold Timing ***************");
		qprintf("\n\tTotal time writers held lock: (%d us)",
			(int)(((LONG_LONG)kmp->mr_totalwholdtime*timer_unit)/1000));
		qprintf("\n\tMax time a writer held the lock: (%d us)",
			(int)(((LONG_LONG)kmp->mr_wholdtime*timer_unit)/1000));
		qprintf("\n\tWriter who released (cpu %d): ",
			kmp->mr_wholdcpu);
		_prsymoff((void *)kmp->mr_wholdpc,NULL,NULL);
		qprintf ("\n******************* Reader Hold Timing ***************");
		qprintf("\n\tTotal time readers held lock: (%d us)",
			(int)(((LONG_LONG)kmp->mr_totalrholdtime*timer_unit)/1000));
		qprintf("\n\tMax time a reader held the lock: (%d us)",
			(int)(((LONG_LONG)kmp->mr_rholdtime*timer_unit)/1000));
		qprintf("\n\tReader who released (cpu %d):",
			kmp->mr_rholdcpu);
		_prsymoff((void *)kmp->mr_rholdpc,NULL,NULL);
		qprintf ("\n");
		qprintf(" p (slp/noslp): acc %d/%d upd %d/%d\n",
			kmp->mr_accpslp, kmp->mr_accphits,
			kmp->mr_updpslp, kmp->mr_updphits);
		qprintf(" v: wake acc %d(+%d), wake upd %d, no waiter %d\n",
			kmp->mr_accv, kmp->mr_dqacc,
			kmp->mr_updv, kmp->mr_vnoslp);

		if (mrp->mr_lbits & (MR_ACC|MR_UPD)) {
			mr_log_t *logp;
			mr_entry_t *mre;
			int i;

			qprintf(" current users:\n");
			for (logp = &kmp->mr_log; logp; logp = logp->mrl_next) {
				for (i = 0, mre = &logp->mrl_entries[0];
				     i < MRE_ENTRIES; i++, mre++) {
					if (!(mre->mre_use))
						continue;
					qprintf("  thread %lld %s%s%sfrom pc: ",
						mre->mre_kid,
						mre->mre_use & MRE_ACCESS ?
						"acc " : "",
						mre->mre_use & MRE_UPDATE ?
						"upd " : "",
						mre->mre_use & MRE_PENDING ?
						"wanted " : "");
					_prsymoff((void *)mre->mre_pc,
							NULL, NULL); 
					qprintf("\n");
				}
			}
		}

		qprintf(" sleep queue [%sPcnt %d] (nwait %d, maxwait %d):",
			mrp->mr_qbits & MR_V ? "V " : "",
			mrp->mr_pcnt, kmp->mr_nwait, kmp->mr_maxnwait);
	} else
		qprintf(" sleep queue [%sPcnt %d]:",
			mrp->mr_qbits & MR_V ? "V " : "", mrp->mr_pcnt);

#ifdef DEBUG
	qprintf("Waiters (hpri = %d)\n", mrp->mr_holders.pq_pri);
#else
	qprintf("Waiters:\n");
#endif
	for (i = 0; i < NSCHEDCLASS; i++) {
		qprintf("CLASS[%d]: ", -i);
		for (kt = mrp->mr_waiters.pq_pq[i]; kt; kt = kt->k_flink) {
			qprintf("kthread 0x%x mpri %d bpri %d kpri %d ",
				kt, kt->k_mrqpri, kt->k_basepri, kt->k_pri);
			if (kt->k_flink == mrp->mr_waiters.pq_pq[i])
				break;
		}
		qprintf("\n");
	}
	qprintf("\n");

}

/*
 * Private data area dump.
 */

char *tbl_pdaflg[] = {
	"master",
	"clock",
	"enabled",
	"fastclk",
	"isolated",
	"no-broadcast",
	"non-preemptive",
	"nointr",
	"UNK",
	"UNK",
	"UNK",
	"UNK",
	"UNK",
	"UNK",
	"UNK",
};

char *tbl_pdakstack[] = {
	"USR",
	"KER",
	"INT",
	"IDL"
};

/*
++
++	kp pda
++		dumps out pda struct for each active processor
++
*/
void
idbg_pda(__psint_t x)
{
	qprintf("Private data area:\n");
	do_pdas(dumppda, x);
}

void
do_pdas(void (*func)(pda_t *), int which)
{
	register int i;
	if (which >= 0 && which < maxcpus) {
		if (pdaindr[which].CpuId != -1) {
			(*func)(pdaindr[which].pda);
		} else {
			qprintf("PDA not valid for cpu %d\n", which);
		}
	} else {
		for (i = 0; i < maxcpus; i++)
			if (pdaindr[i].CpuId != -1)
				(*func)(pdaindr[i].pda);
	}
}

/*
++
++	kp pda - dump out pda (private data area) for all active processors
++
 */
void
dumppda(pda_t *pd)
{
	int flags;
	int mask;
	int ti;
#if defined(R4000) || defined(LARGE_CPU_COUNT)
	int i;
#endif

	qprintf("pda 0x%x (0x%x) id %d ", pd, pd->p_flags, pd->p_cpuid);
#if NUMA_BASE
	qprintf("node %d nasid %d ", pd->p_nodeid, pd->p_nasid);
#endif	/* NUMA_BASE */
	if (pd->p_cpunamestr)
		qprintf("\n name: %s, ", pd->p_cpunamestr);
	flags = pd->p_flags;
	mask = 1;
	ti = 0;
	while (flags != 0) {
		if (mask & flags) {
			qprintf("%s ", tbl_pdaflg[ti]);
			flags &= ~mask;
		}
		mask <<= 1;
		ti++;
	}
	qprintf("\n");
	qprintf(
	"  runrun %d kstkflg %s switching %d lticks %d p_curpri %d\n",
		pd->p_runrun,
		(pd->p_kstackflag <= PDA_CURIDLSTK) ?
			tbl_pdakstack[pd->p_kstackflag] : "UNKNOWN",
		pd->p_switching, pd->p_lticks, pd->p_curpri);
	qprintf( "  curuthread 0x%x fpowner 0x%x\n",
		pd->p_curuthread, pd->p_fpowner);
	qprintf("  curkthread 0x%x id %lld  next 0x%x cpuset %d\n",
		pd->p_curkthread,
		pd->p_curkthread ? pd->p_curkthread->k_id : -1,
		pd->p_nextthread,
		pd->p_idler);

	qprintf(
	"  vmeipl 0x%x curlck 0x%x lastlck 0x%x curcpc 0x%x\n",
		pd->p_vmeipl, pd->p_curlock, pd->p_lastlock,
		pd->p_curlockcpc);
	qprintf("  istk 0x%x eistk 0x%x bootstk 0x%x ebootstk 0x%x\n",
		pd->p_intstack, pd->p_intlastframe,
		pd->p_bootstack, pd->p_bootlastframe);
	qprintf("  idlstkdepth 0x%x nofault %d lock @ 0x%x,\n",
		pd->p_idlstkdepth, pd->p_nofault, &pd->p_special);
	qprintf("  p_kvfault 0x%x, ksaptr 0x%x\n", pd->p_kvfault, pd->ksaptr);

#ifdef R4000
	if (pd->p_flags & PDAF_ISOLATED) {
		for (i=0; i < CACHECOLORSIZE; i++) {
			qprintf("  clrkvflt[0x%x] 0x%x", i, pd->p_clrkvflt[i]);
			if ((i+1) % 3 == 0) qprintf("\n");
		}
		if (i % 3) qprintf("\n");
	}
#endif
	if (pd->p_va_panicspin) qprintf(" VA_PANICSPIN ");
	if (pd->p_va_force_resched) qprintf(" VA_RESCHED ");
	if (pd->p_tlbflush_cnt)
		qprintf(" tlbflush_cnt %d ", pd->p_tlbflush_cnt);
#if LARGE_CPU_COUNT
	qprintf(
	"  schedflags 0x%x action 0x%x firstaction 0x%x cpumask ",
		pd->p_schedflags, pd->p_actionlist.todolist, pd->p_actionlist.firstaction);
	for (i=0; i<CPUMASK_SIZE; i++)
		qprintf("%s%x%s", i?" ":"[", pd->p_cpumask._bits[i], (i==CPUMASK_SIZE-1)?"]\n":"");
#else /* !LARGE_CPU_COUNT */
#ifdef MP
	qprintf("  action 0x%x", pd->p_actionlist.todolist);
#endif
	qprintf(
	"  schedflags 0x%x cpumask 0x%x\n",
		pd->p_schedflags, pd->p_cpumask);
#endif /* !LARGE_CPU_COUNT */

	if (pd->ksaptr) {
		qprintf(
	"  icdev %d icdevsw %d ibdev %d ibdevsw %d istr %d istrsw %d\n",
		pd->ksaptr->dlock.p_indcdev, pd->ksaptr->dlock.p_indcdevsw,
		pd->ksaptr->dlock.p_indbdev, pd->ksaptr->dlock.p_indbdevsw,
		pd->ksaptr->dlock.p_indstr, pd->ksaptr->dlock.p_indstrsw);

	qprintf("  stats: user %d idle %d kernel %d wait %d sxbrk %d intr %d\n",
		pd->ksaptr->si.cpu[CPU_USER], pd->ksaptr->si.cpu[CPU_IDLE],
		pd->ksaptr->si.cpu[CPU_KERNEL], pd->ksaptr->si.cpu[CPU_WAIT],
		pd->ksaptr->si.cpu[CPU_SXBRK], pd->ksaptr->si.cpu[CPU_INTR]);
	}
	qprintf("  timein %x fclock_freq 0x%x decinsperloop %d\n",
		pd->p_timein, pd->fclock_freq,
		pd->decinsperloop);
	qprintf("  delayacvec 0x%x ukstklo 0x%x\n",
		pd->p_delayacvec, pd->p_ukstklo);
#if DEBUG
	qprintf("  switchuthread 0x%x\n",
			pd->p_switchuthread);
#endif /* DEBUG */
#if EXTKSTKSIZE == 1
	qprintf("  stackext: flag 0x%x pde @0x%x (0x%x) bakup_pde 0x%x\n",
		pd->p_mapstackext, &pd->p_stackext, pd->p_stackext.pgi,
		pd->p_bp_stackext.pgi);
#endif
	qprintf(
	"  atsave 0x%llx t0save 0x%llx k1save 0x%llx\n",
		pd->p_atsave, pd->p_t0save, pd->p_k1save);
#if R4000
	if (pd->p_vcelog)
		qprintf("  vcelog 0x%x vceoffset 0x%x vcecount 0x%x\n",
		pd->p_vcelog, pd->p_vcelog_offset, pd->p_vcecount);
#endif
	printtimers(pd->p_curkthread);
	qprintf("  kstr_lfvecp 0x%x, kstr_statsp 0x%x, knaptr 0x%x\n",
		pd->kstr_lfvecp, pd->kstr_statsp, pd->knaptr);

	qprintf("  nfsstatp 0x%x, cfsstatp 0x%x\n", pd->nfsstat, pd->cfsstat);
#if TLBDEBUG
#if !R4000 && !R10000
	qprintf("  sv1lo 0x%x sv2lo 0x%x sv1hi 0x%x sv2hi 0x%x\n",
		pd->p_sv1lo.pgi, pd->p_sv2lo.pgi,
		pd->p_sv1hi, pd->p_sv2hi);
#else
	qprintf("  sv1lo 0x%x sv1lo_1 0x%x sv1hi 0x%x\n",
		pd->p_sv1lo.pgi, pd->p_sv1lo_1.pgi, pd->p_sv1hi);

	qprintf(
	"  sv2lo 0x%x sv2lo_1 0x%x sv2hi 0x%x\n",
		pd->p_sv2lo.pgi, pd->p_sv2lo_1.pgi, pd->p_sv2hi);
#endif
#endif /* TLBDEBUG */
	qprintf(
	"  utlbmisses %d ktlbmisses %d utlbindx %d utlbhndlr 0x%x\n",
		pd->p_utlbmisses, pd->p_ktlbmisses, pd->p_utlbmissswtch,
		pd->p_utlbmisshndlr);
#ifndef SN0
	qprintf(
	"  p_pcopy_lock 0x%x\n", &pd->p_pcopy_lock);
	qprintf(
	"  p_pcopy_pagelist 0x%x 0x%x, p_pcopy_inuse 0x%x 0x%x\n",
		pd->p_pcopy_pagelist[0], pd->p_pcopy_pagelist[1],
		pd->p_pcopy_inuse[0], pd->p_pcopy_inuse[1]);
	qprintf(
	"  p_cacheop_pages 0x%x\n", pd->p_cacheop_pages );
#endif /* !SN0 */
	qprintf("  tstamp:  objp 0x%x mask 0x%x entries 0x%x\n",
		pd->p_tstamp_objp, pd->p_tstamp_mask, pd->p_tstamp_entries);
	qprintf("           ptr 0x%x last 0x%x eobmode %d\n",
		pd->p_tstamp_ptr, pd->p_tstamp_last, pd->p_tstamp_eobmode);
	qprintf("  p_scachesize 0x%x  p_cputype_word 0x%x\n", 
			pd->p_scachesize,pd->p_cputype_word);
	qprintf("  cpufreq %d cycles %d ust_cpufreq %d\n",
		pd->cpufreq, pd->cpufreq_cycles, pd->ust_cpufreq);
#if MP
	qprintf("  last sched %lld\n", pd->last_sched_intr_RTC);
	qprintf("  sched clock latency (microsecs)  max: %d  mean: %d \n",
		pd->counter_intr_over_max/findcpufreq(), 
		(uint)(pd->counter_intr_over_sum/pd->counter_intr_over_count)
			/ findcpufreq());
#endif
#if IP30
	qprintf("  next intr %lld\n", pd->p_next_intr);
	qprintf("  clock_tick %lld (%d,slow=%d) fclock_tick %lld (%d,slow=%d)\n",
		pd->p_clock_tick, pd->p_clock_ticked, pd->p_clock_slow,
		pd->p_fclock_tick,pd->p_fclock_ticked, pd->p_fclock_slow);
        qprintf("<FRS> p_frs_objp: 0x%x, p_frs_flags: 0x%x\n",
                pd->p_frs_objp, pd->p_frs_flags);
#endif  /* IP30 */
#if EVEREST
	qprintf("  sched clock latency (microsecs)  max: %d  mean: %d \n",
		pd->counter_intr_over_max/findcpufreq(), 
		(uint)(pd->counter_intr_over_sum/pd->counter_intr_over_count)
			/ findcpufreq());
	qprintf("  CEL_shadow 0x%x  CEL_hw 0x%x\n",
		pd->p_CEL_shadow, pd->p_CEL_hw);
        qprintf("<FRS> p_frs_objp: 0x%x, p_frs_flags: 0x%x\n",
                pd->p_frs_objp, pd->p_frs_flags);
	/*
        if (pd->p_frs_objp) {
                extern frs_cpu_frsobjp_print(void*);
                frs_cpu_frsobjp_print(pd->p_frs_objp);
        }
	*/
#endif
	qprintf("  cpu_mon 0x%x active_cpu_mon 0x%x\n",
		pd->p_cpu_mon, pd->p_active_cpu_mon);
#if SN0
	qprintf("  &p_intmasks 0x%x  dispatch0 0x%x  dispatch1 0x%x\n",
		&(pd->p_intmasks), pd->p_intmasks.dispatch0,
		pd->p_intmasks.dispatch1);
	qprintf("  p_warbits 0x%x  p_slice 0x%x  p_routertick %d\n", pd->p_warbits, pd->p_slice, pd->p_routertick);
        qprintf("<FRS> p_frs_objp: 0x%x, p_frs_flags: 0x%x\n",
                pd->p_frs_objp, pd->p_frs_flags);
#endif

#ifdef NUMA_BASE
        qprintf("    p_mem_tick_flags 0x%x quiesce %d base_period %d counter %d seq %d\n",
                pd->p_mem_tick_flags, pd->p_mem_tick_quiesce, pd->p_mem_tick_base_period,
                pd->p_mem_tick_counter, pd->p_mem_tick_seq);
        qprintf("    p_mem_cpu_numpfns %d\n", pd->p_mem_tick_cpu_numpfns);
        qprintf("    bounce_numpfns %d, bounce_startpfn %d, bounce_accpfns %d\n",
                pd->p_mem_tick_bounce_numpfns,
                pd->p_mem_tick_bounce_startpfn,
                pd->p_mem_tick_bounce_accpfns);
        qprintf("    unpegging_numpfns %d, unpegging_startpfn %d, unpegging_accpfns %d\n",
                pd->p_mem_tick_unpegging_numpfns,
                pd->p_mem_tick_unpegging_startpfn,
                pd->p_mem_tick_unpegging_accpfns);
        qprintf("    MemTick MaxTime: %d [us], MinTime %d [us], LastTime %d [us], AvgTime %d [us]\n",
                (pd->p_mem_tick_maxtime * timer_unit) / 1000,
                (pd->p_mem_tick_mintime * timer_unit) / 1000,
                (pd->p_mem_tick_lasttime * timer_unit) / 1000,
                (pd->p_mem_tick_avgtime * timer_unit) / 1000);
#endif /* NUMA_BASE */        
#if CELL
	qprintf("  blaptr 0x%x\n", pd->p_blaptr);
#endif
#if JUMP_WAR
	qprintf("jump_war_pid %d, jump_war_uthreadid %d\n", 
		pd->p_jump_war_pid, pd->p_jump_war_uthreadid);
#endif
#if defined (R10000) && defined (R10000_MFHI_WAR)
        qprintf("    war_bits %d mfhi_brcnt %d mfhi_cnt %d skip_cnt %d patchbuf %x\n",
		pd->p_r10kwar_bits, pd->p_mfhi_brcnt, pd->p_mfhi_cnt,
		pd->p_mfhi_skip, pd->p_mfhi_patch_buf);
#endif
}

void
idbg_donodepda(nodepda_t *npda)
{
	extern void idbg_graph_vertex_name(vertex_hdl_t);
#ifdef SN0
	char slotname[SLOTNUM_MAXLENGTH];
	int i;
	nodepda_router_info_t *npda_rip;
#endif
	if (!npda)
		return;
	qprintf("Dumping nodepda at 0x%x\n", npda);
#ifdef	NUMA_BASE
	qprintf(" use nodenuma to get numa parameters\n");
#endif

	/* First print generic stuff */
	qprintf(" node_zones 0x%x node_pg_data %x vertex_hdl %x pdinfo %x\n",
		&npda->node_zones, &npda->node_pg_data, 
		npda->node_vertex, npda->pdinfo);
#if defined (SN0)
	qprintf("node_first_cpu 0x%x node_num_cpus 0x%x\n",
		npda->node_first_cpu, npda->node_num_cpus);
#endif /* SN0 */
	qprintf(" Error pages 0x%x Discard pages 0x%x Clean pages 0x%x\n",
		npda->error_page_count, npda->error_discard_count, 
		npda->error_cleaned_count);
	if (npda->error_discard_count) 
		qprintf("First bad pfdat 0x%x Last bad pfdat 0x%x\n",
			npda->error_discard_plist.pf_next,
			npda->error_discard_plist.pf_prev);
	qprintf("\n");

#if	SN0
	qprintf(" nasidmask: 0x");
	for (i = (NASID_MASK_BYTES - 1); i >= 0; i--)
		qprintf("%x", npda->nasid_mask[i]);
	qprintf("\n intr disp0: %x disp1: %x\n next_prof_timeout %d\n", 
			&npda->intr_dispatch0, &npda->intr_dispatch1, 
			npda->next_prof_timeout);

	/* Print out the information about all the dependent routers
	 * on this guardian node.
	 */
	npda_rip = npda->npda_rip_first;
	while(npda_rip) {
		char			slotname[SLOTNUM_MAXLENGTH];
		char			rtrname[ROUTER_NAME_SIZE];

		get_slotname(npda_rip->router_slot,slotname);
		get_routername(npda_rip->router_type,rtrname);
		
		qprintf(" router: vhdl 0x%x routerinfop 0x%x routernext 0x%x "
			" port %d  portmask 0x%x vector 0x%x module %d slot %s"
			" type %s\n",
			npda_rip->router_vhdl, npda_rip->router_infop,
			npda_rip->router_next,
			npda_rip->router_port, npda_rip->router_portmask,
			npda_rip->router_vector,
			npda_rip->router_module,slotname,rtrname);
		npda_rip = npda_rip->router_next;
	}

	qprintf(" membank flavor: %d (0 - standard , 1 - premium)\n",
		npda->membank_flavor);
	qprintf(" hub: hubstat 0x%x ticks %d\n", &(npda->hubstats),
			npda->hubticks);
	qprintf(" xbow_vhdl 0x%x  xbow_peer nasid %d basew_id %d \n",
			npda->xbow_vhdl, npda->xbow_peer, npda->basew_id);	
	get_slotname(npda->slotdesc, slotname);
	qprintf(" module %d  slotname %s\n", npda->module_id, slotname);
	qprintf(" Hub rev %d\n", npda->hub_chip_rev);
#if defined (HUB_II_IFDR_WAR)
	if (WAR_II_IFDR_ENABLED) {
		qprintf(" dmatimeout kick cnt %d \n", npda->hub_dmatimeout_kick_count);
		qprintf("\n dma_timeout");
		idbg_chklock(&npda->hub_dmatimeout_kick_lock);
		qprintf("hub_ifdr_ovflw_count %d hub_ifdr_check_count %d\n",
			npda->hub_ifdr_ovflw_count, npda->hub_ifdr_check_count);
	}
#endif
#endif	/* SN0 */

#if	SN0 || IP30
	qprintf(" widget_info %x\n", npda->widget_info);
#endif
	

}

void
idbg_nodepda(__psint_t x)
{
	int	i;

	if (x >= 0)
		idbg_donodepda(NODEPDA(x)); 
	else if (x == -1) {
		for (i = 0; i < numnodes; i++) {
			idbg_donodepda(NODEPDA(i));
		}
	} else {
		idbg_donodepda((nodepda_t *)x);
	}
}

char *tab_ptlock[] = {
	"prclck",	/* 0x00000001 */
	"txtlck",	/* 0x00000002 */
	"datlck",	/* 0x00000004 */
	"pglck",	/* 0x00000008 */
	"UNKNOWN",	/* 0x00000010 */
	"UNKNOWN",	/* 0x00000020 */
	"UNKNOWN",	/* 0x00000040 */
	"UNKNOWN",	/* 0x00000080 */
};

/*
 * User	area handling.
++
++	kp user x - dump our	user area (upage)
++		x =	-1 (default) print out current process's uarea
++		x <	0 use x	as address of proc struct
++		x >	0 use x	as a pid - look up proc with that pid
++
 */

void
idbg_uarea(__psint_t x)
{
	proc_t *pp;

	if ((__psint_t) x == -1L) {
		pp = curprocp;
		if (pp == NULL) {
			qprintf("no current process\n");
			return;
		}
		qprintf("user area dump for current process\n");
	}
	else if (x < 0) {
		pp = (proc_t *)x;
		if (!procp_is_valid(pp)) {
			qprintf("WARNING:0x%x is not a valid proc entry\n", x);
		}
	}
	else {
		if ((pp = idbg_pid_to_proc(x)) == NULL) {
			qprintf("%d is not an active pid\n", (pid_t)x);
			return;
		}
	}

	idbg_upage(pp);
}

void
idbg_upage(proc_t *pp)
{
	qprintf("user area dump for proc 0x%x (pid %d)\n",
		pp, pp->p_pid);

	qprintf("  psargs \"%s\" comm \"%s\"\n", pp->p_psargs, pp->p_comm);
	qprintf("  &u_profp 0x%x u_profn %d &rlimit 0x%x\n",
		pp->p_profp, pp->p_profn, pp->p_rlimit);
	qprintf("  dbgflags 0x%x\n",
		pp->p_dbgflags);

#ifdef later
	if (sat_enabled) {
		char *cwd=NULL, *root=NULL;
		qprintf(" satid %d\n", pp->p_sat_proc->sat_uid);
		if (pp->p_sat_proc->sat_wd) {
			if (pp->p_sat_proc->sat_wd->cwd_len)
				cwd = pp->p_sat_proc->sat_wd->data;
			if (pp->p_sat_proc->sat_wd->root_len)
				root = pp->p_sat_proc->sat_wd->data +
				pp->p_sat_proc->sat_wd->cwd_len;
		}
		qprintf("  cdir %s rdir %s satrec 0x%x",
			(cwd)? cwd: "(null)", (root)? root: "(null)",
			pp->p_sat_proc->sat_pn);
		qprintf(" subsys %d\n", pp->p_sat_proc->sat_subsysnum);
		if ((pp->p_sat_proc->sat_suflag & SAT_SUSERCHK)) {
			qprintf("  suser_checked, ");
			if (pp->p_sat_proc->sat_suflag & SAT_SUSERPOSS)
				qprintf("possessed\n");
			else
				qprintf("not possessed\n");
		}
	}
#endif

	qprintf("  &p_cru 0x%x unblkpid %d",
		(__psint_t)&pp->p_cru, pp->p_unblkonexecpid);

	if (pp->p_fdt == NULL)
		qprintf("  no file descriptor table\n");
	else {
		qprintf("  &fdt 0x%x\n", pp->p_fdt);
	}
	qprintf("  aiocbhd 0x%x\n", pp->p_kaiocbhd);
}

void
idbg_fdt(fdt_t *fdt)
{
	struct ufchunk	*ufp;
	int 		i;

	qprintf("  fdt at address 0x%x:\n", fdt);
	qprintf("  nofiles %d &flist 0x%x &fdt_lock 0x%x &syncs 0x%x\n",
		fdt->fd_nofiles, fdt->fd_flist, &fdt->fd_lock, fdt->fd_syncs);
	ufp = fdt->fd_flist;
	qprintf("  fd      uf_ofile               uf_pofile      uf_inuse"
		"       uf_waiting\n");
	for (i = 0; i < fdt->fd_nofiles; i++) {
	        if (ufp[i].uf_ofile == NULL)
		        continue;
		qprintf("  %d\t0x%x\t0x%x\t\t%d\t\t%d\n",
			i,
			ufp[i].uf_ofile, 
			ufp[i].uf_pofile, 
			ufp[i].uf_inuse,
			ufp[i].uf_waiting);
		  }
}

/*
 * Dump the runq queue.
++
++	kp runq - dump the run queue(s)
++
 */
void
idbg_runq(__psint_t howmany)
{
	dumpRunq(howmany);
}


void
dumpRunq(__psint_t n)
{
	kthread_t *kt;
	int i;
	pda_t *pda;

	if (n == 0) {
		qprintf("(no runqinfo)\n");
		return;
	}
	qprintf("idler %d\n\n", PRIMARY_IDLE);
	for (i = 0; i < maxcpus; i++) {
		if (pda = pdaindr[i].pda) {
			qprintf("CPU %2d: %s\n", pda->p_cpuid,
				pda->p_frs_flags ? "FRS" : "");
			if (kt = pda->p_curkthread) {
				qprintf("  on cpu 0x%x     pri %d affnode %d %s\n",kt,kt->k_pri, kt->k_affnode, idbg_kthreadname(kt));
			}
			if (pda->p_nextthread && kt != pda->p_nextthread) {
				kt = pda->p_nextthread;
				qprintf("    next 0x%x     pri %d affnode %d %s\n",kt,kt->k_pri, kt->k_affnode, idbg_kthreadname(kt));
			}
			kt = pda->p_cpu.c_threads;
			if (kt) do {
				qprintf("     kthread 0x%x pri %d affnode %d %s %s\n",
					kt, kt->k_pri, kt->k_affnode, 
					idbg_kthreadname(kt),
					kt->k_flags & KT_HOLD
						? "affinity" : "");
				kt = kt->k_rflink;
			} while (kt != pda->p_cpu.c_threads);
		}
	}
	qprintf("\nRealtime overflow q:\n");
	if (kt = rt_gq)
		do {
			qprintf("     kthread 0x%x pri %d affnode %d %s\n", kt, kt->k_pri, kt->k_affnode, idbg_kthreadname(kt));
			kt = kt->k_rflink;
		} while (kt != rt_gq);
	qprintf("\n");
}

/*
 * Getpages list dump
 */
static void
idbg_dopglst(pglst_t *pglptr)
{
	register gprgl_t *rlst;

	qprintf("  0x%x:", (__psint_t)pglptr);
	if (rlst = pglptr->gp_rlptr)
		qprintf("preg 0x%x cnt %d", rlst->gp_prp, rlst->gp_count);
	qprintf(" ptptr 0x%x pfd 0x%x apn 0x%x sh 0x%x stat %d\n",
		pglptr->gp_ptptr, pglptr->gp_pfd, pglptr->gp_apn,
		pglptr->gp_sh, pglptr->gp_status);
}

char *tab_swap[] = {
	"indel",	/* 0x00000001 */
	"notready",	/* 0x00000002 */
	"stale",	/* 0x00000004 */
	"local",	/* 0x00000008 */
	"ioerr",	/* 0x00000010 */
	"eacces",	/* 0x00000020 */
	"boot",		/* 0x00000040 */
};

/*
 * Dump swap table
++
++	kp swap  - dump swap table
++
 */
/* ARGSUSED */
void
idbg_swap(__psint_t x)
{
	int i;
	register swapinfo_t *st;

	for (i = 0; i < MAXLSWAP; i++) {
		if ((st = lswaptab[i]) == NULL)
			continue;
		qprintf("0x%x:lswap %d name %s bmap 0x%x cksum 0x%x\n",
			st, i, st->st_name, st->st_bmap, st->st_cksum);
		qprintf("  next %d start %lld length %lld swppglo %ld pgs %ld nfpgs %ld\n",
			st->st_next, st->st_start, st->st_length,
			st->st_swppglo, st->st_npgs, st->st_nfpgs);
		qprintf("  maxpgs %ld vpgs %ld pri %d list 0x%x vp 0x%x\n",
			st->st_maxpgs, st->st_vpgs,
			st->st_pri, st->st_list, st->st_vp);
		_printflags((unsigned)st->st_flags, tab_swap, "flags");
		qprintf("\n");
	}
}

/*
 * Dump out pglst
++
++	kp pglst x - dump pglst
++		x < 0 use x as address of pglst
++		x > 0 use x a index of pglst
++
 */
void
idbg_pglst(__psint_t i)
{
	extern struct pglsts pglsts[];
	struct pglsts *s;

	if (i < 0) {
		idbg_dopglst((pglst_t *)i);
	} else {
		qprintf(" page list %d ", i);
		if (i >= NPGLSTS) {
			qprintf("does not exist\n");
			return;
		}
		s = &pglsts[i];
		qprintf(" at 0x%x lreg 0x%x index %d ndone %d ntran %d\n",
			s, s->lockedreg, s->index, s->ndone, s->ntran);
		for (i = 0; i < maxpglst; i++)
			idbg_dopglst(&s->list[i]);
	}
}


/*
 * This function returns the kernel address, if any, associated with the
 * symbol name passed in.
 */
/* ARGSUSED */
void
idbg_kaddr(__psint_t arg0, char *arg1)
{
	__psunsigned_t addr;

	if (idbginvoke != IDBGUSER) {
		qprintf("kaddr only valid for user level idbg\n");
		return;
	}
	
	if (!arg1) {
		qprintf("kaddr needs an argument\n");
		return;
	}
	
	addr = fetch_kaddr(arg1);
	qprintf("0x%x\n", addr);
	return;
}

void
idbg_do_eframe(eframe_t *ep)
{
	extern char *code_names[];
	int code;

	code = ep->ef_cause & CAUSE_EXCMASK;
	qprintf("exception frame at 0x%x\n", ep);
	qprintf("  cause %R", (uint)ep->ef_cause, cause_desc);
	qprintf("  code:%d '%s'\n", code, code_names[code>>CAUSE_EXCSHIFT]);
	qprintf("  sr %R\n",(uint)ep->ef_sr,
#if R4000 && R10000
		IS_R10000() ? r10k_sr_desc :
#endif /* R4000 && R10000 */
						sr_desc);
	qprintf("  epc: 0x%llx badvaddr: 0x%llx\n", 
		ep->ef_epc, ep->ef_badvaddr);
#ifdef EVEREST
	qprintf ("  cel: 0x%llx\n", ep->ef_cel);
#endif
#if TFP
	qprintf ("  config: 0x%x\n", ep->ef_config);
#endif
#if IP32
	qprintf ("  crmmsk: 0x%llx\n", ep->ef_crmmsk);
#endif
	printregs(ep, qprintf);
}

void
idbg_do_ut_eframe(struct uthread_s *ut)
{
	exception_t *up;

	if ((up = ut->ut_exception) == NULL) {
		qprintf("No exception area for user thread 0x%x\n", ut);
		return;
	}
	idbg_do_eframe(&up->u_eframe);
}

/*
++
++	kp eframe addr - Dump an exception frame at addr
++		addr == -1 - use upage of current process
++		addr < 0 - use addr as address of proc struct
++				if not a proc addr use as addr of eframe
++		addr >= 0 - use addr as pid - look up proc for that pid
++
 */
void
idbg_eframe(__psint_t x)
{
#ifdef R4000
	if (x == PHYS_TO_K1(CACHE_ERR_EFRAME)) {
		idbg_do_eframe((eframe_t *)x);
		return;
	}
#endif
	if (idbg_doto_uthread(x, idbg_do_ut_eframe, 1, 1) == 0) {
		qprintf(
"0x%x is not a uthread address, using it as address of exception frame\n", x);
		idbg_do_eframe((eframe_t *)x);
	}
}

/*
 * backtrace user processes
 * If we get an address in K1SEG but < start its the address of	a block
 * of info describing the current stopped state from dbgmon
 * use that instead of u_pcb
++
++	kp ubt x - stack backtrace process
++		x < 0 either current process OR special (see dbgmon)
++		x > 0 use x as pid - look up proc for that pid - (for
++		    SLEEPING processes ONLY)
++
 */
#if EXTKSTKSIZE == 1
unsigned *physextspbase;	/* store xlation for extension sp's */
#endif
unsigned *physspbase;		/* store xlation for sp's */

struct brkpt_table *brkpt_table;/* will point to symmon's break table */
				/* only used by idbg_ubtrace */

static void btrace(unsigned *, unsigned *, unsigned *, int,
			unsigned *, kthread_t *, int);

/* &stopregs[0] where:
	stopreg[0] = ef_epc
	stopreg[1] = ef_sp
	stopreg[2] = ef_ra
	stopreg[3] = &brkpt_table[0]
		OR
	stopreg[0] = -1
	stopreg[1] = User proc #;
	stopreg[2] = 0;
	stopreg[3] = &brkpt_table[0]
*/
void
idbg_ubtrace(__psint_t *x)
{
	proc_t *p;
	uthread_t *ut;
	kthread_t *kt;
	k_machreg_t pc, sp;

   	brkpt_table = (struct brkpt_table *)x[3];
	if ((__psint_t) x[0] != -1L) { /* non -1 must be current stack */

		qprintf("backtrace for current stack\n");
		physspbase = (unsigned *)((__psunsigned_t)KSTACKPAGE);
#if EXTKSTKSIZE == 1
		/* point to kernel stack extension page */
		physextspbase = (unsigned *)((__psunsigned_t)KSTACKPAGE - NBPP);
#endif
		/* got a special info block:
			 * x[0] - starting pc
			 * x[1] - starting sp
			 * x[2] - starting ra
			 * if pc < 0x80000000 then assume a user stack and
			 * set lower bound to 0x400000 else set lower
			 * bound to start
			 */
		btrace((uint *)x[0], (uint *)x[1], (uint *)x[2],
				60,
				x[0] > 0 ? (uint *)0x400000L : (uint *)UT_VEC,
				private.p_curkthread, 0);
		return;
	}

	/*
	 * If x[0] == -1  User passed in some process/thread id
	 * If x[1] < 0, assume to be a uthread, proc, sthread, or ithread addr
	 * otherwise, try to look it up as a pid, or a kthread id.
	 * If its a pid print the first uthread in chain.
	 */
	physspbase = 0;
	if (x[1] < 0) {
	    if (issthread((sthread_t *)x[1])) {
		kt = ST_TO_KT((sthread_t *)x[1]);
dosthread:
		qprintf("backtrace for sthread 0x%x [%s]\n", kt,
			KT_TO_ST(kt)->st_name);
		if (kt->k_eframe) {
			pc = kt->k_eframe->ef_epc;
			sp = kt->k_eframe->ef_sp;
		} else {
			pc = kt->k_regs[PCB_PC];
			sp = kt->k_regs[PCB_SP];
		}

		btrace((uint*)pc, (uint*)sp, (uint*)0, 60, (uint*)UT_VEC, kt, 0);
		return;
	    } else if (isxthread((xthread_t *)x[1])) {
		kt = XT_TO_KT((xthread_t *)x[1]);
doxthread:
		qprintf("backtrace for xthread 0x%x [%s]\n", kt,
			KT_TO_XT(kt)->xt_name);
		if (kt->k_eframe) {
			pc = kt->k_eframe->ef_epc;
			sp = kt->k_eframe->ef_sp;
		} else {
			pc = kt->k_regs[PCB_PC];
			sp = kt->k_regs[PCB_SP];
		}

		btrace((uint*)pc, (uint*)sp, (uint*)0, 60, (uint*)UT_VEC, kt, 0);
		return;
	    } else if (uthreadp_is_valid((uthread_t *)x[1])) {
		ut = (uthread_t *)x[1];
		p = UT_TO_PROC(ut);
	    } else {
		p = (proc_t *)x[1];
		if (!procp_is_valid(p)) {
			qprintf("WARNING:0x%x is not a valid proc address\n",
				p);
		}
		ut = p->p_proxy.prxy_threads;
	    }
	} else {
		if ((uint64_t)x[1] > 0x100000000ULL) {
			int type;
			__psunsigned_t ptr;

			type = find_kthreadid((uint64_t)x[1], 0, &ptr);
			if (type == KID_IS_UTHREAD) {
				ut = (uthread_t *)ptr;
				p = UT_TO_PROC(ut);
			} else if (type == KID_IS_STHREAD) {
				kt = ST_TO_KT((sthread_t *)ptr);
				goto dosthread;
			} else if (type == KID_IS_XTHREAD) {
				kt = XT_TO_KT((xthread_t *)ptr);
				goto doxthread;
			} else {
				qprintf("%d not a valid kthread id\n", x[1]);
				return;
			}
		} else if ((p = idbg_pid_to_proc((pid_t)x[1])) != NULL) {
			ut = p->p_proxy.prxy_threads;
		} else {
			qprintf("%d is not an active pid\n", (pid_t)x[1]);
			return;
		}
	}

	kt = UT_TO_KT(ut);

	pc = kt->k_regs[PCB_PC];
	sp = kt->k_regs[PCB_SP];

	qprintf("backtrace for proc 0x%x uthread 0x%x, pid %d, pc 0x%x sp 0x%x\n",
			p, ut, p->p_pid, pc, sp);

	if(convsp_start(ut))
		btrace((uint*)pc, (uint*)sp, (uint*)0, 60, (uint*)UT_VEC, kt, 0);

	convsp_end();

}

/*
 * Set up for convsp routine.
 */
static int
convsp_start(struct uthread_s *ut)
{
	/*
	 * For physspbase, use a separate kernel virtual address for 
	 * two reasons:
	 * 1) A kernel stack may reside in upper physical memory on a large 
	 *    memory system running a 32-bit kernel so there is no K0SEG 
	 *    representation for it.
	 * 2) On R4000's, we need to avoid VCE's on kernel stack.
	 *
	 * We leave the address mapped after the backtrace so that
	 * a subsequent 'dump' command to look at more info will work.
	 * We have to unmap and clear it prior to re-use in case
	 * the backtrace didn't complete normally and generated an
	 * exception
	 */
#if EXTKSTKSIZE == 1
	/*
	 * See if the process has stack extension page, if so physextspbase
	 * points to it.
	 */
	if (ut->ut_kstkpgs[KSTEIDX].pgi) {
		physextspbase = extsp_kvaddr;
		if (!physextspbase) {
			qprintf(
			  "No extsp_kvaddr allocated for stack traceback!\n");
			return 0;
		}
	}
#endif

#if (_MIPS_SIM == _ABI64) && !defined(R4000)	/* r4k needs it to avoid VCE */
	/*
	 * 64 bit kernels don't need to map the stack page.
	 * We can jut use a k0seg address.
	 */
	physspbase = (uint *)PHYS_TO_K0(ctob(ut->ut_kstkpgs[KSTKIDX].pte.pg_pfn));
#else
	physspbase = sp_kvaddr;
	if (!physspbase) {
		qprintf("No sp_kvaddr allocated for stack traceback!\n");
		return 0;
	}

	pg_setpgi(kvtokptbl(sp_kvaddr),
		  mkpde(PG_VR|PG_G|PG_SV|PG_M|pte_cachebits(),
			ut->ut_kstkpgs[KSTKIDX].pte.pg_pfn));

	unmaptlb(0, btoc(sp_kvaddr));
#endif
#if EXTKSTKSIZE == 1
	/* Do the same thing for kernel stack ext */
	if (ut->ut_kstkpgs[KSTEIDX].pgi) {
		pg_setpgi(kvtokptbl(extsp_kvaddr),
			  mkpde(PG_VR|PG_G|PG_SV|PG_M|pte_cachebits(),
			        ut->ut_kstkpgs[KSTEIDX].pte.pg_pfn));
		unmaptlb(0, btoc(extsp_kvaddr));
	}
#endif

	return 1;
}


static void
convsp_end(void)
{
	physspbase = 0;				/* sanity */
#if EXTKSTKSIZE == 1
	physextspbase = 0;				/* sanity */
#endif

#ifdef MH_R10000_SPECULATION_WAR
        if (IS_R10000()) {
                pg_clrpgi(kvtokptbl(sp_kvaddr));
                unmaptlb(0, btoc(sp_kvaddr));
#if EXTUSIZE == 1
                /* Do the same thing for kernel stack ext */
                if (ut->ut_kstkpgs[KSTEIDX].pgi) {
                        pg_clrpgi(kvtokptbl(extsp_kvaddr));
                        unmaptlb(0, btoc(extsp_kvaddr));
                }
#endif
        }
#endif /* MH_R10000_SPECULATION_WAR */

}

/*
 * convert kernel stack references to physical references
 * Workaround for compiler bug: pass physsp instead
 * of global access!
 */
static unsigned *
convsp(register unsigned *sp, unsigned *physsp)
{
	register __psunsigned_t nsp;

	if (sp > (unsigned *)((__psunsigned_t)KSTACKPAGE) &&
	    sp < (unsigned *)KERNELSTACK) {
		/* compute required # to convert from stack address to
		 * physical stack address
		 */
		nsp = ((__psunsigned_t)sp - (__psunsigned_t)KSTACKPAGE) +
			(__psunsigned_t)physsp;
		return((unsigned *)nsp);
	} else
#if EXTKSTKSIZE == 1
		/*
		 * See if sp falls in the kernel stack ext range if
		 * so, use physextspbase to calculate nsp
		 */

		if (sp > (unsigned *)((unsigned)KSTACKPAGE-NBPP) &&
	    		sp < (unsigned *)((unsigned)KSTACKPAGE)) {
		/* compute required # to convert from ext stack address to
		 * physical stack address
		 */
		nsp = ((__psunsigned_t)sp -
				(__psunsigned_t)KSTACKPAGE-NBPP) +
				(__psunsigned_t)physextspbase;
		return((unsigned *)nsp);
	} else
#endif
		return(sp);
}

void idbg_get_stack_bytes(struct uthread_s *ut, void *sp, void *dp, int n)
{
	char		*csp=sp;
	char		*cdp=dp;

	convsp_start(ut);
	while(n)
		bcopy(convsp((unsigned *)csp++, physspbase), cdp++, n--);
	convsp_end();
		
}

/*
 * asm stack backtrace facility
 *
 * Stack Frame:
 * oldsp|		|
 *	|  arg n	|
 *	|		|frame_size in addiu
 *	|  arg 1	|
 *	|  locals	|
 *	|  save regs+ra	|
 *	|		|
 *	|  arg build	|
 * sp ->|		|
 *
 * Four code templates for adjusting $sp and $ra are recognized.
 *
 * For 32-bit kernels:
 *	addiu	$sp, 32
 *	sw	$ra, 20($sp)
 * OR
 *	li	$at, 32
 *	addu	$sp, $sp, $at
 *	sw	$ra, 20($sp)
 *
 *
 * For 64-bit kernels:
 *	daddiu	$sp, 32
 *	sw	$ra, 20($sp)
 * OR
 *	li	$at, 32
 *	daddu	$sp, $sp, $at
 *	sw	$ra, 20($sp)
 *
 * TODO	- LEAF procs with no frames!
 */
#define	output qprintf
#define DADDIU_MASK	0x67bd0000	/* daddiu sp, sp, # */
#define DADDU_MASK	0x03a1e82d	/* daddu sp, sp, at */
#define DSUBU_MASK	0x03a1e82f	/* dsubu sp, sp, at */
#define LIAT_MASK	0x24010000	/* li at,# */
#define ORI_MASK	0x34010000	/* ori zero,# */
#define ADDIU_MASK	0x27bd0000	/* addiu sp, sp, # */
#define ADDU_MASK	0x03a1e821	/* addu sp, sp, # */

#define SDRA_MASK	0xffbf0000	/* sd ra, x(sp) */
#define SWRA_MASK	0xafbf0000	/* sw ra, x(sp) */

#define SDA0_MASK	0xffa40000	/* sd a0, x(sp) */
#define SDA1_MASK	0xffa50000	/* sd a1, x(sp) */
#define SDA2_MASK	0xffa60000	/* sd a2, x(sp) */
#define SDA3_MASK	0xffa70000	/* sd a3, x(sp) */
#define SDA4_MASK	0xffa80000	/* sd a4, x(sp) */
#define SDA5_MASK	0xffa90000	/* sd a5, x(sp) */
#define SDA6_MASK	0xffaa0000	/* sd a6, x(sp) */
#define SDA7_MASK	0xffab0000	/* sd a7, x(sp) */

#define SWA0_MASK	0xafa40000	/* sw a0, x(sp) */
#define SWA1_MASK	0xafa50000	/* sw a1, x(sp) */
#define SWA2_MASK	0xafa60000	/* sw a2, x(sp) */
#define SWA3_MASK	0xafa70000	/* sw a3, x(sp) */
#define SWA4_MASK	0xafa80000	/* sw a4, x(sp) */
#define SWA5_MASK	0xafa90000	/* sw a5, x(sp) */
#define SWA6_MASK	0xafaa0000	/* sw a6, x(sp) */
#define SWA7_MASK	0xafab0000	/* sw a7, x(sp) */


#if _MIPS_SIM == _ABI64
#define ADDISP_MASK	DADDIU_MASK
#if defined(_COMPILER_VERSION) && (_COMPILER_VERSION>=700) && defined(R4000_DADDIU_WAR)
/* Mongoose */
#define ADDSP_MASK	DSUBU_MASK
#define LI_MASK	ORI_MASK
#define LISIGN		POSITIVE
#else
#define ADDSP_MASK	DADDU_MASK
#define LI_MASK		LIAT_MASK
#define LISIGN		NEGATIVE
#endif /* _COMPILER_VERSION>=700 */
#define STRA_MASK	SDRA_MASK
#define STRA2_MASK	SDRA_MASK
#define NARGS		8
#elif _MIPS_SIM == _ABIN32
#define ADDISP_MASK	ADDIU_MASK
#define ADDSP_MASK	ADDU_MASK
#define STRA_MASK	SDRA_MASK
#define STRA2_MASK	SWRA_MASK
#define NARGS		8
#define LI_MASK		LIAT_MASK
#define LISIGN		NEGATIVE
#elif _MIPS_SIM == _ABIO32
#define ADDISP_MASK	ADDIU_MASK
#define ADDSP_MASK	ADDU_MASK
#define STRA_MASK	SWRA_MASK
#define NARGS		4
#define LI_MASK		LIAT_MASK
#define LISIGN		NEGATIVE
#else
<<BOMB - unknown _MIPS_SIM>>
#endif /* _MIPS_SIM */

enum direction	{ FORWARD,	BACKWARD };
enum sign	{ NOSIGN,	POSITIVE,	NEGATIVE };

unsigned int arg_inst[] = {
	SWA0_MASK, SWA1_MASK, SWA2_MASK, SWA3_MASK,
	SWA4_MASK, SWA5_MASK, SWA6_MASK, SWA7_MASK /*only used in 64 bit mode*/
};

/* in 64 bit mode, search for either sd or sw */
unsigned int arg_inst2[] = {
	SDA0_MASK, SDA1_MASK, SDA2_MASK, SDA3_MASK,
	SDA4_MASK, SDA5_MASK, SDA6_MASK, SDA7_MASK
};

/* special stuff to allow tracing past exceptions etc */
#define SF_EXC		1	/* use previous a0 as an exception frame */
#define SF_NOSPEC	2	/* nothing special (stack-wise) */
#define SF_LEAF		3	/* exception returnd to LEAF func w/ no addiu */
#define SF_IGN		4	/* ignore symbol */

extern int VEC_int(), VEC_syscall(), VEC_tlbmiss(), VEC_tlbmod();
extern int VEC_trap(), VEC_cpfault(), VEC_addrerr(), systrap(), utlbmiss();
extern int kpreemption();
extern int addupc(inst_t *, struct prof *, int, int);

#if R4000 && (! _NO_R4000)
/* part of error tracking - these can hide other more important symbols
 * for backtracing.
 */
extern int locore_exl_1(), locore_exl_2(), locore_exl_3(),
	locore_exl_7(), locore_exl_4(),
	elocore_exl_0(), elocore_exl_1(), elocore_exl_2(),
	elocore_exl_3(), elocore_exl_4(), elocore_exl_5(),
	elocore_exl_6(), elocore_exl_7(), elocore_exl_8(),
	locore_exl_8(), elocore_exl_9(), elocore_exl_10(),
	locore_exl_11(), elocore_exl_11();
extern	int	locore_exl_12(), elocore_exl_12();
extern	int	locore_exl_13(), elocore_exl_13();
#ifdef _R5000_CVT_WAR
extern	int	locore_exl_14(), elocore_exl_14();
extern	int	locore_exl_15(), elocore_exl_15();
#endif /* _R5000_CVT_WAR */
extern	int	locore_exl_16(), elocore_exl_16();
#ifdef USE_PTHREAD_RSA
extern	int	locore_exl_17(), elocore_exl_17();
#endif /* USE_PTHREAD_RSA */
extern	int	locore_exl_18(), elocore_exl_18();
extern	int	locore_exl_19(), elocore_exl_19();
extern	int	locore_exl_20(), elocore_exl_20();
extern	int	locore_exl_21(), elocore_exl_21();
extern	int	locore_exl_22(), elocore_exl_22();
extern	int	locore_exl_23(), elocore_exl_23();
extern	int	locore_exl_24(), elocore_exl_24();
extern	int	locore_exl_25(), elocore_exl_25();
#endif

static struct spec_funcs {
	int (*func)();
	char *name;
	unsigned type;
} sfuncs[]  = {
	{ VEC_int, "VEC_int", SF_EXC },
	{ kpreemption, "kpreemption", SF_EXC },
	{ VEC_syscall, "VEC_syscall", SF_EXC },
	{ VEC_tlbmiss, "VEC_tlbmiss", SF_EXC },
	{ VEC_tlbmod, "VEC_tlbmod", SF_EXC },
	{ VEC_trap, "VEC_trap", SF_EXC },
	{ VEC_cpfault, "VEC_cpfault", SF_EXC },
	{ VEC_addrerr, "VEC_addrerr", SF_EXC },
	{ systrap, "systrap", SF_EXC },
#if !defined (TFP) && !defined (PSEUDO_BEAST)
	/* not a wired address on tfp, what do we do? */
	{ (int (*)())UT_VEC, "utlbmiss", SF_EXC },
#endif
	/* if you want to backtrace from a LEAF procedure - list it here */
	{ (int (*)())splx, "splx", SF_LEAF },
	{ spl0, "spl0", SF_LEAF },
	{ spl1, "spl1", SF_LEAF },
	{ splnet, "splnet", SF_LEAF },
	{ splhi, "splhi", SF_LEAF },
	{ splimp, "splimp", SF_LEAF },
	{ spl5, "spl5", SF_LEAF },
	{ spl6, "spl6", SF_LEAF },
	{ spl65, "spl65", SF_LEAF },
	{ spl7, "spl7", SF_LEAF },
	{ splhi_relnet, "splhi_relnet", SF_LEAF },
	{ (int(*)())bcopy, "bcopy", SF_LEAF },
	{ (int(*)())ovbcopy, "ovbcopy", SF_LEAF },
	{ (int(*)())bzero, "bzero", SF_LEAF },
	{ fubyte, "fubyte", SF_LEAF },
	{ fuword, "fuword", SF_LEAF },
	{ (int (*)())subyte, "subyte", SF_LEAF },
	{ suword, "suword", SF_LEAF },
	{ addupc, "addupc", SF_LEAF },
	{ upath, "upath", SF_LEAF },
	{ (int (*)())strlen, "strlen", SF_LEAF },
	{ sfu32v, "sfu32v", SF_LEAF },
	{ (int (*)())sfu32, "sfu32", SF_LEAF },
	{ spu32, "spu32", SF_LEAF },
#if defined(DEBUG) && !defined(JUMP_WAR)
	{ (int (*)())_unmaptlb, "unmaptlb", SF_LEAF },
#else
	{ (int (*)())unmaptlb, "unmaptlb", SF_LEAF },
#endif
	{ (int (*)())invaltlb, "invaltlb", SF_LEAF },
	{ (int (*)())tlbwired, "tlbwired", SF_LEAF },
	{ (int (*)())tlbdropin, "tlbdropin", SF_LEAF },
#if R4000 || R10000
	{ (int (*)())tlbdrop2in, "tlbdrop2in", SF_LEAF },
#endif
	{ (int (*)())flush_tlb, "flush_tlb", SF_LEAF },
	{ (int (*)())set_tlbpid, "set_tlbpid", SF_LEAF },
#if TFP
	{ (int (*)())set_icachepid, "set_icachepid", SF_LEAF },
#endif
	{ save, "save", SF_LEAF },
	{ (int (*)())resume, "resume", SF_LEAF },
	{ setjmp, "setjmp", SF_LEAF },
	{ longjmp, "longjmp", SF_LEAF },
#if	!defined(MP)
	{ (int (*)())_get_timestamp, "_get_timestamp", SF_LEAF },
#endif
	{ (int (*)())resumethread, "resumethread", SF_LEAF },
	{ (int (*)())sthread_launch, "sthread_launch", SF_LEAF },

	/*
	 * assembler functions called by various high-level lock routines.
	 */
#ifdef MP
	/* these lock routines are special - depending on the lock
	 * type they are LEAF or NESTED
	 * we plug them in as LEAF (the 'nodebug' version) and have
	 * leafinit sort it out
	 * WARNING - leave these 16 entries together and in this order OR
	 * change leafinit
	 */
	{ (int (*)())mutex_spinlock, "mutex_spinlock", SF_LEAF },
	{ (int (*)())mutex_spinunlock, "mutex_spinunlock", SF_LEAF },
	{ (int (*)())nested_spinlock, "nested_spinlock", SF_LEAF },
	{ (int (*)())nested_spinunlock, "nested_spinunlock", SF_LEAF },
	{ (int (*)())mutex_bitlock, "mutex_bitlock", SF_LEAF },
	{ (int (*)())mutex_bitunlock, "mutex_bitunlock", SF_LEAF },
	{ (int (*)())nested_bitlock, "bitlock", SF_LEAF },
	{ (int (*)())mutex_64bitlock, "mutex_64bitlock", SF_LEAF },
	{ (int (*)())mutex_64bitunlock, "mutex_64bitunlock", SF_LEAF },
	{ (int (*)())nested_64bitlock, "nested_64bitlock", SF_LEAF },
	{ (int (*)())nested_64bitunlock, "nested_64bitunlock", SF_LEAF },
	{ (int (*)())mutex_spintrylock, "mutex_spintrylock", SF_LEAF },
	{ (int (*)())mutex_bittrylock, "mutex_bittrylock", SF_LEAF },
	{ (int (*)())nested_bittrylock, "nested_bittrylock", SF_LEAF },
	{ (int (*)())nested_64bittrylock, "nested_64bittrylock", SF_LEAF },
	{ (int (*)())nested_spintrylock, "nested_spintrylock", SF_LEAF },
	{ (int (*)())compare_and_swap_kt, "compare_and_swap_kt", SF_LEAF },
	{ (int (*)())compare_and_swap_int, "compare_and_swap_int", SF_LEAF },
	{ (int (*)())bitlock_set, "bitlock_set", SF_LEAF },
	{ (int (*)())bitlock_clr, "bitlock_clr", SF_LEAF },
	{ (int (*)())bitlock_acquire, "bitlock_acquire", SF_LEAF },
	{ (int (*)())bitlock_acquire_32bit, "bitlock_acquire_32bit", SF_LEAF },
	{ (int (*)())bitlock_condacq, "bitlock_condacq", SF_LEAF },
	{ (int (*)())bitlock_condacq_32bit, "bitlock_condacq_32bit", SF_LEAF },
#endif
#if R4000 && (! _NO_R4000)
	/*
	 * functions in locore to mark EXL states - never interesting
	 */
	{ locore_exl_1, "VEC_int", SF_EXC },
	{ locore_exl_3, "VEC_tlbmiss", SF_EXC },
	{ locore_exl_2, "VEC_tlbmod", SF_EXC },
	{ locore_exl_7, "VEC_cpfault", SF_EXC },
	{ locore_exl_4, "VEC_addrerr", SF_EXC },
	{ elocore_exl_0, "elocore_exl_0", SF_IGN },
	{ elocore_exl_1, "elocore_exl_1", SF_IGN },
	{ elocore_exl_2, "elocore_exl_2", SF_IGN },
	{ elocore_exl_3, "elocore_exl_3", SF_IGN },
	{ elocore_exl_4, "elocore_exl_4", SF_IGN },
	{ elocore_exl_5, "elocore_exl_5", SF_IGN },
	{ elocore_exl_6, "elocore_exl_6", SF_IGN },
	{ elocore_exl_7, "elocore_exl_7", SF_IGN },
	{ elocore_exl_8, "elocore_exl_8", SF_IGN },
	{ locore_exl_8, "locore_exl_8", SF_IGN },
	{ elocore_exl_9, "elocore_exl_9", SF_IGN },
	{ elocore_exl_10, "elocore_exl_10", SF_IGN },
	{ locore_exl_11, "elocore_exl_11", SF_IGN },
	{ elocore_exl_11, "elocore_exl_11", SF_IGN },
	{ locore_exl_12, "locore_exl_12", SF_IGN },
	{ elocore_exl_12, "elocore_exl_12", SF_IGN },
	{ locore_exl_13, "locore_exl_13", SF_IGN },
	{ elocore_exl_13, "elocore_exl_13", SF_IGN },
#ifdef _R5000_CVT_WAR
	{ locore_exl_14, "locore_exl_14", SF_IGN },
	{ elocore_exl_14, "elocore_exl_14", SF_IGN },
	{ locore_exl_15, "locore_exl_15", SF_IGN },
	{ elocore_exl_15, "elocore_exl_15", SF_IGN },
#endif /* _R5000_CVT_WAR */
	{ locore_exl_16, "locore_exl_16", SF_IGN },
	{ elocore_exl_16, "elocore_exl_16", SF_IGN },
#ifdef USE_PTHREAD_RSA
	{ locore_exl_17, "locore_exl_17", SF_IGN },
	{ elocore_exl_17, "elocore_exl_17", SF_IGN },
#endif /* USE_PTHREAD_RSA */
	{ locore_exl_18, "locore_exl_18", SF_IGN },
	{ elocore_exl_18, "elocore_exl_18", SF_IGN },
	{ locore_exl_19, "locore_exl_19", SF_IGN },
	{ elocore_exl_19, "elocore_exl_19", SF_IGN },
	{ locore_exl_20, "locore_exl_20", SF_IGN },
	{ elocore_exl_20, "elocore_exl_20", SF_IGN },
	{ locore_exl_21, "locore_exl_21", SF_IGN },
	{ elocore_exl_21, "elocore_exl_21", SF_IGN },
	{ locore_exl_22, "locore_exl_22", SF_IGN },
	{ elocore_exl_22, "elocore_exl_22", SF_IGN },
	{ locore_exl_23, "locore_exl_23", SF_IGN },
	{ elocore_exl_23, "elocore_exl_23", SF_IGN },
	{ locore_exl_24, "locore_exl_24", SF_IGN },
	{ elocore_exl_24, "elocore_exl_24", SF_IGN },
	{ locore_exl_25, "locore_exl_25", SF_IGN },
	{ elocore_exl_25, "elocore_exl_25", SF_IGN },
#endif /* R4000 && (! _NO_R4000) */
#if MCCHIP
	{ (int (*)())__cache_wb_inval, "__cache_wb_inval", SF_LEAF },
	{ (int (*)())__dcache_wb_inval, "__dcache_wb_inval", SF_LEAF },
	{ (int (*)())__dcache_wb, "__dcache_wb", SF_LEAF },
#if IP26 || IP28
	{ (int (*)())ip26_enable_ucmem, "ip26_enable_ucmem", SF_LEAF },
	{ (int (*)())ip26_disable_ucmem, "ip26_disable_ucmem", SF_LEAF },
	{ (int (*)())ip26_return_ucmem, "ip26_return_ucmem", SF_LEAF },
#ifdef IP28
	{ (int (*)())__icache_inval, "__icache_inval", SF_LEAF },
	{ (int (*)())__dcache_inval, "__dcache_inval", SF_LEAF },
#endif
#endif
#endif
	{ 0, "0", 0 },
};

static inst_t *txtsrch(inst_t *, unsigned, unsigned, int *, enum sign, enum sign,
			int *, inst_t *, struct spec_funcs **, enum direction,
			unsigned, unsigned);
static unsigned *branch_target(inst_t, inst_t *);
static int is_jal(inst_t);
static inst_t fetch_text(inst_t *);
static int ilastframe(k_machreg_t *);

int ubtdebug = 0;

/*
 *	Search for name, return kernel address
 *	return 0 if can't find name
 *	assumed table is sorted in ascending kernel addr
 */
__psunsigned_t
fetch_kaddr(char *s)
{
	register struct dbstbl *dbptr, *dbend;
	register char *targetname, *curname;
	__psunsigned_t address;
	void idbg_maddr(__psint_t *, char *);

	if (symmax != 0) {
		dbptr = &dbstab[0];
		dbend = &dbstab[symmax-1];

		for ( ; dbptr <= dbend; dbptr++ ) {
			if (dbptr->addr == 0)
				return (0);
			targetname = s;
			curname = (char *)&nametab[dbptr->noffst];
			while( *targetname == *curname ) {
				if (*targetname == 0)
					return (dbptr->addr);
				targetname++;
				curname++;
			}
		}
	} else {
		idbg_maddr((__psint_t *)&address, s);
		return (address);
	}

	return(0);
}

/*
 *	Search for kernel address in name table created by setsym,
 *	and return symbol name.  Return NULL if can't find in table.
 */
static char kname[64];

char *
lookup_kname(register void *addr, register void *offst)
{
	int indx;
	__psunsigned_t *off = offst;
	void idbg_mname(__psunsigned_t *, char *);

	indx = ROUNDPC(addr, 0);
	if (indx == -1) {
		*((__psunsigned_t *)offst) = (__psunsigned_t)addr;
		idbg_mname((__psunsigned_t *)offst, (char *)kname);
		if (kname[0])
			return (kname);
		else
			return (NULL);
	} else {
		*off = (__psunsigned_t)addr - dbstab[indx].addr;
		if (*off > 0x2000)
			return (NULL);
		return((char *)&nametab[dbstab[indx].noffst]);
	}
}

/*
 * Search for kernel address in name table created by setsym,
 * and return a corresponding string.  Returns the string "NULL"
 * if not found.
 */
char *
fetch_kname(register void *addr, register void *offst)
{
	static char *null = "NULL";
	char *name;

	name = lookup_kname(addr, offst);
	return(name ? name : null);
}

/*
 * _prsymoff - Translate a kernel address to a "symbol name + offset"
 * string and output this string. If there is no valid translation, just
 * print the hex address.
 *
 * pre and post are arbitrary character strings which are printed before 
 * and after the translated address.
 */
void
_prsymoff(register void *addr, char *pre, char *post)
{
	register char *nptr;
	__psunsigned_t offst;

	if (pre)
		output("%s", pre);

	if (nptr = lookup_kname(addr, &offst)) {
		if (offst)
			output("%s+0x%x", nptr, offst);
		else
			output("%s", nptr);
	} else
		output("0x%x", addr);

	if (post)
		output("%s", post);
}

unsigned *prev_spinc[100];	/* addr of  previous (d)addiu sp */
unsigned *prev_pc[100];		/* previous pc */
unsigned *prev_sp[100];		/* previous sp */

void
idbg_btrace(int maxfr)
{
	btrace((uint_t *)(((char *)&idbg_btrace)+0x20), getsp(), 0,
			maxfr, (uint *)UT_VEC, curthreadp, 1);
}
	
#define REG_A0	4
#define REG_SP	29

static void
btrace(
	unsigned	*pc,		/* current pc */
	unsigned	*sp,		/* current sp */
	unsigned	*lra,		/* if starting	at LEAF	use this ra */
	int		maxfr,		/* max # frames */
	unsigned	*lbound,	/* lower bound of pc */
	kthread_t	*kt,		/* kthread */
	int		myself		/* true if backtracing myself */
	)
{
	unsigned	*ra;			/* return address */
	unsigned	*spinc;			/* addr of  (d)add(i)u sp */
	unsigned	*rast;			/* addr of  sw/sd ra */
	unsigned	*liat;			/* addr of li at */
	unsigned	*routine_start;		/* addr of sp adjustment */
	unsigned	*tmp, *tmp2, *tmp3;
	unsigned	*argp;
	int		frsize;			/* frame size */
	int		raoff;			/* ra stack offset */
	int		argoff, argoff2;	/* arg stack offset */
	struct spec_funcs *sf;		/* ptr to spec funcs */
	eframe_t	*savea0;	/* save all a0's incase an exception */
	eframe_t	*savea2;
	int		i, j;
	int		first = 1;
	int		prevexc = 0;	/* previous frame was an exception */
	int		which, which2;
	__psunsigned_t	xx;
	union mips_instruction inst, inst2;
	inst_t		instruction;
	int		reg, previ;

#ifdef lint
	savea0 = savea2 = 0;
#endif
	if ((__psunsigned_t)pc & 0x3 || (__psunsigned_t)sp & 0x3) {
		output("pc:0x%x sp:0x%x must be word aligned\n", pc, sp);
		return;
	}
	if (!myself)
		sp = convsp(sp, physspbase);
	if (ubtdebug)
		output("initial pc:0x%x sp:0x%x ra:0x%x\n", pc, sp, lra);
	if (!myself)
		output("SP\t\tFROM\t\t\tFUNC()\n");

	/* 
	 * Handle "li at; addu sp,at" preamble with pc=first instruction
	 * of the routine.  Just bump the PC forward by one for the trace.
	 */
	instruction = fetch_text(pc);
	if ((instruction & 0xffff0000) == LI_MASK) {
		instruction = fetch_text(pc+1);
		if ((instruction & 0xffffffff) == ADDSP_MASK)
			pc++;
	}

	previ = 0;
	while (maxfr--) {

		if (!IS_KSEG0(pc) && !IS_KSEG1(pc) && !IS_KSEG2(pc))
			return;

		/* stack frame */
		if (!myself)
			output("0x%x",sp);
		else
			output("\t");

		/* backward search for sp inc */
		spinc = txtsrch(pc, ADDISP_MASK, ADDSP_MASK, &which, NEGATIVE,
				NOSIGN, &frsize, lbound, &sf, BACKWARD,
				0xffff0000, 0xffffffff);

		if ((which == 0) && !sf) {	/* unsuccessful search */
			output("\nCan't find [D]ADD[I]U from 0x%x\n", pc);
			return;
		}

		if (which && ubtdebug)
			output("\nfound [D]ADD[I]SP_MASK at 0x%x\n", spinc);

		if (which == 1) {		/* Add Immediate */
			if (ubtdebug)
				output("\nfound ADDISP_MASK at 0x%x\n", spinc);

			routine_start = spinc;

		} else if (which == 2) {	/* Add from AT register */
			if (ubtdebug)
				output("\nfound ADDSP_MASK at 0x%x\n", spinc);

			/* backward search for li at */
			liat = txtsrch(spinc, LI_MASK, LI_MASK, &which, LISIGN,
				       LISIGN, &frsize, lbound, NULL, BACKWARD,
				       0xffff0000, 0xffff0000);
			if (which == 0) {
				output("\nCan't find LIAT from 0x%x\n", spinc);
				return;
			} 
			if (ubtdebug)
				output("\nfound LIAT_MASK at 0x%x\n", liat);

			routine_start = liat;
		} else
			routine_start = spinc; /* exception frame */

		/* If we're at an exception frame - then use it to compute
		 * the ra, else keep searching
		 */
		if (sf && sf->type == SF_EXC) {
			/* use previous funcs a0 as an exception frame */

			/* for kernel preemption - special hack */
			if (sf->func == (int (*) ())kpreemption)
				savea0 = (eframe_t *)sp;

			/*
			 * In an optimized kernel, we may not have been able
			 * to find where the exception pointer was saved (in
			 * the previous frame), so we should stop the trace
			 * gracefully.
			 */
			if (savea0 == NULL) {
				if (ubtdebug) printf("exit because can't find "
						     "exception pointer\n");
				output("\n");
				return;
			}

			/*
			 * utlbmiss is special - doesn't have a eframe
			 * this is really where we'll return to not the
			 * 'calling addr' - so we fudge the -2 below
			 */
			if (sf->func == (int (*) ())UT_VEC) {
				xx = savea0->ef_k1;
			} else {
				if (!myself)
					savea0 = (eframe_t *)
						       convsp((unsigned*)savea0,
							      physspbase);
				xx = savea0->ef_epc;
			}
			ra = ((unsigned*)xx) + 2;	/* fudge ra-2 below */
			if (ubtdebug)
				output("Using efptr:0x%x ra:0x%x\n", savea0,ra);

		} else if (sf && sf->type == SF_LEAF) {
			/* we just returned from a LEAF
			 * If this is first time - use lra is possible.
			 * if came from an exception -  ra is (eframe_t*)->ef_ra
			 * Otherwise a special hack for asm panic
			 */
			if (first && lra != 0) {
				if (ubtdebug)
					output("Using LEAF first-ra:0x%x\n",
						lra);
				ra = lra;
			} else if (prevexc) {
				if (ubtdebug)
					output("Using LEAF exp-ra:0x%x\n", 
							(int)savea0->ef_ra);
				ra = (unsigned *)savea0->ef_ra;
			} else {
				/* if from an asm PANIC (i.e. cmn_err)
				 * then by convention a2 has ra!
				 */
				/* but how to check for cmn_err?? */
				if (ubtdebug)
					output("Using LEAF savea2:0x%x\n", savea2);
				ra = (unsigned*)savea2;
			}

		} else {
			/* search for store of ra instruction
			 * If we can't find it OR current pc is at the
			 * store ra instruction:
			 * 1) use passed ra (first function only)
			 * 2) if previous frame an exception then use exptr
			 */
			rast = txtsrch(spinc, STRA_MASK, STRA2_MASK, &which,
					POSITIVE, POSITIVE, &raoff, pc, 0,
					FORWARD, 0xffff0000, 0xffff0000);
			if ((rast == (unsigned *)-1L) || (rast == pc)) {
				if (first && lra != 0) {
					ra = lra;
				} else if (prevexc) {
					xx = savea0->ef_ra;
					ra = (unsigned*)xx;
				} else {
					output("\nCan't find STRA from 0x%x\n",
					       pc);
					return;
				}
				if (ubtdebug)
					output("Using calc ra:0x%x\n", ra);
			} else {
				__psunsigned_t *raaddr;
				raaddr = (__psunsigned_t *)((char *)sp + raoff);
#if (_MIPS_SIM == _ABIN32)
				/* N32 stores RA as 64 bits [except for one 
				 * buggy case (#460589) that will be fixed in
				 * the next rev of the compilers after 7.2].
				 * So just make sure it's a sane value, and
				 * then skip over the first word.
				 */
				if (which == 1) {
				    if (*raaddr != 0xffffffff && 
					*(raaddr+1) & (__psunsigned_t)1<<31)
					output("\nImproperly sign-extended RA "
					       "at:0x%x\n", raaddr);
				    raaddr++;
				}
#endif
				ra = (unsigned *)*raaddr;
				if (ubtdebug)
					output("ra:0x%x sp+raoff:0x%x\n",
						ra, raaddr);
			}

			if (ra == 0) {
				output("\n");
				return;
			}

			if (!is_jal(fetch_text(ra-2))) {
				output("\nRa:0x%x doesn't follow a jal?:0x%x\n",
					ra, fetch_text(ra-2));
				return;
			}

			/* if possible check real function start */
			if (ubtdebug && (tmp = branch_target(fetch_text(ra-2),
							     ra-2)) != 0) {
				if (tmp != routine_start)
					output("\njumped to:0x%x before "
					       "calling:0x%x\n\t", tmp, spinc);
			}
		}
		first = 0;

		/* called from */
		_prsymoff(ra-2, "\t", " ");
		/* func() */
		_prsymoff(routine_start, "\t", "(");

		if (sf && sf->type == SF_EXC) {
			/* these guys don't have args */
			;
		} else if (sf && sf->type == SF_LEAF) {
			/* can we get these guys args? */
			;
		} else {
			savea0 = savea2 = 0;
			/* try to find args - at least up to the 1st four */
			for (i = 0; i < NARGS; i++) {
				argp = sp;
				tmp = txtsrch(spinc, arg_inst[i], arg_inst2[i],
						&which, POSITIVE, POSITIVE, &argoff, pc,
						0, FORWARD, 0xffff0000, 0xffff0000);
				if (tmp == (unsigned *)-1L) {
					if (ubtdebug) {
						_prsymoff((void *)spinc, "Can't find STA? from ", " to ");
						_prsymoff((void *)pc, NULL, "\n");
					}
					if (previ == 0)
						break;
				}
				/*
				 * optimized ragnarok code moves a regs
				 * into s regs after saving s regs on
				 * the stack. we have to get tricky...
				 * first look for a "move s?, a?" which
				 * is really "or s?, a?, zero" or
				 * "or s?, zero, a?". It's a good thing
				 * this code doesn't have to be fast. We
				 * have to do this as well as the above
				 * search and then pick the first one.
				 */
				if (previ == 0)
					goto argfound;
				inst.word = 0;
				inst.r_format.opcode = spec_op;
				inst.r_format.func = or_op;
				inst.r_format.rs = REG_A0 + i;
				inst.r_format.rt = 0;
				if (ubtdebug)
					output("Looking for reg mv %x\n", inst.word);
				tmp2 = txtsrch(spinc, inst.word, inst.word,
						&which2, NOSIGN, NOSIGN, &argoff2, pc,
						0, FORWARD, 0xffff07ff, 0xffff07ff);
				if (tmp2 == (unsigned *)-1L) {
					inst.r_format.rs = 0;
					inst.r_format.rt = REG_A0 + i;
					if (ubtdebug)
						output("Looking for reg mv %x\n", inst.word);
					tmp2 = txtsrch(spinc, inst.word,
						inst.word, &which2, NOSIGN, NOSIGN,
						&argoff2, pc, 0, FORWARD,
						0xffff07ff, 0xffff07ff);
						if (tmp2 == (unsigned *)-1L) {
							if (ubtdebug) {
								_prsymoff((void *)spinc, "Can't find reg mv from ", " to ");
								_prsymoff((void *)pc, NULL, "\n");
							}
							if (tmp == (unsigned *)-1L)
								break;
						}
				}
				if ((tmp == (unsigned *)-1L) || (tmp2 < tmp)) {
					tmp = tmp2;
					inst.word = *tmp;
					reg = inst.r_format.rd;	/* found it */
					/*
					 * Now look for sd reg, x(sp) or
					 * sw reg, x(sp). First look in the
					 * current routine (to catch cases
					 * like 'move sx, ax; sd sx, x(sp)'
					 * and then in the next routine
					 * on the stack.
					 */
					inst.word = 0;
					inst.i_format.opcode = sw_op;
					inst.i_format.rt = reg;
					inst.i_format.rs = REG_SP;
					inst2.word = inst.word;
					inst2.i_format.opcode = sd_op;
					if (ubtdebug) {
						_prsymoff((void *)spinc, "Looking for reg store from ", " to ");
						_prsymoff((void *)pc, NULL, " in current routine\n");
					}
					tmp = txtsrch(spinc, inst.word,
							inst2.word, &which,
							POSITIVE, POSITIVE, &argoff, pc,
							0, FORWARD, 0xffff0000, 0xffff0000);
					if (tmp != (unsigned *)-1L) {
						union mips_instruction i1, i2;

						/*
						 * found one, make sure this
						 * register still contained
						 * the argument when we did
						 * the store (look for other
						 * load or move last reg mv
						 * and this store. We could
						 * easily get fooled if some
						 * other instruction is used
						 * to modify the register...
						 */
						i1.word = 0;
						i1.i_format.opcode = lw_op;
						i1.i_format.rt = reg;
						i1.i_format.rs = REG_SP;
						i2.word = i1.word;
						i2.i_format.opcode = ld_op;
						tmp3 = txtsrch(tmp2, i1.word,
							i2.word, &which2,
							POSITIVE, POSITIVE, &argoff2, tmp,
							0, FORWARD, 0xffff0000, 0xffff0000);
						if (tmp3 != (unsigned *)-1L)
							goto subseq_rtn;
						i1.i_format.opcode = lb_op;
						i2.i_format.opcode = lh_op;
						tmp3 = txtsrch(tmp2, i1.word,
							i2.word, &which2,
							POSITIVE, POSITIVE, &argoff2, tmp,
							0, FORWARD, 0xffff0000, 0xffff0000);
						if (tmp3 != (unsigned *)-1L)
							goto subseq_rtn;
						i1.word = 0;
						i1.r_format.opcode = spec_op;
						i1.r_format.func = or_op;
						i1.r_format.rd = reg;
						tmp3 = txtsrch(tmp2, i1.word,
							i1.word, &which2,
							POSITIVE, POSITIVE, &argoff2, tmp,
							0, FORWARD, 0xfc00ffff, 0xfc00ffff);
						if (tmp2 != (unsigned *)-1L)
							goto subseq_rtn;
						if (ubtdebug)
							_prsymoff((void *)tmp2, "Found secondary store at ", "\n");
						goto argfound;
					}
subseq_rtn:
					for (j = previ - 1; j >= 0; j--) {
						if (ubtdebug) {
							_prsymoff((void *)prev_spinc[j], "Looking for reg store from ", " to ");
							_prsymoff((void *)prev_pc[j], NULL, "\n");
						}
						tmp = txtsrch(prev_spinc[j],
							inst.word, inst2.word,
							&which, POSITIVE, POSITIVE,
							&argoff, prev_pc[j],
							0, FORWARD, 0xffff0000, 0xffff0000);
						if (tmp != (unsigned *)-1L)
							break;
					}
					if (j < 0) {
						if (ubtdebug)
							output("Can't find reg store\n");
						break;
					}
					if (!myself)
						argp = convsp(prev_sp[j], physspbase);
					else
						argp = prev_sp[j];
				}
argfound:
				if (ubtdebug)
					output("which %x sp %x ap %x aoff %x\n",
						which, sp, argp, argoff);

				if (which == 1) {
					if (i == 0)
						savea0 = (eframe_t*)(__psunsigned_t)(*((__uint32_t *) ((char *)argp + argoff)));
					else if (i == 2)
						savea2 = (eframe_t*)(__psunsigned_t)(*((__uint32_t *) ((char *)argp + argoff)));
					output("0x%x, ",
						*((__uint32_t *) ((char *)argp + argoff)));
				}
				else if (which == 2) {
					if (i == 0)
						savea0 = (eframe_t*)(*((__uint64_t *) ((char *)argp + argoff)));
					else if (i == 2)
						savea2 = (eframe_t*)(*((__uint64_t *) ((char *)argp + argoff)));
					output("0x%x, ",
						*((__uint64_t *) ((char *)argp + argoff)));
				}
			}
		}
		output(")\n");

		/* compute new stack pointer */
		prevexc = 0;
		prev_sp[previ] = sp;
		if (sf && sf->type == SF_EXC) {
			/*
			 * we're at an exception header - with all the new stack
			 * switching there are lots of possibilities
			 * here we handle:
			 * kernel back to kernel (simple - savea0 has ep)
			 * interrupt back to kernel
			 * interrupt back to idle
			 */
			if (ilastframe((k_machreg_t *)sp)) {
				/*
				 * getting off of istack - use either
				 * k_eframe or private.idlstkdepth
				 * as new ep
				 */
				if (private.p_idlstkdepth) {
					if (private.p_intr_resumeidle) {
						output("\t\t\t\tIDLE (will restart resumeidle())\n");
						return;
					}
					savea0 = (eframe_t*)(
						private.p_intlastframe);
				} else {
					savea0 = (eframe_t*)kt->k_eframe;
				}
				if (savea0 == NULL) {
					output("depth is NULL!\n");
					return;
				}
			}
			/* eframe may now be in upage so may need converting */
			if (!myself)
				savea0 = (eframe_t*)convsp((unsigned*)savea0, physspbase);
			xx = savea0->ef_sp;
			sp = (unsigned*)xx;
			if (!myself)
				sp = convsp(sp, physspbase);
			prevexc = 1;
			/* super kludge - undo ra += 2 now that we're past
			 * printing
			 */
			ra -= 2;
		} else if (sf && sf->type == SF_LEAF) {
			/* stack stays the same */
			;
		} else {
			/* if we're at the instruction(s) that adjust the stack
			 * then don't touch sp since that instruction hasn't 
			 * yet been executed!
			 */
			if (pc > spinc)
				/* With mongoose it can be both positive and negative */
				sp = (uint *)((char *)sp + ((frsize > 0)? frsize : -frsize));
		}
		prev_pc[previ] = pc;
		prev_spinc[previ] = spinc;	/* remember the last routine */
		previ++;
		pc = ra;
		if (ubtdebug)
			output("New sp:0x%x pc:0x%x\n", sp, pc);

		/* If we happend to be at the end of the kernel interrupt
		 * stack AND we interrupted the idleloop, then we need to
		 * setup the correct value of "savea0" the exception .
		 */
		if (ilastframe((k_machreg_t *)sp)) {
			if (ubtdebug)
				output("at top of interrupt stack: 0x%x\n",sp);
			if (private.p_idlstkdepth) {
				savea0 = (eframe_t*)(private.p_intlastframe);
				if (ubtdebug) {
					output("switch savea0 0x%x\n",savea0);
				}
			} else {
				savea0 = (eframe_t*)kt->k_eframe;
				if (ubtdebug) {
					output("kthread switch savea0 0x%x\n",savea0);
				}
			}
		}
	}
	if (!myself)
		output("--More--\n");
	return;
}

/*
 * txtsrch - search for an instruction
 *
 * Reasons that txtsrch terminates:
 * 	found instruction that matches inst1mask/check1mask/im1sign
 * 	found instruction that matches inst2mask/check2mask/im2sign
 *	search took us into a "special function"
 *	we looked at a gazillion of instructions
 *
 * Returns:
 * != -1 on success == pc of found instruction
 * -1 of failure
 */
static inst_t *
txtsrch(
	inst_t		*pc,		/* start search point */
	unsigned	inst1mask,	/* instruction to search for */
	unsigned	inst2mask,	/* alt instruction to search for */
	int		*which,		/* which one did we match? */
					/* 0 = none */
					/* 1 = first */
					/* 2 = second */
	enum sign	im1sign,	/* desired sign of immediate value */
	enum sign	im2sign,	/* desired sign of immediate value */
	int		*im_ptr,	/* immediate value in instruction */
	inst_t		*llimit,	/* stop looking here */
	struct spec_funcs **specf,	/* pointer to returned special func */
	enum direction	dir,		/* direction of search */
	unsigned	check1mask,	/* mask instruction with this mask */
	unsigned	check2mask	/* mask instruction with this mask */
	)
{
	struct spec_funcs	*sf;
	int			tries;
	inst_t			instr;
	int			im_val;
	__psint_t		incr	= (dir == BACKWARD) ? -1 : 1;

	if (specf) *specf = 0;	/* no special function match yet */
	*which = 0;		/* no match of inst1 or inst2 yet */

	for (tries = 0; pc != llimit && tries < 10000; pc += incr, tries++) {

		instr = fetch_text(pc);
		im_val = (int) ((short)(instr & 0xffff));
		*which = (instr & check1mask) == inst1mask ? 1 :
			 (instr & check2mask) == inst2mask ? 2 : 0;

		if (*which) {	/* found one or the other */

			int desired_sign = (*which == 1) ? im1sign : im2sign;

			if ((desired_sign == POSITIVE && im_val < 0) ||
			    (desired_sign == NEGATIVE && im_val > 0))
				continue;
			
			*im_ptr = im_val;

			if (ubtdebug) {
				output("found %x at %x ", instr, pc);
				_prsymoff((void *)pc, "(", ") ");
				output("[called from %x]\n", __return_address);
			}
		}

		/* see if we're at a special function */
		for (sf = sfuncs; sf->func; sf++) {

			if (pc == (inst_t *) sf->func) {
				if (ubtdebug)
					output("found specf:%s\n", sf->name);

				if (sf->type == SF_IGN) {	/* ignore */
					*which = 0;
					break;
				}

				/* found interesting special function */
				if (specf) *specf = sf;
				return pc;
			}
		}

		if (*which)	/* found, not in special function */
			return pc;
	}

	return((inst_t *)-1L);
}

static void
leafinit(void)
{
#ifdef MP
	inst_t			spinc;
	struct spec_funcs	*sf;
	int			i;

	/* see if mutex_spinlock is a LEAF function or not */
	spinc = fetch_text(((inst_t *)mutex_spinlock));
	if (((spinc & 0xffff0000) == ADDISP_MASK) || (spinc == ADDSP_MASK)) {
		/* NOT a leaf - change the LEAF table */
		qprintf("locking routines not LEAF\n");
		for (sf = sfuncs; sf->func; sf++) {
			if (mutex_spinlock == sf->func) {
				for (i = 0; i < 16; i++, sf++)
					sf->func = (int (*)())-1L;
				break;
			}
		}
	}
#endif
}

/*
 * is_jal -- determine if instruction is jump and link
 */
static int
is_jal(inst_t inst)
{
	union mips_instruction i;

	i.word = inst;
	switch (i.j_format.opcode) {
	case spec_op:
		switch (i.r_format.func) {
		case jalr_op:
			return(1);
		}
		return(0);

	case jal_op:
		return(1);
	}
	return(0);
}

/*
 * branch_target -- calculate branch target
 */
static inst_t *
branch_target(inst_t inst, inst_t *pc)
{
	union mips_instruction i;

	i.word = inst;
	switch 	(i.j_format.opcode) {
	case spec_op:
		switch (i.r_format.func) {
		case jr_op:
		case jalr_op:
			return NULL;
		}
		break;

	case j_op:
	case jal_op:
		return((inst_t *) (((__psint_t)(pc+1)&~((1<<28)-1))
					| (i.j_format.target<<2)) );
	}
	return NULL;
}

/*
 * fetch_text - get an instruction from text space
 */
static inst_t
fetch_text(inst_t *pc)
{
	register struct brkpt_table *bt;
	inst_t inst;

	if (brkpt_table == NULL)
		return(*pc);

	for (bt = brkpt_table; bt < &brkpt_table[NBREAKPOINTS]; bt++)
		if (bt->bt_type != BTTYPE_EMPTY && bt->bt_addr == (__psunsigned_t)pc)
			return(bt->bt_inst);

	inst = *pc;
	return(inst);
}

/*
 * ilastframe - is sp at end of interrupt stack
 */
static int
ilastframe(register k_machreg_t *sp)
{
	if (sp == private.p_intlastframe)	/* more later */
		return(1);
	return(0);
}

void
idbg_symtab(
	struct dbstbl **db,
	char **name)
{
	*db = &dbstab[0];
	*name = &nametab[0];
}

void
idbg_symsize(int *size )
{
	*size = symmax;
}

#ifdef IDBG_WD93

char *gms_flag_name[] = {
	"initialized",
	"winmgr",
	"waitack",
	"response",
	"dead",
	"dma_inuse",
	"dma_wait",
	"dma_want",
	"dma_err",
	"diag_busy",
};

char *gms_pflag_name[] = {
	"pipe_host",
	"pipe_switching",
	"pipe_switched",
};

char *cx_flag_name[] = {
	"swap_pending",
	"picking",
	"feedback",
	"unload",
	"overflow",
	"notake",
};

static char *dflnames[] = {
	"promsync",
	"mapret",
	"only",
	"93B",
	"93A",
	"93",
	"busy"
};

static char *dsyncnames[] = {
	"40",
	"20",
	"nosync",
	"targsync",
	"cantsync",
	"resync",
	"sync"
};

static char *sr_hanames[] = {
	"40",
	"20",
	"10",
	"8",
	"abort",
	"neg_on",
	"neg_off"
};


static char *sflnames[] = {
	"mapbp",
	"xferin",
	"rqsens",
	"restart",
	"savedp",
	"disconnect",
	"busy"
};

void wd93_debug_subchannel(scsi_request_t *);

/*
++
++	kp scsi flag - Print information about the SCSI interface
++		flag == 0  - print the SCSI chip registers
++		flag == 1  - print controller struct and active subchans
++		flag == 2  - print controller struct and all subchans
++		anything else  - print subchannel at this address
++
*/
void
_idbg_wd93(__psint_t type)
{
	register wd93dev_t	*dp;
	register scsi_request_t	*sp;
	register int		adap, any;
	extern wd93dev_t *wd93dev;
	extern int wd93cnt;

	switch (type) {
	case 0:
		/*
		 * Dump the registers out of the SCSI chip
		 */
		for(adap=0; adap<wd93cnt; adap++) {
			dp = &wd93dev[adap];
			if(dp->d_flags)	/* else doesn't exist, or not initted */
				wd93_debug_chip(dp);
		}
		break;

	case 1:
	case 2:
		/*
		 * Dump the structure associated with controller
		 * given by the argument.
		 */
		for(adap=0; adap<wd93cnt; adap++) {
			dp = &wd93dev[adap];
			if(!dp->d_map || dp->d_map == (struct dmamap *)-1L)
				continue;	/* adapter not present */
			qprintf("adapter %d struct at %x flags: %x ",
				adap, dp,dp->d_flags);
			wd93_debug_pflags(dp->d_flags, dflnames);
			if(!dp->d_flags)	/* doesn't exist, or not initted */
				continue;
			qprintf("\nsleepsem %x, count %d; qlock %x\n",
				&dp->d_sleepsem, dp->d_sleepcount,  &dp->d_qlock);
			qprintf("reglock %x consub=%x active=%x waithead=%x tail=%x\n",
				 dp->d_reglock, dp->d_consubchan, dp->d_active,
				 dp->d_waithead, dp->d_waittail);
			qprintf("addr %x data %x\n",
				dp->d_addr, dp->d_data);
			{
			  int t,l,nl=0;
			  for(t=0; t<SC_MAXTARG; t++)
			    for(l=0; l<SC_MAXLU; l++)
			      if(dp->d_refcnt[t][l] || (dp->d_unitflags[t][l] &&
				   dp->d_unitflags[t][l] != U_CHMAP)) {
			    qprintf("%d,%d syncr=%x f=%x, uflgs=%x, ref=%x",
					t,l, dp->d_syncreg[t],
					dp->d_syncflags[t],
					dp->d_unitflags[t][l],
					dp->d_refcnt[t][l]);
				qprintf(!(++nl%2)?"\n":";   ");
			      }
			  if (nl%2) qprintf("\n");
			}

			any = 0;
			if((sp=dp->d_consubchan) != NULL) {
				qprintf("Connected subchannel:");
				qprintf("%d,%d  ", sp->sr_target, sp->sr_lun);
				any = 1;
			}
			if(dp->d_active) {
			    int hding = 0;
			    for(sp = dp->d_active; sp; sp = ((wd93_ha_t *)sp->sr_ha)->wd_link)
				if(sp != dp->d_consubchan) {
				    if(!hding) { 
					qprintf("active channels: ");
				    	hding++;
				    }
				    qprintf("%d,%d  ", sp->sr_target, sp->sr_lun);
				    any |= 2;
				}
			}

			if(dp->d_waithead) {
				int hding = 0;
				for(sp = dp->d_waithead; sp; sp = ((wd93_ha_t *)sp->sr_ha)->wd_link) {
				    if(!hding) { 
					qprintf("queued channels: ");
				    	hding++;
				    }
				    qprintf("%d,%d  ", sp->sr_target, sp->sr_lun);
				    any |= 4;
				}
			}
			if(any)
				qprintf("\n");

			if(sp=dp->d_consubchan) {
					qprintf("Connected subchannel:\n");
					wd93_debug_subchannel(sp);
			}
			if(type == 2) {
				if(any & 2) {
				qprintf("Active subchannels:\n");
				for(sp=dp->d_active; sp; sp= ((wd93_ha_t *)sp->sr_ha)->wd_link)
					if(sp != dp->d_consubchan)
						wd93_debug_subchannel(sp);
				}
				if(any & 4) {
				qprintf("Queued subchannels:\n");
				for(sp=dp->d_waithead; sp; sp= ((wd93_ha_t *)sp->sr_ha)->wd_link)
					if(sp != dp->d_consubchan)
						wd93_debug_subchannel(sp);
				}
			}
		}
		break;

	case 3:	/* print list of allocated channels; it is the most
		recent request sense information for each channel */
		for(adap=0; adap<wd93cnt; adap++) {
			if(!wd93dev[adap].d_flags)
				continue;	/* ignore missing adapters */
			qprintf("sc%d: Allocated subchannels (most recent reqsense):\n", adap);/*
			for(sp=wd93dev[adap].d_allocated; sp; ((wd93_ha_t *)sp->sr_ha)->wd_alloc) {
				if(((scsi_spare_t *)sp->sr_dev)->s_refcnt>1)
					qprintf("alloc %d times ", ((scsi_spare_t *)sp->sr_dev)->s_refcnt);
				wd93_debug_subchannel(sp);
			}*/
		}
		break;

	default:
		/*
		 * Dump the subchannel request structure at the given address
		 */
		wd93_debug_subchannel((scsi_request_t *)type);
		break;
	}
}

void
wd93_debug_subchannel(scsi_request_t *sp)
{
	extern wd93dev_t *wd93dev;
	unsigned char	*tp;
	wd93dev_t *dp = &wd93dev[sp->sr_ctlr];
	wd93_ha_t	*ssp;

	qprintf(" SR (%d,%d,%d) @ %x ",
		sp->sr_ctlr, sp->sr_target, sp->sr_lun, sp);

	ssp = (wd93_ha_t *)sp->sr_ha;
	if(!ssp) {
		static wd93_ha_t s;
		ssp = &s;
		qprintf("[sr_ha NULL!]");
	}
	else qprintf("sr_ha=%x", ssp);

	qprintf("; flags: ");
	wd93_debug_pflags(ssp->wd_flags, sflnames);
	wd93_debug_pflags(dp->d_syncflags[sp->sr_target], dsyncnames);
	qprintf("srha_fl: ");
	wd93_debug_pflags(sp->sr_ha_flags, sr_hanames);
	qprintf("\n  notify=%x cmdstat=%d scstat=%d syncreg=%x dir=%d map=%x\n",
		sp->sr_notify, sp->sr_status, sp->sr_scsi_status,
		dp->d_syncreg[sp->sr_target],
		(sp->sr_flags&SRF_DIR_IN), ssp->wd_map);
	qprintf("  tval=%d timeid=%d retry=%d link=%x nextha %x\n",
		sp->sr_timeout, ssp->wd_timeid, ssp->wd_selretry,
		ssp->wd_link, ssp->wd_nextha);
	qprintf("  sensdat=%x dstid=%x xferaddr=%x xlen=%x reqlen=%x, %x remain\n",
		sp->sr_sense, ssp->wd_dstid, sp->sr_buffer, ssp->wd_xferlen,
		sp->sr_buflen, ssp->wd_bcnt);
	if ((tp = (unsigned char *) sp->sr_command) != 0) {
		unsigned cnt = sp->sr_cmdlen;
		if(cnt>16) {
			qprintf("bad cmdlen=%d ", cnt);
			cnt=16;
		}
		qprintf("  cmd [");
		while(cnt-->0)
			qprintf("%x ", *tp++);
		qprintf("] len %d data_len %x\n", sp->sr_cmdlen,sp->sr_buflen);
	}
}

void
wd93_debug_pflags(int flg, char **names)
{
	int mask;
	int i;


	if (flg == 0) {
		qprintf(" <none>");
		return;
	}
	mask =	0x40;
	if(flg	> mask)
		qprintf(" %x", flg & ~(mask-1));
	for (i = 0; i < 7; i++, mask >>= 1)
		if (mask & flg)
			qprintf(" %s", names[i]);
}

static struct {
	char		*rname;
	int		rnum;
} reglist[] = {	/* rearranged to be in more usable order */
	"Phas",	16,
	"Stat",		23,
	"Data",		25,
	"C1",		3,
	"C2",		4,
	"C3",		5,
	"C4",		6,
	"C5",		7,
	"C6",		8,
	"C7",		9,
	"C8",		10,
	"C9",		11,
	"C10",		12,
	"C11",		13,
	"C12",		14,
	"CntHi",	18,
	"Md",	19,
	"Lo",	20,
	"ID",		0,
	"Ctrl",		1,
	"Tout",		2,
	"Sync",		17,
	"Dest",	21,
	"Src",	22,
	"Lun",		15,
	"Cmd",		24,
 	"Qtag",		26,
	0,		0,
};

#define getreg93(p,r) (*(volatile char *)(p)->d_addr=r, *(volatile char *)(p)->d_data)
#define getauxstat(p) (*(volatile char *)(p)->d_addr)

/* note that when we read the status register, we might clear
   a pending interrupt, but that's life.... */

void
wd93_debug_chip(wd93dev_t *dp)
{
	int	i;
	u_char astat;

	qprintf("WD93%s #%d @ %x",
 		dp->d_flags&D_WD93A ? "A" : (dp->d_flags&D_WD93B ? "B" : ""),
 		dp->d_adap, dp->d_addr);
 	qprintf("  AUX=%x ", astat=getauxstat(dp));
	if(astat & AUX_BSY) {	/* only aux, cmd and data */
		qprintf("BUSY; %s %x ", reglist[WD93CMD].rname,
			getreg93(dp, reglist[WD93CMD].rnum));
		qprintf("%s %x ", reglist[WD93DATA].rname,
			getreg93(dp, reglist[WD93DATA].rnum));
		qprintf("\n");
		return;
	}
	for(i=0; i<3; i++)
		qprintf("%s=%x ", reglist[i].rname,
			getreg93(dp, reglist[i].rnum));
	qprintf("\n");
	for(; reglist[i].rname; i++) {
		if(!strcmp(reglist[i].rname, "Qtag") &&
			!(dp->d_flags&D_WD93B))
			continue;	/* 93B only register */
		qprintf("%s=%x ", reglist[i].rname,
			getreg93(dp, reglist[i].rnum));
		if(!strcmp(reglist[i].rname, "C12"))
		    qprintf("\n"); /* get 12 cmd bytes on 1st line */
	}
	qprintf("\n");
}
#endif  /* IDBG_WD93 */

/*
++	kp scsi addr - print scsi request data
*/
void
idbg_scsi(struct scsi_request *req)
{
	int		i;

	qprintf("SCSI request @ 0x%x\n", req);
	if (req == NULL)
		return;
	qprintf("lun_vhdl %d, ctlr/targ/lun/tag %d/%d/%d/%d\n", 
		req->sr_lun_vhdl, 
		req->sr_ctlr, req->sr_target, req->sr_lun, req->sr_tag);
	qprintf("command:");
	if (req == NULL)
		qprintf(" NULL");
	else
		for (i = 0; i < req->sr_cmdlen; i++)
			qprintf(" 0x%x", req->sr_command[i]);
	qprintf("\nflags 0x%x, timeout %d HZ (%d seconds)\n", req->sr_flags,
		req->sr_timeout, req->sr_timeout/HZ);
	qprintf("buffer 0x%x, buflen %d (0x%x), sense 0x%x, senselen %d\n",
	        req->sr_buffer, req->sr_buflen, req->sr_buflen, req->sr_sense,
		req->sr_senselen);
	qprintf("notify 0x%x, bp 0x%x, sr_dev 0x%x, sr_ha 0x%x, sr_spare 0x%x"
		"\n", req->sr_notify, req->sr_bp, req->sr_dev, req->sr_ha,
		req->sr_spare);
	qprintf("status %d, scsi status 0x%x, ha_flags 0x%x, sensegotten %d, "
		"resid %d (0x%x)\n", req->sr_status, req->sr_scsi_status,
		req->sr_ha_flags, req->sr_sensegotten, req->sr_resid);
}

/*
 * IDBG routine used to implement the kp FOI command. Dumps the
 * contents of a FO instance structure.
 */
void
idbg_foi(vertex_hdl_t lun_vhdl)
{
  struct scsi_candidate   *foc;
  struct scsi_fo_instance *foi;
  int                      i;

  if ((int)lun_vhdl > 0) {
    vertex_hdl_t     tmp_vhdl;
    int              rc;
    scsi_lun_info_t *lun_info;

    rc = hwgraph_traverse(lun_vhdl, "../../"EDGE_LBL_LUN, &tmp_vhdl);
    if (rc != GRAPH_SUCCESS) {
      qprintf("idbg_foi: vertex %d is not a hwg-lun vertex\n", lun_vhdl);
      return;
    }
    lun_info = scsi_lun_info_get(lun_vhdl);
    if (SLI_FO_INFO(lun_info) == NULL) {
      qprintf("idbg_foi: vertex %d doesn't have failover info associated with it\n", lun_vhdl);
      return;
    }
    foi = SLI_FO_INFO(lun_info);
    foc = foi->foi_foc;
    qprintf("\t\t[%x] Group Name: %s\n", foi, (*foc->scsi_sprintf_func)(foi->foi_grp_name));
    qprintf("\t\t\tPrimary path: %d\n", foi->foi_primary);
    for (i = 0; i < MAX_FO_PATHS; ++i) {
      qprintf("\t\t\t[%d]: Path VHDL: %d, status: %d\n", 
	      i, foi->foi_path[i].foi_path_vhdl, foi->foi_path[i].foi_path_status);
    }
    return;
  }
  else {
    qprintf("idbg_foi: lun_vhdl = %d\n", lun_vhdl);
    for (foc = fo_candidates; foc->scsi_match_str1; ++foc) {
      qprintf("\t Device %s: %s \n", foc->scsi_match_str1, foc->scsi_match_str2);    
      for (foi = foc->scsi_foi; foi; foi = foi->foi_next) {
	qprintf("\t\t[%x] Group Name: %s\n", foi, (*foc->scsi_sprintf_func)(foi->foi_grp_name));
	qprintf("\t\t\tPrimary path: %d\n", foi->foi_primary);
	for (i = 0; i < MAX_FO_PATHS; ++i) {
	  qprintf("\t\t\t[%d]: Path VHDL: %d, status: %d\n", 
		  i, foi->foi_path[i].foi_path_vhdl, foi->foi_path[i].foi_path_status);
	}
      }
    }
  }
}

/*
 * Print out the queue of active requests for the given dk struct.
 */
static void
dkactive(struct dksoftc *dkp)
{
	buf_t	*bp;

	for (bp = dkp->dk_active; bp != NULL; bp = bp->av_forw) {
		qprintf("bp 0x%x sort 0x%x count 0x%x\n",
			bp, bp->b_sort, (long)bp->b_bcount);
	}
}

/*
 * Print out the request queue for the given dk struct.
 */
static void
dkqueue(struct dksoftc *dkp)
{
	buf_t	*bp;

	for (bp = dkp->dk_queue.io_head; bp != NULL; bp = bp->av_forw) {
		qprintf("bp 0x%x sort 0x%x count 0x%x\n",
			bp, bp->b_sort, (long)bp->b_bcount);
	}
}

/*
 * Print out the dksc driver dk struct.
 */
void
idbg_dksoftc(struct dksoftc *dkp)
{
	qprintf("&dk_vh 0x%x &dk_buf 0x%x &dk_wait 0x%x blkstats 0x%x\n",
		&(dkp->dk_vh), &(dkp->dk_buf), &(dkp->dk_wait),
		dkp->dk_blkstats);
	qprintf("subchan 0x%x freereq 0x%x maxq %d curq %d driver %d\n",
		dkp->dk_subchan, dkp->dk_freereq, (int)dkp->dk_maxq,
		(int)dkp->dk_curq, (int)dkp->dk_driver);
	qprintf("iostart 0x%x drivecap 0x%x blksz %d &openparts 0x%x\n",
		dkp->dk_iostart, dkp->dk_drivecap, dkp->dk_blksz,
		&(dkp->dk_openparts));
	qprintf("&lyrcount 0x%x flags 0x%x selflags 0x%x qdepth %d\n",
		&(dkp->dk_lyrcount), (ulong)dkp->dk_flags,
		(ulong)dkp->dk_selflags, (int)dkp->dk_qdepth);
	qprintf("inqtyp 0x%x &sema 0x%x &done 0x%x\n",
		dkp->dk_inqtyp, &(dkp->dk_sema), &(dkp->dk_done));
	qprintf("&queue 0x%x q.head 0x%x tail 0x%x prevblk 0x%x\n",
		&(dkp->dk_queue), dkp->dk_queue.io_head,
		dkp->dk_queue.io_tail, dkp->dk_queue.io_state.prevblk);
	qprintf("seq_count %d next %d\n",
		dkp->dk_queue.seq_count, dkp->dk_queue.seq_next);
	qprintf("dk_lun_vhdl %x dk_disk_vhdl %x\n",
		dkp->dk_lun_vhdl, dkp->dk_disk_vhdl);
	qprintf("active queue:\n");
	dkactive(dkp);
	qprintf("sort queue:\n");
	dkqueue(dkp);
}


#if EVEREST
/*
++	kp scip ctlr - print data structures for scip controller
 */
void
idbg_scip(__psint_t ctlr)
{
#ifndef SABLE
	struct scipctlrinfo		*ci;
	extern struct scipctlrinfo	*scipctlr[SCSI_MAXCTLR];
	int				 i;

	ci = scipctlr[ctlr];
	qprintf("SCIP ctlr %d at 0x%x", ctlr, ci);
	if (ci == NULL)
	{
		qprintf(" not present\n");
		return;
	}
	qprintf(" slice %d,", ci->chan);
	if (ci->quiesce)
		qprintf(" quiescent,");
	if (ci->dead)
		qprintf(" dead,");
	if (ci->reset)
		qprintf(" reset in progress,");
	if (ci->intr)
		qprintf(" intr in progress,");
	qprintf(" %d commands in progress, %d resets done\n",
		ci->cmdcount, ci->reset_num);
	qprintf("low pri queue 0x%x, IQ add index %d, CQ remove ptr 0x%x\n",
	        ci->scb, ci->scb_iqadd, ci->scb_cqremove);
	qprintf("med pri queue 0x%x, high pri scb 0x%x, err scb 0x%x\n",
		ci->mcb, ci->hcb, ci->ecb);
	qprintf("finish info: hi 0x%x med 0x%x lo 0x%x error 0x%x\n",
		ci->finish->high, ci->finish->med, ci->finish->low,
		ci->finish->error);
	qprintf("wait Q head/tail 0x%x/0x%x, free sciprequest 0x%x\n",
		ci->waithead, ci->waittail, ci->request);
	qprintf("intr Q head/tail 0x%x/0x%x, SCIP addr 0x%x\n",
		ci->intrhead, ci->intrtail, ci->chip_addr);
	qprintf("unit info structs:\n");
	for (i = 0; i < 16; i += 4)
		qprintf("0x%x 0x%x 0x%x 0x%x\n", ci->unit[i], ci->unit[i+1],
			ci->unit[i+2], ci->unit[i+3]);
	qprintf("lun 0 info structs:\n");
	for (i = 0; i < 16; i += 4)
		qprintf("0x%x 0x%x 0x%x 0x%x\n", ci->unit[i]->lun[0],
			ci->unit[i+1]->lun[0], ci->unit[i+2]->lun[0],
			ci->unit[i+3]->lun[0]);
#endif /* SABLE */
}
#endif /* EVEREST */


#if defined(SN0)

#if defined(DEBUG)

/* ARGSUSED */
void
idbg_intrmap(__psint_t  x)
{
	cnodeid_t cnodeid = CNODEID_NONE;

	extern void intr_dev_targ_map_print(cnodeid_t cnodeid);


	if (x >= 0)
		cnodeid = x;

	intr_dev_targ_map_print(cnodeid);

}

#endif /* defined(DEBUG) */
#endif /* defined(SN0) */

#if SN || MULTIKERNEL

void
idbg_partition(void)
{
    extern void part_dump(void (*)(char *, ...));
    part_dump(qprintf);
}

void
idbg_xpr(void *xpr)
{
    extern	void	xpc_dump_xpr(char *, void *, void (*)(char *, ...));
    xpc_dump_xpr("DBG", xpr, qprintf);
}

void
idbg_xp(void)
{
    extern	void	xpc_dump_xpe(char *, void (*)(char *, ...));
    xpc_dump_xpe("DBG", qprintf);
}

void
idbg_xpm(__psint_t x)
{
    extern	void	xpc_dump_xpm(char *, void *, void *, 
				     void (*)(char *, ...));
    xpc_dump_xpm("DBG", NULL, (void *)x, qprintf);
}

#endif /* SN || MULTIKERNEL */

void
intvec_tinfo(thd_int_t *tip)
{
		qprintf("  tinfo 0x%x %x flags 0x%x",
			tip, tip->thd_flags);
		if (tip->thd_flags & THD_ISTHREAD)
		    qprintf(" ISTHREAD");
		if (tip->thd_flags & THD_REG)
		    qprintf(" REG");
		if (tip->thd_flags & THD_INIT)
		    qprintf(" INIT");
		if (tip->thd_flags & THD_EXIST)
		    qprintf(" EXIST");
		if (tip->thd_flags & THD_OK)
		    qprintf(" OK");
		if (tip->thd_flags & THD_EXITING)
		    qprintf(" EXITING\n");
		qprintf("\n");
}

#ifdef IP30
#include <sys/RACER/heart_vec.h>

extern void heart_stray_intr(intr_arg_t);

/*
 * Print the heart_ivec table contents.
 */
void
idbg_intvec(__psint_t l)
{
	heart_ivec_t *hvp;
	char *ptr;
	__psunsigned_t offst;

	if (l >= 0 && l < HEART_INT_VECTORS) {
		hvp = &heart_ivec[l];

		if (hvp->hv_func == heart_stray_intr || hvp->hv_func == 0)
			return;

		qprintf("heart_ivec(%d) cpu: %d", l, hvp->hv_dest);

		/* Try to get symbolic name for function */
		if (ptr = lookup_kname((void*)hvp->hv_func, &offst))
			qprintf("  func:%s  arg:0x%x\n",
				ptr, hvp->hv_arg);
		else
			qprintf("  func 0x%x arg 0x%x\n",
				hvp->hv_func, hvp->hv_arg);
		(void) intvec_tinfo(&hvp->hv_tinfo);
	} else {
		/* Call myself for every level. */
		for (l = 0; l < HEART_INT_VECTORS; l++) {
			(void) idbg_intvec(l);
		}
	}
}
#endif /* IP30 */

#ifdef EVEREST
extern void evintr_stray(eframe_t *ep, void *arg);

/*
 * Print the evintr table contents.
 */
void
idbg_intvec(__psint_t l)
{
	evintr_t *ep;
	char *ptr;
	__psunsigned_t offst;

	if (l >= 0 && l < EVINTR_MAX_LEVELS) {
		ep = &evintr[l];

		if (ep->evi_func == evintr_stray || ep->evi_func == 0)
			return;

		qprintf("evintr(0x%x):lvl %d dest %d\n",
			ep, ep->evi_level, ep->evi_dest);

		if (ptr = lookup_kname((void*)ep->evi_func, &offst))
			qprintf("  spl %d reg 0x%x func %s arg 0x%x\n",
				ep->evi_spl, ep->evi_regaddr,
				ptr, ep->evi_arg);
		else
			qprintf("  spl %d reg 0x%x func 0x%x arg 0x%x\n",
				ep->evi_spl, ep->evi_regaddr,
				ep->evi_func, ep->evi_arg);
		(void) intvec_tinfo(&ep->evi_tinfo);
	} else {
		/* Call myself for every level. */
		for (l = 0; l < EVINTR_MAX_LEVELS; l++) {
			(void) idbg_intvec(l);
		}
	}
}
#endif /* EVEREST */

#ifdef MCCHIP
void
idbg_intvec(__psint_t x)
{
	lclvec_t *lvp;
	int i, num_entries=8;
	 __psunsigned_t offst;

	if (x == 0) {
		lvp = &lcl0vec_tbl[1];
#ifdef IP26
		num_entries=10; /* Teton has a couple extra in lcl0. */
#endif /* IP26 */
	} else if (x == 1) {
		lvp = &lcl1vec_tbl[1];
#ifndef IP20
	} else if (x == 2) {
		lvp = &lcl2vec_tbl[1];
#endif /* !IP20 */
	} else if (x == -1) {
		/* print all of them */
		idbg_intvec(0);
		idbg_intvec(1);
#ifndef IP20
		idbg_intvec(2);
#endif /* !IP20 */
		return;
	} else {
		return;
	}

	for (i = 0; i < num_entries; i++, lvp++) {
		qprintf( "lcl%dvec_tbl[%d]:isr %s\targ 0x%x bit 0x%x\n",
			x, i+1, lookup_kname((void *)lvp->isr, (void *)&offst),
			lvp->arg, lvp->bit);
		(void) intvec_tinfo(&lvp->lcl_tinfo);
	}
	
}
#endif /* MCCHIP */

/*
 *
++	kp call name/addr - call a function
++
 */
void
idbg_call(__psint_t p)
{
	if (!IS_KSEG0(p) && !IS_KSEG1(p)) {
		qprintf("address %x must be in kseg0/1\n", p);
		return;
	}
	if (p & 0x3) {
		qprintf("address 0x%x must be four-byte aligned\n", p);
		return;
	}
	(*(int (*)(void))p)();
}

/*
++
++	kp kill arg - give SIGKILL to a process
++		arg < 0 - use arg as address of proc struct
++		arg >= 0 - use arg as pid - lookup proc for pid <arg>
++
*/
void
idbg_kill(__psint_t p1)
{
   proc_t *pp;
	uthread_t	*ut;

	if (p1 == -1L) {
		qprintf("No process specified\n");
		return;
	}

	if (p1 < 0) {
		pp = (proc_t *) p1;
		if (!procp_is_valid(pp)) {
			qprintf("WARNING:0x%x is not a proc address\n", pp);
		}
	} else {
		if ((pp = idbg_pid_to_proc(p1)) == NULL) {
			qprintf("%d is not an active pid\n", (pid_t)p1);
			return;
		}
	}

	qprintf("Giving SIGKILL to pid %d\n", pp->p_pid);
	/*
	 * This is pretty ugly and brutal, but it's too dangerous to try
	 * to call psignal(). Obviously this won't help if the process
	 * is permanently hung out to dry on a semaphore.  This only
	 * helps when the guy is running in foreground on your only terminal,
	 * but can't be killed with control-C.
	 */
	for (ut = pp->p_proxy.prxy_threads; ut; ut = ut->ut_next) {
		sigaddset(&ut->ut_sig, SIGKILL);
	}
}

/*
++
++	kp qbuf dev - list buffers outstanding for the given device
++
*/

void
idbg_qbuf(__psint_t dev)
{
	int i;

	qprintf("Data buffers queued for device 0x%x:\n", dev);
	for (i = 0; i < v.v_buf; i++) {
		if (global_buf_table[i].b_edev == dev) {
			buf_t *bp = &global_buf_table[i];
			sema_t *sp = &bp->b_lock;
			k_semameter_t *kp = smeter(sp);

			qprintf(" 0x%x [%d] blk 0x%x bsz %d flags:", bp,
				i, bp->b_blkno, bp->b_bufsize);
			_printflags((unsigned)bp->b_flags, tab_bflags, 0);
			if (kp) {
				if (sp->s_un.s_st.count <= 0) {
					printid(kp->s_thread, kp->s_id, " owner");
				} else
					qprintf(" unlocked");
			}
			qprintf("\n");
		}
	}
}

/*
++	List buffers marked as B_NFS_UNSTABLE or B_NFS_ASYNC
++
*/
void
idbg_unstablebuf(void)
{
	int i;

	qprintf("Unstable buffers in the buffer cache\n");
        for (i = 0; i < v.v_buf; i++) {
                if (global_buf_table[i].b_flags & (B_NFS_UNSTABLE|B_NFS_ASYNC)) {
                        buf_t *bp = &global_buf_table[i];

                        qprintf(" 0x%x [%d] blk 0x%x bsz %d vp 0x%x flags:", 
				bp, i, bp->b_blkno, bp->b_bufsize, bp->b_vp);

                        _printflags(bp->b_flags, tab_bflags, "flags");
                        qprintf("\n");
                }
        }
}


#ifdef SPLMETER
/*
++
++	kp spl - list splhi history of the system routine
++	count
++
*/

extern splmeter_t splhead[];
extern splmeter_t timerhead[];
extern void reset_splmeter(void);
extern splmeter_t *splactive[];

void
idbg_splmeter(__psint_t list)
{
	splmeter_t *spl;
	int i;
	extern int spl_level;
	extern int maxcpus;
	int cpumax;
	__psunsigned_t offst;

	if (maxcpus > 8)
		cpumax = 8;
	else
		cpumax = maxcpus;
	if (list == 1) {
		qprintf("TIMER LIST\n");
		qprintf("start(cpu#)\tstop(cpu#)\tmax\n");
		qprintf("\t\t\t\t(us)\n\n");
		for (i =0; i<maxcpus; i++) {	/* 8 cpu max */
			if (pdaindr[i].CpuId == -1)
				continue;
			spl = timerhead[i].next;
			while (spl != 0) {
				qprintf("%s+0x%x(%d)",
					fetch_kname(spl->hipc-2, &offst), offst,
					spl->cpustart);
				qprintf("\t%s+0x%x(%d)\t\t%d\n", 
					fetch_kname(spl->lopc-2, &offst), offst,
					spl->cpuend,
					(spl->tickmax*timer_unit)/1000);
				spl = spl->next;
			}
		}

	} else if (list == 2) {
		reset_splmeter();
	} else if (list == 3) {
		qprintf("SPL LEVEL %d\n",spl_level);
		qprintf("raise		lower		max	total	latbust	at	max	time\n");
		qprintf("spl(cpu#)	spl(cpu#)	(us)	spl	spl	spl	ishi	stamp\n\n");

		for (i =0; i<8; i++) {	/* 8 cpu max */
			if (pdaindr[i].CpuId == -1)
				continue;
			spl = splactive[i];
			qprintf("< CPU #%d >\n",i);
			while (spl != 0) {
				qprintf("%s+0x%x",
					fetch_kname(spl->hipc-2, &offst), offst);
				qprintf("\t%s+0x%x\t%d\t%d\t%d\t%d\t%d\t%d\n",
					fetch_kname(spl->lopc-2, &offst), offst,
					(spl->tickmax*timer_unit)/1000,
					spl->splcount,
					spl->latbust,
					spl->ishicount,
					spl->mxishi,
					(spl->tickstart/1000)*timer_unit);
				spl = spl->active;
			}
		}
	} else {
		qprintf("SPL LEVEL %d\n",spl_level);
		qprintf("raise		lower		max	total	latbust	at	max	time\n");
		qprintf("spl(cpu#)	spl(cpu#)	(us)	spl	spl	spl	ishi	stamp\n\n");

		for (i =0; i<8; i++) {	/* 8 cpu max */
			if (pdaindr[i].CpuId == -1)
				continue;
			spl = splhead[i].next;
			qprintf("< CPU #%d >\n",i);
			while (spl != 0) {
				qprintf("%s+0x%x",
					fetch_kname(spl->hipc-2, &offst), offst);
				qprintf("\t%s+0x%x\t%d\t%d\t%d\t%d\t%d\t%d\n",
					fetch_kname(spl->lopc-2, &offst), offst,
					(spl->tickmax*timer_unit)/1000,
					spl->splcount,
					spl->latbust,
					spl->ishicount,
					spl->mxishi,
					(spl->timestamp/1000)*timer_unit);
				spl = spl->next;
			}
		}
	}
}
#endif

static void
idbg_dozone(zone_t *zp, int all)
{
	int *fp;


	qprintf("\n");
	{
	    int 	lastnamep;
	    extern char *zoneuser_name_lookup(int, int *);
	    char 	*name;
	    lastnamep =0;
	    if (zp->zone_type == ZONE_SET){
		while (name = zoneuser_name_lookup(zp->zone_index, &lastnamep))
			qprintf("\"%s\" ", name);
	    }
	}

	qprintf("%s zone at 0x%x use count %d [%d/pp]\n",
		zp->zone_name ? zp->zone_name : "???",
		zp,
		zp->zone_usecnt, zp->zone_units_pp);
	qprintf("  node %d radius %d index %d  type %d \n",
		zp->zone_node, zp->zone_radius, zp->zone_index, 
		zp->zone_type);
	qprintf("  size 0x%x minsize 0x%x kvsz %d nfree %d freelist 0x%x free_dmap 0x%x\n",
		zp->zone_total_pages * ctob(1),
		zp->zone_minsize * ctob(1),
		zp->zone_kvm * ctob(1),
		zp->zone_free_count,
		zp->zone_free, 
		zp->zone_free_dmap);
	qprintf("  pagelist 0x%x dmap pagelist 0x%x\n",
		zp->zone_page_list, 
		zp->zone_dmap_page_list);
	qprintf("  shake_skip %d shake_skipped %d pgs freed %d nxt zone 0x%x\n",
		zp->zone_shake_skip,
		zp->zone_shakes_skipped,
		zp->zone_pages_freed,
		zp->zone_next);


	if (!all)
		return;

	qprintf("  [ ");
	for (fp = (int *)zp->zone_free; fp; fp = *(int **)((__psunsigned_t )fp & ~1)) {
		qprintf("%x ", fp);
	}
	qprintf("]\n");

	qprintf("  Direct map list[ ");
	for (fp = (int *)zp->zone_free_dmap; fp; fp = *(int **)((__psunsigned_t )fp & ~1)) {
		qprintf("%x ", fp);
	}
	qprintf("]\n");
}

void
idbg_zone(__psint_t p)
{
	zone_t *zp;
	extern gzone_t kmem_private_zones, kmem_zoneset;

	if (p >= 0 ) {
		int	n;
		for (n = 0; n < numnodes; n++){
			zp = NODE_ZONE(n, p);
			if (zp)
				idbg_dozone(zp, 1);
		}
		return;
	} 
	if (p == -1) {
		int totalsize = 0;
		int free = 0;

		qprintf("Kmem zoneset\n");
		for (zp = kmem_zoneset.zone_list; zp; zp = zp->zone_next) {
			free +=  zp->zone_unitsize * zp->zone_free_count;
			totalsize += zp->zone_total_pages;
			/* Avoid printing uninitialized zones in NUMA mode. */
			if (zp->zone_total_pages || zp->zone_free_count)
				idbg_dozone(zp, 0);
		}

		qprintf("\nKmem private zones (%d zones)\n",
			kmem_private_zones.zone_nzones);
		for (zp = kmem_private_zones.zone_list; zp; zp = zp->zone_next)
		{
			free += zp->zone_unitsize * zp->zone_free_count;
			totalsize += zp->zone_total_pages;
			idbg_dozone(zp, 0);
		}
		totalsize *= ctob(1);
		qprintf("\nTotal zone memory %d, %d free\n", totalsize, free);
		return;
	}
	(void)idbg_dozone((zone_t *)p, 1);
}

void
idbg_tracemasks(struct pr_tracemasks *t)
{

	qprintf("  entrymask %w32x:%w32x:%w32x:%w32x:%w32x:%w32x"
		" exitmask %w32x:%w32x:%w32x:%w32x:%w32x:%w32x\n",
		t->pr_entrymask.word[0], t->pr_entrymask.word[1],
		t->pr_entrymask.word[2], t->pr_entrymask.word[3],
		t->pr_entrymask.word[4], t->pr_entrymask.word[5],
		t->pr_exitmask.word[0], t->pr_exitmask.word[1],
		t->pr_exitmask.word[2], t->pr_exitmask.word[3],
		t->pr_exitmask.word[4], t->pr_exitmask.word[5]);

	qprintf("  &fltmask 0x%x systrap 0x%x\n",
		&t->pr_fltmask, t->pr_systrap);
	qprintf("  rval1 %lld rval2 %lld error %d \n",
		t->pr_rval1, t->pr_rval2, t->pr_error);
}

/*
 * kp tstamp <arg> - dump tstamp state;
 *     where <arg> is p_tstamp_objp from the pda.
 */
void
idbg_tstamp(const tstamp_obj_t* objp)
{
    qprintf(" objp 0x%x:\n", objp);
    qprintf(" shared_state 0x%x [lost %d tstamp %d curr %d]\n",
	    objp->shared_state,
	    objp->shared_state->lost_counter,
	    objp->shared_state->tstamp_counter, 
	    objp->shared_state->curr_index
	);
    qprintf(" allocated_base 0x%x allocated_size %d\n",
	    objp->allocated_base, objp->allocated_size);
    qprintf(" nentries %d water_mark %d nsleepers %d mask 0x%llx\n",
	    objp->nentries, objp->water_mark, objp->nsleepers,
	    objp->tracemask);
}

#define	TEST_CPU_INRANGE(cpu)	(0 <= cpu && cpu < maxcpus)
#define	TEST_CPU_OK(cpu) \
	(TEST_CPU_INRANGE(cpu) && pdaindr[cpu].CpuId != -1)
#define	TSTAMP_OBJP(cpu) \
	((tstamp_obj_t*) pdaindr[cpu].pda->p_tstamp_objp)
#define TEST_TSTAMP_OBJECT(cpu) \
	(TSTAMP_OBJP(cpu) != (tstamp_obj_t*) 0)
#define TSTAMP_ENTRIES(cpu) \
	((tstamp_event_entry_t *) pdaindr[cpu].pda->p_tstamp_entries)
#define TSTAMP_LAST(cpu) \
	((tstamp_event_entry_t *) pdaindr[cpu].pda->p_tstamp_last)
#define TSTAMP_INDEX(cpu) \
	(pdaindr[cpu].pda->p_tstamp_eobmode == RT_TSTAMP_EOB_WRAP ? \
	 (tstamp_event_entry_t *)pdaindr[cpu].pda->p_tstamp_ptr - TSTAMP_ENTRIES(cpu):\
	 TSTAMP_OBJP(cpu)->shared_state->curr_index)

/* Dump a single tstamp event entry -- called by idbg_utrace */
static void
utrace_dump(tstamp_event_entry_t *event, int cpu, 
	    tstamp_event_entry_t *limit,
	    __psint_t sval) {
    char buf[50], *name;
    utrace_trtbl_t *entry;
    int c, c2;
    __uint64_t *p;
    extern utrace_trtbl_t utrace_trtbl[];

    /* UTRACEs have the event name in a qualifier */
    if (event->evt == UTRACE_EVENT) {
if (sval) {
  if((sval != event->qual[0]) && (sval != event->qual[1]))
	return;
}
	bcopy(&event->qual[2], buf, sizeof(__uint64_t));
	buf[sizeof(__uint64_t)] = '\0'; /* null-terminate the string */
	name = buf;
    }
    else if (event->evt == JUMBO_EVENT) {
if (sval) {
  if((sval != event->qual[0]) && (sval != event->qual[1]) &&
     (sval != event->qual[2]) && (sval != event->qual[3]))
	return;
}
	name = "  ---";
    }
    else {
if (sval) {
  if((sval != event->qual[0]) && (sval != event->qual[1]) &&
     (sval != event->qual[2]) && (sval != event->qual[3]))
	return;
}
	/* Look up the event no. in the table.  Should really be a hash... */
	for(entry = &utrace_trtbl[0]; entry->name; entry++) {
	    if (entry->evt == event->evt) {
		name = entry->name;
		break;
	    }
	}
	if (!entry->name) {		/* didn't find it */
	    sprintf(buf, "Evt %d", event->evt);
	    name = buf;
	}
    }
    qprintf("%22s %4s %16s %16s %16s", 
	    _padstr(name, 22),
	    padnum(cpu, 10, 4),
	    padnum64(event->tstamp, 16, sizeof(__uint64_t)*2),
	    pad_utrace_qual(event->qual[0]),
	    pad_utrace_qual(event->qual[1]));
    if (event->evt != UTRACE_EVENT) {
	sprintf(buf, "0x%s:", padnum((__psint_t)event, 16,
				     sizeof(__psint_t)*2));
	qprintf("\n  %19s ev=%9s r=%4s    %16s %16s",
		_padstr(buf, 19),
		padnum(event->evt, 10, 9),
		padnum(event->jumbocnt, 10, 4),
		pad_utrace_qual(event->qual[2]),
		pad_utrace_qual(event->qual[3]));
    }
    /* Dump remaining jumbo event data (if any) */
    c = event->jumbocnt * sizeof(tstamp_event_entry_t) / sizeof(__uint64_t);
    p = (__uint64_t *)(event + 1);
    for (c2 = 0; c; c--, c2++) {
	if (p >= (__uint64_t *)(TSTAMP_LAST(cpu)+1))
	    p = (__uint64_t *)TSTAMP_ENTRIES(cpu);
	if (c2 % 3 == 0)
	    qprintf("\n%27s", _padstr("", 27));
	qprintf(" %16s", padnum64(*p, 16, sizeof(__uint64_t)*2));
	p++;
    }
    /* Dump any continuation event data, five doublewords/event.
       But don't go beyond the specified limit. */
    event = (event < TSTAMP_LAST(cpu) ? (event + 1) : TSTAMP_ENTRIES(cpu));
    while (event->evt == JUMBO_EVENT && event != limit) {
	for (p=(__uint64_t *)&event->tstamp, c=5; c; c--, c2++) {
	    if (c2 % 3 == 0)
		qprintf("\n%27s", _padstr("", 27));
	    qprintf(" %16s", padnum64(*p, 16, sizeof(__uint64_t)*2));
	    p++;
	}
	event = (event < TSTAMP_LAST(cpu) ? (event + 1) : TSTAMP_ENTRIES(cpu));
    }
    qprintf("\n");
}


/* idbg_utrace() uses an array of these structures to sort the events */
typedef struct trlist {
    int cpu;			/* CPU ID */
    int index;			/* "current" buffer entry */
    int prev;			/* "previous" entry */
} tracelist_t;

/* Macro to advance to the "next" (previous in time) event in the
   buffer.  Must scan the whole buffer, since we can't index backwards
   over a jumbo event.  Keep going until we find an event which isn't
   a continuation.  Yes, it's horribly inefficient. */
#define NEXT_TRACE(elt) \
	{								\
		int i, ni, e, left, cnt;				\
		int nentries = TSTAMP_OBJP((elt).cpu)->nentries;	\
		(elt).prev = i = (elt).index;				\
		do {							\
		    e = i;						\
		    for(left = nentries; left; left -= cnt) {		\
			cnt = TSTAMP_ENTRIES((elt).cpu)[i].jumbocnt + 1; \
			ni = i + cnt;					\
			if (ni >= nentries) ni -= nentries;		\
			if ((i < e && e <= ni) || (e <= ni && ni < i))	\
			    break;					\
			i=ni;						\
		    }							\
		} while (TSTAMP_ENTRIES((elt).cpu)[i].evt == JUMBO_EVENT); \
		(elt).index = i;					\
	}
static char *utusage = "Usage: utrace <count> <cpu>  (or -1 for all CPUs)\n";

/*
 *  kp utrace <count> <cpu> -- Merge and dump utraces
 */

void
_idbg_utrace(__psint_t arg1, __psint_t arg2, __psint_t sval)
{
    __psint_t count, cpu;
    int cpucount, i, highest;
    tstamp_event_entry_t *evt, *evt2;
    tracelist_t list[MAXCPUS]; /* XXX one/CPU  Can I kern_malloc() from idbg? */

    /* symmon and user-level idbg have different calling conventions */
    if (idbginvoke == IDBGUSER) { /* user-level */
	count = (__psint_t)arg1;
	cpu = (__psint_t)arg2;
    }
    else {			  /* symmon */
	/* arg2 is ptr to string */
	count = (__psint_t)arg1;
	if (arg2)
	    cpu = atoi((char *)arg2);
	else
	    cpu = cpuid();
    }

    if (count < 0) {
	qprintf("Invalid UTRACE count: %d\n", count);
	qprintf(utusage);
	return;
    }

    if (cpu == -1) {			/* merge traces for all CPUs */
	for (i=0, cpucount = 0; i<maxcpus; i++) {
	    if (pdaindr[i].CpuId != -1 && TEST_TSTAMP_OBJECT(i)) {
		list[cpucount].cpu = i;
		list[cpucount].index = TSTAMP_INDEX(i);
		NEXT_TRACE(list[cpucount]);
		cpucount++;
	    }
	}
	qprintf("Latest UTRACEs for all CPUs:\n");
    }
    else if (TEST_CPU_OK(cpu)) { /* dump a single CPU */
	if (!TEST_TSTAMP_OBJECT(cpu)) {
	    qprintf("CPU %d has no UTRACE buffer\n", cpu);
	    return;
	}
	list[0].cpu = cpu;
	list[0].index = TSTAMP_INDEX(cpu);
	NEXT_TRACE(list[0]);
	cpucount = 1;
	qprintf("Latest UTRACEs for CPU %d:\n", cpu);
    }
    else {
	qprintf("Invalid CPU: %d\n", cpu);
	qprintf(utusage);
	return;
    }

    qprintf("ID, ADDR               CPU  REAL TIME CLOCK  INFO 1,3"
	    "         INFO 2,4\n");
    while(count--) {
	/* Find the highest tstamp in current list and print it */
	evt = &(TSTAMP_ENTRIES(list[0].cpu)[list[0].index]);
	highest = 0;
	for (i=0; i<cpucount; i++) {
	    evt2 = &(TSTAMP_ENTRIES(list[i].cpu)[list[i].index]);
	    if (evt2->tstamp > evt->tstamp) {
		evt = evt2;
		highest = i;
	    }
	}

	utrace_dump(evt, list[highest].cpu,
		    &(TSTAMP_ENTRIES(list[highest].cpu)[list[highest].prev]),sval);
	/* Advance to next event on this CPU */
	NEXT_TRACE(list[highest]);
    }
}
void
idbg_utrace(__psint_t arg1, __psint_t arg2)
{
	_idbg_utrace(arg1, arg2, 0);
}

void
idbg_utrace_find(__psint_t arg1, __psint_t arg2)
{
	_idbg_utrace(arg1, (__psint_t)"-1", atoi((char *)arg2));
}

extern int _utrace_sanity(void *, uint64_t);
extern long long utrace_mask;

void
idbg_utrace_mask(uint64_t arg1)
{
	if (arg1 == -1LL)
		qprintf("utrace_mask: 0x%llx\n", utrace_mask);
	else
		_utrace_sanity(&utrace_mask, arg1);
}
#undef NEXT_TRACE


/*
++
++	kp softfp <arg> - dump information about a process
++		arg == -1 - use curprocp
++		arg < 0 - use addr as address of proc struct
++		arg >= 0 - use arg as pid to lookup
++
*/
static void
idbg_do_softfp(uthread_t *ut)
{
	qprintf("epcinst 0x%x, bdinst 0x%x, fp_enables 0x%x fp_csr 0x%x\n",
		ut->ut_epcinst, ut->ut_bdinst,
		ut->ut_fp_enables, ut->ut_fp_csr);
	qprintf("fp_csr_rm 0x%x, flags 0x%x, rdnum 0x%x\n",
		ut->ut_fp_csr_rm, ut->ut_softfp_flags, ut->ut_rdnum);
}

void
idbg_softfp(__psint_t p1)
{
	(void) idbg_doto_uthread(p1, idbg_do_softfp, 1, 0);
}


static void
print_maclabel(char *cp, mac_label *lp)
{
	char *cheater = (char *)lp;
	int i;

	if (lp == NULL) {
		qprintf("%s <NULL>\n", cp);
		return;
	}
	if (mac_invalid(lp)) {
		qprintf("%s <invalid>[", cp);
		for (i = 0; i < 8; i++)
			qprintf("%x ", *cheater++);
		qprintf("]\n");
		return;
	}
	qprintf("%s ", cp);
	qprintf("%c,%d", lp->ml_msen_type, lp->ml_level);
	for (i = 0; i < lp->ml_catcount; i++)
		qprintf(",%d", lp->ml_list[i]);
	qprintf("/%c,%d", lp->ml_mint_type, lp->ml_grade);
	for (i = 0; i < lp->ml_divcount; i++)
		qprintf(",%d", lp->ml_list[i + lp->ml_catcount]);
	qprintf("\n");
}

#if defined(MP)
/*
 * Cause all CPUs to stop by sending them each a DEBUG interrupt.
 */
void
idbg_stop(void *stoplist)
{
	debug_stop_all_cpus(stoplist);
}
#endif

#if defined (SN)
#include <sys/SN/error.h>
void
idbg_hwstate()
{
	kl_log_hw_state();
	kl_error_show_idbg("Hardware Error State (symmon forced dump)\n",
		      "End Hardware Error State (symmon forced dump)\n");
}


#endif /* SN0 */

#if defined(SN0) || defined(IP30)
/*
 * Interface to dump stuff in ioerror
 */

void
idbg_ioerror(__psunsigned_t arg)
{
	ioerror_t	*ioerror = (ioerror_t *)arg;

	qprintf("Printing IOerror structure at 0x%x\n", ioerror);

#define	PRFIELD(f)							\
	if (IOERROR_FIELDVALID(ioerror,f))				\
		qprintf("\t%20s: %d ", #f, IOERROR_GETVALUE(ioerror,f));

	PRFIELD(errortype);		/* error type: extra info about error */
	PRFIELD(widgetnum);		/* Widget number that's in error */
	PRFIELD(widgetdev);		/* Device within widget in error */
	PRFIELD(srccpu);		/* CPU on srcnode generating error */
	PRFIELD(srcnode);		/* Node which caused the error 	 */
	PRFIELD(errnode);		/* Node where error was noticed	 */
	PRFIELD(sysioaddr);		/* Sys specific IO address	 */
	PRFIELD(xtalkaddr);		/* Xtalk (48bit) addr of Error 	 */
	PRFIELD(busspace);		/* Bus specific address space	 */
	PRFIELD(busaddr);		/* Bus specific address	 	 */
	PRFIELD(vaddr);			/* Virtual address of error 	 */
	PRFIELD(memaddr);		/* Physical memory address  	 */

#undef	PRFIELD

	qprintf("\n");
}
#endif /* SN0 || IP30 */

#if EVEREST
/*
 *  Display Everest memory singlebit error information
 */
extern mc3_array_err_t mc3_errcount[];

void
idbg_memecc(void)
{
	int slot, mbid, bank, simm;
	uint cum_count = 0, errcount;
	mc3_bank_err_t *bank_info;

	for (slot = 0; slot < EV_MAX_SLOTS; slot++) {
	    evbrdinfo_t *eb = &(EVCFGINFO->ecfg_board[slot]);

	    if (eb->eb_type == EVTYPE_MC3) {
		mbid = eb->eb_mem.eb_mc3num;

	        if (errcount = mc3_errcount[mbid].m_unk_bank_errcount) {
		    qprintf("Slot %d  bank ?:  %d singlebit errors\n",
			    slot, errcount);
		    cum_count += errcount;
	        } else
		    for (bank = 0; bank < MC3_NUM_BANKS; bank++) {
		        bank_info = &(mc3_errcount[mbid].m_bank_errinfo[bank]);
			if (errcount = bank_info->m_bank_errcount) {
			    qprintf("Slot %d bank %d: %d singlebit errors ",
				    slot, bank, errcount);
			    cum_count += errcount;
			    for (simm = 0; simm < MC3_SIMMS_PER_BANK; simm++) {
				if (errcount = bank_info->m_simm_errcount[simm])
					qprintf(" simm%d:%d", simm, errcount);
			    }
			    qprintf("\n");
			}
		    }
	    }
	}
	if (cum_count == 0)
		qprintf("No singlebit errors have occurred.\n");
}
#endif

#undef CEOF
/*
 * Cruft to deal with regular expression matching.
 */

/*
 * Copyright (c) 1980 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)regex.c	5.2 (Berkeley) 3/9/86";
#endif /* LIBC_SCCS and not lint */

#

/*
 * routines to do regular expression matching
 *
 * Entry points:
 *
 *	re_comp(s)
 *		char *s;
 *	 ... returns 0 if the string s was compiled successfully,
 *		     a pointer to an error message otherwise.
 *	     If passed 0 or a null string returns without changing
 *           the currently compiled re (see note 11 below).
 *
 *	re_exec(s)
 *		char *s;
 *	 ... returns 1 if the string s matches the last compiled regular
 *		       expression, 
 *		     0 if the string s failed to match the last compiled
 *		       regular expression, and
 *		    -1 if the compiled regular expression was invalid 
 *		       (indicating an internal error).
 *
 * The strings passed to both re_comp and re_exec may have trailing or
 * embedded newline characters; they are terminated by nulls.
 *
 * The identity of the author of these routines is lost in antiquity;
 * this is essentially the same as the re code in the original V6 ed.
 *
 * The regular expressions recognized are described below. This description
 * is essentially the same as that for ed.
 *
 *	A regular expression specifies a set of strings of characters.
 *	A member of this set of strings is said to be matched by
 *	the regular expression.  In the following specification for
 *	regular expressions the word `character' means any character but NUL.
 *
 *	1.  Any character except a special character matches itself.
 *	    Special characters are the regular expression delimiter plus
 *	    \ [ . and sometimes ^ * $.
 *	2.  A . matches any character.
 *	3.  A \ followed by any character except a digit or ( )
 *	    matches that character.
 *	4.  A nonempty string s bracketed [s] (or [^s]) matches any
 *	    character in (or not in) s. In s, \ has no special meaning,
 *	    and ] may only appear as the first letter. A substring 
 *	    a-b, with a and b in ascending ASCII order, stands for
 *	    the inclusive range of ASCII characters.
 *	5.  A regular expression of form 1-4 followed by * matches a
 *	    sequence of 0 or more matches of the regular expression.
 *	6.  A regular expression, x, of form 1-8, bracketed \(x\)
 *	    matches what x matches.
 *	7.  A \ followed by a digit n matches a copy of the string that the
 *	    bracketed regular expression beginning with the nth \( matched.
 *	8.  A regular expression of form 1-8, x, followed by a regular
 *	    expression of form 1-7, y matches a match for x followed by
 *	    a match for y, with the x match being as long as possible
 *	    while still permitting a y match.
 *	9.  A regular expression of form 1-8 preceded by ^ (or followed
 *	    by $), is constrained to matches that begin at the left
 *	    (or end at the right) end of a line.
 *	10. A regular expression of form 1-9 picks out the longest among
 *	    the leftmost matches in a line.
 *	11. An empty regular expression stands for a copy of the last
 *	    regular expression encountered.
 */

/*
 * constants for re's
 */
#define	CBRA	1
#define	CCHR	2
#define	CDOT	4
#define	CCL	6
#define	NCCL	8
#define	CDOL	10
#define	CEOF	11
#define	CKET	12
#define	CBACK	18

#define	CSTAR	01

#define	ESIZE	512
#define	NBRA	9

#ifdef sgi
struct regex {
	char	expbuf[ESIZE];
	char	*braslist[NBRA];
	char	*braelist[NBRA];
};
static struct regex _rgx;
static struct regex *rgx = &_rgx;
#else  /* sgi */
static	char	expbuf[ESIZE], *braslist[NBRA], *braelist[NBRA];
#endif /* sgi */
static	char	circf = 0;

static	int	advance(const char *lp, char *ep);
static		backref(int i, const char *lp);
static	int	cclass(char *set, char c, int af);
/*
 * compile the regular expression argument into a dfa
 */
char *
re_comp(const char *sp)
{
	register int	c;
	register char	*ep;
	int	cclcnt, numbra = 0;
	char	*lastep = 0;
	char	bracket[NBRA];
	char	*bracketp = &bracket[0];
	static	char	*retoolong = "Regular expression too long";

#ifdef sgi
	ep = rgx->expbuf;
#endif /* sgi */
#define	comerr(msg) {rgx->expbuf[0] = 0; numbra = 0; return(msg); }

	if (sp == 0 || *sp == '\0') {
		if (*ep == 0)
			return("No previous regular expression");
		return(0);
	}
	if (*sp == '^') {
		circf = 1;
		sp++;
	}
	else
		circf = 0;
	for (;;) {
		if (ep >= &rgx->expbuf[ESIZE])
			comerr(retoolong);
		if ((c = *sp++) == '\0') {
			if (bracketp != bracket)
				comerr("unmatched \\(");
			*ep++ = CEOF;
			*ep++ = 0;
			return(0);
		}
		if (c != '*')
			lastep = ep;
		switch (c) {

		case '.':
			*ep++ = CDOT;
			continue;

		case '*':
			if (lastep == 0 || *lastep == CBRA || *lastep == CKET)
				goto defchar;
			*lastep |= CSTAR;
			continue;

		case '$':
			if (*sp != '\0')
				goto defchar;
			*ep++ = CDOL;
			continue;

		case '[':
			*ep++ = CCL;
			*ep++ = 0;
			cclcnt = 1;
			if ((c = *sp++) == '^') {
				c = *sp++;
				ep[-2] = NCCL;
			}
			do {
				if (c == '\0')
					comerr("missing ]");
				if (c == '-' && ep [-1] != 0) {
					if ((c = *sp++) == ']') {
						*ep++ = '-';
						cclcnt++;
						break;
					}
					while (ep[-1] < c) {
						*ep = ep[-1] + 1;
						ep++;
						cclcnt++;
						if (ep >= &rgx->expbuf[ESIZE])
							comerr(retoolong);
					}
				}
				*ep++ = c;
				cclcnt++;
				if (ep >= &rgx->expbuf[ESIZE])
					comerr(retoolong);
			} while ((c = *sp++) != ']');
			lastep[1] = cclcnt;
			continue;

		case '\\':
			if ((c = *sp++) == '(') {
				if (numbra >= NBRA)
					comerr("too many \\(\\) pairs");
				*bracketp++ = numbra;
				*ep++ = CBRA;
				*ep++ = numbra++;
				continue;
			}
			if (c == ')') {
				if (bracketp <= bracket)
					comerr("unmatched \\)");
				*ep++ = CKET;
				*ep++ = *--bracketp;
				continue;
			}
			if (c >= '1' && c < ('1' + NBRA)) {
				*ep++ = CBACK;
				*ep++ = c - '1';
				continue;
			}
			*ep++ = CCHR;
			*ep++ = c;
			continue;

		defchar:
		default:
			*ep++ = CCHR;
			*ep++ = c;
		}
	}
}

/* 
 * match the argument string against the compiled re
 */
int
re_exec(const char *p1)
{
	register char	*p2 = rgx->expbuf;
	register int	c;
	int	rv;

	for (c = 0; c < NBRA; c++) {
		rgx->braslist[c] = 0;
		rgx->braelist[c] = 0;
	}
	if (circf)
		return((advance(p1, p2)));
	/*
	 * fast check for first character
	 */
	if (*p2 == CCHR) {
		c = p2[1];
		do {
			if (*p1 != c)
				continue;
			if (rv = advance(p1, p2))
				return(rv);
		} while (*p1++);
		return(0);
	}
	/*
	 * regular algorithm
	 */
	do
		if (rv = advance(p1, p2))
			return(rv);
	while (*p1++);
	return(0);
}

/* 
 * try to match the next thing in the dfa
 */
static	int
advance(const char *lp, register char *ep)
{
	const char	*curlp;
	int	ct, i;
	int	rv;

	for (;;)
		switch (*ep++) {

		case CCHR:
			if (*ep++ == *lp++)
				continue;
			return(0);

		case CDOT:
			if (*lp++)
				continue;
			return(0);

		case CDOL:
			if (*lp == '\0')
				continue;
			return(0);

		case CEOF:
			return(1);

		case CCL:
			if (cclass(ep, *lp++, 1)) {
				ep += *ep;
				continue;
			}
			return(0);

		case NCCL:
			if (cclass(ep, *lp++, 0)) {
				ep += *ep;
				continue;
			}
			return(0);

		case CBRA:
			rgx->braslist[*ep++] = (char *)lp;
			continue;

		case CKET:
			rgx->braelist[*ep++] = (char *)lp;
			continue;

		case CBACK:
			if (rgx->braelist[i = *ep++] == 0)
				return(-1);
			if (backref(i, lp)) {
				lp += rgx->braelist[i] - rgx->braslist[i];
				continue;
			}
			return(0);

		case CBACK|CSTAR:
			if (rgx->braelist[i = *ep++] == 0)
				return(-1);
			curlp = lp;
			ct = rgx->braelist[i] - rgx->braslist[i];
			while (backref(i, lp))
				lp += ct;
			while (lp >= curlp) {
				if (rv = advance(lp, ep))
					return(rv);
				lp -= ct;
			}
			continue;

		case CDOT|CSTAR:
			curlp = lp;
			while (*lp++)
				;
			goto star;

		case CCHR|CSTAR:
			curlp = lp;
			while (*lp++ == *ep)
				;
			ep++;
			goto star;

		case CCL|CSTAR:
		case NCCL|CSTAR:
			curlp = lp;
			while (cclass(ep, *lp++, ep[-1] == (CCL|CSTAR)))
				;
			ep += *ep;
			goto star;

		star:
			do {
				lp--;
				if (rv = advance(lp, ep))
					return(rv);
			} while (lp > curlp);
			return(0);

		default:
			return(-1);
		}
}

static
backref(register int i, const char *lp)
{
	register char	*bp;

	bp = rgx->braslist[i];
	while (*bp++ == *lp++)
		if (bp >= rgx->braelist[i])
			return(1);
	return(0);
}

static int
cclass(register char *set, register char c, int af)
{
	register int	n;

	if (c == 0)
		return(0);
	n = *set++;
	while (--n)
		if (*set++ == c)
			return(af);
	return(! af);
}
#undef CEOF


/*
**  NAME
**	glob2regex - convert a shell-style glob expr to a regular expr
**
**  DESCRIPTION
**	glob2regex converts the glob expression character string 'in' to a
**	the regex(3X) style regular expression.  This conversion permits the
**	use of a single expression matching algorithm yet allows globbing
**	style input.  The globbing style is similar to csh(1) globbing style
**	with the exception that curly brace '{' or'ing notation is not
**	supported.  Also ~ is ignored as it only applies to path names.
**
**	A pointer to 'out' is returned as a matter of convenience as is the
**	style for other string(3) routines.
**
**  SEE ALSO
**	csh(1), regex(3X), string(3).
**
*/
void
glob2regex(char* out, const char* in)
{
    const char* i;

    *out++ = '^';
    for(i=in; *i; i++) {
	switch (*i) {
	/*
	** Ignore certain regex meta characters which no correspondence in
	** globbing.
	*/
	case '^':
	case '$':
	case '+':
	case '.':
	case '(':
	case ')':
	case '{':	/* Curly brace or'ing notation is not supported */
	case '}':
	    *out = '\\'; out++;
	    *out = *i; out++;
	    break;

	/*
	** Map single character match
	*/
	case '?':
	    *out = '.'; out++;	
	    break;

	/*
	** Map any string of characters match
	*/
	case '*':
	    *out = '.'; out++;	
	    *out = '*'; out++;	
	    break;

	/*
	** All other characters are treated as is.  This fall through also
	** supports the square bracket '[' match character set notation which
	** is the same for both globbing and regex.
	*/
	default:
	    *out = *i; out++;
	}
    }
    *out++ = '$';
    *out = *i;
}


/*
 * Convert strings into an arg list.
 */
static void
strtoargs(char *buf, int *ac, char **ap)
{
	int i;
	char *bp;
	int mac = *ac;

	/*
	 * Init to null
	 */
	for (i = 0; i < *ac; i++) {
		ap[i] = 0;
	}
	*ac = 0;
	
	for (bp =  buf; *bp && *ac < mac; ) {
		while (*bp && isspace(*bp))
			bp++;
		if (!*bp)
			return;
		*ap++ = bp;
		(*ac)++;
		while (*bp && !isspace(*bp))
			bp++;
		if (!*bp)
			return;
		*bp++ = 0;
	}
	return;
}

/*
 * Pad out.  Work around botch in kp sprintf
 *
 * This is all very hokey stuff, in lieu of %-x.y
 */
#define MAXPADLEN	40
#define NUMPAD		8
#define NEXTPADIX()	{ \
		padix++; \
		if (padix >= NUMPAD) \
			padix = 0; \
		}
static char pads[NUMPAD][MAXPADLEN+1];
static int padix = 0;

char *
_padstr(char *buf, int len)
{
	char *pp = (char *)&pads[padix];
	int slen = strlen(buf);
	int i;

	if (len > MAXPADLEN)
		len = MAXPADLEN;
	if (slen > len)
		slen = len;
	bcopy(buf, pp, slen);
	for (i = slen; i < len; i++)
		pp[i] = ' ';
	pp[len] = 0;
	NEXTPADIX();
	return pp;
}

char *
_padstrn(char *buf, int buflen, int padlen)
{
	char *pp = (char *)&pads[padix];
	int slen;
	int i;

	if (padlen > MAXPADLEN)
		padlen = MAXPADLEN;
	if (buflen > padlen)
		buflen = padlen;
	for(slen=0; slen < buflen && *(buf+slen) != NULL; ++slen)
		; /* just computing strnlen(buf, buflen) */

	bcopy(buf, pp, slen);
	for (i = slen; i < padlen; i++)
		pp[i] = ' ';
	pp[padlen] = 0;
	NEXTPADIX();
	return pp;
}

char *
padnum(__psint_t num, int base, int len)
{
	char *pp = (char *)&pads[padix];
	char *f;
	char buf[MAXPADLEN];
	int slen;
	int i;

	if (len > MAXPADLEN)
		len = MAXPADLEN;
	
	switch (base) {
	case 8:
		f = "%o";
		break;
	case 10:
		f = "%d";
		break;
	default:
	case 16:
		f = "%x";
	}
	sprintf(buf, f, num);
	slen = strlen(buf);
	if (slen > len) {
		NEXTPADIX();
		return "???";
	}
	for (i = 0; i < len - slen; i++)
		pp[i] = '0';
	bcopy(buf, &pp[i], slen);
	pp[len] = 0;
	NEXTPADIX();
	return pp;
}

char *
padnum64(__int64_t num, int base, int len)
{
	char *pp = (char *)&pads[padix];
	char *f;
	char buf[MAXPADLEN];
	int slen;
	int i;

	if (len > MAXPADLEN)
		len = MAXPADLEN;
	
	switch (base) {
	case 8:
		f = "%llo";
		break;
	case 10:
		f = "%lld";
		break;
	default:
	case 16:
		f = "%llx";
	}
	sprintf(buf, f, num);
	slen = strlen(buf);
	if (slen > len) {
		NEXTPADIX();
		return "???";
	}
	for (i = 0; i < len - slen; i++)
		pp[i] = '0';
	bcopy(buf, &pp[i], slen);
	pp[len] = 0;
	NEXTPADIX();
	return pp;
}

/*
 * pad_utrace_qual() -- Pad a utrace qualifier as a string.
 */
static char *
pad_utrace_qual(__uint64_t qual)
{
	int     i;
	char    *ptr;

	/*
	 * If any byte is not printable, treat it as a number.
	 */
	for (i=0, ptr=(char*)&qual; i<sizeof(__uint64_t); ++i, ++ptr) {
		if (!isprint(*ptr))
			return padnum64(qual, 16, sizeof(__uint64_t)*2);
	}
	return _padstrn((char*)&qual, sizeof(__uint64_t), sizeof(__uint64_t)*2);
}



/*
 *	_mcount() is a dummy routine to allow kdbx to make procedure calls
 */

int
_mcount(void)
{
	return(0);
}

#if TRAPLOG_DEBUG && (_PAGESZ == 16384)
static __uint64_t traplog_dumpcnt = ((TRAPLOG_BUFEND - TRAPLOG_BUFSTART) / TRAPLOG_ENTRYSIZE);


typedef unsigned char byte;

/*
++
++	kp traplog [cpu #]
++		dumps out traplog from pda struct for each active processor
++
*/
/*ARGSUSED*/
void
idbg_traplog(int argc, char **argv)
{

#if defined (SN)
	qprintf("TRAPLOG  on  CPU %d PDA addr = 0x%x\n",cpuid(),PDAADDR);
	qprintf("KERNELSTACK=0x%x\n",KERNELSTACK);
	qprintf("PDAPAGE=0x%x\n",PDAPAGE);
	qprintf("PDAADDR=0x%x\n",PDAADDR);
	qprintf("PDASIZE=0x%x\n",PDASIZE);
	qprintf("KSTACKPAGE=0x%x\n",KSTACKPAGE);
	qprintf("PDAOFFSET=0x%x\n",PDAOFFSET);
	qprintf("PRDASIZE=0x%x\n",PRDASIZE);
	qprintf("sizeof pda = %d\n",sizeof (pda_t));
	qprintf("TRAPLOG_ENTRYSIZE=0x%x\n",TRAPLOG_ENTRYSIZE);
	qprintf("traplog_dumpcnt = %d\n",traplog_dumpcnt);
#endif

	dumptraplog(cpuid());

}

char *exception_code_ascii[32] = {
	/*  0 */ "interrupt",
	/*  1 */ "TLB mod",
	/*  2 */ "Read TLB Miss",
	/*  3 */ "Write TLB Miss",
	/*  4 */ "Read Address Error",
	/*  5 */ "Write Address Error",
	/*  6 */ "Instruction Bus Error",
	/*  7 */ "Data Bus Error",
	/*  8 */ "SYSCALL",
	/*  9 */ "BREAKpoint",
	/* 10 */ "Illegal Instruction",
	/* 11 */ "CoProcessor Unusable",
	/* 12 */ "OVerflow",
	/* 13 */ "Trap exception",
	/* 14 */ "reserved 14",
	/* 15 */ "Floating point exception",
	/* 16 */ "reserved 16",
	/* 17 */ "reserved 17",
	/* 18 */ "reserved 18",
	/* 19 */ "reserved 19",
	/* 20 */ "reserved 20",
	/* 21 */ "reserved 21",
	/* 22 */ "reserved 22",
	/* 23 */ "watch exception",
	/* 24 */ "reserved 24",
	/* 25 */ "reserved 25",
	/* 26 */ "reserved 26",
	/* 27 */ "reserved 27",
	/* 28 */ "reserved 28",
	/* 29 */ "reserved 29",
	/* 30 */ "reserved 30",
	/* 31 */ "reserved 31",
};

void
dumptraplog_entry( __uint64_t *entry)
{
	int cause;

	/*
	 * Note that on SN0 traplog only seems to work on CPU 0,
	 * this needs to be fixed
	 */

	qprintf("(0x%0x) ",*entry & 0xffff);

	switch (entry[TRAPLOG_CODE]) {
	case 1:
		qprintf("TLBrefl\n");
		break;
	case 2:				/* TFP only */
		qprintf("KTLBrfl\n");
		break;
	case 4:
		qprintf("EXCPTIN\n");
		break;
	case 5:
		qprintf("EX EXIT\n");
		break;
	case 6:
		qprintf("TLBrfl2\n");
		break;
	case 7:				/* TFP only */
		qprintf("EXCPTN2\n");
		break;
	default:
		qprintf("CODE 0x%x:\n", entry[TRAPLOG_CODE]);
	}

	qprintf("    EPC=0x%x BVADR=0x%x\n",
			entry[TRAPLOG_EPC],
			entry[TRAPLOG_BADVA]);

	cause = (entry[TRAPLOG_C0_CAUSE] >> CAUSE_EXCSHIFT) & 0x1f;

	qprintf("    C0_CAUSE=0x%x  [%d %s]\n",
			entry[TRAPLOG_C0_CAUSE],
			cause,
			exception_code_ascii[cause]);

	qprintf("    RA=0x%x SP=0x%x CURTH=0x%x\n",
			entry[TRAPLOG_RA],
			entry[TRAPLOG_SP],
			entry[TRAPLOG_VPDA_CURUTHREAD]);

#if defined (FULL_TRAPLOG_DEBUG)
	qprintf("    at=0x%x v0=0x%x v1=0x%x\n",
			entry[TRAPLOG_AT],
			entry[TRAPLOG_V0],
			entry[TRAPLOG_V1]);
	qprintf("    a0=0x%x a1=0x%x a2=0x%x a3=0x%x\n",
			entry[TRAPLOG_A0],
			entry[TRAPLOG_A1],
			entry[TRAPLOG_A2],
			entry[TRAPLOG_A3]);
	qprintf("    a4=0x%x a5=0x%x a6=0x%x a7=0x%x\n",
			entry[TRAPLOG_A4],
			entry[TRAPLOG_A5],
			entry[TRAPLOG_A6],
			entry[TRAPLOG_A7]);
	qprintf("    t0=0x%x t1=0x%x t2=0x%x t3=0x%x\n",
			entry[TRAPLOG_T0],
			entry[TRAPLOG_T1],
			entry[TRAPLOG_T2],
			entry[TRAPLOG_T3]);
	qprintf("    s0=0x%x s1=0x%x s2=0x%x s3=0x%x\n",
			entry[TRAPLOG_S0],
			entry[TRAPLOG_S1],
			entry[TRAPLOG_S2],
			entry[TRAPLOG_S3]);
	qprintf("    s4=0x%x s5=0x%x s6=0x%x s7=0x%x\n",
			entry[TRAPLOG_S4],
			entry[TRAPLOG_S5],
			entry[TRAPLOG_S6],
			entry[TRAPLOG_S7]);
	qprintf("    t8=0x%x t9=0x%x\n",
			entry[TRAPLOG_T8],
			entry[TRAPLOG_T9]);
	qprintf("    gp=0x%x fp=0x%x ra=0x%x\n",
			entry[TRAPLOG_GP],
			entry[TRAPLOG_FP],
			entry[TRAPLOG_RA]);

#if defined (TRAPLOG_STACK_TRACE_SZ)
	{
	int i;

	for (i = 0 ; i < (TRAPLOG_STACK_TRACE_SZ >> 3) ; i += 2) {

		qprintf("    stack @0x%x  0x%x  0x%x\n",
			entry[TRAPLOG_SP] + i,
			entry[TRAPLOG_STACK + i    ],
			entry[TRAPLOG_STACK + i + 1]);
			
	}

	}
#endif /* TRAPLOG_STACK_TRACE_SZ */

#endif /* FULL_TRAPLOG_DEBUG */
}

/*ARGSUSED*/
void
traplog_save_stack(__uint64_t sp,__uint64_t traplog_buf)
{
#if defined (FULL_TRAPLOG_DEBUG) && defined (TRAPLOG_STACK_TRACE_SZ)
 

	pfn_t pfn;
	__uint64_t addr;

        if (IS_KUSEG(sp)) {
                if (vtop((uvaddr_t)sp, 1, &pfn, 1) == NULL)
			return;

		addr = PHYS_TO_K0((__psunsigned_t)pfn << PNUMSHFT) +
				(sp & ((1<<PNUMSHFT) -1));

		bcopy((void *) addr,(void *) traplog_buf,TRAPLOG_STACK_TRACE_SZ);

	}
#endif /* FULL_TRAPLOG_DEBUG */
}



void
dumptraplog(cpuid_t cpu)
{
	int cnt;
	__uint64_t bufstart;

	bufstart = (__uint64_t) pdaindr[cpu].pda +
		   (TRAPLOG_BUFSTART & (NBPP-1));

	qprintf("TRAPLOG @0x%x\n",bufstart);

	for (cnt = 0 ; cnt < traplog_dumpcnt ; cnt++) 
		dumptraplog_entry((__uint64_t *) 
					(bufstart + 
					 (cnt * TRAPLOG_ENTRYSIZE)));
		
}
#endif /* TRAPLOG_DEBUG */



	    


#ifdef R10000
void
idbg_print_cpumon(cpu_mon_t *cmp)
{
	int i;

	qprintf(" cpumon 0x%x:", cmp);
	qprintf(" flags 0x%x gen 0x%x sig 0x%x cpu 0x%x\n", cmp->cm_flags,
		cmp->cm_gen, cmp->cm_sig, cmp->cm_counting_cpu);
	for(i = 0; i < cmp->cm_num_counters; i++) 
		qprintf(" cmp evn0 0x%x indx0 0x%x max0 0x%x\n",
			cmp->cm_events[i], cmp->cm_evindx[i], cmp->cm_evmax[i]);

	for (i = 0; i < cmp->cm_num_events; i++) {
		qprintf(" Ev %d: spec 0x%x cumu 0x%x saved 0x%x preld 0x%x\n",
			i, cmp->cm_evspec[i], cmp->cm_eventcnt[i],
			cmp->cm_savecnt[i], cmp->cm_preloadcnt[i]);
	}
}

void
idbg_cpumon(__psint_t x)
{
	proc_t *pp;
	cpu_mon_t *cmp = NULL;
	uthread_t *ut;

	if ((__psint_t) x == -1L) {
		ut = curuthread;

		if (ut == NULL) {
			qprintf("no current uthread\n");
			return;
		}
		if (!(cmp = ut->ut_cpumon)) {
			qprintf("no cpu mon area for current uthread\n");
			return;
		}
		qprintf("cpu mon area dump for current uthread\n");
	} else if (x < 0) {
		ut = (uthread_t *) x;
		if (!uthreadp_is_valid(ut)) {
			cmp = (cpu_mon_t *)x;
		} else if (!(cmp = ut->ut_cpumon)) {
			qprintf("no cpu mon area for uthread @ 0x%x\n", x);
			return;
		} else {
			qprintf("cpu mon area for uthread @ 0x%x\n", ut);
		}
	} else {
		if ((pp = idbg_pid_to_proc(x)) == NULL) {
			qprintf("%d is not an active pid\n", (pid_t)x);
			return;
		}

		qprintf("cpu mon areas for process pid %d:", x);

		for (ut = pp->p_proxy.prxy_threads; ut; ut = ut->ut_next) {
			if (ut->ut_cpumon) {
				qprintf(" uthread @ 0x%x:\n", ut);
				idbg_print_cpumon(ut->ut_cpumon);
			} else
				qprintf("uthread @ 0x%x: (NULL)\n",
					ut);
		}
		return;
	}
	
	if (cmp)
		idbg_print_cpumon(cmp);
}

void start_event_counters(int, short, short, short, short);
void read_event_counters(void);
void stop_event_counters(int);

void
start_event_counters(int mode, short event0, short event1, short event2, short event3)
{
	short		spec0, spec1, spec2, spec3;

	/* R5K does not have event counters so return */

	if (!IS_R10000()) {
		qprintf("Function not supported\n");
		return;
	}

	if ((event0 != -1) && ((event0 > HWPERF_MAXEVENT) || 
			(event0 < HWPERF_MINEVENT))) {
		qprintf("Invalid event(s) specified\n"); 
		return;
	}
	if ((event1 != -1) && ((event1 > HWPERF_MAXEVENT) || 
			(event1 < HWPERF_MINEVENT))) {
		qprintf("Invalid event(s) specified\n");
		return;
	}
	if (IS_R12000()) {
		if ((event2 != -1) && ((event2 > HWPERF_MAXEVENT) || 
			(event2 < HWPERF_MINEVENT))) {
			qprintf("Invalid event(s) specified\n"); 
			return;
		}
		if ((event3 != -1) && ((event3 > HWPERF_MAXEVENT) || 
			(event3 < HWPERF_MINEVENT))) {
			qprintf("Invalid event(s) specified\n");
			return;
		}
	}

	switch(mode) {

	case 1:
		qprintf("must be in kernel mode\n");
		return;
	case 2:
	default:
		mode = HWPERF_CNTEN_K;
		break;
	case 3:
		mode = HWPERF_CNTEN_U | HWPERF_CNTEN_K;
	}
	mode |= HWPERF_CNTEN_IE;

	spec0 = ((event0 << HWPERF_EVSHIFT) | mode);
	spec1 = ((event1 << HWPERF_EVSHIFT) | mode);

	if (IS_R12000()) {
		spec2 = ((event2 << HWPERF_EVSHIFT) | mode);
		spec3 = ((event3 << HWPERF_EVSHIFT) | mode);
	}
	
	if (IS_R12000()) {
		/* Initialize with  event specifier for counter0 */
		r1nkperf_data_register_set(0,0);/* set the counter0 to zero */
		r1nkperf_control_register_set(0,spec0);   

		/* Initialize with  event specifier for counter1 */
		r1nkperf_data_register_set(1,0); /* set the counter1 to zero */
		r1nkperf_control_register_set(1,spec1);   
		/* Initialize with  event specifier for counter0 */
		r1nkperf_data_register_set(2,0);/* set the counter0 to zero */
		r1nkperf_control_register_set(2,spec2);   

		/* Initialize with  event specifier for counter1 */
		r1nkperf_data_register_set(3,0); /* set the counter1 to zero */
		r1nkperf_control_register_set(3,spec3);   
	}
	else {
		/* Initialize with  event specifier for counter0 */
		r1nkperf_data_register_set(0,0);/* set the counter0 to zero */
		r1nkperf_control_register_set(0,spec0);   

		/* Initialize with  event specifier for counter1 */
		r1nkperf_data_register_set(1,0); /* set the counter1 to zero */
		r1nkperf_control_register_set(1,spec1);   
	}
}

/*
 * Just read the hardware counters- don't reset the count.
 */
void
read_event_counters(void)
{
	__uint64_t	count0, count1, count2, count3;
	uint		spec0, spec1, spec2, spec3;
	int		mode0, mode1, mode2, mode3;
	
	/* R5K does not have event counters so return */

	if (!IS_R10000()) {
		qprintf("Function not supported\n");
		return;
	}
	if (IS_R12000()) {
		spec0 = r1nkperf_control_register_get(0);
		spec1 = r1nkperf_control_register_get(1);
		spec2 = r1nkperf_control_register_get(2);
		spec3 = r1nkperf_control_register_get(3);
	}
	else {
		spec0  = r1nkperf_control_register_get(0);
		spec1  = r1nkperf_control_register_get(1);
	}

	if (IS_R12000()) {
		if ((spec0 == 0) && (spec1 == 0) && (spec2 == 0) && (spec3 == 0)) {
			qprintf("event counters not in use by system\n");
			return; 
		}
	}
	else {
		if ((spec0 == 0) && (spec1 == 0)) {
		qprintf("event counters not in use by system\n");
		return;
		}
	}

	mode0 = spec0 & (HWPERF_CNTEN_U | HWPERF_CNTEN_K);
	mode1 = spec1 & (HWPERF_CNTEN_U | HWPERF_CNTEN_K);
	if (IS_R12000()) {
		mode2 = spec2 & (HWPERF_CNTEN_U | HWPERF_CNTEN_K);
		mode3 = spec3 & (HWPERF_CNTEN_U | HWPERF_CNTEN_K);
	}

	if (IS_R12000()) {
		count0 = r1nkperf_data_register_get(0);
		count1 = r1nkperf_data_register_get(1);
		count2 = r1nkperf_data_register_get(2);
		count3 = r1nkperf_data_register_get(3);
	}
	else {
		count0 = r1nkperf_data_register_get(0);
		count1 = r1nkperf_data_register_get(1);
	}

	qprintf("counter 0: event %d, mode %s, count 0x%X\n", 
				HWPERF_EVENTSEL(spec0), 
	 (mode0 == HWPERF_CNTEN_U) ? "user" :
	 ((mode0 == HWPERF_CNTEN_K) ? "kernel" : 
	 (mode0) ? "user and kernel" : "not enabled"),
						count0);
	qprintf("counter 1: event %d, mode %s, count 0x%X\n", 
				HWPERF_EVENTSEL(spec1), 
	 (mode1== HWPERF_CNTEN_U) ? "user" :
	 ((mode1 == HWPERF_CNTEN_K) ? "kernel" : 
	 (mode1) ? "user and kernel" : "not enabled"),
						count1);
	if (IS_R12000()) {
		qprintf("counter 2: event %d, mode %s, count 0x%X\n", 
				HWPERF_EVENTSEL(spec0), 
		 (mode2 == HWPERF_CNTEN_U) ? "user" :
		 ((mode2 == HWPERF_CNTEN_K) ? "kernel" : 
		 (mode2) ? "user and kernel" : "not enabled"),
						count2);
		qprintf("counter 3: event %d, mode %s, count 0x%X\n", 
					HWPERF_EVENTSEL(spec1), 
		 (mode3== HWPERF_CNTEN_U) ? "user" :
		 ((mode3 == HWPERF_CNTEN_K) ? "kernel" : 
		 (mode3) ? "user and kernel" : "not enabled"),
	 					count3);
	}

}

/*
 * Stop one of both event counters- 
 *	
 *	0 ==> stop only counter 0
 *	1 ==> stop only counter 1
 *	2 ==> stop both counters
 *	
 *	For R12000
 *
 *	0 ==> stop only counter 0
 *	1 ==> stop only counter 1
 *	2 ==> stop only counter 2
 *	3 ==> stop only counter 3
 *	4 ==> stop all counters
 */
void
stop_event_counters(int counters)
{

	/* R5K does not have event counters so return */

	if (!IS_R10000()) {
		qprintf("Function not supported\n");
		return;
	}
	if (IS_R12000()) {
		switch (counters) {
		case 0:
			r1nkperf_control_register_set(0,0);
			break;
		case 1:
			r1nkperf_control_register_set(1,0);
			break;
		case 2:
			r1nkperf_control_register_set(2,0);
			break;
		case 3:
			r1nkperf_control_register_set(3,0);
			break;
		default:
			r1nkperf_control_register_set(0,0);
			r1nkperf_control_register_set(1,0);
			r1nkperf_control_register_set(2,0);
			r1nkperf_control_register_set(3,0);
			break;
		}
	}
	else {
		switch (counters) {
		case 0:
			r1nkperf_control_register_set(0,0);
			break;
		case 1:
			r1nkperf_control_register_set(1,0);
			break;
		default:
			r1nkperf_control_register_set(0,0);
			r1nkperf_control_register_set(1,0);
			break;
		}
	}
}

#if EVEREST || IP28 || IP30 || IP32 || SN
static void
idbg_cache_errors(void)
{
        extern void ecc_printPending(void (*)(char *, ...));

        ecc_printPending(qprintf);
}
#endif

#endif /* R10000 */
#ifdef ITHREAD_LATENCY
void
idbg_print_latstats(__psint_t x)
{
	kthread_t *kt;
	xt_latency_t *l;

	if (x == -1L) {
		kt = private.p_curkthread;
		if (kt == NULL || !KT_ISXTHREAD(kt)) {
			qprintf("no current xthread\n");
			return;
		}
	} else
		kt = (kthread_t *)x;

	if (KT_ISXTHREAD(kt))
		l = ((xthread_t *)kt)->xt_latstats;
	else
		return;

	xthread_print_latstats(l);
}

void
idbg_zero_latstats(__psint_t x)
{
	kthread_t *kt;
	xt_latency_t *l;

	if (x == -1L) {
		kt = private.p_curkthread;
		if (kt == NULL || !KT_ISXTHREAD(kt)) {
			qprintf("no current xthread\n");
			return;
		}
	} else
		kt = (kthread_t *)x;

	if (KT_ISXTHREAD(kt))
		l = ((xthread_t *)kt)->xt_latstats;
	else
		return;

	xthread_zero_latstats(l);
}
#endif /* ITHREAD_LATENCY */

/*
 * hack function to mess around with thread pri;
 * only works with user-level idbg
 */
/* ARGSUSED */
void
idbg_setpri(__psint_t arg0, char *arg1)
{
	kthread_t *kt;
	char *av[8];
	int ac = 8;
	unsigned int pri;

	if (idbginvoke != IDBGUSER) {
		qprintf("setpri only valid for user level idbg\n");
		return;
	}

	if (!arg1) {
		qprintf("usage: setpri kthread* pri\n");
		return;
	}
	
	strtoargs(arg1, &ac, av);
	if (ac == 0) {
		qprintf("usage: setpri kthread* pri\n");
		return;
	}
	kt = (kthread_t *)strtoull(av[0], (char **)NULL, 0);
	pri = (unsigned int)strtoull(av[1], (char **)NULL, 0);

	if (pri <= 255) {
		kt->k_basepri = kt->k_pri = pri;
	} else {
		qprintf("priority %d is out of range 0-255\n",pri);
	}
	return;
}


#include "net/soioctl.h"

#ifdef EVEREST
#define IFUNIT_NAME		"et0"
#include "bsd/misc/ether.h"
static int oldrcmd;
extern u_long ee_reconnect(struct etherif *, u_long);
extern u_long ee_disconnect(struct etherif *);
extern u_long ee_get_tbase(struct etherif *);
extern u_long ee_get_rbase(struct etherif *);
extern int ee_initialized;
#else
#define IFUNIT_NAME		"ec0"
#endif /* EVEREST */

#ifdef KDBX_DEBUG
#define kdprintf(x)  printf x
#else
#define kdprintf(x)
#endif

#ifdef DEBUG
extern int kdbx_on;
#endif

extern struct 	ifnet 	*ifunit(char *);

static kthread_t        *kthread_holder;
static kthread_t        fake_kthread;
#define REPLACE_KTHREAD private.p_curkthread = kthread_holder;

int
ec0_down(void)
{
	struct ifreq 		ifr;
	struct ifnet 		*ifp;
#if EVEREST && !SABLE
	struct etherif 		*eif;
#endif

#ifdef DEBUG
	kdbx_on = 1;
#endif

	kdprintf(("ec0_down => Enter\n"));

	/* since we're coming from symmon with don't have a valid kthread
	   save what we have (if it's anything) and then fake one so that
	   mutex_lock et el. will be happy */
	kthread_holder = private.p_curkthread;

	private.p_curkthread = &fake_kthread;
	
	ifp = ifunit(IFUNIT_NAME);
	if (ifp == 0) {
		printf("ec0_down: Cannot find ec0 interface\n");
		REPLACE_KTHREAD;
		return(2);
	}
	kdprintf(("ec0_down => got unit\n"));
#ifdef _MP_NETLOCKS
	kdprintf(("ec0_down => attempting lock\n"));
#ifdef DEBUG
	if (! kdbx_on)
#endif
		if (mutex_trylock(&ifp->if_mutex) == 0) {
		kdprintf(("****** ec0_down: Found iflocked\n"));
		REPLACE_KTHREAD;
		return 1;
	}
	/* IFNET_LOCK (ifp, s); */
	kdprintf(("ec0_down => lock successful\n"));
#endif /* _MP_NETLOCKS */


	/* 
	 *      Stop the ethernet driver, enter debugger,
	 *      bring it back up on re-entry.   
	 */

	if (ifp->if_ioctl) {
		kdprintf(("\n*** Stopping interface\n"));
		ifr.ifr_flags = ifp->if_flags;
		ifr.ifr_flags &= (~IFF_UP);
		(void) (*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, &ifr);
#if EVEREST && !SABLE
		eif = ifptoeif(ifp);
		oldrcmd = ee_disconnect(eif);
#ifdef DEBUG
		if (!kdbx_on)
#endif
			IFNET_UNLOCK (ifp);
		kdprintf(("Interface stopped.\n"));
		REPLACE_KTHREAD;
		return(ee_get_tbase(eif));
#else
#ifdef _MP_NETLOCKS
#ifdef DEBUG
		if (!kdbx_on)
#endif
			IFNET_UNLOCK (ifp);
#endif /* _MP_NETLOCKS */
		kdprintf(("Interface stopped.\n"));
		REPLACE_KTHREAD;
		return(1);
#endif /* !(EVEREST && !SABLE) */
	} else {
		printf("Warning! : No interace specific ioctl\n");
#ifdef _MP_NETLOCKS
#ifdef DEBUG
		if (!kdbx_on)
#endif
			IFNET_UNLOCK (ifp);
#endif
		REPLACE_KTHREAD;
		return(0);
	}
}

int
ec0_up(void)
{
	struct ifreq 		ifr;
	struct ifnet 		*ifp;
#if EVEREST && !SABLE
	struct etherif 		*eif;
#endif
#ifdef _MP_NETLOCKS
	int			locked = 1;
#endif

#ifdef EVEREST
	if (!ee_initialized)
		return 2;
#endif
	ifp = ifunit(IFUNIT_NAME);
	if (ifp == 0) {
		printf("ec0_up: Cannot find ec0 interface\n");
#ifdef DEBUG
		kdbx_on = 0;
#endif
		return(2);
	}

#ifdef _MP_NETLOCKS
#ifdef DEBUG
	if (! kdbx_on)
#endif
		if (mutex_trylock(&ifp->if_mutex) == 0) {
		/* printf("Warning! ec0_up: Found iflocked.\n"); */
		locked = 0;
		/* NOTREACHED */
	}
#endif
	if (ifp->if_ioctl) {

		kdprintf(("\n***Re-entry from debugger; starting interface\n"));
#if EVEREST && !SABLE
		eif = ifptoeif(ifp);
		ee_reconnect(eif, oldrcmd);  /* reset to old state */
#endif
		ifr.ifr_flags = ifp->if_flags;
		ifr.ifr_flags |= IFF_UP;
		(void) (*ifp->if_ioctl)(ifp, SIOCSIFFLAGS, &ifr);
		kdprintf(("Interface Started.\n"));
#ifdef _MP_NETLOCKS
		if (locked) {
#ifdef DEBUG
			if (! kdbx_on)
#endif
				IFNET_UNLOCK (ifp);
		}
#endif
#ifdef DEBUG
		kdbx_on = 0;
#endif
		return(1);
	} else  {
		printf("Warning! : No interace specific ioctl\n");
#ifdef _MP_NETLOCKS
		if (locked) {
#ifdef DEBUG
			if (! kdbx_on)
#endif
				IFNET_UNLOCK (ifp);
		}
#endif
#ifdef DEBUG
		kdbx_on = 0;
#endif
		return(0);
	}
}

int
getrbase(void)
{
#ifdef EVEREST
	struct ifnet 		*ifp;
	struct etherif 		*eif;

	ifp = ifunit(IFUNIT_NAME);
	if (ifp == 0) {
		printf("getrbase: Cannot find %s interface\n", IFUNIT_NAME);
		return(1);
	}
	eif = ifptoeif(ifp);
	return(ee_get_rbase(eif));
#else
	return(1); /* not needed, and not supported on other platforms */
#endif
}

void
idbg_etrace(__psint_t *x)
{
	eframe_t *ef = (eframe_t *)x;

        (void)btrace((unsigned *)ef->ef_epc, (unsigned *)ef->ef_sp,
                (unsigned *)ef->ef_ra, 24, (unsigned *)K0BASE, 
		private.p_curkthread, 0);	/* XXX */
        return;
}

#ifdef _STRQ_TRACING
char *tab_qflags[] = {
	"QENAB",	/* 0x01 */
	"QWANTR",	/* 0x02 */
	"QWANTW",		/* 0x04 */
	"QFULL",	/* 0x08 */
	"QREADR",	/* 0x10 */
	"QUSE",	/* 0x20 */
	"QNOENAB",	/* 0x40 */
	"QXX1",		/* 0x80 */
	"QBACK",	/* 0x100 */
	"QHLIST",	/* 0x200 */
	"QXX2",	/* 0x400 */
	"QTRC",	/* 0x800 */
	0
};
/*
 * Print a queue trace entry.
 */
static int
q_trace_pr_entry(ktrace_entry_t *ktep)
{
	if ((__psint_t)ktep->val[0] == 0)
		return 0;
	switch ((__psint_t)ktep->val[0]) {
	case QUEUE_KTRACE_PUTQ:
		qprintf("putq of 0x%x %d[%d], count %d flags 0x%x\n", 
			ktep->val[1], ktep->val[2], ktep->val[8],
			ktep->val[3], ktep->val[5]);
		break;
	case QUEUE_KTRACE_PUTBQ:
		qprintf("putbq of 0x%x %d[%d], count %d flags 0x%x\n",
			ktep->val[1], ktep->val[2], ktep->val[8],
			ktep->val[3], ktep->val[5]);
		break;
	case QUEUE_KTRACE_GETQ:
		qprintf("getq of 0x%x %d[%d], count %d flags 0x%x\n", 
			ktep->val[1], ktep->val[2], ktep->val[8],
			ktep->val[3], ktep->val[5]);
		break;
	case QUEUE_KTRACE_RMVQ:
		qprintf("rmvq of 0x%x %d[%d], count %d flags 0x%x\n", 
			ktep->val[1], ktep->val[2], ktep->val[8],
			ktep->val[3], ktep->val[5]);
		break;
	case QUEUE_KTRACE_INSQ:
		qprintf(
		       "insq of 0x%x %d[%d] after 0x%x, count %d flags 0x%x\n", 
			ktep->val[1], ktep->val[2], ktep->val[9],
			ktep->val[8], ktep->val[3], ktep->val[5]);
		break;
	case QUEUE_KTRACE_ADD1:
		qprintf("add bytes before count %d flags 0x%x\n", ktep->val[3], 
			ktep->val[5]);
		break;
	case QUEUE_KTRACE_ADD2:
		qprintf("add bytes after count %d flags 0x%x\n", ktep->val[3], 
			ktep->val[5]);
		break;
	case QUEUE_KTRACE_SUB1:
		qprintf("sub bytes before count %d flags 0x%x\n", ktep->val[3], 
			ktep->val[5]);
		break;
	case QUEUE_KTRACE_SUB2:
		qprintf("sub bytes after count %d flags 0x%x\n", ktep->val[3], 
			ktep->val[5]);
		break;
	case QUEUE_KTRACE_ZERO:
		qprintf("count zeroed %d flags 0x%x\n", ktep->val[3], 
			ktep->val[5]);
		break;
	default:
		qprintf("unknown qtrace record\n");
		return 1;
	}
	qprintf(" ra=0x%x cpu=%d thrd=0x%x ",
		(inst_t *)ktep->val[4], (__psint_t)ktep->val[6],
		(kthread_t *)ktep->val[7]);
	_printflags((__psunsigned_t)ktep->val[5], tab_qflags, "flag");
	return 1;
}

/*
 * Print out the trace buffer attached to the given vnode.
 */
void
idbg_qtrace(queue_t *q)
{
	ktrace_entry_t	*ktep;
	ktrace_snap_t	kts;

	if (q->q_trace == NULL) {
		qprintf("The queue trace buffer is not initialized\n");
		return;
	}

	qprintf("qtrace q 0x%x\n", q);
	ktep = ktrace_first(q->q_trace, &kts);
	while (ktep != NULL) {
		if (q_trace_pr_entry(ktep))
			qprintf("\n");
		ktep = ktrace_next(q->q_trace, &kts);
	}
}
#endif

void
idbg_tlbsync()
{
 	int i;
  	qprintf("tlbsync operation debug\n");
	for (i=0; i <maxcpus; i++) {
		if (pdaindr[i].CpuId != -1) {
	    
			if (pdaindr[i].pda->p_tlbflush_cnt)
				qprintf("PDA %d tlbflush_cnt %d\n", i,
					pdaindr[i].pda->p_tlbflush_cnt);
#if MP
			if (pdaindr[i].pda->p_actionlist.todolist)
				qprintf("PDA %d actionlist 0x%x\n", i,
					pdaindr[i].pda->p_actionlist.todolist);
#endif /* MP */
			if (pdaindr[i].pda->p_tlbsync_asgen)
				qprintf("PDA %d tlbsync_asgen 0x%llx\n", i,
					pdaindr[i].pda->p_tlbsync_asgen);
		}
	}
}

#if EVEREST || SN0
void
idbg_nmidump(int cmd)
{
	extern void	adjust_nmi_handler(int);

	adjust_nmi_handler(cmd);
}
#endif

#ifdef MH_R10000_SPECULATION_WAR
/* vfntopfn(), without the call to bad_vfn() */
#define vfntopfn_noerr(x)	(IS_SMALLVFN_K0(x) ?	\
				    ((x)-SMALLMEM_VFNMIN_K0) : \
				(IS_SMALLVFN_K1(x) ? \
				    ((x)-SMALLMEM_VFNMIN_K1) : \
				(IS_K2VFN(x) ? \
					kptbl[(x)-K2MEM_VFNMIN].pte.pg_pfn : \
				(0))))
#define kvatopfn_noerr(x)	vfntopfn_noerr(vatovfn(x))
extern int is_extk0_pfn(uint);
#endif

/*
 * attempt to verify that a given pointer
 * is indeed a pointer to an allocated proc or uthread struct.
 */
int
uthreadp_is_valid(uthread_t *ut)
{
	/* ensure that we can dereference the pointer */

	if (!IS_KSEG0(ut)
#ifdef MH_R10000_SPECULATION_WAR
			&& !is_extk0_pfn(kvatopfn_noerr(ut))
#endif
	)
		return 0;
	if (CKPHYSPNUM(pnum(svirtophys(ut)))
#ifdef MH_R10000_SPECULATION_WAR
			&& !is_extk0_pfn(kvatopfn_noerr(ut))
#endif
	)
		return 0;
	/* ... and the proc pointer */
	return(procp_is_valid(UT_TO_PROC(ut)));
}

int
procp_is_valid(proc_t *p)
{
	/* ensure that we can dereference the pointer. */

	if (!IS_KSEG0(p)
#ifdef MH_R10000_SPECULATION_WAR
		&& !is_extk0_pfn(kvatopfn_noerr(p))
#endif
	)
		return 0;
	if (CKPHYSPNUM(pnum(svirtophys(p)))
#ifdef MH_R10000_SPECULATION_WAR
			&& !is_extk0_pfn(kvatopfn_noerr(p))
#endif
	)
		return 0;
	if (idbg_pid_to_proc(p->p_pid) == p)
		return 1;
	return 0;
}


#if defined(DEBUG) || defined(DEBUG_CONPOLL)
/*
 * usage: conpoll [0|1]
 */
void
idbg_conpoll(int x)
{
	/* if no args then toggle dbg console poll */
	if (x == -1) {
	    if (dbgconpoll)
		x = 0;
	    else
		x = 1;
	}
	qprintf("conpoll: %s debugger console poll\n",
		x ? "enabling" : "disabling");
	if (x) {
	    startprfclk();
	    dbgconpoll = 1;
	} else {
	    stopprfclk();
	    dbgconpoll = 0;
	}
}
#endif /* DEBUG || DEBUG_CONPOLL */


extern __int32_t _ftext[];
extern __int32_t _etext[];


#if SN
/* ARGSUSED */
void
idbg_patch_instruction(__int32_t *p, __psint_t a2, int argc, char **argv)
{
	nasid_t		nasid;
	int		cnode;
	__psint_t	offset;
	__uint32_t	ins, newins, *ip;

	if (p < _ftext || p >= _etext) {
		qprintf("invalid address - not in text region\n");
		return;
	}

	ins = *p;
	if (argc <= 1) {
		qprintf("  ins 0x%x: 0x%08x\n", p, ins);
	} else {
		newins = (__uint32_t)atoi(argv[1]);
		qprintf("  ins 0x%x: 0x%08x. Changed to 0x%08x\n", p, ins, newins);
		offset = ((__psint_t)p & TO_PHYS_MASK);
		for(cnode = 0; cnode < numnodes; cnode++) {
			nasid = COMPACT_TO_NASID_NODEID(cnode);
			ip = (__uint32_t*)PHYS_TO_K0(TO_NODE(nasid, offset));
			if (*ip != ins)
				qprintf("  BUG!! K2 did not match K0: K0 0x%x. Found 0x%x, expected 0x%x\n",
					ip, *ip, ins);
			else
				*ip = newins;
		}
	}

}
#endif /* SN */

/*
 * Debugging aid. Used to count the number of occurrences of interesting
 * events in the kernel.
 * Insert calls to this routine via
 *	idbg_dstat(<string>);
 *
 * To print the statistics:
 *	dstats
 *
 * To print statistics & zero them out,
 *	dstats 0
 */

#define DSTATMAX	64
char	*dstatname[3*DSTATMAX];
int	dstatcnt[MAXCPUS][DSTATMAX];

void 
idbg_dstat(char *s)
{
	int	i;
	char	*p;

  again:
	for (i=0; i<DSTATMAX-1; i++) {
		p = dstatname[i];
		if (s == p)
			break;
		if (p)
			continue;
		compare_and_swap_ptr((void**)&dstatname[i], 0, s);
		goto again;
	}
	dstatcnt[cpuid()][i]++;
}

void
idbg_dstats(uint64_t flag)
{
	int	imax, cpu, i, j, k, sum;
	int	dir[DSTATMAX];

	for (imax=0; imax<DSTATMAX && dstatname[imax]; imax++)
		dir[imax] = imax;

	for (i=0; i<imax; i++)
		for (j=i; j<imax; j++)
			if (strcmp(dstatname[dir[i]], dstatname[dir[j]]) > 0) {
				k = dir[i];
				dir[i] = dir[j];
				dir[j] = k;
			}

	for (i=0; i<imax; i++) {
		j = dir[i];
		for (sum=0, cpu=0; cpu<MAXCPUS; cpu++)
			sum += dstatcnt[cpu][j];
		if (sum)
			qprintf("%10d  %s\n", sum, dstatname[j]);
	}

	if (flag == 0)
		for (i=0; i<DSTATMAX; i++)
			for (cpu=0; cpu<MAXCPUS; cpu++)
				dstatcnt[cpu][i] = 0;
}

/*
 * idbg_retarget
 * This routine will scan all xthreads & sthreads. Any
 * thread that has a k_mustrun within the specified range
 * of cpus will be reassign to the new range of cus.
 *
 * NOTE: experimental use only!!!
 *
 * Example:
 *	retarget 96 127 32
 *  will change mustrun of 96->32, 97->33, .. 127->63
 */

/* ARGSUSED */
void
idbg_retarget(void *p, void *a2, int argc, char **argv)
{
        xthread_t	*xt;
        sthread_t	*st;
	kthread_t	*kt, *ukt=NULL;
	extern sthread_t sthreadlist;
	extern xthread_t xthreadlist;
	int		cpu, locpu, hicpu, locpunu, hicpunu;

	if (argc == 2) {
		ukt = (kthread_t*)p;
		hicpunu = locpunu = atoi(argv[1]);
	} else if (argc == 4) {
		locpu = atoi(argv[0]);
		hicpu = atoi(argv[1]);
		locpunu = atoi(argv[2]);
		hicpunu = atoi(argv[3]);
	} else {
		qprintf("retarget <locpu> <hicpu> <new_locpu> <new_hicpu>\n");
		qprintf("   OR\n");
		qprintf("retarget <kthread> <new_cpu>\n");
		return;
	}


	if (locpu > hicpu || locpunu > hicpunu || hicpu < 0 || hicpunu < 0 ||
			locpu >= maxcpus || locpunu >= maxcpus) {
		qprintf("retarget <locpu> <hicpu> <cpulonu> <hicpunu>\n");
		return;
	}


        for (xt = xthreadlist.xt_next; xt != &xthreadlist; xt = xt->xt_next) {
		kt = XT_TO_KT(xt);
                cpu = kt->k_mustrun;
		if (ukt == NULL && (cpu < locpu || cpu > hicpu) ||
		    ukt != NULL && (ukt != kt))
			continue;

		kt->k_mustrun = locpunu + (cpu - locpu) % (hicpunu - locpunu + 1);
		qprintf("  retarget XT 0x%x [%s] from cpu %d to cpu %d\n", 
			kt, xt->xt_name, cpu, kt->k_mustrun);
        }

        for (st = sthreadlist.st_next; st != &sthreadlist; st = st->st_next) {
		kt = ST_TO_KT(st);
                cpu = kt->k_mustrun;
		if (ukt == NULL && (cpu < locpu || cpu > hicpu) ||
		    ukt != NULL && (ukt != kt))
			continue;

		kt->k_mustrun = locpunu + (cpu - locpu) % (hicpunu - locpunu + 1);
		qprintf("  retarget ST 0x%x [%s] from cpu %d to cpu %d\n", 
			kt, st->st_name, cpu, kt->k_mustrun);
        }
}

