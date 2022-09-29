/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996 Silicon Graphics, Inc.           *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/***********************************************************************\
*	File:		sysinit.c					*
*									*
*	Contains the code which completes system startup, runs 		*
*	standard diags, checks the system configuration state,		*
*	and finally calls main.						*
*									*
\***********************************************************************/

#ident  "$Revision: 1.108 $"

#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/sbd.h> 
#include <pon.h>
#include <arcs/errno.h>
#include <arcs/io.h>
#include <arcs/hinv.h>
#include <arcs/folder.h>
#include <gfxgui.h>
#include <style.h>
#include <ksys/elsc.h>
#include <libsc.h>
#include <libsk.h>
#include <libkl.h>
#include <saioctl.h>
#include <setjmp.h>
#include "io6prom_private.h"
#include <klcfg_router.h>
#include <sys/SN/klpart.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/SN0/sn0drv.h>

#include <sys/SN/launch.h>	/* For slave launch testing */

#include "ip27prom.h"
extern ulong  get_BSR(void) ;
extern int bssmagic ;

#include <sys/SN/gda.h>
#include <sys/SN/nvram.h>
#include <sys/SN/kldiag.h>
#include <sys/SN/klconfig.h>

#include "traverse_nodes.h"

void main(void);
void init_prom_soft(int);
void reboot(void);
void config_cache(void);
void init_tlb(void);
int  version(void);
extern void dump_diag_results(int);
extern void check_early_init(void);
extern void checkclk_mod_all(void) ;
extern void check_edtab(void) ;
extern void nvram_disabled_io_info_update(void);
void dump_inventory(void) ;
static void alloc_memdesc(void);
static void show_halt_msg(void);
static nasid_t update_master_baseio(void) ;
int    update_old_master_baseio(nasid_t) ;
__psunsigned_t io6prom_elsc_init(void) ;
int	read_skipauto(void) ;
void	check_part_reboot(void) ;
extern void ei_pwr_cycle_launch(void);

#ifdef SABLE
extern int fake_prom;
extern int sc_sable;
extern int sk_sable;
#endif /* SABLE */

extern __psint_t _gp;
extern int num_nodes;
extern void tlb_setup(nasid_t);
void nvaddr_init(void) ;
extern void slave_print_mod_num(nasid_t);
#if 0
extern int propagate_serial_num() ;
#endif

void elsc_print_mod_num(int);
__psunsigned_t getnv_base_lb(lboard_t *) ;

extern 	__psunsigned_t	nvram_base ;
	__psunsigned_t	ioc3uart_base ;

nasid_t master_nasid;
nasid_t master_baseio_nasid;
extern moduleid_t      master_baseio_modid ;
extern slotid_t        master_baseio_slotid ;
extern char            *master_console_path ;

extern unsigned char	prom_versnum,prom_revnum;

long Stack[IO6PROM_STACK_SIZE / sizeof(long)] = {0};

__uint64_t         diag_malloc_brk ;
     
/*
 * Set master control flags in the prom.
 */

void 
sysinit(int diag_mode)
{
    int restart;
    char restart_ll ;
    int node, slice;
    int i;
    kl_config_hdr_t *klch_m, *klch_b ;

#ifdef SABLE
    hubuart_init(); /* Assembly version */
    sc_sable = 1;
    sk_sable = 1;
#endif

    /* If we get here, we have been successfully loaded.
       print the rest of the Loading ... msg */

    printf("DONE\n\n") ;

    /* Display the IO6PROM sign on message */
    (void) version();

    master_nasid = get_nasid();
    diag_set_mode(diag_mode);

#if defined (HUB_ERR_STS_WAR)
    if (do_hub_errsts_war() == 0)
	printf("Force read error for HUB_ERR_STS_WAR failed\n");
#endif

    /* set up GDA on master_nasid */
    init_local_dir();

    io6prom_elsc_init() ;

    /* Choose one of the local master io6 as global master */
    /* change the KLCONFIG HEADER of the master_nasid so that its
       console_t points to the global master baseio. Any onw who
       needs the console uart base can get it from here. 
    */
    if ((master_baseio_nasid = update_master_baseio()) >= 0) {
    	klch_m = KL_CONFIG_HDR(master_nasid) ;
    	klch_b = KL_CONFIG_HDR(master_baseio_nasid) ;
    	bcopy((void *)&klch_b->ch_cons_info, 
	      (void *)&klch_m->ch_cons_info, 
	      sizeof(console_t)) ;
    } else { 
    /* For compatibility with old proms. Old proms do not have
       the GLOBAL_MASTER_IO6 and LOCAL_MASTER_IO6 bits set.
       Set it for atleast the master_nasid.
     */
	update_old_master_baseio(master_nasid) ;
    }

    nvaddr_init() ;

    /* 
     * PV 669589.  If memory banks 0 and 1 are swapped, the correct
     * behavior is for the prom to also swap all other pairs of banks.
     * New versions of the proms know how to do this.  The IO prom must be
     * forwards and backwards compatible with all versions of the CPU
     * prom, but we don't know whether the CPU prom which loaded this IO
     * prom had the old or new behavior.  If the CPU prom had the new
     * behavior, it "unswapped" the banks before loading the IO prom,
     * thereby re-creating the old behavior.  Old CPU proms have the old
     * behavior (obviously).  Here, we make the behavior correct by
     * swapping the config information for banks 2 and up.
     */ 
    swap_some_memory_banks();

    /* Initialize the globals pertaining to cache size */
    /* Finds out values of dmabuf_linemask, _icache_linemask
       _dcache_linemask based on the cache in Global Master.
     */

    config_cache();
    verbose_faults = 1;

    set_SR(IO6PROM_SR & ~(SR_IE));

    init_env();

#if defined (HUB_ERR_STS_WAR)
    if (do_hub_errsts_war() == 0)
	printf("Force read error for HUB_ERR_STS_WAR failed\n");
#endif

    nvram_prom_version_set(prom_versnum,prom_revnum);

    /* 
     * These need to be set to get stdio and certain prom initialization
     * to behave correctly.
     */
    _prom      = 1;
    stdio_init = 0;

    init_prom_soft(1);

#if defined (HUB_ERR_STS_WAR)
    if (do_hub_errsts_war() == 0)
	printf("Force read error for HUB_ERR_STS_WAR failed\n");
#endif

    /* Run the checkclk test on all modules */
    if ((GET_DIAG_MODE(diag_mode) == DIAG_MODE_HEAVY) ||
        (GET_DIAG_MODE(diag_mode) == DIAG_MODE_MFG))
        checkclk_mod_all() ;

    /* Dump system config info and diag results */
    dump_diag_results(diag_mode);

    /* Do the right thing depending on how the RESTART stuff is set */

    /*
     * Bug PV 464448
     * init_prom_soft above may not return if there was a
     * scsi time out, and the war code reset the system.
     * We may lose the restart info in such a case. The fix
     * involves storing the reboot state also in nvram.
     * Read that location and if it appears that we have a
     * valid value there, use it as the restart value and
     * reset the nvram. This same mechanism can be used to
     * remember additional state. upto 5 bits.
     */
    restart_ll = get_scsi_to_ll_value() ;
    if ((restart_ll & (SCSI_TO_LL_MAGIC | SCSI_TO_LL_REBOOT)) == 
	 (SCSI_TO_LL_MAGIC | SCSI_TO_LL_REBOOT)) {
	restart = PROMOP_REBOOT ;
        set_scsi_to_ll_value(0);
    }
    else
    	restart = diag_get_lladdr();
	
    diag_set_lladdr(PROMOP_INVALID);

#if defined (HUB_ERR_STS_WAR)
    if (do_hub_errsts_war() == 0)
	printf("Force read error for HUB_ERR_STS_WAR failed\n");
#endif

#ifdef DEBUG
    printf("printing module numbers...\n");
#endif

    /* Loop through all the nodes and launch the module printing code on
       one of its processors. This prints each module's number twice but
       makes the code simpler.
    */
    for (node = 1; node < num_nodes; node++) {
      for (slice = 0; slice < CPUS_PER_NODE; slice++) {
	klcpu_t *klcpu;
        cpuid_t cid = cnode_slice_to_cpuid(node, slice);
	if (cid != -1)
	    klcpu = get_cpuinfo(cid);
	else
	    klcpu = NULL;

        /* if it's there and it's enabled have it print its module number */
        if (klcpu && KLCONFIG_INFO_ENABLED(&(klcpu->cpu_info))) {

#if DEBUG
          printf("printing module number on node %d, cpu %d\n",
                 node,slice);
#endif
          
          LAUNCH_SLAVE(GDA->g_nasidtable[node],slice,
                (launch_proc_t)PHYS_TO_K0(kvtophys(
			(void *)slave_print_mod_num)),
                       get_nasid(),
	(void*)TO_NODE(GDA->g_nasidtable[node],IP27PROM_STACK_A + 1024),
                       (void*)0);
          break;
        }
      } /* for */
    } /* for */

    /* print this module's number on the ELSC */
    us_delay(40000);
    elsc_print_mod_num(1);

    /* RFE PV # 445985 */
    /* If the skip autoboot virtual dip switch is set
       and we were about to do a startup() dont do it.
       Take a shortcut and go to EnterInteractiveMode
       right here.
    */
    if (read_skipauto() && 
	(restart != PROMOP_RESTART) &&
	(restart != PROMOP_REBOOT))
	restart = PROMOP_RESTART ;

    /* check if this is a reboot controlled by another partition. */

    check_part_reboot() ;

    switch (restart) {
	case PROMOP_RESTART:
	    EnterInteractiveMode();
	    break;

	case PROMOP_REBOOT:
	    reboot();
	    break;

	default: 
	    startup();
	    break;
    }
}

/*
 * Function:            slave_set_moduleid -> executed on slave CPU to set
 *                      moduleid
 * Args:                module -> module to set to
 * Returns:             Nothing
 */

void slave_set_moduleid(int module)
{
    io6prom_elsc_init() ;
    elsc_module_set(get_elsc(), module);
}

static void 
make_mod_msg(char *mmsg, moduleid_t mnum, int partition, int master)
{

#ifdef OLD_CODE
    int 	m10, m100 ;
    strcpy(mmsg, "Mod ") ;
    m10     = (mnum/10) ;       
    m100    = (mnum/100) ;     

    mmsg[4] = (m100) ? (m100 + '0') : ' ' ;
    mmsg[5] = (m10%10) ? (m10%10) + '0' : ' ' ;
    mmsg[6] = (mnum%10) + '0' ;
    mmsg[7] = master ? 'C' : ' ' ;
    mmsg[8] = 0 ;
#endif

    /*
     * Right now we only check partition id's for validity 
     */
     
    if(partition >=0 && partition <= 63){
      if(master)
	sprintf(mmsg, "P%2d M%2dC", partition, mnum);
      else
	sprintf(mmsg, "P%2d M%2d", partition, mnum);
    } 
    else {
      if(master)
	sprintf(mmsg, "    M%2dC", mnum);
      else
	sprintf(mmsg, "    M%2d ", mnum);
    }
}

/* 
 * elsc_print_mod_num_
 *       sets up the ELSC and then prints the module number on its LEDs
 */
void
elsc_print_mod_num(int master) {
    lboard_t *board;
    char mod_msg[48];

    if (SN00)
	return;

    io6prom_elsc_init() ;
    
    board = find_lboard((lboard_t*)KL_CONFIG_INFO(get_nasid()),
                        KLTYPE_IP27);

    /* convert to a 3 digit number. */
    make_mod_msg(mod_msg, board->brd_module, 		 
		 elsc_partition_get(get_elsc()), master);

    elsc_display_mesg(get_elsc(), mod_msg);
    /* we can't use printf so no error message if it fails :( */
}

/*
 * get all slave nvram bases into nslvnv_base
 */

int
get_slvnv_base(lboard_t *l, void *ap, void *rp)
{
	__psunsigned_t  *slvp = (__psunsigned_t *)rp ;
	int		*nslv_p = (int *)ap ;

	if (l->brd_type != KLTYPE_BASEIO)
		return 1 ;
	if (l->brd_flags & GLOBAL_MASTER_IO6)
		return 1 ;

	if (slvp[*nslv_p] = getnv_base_lb(l)) {
		(*nslv_p) += 1 ;
	}

	return 1 ;
}

/*
 * init_prom_soft()
 *	Calls the initialization routines for the major
 *	PROM subsystems.
 */

void
init_prom_soft(int no_init_env)
{

#if DEBUG
    /* Check for the value of stack on re-entry */
    int dummy ;

    printf("Stack = 0x%lx, SP = 0x%lx\n", Stack, &dummy) ;
#endif

    master_nasid 	= get_nasid();
    if (!no_init_env)
        update_master_baseio() ;
    nvaddr_init() ;
    io6prom_elsc_init() ;

    /* Init diag malloc area every time we re-enter the IO6prom */

    diag_malloc_brk = DIAG_BASE ;

    init_klmalloc_device(master_nasid) ;
#if DEBUG
    dump_klmalloc_table(master_nasid) ;
#endif

    sysctlr_message("InitCach");
    config_cache();
    flush_cache();

    set_SR(IO6PROM_SR & ~(SR_IE));	/* Set SR to its "normal" state */

    check_edtab();
    nvram_disabled_io_info_update();

    if (!no_init_env)
	init_env();

    sysctlr_message("InitSaio");
    _init_saio();

/* Just for testing since this runs with the 'init' command and
   can be run any number of times without reboot. 
*/

    read_extra_nic() ;
    /*do_dis_node_brds() ;*/

    check_inventory() ;
    elsc_print_mod_num(1);

#ifdef DEBUG
    dump_inventory() ;
#endif

#if 0
    propagate_serial_num() ;
#endif
    init_tlb();
    alloc_memdesc();
}


/*
 * show_halt_msg()
 *	Display a nice friendly halt message for the user
 */

static void
show_halt_msg(void)
{
    if (doGui()) setGuiMode(1,GUI_NOLOGO);
	popupDialog("\n\nOkay to power off the system now.\n\n"
		    "Press RESET button to restart.",
		    0,DIALOGINFO,DIALOGCENTER|DIALOGBIGFONT);
}


/*
 * halt()
 *	Bring the system to a quiescent state
 */

void
halt()
{
    show_halt_msg();
}


/*
 * powerdown()
 *	Attempt a soft power down of the system.
 */

void
powerdown(void)
{
    cpu_soft_powerdown();
    show_halt_msg();
}


/*
 * restart()
 *	Attempt to restart the machine.
 */

void
restart(void)
{
    static int restart_buttons[] = {DIALOGRESTART,-1};
    if (doGui()) setGuiMode(1,GUI_NOLOGO);
    popupDialog("\n\nYou may shutdown the system.\n\nPress any key to restart.",
		 restart_buttons, DIALOGINFO, DIALOGCENTER|DIALOGBIGFONT);
    if (doGui()) setGuiMode(0, GUI_RESET);
    startup();
}


/*
 * reboot()
 *	This is supposed to try to reproduce the last system boot
 *	command.
 */

void
reboot(void)
{
    int Verbose = 0;	/* ??? */

    if (!Verbose && doGui()) {
	setGuiMode(1,0);
    }
    else {
	p_panelmode();
	p_cursoff();
    }
    if (ESUCCESS != autoboot(0, 0, 0)) {
        putchar('\n');
        setTpButtonAction(EnterInteractiveMode,TPBCONTINUE,WBNOW);
        p_curson();
        p_printf("Unable to boot; press any key to continue: ");
        getchar();
        putchar('\n');
    }

    /* If reboot fails, reinitialize software in case a partially
     * booted program has messed up the prom state.
     */
    EnterInteractiveMode();             /* should not get here */
}

/*
 * Update the IOC3 flags for the master console.
 * These are needed so that drivers can be installed
 * further down in the IO prom.
 */

nasid_t
update_master_baseio(void)
{
        cnodeid_t       cnode ;
        nasid_t         nasid ;
        lboard_t        *lb;
        gda_t           *gdap;
	klioc3_t	*ioc3p ;

        gdap = (gda_t *)GDA_ADDR(get_nasid());
        if (gdap->g_magic != GDA_MAGIC) {
                printf("update_master_io6: Invalid GDA MAGIC\n") ;
		return -1 ;
        }

	/* for all nodes */
        for (cnode = 0; cnode < MAX_COMPACT_NODES; cnode ++) {
                nasid = gdap->g_nasidtable[cnode];
                if (nasid == INVALID_NASID)
                        continue;

		lb = (lboard_t *)KL_CONFIG_INFO(nasid) ;
		/* find all baseio boards */
		while (lb) {
                	lb = find_lboard(lb, KLTYPE_BASEIO);
			if (!lb)
				break ;

			/* the first local master is the global master */
			if (lb->brd_flags & LOCAL_MASTER_IO6) {
				lb->brd_flags |= GLOBAL_MASTER_IO6 ;

				master_baseio_modid = lb->brd_module ;
				master_baseio_slotid = lb->brd_slot ;
				ioc3p = NULL ;
				ioc3p = (klioc3_t *)find_component(lb,
					(klinfo_t *)ioc3p, KLSTRUCT_IOC3) ;

				/* Install all drivers for this IOC3 */
				ioc3p->ioc3_info.flags    |= KLINFO_INSTALL ;
				ioc3p->ioc3_superio.flags |= KLINFO_INSTALL ;
				ioc3p->ioc3_enet.flags    |= KLINFO_INSTALL ;

				return lb->brd_nasid ;
			}

			lb = KLCF_NEXT(lb);
		}
        }

	/* did not find any local master in the whole system */
        return -1 ;
}

/*
 * enter_imode()
 *	Drop back into Interactive Mode.
 */

void
enter_imode(void)
{
    _hook_exceptions();
    _init_saio();               /*  initialize saio library */
    alloc_memdesc();            /*  allocate memory descriptors for prom */
    set_SR(IO6PROM_SR & ~(SR_IE));
    main();
    EnterInteractiveMode();     /* shouldn't get here */
}

/*
 * alloc_memdesc()
 *	Allocate memory descriptors.
 */

static void
alloc_memdesc(void)
{

}

/*
 * Set up some data structs for compatiblity with old
 * proms
 */

update_old_master_baseio(nasid_t nasid)
{
        lboard_t *lboard;
        lboard = (lboard_t *)KL_CONFIG_INFO(nasid);
        if ((lboard = find_lboard(lboard, KLTYPE_BASEIO)) == NULL) {
		printf("CPU and IO proms highly incompatible.\
Update CPU and IO proms to the same rev level.\n") ;
		return 0 ;
	}
	lboard->brd_flags |= (LOCAL_MASTER_IO6|GLOBAL_MASTER_IO6) ;
	return 1 ;
}

void dump_diag_results(int diag_mode)
{
   sn0_dump_diag(diag_mode);
#if IP27_CPU_EARLY_INIT_WAR
   check_early_init();
#endif
}

/*
 * Scan all io boards and collect their base addrs.
 */

void
nvaddr_init()
{
    int i ;

    extern __psunsigned_t  slvnv_base[] ;
    extern int		   nslv_nvram ;
    extern __uint64_t      mstnv_state ;

    nslv_nvram = mstnv_state = 0 ;
    for (i=0;i<MAX_NVRAM_IN_SYNC;i++)
	slvnv_base[i] = 0 ;

    nvram_base 		= kl_ioc3nvram_base(get_nasid());
    ioc3uart_base 	= kl_ioc3uart_base(get_nasid());

    visit_lboard(get_slvnv_base, (void *)&nslv_nvram, (void *)slvnv_base) ;
#ifdef DEBUG
    printf("nslv_nvram = %d", nslv_nvram) ;
    for(i=0;i<nslv_nvram;i++) printf(" 0x%lx,", slvnv_base[i]) ;
    printf("\n") ;
#endif
}

/*
 * Init elsc for putting msgs on sys ctlr
 */

__psunsigned_t
io6prom_elsc_init()
{
    __psunsigned_t base ;
    extern void set_elsc(elsc_t *);

   /* Use the very bottom of the stack since no one is using
      it right now. 
    */
    base = SYMMON_STK_ADDR(get_nasid(),
                     REMOTE_HUB_L(get_nasid(), PI_CPU_NUM));

    set_elsc((elsc_t *) base);

    elsc_init(get_elsc(), get_nasid());
    elscuart_init(0);

    return base ;
}

/*
 * read the skip auto boot virtual dip switch from msc
 */

int
read_skipauto()
{
    u_char      dbg_p, dbg_v ;

    if ((elsc_debug_get(get_elsc(), &dbg_v, &dbg_p)) < 0)
        dbg_v = dbg_p = 0;

    return (dbg_v & (1 << ((DIP_NUM_IGNORE_AUTOBOOT) - 1 - 8))) ;
}

#if IP27_CPU_EARLY_INIT_WAR

lboard_t        *pwr_cyc[MAX_MODULES];
int             pwr_cyc_indx;

static void
add_pwr_cyc(lboard_t *lb)
{
        int     i;

        for (i = 0; i < pwr_cyc_indx; i++)
                if (pwr_cyc[i]->brd_module == lb->brd_module)
                        return;

        pwr_cyc[pwr_cyc_indx] = lb;
        pwr_cyc_indx++;
}

int
search_early_init(lboard_t *lb, void *arg, void *result)
{
        klcpu_t         *cpuinfo;

        if (lb->brd_type != KLTYPE_IP27)
                return 1;

        cpuinfo = (klcpu_t *) find_component(lb, NULL, KLSTRUCT_CPU);
        if (cpuinfo && !(cpuinfo->cpu_info.flags & KLINFO_ENABLE) &&
                (cpuinfo->cpu_info.diagval == KLDIAG_EARLYINIT_FAILED)) {
                add_pwr_cyc(lb);
                return 1;
        }

        cpuinfo = (klcpu_t *) find_component(lb, (klinfo_t *) cpuinfo,
                KLSTRUCT_CPU);
        if (cpuinfo && !(cpuinfo->cpu_info.flags & KLINFO_ENABLE) &&
                (cpuinfo->cpu_info.diagval == KLDIAG_EARLYINIT_FAILED))
                add_pwr_cyc(lb);

        return 1;
}

void ei_pwr_cycle_module(void)
{
        io6prom_elsc_init();
	us_delay(200000);
        elsc_power_cycle(get_elsc());
}

static int
ei_pwr_cycle(lboard_t *lb)
{
        lboard_t        *mod_brds[NODESLOTS_PER_MODULE];
        int             i;

        if (module_brds(lb->brd_nasid, mod_brds, NODESLOTS_PER_MODULE) < 0)
                return -1;

        for (i = 0; i < NODESLOTS_PER_MODULE; i++)
                if (mod_brds[i]) {
                        klcpu_t         *cpuinfo;

                        cpuinfo = (klcpu_t *) find_component(mod_brds[i],
                                NULL, KLSTRUCT_CPU);
                        if (cpuinfo && (cpuinfo->cpu_info.flags &
                                        KLINFO_ENABLE)) {
				LAUNCH_SLAVE(mod_brds[i]->brd_nasid,
                                cpuinfo->cpu_info.physid,
                                (launch_proc_t) PHYS_TO_K0(kvtophys((void *)
                                ei_pwr_cycle_launch)), get_nasid(),
                                (void *) TO_NODE(mod_brds[i]->brd_nasid,
                                IP27PROM_STACK_A + 1024), 0);
                                return 0;
                        }

                        cpuinfo = (klcpu_t *) find_component(mod_brds[i],
                                (klinfo_t *) cpuinfo, KLSTRUCT_CPU);
                        if (cpuinfo && (cpuinfo->cpu_info.flags &
                                        KLINFO_ENABLE)) {
				LAUNCH_SLAVE(mod_brds[i]->brd_nasid,
                                cpuinfo->cpu_info.physid,
                                (launch_proc_t) PHYS_TO_K0(kvtophys((void *)
                                ei_pwr_cycle_launch)), get_nasid(),
                                (void *) TO_NODE(mod_brds[i]->brd_nasid,
                                IP27PROM_STACK_A + 1024), 0);
                                return 0;
                        }
                }

        return -1;
}

#define EARLY_INIT_MAX_CNT      3

void
check_early_init(void)
{
        int             i, self = 0;
        moduleid_t      module;
        char            ea_cnt;

	if (SN00) {
		char	buf[4];

		ip27log_getenv(get_nasid(), IP27LOG_MODULE_KEY, buf, "1", 0);
		module = atoi(buf);
	}
	else
		module = elsc_module_get(get_elsc());

        visit_lboard(search_early_init, NULL, NULL);

        if (!pwr_cyc_indx) {
                set_nvreg(NVOFF_EA_CNT, 0);
		update_checksum();
                return;
        }
        else if ((ea_cnt = get_nvreg(NVOFF_EA_CNT)) >= EARLY_INIT_MAX_CNT) {
                printf("\nWARNING: Some CPUs failed during early init. Failed"                        " to start up CPUs\n");
                return;
        }

        set_nvreg(NVOFF_EA_CNT, ea_cnt + 1);
	update_checksum();

        printf("\nWARNING: Some CPUs failed during early init. Power cycling "
		"affected modules (Try %d of %d)\n", ea_cnt + 1, 
		EARLY_INIT_MAX_CNT);

        for (i = 0; i < pwr_cyc_indx; i++)
                if (pwr_cyc[i]->brd_module == module)
                        self = 1;
                else if (ei_pwr_cycle(pwr_cyc[i]) < 0)
                        printf("WARNING: /hw/module/%d/slot/n%d/cpu: Failed "
                                "early init. Please power cycle manually\n",
                                pwr_cyc[i]->brd_module,
                                SLOTNUM_GETSLOT(pwr_cyc[i]->brd_slot));

        if (self) {
		us_delay(200000);
                elsc_power_cycle(get_elsc());
	}
}
#endif

#define SAVE_CLEAR_FENCE	1
#define RESTORE_FENCE		2

#define PART_REBOOT_WAIT	180

struct save_fence_s {
	__uint64_t	vector ;
	sn0drv_fence_t 	fence ;
} *sf ;
static int	sfcnt ;

void
do_rou_reg(klrou_t *klr, struct save_fence_s *sfp, int flag)
{
	__uint64_t value ;
	int	port ;
	int	err ;

	sfp->vector = klr->rou_vector ;
	if (flag == SAVE_CLEAR_FENCE) {
        	err = vector_read_node(sfp->vector, get_nasid(), 0, 
				RR_PROT_CONF, &value);
		if (err<0)
			return ;

		sfp->fence.local_blk.reg = value ;

        	err = vector_write_node(sfp->vector, get_nasid(), 0, 
				RR_PROT_CONF, value|0x3f);
        	if (err != NET_ERROR_NONE)
			return ;
	}

	ForEachRouterPort(port) {
		if (klr->rou_port[port].port_nasid == INVALID_NASID)
			continue ;
		if (flag == SAVE_CLEAR_FENCE) {
			err = vector_read_node(sfp->vector, get_nasid(), 0, 
				RR_RESET_MASK(port), &value);
			if (err<0)
				continue ;
			sfp->fence.ports[port-1].reg = value ;
        		err = vector_write_node(sfp->vector, get_nasid(), 0, 
					RR_RESET_MASK(port), value|0x3f);
        		if (err != NET_ERROR_NONE)
				return ;
		}
	}
}

int
vlb_do_router_fences(lboard_t *lb, void *ap, void *rp)
{
	klrou_t 	*klr ;
	lboard_t	*myip27 ;

	/* Do nothing with META ROUTERS XXX */

	if (KLCLASS(lb->brd_type) == KLCLASS_ROUTER) {

#ifdef LONG_POLE_128P

	/* This is one of the alternatives to read and store
	 * the status of the meta router port fences. It works
	 * OK, but under certain h/w configs the system freezes
	 * if we type unix at the io6prom prompt. To be investigated.
	 */
	if (lb->brd_type == KLTYPE_META_ROUTER) {
		printf("M.R = %llx\n", lb->brd_nic) ;
		if (myip27 = find_lboard((lboard_t *) 
				KL_CONFIG_INFO(get_nasid()), KLTYPE_IP27)) {
			net_vec_t vec ;
			vec = klcfg_discover_route(myip27, lb, 0) ;
			if (vec != NET_VEC_BAD)
				printf("M.R. vec = %llx\n", vec) ;
			else
				printf("M.R. bad vec\n") ;
		}

	}
#endif /* LONG_POLE_128P */

		if ((klr = (klrou_t *)find_first_component(lb, KLSTRUCT_ROU))){
			do_rou_reg(klr, &sf[sfcnt++], *(int *)ap) ;
		}
	}

	return 1 ;
}

void
do_router_fences(int flag)
{
	int	i, err, port ;
	int	num_rou = num_nodes/2 + num_nodes ;
		/* normal routers + meta routers */

	if (flag == SAVE_CLEAR_FENCE) {
		sf = (struct save_fence_s *) malloc(
			sizeof(struct save_fence_s) * num_rou) ;
		if (sf == NULL) 
			return ;
		visit_lboard(vlb_do_router_fences, (void *)&flag, NULL) ;
		return ;
	}

#ifdef DEBUG
	printf("No of vectors restored = %d\n", sfcnt) ;
#endif

	if (flag == RESTORE_FENCE) {
		for (i=0; i<sfcnt; i++) {
			err = vector_write_node(sf[i].vector	, 
						get_nasid(), 0	,
						RR_PROT_CONF	, 
						sf[i].fence.local_blk.reg) ;
        		if (err != NET_ERROR_NONE)
				return ;
			ForEachRouterPort(port) {
				err = vector_write_node(sf[i].vector	, 
							get_nasid(), 0	,
							RR_RESET_MASK(port), 
						sf[i].fence.ports[port-1].reg) ;
        			if (err != NET_ERROR_NONE)
					return ;
			}
		}
	}
}

jmp_buf part_reboot_buf ;

static void
part_reboot_intr()
{
    printf("\n") ;
    Signal (SIGALRM, SIGIgnore);
    longjmp (part_reboot_buf, 1);
}

void
check_part_reboot(void)
{
	char 		part_reboot ;
	int		cnt=PART_REBOOT_WAIT ;
	kldir_ent_t	*kld ;
	klp_t		*klp ;
	unsigned char 	*cp ;
	char		buf[8] ;
	ULONG		ttycnt ;
	char		c ;

	/* Lets change the state of the KLDIR entry to prom now. */

	kld = KLD_KERN_PARTID(get_nasid()) ;
	klp = (klp_t *)kld->pointer ;
	klp->klp_state = KLP_STATE_PROM ;

	/* Do we have to do it */

	part_reboot = get_nvreg(NVOFF_RESTART) ;

#ifdef LONG_POLE_128P
/*
 * During debug, we may want to optionally do this stuff. */
 */
	printf("Part reboot stuff? ") ;
	if ((c = getchar()) == 'y')
		part_reboot = 0xe ;
#endif
	if (part_reboot == 0xe) {

		/* We have seen this once. Clear it. */

		set_nvreg(NVOFF_RESTART, 0) ;
		update_checksum();

		/* Clear all our fences so that another part may reset us. */

		do_router_fences(SAVE_CLEAR_FENCE) ;

		/* Wait for secs */

		printf("Waiting %d secs for reset from remote partition.\n",
			cnt) ;
		printf("If this seems to be wrong, Please hit ctrl-C to "
			"go back to ioprom prompt.\n") ;

		Signal (SIGINT, part_reboot_intr);

		while (cnt--) {
			printf(". ") ;
#if 0
			/* Failed attempt at doing async io in prom. */

			if (gets(buf) && (*buf == CTRL('c')))
				break ;
			ttycnt = 0 ;
			Read(0, buf, 1, &ttycnt) ;
			if (ttycnt == 1) {
				printf("%x\n", *buf) ;
				if (*buf == CTRL('c'))
					break ;
			}
#endif
			if (setjmp(part_reboot_buf))
				break ;

			us_delay(1000000L) ;
		}

		printf("\n") ;

		/* If we reach here, no one reset us. Put back the
		   fences and go to prom menu. XXX */

		printf("Timed out waiting for remote partition reboot.\n") ;
		printf("If this shutdown was due to mkpart command you will\n"
			"need to reset the system manually when the other\n"
			"partitions are reset.\n") ;

		do_router_fences(RESTORE_FENCE) ;
	}
}

