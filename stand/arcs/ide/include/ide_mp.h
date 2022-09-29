#ifndef __IDE_MP_H__
#define __IDE_MP_H__

/*
 * ide_mp.h - ide-specific mp definitions
 *
 * $Revision: 1.21 $
 */

#if _LANGUAGE_C
#include <sys/types.h>
#include <sys/systm.h>
#include <stringlist.h>
#endif /* _LANGUAGE_C */

/* 
 * genpda.h evaluates MULTIPROCESSOR; ensure it's been included in 
 * whatever .c file we're included in.
 */
#ifndef __GENPDA_H__
  dontcompile("#include ERROR, ide_mp.h: genpda.h hasn't been included!")
#endif

/*
 * define structs and values to interrelate the following processor terms:
 *
 *   virtual ids (vid): [0..ActiveProcessors-1]
 *	- contiguous, sequential numbering assigned during system init.
 *   physical indices (phys_index): [0..MAXCPUS]
 *     - hardware-determined ordering, readable at runtime.  Used only
 *       as a guaranteed-unique index to fetch assigned vid
 * EVEREST specific:
 *   slot  - (Everest) [0..EV_BOARD_MAX]:
 *     - backplane slot board is occupying (hw-numbered)
 *   slice - (Everest) [0..EV_MAX_CPUS_BOARD]:
 *     - position on cpu board of specific processor (hw-numbered)
 *   physical indices are derived from <slot,slice> pairs
 *
 * In order to allow runtime instead of compile-time decision making 
 * (due to architectural differences) set up dummy structs for 
 * inapplicable concepts and initialize them with values that make
 * everything work anyway.
 */
 
#if EVEREST
#define _SLOTS	EV_BOARD_MAX		/* slots in Everest backplane */
#define _SLICES	EV_MAX_CPUS_BOARD	/* max cpus per processor bd */
#else	/* !EVEREST */
#define _SLOTS	1
#define _SLICES	1
#endif	/* EVEREST */

/*
 * XXXX For all R4K-based architectures, the master and slave SRs 
 * should really have SR_KX & SR_FR set, but one/both of these
 * caused problems (FPE exceptions) before 5.0.1, so I nuked 'em.
 * They'll be required for 6.0. 
 */
#if _MIPS_SIM == _ABI64
#define IDE_SLAVE_SR	SR_KX
#else
#define IDE_SLAVE_SR	0
#endif

/* ide-specific pda */
#define IDE_PDASIZE	512	/* size in bytes of ide's PDA */
#define IDE_PDASIZE_2	9	/* log2 of IDE_PDASIZE */

/* values of empty slots: SP can't have any bad cpus! */
#if MULTIPROCESSOR
#define BAD_CPU		0xDEADBEEF   /* disabled cpu: failed power-on diags */
#endif

#define MTY		(-1)		/* slot or slice has no cpu in it. */ 
#define NO_VID		(-1)		/* potential talking_vid value */

#if !defined(LANGUAGE_ASSEMBLY)

/* 
 * each diagnostic has <passcnt,failcnt,skipcnt> triple per cpu.  A back-ptr to 
 * the diag's symbol makes flexible log-dumping easy.
 */
typedef struct mp_logrec_s {
	sym_t *diagsym;
	unsigned pass_tot;
	unsigned fail_tot;
	unsigned skip_tot;
	unsigned passcnt[MAXCPU];
	unsigned failcnt[MAXCPU];
	unsigned skipcnt[MAXCPU];
} mp_logrec_t;
extern mp_logrec_t statlog[MAXDIAG];
extern int diag_tally;

extern void	ide_mpsetup(void);

extern int      dump_cpu_info(void);
extern int      ide_whoisthat(int, char *);
extern int      ide_whoami(char *);

/*
 * the following arrays convert between different methods of processor
 * numbering.  Some of these are redundant but convenient.
 */
extern int	vid_to_phys_idx[];	/* vid --> phys_index */
extern int	phys_idx_to_vid[];	/* phys_index --> vid */
extern int	vid_to_slot[];		/* vid --> slot */
extern int	vid_to_slice[];		/* vid --> slice */
extern cpuid_t	ss_to_vid[_SLOTS][_SLICES];	/* slot/slice --> vid */

extern struct string_list	argv_str_tab[];
extern struct string_list	environ_str_tab[];
extern void	ide_initargv(int *, char ***, struct string_list *);

/* <slot,slice> --> phys_index */
#define SS_TO_PHYS(_slot, _slice)       ((_slot << 2) + _slice)

typedef volatile unsigned long volul_t;
typedef volatile long voll_t;

/* kludge typedef for fns invoked by launching code */
typedef volatile void *volvp;


#ifdef USE_THE_LEDS
> /* 
>  * The master issues commands to slaves, and the slaves ACK those
>  * commands with a response.  The inconvenient values below were
>  * mandated by the shortage of LEDs on Everests (6).
>  * These bits are split 3 and 3, with the top three encoded to
>  * yield 8 commands from the master to a slave, and the bottom 
>  * three bits encoded for 8 slave-responses to those commands.
>  */
> #define DONT_CARE		((voll_t)0x37)	/* 11 1111 */
> /* ide_master commands to slaves */
> #define CMD_WAKEUP		((voll_t)0x08)	/* 00 1000 */
> #define CMD_CALL_FN		((voll_t)0x10)	/* 01 0000 */
> #define CMD_GIMME_RES		((voll_t)0x18)	/* 01 1000 */
> #define CMD_GO_HOME		((voll_t)0x20)	/* 10 0000 */
> #define CMD_SWITCH_CPUS		((voll_t)0x28)	/* 10 1000 */
> #define CMD_UNUSED1		((voll_t)0x30)	/* 11 0000 */
> 
> /* slave responses to ide_master's commands) */
> #define SLAVE_IDLE		((voll_t)0x00)	/* 00 0000 */
> #define SLAVE_AWAKE		((voll_t)0x01)	/* 00 0001 */
> #define SLAVE_WORKING		((voll_t)0x02)	/* 00 0010 */
> #define SLAVE_GAVE_EM		((voll_t)0x03)	/* 00 0011 */
> #define SLAVE_UNUSED1		((voll_t)0x04)	/* 00 0100 */
> #define SLAVE_UNUSED2		((voll_t)0x05)	/* 00 0101 */
> #define SLAVE_UNUSED3		((voll_t)0x06)	/* 00 0110 */
#endif /* forget about trying to use the LEDs for this */


#define SLAVE_INTER	0x80000000

/* 
 * values of the slaves' `doit_done' status field:
 * 	pending, finished, interrupted, or error
 */
#define STAT_SDOIT		1
#define STAT_SDONE		2
#define STAT_SINTER		3
#define STAT_SERROR		4

/*
 *commands from ide_master to remote slaves
 */
#define SLAVE_WAKEUP		(0x01)		/* 00 0001 */
#define SLAVE_CALL_FN		(0x02)		/* 00 0010 */
#define SLAVE_GO_HOME		(0x03)		/* 00 0011 */
#define SLAVE_SWITCH_CPUS	(0x04)		/* 00 0100 */

#define SBADCMD_STR		"?"
#define SWAKE_STR		"wake_up"
#define SCALL_STR		"exec_fn"
#define SGOHOME_STR		"go_home"
#define SSWTCH_STR		"swtch_cpus"

#define BADSCMD(_CMD)	(_CMD < SLAVE_WAKEUP || _CMD > SLAVE_SWITCH_CPUS)
#define BADSSTAT(S_STAT)  (S_STAT < STAT_SDOIT || S_STAT > STAT_SERROR)


#define IDLE_LEDS(_VID_)				\
	{						\
		int led5 = ((0x1 << 5) | _VID_);	\
		int led4 = ((0x1 << 4) | _VID_);	\
		int i;					\
							\
		_set_cc_leds(0);			\
		for (i = 150000; i > 0; i--);		\
		_set_cc_leds(led5);			\
		for (i = 150000; i > 0; i--);		\
		_set_cc_leds(0);			\
		for (i = 150000; i > 0; i--);		\
		_set_cc_leds(led4);			\
	}


/*
 * - VID_OUTARANGE check vids against highest assigned vid found during
 *   traversal of the prom-built config-tree, or 0 if an SP machine.
 *   use this check for verifying launch vids, etc.
 * - PLAUSIBLE_VID checks only that the vid is in the range of 
 *   0 <= vid < MAXSETSIZE.  PLAUSIBLE_VID is used in set operations like
 *   set_in and add_cpu, where the target set isn't referring to the
 *   active system.
 * - PHYS_IDX_OUTARANGE checks physically-numbered cpunums against the
 *   defined system-max `MAXCPU'.
 */
#define VID_OUTARANGE(VID)		((VID) < 0 || (VID) > _ide_info.vid_max)
#define PHYS_IDX_OUTARANGE(PHYS_IDX)	((PHYS_IDX) < 0 || (PHYS_IDX) >= MAXCPU)
#define PLAUSIBLE_VID(V_ID)		((V_ID) >= 0 && (V_ID) < MAXSETSIZE)

typedef struct argdesc_s {
	int argc;
	char **argv;
} argdesc_t;

extern int _get_disopts(argdesc_t *arginfop, optlist_t *optlistp);

typedef volatile struct proc_talk_s {
    volvp fnaddr;
    argdesc_t fnargcv;		/* contains fn's <argc, argv> combo */
    voll_t fnreturn;		/* launched job's return value */
    voll_t state;		/* indicates slave's current state */
    voll_t slavecmd;		/* command from master: slave spins on this */
    voll_t doitdone;		/* */
} proc_talk_t;


/*
 * IDE-specific Private Data Area
 * 
 * The IDE pda contains the IDE-specific globals: not needed or accessed
 * by any other arcs standalones or library routines, but written and
 * read in shared code.
 */
#define IDE_PDAMAGIC	0xDBADBD

#if (_MIPS_ISA == _MIPS_ISA_MIPS1 || _MIPS_ISA == _MIPS_ISA_MIPS2) 
typedef int jbuf_elem;
#endif
#if (_MIPS_ISA == _MIPS_ISA_MIPS3 || _MIPS_ISA == _MIPS_ISA_MIPS4) 
typedef __uint64_t jbuf_elem;
#endif

typedef union ide_pda_u {
    struct ide_pda_s {
	int		ide_pdamagic;	/* validate each processor's pda */
	int		ide_mode;	/* IDE_MASTER or IDE_SLAVE */
	proc_talk_t	launch_fn;	/* requested function to launch */
	proc_talk_t	cleanup_fn;	/* clean-up routine (optional) */
	struct string_list *argv_str;
	struct string_list *environ_str;
	jbuf_elem	*slave_jbuf;
	jbuf_elem	*wakeup_jbuf;
	char		*putbuf;
	int		putbufndx;
/* 	int		*fault_buf; 	-- may use for diags later */
    } idata;
  char	filler[IDE_PDASIZE];		/* any reasonable power of 2 */
} ide_pda_t;
extern ide_pda_t ide_pda_tab[];

#define slave_cmd		launch_fn.slavecmd	
#define doit_done		launch_fn.doitdone	

#define fn_addr			launch_fn.fnaddr	
#define fn_argcv		launch_fn.fnargcv	
#define fn_return		launch_fn.fnreturn	

#define cleanup_addr		cleanup_fn.fnaddr	
#define cleanup_argcv		cleanup_fn.fnargcv	
#define cleanup_return		cleanup_fn.fnreturn	


/*
 * 'GPRIVATE' is already defined in genpda.h; that'll get us into our 
 * generic pda.  IDE-specific global variables are stored in another
 * set of pdas: to access values in other processors' pdas (master does
 * this) define 'IDE_PRIVATE(vid,fieldname)'.  Transparent access to 
 * one's own pda fields is provided by prepending the field names with an
 * underscore.  e.g. IDE_PRIVATE(vid,slave_jbuf) and _slave_jbuf both
 * refer to ide_pda_tab[vid].ide_pda.slave_jbuf.
 */
#define IDE_PRIVATE(_VID_,_FIELD_)	(ide_pda_tab[_VID_].idata._FIELD_)
#define IDE_ME(_FIELD_)		(ide_pda_tab[_cpuid()].idata._FIELD_)

#if EVEREST
/*
 * boardinfo_t tallies the number of each type of legal EVEREST board
 * based on the current configuration, created by the prom during bootup
 */
typedef struct boardinfo_s {
    int ip19;
    int ip21;
    int ip25;
    int io4;
    int mc3;
    int mty;		/* total number of empty slots */
    int unknown;
} boardinfo_t;
extern boardinfo_t evboards;
extern int      dump_ev_bdinfo(boardinfo_t *);
#endif /* EVEREST */


/*
 * ide_slave() provides the setup and cleanup between libsk's 
 * call_coroutine() and the invoked function.  It sets up the stack
 * using the specified sp, and--interpretting the single argument provided
 * by the launch facility as a pointer to the necessary args--sets up the
 * normal <int argc, char *argv[]> format and calls the function.  
 */
extern int ii_spl;
extern lock_t ide_info_lock;		

#if defined(MULTIPROCESSOR) && defined(_LOCK_IINFO)
#define _DO_IILOCK
#define	LOCK_IDE_INFO()		ii_spl=splockspl(ide_info_lock, splhi);
#define	UNLOCK_IDE_INFO()	spunlockspl(ide_info_lock, ii_spl);
#else
#define	LOCK_IDE_INFO()
#define	UNLOCK_IDE_INFO()
#endif /* MULTIPROCESSOR  && _LOCK_IINFO */


#define IDE_PMGR	"ide_pmgr"	/* IDE Process Manager */

/* 
 * global_info_t contains information of general interest to ide slaves
 * and the ide_master: the tally and set of logical processors in the
 * system, the vid of the processor currently executing as master, 
 * status variables used for user-interface arbitration/control, and
 * a set of variables which determine the default environment under which
 * all diagnostics execute.  It is initialized during ide startup by the master
 * code, and updated by the new ide_master when the master execution
 * thread migrates to a different processor.  The ui data is written
 * by slaves and the master, so it's lock-protected.  The remaining
 * fields of the structure are only modified by the processor executing
 * as 'master', so it needn't be locked.
 */
typedef struct global_info_s {
	uint	g_magic;	/* confirms initialization of vp_info struct */
	uint	vid_cnt;	/* total # of active (==virtual) processors */
	uint	vid_max;	/* largest allocated virtual id in system */
	uint	master_vid;	/* current ide-master's virtual processor# */
	uint	*saved_normsp;	/* master's normal-mode SP when slave */
	uint	*saved_faultsp;	/* master's fault-mode SP when slave */
	uint	talking_vid;	/* vid of proc currently controlling duart */
	volul_t c_pending;	/* pending state of system */
	volul_t c_state;	/* current state of system (coherent/noncoh) */
	uint	pad;		/* must have dbl-word boundary for long longs */
	set_t	waiting_vids;	/* bitmask of cpus currently in ide_slave_wait*/
	set_t	vid_set;	/* bit-vector of virtual processors */
} global_info_t;
#define IDE_GMAGIC	0xDEADEDDE
extern global_info_t	_ide_info;

extern int	dump_global(global_info_t *);
extern int	_do_launch(uint, volvp, argdesc_t *);
extern int	_do_slave_asynch(uint, volvp, argdesc_t *);
extern int	_insert_arg(argdesc_t *, char *, int);
extern void	dump_gpda(int);
extern void	dump_idepda(int);
extern int	setup_idarr(int, int *, int *);

#endif /* !_LANGUAGE_ASSEMBLY */

/*
 * A non-fault-handling version of _get_gpda macro in genpda.h.
 * This one uses t0 and t1 instead of k0 and k1 to compute
 * a pointer to the processor's correct (generic) pda, as indexed
 * by virtual id.  If expanded for Everest systems, it requires
 * that the ss_to_vid array be initialized.
 * Ends up with the running process's pda in t1.
 * It is used by slave startup code in all three csu.s files
 * (libsk, libsc, and ide) to read and write the initial_sp and
 * fault_sp variables (which previously were in BSS).
 *
 * Some of the things buried in this macro:
 * 	- pda_t is 512 bytes
 *	- ss_to_vid is an array of bytes
 *	- (EVEREST only): EV_GET_SPNUM returns a "physical cpuid",
 *	    a number suitable for use as an index into ss_to_vid.
 *	- NOTE THE BDSLOT after the branch!
 */
#ifdef IP19
#define		_get_spda		\
	/*la	t0,isaneverest;		\
	lw	t1,0(t0);		\
	li	t0, YES_EVEREST;	\
	bne	t0,t1,4f;		\
	move	t0, zero;*/		\
					\
	li	t0,EV_SPNUM;		\
	ld	t0,0(t0);		\
	la	t1,ss_to_vid;		\
	and	t0,EV_SPNUM_MASK;	\
	addu	t1,t0;			\
	lb	t0,0(t1);		\
	sll	t0,GENERIC_PDASIZE_2;	\
	la	t1,gen_pda_tab;		\
4:	la	t1,gen_pda_tab;		\
	addu	t1,t0
#elif EVEREST

#define		_get_spda		\
	/*la	t0,isaneverest;		\
	lw	t1,0(t0);		\
	dli	t0, YES_EVEREST;	\
	bne	t0,t1,4f;		\
	move	t0, zero;*/		\
					\
	dli	t0,EV_SPNUM;		\
	ld	t0,0(t0);		\
	dla	t1,ss_to_vid;		\
	and	t0,EV_SPNUM_MASK;	\
	addu	t1,t0;			\
	lb	t0,0(t1);		\
	sll	t0,GENERIC_PDASIZE_2;	\
	dla	t1,gen_pda_tab;		\
4:	dla	t1,gen_pda_tab;		\
	addu	t1,t0
#else
#define		_get_spda		\
	_get_gpda(t1,t0)
#endif

#endif /* __IDE_MP_H__ */
