/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1994 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ifndef __SYS_SYSTM_H__
#define __SYS_SYSTM_H__
#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 3.196 $"

#if DISCONTIG_PHYSMEM

int ckphyspnum(pfn_t);

#define CKPHYS(addr)	(CKPHYSPNUM(btoct(addr)))
#define CKPHYSPNUM(pfn)	ckphyspnum(pfn)

#else

#define CKPHYS(addr) (((ulong)(addr) >= ((ulong)ctob(maxclick+1))) || \
		      ((ulong)(addr) < ((ulong)ctob(pmem_getfirstclick()))) || \
		      ((ulong)(addr) >= kpbase && \
				(PG_ISHOLE(pfntopfdat(btoc(addr))))))
#define CKPHYSPNUM(pfn)	(((ulong)(pfn) > (ulong)maxclick) || \
			 ((ulong)(pfn) < (ulong)(pmem_getfirstclick())) || \
			  (ctob(pfn) >= kpbase && \
				(PG_ISHOLE(pfntopfdat(pfn)))))

#endif

#include "reg.h"	/* for eframe_t */
#include	<stdarg.h>

/*
 * Variables and function prototypes
 */

extern short	cputype;	/* type of cpu = 40, 45, 70, 780, 0x3b5 */

extern short	dbgconpoll;	/* check console for debugger break */
extern short	kdebug;		/* is kernel debugger active */
extern time_t	lbolt;		/* time in HZ since last boot */
extern time_t	time;		/* time in sec from 1970 */
extern lock_t	timelock;	/* lock for time variables */

extern pfn_t	maxmem;		/* max available memory (clicks) */
extern pfn_t	physmem;	/* physical memory (clicks) on this CPU */
extern pfn_t	maxclick;	/* Highest physical click. */
extern daddr_t	swplo;		/* block number of start of swap space */
extern int	nswap;		/* size of swap space in blocks*/
extern dev_t	rootdev;	/* device of the root */
extern dev_t	swapdev;	/* swapping device */
extern dev_t	dumpdev;	/* dump device */
extern int	dumplo;		/* location on dump device where dump starts */
extern char	*panicstr;	/* panic string pointer */
extern int	showconfig;
extern int	restricted_chown; /* 1 ==> restrictive chown(2) semantics */
extern int	cachewrback;	/* writeback cache flag */

struct eframe_s;
struct tune;
struct vnode;
struct proc;
struct pfdat;
struct callout;

extern void	kern_free(void *);
extern void	*kern_calloc(size_t, size_t);
extern void	*kern_malloc(size_t);
extern void	*kern_realloc(void *, size_t);

extern void	*kern_calloc_node(size_t, size_t, cnodeid_t);
extern void	*kern_malloc_node(size_t, cnodeid_t);

extern void	*low_mem_alloc(int, caddr_t*, char*);

extern int	sleep(void *, int);
extern void	wakeup(void *);

#if defined(_COMPILER_VERSION) && (_COMPILER_VERSION>=700)
#define SYNCHRONIZE()	__synchronize()
#define INLINE		__inline
#else
#define SYNCHRONIZE()
#define INLINE
#endif

#define disableintr()	splhi()
#define enableintr(s)	splx(s)

extern int	splhi(void);
extern int	spl0(void);
extern int	spl1(void);
extern int	spl2(void);
extern int	spl3(void);
extern int	spl4(void);
extern int	spl5(void);
extern int	spl6(void);
extern int	spl65(void);
extern int	spl7(void);
extern int	splvme(void);
extern int	splnet(void);
extern int	spltty(void);
extern int	splimp(void);
extern int	splgio1(void);
extern int	splprof(void);
extern int	splecc(void);
extern int	splerr(void);
extern int	splhi_relnet(void);
extern void	splx(int);
extern void	splxecc(int);
extern void	setsr(ulong);
extern ulong	getsr(void);
extern void	setcause(uint);
extern uint	getcause(void);
extern void	*getbadvaddr(void);
#ifndef _STANDALONE
/* avoid conflict with libsc */
extern void	*getpc(void);
#endif
extern void	*getsp(void);
extern void	getpcsp(__psunsigned_t *, __psunsigned_t *);
extern uint	get_r4k_counter(void);
extern void	delay_for_intr(void);
extern int	issplhi(uint);
extern int	isspl0(uint);
extern int	issplprof(uint);
extern int	isspl7(uint);
extern void	siron(uint);
extern void	siroff(uint);
extern void	splset(int);
#ifdef SPLMETER
extern uint	raisespl(inst_t *, uint);
extern void	lowerspl(int, inst_t *);
extern int	_splhi(inst_t *);
extern void	__splx(int);
extern int	__spl0(void);
extern void	_cancelsplhi(void);
extern void	store_if_greater(int *, int);
#endif

#if !_STANDALONE
extern void	clean_icache(void *, unsigned int, unsigned int, unsigned int);
extern void	clean_dcache(void *, unsigned int, unsigned int, unsigned int);
#endif /* _STANDALONE */
extern void	cacheinit(void);
extern void	bclean_caches(void *, unsigned int, unsigned int, unsigned int);
extern void	_bclean_caches(void *, unsigned int, unsigned int, unsigned int);
extern void	flush_cache(void);
extern void	wbflush(void);
extern uint	getcachesz(cpuid_t);

#ifndef icache_inval
extern void	icache_inval(void *, unsigned int);
#endif
extern void	cache_operation(void *, unsigned int, unsigned int);
extern void 	cache_operation_pfnlist(pfn_t *pfn_list, int pfn_count, unsigned int flags);
extern void	clean_isolated_icache(cpumask_t);
extern int	sync_dcaches(void *, unsigned int, pfn_t, unsigned int);
extern int	sync_dcaches_excl(void *, unsigned int, pfn_t, unsigned int);

#define	dcache_wb(X,Y)		\
		cache_operation(X,Y,CACH_DCACHE|CACH_INVAL|CACH_WBACK|CACH_IO_COHERENCY)
#define	data_cache_wb(addr, len, flags)				\
		cache_operation(addr, len,			\
			CACH_DCACHE|CACH_WBACK|((flags) & ~CACH_OPMASK))
#define	data_cache_inval(addr, len, flags)			\
		cache_operation(addr, len,			\
			CACH_DCACHE|CACH_WBACK|CACH_INVAL|((flags) & ~CACH_OPMASK))
#define	data_cache_wbinval(addr, len, flags)			\
		cache_operation(addr, len,			\
			CACH_DCACHE|CACH_WBACK|CACH_INVAL|((flags) & ~CACH_OPMASK))

#define	inst_cache_inval(addr, len, flags)			\
		cache_operation(addr, len,			\
			CACH_ICACHE|CACH_INVAL|((flags) & ~CACH_OPMASK))

extern void	dki_dcache_wb(void *, int);
extern void	dki_dcache_inval(void *, int);
extern void	dki_dcache_wbinval(void *, int);

extern void	xdki_dcache_inval_readonly(void *, int);
extern void	xdki_dcache_wbinval_readonly(void *, int);
extern void	xdki_dcache_validate(void *, int);
extern void	xdki_dcache_validate_readonly(void *, int);

extern int	badaddr(volatile void *, int);
extern int	badaddr_val(volatile void *, int, volatile void *);
extern int	wbadaddr(volatile void *, int);
extern int	wbadaddr_val(volatile void *, int, volatile void *);

extern void	hwcpin(volatile void *, void *, int);
extern void	hwcpout(void *, volatile void *, int);

extern int	copyinstr(char *, char *, size_t, size_t *);
extern int	copystr(char *, char *, size_t, size_t *);
extern int	upath(char *, char *, size_t);
extern int	bcmp(const void *, const void *, size_t);
extern void	bcopy(const void *, void *, size_t);
extern void	swbcopy(const void *, void *, int);
extern void	ovbcopy(const void *, void *, size_t);
extern void	bzero(void *, size_t);
extern int	uzero(void *, int);
extern int	gencopy(void *, void *, int, int);

extern int	copyout(void *,void *, int);
extern int	swcopyout(void *,void *, int);
extern int	swcopyin(void *,void *, int);
extern int	copyin(void *, void *, int);
extern __int64_t	fulong(void *);
extern int	fuword(void *);
extern int	fuiword(void *);
extern int	fuibyte(void *);
extern int	fubyte(void *);
extern int	suword(void *, uint);
extern int	suiword(void *, uint);
extern int	suhalf(void *, ushort);
extern int	subyte(void *, unchar);
extern int	sfu32(void *, void *);
extern int	spu32(void *, void *, uint);
extern int	sfu32v(void *, void *, void *, uint);
extern __int64_t	sulong(void *, __int64_t);
extern int	fkiword(void *);

/* printing/error routines */
extern void	cmn_err(int, char *, ...);
#pragma mips_frequency_hint NEVER cmn_err
extern void	panic(char *, ...);
#pragma mips_frequency_hint NEVER panic
#if !_STANDALONE
extern void	aptoargs(char *, va_list, __psint_t[]);
extern void	sprintf(char *, char *, ...);
extern void	vsprintf(char *, char *, va_list);
extern void	lo_sprintf(char *, char *, ...);
extern void	printf(char *, ...);
extern void	sync_printf(char *, ...);
extern void	printputbuf(int, void (*)(char *, ...));
extern void	printregs(eframe_t *, void (*)(char *, ...));
extern void	dprintf(char *, ...);

#endif /* !_STANDALONE */

extern void	dri_printf(char *, ...);
extern void	prdev(char *, int, ...);	/* 2nd arg is really dev_t */
extern void	panic(char *, ...);
extern void	conbuf_flush(int);
extern char	*stdname(char *, char *, int, int, int);
extern void	prom_reboot(void);
extern void	nomemmsg(char *);
extern void	_assfail(char *, char *, int);

/* miscellaneous homeless routines */
extern void	exit(int, int);
extern void	force_resched(void);
extern void	flushicache(void);
extern int	min(int, int);
extern int	max(int, int);
extern int	nodev(void);
extern int	nulldev(void);
extern int	getudev(void);
extern int	nosys(void);
extern int	nopkg(void);
extern void	noreach(void);
extern int	nullsys(void);
#if defined(_KERNEL) && !defined(_STANDALONE)
extern int	numtos(int, char *);
extern long	atoi(char *);
extern unsigned long long strtoull(const char *, char **, int);
extern unsigned long random(void);
#endif
extern cell_t	cellid(void);
extern bitlen_t	bftstset(char *, bitnum_t, bitlen_t);
extern bitlen_t	bftstclr(char *, bitnum_t, bitlen_t);
extern void	bset(char *, bitnum_t);
extern void	bclr(char *, bitnum_t);
extern int	btst(char *, bitnum_t);
extern void	bfset(char *, bitnum_t, bitlen_t);
extern void	bfclr(char *, bitnum_t, bitlen_t);
extern bitlen_t	bbftstset(char *, bitnum_t, bitlen_t);
extern bitlen_t	bbftstclr(char *, bitnum_t, bitlen_t);
extern void	bbset(char *, bitnum_t);
extern void	bbclr(char *, bitnum_t);
extern int	bbtst(char *, bitnum_t);
extern void	bbfset(char *, bitnum_t, bitlen_t);
extern void	bbfclr(char *, bitnum_t, bitlen_t);
extern bitlen_t bfcount(char *, bitnum_t, bitlen_t);
extern void	bset_atomic(unsigned int *, bitnum_t);
extern void	bclr_atomic(unsigned int *, bitnum_t);
extern void	debug(char *);
extern int	grow(caddr_t);
extern int	is_branch(inst_t);
extern int	is_load(inst_t);
extern int	is_store(inst_t);
extern int	iointr_at_cpu(int, int, int);
extern int	is_kmem_space(void *, unsigned long);
extern int	cpu_isvalid(int);
extern int	nfs_cnvt(void *, int, int *);
extern int	nfs_cnvt3(void *, int, int *);
extern void	softfp_psignal(int, int, struct eframe_s *);
extern void	signal(pid_t, int);
extern void	hold_nonfatalsig(k_sigset_t *);
extern void	release_nonfatalsig(k_sigset_t *);
extern void	_hook_exceptions(void);
extern void	allowintrs(void);
extern void	reset_leds(void);

extern int	readadapters(uint);
extern int	tune_sanity(struct tune *);
extern int	getsysid(char *);
extern pfn_t	pagecoloralign(pfn_t, __psunsigned_t);
extern pfn_t	pmem_getfirstclick(void);
extern pfn_t	node_getfirstfree(cnodeid_t);
extern pfn_t	node_getmaxclick(cnodeid_t);
#if DISCONTIG_PHYSMEM
extern pfn_t	slot_getsize(cnodeid_t, int);
#endif
extern pfn_t	setupbadmem(struct pfdat *, pfn_t, pfn_t);
extern struct pfdat	*pfdat_probe(struct pfdat *, int *);

extern void	uidacthash_init(int);
extern int	uidact_incr(uid_t);
extern void	uidact_decr(uid_t);
extern void	uidact_switch(uid_t, uid_t);

extern void	remapf(struct vnode *, off_t, int);
extern int	cn_link(struct vnode *, int);
extern void	cn_init(int, int);
extern int	cn_is_inited(void);

extern int	intr(struct eframe_s *, uint, uint, uint);
extern k_machreg_t ldst_addr(struct eframe_s *);
extern k_machreg_t store_data(struct eframe_s *);
extern void	buserror_intr(struct eframe_s *);
extern int	dobuserre(struct eframe_s *, inst_t *, uint);
extern int	dobuserr(struct eframe_s *, inst_t *, uint);
extern void	idle_err(inst_t *, uint, void *, void *);
extern void	*get_badvaddr(void);
extern k_machreg_t	emulate_branch(struct eframe_s *, inst_t, __int32_t, int *);
extern void	machine_error_dump(char *);
extern void	init_mfhi_war(void);
extern int	sizememaccess(struct eframe_s *, int *);
extern int	ktlbfix(caddr_t);
extern int	earlynofault(struct eframe_s *, uint);
extern int	cpuboard(void);
extern void	mlreset(int);
extern void	mdboot(int, char *);
extern int	findcpufreq(void);
extern int	findcpufreq_raw(void);
extern void	cache_preempt_limit(void);
extern void	coproc_find(void);
extern void	set_leds(int);
extern void	flushbus(void);
extern int	bad_badva(eframe_t *);
extern void	kicksched(void);

/* textport to keyboard driver interface */
extern void	gl_setporthandler(int, int (*fp)(unsigned char));
extern int	du_keyboard_port(void);
extern int	du_getchar(int);
extern void	du_putchar(int, unsigned char);
extern void	du_conpoll(void);
extern void	du_init(void);

/* timers */
extern void	clkstart(void);
extern void	clkset(time_t);
extern void	clkreld(void);
extern struct kthread *idle(void);
extern void	startrtclock(void);
extern void	stopclocks(void);
extern void	startkgclock(void);
extern void	slowkgclock(void);
extern toid_t	fast_timeout(void (*)(), void *, long , ...);
extern toid_t	fast_prtimeout(processorid_t, void (*)(), void *, long , ...);
extern toid_t	fast_itimeout(void (*)(), void *, long, pl_t, ...);
extern toid_t	fast_itimeout_nothrd(void (*)(), void *, long, pl_t, ...);
extern toid_t	clock_prtimeout(processorid_t, void (*fun)(),
				void *, __int64_t, int,  ...);
extern toid_t	clock_prtimeout_nothrd(processorid_t, void (*fun)(),
				       void *, __int64_t, ...);
extern int	untimeout(toid_t);
extern int	untimeout_wait(toid_t);
extern void	timein_entry(struct callout *);

extern toid_t	timeout(void (*)(), void *, long, ...);
extern toid_t	timeout_pri(void (*)(), void *, long, int, ...);
extern toid_t	timeout_nothrd(void (*)(), void *, long, ...);
extern toid_t	prtimeout(processorid_t, void (*)(), void *, long, ...);
#if RTINT_WAR
extern toid_t	prtimeout_nothrd(processorid_t, void (*)(), void *, long, ...);
#endif
extern toid_t	itimeout(void (*)(), void *, long, pl_t, ...);
extern toid_t	itimeout_nothrd(void (*)(), void *, long, pl_t, ...);
extern void     migrate_timeouts(processorid_t, processorid_t);
extern void	us_delay(uint);
extern void	us_delaybus(uint);
extern void	clr_r4kcount_intr(void);
extern void	delay(long);
extern __psint_t getgp(void);
extern void	inittodr(time_t);
extern void	resettodr(void);
extern int	rtodc(void);
extern void	wtodc(void);
extern		toid_t dotimeout(processorid_t, void (*)(), void *, __int64_t,
				int, long, va_list ap);
extern dev_t	blocktochar(dev_t);
extern dev_t	chartoblock(dev_t);

extern char	get_current_abi(void);

#if JUMP_WAR
extern void clr_jump_war_wired(void);
extern int sexc_eop(struct eframe_s *, uint, int *);
extern void setwired(int);
extern int getwired(void);
#endif 

#define	cputimeout	timeout

/* Flags for add_exit_callback and uthread_add_exit_callback */
#define ADDEXIT_CHECK_DUPS	0x1	/* Check if function already in list */
#define ADDEXIT_REMOVE		0x2	/* Remove instead of add */
extern int	add_exit_callback(pid_t, int, void (*)(void *), void *);
extern int	uthread_add_exit_callback(int, void (*)(void *), void *);

/*
 * Structure of the return-value parameter passed by reference to
 * system entries.
 */
union rval {
	struct	{
		__int64_t	r_v1;
		__int64_t	r_v2;
	} r_v;
	off_t			r_off;
	struct {
		int		r_pad0;
		time_t		r_tm;
	} r_t;
};
#define r_val1	r_v.r_v1
#define r_val2	r_v.r_v2
#define r_time  r_t.r_tm
	
typedef union rval rval_t;

#ifdef __cplusplus
}
#endif

#endif /* __SYS_SYSTM_H__ */
