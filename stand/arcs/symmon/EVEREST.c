/*
 * EVEREST.c - EVEREST-specific routines for symmon
 *
 */
#ident "symmon/EVEREST.c: $Revision: 1.33 $"


#include <string.h>
#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <libsc.h>
#include "dbgmon.h"
#include "fault.h"
#include "parser.h"
#include "saioctl.h"
#include "mp.h"
#include "cache.h"
#include "sys/EVEREST/everest.h"
#include "sys/EVEREST/evconfig.h"
#include "sys/EVEREST/addrs.h"
#include "sys/EVEREST/everror.h"


#define _SLOTS  EV_BOARD_MAX
#define _SLICES EV_MAX_CPUS_BOARD

extern int bdeb;

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
 */
__psunsigned_t _dbgstack = SYMMON_STACK;

static int _bustag(int, char *[], char *[], struct cmd_table *xxx);
static int _chkcoh(int, char *[], char *[], struct cmd_table *xxx);
static int _reset(int, char *[], char *[], struct cmd_table *xxx);
static int _dc(int, char *[], char *[], struct cmd_table *xxx);
static int _wc(int, char *[], char *[], struct cmd_table *xxx);
static int _info(int, char *[], char *[], struct cmd_table *xxx);
extern int _switch_cpus(int, char *[], char *[], struct cmd_table *xxx);
static int _hwstate(int, char *[], char *[], struct cmd_table *);

/*
 * Platform-Dependent command table.
 */
struct cmd_table pd_ctbl[] = {
	{ "bustag",	_bustag,	"bustag:\t\tbustag [RANGE]"},
	{ "chkcoh",	_chkcoh,	"chkcoh:\t\tchkcoh [RANGE]"},
	{ "cpu",	_switch_cpus,	"cpu:\t\tcpu [CPUID]" },
	{ "reset",	_reset,		"reset:\t\treset system"},
	{ "dc",		_dc,		"dc:\t\tdc SLOT REGISTER"},
	{ "wc",		_wc,		"wc:\t\twc SLOT REGISTER VALUE"},
	{ "info",	_info,		"info:\t\tsystem bus information"},
	{ "hwstate", 	_hwstate,	"hwstate:\thwstate [-f]"}, 
	{ 0,		0,		"" }
};


pd_name_table_t pd_name_table[] = {
	/*
	 * Everest addresses for fixed-location 
	 * software structures.
	 */
	{ "SPB",		SYSPARAM_ADDR },
	{ "EVCFG",		EVCFGINFO_ADDR },
	{ "EVCFGINFO",		EVCFGINFO_ADDR },
	{ "MPCONF",		MPCONF_ADDR },

	/*
	 * Everest local resource addresses.
	 */
	{ "SPNUM",		EV_SPNUM },
	{ "KZRESET",		EV_KZRESET },
	{ "SENDINT",		EV_SENDINT },
	{ "SYSCONFIG",		EV_SYSCONFIG },
#ifndef IP25
	{ "WGDST",		EV_WGDST },
#endif
#if IP19
	{ "WGCNTRL",		EV_WGCNTRL },
#endif
#if IP21
	{ "WGA",		EV_WGA },
	{ "WGB",		EV_WGB },
#endif
	{ "UART_BASE",		EV_UART_BASE },
	{ "UART_CMD",		EV_UART_CMD },
	{ "UART_DATA",		EV_UART_DATA },
	{ "IP0",		EV_IP0 },
	{ "IP1",		EV_IP1 },
	{ "HPIL",		EV_HPIL },
	{ "CEL",		EV_CEL },
	{ "CIPL0",		EV_CIPL0 },
	{ "CIPL124",		EV_CIPL124 },
	{ "IGRMASK",		EV_IGRMASK },
	{ "ILE",		EV_ILE },
	{ "ERTOIP",		EV_ERTOIP },
	{ "CERTOIP",		EV_CERTOIP },
	{ "RO_COMPARE",		EV_RO_COMPARE },
	{ "CONFIGREG_BASE",	EV_CONFIGREG_BASE },
	{ "RTC",		EV_RTC },
	{ "LED_BASE",		EV_LED_BASE },
#if IP19
	{ "EBUSRATE0_LOC",	EV_EBUSRATE0_LOC },
	{ "EBUSRATE1_LOC",	EV_EBUSRATE1_LOC },
	{ "EBUSRATE2_LOC",	EV_EBUSRATE2_LOC },
	{ "EBUSRATE3_LOC",	EV_EBUSRATE3_LOC },
	{ "PGBRDEN_LOC",	EV_PGBRDEN_LOC },
	{ "CACHE_SZ_LOC",	EV_CACHE_SZ_LOC },
	{ "EPROCRATE0_LOC",	EV_EPROCRATE0_LOC },
	{ "EPROCRATE1_LOC",	EV_EPROCRATE1_LOC },
	{ "EPROCRATE2_LOC",	EV_EPROCRATE2_LOC },
	{ "EPROCRATE3_LOC",	EV_EPROCRATE3_LOC },
	{ "RTCFREQ0_LOC",	EV_RTCFREQ0_LOC },
	{ "RTCFREQ1_LOC",	EV_RTCFREQ1_LOC },
	{ "RTCFREQ2_LOC",	EV_RTCFREQ2_LOC },
	{ "RTCFREQ3_LOC",	EV_RTCFREQ3_LOC },
	{ "WCOUNT0_LOC",	EV_WCOUNT0_LOC },
	{ "WCOUNT1_LOC",	EV_WCOUNT1_LOC },
	{ "BE_LOC",		EV_BE_LOC },
	{ "ECCSB_DIS",		EV_ECCSB_DIS },
	{ "BUSTAG_BASE",	EV_BUSTAG_BASE },
	{ "WGINPUT_BASE",	EV_WGINPUT_BASE },
	{ "WGCOUNT",		EV_WGCOUNT },
	{ "EAROM_BASE",		EV_EAROM_BASE },
	{ "PROM_BASE",		EV_PROM_BASE },
	{ "IW_TRIG_LOC",	EV_IW_TRIG_LOC },
	{ "RR_TRIG_LOC",	EV_RR_TRIG_LOC },
#endif

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

/*
 * use ss_to_vid array in arcs/lib/libsk/lib/skutils.c
 * for physical slot/slice --> virtual ID conversions.
 */
extern cpuid_t ss_to_vid[_SLOTS][_SLICES];     /* <slot,slice> --> vid */

static int cpus_enabled;	/* no of enabled cpus in system */

/*
 * Probe everest hardware to find out where cpu boards are,
 * and set up mappings between physical CPU IDs and logical
 * CPU IDs according to PROM mappings.
 *
 * Some cpus may have been disabled by the PROM, but they
 * may be enabled later under operator control.
 *
 * Returns maximum logical cpu ID (which should be the same as the
 * maximum number of cpu's, though some may be disabled).
 */
int
cpu_probe(void)
{
	int slot, cpu;
	cpuid_t cpuid, max_cpuid;
	evreg_t cpumask;

	cpus_enabled=0;

	cpumask = (EV_GET_LOCAL(EV_SYSCONFIG) & EV_CPU_MASK) >> EV_CPU_SHFT;

	for (slot=0; slot<_SLOTS; slot++) {
		if (cpumask & 1) {    /* Found a slot with a processor board */
			for (cpu=0; cpu<_SLICES; cpu++) {
				if (cpu >= EV_CPU_PER_BOARD) {	/* for IP21 */
					ss_to_vid[slot][cpu] = EV_CPU_NONE;
					continue;
				}
				if (EVCFG_CPUDIAGVAL(slot,cpu) &&
				  !(EVCFG_CPUDIAGVAL(slot,cpu)==EVDIAG_CPUREENABLED)) {
					ss_to_vid[slot][cpu] = EV_CPU_NONE;
					continue;
				}
				cpuid = EVCFG_CPUID(slot,cpu);
#if LARGE_CPU_COUNT_EVEREST
				if ((cpuid & 0x01) && (cpuid<REAL_EV_MAX_CPUS))
					cpuid = EV_MAX_CPUS - cpuid;
#endif
				cpuid_to_slot[cpuid] = slot;
				cpuid_to_cpu[cpuid] = cpu;
				ss_to_vid[slot][cpu] = cpuid;
				if (cpuid > max_cpuid)
					max_cpuid = cpuid;
				if (EVCFG_CPUSTRUCT(slot,cpu).cpu_enable)
					cpus_enabled++;
			}
		} else {	/* No processor board in this slot */
			for (cpu=0; cpu<_SLICES; cpu++)
				ss_to_vid[slot][cpu] = EV_CPU_NONE;
		}
		cpumask = cpumask >> 1;
	}
	return(max_cpuid);
}

/*
 * Determine whether a given CPU is present and enabled.
 */
int 
cpu_enabled(cpuid_t virtid) 
{
 	int slot, slice;
 	int physid;
 
 	if ((virtid < 0) || (virtid > EV_MAX_CPUS))
 		return 0;
 
#ifdef LARGE_CPU_COUNT_EVEREST
	if (virtid & 0x01) {
	 	physid = MPCONF[EV_MAX_CPUS - virtid].phys_id;
	} else
#endif
 	physid = MPCONF[virtid].phys_id;
 
 	slot = (physid & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
 	slice = (physid & EV_PROCNUM_MASK);

	if ((EVCFGINFO->ecfg_board[slot].eb_type & EVCLASS_MASK) !=EVCLASS_CPU)
		return 0;

	if ((EVCFG_CPUSTRUCT(slot, slice).cpu_vpid) != virtid)
		return 0;

	return (EVCFG_CPUSTRUCT(slot, slice).cpu_enable);
}

void 
cpu_physical(int virtid, int *slot, int *slice)
{
        int	physid;

#ifdef LARGE_CPU_COUNT_EVEREST
	if (virtid & 0x01) {
	 	physid = MPCONF[EV_MAX_CPUS - virtid].phys_id;
	} else
#endif
	physid = MPCONF[virtid].phys_id;

	*slot = (physid & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
	*slice = (physid & EV_PROCNUM_MASK);
}

/*
 * symmon_fault -- display info pertinent to fault
 */
void
symmon_fault(void)
{
	int epc;

#if R4000
	if (private.dbg_exc == EXCEPT_NMI)
		epc = E_ERREPC;
	else
#endif
		epc = E_EPC;

	
	if (private.dbg_exc == EXCEPT_NMI)
		private.nmi_int_flag = 1;

	ACQUIRE_USER_INTERFACE();

	printf("\nSYMMON EXCEPTION: %r\n", private.dbg_exc, except_desc);
	/* try to call disassembler */
	_show_inst((inst_t *)private.exc_stack[epc]);	
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
default:
		break;
	}

	RELEASE_USER_INTERFACE(NO_CPUID);
}

#if IP19 || IP25
static void
show_bustag(int index)
{
	evreg_t tag;
	long long *bustag_addr = 
		(long long *)((__psunsigned_t)EV_BUSTAG_BASE + (index<<3));

	tag = load_double(bustag_addr);
	printf("0x%x:\t0x%llx\n", index, tag);
}
#endif	/* IP19 || IP25 */

#include "cache.h"
extern int _sidcache_size, _scache_linemask;
/*
 * Print duplicate bus tags.
 * User invokes with a starting address or starting index, and a
 * count of the number of tags to be displayed.
 */
/*ARGSUSED*/
int
_bustag(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
#ifdef IP19 || IP25
	int index, count;
	numinp_t addr;
	int i;

	int num_bustags = _sidcache_size / _scache_linemask;

	if (argc > 2)
		return(1);

	if (argc == 1) {
		index = 0;
		count = num_bustags;
	} else {
		if (parse_sym_range(argv[1], &addr, &count, SW_BYTE))
			return(1);

		/*
		 * If range specified as K0SEG addresses, convert
		 * to bus tags indices.
		 */
		if (addr > _sidcache_size) {
			index = (addr & (_sidcache_size-1)) >> (_scache_linemask+1);
		} else
			index = addr;
	}

	for (i=0; i<count; i++)
		show_bustag(i+index % num_bustags);

	return(0);
#else
	notyetmess("bustag");
	return 0;
#endif
}

/*
 * Reset the machine.
 */
/*ARGSUSED*/
int
_reset(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	EV_SET_LOCAL(EV_KZRESET, 1);
	return 0;		/* keep compiler happy */
}


#define READ 0
#define WRITE 1

int
conf_reg(int rw, int argc, char *argv[])
{
    int slot, reg;

    /* Check the argument count */
    if ((rw == READ && argc != 3) || (rw == WRITE && argc != 4))
	return 1;

    if (*atob(argv[1], &slot)) {
	printf("Error: cannot parse slot '%s'\n", argv[1]);
	return 0;
    }

    if (*atob(argv[2], &reg)) {
	printf("Error: cannot parse register '%s'\n", argv[2]);
	return 0;
    }

    if (slot < 1 || slot > EV_MAX_SLOTS) {
	printf("Error: slot must between 0 and %d\n", EV_MAX_SLOTS);
	return 0;
    }

    if (rw == READ) {
	printf("Slot %d register 0x%x: 0x%Lx\n", slot, reg, 
		load_double((long long*)EV_CONFIGADDR(slot, 0, reg)));
    } else {
   	long long value; 
		
	if (*atob_L(argv[3], &value)) {
	    printf("Error: cannot parse value '%s'\n", argv[3]);
	    return 0;
	}

	printf("Writing 0x%Lx\n", value);
	store_double((long long*)EV_CONFIGADDR(slot, 0, reg), value);
    }

    return 0;
}

/*ARGSUSED*/
int
_dc(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
    return conf_reg(READ, argc, argv);
}

/*ARGSUSED*/
int
_wc(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
    return conf_reg(WRITE, argc, argv);
}


/*ARGSUSED*/
int
_info(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
    businfo(0);
    return 0;
}

#define	KP_HWSTATE	"hwstate"

extern	cpumask_t	waiting_cpus;
extern	void		get_ertoip();
/* Dump system hardware state */
/*ARGSUSED*/
int
_hwstate(int argc, char *argv[], char *argp[], struct cmd_table *xxx)
{
	
	char		*aargv[3];
	int		slot,cpu,cpuid, first=1;

	if (argc > 2)
	    return(0);

	if((CPUMASK_IS_ZERO(waiting_cpus)) && (argc == 1) && (cpus_enabled > 1) ){
	    printf("CPUs are not stopped. Use 'stop' command first\n");
	    printf("Or use '%s -f' (ERTOIPs may not be dumped..)\n",argv[0]);
	    return(0);
	}
	if ((argc == 2) && ((argv[1][0] != '-') || (argv[1][1] != 'f')))
	    return(0);

	for (slot=1; slot < _SLOTS; slot++){
	    for(cpu=0; cpu < _SLICES; cpu++){
		if (((cpuid = ss_to_vid[slot][cpu]) == EV_CPU_NONE) ||
		    (EVCFG_CPUSTRUCT(slot,cpu).cpu_enable == 0)) /* disabled */
		    continue;
		if (slave_func(cpuid, get_ertoip, 0)){
		    if (first){
			printf("Failed to get ERTOIP for CPU(s)(vid): ");
			first = 0;
		    }
		    printf("%d ", cpuid);
		}
	    }

	}
	printf("\n");
	aargv[0] = KP_HWSTATE;
	aargv[1] = "0";
	aargv[2] = NULL;
	return(_kpcheck(2, aargv, 1));

}

void
get_ertoip()
{
        cpuerror_t *ce = &(EVERROR->cpu[cpuid()]);
        ce->cc_ertoip = (ushort)EV_GET_REG(EV_ERTOIP);
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



#if !TFP
static void
dumpCache(void *arg)
{
  CallStruct *c = (CallStruct *)arg;
  _read_tag(CACH_SD,c->readAddr,&(c->tags->sTag)); /* read 2ndary tag */
  c->tags->busTag=load_double((long long *)Addr2BusTag(c->readAddr));

  c->done=1; /* so the caller knows we're finished */

} /* end of dumpCache */
#endif


/* Routine that does the coherency checking.  It should be called from       */
/* symmon with an address and a byte count.                                  */
static void
coherencyCheck(u_numinp_t startAddr, unsigned int byteCount)
{
#if IP19
  Tags cpuTags[EverestMaxCPUs];
  int cpuNum;
  u_numinp_t thisAddr;
  u_numinp_t endAddr;
  CallStruct c;
  int retCode;
  int cpuThere[EverestMaxCPUs];

  endAddr=startAddr+byteCount;

  for (thisAddr=startAddr;thisAddr<endAddr;thisAddr+=SCacheLineSize)
  { /* check each cache line within the desired range */
    for (cpuNum=0;cpuNum<EverestMaxCPUs;cpuNum++)
    { /* build an array of tags for this line across all CPUs */
      c.readAddr=thisAddr;
      c.done=0;
      c.tags = &cpuTags[cpuNum];
      retCode=slave_func(cpuNum,dumpCache,&c);

      if (retCode==0)
      { /* that CPU responded intelligently */
        cpuThere[cpuNum]=1;
        while (!c.done); /* wait for the other process to finish */
      }
      else cpuThere[cpuNum]=0; /* CPU isn't there */

    } /* looping through all CPUs */

    /* at this point, the array should have tag information for one */
    /* line across all cpu's.  Check for consistency across those cpu's */
    cacheStateCheck(cpuTags, cpuThere, thisAddr);

  } /* looping through all lines in the user's desired range */

#else
	notyetmess("coherencyCheck");
#endif
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

#if EVEREST && SABLE
void
sysinit(void)
{
	extern int _prom;

	_prom = 1;
	EVCFGINFO->ecfg_debugsw |= VDS_MANUMODE;
#ifdef IP19
	EVCFGINFO->ecfg_board[2].eb_type = EVTYPE_IP19;
#elif IP21
	EVCFGINFO->ecfg_board[2].eb_type = EVTYPE_IP21;
#elif IP25
	EVCFGINFO->ecfg_board[2].eb_type = EVTYPE_IP25;
#endif
	EVCFGINFO->ecfg_board[2].eb_cpuarr[2].cpu_enable = 1;
	EVCFGINFO->ecfg_memsize = 32768;	/* 8 mb */
	MPCONF[2].mpconf_magic = MPCONF_MAGIC;
	MPCONF[2].virt_id = 0;
	MPCONF[2].pr_id = get_prid();
}
#endif

#if TFP || R10000
void
notyetmess(char *mess)
{
	printf("%s not implemented for IP21\n", mess);
}
#endif
