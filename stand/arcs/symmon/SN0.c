/*
 * SN0.c - SN0-specific routines for symmon
 *
 */
#ident "symmon/SN0.c: $Revision: 1.57 $"


#include <string.h>
#include <sys/types.h>
#ifdef SABLE
#define	PROM	1
#include <sys/SN/kldir.h>
#undef PROM
int fake_prom=0;
#endif
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <libsc.h>
#include "dbgmon.h"
#include "fault.h"
#include "parser.h"
#include "saioctl.h"
#include "mp.h"
#include "cache.h"
#include <ksys/elsc.h>
#include "sys/SN/agent.h"
#include "sys/SN/kldiag.h"
/* #include "sys/SN/addrs.h" */
#include <sys/SN/klconfig.h>
#ifdef SABLE
#include <sys/SN/promcfg.h>
#include <sys/kopt.h>		/* kopt_set */
#include <arcs/spb.h>
#endif
#include <sys/SN/gda.h>
#include "hwreg.h"
#include <libkl.h>
#include "stringlist.h"

extern int bdeb;
extern  hwreg_set_t		hwreg_hub;

char  diag_name[128];
__uint64_t         diag_malloc_brk;

/*
 * This flag gets set to 1 if this lib is linked into symmon.
 * This is not the same as _prom. Symmon also sets _prom
 * to 1, so that flag can't be used for some decisions.
 * This should really be done for all machines, but right now
 * only SN0-specific code looks at it.
 */
int _symmon = 1;

/*
 * register descriptions
 */
extern struct reg_desc sr_desc[], cause_desc[];
extern struct reg_desc _liointr0_desc[], _liointr1_desc[]; 

void notyetmess(char *);

static struct reg_values except_values[] = {
	{ EXCEPT_UTLB,	"UTLB Miss" },
	{ EXCEPT_XUT,	"XTLB Miss" },
	{ EXCEPT_NORM,	"NORMAL" },
	{ EXCEPT_BRKPT,	"BREAKPOINT" },
	{ EXCEPT_NMI,	"NMI" },
	{ 0,		NULL }
};

static struct reg_desc except_desc[] = {
	/* mask		shift	name	format	values */
	{ (__scunsigned_t)-1,	0,	"vector",NULL,	except_values },
	{ 0,		0,	NULL,	NULL,	NULL }
};

/*
 * Start of symmon's stack for boot processor.
 * Note that _dbgstack should point to TOP of stack (i.e. max stack value).
 * We use PHYS_TO_K1 since that's what the SYMMON_STK_ADDR macro in
 * sn0addrs.h currently uses.
 */
/* This is really an "extra" stack since the proms have allocated us a
 * real stack.  Problem is that we can't quickly determine the correct
 * stack to use at the "start" entry point.  So for now the boot processor
 * uses this temporary stack.
 * Less of a kludge than the earlier hack which used a hard coded address
 * of nasid == 0 SYMMON_STACK.
 */

long long initial_dbgstk[SYMMON_STACK_SIZE/sizeof(long long)];
long long *_dbgstack=&initial_dbgstk[SYMMON_STACK_SIZE/sizeof(long long)];

static int _chkcoh(int, char *[], char *[], struct cmd_table *xxx);
static int _reset(int, char *[], char *[], struct cmd_table *xxx);
extern int _switch_cpus(int, char *[], char *[], struct cmd_table *xxx);
static int _hwreg(int, char *[], char *[], struct cmd_table *);
static int _nmi(int, char *[], char *[], struct cmd_table *);
#if 0
static int _crb(int, char *[], char *[], struct cmd_table *);
static int _dumpspool(int, char *[], char *[], struct cmd_table *);
#endif
static int _softreset(int, char *[], char *[], struct cmd_table *);
static int _do_on(int, char *[], char *[], struct cmd_table *);

/*
 * Platform-Dependent command table.
 */
struct cmd_table pd_ctbl[] = {
	{ "chkcoh",	_chkcoh,	"chkcoh:\t\tchkcoh [RANGE]"},
	{ "cpu",	_switch_cpus,	"cpu:\t\tcpu [CPUID]" },
	{ "reset",	_reset,		"reset:\t\treset system"},
	{ "hwreg", 	_hwreg,		"hwreg:\t\thwreg REGNAME"}, 
	{ "pr", 	_hwreg,		"pr:\t\tpr REGNAME"}, 
	{ "nmi",	_nmi,		"nmi:\t\tnmi CPUID"},
	{ "softreset",	_softreset,	"softreset:\tsoftreset CPUID"},
	{ "do_on",	_do_on,		"do_on:\t\tdo_on \"CMD\" CPULIST: "
					"execute cmd on other cpu(s)"},
#if 0
	{ "dumpspool",  _dumpspool,	"dumpspool:\tdumpspool CPUID"},
	{ "crb",  _crb,	"crb:\t\tcrb NASID"},
	{ "crbx",  _crb,"crbx:\t\tcrb NASID"},
#endif
	{ 0,		0,		"" }
};

pd_name_table_t pd_name_table[] = {
	/*
	 * SN0 addresses for fixed-location 
	 * software structures.
	 */

	/* XXX fixme: add some more addresses here XXX */
	/*
	 * symmon internal symbols
	 */
	{ "bdeb",		(long)&bdeb },

	{ 0,			0 }
};

/* Convert logical cpuid to physical slot */
int cpuid_to_slot[MAXCPU];

/* Convert logical cpuid to physical cpu# on board */
int cpuid_to_cpu[MAXCPU];

static int cpus_enabled;	/* no of enabled cpus in system */

cnodeid_t nasid_to_compact_node[MAX_NASIDS];
nasid_t compact_to_nasid_node[MAX_COMPACT_NODES];
cpuid_t nasid_slice_to_vid[MAX_NASIDS][CPUS_PER_NODE];
nasid_t master_nasid;
int numnodes;

char cpu_enable_flag[MAXCPU];

int have_elscuart = -1;

/*
 * Set up the ELSC UART (if we have one)
 * XXX - This code assumes that all ELSCs in a system will either support
 * the diagnostic console or not.
 */
void
elscuart_setup()
{
	__psunsigned_t base;
	extern void set_elsc(elsc_t *);

	/*
	 * Set up the entry-level system controller
	 * Steal the first part of the symmon stack for the ELSC buffer.
	 */

	base = SYMMON_STK_ADDR(get_nasid(),
                      get_cpu_slice(cpuid()));

	set_elsc((elsc_t *) base);
	elsc_init(get_elsc(), get_nasid());
	elscuart_init(0);

	if (have_elscuart == -1) {
		have_elscuart = elscuart_probe();
		if (!have_elscuart)
			printf("WARNING: No diagnostic console available\n");
	}
}


/*
 * int do_cpumask(cnodeid_t, nasid_t, cpumask_t *)
 *
 *	Look around klconfig to determine how many CPUs we have and which
 * 	ones they are.  Return the number of CPUs on this node.
 *	It's too early to do anything useful with this information
 *	so we'll be back later.
 *
 *	If it should be convenient later, we can store away a pointer to
 *	the CPU structure.
 */
/* ARGSUSED */
int
do_cpumask(cnodeid_t cnode, nasid_t nasid, cpumask_t *boot_cpumask)
{
	int cpus_found = 0;
	cpuid_t cpuid;
	int slice;

	for (slice = 0; slice < CPUS_PER_NODE; slice++)
	    if ((cpuid = cnode_slice_to_cpuid(cnode, slice)) != -1) {
		    /* Set the bit. */
		    CPUMASK_SETB(*boot_cpumask, cpuid);
		    cpus_found++;
	    }
	return cpus_found;
}

/*
 * Probe SN0 hardware to find out where cpu boards are,
 * and set up mappings between physical CPU IDs and logical
 * CPU IDs according to PROM mappings.
 *
 * Some cpus may have been disabled by the PROM, but they
 * may be enabled later under operator control.
 *
 * Returns maximum logical cpu ID (which should be the same as the
 * maximum number of cpu's, though some may be disabled).
 */

extern nasid_t master_nasid;

int
cpu_probe(void)
{
	int i, j, high, cpus;
	gda_t *gdap;
	nasid_t nasid;

	master_nasid = get_nasid();

	diag_set_mode(DIAG_MODE_NONE);

	for (i = 0; i < MAX_COMPACT_NODES; i++) {
		compact_to_nasid_node[i] = INVALID_NASID;
	}

	for (i = 0; i < MAX_NASIDS; i++) {
		nasid_to_compact_node[i] = INVALID_CNODEID;
	}

#ifdef SABLE
	if (fake_prom) {
		gdap = GDA;
		/* Clear the NASID table */
		for (i = 1; i < MAX_COMPACT_NODES; i++)
			gdap->g_nasidtable[i] = INVALID_CNODEID;

		/* setup for 2 nodes */
		for (i=0; i<2; i++)
			gdap->g_nasidtable[i] = i;
	}
#endif /* SABLE */	
	/* Real code: */
	gdap = GDA;
	numnodes = cpus = 0;
	for (i = 0; i < MAX_COMPACT_NODES; i++) {
		if ((nasid = gdap->g_nasidtable[i]) == INVALID_NASID) {
			break;
		} else {
			compact_to_nasid_node[i] = nasid;
			nasid_to_compact_node[nasid] = i;
			numnodes++;

			for (j = 0; j < CPUS_PER_NODE; j++) {
				high = cnode_slice_to_cpuid(i, j);
				nasid_slice_to_vid[nasid][j] = high;
				if (high > cpus) 
				    cpus = high;
			}
		}
	}

	return cpus + 1;
}

/*
 * Determine whether a given CPU is present and enabled.
 *
 * NOTE: If we call this routine often, we should create an
 * array of cpu_enables[] so we only perfom this (slow?)
 * lookup once instead of every time.
 */
int 
cpu_enabled(cpuid_t virtid) 
{
	int enabled, slice;
	nasid_t nasid;
	klcpu_t *acpu;

	/* we cache the cpu enable once we look it up */
	if (cpu_enable_flag[virtid])
		return(cpu_enable_flag[virtid]-1);

	acpu = (klcpu_t*)get_cpuinfo(virtid);

	if (acpu) {
		enabled = (acpu->cpu_info.flags & ENABLE_BOARD);
		return(enabled);
	}

#if SABLE
	{
	extern int slave_ready[];

	if (slave_ready[virtid]) {
		printf("fixme: cpu%d NOT found in KL_CONFIG_INFO, but ready!\n", virtid);
		cpu_enable_flag[virtid] = 2;
	}
	}
#endif /* SABLE */

	return(0);
}

void 
cpu_physical(int virtid, int *slot, int *slice)
{
	nasid_t nasid;
	if (get_nasid_slice((cpuid_t)virtid, &nasid, slice)) {
		*slot = 0;
		*slice = 0;
	} else {
		*slot = (int)nasid;
	}
	return;
}

/*
 * symmon_fault -- display info pertinent to fault
 */
void
symmon_fault(void)
{
	int epc;

	if (private.dbg_exc == EXCEPT_NMI)
		epc = E_ERREPC;
	else
		epc = E_EPC;

	ACQUIRE_USER_INTERFACE();

	printf("\nSYMMON EXCEPTION: %r\n", private.dbg_exc, except_desc);
	/* try to call disassembler */
	if (private.dbg_exc == EXCEPT_NMI)
		printf("Error EPC: 0x%x  EPC: 0x%x\n",
			(__psint_t)private.exc_stack[E_ERREPC],
			(__psint_t)private.exc_stack[E_EPC]);
	else
		printf("EPC: 0x%x\n",
			(__psint_t)private.exc_stack[E_EPC]);
	printf("Cause register: %R\n",
		(__psint_t)private.exc_stack[E_CAUSE], cause_desc);
	printf("Status register: %R\n",
		(__psint_t)private.exc_stack[E_SR], sr_desc);
	printf("RA: 0x%x\tSP: 0x%x\n",
		(__psint_t)private.exc_stack[E_RA],
		(__psint_t)private.exc_stack[E_SP]);
	if ((private.dbg_exc == EXCEPT_UTLB) || (private.dbg_exc == EXCEPT_XUT))
		goto printvaddr;
	switch ((__psint_t)private.exc_stack[E_CAUSE] & CAUSE_EXCMASK) {
		case EXC_MOD:	
		case EXC_RMISS:	
		case EXC_WMISS:
		case EXC_RADE:	
		case EXC_WADE:
printvaddr:
		printf("Bad Vaddress: 0x%Ix\n", private.exc_stack[E_BADVADDR]);
		break;
	}

	_show_inst((inst_t *)private.exc_stack[epc]);	

	RELEASE_USER_INTERFACE(NO_CPUID);
}

/*
 * Reset the machine.
 */
/*ARGSUSED*/
int
_reset(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
        printf("Resetting the system...\n");

        /* Reset this hub and the SN0Net */
        *LOCAL_HUB(NI_PORT_RESET) = NPR_PORTRESET | NPR_LOCALRESET;

	return 0;
}

/*
 * Send a processor a non-maskable interrupt
 */
/*ARGSUSED*/
int
_softreset(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	long long cpu;
	int nasid, slice;

	if (*atob_L(argv[1], &cpu)) {
		printf("Can't decode cpu number: %s\n", argv[1]);
	} else {
		cpu_physical(cpu, &nasid, &slice);

		printf("Sending soft reset to nasid %d,  (CPUs %d and %d)...\n",
			nasid, (cpu | 1) ^ 1, cpu | 1);

		*REMOTE_HUB(nasid, PI_HARDRESET_BIT) = 0;
		*REMOTE_HUB(nasid, PI_SOFTRESET) = 1;
	}

	return 0;
}


typedef struct cmd_info_s {
	struct cmd_table	*found_in_table,
				*cmd_entry;
	struct string_list 	*cmd_args;
} cmd_info_t;


/* 
 * This function is launched onto a remote cpu to run a command with multiple
 * arguments.
 */
void
remote_cmd(void *arg) {
	struct cmd_table *cmd_entry = ((cmd_info_t *)arg)->cmd_entry;
	int cmd_argc = ((cmd_info_t *)arg)->cmd_args->strcnt;
	char **cmd_argv = ((cmd_info_t *)arg)->cmd_args->strptrs;

	if (cmd_entry == NULL) {
		if (_kpcheck(cmd_argc,cmd_argv,1))
			printf("%s: Command not found.\n",cmd_argv[0]);
	}
	else if (cmd_entry->ct_routine(cmd_argc, cmd_argv, 0,
				  ((cmd_info_t *)arg)->found_in_table))
		usage(cmd_entry);
}


#define DO_ON_SERIES_STR ".."

/*ARGSUSED*/
int
_do_on(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	static int in_do_on = 0;
	int i,
		series = 0;
	long long	cpu = -1,
		cpu_start = -1;
	char *cur,
		*cpu_str;
	struct string_list cmd_args;
	cmd_info_t cmd_info;
	cpuid_t cur_cpu;

	/* check to see if we are calling ourselves */
	if (in_do_on)
		return 1;
	else
		in_do_on = 1;

	if (argc != 3) {
		printf("%d is an invalid number of arguments\n",argc);

		in_do_on = 0;
		return 1;
	}

	cur_cpu = cpuid();

	/* break up command line into argv */
	_argvize(argv[1],&cmd_args);
	
	cmd_info.cmd_args = &cmd_args;
			
	/* find command entry */
	if (cmd_info.cmd_entry = 
	    lookup_cmd(ctbl, cmd_args.strptrs[0])) {
		cmd_info.found_in_table = ctbl;
	}
	else if (cmd_info.cmd_entry = 
		 lookup_cmd(pd_ctbl, cmd_args.strptrs[0])) {
		cmd_info.found_in_table = pd_ctbl;
	}
	else if (_is_kp_func(cmd_args.strptrs[0]) == 1) {
		cmd_info.found_in_table = 0;
		cmd_info.cmd_entry = 0;
	}
	else {
		printf("Could not find command: %s\n",
		       cmd_args.strptrs[0]);
		
		in_do_on = 0;
		return 1;
	}


	/* parse the cpu list */

	cpu_str = cur = argv[2];
	
	while (*cpu_str != '\0') { 
		
		if ((*cur != ',') && (*cur != '\0')) {
			cur++;
			continue;
		}

		if (*cur != '\0')
			*cur++ = '\0';
		
		if (!strcmp(cpu_str, DO_ON_SERIES_STR)) {

			if (series == 1) {
				/* if we've already got a series ignore
				this one */
				cpu_str = cur;
				continue;
			}

			series = 1;
			
			cpu_start = cpu + 1;
			
			if (*cur != '\0') {
				/* if we're not done with the list, parse the
				upper bound */
				cpu_str = cur;
				continue;
			}

			cpu = _get_numcpus() - 1;

		}
		else if (*atob_L(cpu_str,&cpu)) {
			printf("Can't decode cpu number: %s\n",cpu_str);

			in_do_on = 0;
			return 1;
		}

		if (!series) {
			cpu_start = cpu;
		}
		
		for (i = cpu_start; i <= cpu; i++) {
			printf("\n\nExecuting command %s on cpu %d\n\n",
			       cmd_args.strptrs[0],i);
			
			if (i == cur_cpu)
				remote_cmd((void*)&cmd_info);
			else if (slave_func(i,remote_cmd,&cmd_info) < 0)
				printf("cpu %d is not stopped\n",i);
		}
		
		series = 0;
		cpu_str = cur;
	}

	in_do_on = 0;
	return 0;
}

#if 0
/*
 * Dump a processor's PI error spool.
 */
/*ARGSUSED*/
int
_dumpspool(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	long long cpu;
	int nasid, slice;

	if (*atob_L(argv[1], &cpu)) {
		printf("Can't decode cpu number: %s\n", argv[1]);
	} else {
		cpu_physical(cpu, &nasid, &slice);
		dump_error_spool(nasid, slice, printf);
	}

	return 0;
}

/*
 * Send a processor a non-maskable interrupt
 */
/*ARGSUSED*/
int
_crb(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	int nasid;

	if (*atob_L(argv[1], &nasid)) {
		printf("Can't decode nasid number: %s\n", argv[1]);
	} else {
		crbx(nasid, printf);
	}

	return 0;
}
#endif /* 0 */

/*
 * Send a processor a non-maskable interrupt
 */
/*ARGSUSED*/
int
_nmi(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	long long cpu;
	int nasid, slice;

	if (*atob_L(argv[1], &cpu)) {
		printf("Can't decode cpu number: %s\n", argv[1]);
	} else {
		cpu_physical(cpu, &nasid, &slice);

		printf("Sending NMI to CPU %d, nasid %d, slice %d...\n",
			cpu, nasid, slice);

		*REMOTE_HUB(nasid, PI_NMI_A + (slice * PI_NMI_OFFSET)) = 1;
	}

	return 0;
}

/* Decode system registers*/
/*ARGSUSED*/
int
_hwreg(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
    hwreg_t            *hwreg;
    volatile __uint64_t *addr;
    __uint64_t		source;
    long long		value;

    if (argc == 1) {
        printf("Address required\n");
        return 0;
    }

    /* Find out which register we're talking about */
    hwreg = hwreg_lookup_name(&hwreg_hub, argv[1], 1);

    if (!hwreg) {
	printf("Couldn't decode '%s'\n", argv[1]);
	return 0;
    }

    /* Don't handle remote hubs in symmon yet. */
    addr = LOCAL_HUB(hwreg->address);

    if (argc == 2) {
	source = 0;
    } else if (argc == 3) {
	if (argv[2][0] == '~') {
		source = 3;
	} else {
	    if (*atob_L(argv[2], &value)) {
		    printf("Error: cannot parse value '%s'\n", argv[2]);
		    return 0;
	    }
	    source = 2;
        }		
    } else {
	printf("hwreg:\thwreg REGNAME [VALUE]\n");
    }

    printf("Register: %s (0x%016llx)\n",
	   hwreg_hub.strtab + hwreg->nameoff, addr);

#if 0
    if (source == 1)
        addr = REMOTE_HUB(nasid, hwreg->address);
    else
#endif
        addr = LOCAL_HUB(hwreg->address);

    if (source < 2)
        value = *addr;
    else if (source == 3)
        value = hwreg_reset_default(&hwreg_hub, hwreg);

    printf("Value   : 0x%016llx (%s)\n",
           value,
           source == 0 ? "loaded from register" :
           source == 1 ? "loaded from remote register" :
           source == 2 ? "as requested" :
           source == 3 ? "reset default" : "");

    hwreg_decode(&hwreg_hub, hwreg, 0, 0, 16, value, printf);

    return 0;
}

/***************************************************************************/
/* Utilities for cache coherency checker                                   */
/***************************************************************************/
/***************************************************************************/
/* returns a string form of a cache state (for printing)                   */
char *cacheStateStr(int cacheState)
{
  if (cacheState==Invalid) return("Invalid");
  if (cacheState==CleanExclusive) return("CleanExclusive");
  if (cacheState==DirtyExclusive) return("DirtyExclusive");
  if (cacheState==Shared) return("Shared");
  if (cacheState==DirtyShared) return("DirtyShared");
  return("Unknown");
}
/***************************************************************************/
/***************************************************************************/
/* returns a string form of a cache state (for printing)                   */
char *busTagStateStr(int busTagState)
{
  if (busTagState==BusInvalid) return("BusInvalid");
  if (busTagState==BusExclusive) return("BusExclusive");
  if (busTagState==BusShared) return("BusShared");
  return("Unknown");
}
/***************************************************************************/
/***************************************************************************/
/* displays two blocks of memory, pointing out the differences             */
void memDiffDisplay(char *a,char *b,int length,char *titleA,char *titleB)
{
  int i;
  int j;
  int rows;
  int pos;
  int rowLength = 16;

  rows=length/rowLength; 

  printf("%s:\n",titleA);

  for (i=0;i<rows;i++)
  { /* loop through the rows */
    for (j=0;j<rowLength;j++)
    { /* print it in hex */
      pos=(i*rowLength)+j;
      if (a[pos]!=b[pos]) printf(">"); else printf(" ");
      printf("%02x",a[pos]);
      if (j==((rowLength>>1)-1)) printf("  ");
    }

    printf("   ");

    for (j=0;j<rowLength;j++)
    { /* print in ascii */
      pos=(i*rowLength)+j;
      if ((a[pos]<32)||(a[pos]>126)) printf("."); else printf("%c",a[pos]);
      if (j==((rowLength>>1)-1)) printf(" ");
    }

    printf("\n");

  } /* looping through rows */

  /* now do the "b" half */

  printf("%s:\n",titleB);

  for (i=0;i<rows;i++)
  { /* loop through the rows */
    for (j=0;j<rowLength;j++)
    { /* print it in hex */
      pos=(i*rowLength)+j;
      if (a[pos]!=b[pos]) printf(">"); else printf(" ");
      printf("%02x",b[pos]);
      if (j==((rowLength>>1)-1)) printf("  ");
    }

    printf("   ");

    for (j=0;j<rowLength;j++)
    { /* print in ascii */
      pos=(i*rowLength)+j;
      if ((b[pos]<32)||(b[pos]>126)) printf("."); else printf("%c",b[pos]);
      if (j==((rowLength>>1)-1)) printf(" ");
    }

    printf("\n");

  } /* looping through rows */

} /* end of memDiffDisplay */

/***************************************************************************/
/***************************************************************************/
/* checks a secondary cache against other caches                            */
int cacheStateCheck(Tags cpuTags[],
                    int cpuThere[], 
                    unsigned int cacheLineOffset)
{ 
  int physTag1;
  int physTag2;
  int cs1;
  int cs2;
  int bs1;
  int cpu1;
  int cpu2;
  int c1;
  int c2;

  if ((cacheLineOffset & 0xfff) == 0)
	printf("checking 0x%x\n", cacheLineOffset);

  for (cpu1=0;cpu1<EverestMaxCPUs;cpu1++)
  {  /* loop through all CPUs */

    if (cpuThere[cpu1])
    { /* this CPU exists */

      cs1=CacheState(cpuTags[cpu1].sTag);
      physTag1=PhysicalTag(cpuTags[cpu1].sTag);
      bs1=BusTagState(cpuTags[cpu1].busTag);

      if (cs1==DirtyShared)
      { /* this is illegal */
         printf("ERROR: Cache %2d offset 0x%05X STagLo=0x%05x marked %s\n",
                cpu1,cacheLineOffset,
                physTag1,cacheStateStr(cs1));
      }

      /**********************/
      /* check the bus tags */
      /**********************/
      c1=((cs1==DirtyExclusive)||(cs1==CleanExclusive))&&(bs1!=BusExclusive);
      c2=((cs1==DirtyShared)||(cs1==Shared))&&(bs1!=BusShared);

      if (c1||c2)
      { /* error */
        printf("ERROR: Cache %2d offset 0x%05X STagLo=0x%05x marked %s\n",
                cpu1,cacheLineOffset,
                physTag1,cacheStateStr(cs1));
        printf("       Corresponding bus tag marked %s\n",
                busTagStateStr(bs1));
      }

      if ((cs1==DirtyExclusive)&&(bs1==BusExclusive))
      { /* compare the tags */
        c1=PhysicalTag(cpuTags[cpu1].sTag);
        c2=BusPhysTag(cpuTags[cpu1].busTag);

        if (!TagsMatch(c1,c2))
        { /* cache tag and bus tag don't match */
          printf("ERROR: Cache %2d offset 0x%05X STagLo=0x%05x marked %s\n",
                cpu1,cacheLineOffset,c1,cacheStateStr(cs1));
          printf("       Corresponding bus tag value is 0x%05x\n",c2);
        }

      } /* comparing secondary cache tag with bus tag */

      /*************************/
      /* Check cache vs. cache */
      /*************************/
      for (cpu2=cpu1+1;cpu2<EverestMaxCPUs;cpu2++)
      { /* loop through the cpu's we're comparing against */

        if (cpuThere[cpu2])
        { /* now we can compare things */

          physTag2=PhysicalTag(cpuTags[cpu2].sTag);

          cs2=CacheState(cpuTags[cpu2].sTag);

          if ((cs1!=Invalid)&&
              (cs2!=Invalid)&&
              (physTag1==physTag2))
          { /* don't go further if either cache line is invalid */
        
            if ((cs1==CleanExclusive)||(cs1==DirtyExclusive))
            { /* current is in exclusive state */
        
              printf("ERROR: Cache %2d offset 0x%04X STagLo=0x%05x marked %s\n",
                      cpu1,cacheLineOffset,physTag1,
                      cacheStateStr(cs1));
              printf("       cache %2d has same tag, marked %s\n",
                      cpu2, cacheStateStr(cs2));
        
            } /* current cache is in exclusive state */
        
            if (cs1==Shared)
            { /* current cache is shared */

              if (cs2!=cs1)
              { /* if they're shared they must be all clean or all dirty */
                printf("ERROR: Cache %2d offset 0x%05X STagLo=0x%05x marked %s\n",
                        cpu1,cacheLineOffset,
                        physTag1,cacheStateStr(cs1));
                printf("       cache %2d has same tag, marked %s\n",
                        cpu2,cacheStateStr(cs2));
              } /* if the caches are shared but different dirty/clean */
              else
              {
                /* at this point, compare the cache lines themselves -   */
                /* they should be the same.                              */
              }
         
            } /* current cache line is shared */
        
          } /* current cache line is valid */

        } /* if cpu2 exists */

      } /* looping through second cpu values */

    } /* if the first cpu exists */

  } /* looping through first cpu's */

  return(0);

} /* end of cacheStateCheck */



static void
dumpCache(void *arg)
{
  CallStruct *c = (CallStruct *)arg;
  _read_tag(CACH_SD,c->readAddr,&(c->tags->sTag)); /* read 2ndary tag */

  c->done=1; /* so the caller knows we're finished */

} /* end of dumpCache */


/* Routine that does the coherency checking.  It should be called from       */
/* symmon with an address and a byte count.                                  */
static void
coherencyCheck(u_numinp_t startAddr, unsigned int byteCount)
{
	notyetmess("coherencyCheck");
} /* end of coherencyCheck */

/*ARGSUSED*/
int
_chkcoh(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	int count;
	u_numinp_t addr;

	if (argc != 2)
		return(1);

	if (parse_sym_range(argv[1], (numinp_t *)&addr, &count, SW_BYTE))
		return(1);

	coherencyCheck(addr, count);

	return(0);
}

#if SABLE
extern int init_ip27() ;

/*
 * init_kldir
 */

static void init_kldir(nasid_t nasid)
{
	kldir_ent_t	*ke;

	ke = KLD_LAUNCH(nasid);

	ke->magic  = KLDIR_MAGIC;
	ke->offset = IP27_LAUNCH_OFFSET;
	ke->size   = IP27_LAUNCH_SIZE;
	ke->count  = IP27_LAUNCH_COUNT;
	ke->stride = IP27_LAUNCH_STRIDE;

	ke = KLD_NMI(nasid);

	ke->magic  = KLDIR_MAGIC;
	ke->offset = IP27_NMI_OFFSET;
	ke->size   = IP27_NMI_SIZE;
	ke->count  = IP27_NMI_COUNT;
	ke->stride = IP27_NMI_STRIDE;

	ke = KLD_KLCONFIG(nasid);

	ke->magic  = KLDIR_MAGIC;
	ke->offset = IP27_KLCONFIG_OFFSET;
	ke->size   = IP27_KLCONFIG_SIZE;
	ke->count  = IP27_KLCONFIG_COUNT;
	ke->stride = IP27_KLCONFIG_STRIDE;

	ke = KLD_PI_ERROR(nasid);

	ke->magic  = KLDIR_MAGIC;
	ke->offset = IP27_PI_ERROR_OFFSET;
	ke->size   = IP27_PI_ERROR_SIZE;
	ke->count  = IP27_PI_ERROR_COUNT;
	ke->stride = IP27_PI_ERROR_STRIDE;

	ke = KLD_SYMMON_STK(nasid);

	ke->magic  = KLDIR_MAGIC;
	ke->offset = IP27_SYMMON_STK_OFFSET;
	ke->size   = IP27_SYMMON_STK_SIZE;
	ke->count  = IP27_SYMMON_STK_COUNT;
	ke->stride = IP27_SYMMON_STK_STRIDE;

	ke = KLD_FREEMEM(nasid);

	ke->magic  = KLDIR_MAGIC;
	ke->offset = IP27_FREEMEM_OFFSET;
	ke->size   = IP27_FREEMEM_SIZE;
	ke->count  = IP27_FREEMEM_COUNT;
	ke->stride = IP27_FREEMEM_STRIDE;

}

void
init_gda(gda_t *gdap)
{
	int i;

	if (get_nasid() != master_nasid)
		return;

	gdap->g_magic = GDA_MAGIC;
	gdap->g_version = GDA_VERSION;
	gdap->g_promop = PROMOP_INVALID;
	gdap->g_masterid = makespnum(master_nasid, *LOCAL_HUB(PI_CPU_NUM));

	gdap->g_vds = 0;

	/* Clear the NASID table */
	for (i = 1; i < MAX_COMPACT_NODES; i++)
		gdap->g_nasidtable[i] = INVALID_CNODEID;

	if (fake_prom)
		for (i = 1; i < numnodes; i++)
			gdap->g_nasidtable[i] = i;

	/* We're the master node so we get to be node 0 */
	if (master_nasid)
		gdap->g_nasidtable[master_nasid] = 0;
	gdap->g_nasidtable[0] = master_nasid;
}

void
propagate_lowmem(gda_t *gdap, __int64_t spb_offset)
{
        int i;
        kldir_ent_t *ke;
        nasid_t nasid;

        /*
         * The GDA is already available on this node (the master)
         * but a pointer to it needs to be propagated to all other nodes.
         * Therefore, start at node 1 (0 is the master) and copy away.
         */
        for (i = 1; i < numnodes; i++) {
                nasid = gdap->g_nasidtable[i];
                if (nasid == INVALID_NASID) {
                        printf("propagate_gda: Error - bad nasid (%d) for cnode %d\n",
                                nasid, i);
                        continue;
                }

                ke = KLD_GDA(nasid);

                ke->magic  = KLDIR_MAGIC;
                ke->pointer = (__psunsigned_t) gdap;
                ke->size   = IO6_GDA_SIZE;
                ke->count  = IO6_GDA_COUNT;
                ke->stride = IO6_GDA_STRIDE;

		/* Copy the SPB to all nodes.
		 * Note: This assumes that while salves spin in their
		 * slave loops, they don't have this stuff in their
		 * caches.  The IP27 prom flushes the cache when a slave
		 * enters the loop so this hsould be okay.
		 * Also note: We're copying into uncached space to avoid calias
		 * problems when copying to node 0.
		 */
		bcopy(SPB, (void *)NODE_OFFSET_TO_K1(nasid, spb_offset),
			sizeof(*SPB));

		/* Set its CALIAS_SIZE */
		*REMOTE_HUB(nasid, PI_CALIAS_SIZE) = PI_CALIAS_SIZE_8K;
        }
}


static already_inited=0;

/*
 * Initialize the portions of the low memory directory that need to be
 * set up in the IO6 prom.
 */
void init_local_dir()
{
	kldir_ent_t *ke;
	nasid_t nasid = get_nasid(), master_nasid;
	int num_nodes;
	gda_t *gdap;
	
	
	if (already_inited){
		printf("init_local_dir: already invoked once, EXIT!\n");
		return;
	}
	already_inited = 1;
	  
	ke = KLD_LAUNCH(nasid);

	if (ke->magic != KLDIR_MAGIC) {
		fake_prom = 1;
		master_nasid = 0;	/* hard-code master nasid */

		init_kldir(nasid);

		ke = KLD_GDA(nasid);

		ke->magic  = KLDIR_MAGIC;
		/* There's only one GDA per kernel.  Set up an absolute ptr. */
		ke->pointer = PHYS_TO_K0(NODE_OFFSET(master_nasid) + IO6_GDA_OFFSET);
		ke->size   = IO6_GDA_SIZE;
		ke->count  = IO6_GDA_COUNT;
		ke->stride = IO6_GDA_STRIDE;

		/* more_ip27prom_init */
			/* ip27prom_simulate() */
			/* ip27prom_epilogue() */
			/* init_ip27() */
			/* init_other_local_dir() */

			/* loop on init_klcfg() on each hib */
			/* loop on init_klcfg_hw() on each hub */

		gdap = (gda_t *)GDA_ADDR(master_nasid);
		init_gda(gdap);
#if 0
		num_nodes=node_probe((lboard_t *)KL_CONFIG_INFO(master_nasid),
				     gdap, 1);
		propagate_lowmem(gdap, SPBADDR);
#endif /* 0 */

	} else {
		/* FIXME_ALEXP -- kludgy hard coded address for SABLE_SYMMON*/
		/* by clearing the jump address, kernel will see 
		 * real prom (well the real prom code running under sable).
		 */
#ifdef	SABLE
		*(long long *)0xa800000000034204 = 0;
#endif	/* SABLE */
	}

	return;
}

void init_other_local_dir(nasid_t nasid)
{
	kldir_ent_t *ke;

	ke = KLD_LAUNCH(nasid);

	if (ke->magic != KLDIR_MAGIC) {
		init_kldir(nasid);
	} else
		printf("init_other_local_dir: nasid %d ALREADY SETUP!\n");

	return;
}


void
sysinit(void)
{
	master_nasid = get_nasid();
	init_local_dir();

	if (fake_prom) {
		int i;

		init_other_local_dir(1);	/* node 1 prom init */

		init_ip27();

		init_klcfg(0);	/* nasid == 0 */
		init_klcfg(1);	/* nasid == 1 */

		/*
		 * XXX During early IO6 bring up ...
		 * Do a IODISCOVER on each hub from the Global Master.
		 * Later the iodiscover can be launched on each hub in MP.
		 */

		init_klcfg_hw(0);	/* nasid == 0 */
		init_klcfg_hw(1);	/* nasid == 1 */

		init_env();
	}

	/*
	 * Steal the first part of the symmon stack for the ELSC buffer.
	 */
	elscuart_setup();

}

#endif	/* !SABLE */

void
notyetmess(char *mess)
{
	printf("%s not implemented for IP21\n", mess);
}
