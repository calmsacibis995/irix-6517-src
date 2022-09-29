/*
 * dbgmon.h -- debug monitor definitions
 * "symmon/dbgmon.h: $Revision: 1.55 $"
 */

#if LANGUAGE_C
#include <sys/types.h>
#if TFP
#include <sys/tfp.h>	/* for SR_PAGESIZE */
#endif

typedef long long numinp_t;	/* numeric input from user */
typedef unsigned long long u_numinp_t;	/* numeric input from user */

/*
 * Sign extend a long (or int) to a long long.  If the value
 * is already a long long, just leave it alone.
 */
#define sign_extend(x) \
	(((unsigned long long)(x) == (unsigned long)(x)) ? \
		(long long)((long)(x)) : (x))

#ifdef NETDBX
/*
 * Cpu state relative to symmon.
 */
typedef enum {	EnteredSymmon=0x0,
		ReadyToResume=0x1,
		OutOfSymmon=0x2,
		ForcedContinue=0x3,
		CSInvalid=0x4
	     } CpuState;
#endif /* NETDBX */

#endif /* LANGUAGE_C */

/*
 * Monitor modes
 */
#define	MODE_DBGMON	0	/* debug monitor is executing */
#define	MODE_CLIENT	1	/* client is executing */
#define MODE_IDBG	2	/* client executing on behalf of dbgmon */

#ifndef NULL
#define NULL 0
#endif

/*
 * String constants
 */
#define	PROMPT	"DBG: "
#define	DEFAULT_STRLEN	70		/* default max strlen for string cmd */

/*
 *
 */

#ifndef LANGUAGE_ASSEMBLY
struct dbgargs {
	char *name;
	char *str_ptr;
	int  strlen;
};

/*
 * breakpoint table structure, used to be static
 *
 * WARNING: If you modify brkpt_table here, you must also modify it in
 * 	    irix/kern/sys/idbg.h (need to be consistent).
 */
struct brkpt_table {
	inst_t *bt_addr;		/* address where brkpt is installed */
	cpumask_t bt_cpu;		/* for which cpu(s) - bit mask */
	unsigned bt_inst;		/* previous instruction */
	int bt_type;			/* breakpoint type */
	int bt_oldtype;			/* type before being suspended */
};

/*
 * register table structure, used to hold names, constants, and
 * descriptor pointers associated with each register.
 */
struct reg_table {
	char *rt_name;
	int rt_index;
	struct reg_desc *rt_desc;
}; 

/*
 * Platform dependent name table structure.  Associates
 * an arbitrary name/value pair.  Currently used to map 
 * platform dependent register names to associated
 * memory-mapped addresses.  Also used to map locations 
 * of software structures in low memory.
 */
typedef struct pd_name_table_s {
	char	*pd_name;
	long	pd_value;
} pd_name_table_t;
#endif

/*
 * symmon exception frame
 *
 */

#define E_K1		0		/* not valid on IP5 */
#define E_GP		1
#define E_EPC		2
#define	E_SR		3
#define E_BADVADDR	4
#define E_CAUSE		5
#define E_SP		6
#define E_RA		7
#define E_ERREPC	8

#if EVEREST
/*
 * The following value is used to adjust the stack pointer.
 * In order to support doubleword stack loads/stores, the
 * compiler supports a convention that the stack pointer 
 * can be adjusted only by multiples of 16.  The E_SIZE,
 * must follow this convention!
 */

/* 9 doublewords, but use 10 to ensure 16 byte stack alignment */
#define E_SIZE		(10*SZREG)	/* size of exception frame */
#endif /* EVEREST */

#if SN0
/* for now, same as everest... may change later */
/* 9 doublewords, but use 10 to ensure 16 byte stack alignment */
#define E_SIZE		(10*SZREG)	/* size of exception frame */
#endif /* SN0 */

#if IP20 || IP22 || IP26 || IP28
#define E_CPU_PARERR	8
#define E_GIO_PARERR	9
#define E_LIOINTR0	10
#define E_LIOINTR1	11
#define E_CPUADDR	12
#define E_GIOADDR	13
#if IP20 || IP22 || IP28
#define	E_SIZE		(14*SZREG)	/* size of exception frame */
#endif
#if IP26
#define E_LIOINTR2	14
#define E_TCC_INTR	15
#define E_TCC_BE_ADDR	16
#define E_TCC_PARITY	17
#define E_TCC_ERROR	18
#define E_PADDING	19
#define	E_SIZE		(20*SZREG)	/* size of exception frame */
#endif
#endif

#ifdef IP30
#define	E_SIZE		(10*SZREG)	/* size of exception frame */
#endif

#if IP32 || IPMHSIM
#define E_CRM_CPU_ERROR_ADDR   8
#define E_CRM_CPU_ERROR_STAT    9
#define E_CRM_CPU_ERROR_VICE_ERR_ADDR   10
#define E_CRM_MEM_ERROR_STAT	11
#define E_CRM_MEM_ERROR_ADDR	12
#define E_CRM_MEM_ERROR_ECC_SYN	13
#define E_CRM_MEM_ERROR_ECC_CHK	14
#define E_CRM_MEM_ERROR_ECC_REPL	15
#define E_SIZE          (16*SZREG)      /* size of exception frame */
#endif

#if _MIPS_SIM == _ABI64
#if TFP
#define SYMMON_SR	(SR_KX|SR_PAGESIZE)
#define SYMMON_SR_KEEP	0
#elif R10000
#define SYMMON_SR	(SR_KX | SR_UX)
#define SYMMON_SR_KEEP	0
#else
#define SYMMON_SR	(SR_KX)
#define SYMMON_SR_KEEP	SR_DE
#endif
#else
#define SYMMON_SR	(0)
#define SYMMON_SR_KEEP	0
#endif

#ifndef LANGUAGE_ASSEMBLY
int	parse_sym(char *,numinp_t *);
int	parse_sym_range(char *,numinp_t *,int *,int);

struct cmd_table;
int	_brk(int, char *[], char *[], struct cmd_table *);
int	_cacheflush(int, char *[], char *[], struct cmd_table *);
int	_calc(int, char *[], char *[], struct cmd_table *);
int	_call(int, char *[], char *[], struct cmd_table *);
int	_cont(int, char *[], char *[], struct cmd_table *);
int	_disass(int, char *[], char *[], struct cmd_table *);
int	_dobtrace(int, char *[], char *[], struct cmd_table *);
int	_get(int, char *[], char *[], struct cmd_table *);
int	_go_to(int, char *[], char *[], struct cmd_table *);
int	_kp(int, char *[], char *[], struct cmd_table *);
int	_put(int, char *[], char *[], struct cmd_table *);
int	_quit(int, char *[], char *[], struct cmd_table *);
int	_rdebug(int, char *[], char *[], struct cmd_table *);
int	_step(int, char *[], char *[], struct cmd_table *);
int	_Step(int, char *[], char *[], struct cmd_table *);
int	_string(int, char *[], char *[], struct cmd_table *);
int	_tlbdump(int, char *[], char *[], struct cmd_table *);
int	_tlbflush(int, char *[], char *[], struct cmd_table *);
int	_tlbpid(int, char *[], char *[], struct cmd_table *);
int	_tlbmap(int, char *[], char *[], struct cmd_table *);
#if TFP
int	_tlbx(int, char *[], char *[], struct cmd_table *);
#endif
#if R4000 || R10000
int	_tlbpfntov(int, char *[], char *[], struct cmd_table *);
#else
int	_tlbptov(int, char *[], char *[], struct cmd_table *);
#endif /* R4000 || R10000 */
int	_tlbvtop(int, char *[], char *[], struct cmd_table *);
int	_unbrk(int, char *[], char *[], struct cmd_table *);
int	clear(int, char *[], char *[], struct cmd_table *);
int	disable(int, char *[], char *[], struct cmd_table *);
int	dump(int, char *[], char *[], struct cmd_table *);
int	_dump(int, char *[], char *[], struct cmd_table *);
int	enable(int, char *[], char *[], struct cmd_table *);
int	hxtonm(int, char *[], char *[], struct cmd_table *);
int	lkaddr(int, char *[], char *[], struct cmd_table *);
int	lkup(int, char *[], char *[], struct cmd_table *);
int	nmtohx(int, char *[], char *[], struct cmd_table *);
int	test(int, char *[], char *[], struct cmd_table *);
int	printregs(int, char *[], char *[], struct cmd_table *);
int	_dbxdump(char *);
int	_c_tags(int, char *[], char *[], struct cmd_table *);
#if R10000
int	dumpPrimaryCache(int, char *[], char *[], struct cmd_table *);
int	dumpSecondaryCache(int, char *[], char *[], struct cmd_table *);
int	_dump_sline(int, char *[], char *[], struct cmd_table *);
int	_dump_dline(int, char *[], char *[], struct cmd_table *);
int	_dump_iline(int, char *[], char *[], struct cmd_table *);
#ifdef EVEREST
void	_dump_ct(int, char *[], char *[], struct cmd_table *);
void	_dump_cta(int, char *[], char *[], struct cmd_table *);
void	_dump_cts(int, char *[], char *[], struct cmd_table *);
void	_dump_ctas(int, char *[], char *[], struct cmd_table *);
#endif /* EVEREST */
#endif /* R10000 */
#if R4000 || R10000
int	_wpt(int, char*[], char *[], struct cmd_table *);
#endif

void _save_nmi_exceptions(void);
void _hook_nmi_exceptions(void);
void _restore_nmi_exceptions(void);

extern k_machreg_t _get_register(int);
extern void _set_register(int, k_machreg_t);
extern long long _get_memory(caddr_t, int);
extern void _set_memory(caddr_t, int, long long);
extern int _PrintInstruction(inst_t *, int, int);
extern inst_t getinst(inst_t *);
#if !IP19 && !IP25 && !SN0
extern long long load_double(long long *);
#endif
#if (_MIPS_SIM == _ABIO32)
extern void store_double(long long *, long long);
#endif
extern void _show_inst(inst_t *);

extern void _fatal_error(const char *msg);
extern void _dbg_error(const char *, void *);

extern void idbg_cleanup(void);

extern int  _do_it(__psint_t, void (*)(), int, char *[]);
extern void symmon_fault(void);
extern void symmon_spl(void);

extern void map_k2seg_addr(void *);
extern int  _kpcheck(int, char *[], int);
extern int  _is_kp_func(char *cmd);
extern int  trykfix(void);

extern int invoke(inst_t *, long, long, long, long, long, long, long, long);
extern void _resume_brkpt(void);
extern void _restart(void);

extern void validate_brkpts(void);
extern void _check_bp(void);
extern int is_jal(inst_t);

extern int symtab(void);
extern int do_it2(char *, __psint_t, __psint_t);
extern int lookup_regname(char *, numinp_t *);
extern void close_on_exception(unsigned long);
extern void new_brkpt(inst_t *, int, cpumask_t);
extern int old_brkpt(inst_t *);
extern void single_step(void);
extern char *fetch_kname(inst_t *);
extern caddr_t fetch_kaddr(char *);
extern void main_intr (void);

extern struct cmd_table ctbl[];		/* generic command table */
extern struct cmd_table pd_ctbl[];	/* platform-dependent command table */


#define GETINST(addr)			getinst((inst_t *)(addr))
#define BRANCH_TARGET(a0,pc)		branch_target(a0, (inst_t *)(pc))
#define NEW_BRKPT(addr,type,cpu)	new_brkpt((inst_t *)(addr), type, cpu)
#define OLD_BRKPT(addr)			old_brkpt((inst_t *)(addr))
#define GET_MEMORY(caddr,width)		_get_memory((caddr_t)(caddr), (width))

#define SET_MEMORY(caddr,width,value) \
	_set_memory((caddr_t)(caddr), (width), (long long)(value))

#define PRINTINSTRUCTION(addr,style,showregs) \
	_PrintInstruction((inst_t *)(addr), (style), (showregs))

#define atob_s(string,ptr)		atob_L(string, ptr)
#define atobu_s(string,ptr)		atobu_L(string, ptr)

#endif
