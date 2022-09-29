
#include <sys/types.h>
#include <sys/sbd.h>

#include <genpda.h>
/*#include <alocks.h>*/
#include <uif.h>
#include "ide.h"
#include "ide_mp.h"
#include "ide_sets.h"
#include "ide_msg.h"
#include <libsc.h>
#include <libsk.h>

#if EVEREST
#include <sys/EVEREST/evconfig.h>
#endif /* EVEREST */

/* imports */
extern int	ide_update_prompt(void);
extern int	ide_whoisthat(int, char *);
extern int	ide_whoami(char *);
extern void	ide_mpsetup(void);
static void	ide_initpdas(void);

static int	ide_cpu_probe(void);
extern int	dump_cpu_info(void);
extern int	dump_global_info(global_info_t *);
extern int	print_stopped(void);
extern int	show_addrmap(void);

#if EVEREST
static void	ide_init_arrays(void);
static void	ide_scanslots(void);
extern int	dump_ev_bdinfo(boardinfo_t *);
#endif


#if MULTIPROCESSOR
extern void	slave_startup(void *);	/* declared in ide/csu.s */
static void	launch_slaves(void);
#ifdef NOTYET
static int	new_master(global_info_t *);
#endif

/* lock global information structure */
int		ii_spl;
lock_t		ide_info_lock;		
#endif /* MULTIPROCESSOR */

extern gen_pda_t	gen_pda_tab[MAXCPU];
extern arcs_reg_t	excep_regs[MAXCPU][NREGS];

/*
 * Only one processor may be executing as IDE_MASTER at any time; the 
 * remaining processors spin in slave loops waiting for the master to
 * launch a diagnostic on their cpu.  When ide initially boots, the
 * processor which was designated 'bootmaster' by the prom inherits the
 * IDE_MASTER mode, but any slave may become the master (and the former
 * master becomes a slave) via the ide_switch_cpus() primitive.  Only 
 * IDE_MASTER interprets scripts (which requires a large stack due to 
 * the resulting recursion), so the master stack floats with the IDE_MASTER
 * designation.  The outgoing master--now a slave--begins executing slave
 * code only, using the standard-sized slave stack for that processor.
 */

/* SLAVE_STACKSIZE is passed in from ide/Makedefs */
#define BOS_TO_TOS(Sbottom, Ssize)	(Sbottom + Ssize)
#define SLAVE_STACKADDR(VID)	\
    BOS_TO_TOS(((char *)slave_stacks + (SLAVE_STACKSIZE*VID)), SLAVE_STACKSIZE)

jmp_buf	slave_jbufs[MAXCPU];	/* for uncaught slave-excepts.*/
jmp_buf	wakeup_jbufs[MAXCPU];	/* awakened slaves use this */
char	slave_stacks[MAXCPU][SLAVE_STACKSIZE];
char	private_putbuf[MAXCPU][PUTBUFSIZE];	/* used by msg_printf */
ide_pda_t ide_pda_tab[MAXCPU];

/*
 * initargv() and initenv() are called early in csu.s, but the master 
 * code can use the global string_list structs.  'environ',
 * 'argv_str', and 'environ_str' must be in pdas and setup_gpdas()
 * must be called before the init* fns.
 */
struct string_list argv_str_tab[MAXCPU];
struct string_list environ_str_tab[MAXCPU];

#if EVEREST
boardinfo_t	evboards = { 0, 0, 0, 0, 0, 0 };
#endif /* EVEREST */


/* g_magic == IDE_IMAGIC when _ide_info is valid */
global_info_t _ide_info = {
	(uint)-1,	/* g_magic	*/
	(uint)-1,	/* vid_cnt	*/
	(uint)-1,	/* vid_max	*/
	(uint)-1,	/* master_vid	*/
	(uint *)(__psint_t)-1,	/* saved_normsp */
	(uint *)(__psint_t)-1,	/* saved_faultsp*/
	NO_VID,	/* talking_vid	*/
	(set_t)0,	/* waiting_vids	*/
	(set_t)0,	/* vid_set	*/
};
global_info_t *giptr = &_ide_info;


#ifdef notdef	/* moved to libsk */
/*
 * next 5 data arrays are now declared in libsk/lib/skutils.c, and are
 * initialized by libsk/lib/skutils:setup_idarr(), which is called 
 * circuitously from ide_mpsetup(), invoked early in main.
 */
! int vid_to_phys_idx[MAXCPU];	/* vid --> phys_index */
! int phys_idx_to_vid[MAXCPU];	/* phys_index --> vid */
! int vid_to_slot[MAXCPU];		/* vid --> slot */
! int vid_to_slice[MAXCPU];		/* vid --> slice */
! cpuid_t ss_to_vid[_SLOTS][_SLICES];	/* <slot,slice> --> vid */
#endif /* notdef */

#if EVEREST
/*
 * if bufptr != NULL, use sprintf to write virt_id's <vid,slot,slice>
 * string to that buffer; else 'printf' the info.  Also cross-checks
 * the mapping of ide's array slot with that of the corresponding mpconf
 * struct.  If no errors, ide_whoisthat returns the phys_idx to which
 * 'virt_id' corresponds, else < 0.
 */
int
ide_whoisthat(int virt_id, char *bufptr)
{
    register uint slot, slice, phys_idx;

    msg_printf(SUM,"ide_whoisthat(vid %d)\n", virt_id);

    if (VID_OUTARANGE(virt_id))
	return(ERR_VP_RANGE);

    slot = vid_to_slot[virt_id];
    slice = vid_to_slice[virt_id];
    phys_idx = SS_TO_PHYS(slot, slice);

#if 	EVEREST
    if (MPCONF[virt_id].phys_id != phys_idx) {
	msg_printf(SUM,"ide ERROR, whoisthat: mpconf[vid %d]==%d, but\n\tide",
		virt_id, MPCONF[virt_id].phys_id);
	msg_printf(SUM, " shows vid (%d) --> phys %d (mpmagic 0x%x)\n",virt_id,
		phys_idx, MPCONF[virt_id].mpconf_magic);
	ide_panic("ide_whoisthat");
    }
#endif
    if (bufptr) {
	if (IDE_ME(ide_mode) == IDE_MASTER)
	    sprintf(bufptr, "ide_master <%.2d/%.2d>: ", slot, slice);
	else
	    sprintf(bufptr, "ide_slave <%.2d/%.2d>: ", slot, slice);

    } else {
	if (IDE_ME(ide_mode) == IDE_MASTER)
	    msg_printf(SUM, "ide_master <%.2d/%.2d>: ", slot, slice);
	else
	    msg_printf(SUM, "ide_slave <%.2d/%.2d>: ", slot, slice);
    }

    return(phys_idx);

} /* ide_whoisthat */
#endif /* EVEREST */

#if EVEREST
/* 
 * cross checks its hw slot/slice with the phys_idx which
 * the mpconf slot indexed by its pda's vid fetches.
 */
int
ide_whoami(char *inbuf)
{
    char cbuf[2*MAXLINELEN], *cp = &cbuf[0];
    register evreg_t slotproc;
    register uint slot, slice, phys_idx, mpc_phys;
    int error = 0;
    int vid_me = cpuid();
    int pda_virtid, pda_physid;

    cbuf[0] = '\0';
    if (inbuf)
	inbuf[0] = '\0';

    if (Reportlevel >= SUM) {
	cp += sprintf(cbuf,  "  %s(v%d): %s\n", 
		(IDE_ME(ide_mode)==IDE_MASTER ? "IDE" : "ide"), vid_me,
		(IDE_ME(ide_mode)==IDE_MASTER ? "-master-" : "-slave-"));
    }

    if (VID_OUTARANGE(vid_me)) {
	msg_printf(IDE_ERR,
	    "ide_whoami: cpuid() %d out of range (largest assigned was %d)\n",
		vid_me, _ide_info.vid_max);
	ide_panic("ide_whoami");
    }

    pda_virtid = GPME(my_virtid);
    pda_physid = GPME(my_physid);

    slotproc = EV_GET_LOCAL(EV_SPNUM);
    slot = (slotproc & EV_SLOTNUM_MASK) >> EV_SLOTNUM_SHFT;
    slice = (slotproc & EV_PROCNUM_MASK) >> EV_PROCNUM_SHFT;
    phys_idx = SS_TO_PHYS(slot, slice);
    if (pda_virtid != vid_me || pda_physid != phys_idx) {
	msg_printf(IDE_ERR,
		"  whoami: pda.vid %d vid_me %d pda.phys %d hwphys %d\n",
		pda_virtid, vid_me, pda_physid, phys_idx);
	error++;
    }
    if (VID_OUTARANGE(pda_virtid)) {
	msg_printf(IDE_ERR,
	    "  ide_whoami: pda.vid %d, cpuid said %d\n", pda_virtid, vid_me);
	msg_printf(IDE_ERR,"  vid range is (0 <= vid <= %d)\n",
		_ide_info.vid_max);
	error++;
    }
    if (!error) {
	mpc_phys = MPCONF[pda_virtid].phys_id;
	if (mpc_phys != phys_idx) {
	    msg_printf(IDE_ERR,"  ide_whoami: mpconf maps vid %d --> ",
		pda_virtid);
	    msg_printf(IDE_ERR,
		"  phys %d;\n\tbut gpda.vid == %d & our physid %d \n",
		mpc_phys, GPRIVATE(vid_me,my_virtid), phys_idx); 
	}
    }

    if (error) {
	msg_printf(IDE_ERR, "  vid <--> phys_idx errors are fatal...\n");
	ide_panic("ide_whoami");
    }

    if (Reportlevel >= VDBG) {    
	cp += sprintf(cp,
		"  ide_whoami: pda.vid %d:cpuid %d, pdaphys %d hwphys %d\n",
		pda_virtid, vid_me, pda_physid, phys_idx);
    }
    if (cp != &cbuf[0]) {
	if (inbuf)
	    strcpy(inbuf, cbuf);
	else
	if (Reportlevel >= SUM)
		msg_printf(JUST_DOIT, "%s", cbuf);
    }

    return(strlen(cbuf));
} /* ide_whoami */

#elif IP30 && MULTIPROCESSOR
int
ide_whoami(char *bufptr)
{
	int cpu = cpuid();
	char *str = cpu == IDE_ME(ide_mode)==IDE_MASTER ? "master" : "slave";

	if (bufptr) {
		sprintf(bufptr,"IDE CPU %d [%s]\n",cpu,str);
		return strlen(bufptr);
	}

	msg_printf(SUM, "IDE CPU %d [%s]\n",cpu,str);

	return 0;
} /* ide_whoami */
#else /* !EVEREST: currently this means SP; return 0 */

/* guard against non-EVEREST MP machines (someday) getting this ifdef */
#ifdef MULTIPROCESSOR
  dontcompile("#define ERROR, ide_mp.c: MULTIPROCESSOR and !EVEREST!")
#else
int
ide_whoami(char *bufptr)
{
    msg_printf(SUM, " IDE");

    if (bufptr != NULL) bufptr [0] = '\0';
    return(0);

} /* ide_whoami */
#endif /* MULTIPROCESSOR */

#endif /* EVEREST version of ide_whoami */


/* 
 * ide_mpsetup() is called from ide's main().  The processor 
 * executing this routine is the initial ide_master.
 */
void
ide_mpsetup(void)
{
#if EVEREST
    msg_printf(VRB,"\n  *mpsetup: ");
#endif

#if _DO_IILOCK
    initlock(&ide_info_lock);
#endif

#if EVEREST
    ide_init_arrays();
#endif

    ide_cpu_probe();		/* <slot,slice> <--> vid mappings */

#if EVEREST
    ide_scanslots();
#endif

    ide_initpdas();

    /*
     * sanity-check everything; with reportlevel <= SUM, if nothing's
     * amiss nothing prints
     */
#ifdef	EVEREST
    dump_cpu_info();
#endif
    dump_global_info(&_ide_info);

#if EVEREST
    dump_ev_bdinfo(&evboards);
#endif

#ifdef MULTIPROCESSOR
    launch_slaves();
#endif
    if (Reportlevel >= VRB)
	show_addrmap();

    return;

} /* ide_mpsetup */


int
show_addrmap(void)
{
    __psunsigned_t addr;
    int len;
    __psunsigned_t endaddr;

    msg_printf(JUST_DOIT, "  Addresses and lengths of major data structs:\n");
    msg_printf(JUST_DOIT,
	"    MAXCPU %d, NREGS %d;  sizeof gpda 0x%x, ide_pda 0x%x\n",
	MAXCPU, NREGS, sizeof(gen_pda_t), sizeof(ide_pda_t));

    addr = (__psunsigned_t)gen_pda_tab;
    len = sizeof(gen_pda_tab);
    endaddr = (addr+len);
    msg_printf(JUST_DOIT, "    &gen_pda_tab 0x%x, len 0x%x (end 0x%x)\n",
	addr, len, endaddr);
    msg_printf(JUST_DOIT, "    &excep_regs 0x%x, length 0x%x (end 0x%x)\n",
	excep_regs, sizeof(excep_regs), 
	((char *)excep_regs+sizeof(excep_regs)));


    msg_printf(JUST_DOIT, "    &ide_pda_tab 0x%x, length 0x%x (end 0x%x)\n",
	ide_pda_tab, sizeof(ide_pda_tab),
	(char *)ide_pda_tab + sizeof(ide_pda_tab));
    msg_printf(JUST_DOIT, "    &slave_stacks 0x%x, length 0x%x (end 0x%x)\n",
	slave_stacks, sizeof(slave_stacks),
	(char *)slave_stacks+sizeof(slave_stacks));
    msg_printf(JUST_DOIT, "    &private_putbuf 0x%x, length 0x%x (end 0x%x)\n",
	private_putbuf, sizeof(private_putbuf), 
	((char *)private_putbuf+sizeof(private_putbuf)));

    msg_printf(JUST_DOIT, "    &slave_jbufs 0x%x, length 0x%x\n",
	&slave_jbufs[0], sizeof(slave_jbufs));
    msg_printf(JUST_DOIT, "    &wakeup_jbufs 0x%x length 0x%x\n",
	&wakeup_jbufs[0], sizeof(wakeup_jbufs));
    msg_printf(JUST_DOIT, "    &argv_str_tab 0x%x length 0x%x\n",
	&argv_str_tab[0], sizeof(argv_str_tab));
    msg_printf(JUST_DOIT, "    &environ_str_tab 0x%x length 0x%x\n",
	&environ_str_tab[0], sizeof(environ_str_tab));

    return 0;
} /* show_addrmap */


static void
ide_initpdas(void)
{
    register int vid, me = cpuid();
#if defined(PDADEBUG)
    register int vid_max;
#endif
    char * addr = slave_stacks[0];
    uint len = sizeof(slave_stacks);

    msg_printf(VRB," -initialize PDAs- ");

    msg_printf(VRB, "&slave stacks[0] 0x%x, length 0x%x (end 0x%x)\n",
	addr, len, (addr+len));

    msg_printf(VDBG+1,
	"  >ide_initpdas: sizeof(ide_pda_t) %d, IDE_PDASIZE %d\n",
	sizeof(ide_pda_t), IDE_PDASIZE);

    if (sizeof(ide_pda_t) != IDE_PDASIZE) {
	msg_printf(IDE_ERR,
	    "!!ide_initpdas: ide_pda_t struct (%d) != IDE_PDASIZE (%d)!!\n",
		sizeof(ide_pda_t), IDE_PDASIZE);
	ide_panic("ide_initpdas");
    }

#if defined(PDADEBUG)
    vid_max = (_ide_info.vid_max >= MAXCPU ? MAXCPU-1 : _ide_info.vid_max);
#endif

    /* set up pda for all cpus */
    for (vid = 0; vid < MAXCPU; vid++) {
	register struct ide_pda_s *i_pdap;
	register struct generic_pda_s *g_pdap;
	register char *fstack;
	register char *istack;

	i_pdap = (struct ide_pda_s *)&ide_pda_tab[vid];
	g_pdap = (struct generic_pda_s *)&gen_pda_tab[vid];

/* 	g_pdap->specific_pda = (void *)&ide_pda_tab[vid]; */
	
	/* 
	 * libsk/lib/fault.c:setup_gpdas() already grabbed master's SPs,
	 * so only the slaves' normal and exception SPs need initializing.
	 * Store the normal and fault-mode values of the stack on which
	 * the current master will run as a slave if he swaps jobs with
	 * a slave (when I implement this feature).  The thread running
	 * as master always uses the master stack, leaving its slave 
	 * stack idle.
	 */
	fstack = SLAVE_STACKADDR(vid);
	istack = (fstack - EXSTKSZ);
	if (vid != me) {
	    g_pdap->pda_fault_sp = (arcs_reg_t *)fstack;
	    g_pdap->pda_initial_sp = (arcs_reg_t *)istack;
	    g_pdap->stack_mode = MODE_NORMAL;
	    i_pdap->ide_mode = IDE_SLAVE;

	} else {
	    i_pdap->ide_mode = IDE_MASTER;
	    _ide_info.saved_normsp = (uint *)istack;
	    _ide_info.saved_faultsp = (uint *)fstack;
	}

	/* 
	 * ide_master uses the global copies of argv_str and environ_str;
	 * slaves must all have private copies in their pdas
	 */
	i_pdap->argv_str = (struct string_list *)&argv_str_tab[vid];
	i_pdap->environ_str = (struct string_list *)&environ_str_tab[vid];

	/*
	 * msg_printf() uses sprintf to format the requested message into
	 * a per-process private buffer located via the pda.  This string
	 * is then transferred either into the large, shared circular 
	 * buffer or written to tty or console, depending on the value 
	 * of 'msg_where'.  Only this final step requires locking.
	 */
	i_pdap->putbuf = private_putbuf[vid];
	i_pdap->putbufndx = 0;

	/* 
	 * each processor has a private proc_talk_t struct, which 
	 * allows the master and it to syncronize and exchange
	 * information during function launches.  Currently none
	 * of the fields require locking as long as the master-slave
	 * protocol is observed.
	 */
	i_pdap->fn_addr = i_pdap->cleanup_addr = 0;
	i_pdap->fn_return = i_pdap->cleanup_return = 0;

/*	i_pdap->fn_handshake = i_pdap->cleanup_handshake = (voll_t)SLAVE_IDLE;*/

	/*
	 * SLAVE_GO_HOME cmd with a pending doit causes slaves to go into
	 * ide_slave_wait() and ACK when they're there.  Initialize the
	 * fields this way to sync-up at initialization time: master can't
	 * issue a command until the slaves set _DONE.
	 */
	i_pdap->doit_done = i_pdap->doit_done = STAT_SDOIT;
	i_pdap->slave_cmd = i_pdap->slave_cmd = SLAVE_GO_HOME;

	i_pdap->fn_argcv.argc = i_pdap->cleanup_argcv.argc = (int)0;
	i_pdap->fn_argcv.argv = i_pdap->cleanup_argcv.argv = (char **)0;

	/*
	 * each slave uses only one private setjmp buffer (when awakened
	 * to launch a diag).  In order to switch masters, the exiting master
	 * must save its entire context for the entering slave to pick up.
	 */
	i_pdap->slave_jbuf = slave_jbufs[vid];
	i_pdap->wakeup_jbuf = wakeup_jbufs[vid];

#if defined(PDADEBUG)
	if (vid <= vid_max) {
	    msg_printf(VDBG+1,
		"    (v%d/p%d: %s: initial sp 0x%x, fault sp 0x%x ",
		g_pdap->my_virtid, g_pdap->my_physid, 
		(vid==me ? "master" : "slave"), (int)g_pdap->pda_initial_sp,
		(int)g_pdap->pda_fault_sp);
	
	    msg_printf(VDBG+1,
		"    stack_mode %d\n  sjbuf 0x%x wjbuf 0x%x, regs 0x%x\n",
		g_pdap->stack_mode,(int)i_pdap->slave_jbuf,
		(int)i_pdap->wakeup_jbuf,(int)g_pdap->regs);
	}
#endif /* PDADEBUG */
	i_pdap->ide_pdamagic = IDE_PDAMAGIC;
    }

} /* ide_initpdas */


#if MULTIPROCESSOR
static void
launch_slaves(void)
{
    int vid, result;
    int master_vid = cpuid();
#if 0
    uint stack;
#endif

    msg_printf(VRB," -launch slaves-\n");

    if (IDE_ME(ide_pdamagic) != IDE_PDAMAGIC) {
	msg_printf(IDE_ERR, "launch_slaves: IDE PDAs aren't initialized!\n");
	ide_panic("launch_slaves");	
    }

    for (vid = 0; vid <= _ide_info.vid_max; vid++) {
	if (vid != master_vid) {
#if 0
	    stack = (uint)GPRIVATE(vid,pda_initial_sp);
#endif

#if defined(LAUNCH_DEBUG)
	    msg_printf(VDBG+1,
		    "  launch vid %d (initial_sp 0x%x fault_sp 0x%x)\n",
	    vid, GPRIVATE(vid,pda_initial_sp), GPRIVATE(vid,pda_fault_sp));
#endif /* LAUNCH_DEBUG */
	    result = launch_slave(vid, slave_startup,0,0,0, 
				(char *)(GPRIVATE(vid,pda_initial_sp)));

	    if (result)
		msg_printf(IDE_ERR,"launch_slaves: vid %d's launch res %d\n",
			vid, result);
	}
/* xxxxx need to set master's slave stack to work if ide_switch_cpus! xxxxx */
/* xxxxx and need to duplicate _fault_sp per slave (in arcs_mp) */
    }

    /* pause for a bit while the slaves start up: then they wait quietly */
    us_delay(US_PER_SEC);
    return;

} /* launch_slaves */
#endif /* MULTIPROCESSOR */

#if EVEREST
static void
ide_init_arrays(void)
{
    int slot, slice, vid;

    msg_printf(VDBG+1, "    MAXCPU %d, slots %d, slices %d, empty %d\n",
	MAXCPU, _SLOTS, _SLICES, MTY);

    /* init structs non-zero for later bogosity checking */
    for (vid = 0; vid < MAXCPU; vid++) {
	vid_to_phys_idx[vid] = MTY;
	phys_idx_to_vid[vid] = MTY;
	vid_to_slot[vid] = MTY;
	vid_to_slice[vid] = MTY;
    }
    for (slot = 0; slot < _SLOTS; slot++)
	for (slice = 0; slice < _SLICES; slice++)
	    ss_to_vid[slot][slice] = (cpuid_t)MTY;

    return;

} /* ide_init_arrays */
#endif /* EVEREST */

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
static int
ide_cpu_probe(void)
{
    int vid, vid_max=0, vid_cnt=0;
    volatile int cpu_cnt;
    set_t vid_set=0;
    int vid_me = cpuid();
#if PROBE_DEBUG
    char sbuf[128];
#endif

    cpu_cnt = setup_idarr(vid_me, &vid_max, &vid_cnt);
    _ide_info.vid_cnt = vid_cnt;
    _ide_info.vid_max = vid_max;
    _ide_info.master_vid = _ide_info.talking_vid = vid_me;
    _ide_info.waiting_vids = 0;
/*    _ide_info.c_pending = NON_COHERENT;	*/
/*    _ide_info.c_state = NON_COHERENT;		*/

    for (vid=0; vid <= vid_max; vid++)
#if EVEREST
	if (vid_to_slot[vid])
#endif /* EVEREST */
	    vid_set |= (SET_ZEROBIT >> vid);

    _ide_info.vid_set = vid_set;

    _ide_info.g_magic = IDE_GMAGIC;

#if PROBE_DEBUG
    if (Reportlevel >= VRB) {
	sprintf_cset(sbuf, _ide_info.vid_set);
	msg_printf(JUST_DOIT,"ide_cprobe: ret %d vid_cnt %d max %d set %s\n",
		cpu_cnt, _ide_info.vid_cnt, _ide_info.vid_max, sbuf);
    }
#endif

    return(vid_cnt);

} /* ide_cpu_probe */


#if EVEREST
static void
ide_scanslots()
{
    register uint slot, slice;
    register uint vpcount = 0;
    register uint virt_id;
    register set_t vpset = 0;
    evbrdinfo_t *brdp;
    evcpucfg_t *cpup;

    msg_printf(VRB," -hinv- ");

    for (slot = 1; slot < EV_MAX_SLOTS; slot++) {
	brdp = &(EVCFGINFO->ecfg_board[slot]);

	switch(brdp->eb_type) {
	  case EVTYPE_IP19:
	    evboards.ip19++;
	    break;
	  case EVTYPE_IP21:
	    evboards.ip21++;
	    break;
	  case EVTYPE_IP25:
	    evboards.ip25++;
	    break;
	  case EVTYPE_IO4:
	    evboards.io4++;
	    break;
	  case EVTYPE_MC3:
	    evboards.mc3++;
	    break;
	  case EVTYPE_EMPTY:
	    evboards.mty++;
	    break;

	  default:
	    evboards.unknown++;
	    break;
	}
    }

    return;

} /* ide_scanslots */
#endif /* EVEREST */



/* begin data-display routines */

#if EVEREST
int
dump_cpu_info()
{
    int slot, slice, mvid, vid;
    evreg_t cpumask = (EV_GET_LOCAL(EV_SYSCONFIG)&EV_CPU_MASK) >> EV_CPU_SHFT;

    mvid = _ide_info.master_vid;
    slot = vid_to_slot[mvid];
    slice = vid_to_slice[mvid];

    /* if reportlevel is < INFO, only abnormalities will show */
    msg_printf(VRB, "  cpu configinfo: %d vids (0..%d)",
		_ide_info.vid_cnt, _ide_info.vid_max);

    msg_printf(VDBG, "  HW maximums:  %d cpus, %d slots, %d slices/cpuboard\n",
	MAXCPU, EV_BOARD_MAX, EV_CPU_PER_BOARD);

    msg_printf(VDBG, "    vid ---> <slot/slice> mappings: \n");

    if (Reportlevel >= VDBG) {
	for (vid = 0; vid < MAXCPU; vid++) {
	    slot = vid_to_slot[vid];
	    slice = vid_to_slice[vid];

	    if (vid <= _ide_info.vid_max) {
		msg_printf(VRB, "  vid %d --> <slot %d/slice %d>\n",
			vid,slot,slice);
	    } else {  /* no valid vids here: sanity check initialized values */
	        if (slot != MTY || slice != MTY)
		    msg_printf(IDE_ERR, "?? (maxvid %d) vindex %d: <%d/%d\n",
			_ide_info.vid_max, vid, slot,slice);
	    }
	}
    }

    /* translate <slot/slice> pair to corresponding virtual processor id */
    msg_printf(VDBG, "\n  <slot/slice)--> vid\n");
    for (slot=0; slot < EV_BOARD_MAX; slot++) {
	if (cpumask & 1) {	/* slot has a cpu board in it */
	    msg_printf(VDBG, "  slot %d cpu board:  ", slot);
	    for (slice=0; slice<EV_CPU_PER_BOARD; slice++) {
		vid = ss_to_vid[slot][slice];
		if (vid == BAD_CPU) {
		    msg_printf(INFO,
			"\n  <cpu slice %d/slot %d is defective>\n",
			slice, slot);
		} else {
		    if (vid != EV_CPU_NONE)
		        msg_printf(VDBG, "  slice %d->vid %d,  ", slice, vid);
		}
	    }
	    msg_printf(VDBG, "\n");
	}
	cpumask = cpumask >> 1;
    }

    return(0);

} /* dump_cpu_info */
#endif /* EVEREST */

int
dump_global_info(global_info_t *ginfop)
{
    char setbuffer[SETBUFSIZ];
    int vid;
#if EVEREST
    uint slot, slice;
#endif /* EVEREST */

    ASSERT(ginfop != NULL);

    if (ginfop->g_magic != IDE_GMAGIC) {
	msg_printf(IDE_ERR, "Invalid global-info magic # (0x%x, not 0x%x)\n",
		ginfop->g_magic, IDE_GMAGIC);
	ide_panic("dump_global_info");
    }

    vid = ginfop->master_vid;
    if (VID_OUTARANGE(vid)) {
	msg_printf(IDE_ERR, "global info: invalid master_vid (%d)\n", vid);
	ide_panic("dump_global_info");
    }

#if EVEREST
    slot = vid_to_slot[vid];
    slice = vid_to_slice[vid];
#endif /* EVEREST */

#if EVEREST
    msg_printf(VRB,"\n  ide_master(%d: %.2d/%.2d); talking vid %d\n",
	ginfop->master_vid, slot, slice, ginfop->talking_vid);
#else
    msg_printf(VRB,"\n  ide_master %d; talking vid %d\n",
	ginfop->master_vid, ginfop->talking_vid);
#endif /* EVEREST */

    sprintf_cset(setbuffer, ginfop->vid_set);
    msg_printf(VRB, "  %d active processor%s: %s\n", (int)ginfop->vid_cnt,
	_PLURAL(ginfop->vid_cnt), setbuffer);

    if (Reportlevel >= VRB) {
        if (ginfop->waiting_vids) {
	    sprintf_cset(setbuffer, ginfop->waiting_vids);
	    msg_printf(INFO, "  waiting slaves: %s\n", setbuffer);
    	} else
	    msg_printf(INFO, "  no waiting slaves\n");
    }
    msg_printf(VRB, 
	"  IDE is operating in <%s> mode;  message level is %d (0x%x)", 
	(_ide_info.c_state==COHERENT ? "coherent" : "non-coherent"),
	Reportlevel, Reportlevel); 

    msg_printf(VDBG+1, "    v_cnt %d, v_max %d  magic 0x%x\n",
	_ide_info.vid_cnt, _ide_info.vid_max, _ide_info.g_magic); 

    return(0);

} /* dump_global_info */


void
dump_idepda(int vid)
{
    int vid_me = cpuid();
    struct ide_pda_s *i_pdap;

    i_pdap = (struct ide_pda_s *)&ide_pda_tab[vid];
    
    if (vid == vid_me)
	msg_printf(JUST_DOIT, "IDE PDA, vid %d  (pda addr 0x%x, sp 0x%x)\n",
		vid, i_pdap, get_sp());
    else
	msg_printf(JUST_DOIT, "IDE PDA, vid %d  (pda addr 0x%x)\n",
		vid, i_pdap);

    msg_printf(JUST_DOIT,
	"    magic 0x%x, mode %d, argv_str 0x%x, env_str 0x%x\n",
	i_pdap->ide_pdamagic, i_pdap->ide_mode, i_pdap->argv_str,
	i_pdap->environ_str);

    msg_printf(JUST_DOIT,
	"    slave_jbuf 0x%x, wakeup_jbuf 0x%x, putbuf 0x%x, putbufndx %d\n",
	i_pdap->slave_jbuf, i_pdap->wakeup_jbuf, i_pdap->putbuf,
	i_pdap->putbufndx);

    return;
}

#if 0
static void
show_SPs(void)
{
    register int vid, me = cpuid();
    register int vid_max;
    register char *fstack;
    register char *istack;

    vid_max = (_ide_info.vid_max >= MAXCPU ? MAXCPU-1 : _ide_info.vid_max);

    msg_printf(JUST_DOIT,"  I'm vid %d: show SPs of vids 0..%d\n",me,vid_max);

    for (vid = 0; vid <= vid_max; vid++) {
	register struct ide_pda_s *i_pdap;
	register struct generic_pda_s *g_pdap;

	i_pdap = (struct ide_pda_s *)&ide_pda_tab[vid];
	g_pdap = (struct generic_pda_s *)&gen_pda_tab[vid];

	/* slave's normal and exception-stack pointers */
	fstack = (char *)g_pdap->pda_fault_sp;
	istack = (char *)g_pdap->pda_initial_sp;

	if (vid <= vid_max) {
	    msg_printf(DBG,"  vid %d pid %d (mode %d): i_sp 0x%x f_sp 0x%x, ",
		vid, g_pdap->my_physid, i_pdap->ide_mode,
		istack, fstack);
	    msg_printf(DBG,"s-mode %d\n    noflt 0x%x !1st %d 1st_epc 0x%x",
		g_pdap->stack_mode,g_pdap->pda_nofault,g_pdap->notfirst,
		g_pdap->first_epc);
	    msg_printf(DBG,"   sjbuf 0x%x wjbuf 0x%x  regs 0x%x\n",
		i_pdap->slave_jbuf,i_pdap->wakeup_jbuf,
		g_pdap->regs);
	}

    }

} /* show_SPs */
#endif


#if EVEREST
int
dump_ev_bdinfo(boardinfo_t *bdp)
{
    int total;

    ASSERT(bdp != NULL);

    total = bdp->ip19 + bdp->ip21 + bdp->ip25 + bdp->io4 + bdp->mc3;

    if (Reportlevel < INFO) {
	msg_printf(INFO, "\n  Configuration:  %d occupied slot%s:\n", 
	total, _PLURAL(total));
	return(0);
    }

    msg_printf(INFO, "\n  Configuration info: %d occupied slot%s:\n", 
	total, _PLURAL(total));

    if (bdp->ip19)
	msg_printf(JUST_DOIT, "    %d IP19 board%s\n", bdp->ip19,
		_PLURAL(bdp->ip19));
    if (bdp->ip21)
	msg_printf(JUST_DOIT, "    %d IP21 board%s\n", bdp->ip21,
		_PLURAL(bdp->ip21));
    if (bdp->ip25)
	msg_printf(JUST_DOIT, "    %d IP25 board%s\n", bdp->ip25,
		_PLURAL(bdp->ip25));
    if (bdp->io4)
	msg_printf(JUST_DOIT, "    %d IO4 board%s\n", bdp->io4,
		_PLURAL(bdp->io4));
    if (bdp->mc3)
	msg_printf(JUST_DOIT, "    %d MC3 board%s\n", bdp->mc3,
		_PLURAL(bdp->mc3));
    if (bdp->unknown)
	msg_printf(JUST_DOIT, "===> %d board%s were of unknown type\n", 
		bdp->unknown, _PLURAL(bdp->unknown));
    msg_printf(JUST_DOIT, "\n");

    return(0);

} /* dump_ev_bdinfo */
#endif /* EVEREST */


#if MULTIPROCESSOR
int
print_stopped(void)
{
    char setbuffer[SETBUFSIZ];
    set_t vset;

    if (!_ide_info.waiting_vids) {
	msg_printf(JUST_DOIT, "  Currently no IDE slaves are available\n");
    } else {	/* just take a snapshot while holding the lock */
	LOCK_IDE_INFO();
	vset = _ide_info.waiting_vids;
	UNLOCK_IDE_INFO();

	sprintf_cset(setbuffer, vset);
	msg_printf(JUST_DOIT, "  waiting slaves: %s\n", setbuffer);
    }

    return(_ide_info.vid_cnt);

} /* print_stopped */
#endif /* MULTIPROCESSOR */


/* XXXXXXXXXXXXXXXXXXXXXXXX end of included fns XXXXXXXXXXXXXXXXXXXXXXXXXXXXX */




#ifdef NOTYET
* int
* new_master(vp_info_t *vp)
* {
* 
*   CHANGE both slave and master's fault_sp PDA values
*   change vp vid, slot,slice
* 
*   and update ide_update_prompt
* 
* } /* new_master */
* 
* 
* /*
*  * Functions to acquire and release control of the single
*  * UART shared by multiple processors.
*  */
* void
* ACQUIRE_USER_INTERFACE(void)
* {
* again:
*  
* 	if (_ide_info.talking_vid == NO_VID) {
* 		_ide_info.talking_vid = cpuid();
* 		msg_printf(DBG, "(VID %d) ", cpuid());
* 	}
* 	if (_ide_info.talking_vid != cpuid()) {
* 		_ide_info.waiting_vids |= (1ULL<<cpuid());
* 		while (_ide_info.talking_vid != cpuid()) {
* 			if (cpuid() == 0) {
* 				if (early_exit_check()) {
* 					_ide_info.waiting_vids &= ~(1ULL<<cpuid());
* 					goto again;
* 				}
* 			}
* 		}
* 		_ide_info.waiting_vids &= ~(1ULL<<cpuid());
* 		ide_update_prompt();
* 
* 		return;
* 	}
* 
* }
* 
* RELEASE_USER_INTERFACE(int id)
* {
* 	_ide_info.talking_vid = id;
* }
* 
* 
* extern int consgetc(int);
* int
* early_exit_check()
* {
* 	int retval = 0;
* 
* 	if (_ide_info.talking_vid == NO_VID) {
* 		if ((char)consgetc(0) == 1 /* CNTRL-A */) {
* 			retval = 1;
* 		}
* 	}
* 
* 	return(retval);
* }
#endif /* NOTYET */
 

#ifdef NONONO
> /*
>  * Pass control of the user interface to the specified
>  * processor, and wait 'till someone tells us what to do.
>  */
> _switch_cpus(int argc, char **argv)
> {
> 	numinp_t newvid;
> 	int vid = cpuid();
> 
> 	if (argc == 1) {
> 	    msg_printf(SUM, "Currently connected to cpu 0x%x (slot=0x%x proc=0x%x)\n", 
> 			vid, vid_to_slot[vid], vid_to_slice[vid]);
> 
> 		print_stopped();
> 		return(0);
> 	}
> 
> 	if (argc != 2)
> 		return(1);
> 
> 	argv++;
> 	if (parse_sym(*argv, &newvid) && *atob_s(*argv, &newvid))
> 		_dbg_error("Illegal cpuid: %s", *argv);
> 		/* doesn't return */
> 
> 	if ((newvid < 0) || (newvid > MAXCPU)) {
> 		msg_printf(SUM, "virt_id %d is out of range\n", newvid);
> 		return(1);
> 	}
> 
> 	if (!(waiting_vids & (1ULL<<newvid))) {
> 		msg_printf(SUM, "cpu %s is not stopped\n", *argv);
> 		print_stopped();
> 		return(1);
> 	}
> 
> 	RELEASE_USER_INTERFACE(newvid);
>  
>  new_master(&_ide_info);
>  and update ide_prompt
>
> 	ACQUIRE_USER_INTERFACE();
> }
>
>
> void
> do_command_parser(void)
> {
> 	dbg_mode = MODE_DBGMON;
> 
> 	ACQUIRE_USER_INTERFACE();
> 
> 	if (setjmp(private._restart_buf)) {
> 		ACQUIRE_USER_INTERFACE();
> 		putchar('\n');
> 	}
> 
> 	symmon_spl();
> 	command_parser(ctbl, PROMPT);
> }
#endif /* NONONO */


void
slave_dummy(void) {
	msg_printf(SUM, "Slave executing dummy function\n");
}

