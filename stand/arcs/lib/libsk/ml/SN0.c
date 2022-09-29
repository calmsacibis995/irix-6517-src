/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1992-1996, Silicon Graphics, Inc.          *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/***********************************************************************\
*	File:		SN0.c						*
*									*
* 	This file contains the the definitions of the cpu-dependent	*
*	functions for SN0-based systems.  For the most part,		*
*	this file should work for IP27 based machines.			*
*									*
\***********************************************************************/

#define PROM	/* XXX - We can change this when the address defines for
		 * the PROM move out of sn0addrs.h
		 */

#include <sys/i8251uart.h>
#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/immu.h>
#include <sys/time.h>
#include <sys/ktime.h>
#include <sys/cpu.h>
#include <sys/loaddrs.h>
#include <sys/iotlb.h>
#include <sys/i8254clock.h>
#include <sys/z8530.h>
#include <sys/ns16550.h>
#include <arcs/io.h>
#include <sys/nic.h>
#include <standcfg.h>
#include <stringlist.h>
#include <libsc.h>
#include <libsk.h>
#include <libkl.h>
#include <menu.h>
#include <stdarg.h>
#include <io6fprom.h>
#include <arcs/folder.h>
#include <arcs/errno.h>
#include <arcs/hinv.h>
#include <arcs/cfgtree.h>
#include <arcs/types.h>
#include <arcs/time.h>
#include <sys/SN/addrs.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/kldiag.h>
#include <sys/SN/promcfg.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/gda.h>
#include <sys/SN/SN0/klhwinit.h>
#include <sys/SN/nvram.h>
#include <sys/PCI/bridge.h>
#include <promgraph.h>
#include <pgdrv.h>
#include <sys/iograph.h>
#include <klclock.h>
#include <sys/PCI/PCI_defs.h>
#include <gfxgui.h>

#include <libsc_internal.h>
#include <sn0hinv.h>

#define	DECLARE_HWGHDL	graph_hdl_t	ghdl ; \
			vertex_hdl_t	rvhdl ;

#ifdef SN_PDI
extern graph_hdl_t      pg_hdl ;
extern vertex_hdl_t     snpdi_rvhdl ;
#define CHOOSE_GHDL	(ghdl = pg_hdl) ;
#define CHOOSE_RVHDL	(rvhdl = snpdi_rvhdl) ;
#else
extern graph_hdl_t      prom_graph_hdl ;
extern vertex_hdl_t     hw_vertex_hdl ;
#define CHOOSE_GHDL	ghdl = prom_graph_hdl ;
#define CHOOSE_RVHDL	rvhdl = hw_vertex_hdl ;
#endif

extern int	month_days[12];		/* in libsc.a */
extern int	sk_sable;
extern int 	_symmon;

moduleid_t	master_baseio_modid ;
slotid_t  	master_baseio_slotid ;
char		master_console_path[64] ;

extern int cpu_set_nvram_offset(int, int, char *);

char *copy_nic_field(char *, char *) ;
void sn0_dev2eng(COMPONENT *, char *) ;
char *make_serial(int) ;

/*nasid_t get_dis_node_brd_nasid(moduleid_t, slotid_t) ;*/

/*
 * cpu_makecfgroot()
 *	Creates the Root node of the SN0 ARCS configuration tree.
 */

static int
log2(int x)
{
    int n, v;
    for (n = 0, v = 1; v < x; v <<= 1, n++) ;
    return n;
}

#define SYSBOARDIDLEN   9 

static COMPONENT root_template = {
    SystemClass,	
    ARC,
    (IDENTIFIERFLAG)0,
    SGI_ARCS_VERS,
    SGI_ARCS_REV,
    0,
    0,
    0,
    SYSBOARDIDLEN,
    0
};

cfgnode_t *
cpu_makecfgroot(void)
{
    COMPONENT *root = &root_template;
 
    printf("cpu_makecfgroot called. \n") ;

    root->Identifier = "SGI-IP27";
    root = AddChild(NULL, &root_template, (void*)NULL);
    if (root == (COMPONENT*)NULL)
	cpu_acpanic("root");

    return ((cfgnode_t*)root);
}


/*
 * cpu_install()
 *	Adds nodes in the ARCS configuration tree for
 *	all of the cpus and their subcomponents (FPUS and
 *	caches).  This routine deals with IP27 based
 *	boards (and hopefully their descendents, if
 *	any ever show up).
 */ 

static COMPONENT cputmpl = {
    ProcessorClass,             /* Class */
    CPU,                        /* Type */
    (IDENTIFIERFLAG)0,          /* Flags */
    SGI_ARCS_VERS,              /* Version */
    SGI_ARCS_REV,               /* Revision */
    0,                          /* Key */
    0x01,                       /* Affinity */
    0,                         	/* ConfigurationDataSize */
    0,         			/* IdentifierLength */
    NULL	                /* Identifier */
};
static COMPONENT fputmpl = {
    ProcessorClass,             /* Class */
    FPU,                        /* Type */
    (IDENTIFIERFLAG)0,          /* Flags */
    SGI_ARCS_VERS,              /* Version */
    SGI_ARCS_REV,               /* Revision */
    0,                          /* Key */
    0x01,                       /* Affinity */
    0,                          /* ConfigurationDataSize */
    0,                          /* IdentifierLength */
    NULL                 	/* Identifier */
};
static COMPONENT cachetmpl = {
    CacheClass,                 /* Class */
    PrimaryICache,              /* Type */
    (IDENTIFIERFLAG)0,          /* Flags */
    SGI_ARCS_VERS,              /* Version */
    SGI_ARCS_REV,               /* Revision */
    0,                          /* Key */
    0x01,                       /* Affinity */
    0,                          /* ConfigurationDataSize */
    0,                          /* IdentifierLength */
    NULL                        /* Identifier */
};

static COMPONENT memtmpl = {
    MemoryClass,                /* Class */
    Memory,                     /* Type */
    (IDENTIFIERFLAG)0,          /* Flags */
    SGI_ARCS_VERS,              /* Version */
    SGI_ARCS_REV,               /* Revision */
    0,                          /* Key */
    0x01,                       /* Affinity */
    0,                          /* ConfigurationDataSize */
    0,                          /* Identifier Length */
    NULL                        /* Identifier */
};

void
cpu_install(COMPONENT *root)
{
	panic("cpu_install should not be called\n") ;
}

/*
 * cpu_reset
 *	Perform any board-specific start up initialization.
 */

void
cpu_reset(void)
{
	initsplocks();		/* initialize spin lock subsystem */
}


/*
 * cpu_hardreset
 *	Performs a hard reset by slamming the Kamikaze reset
 *	vector.
 */

void
cpu_hardreset(void)
{
    printf("Resetting the system...\n");
    *LOCAL_HUB(NI_PORT_RESET) = NPR_PORTRESET | NPR_LOCALRESET;

    for (;;) /* Loop Forever */ ;
    /* NOTREACHED */
}


/*
 * cpu_show_fault()
 */

/*ARGSUSED*/
void
cpu_show_fault(unsigned long saved_cause)
{
    printf("fixme: cpu_show_fault\n") ;
}


/*
 * cpu_soft_powerdown()
 */

void
cpu_soft_powerdown(void)
{
	printf("fixme: cpu_soft_powerdown\n") ;
}

int
cpu_get_max_nodes(void)
{
    int         cnode ;
    gda_t       *gdap ;

    gdap = GDA ;
    if (gdap) {
        for (cnode = 0; cnode < MAX_COMPACT_NODES; cnode ++) {
            if (gdap->g_nasidtable[cnode] == INVALID_NASID)
                return cnode ;
        }
    }
    /* NOTREACHED */
}

/*
 * Visits every board struct in klconfig.
 * Calls the function pointer fp with the argument pointer ap for
 * respective lboard struct and a result pointer rp.
 * fp(lboard_t *, void *, void *)
 */

int
visit_lboard(int (*fp)(), void *ap, void *rp)
{
        int             rv = 0 ;
        cnodeid_t       cnode ;
        nasid_t         nasid ;
        lboard_t        *lbptr ;
        gda_t           *gdap;

        gdap = (gda_t *)GDA_ADDR(get_nasid());
        if (gdap->g_magic != GDA_MAGIC) {
                printf("visit_lboard: Invalid GDA MAGIC\n") ;
                return 0 ;
        }

        for (cnode = 0; cnode < MAX_COMPACT_NODES; cnode ++) {
                nasid = gdap->g_nasidtable[cnode];
                if (nasid == INVALID_NASID)
                        continue;

                lbptr = (lboard_t *)KL_CONFIG_INFO(nasid) ;

                while (lbptr) {
                        if (!(lbptr->brd_flags & DUPLICATE_BOARD))
                                if (!((*fp)(lbptr, ap, rp)))
                                        return 1 ;
                        lbptr = KLCF_NEXT(lbptr);
                }
        }

        return 1 ;
}

int
mem_size(lboard_t *l, void *ap, void *rp)
{
        klmembnk_t        *mem_ptr ;

        if (l->brd_type == KLTYPE_IP27) {
                mem_ptr = NULL ;
                mem_ptr = (klmembnk_t *)find_component(l,
                (klinfo_t *)mem_ptr, KLSTRUCT_MEMBNK) ;
                /* 4k pages */
                *(int *)rp = *(int *)rp + (mem_ptr->membnk_memsz * 256) ;
        }
        return 1 ;
}

/*
 * cpu_get_memsize()
 */

unsigned int
cpu_get_memsize(void)
{
        int     total_mem = 0 ;
        visit_lboard(mem_size, NULL, &total_mem) ;
        return total_mem ;
}

/* Memory disable routines */

unsigned int
dismem_size(nasid_t nasid, char *dmbnk)
{
    int             disable, mem_dis = 0, bank ;
    char            mem_disable[MD_MEM_BANKS + 1];
    char            disable_sz[MD_MEM_BANKS + 1];

    disable = (ip27log_getenv(nasid, "DisableMemMask",
                        mem_disable, 0, 0) >= 0);

    if (disable) {
        if (ip27log_getenv(nasid, "DisableMemSz", disable_sz,
            0, 0) >= 0)
            for (bank = 0; bank < MD_MEM_BANKS; bank++)
                if (strchr(mem_disable, '0' + bank))
                    mem_dis += MD_SIZE_MBYTES(
                        disable_sz[bank] - '0');
        if (dmbnk) {
    	    *dmbnk =  0 ;
	    strcpy(dmbnk, disable_sz) ;
	}
    }
    return mem_dis ;

}

dismem_size_l(lboard_t *l, void *ap, void *rp)
{
    if (l->brd_type == KLTYPE_IP27)
    	*(int *)rp = *(int *)rp + dismem_size(l->brd_nasid, NULL) ;
    return 1 ;
}

dismem_size_k(klinfo_t *k, char *dmbnk)
{
        return dismem_size(k->nasid, dmbnk) ;
}

cpu_get_dismem(void)
{
        int     dismem = 0 ;
        visit_lboard(dismem_size_l, NULL, &dismem) ;
        return dismem ;
}

/* these decode routines need to be kept in sync with the routines in 
irix/kern/ml/SN0/module_info.c */
#define SN00_SERIAL_FUDGE 	0x3b1af409d513c2
#define SN0_SERIAL_FUDGE	0x6e
void
decode_int_serial(unsigned long long src, unsigned long long *dest) {
	unsigned long long val;
	int i;

	for (i = 0; i < sizeof(long long); i++) {
		((char*)&val)[sizeof(long long)/2 + 
			     ((i%2) ? ((i/2 * -1) - 1) : (i/2))] = 
		  ((char*)&src)[i];
	}

	*dest = val - SN00_SERIAL_FUDGE;
}


void
decode_str_serial(const char *src, char *dest) {
	int i;

	for (i = 0; i < MAX_SERIAL_NUM_SIZE; i++) {
		dest[MAX_SERIAL_NUM_SIZE/2 + 
		    ((i%2) ? ((i/2 * -1) - 1) : (i/2))] = src[i] - 
			SN0_SERIAL_FUDGE;
	}
}

int
prmod_snum(lboard_t *l, void *ap, void *rp)
{
    int i, cnt ;
    klmod_serial_num_t      *snump ;

    if (l->brd_type == KLTYPE_MIDPLANE) {

	/* Print serial numbers only for xbow 0 */
	if (!SN00) {
	    if (CONFIG_12P4I) {
		/* Print the serial number only once .
		 * Without this check in the worst case
		 * we might see 5 duplicate error messages
		 */
		if (l->brd_nasid)
		    return 1;
	    }
  	    if (get_node_crossbow(l->brd_nasid) & 1)
	        return 1 ;
	}
        snump = GET_SNUM_COMP(l) ;

        if (snump->snum_info.struct_type == KLSTRUCT_MOD_SERIAL_NUM) {
		char snum_str[MAX_SERIAL_NUM_SIZE + 1];
		unsigned long long snum_val = 0;

		if (SN00)
			decode_int_serial(snump->snum.snum_int,
					  &snum_val);
		else {
			decode_str_serial(snump->snum.snum_str,
					  snum_str);

			/* make sure it is null terminated */
			snum_str[MAX_SERIAL_NUM_SIZE] = '\0';
		}
		if (snum_val) {
			/* if we have a valid SN00 serial number,
			we are only printing out the lower 4 bytes to
			be consistent with the kernel. The uppper 2 bytes
			of the MAC address will always be 0x0800 */
			printf("Module %d: Serial: %x\n", 
			       l->brd_module,snum_val & 0xffffffffll);
		}
		else if (snum_str[0] == 'K') {
			/* if we have a valid SN0 serial number */
			printf("Module %d: Serial: %s\n", 
			       l->brd_module,snum_str);

		}
		else {
			printf("Invalid serial number for module %d\n",
			       l->brd_module);
		}
	}
    }
    return 1 ;
}

char *
get_nicstr_from_devinfo(lboard_t *lb)
{
        klbri_t        *brip ;
	char		*nicstr = NULL ;
	brip = (klbri_t *)find_component(lb, NULL, KLSTRUCT_BRI) ;
	if (brip)
            	nicstr = (char *)NODE_OFFSET_TO_K1(lb->brd_nasid, 
					brip->bri_mfg_nic) ;

	return nicstr ;
}

/*
 * find the frequency (in HZ) of the running CPU
 */

int
get_freq(lboard_t *l, void *ap, void *rp)
{
        klcpu_t        *cpu_ptr ;

        if (l->brd_type != KLTYPE_IP27)
                return 1 ;

        cpu_ptr = NULL ;
        do {
            cpu_ptr = (klcpu_t *)find_component(l, (klinfo_t *)cpu_ptr,
                                        KLSTRUCT_CPU) ;

            if ((cpu_ptr) &&
                (KLCONFIG_INFO_ENABLED(&cpu_ptr->cpu_info)) &&
                (cpu_ptr->cpu_info.virtid == *(int *)ap)) {
                    *(int *)rp = (cpu_ptr->cpu_speed * 1000000) ;
                        return 0 ; /* stop the loop */
                }
        } while (cpu_ptr) ;

        return 1 ;

}

unsigned int
cpu_get_freq(int acpuid)
{
        int freq = 0 ;
        visit_lboard(get_freq, &acpuid, &freq) ;
        if (!freq) /* value not changed */
                printf("*** Warning: Cannot find frequency of CPU %d\n", acpuid);
        return freq ;
}

int
fill_cpu_list(lboard_t *l, void *ap, void *rp)
{
        klinfo_t        *kliptr ;
        cpu_list_t      *cur_rv = (cpu_list_t *)rp ;
        int             cpu_freq, i, dis_cpus;

        if (l->brd_type == KLTYPE_IP27) {
                kliptr = NULL ;
                while(kliptr = find_component(l, kliptr, KLSTRUCT_CPU)) {
                        cpu_freq = ((klcpu_t *)kliptr)->cpu_speed;
                        if(cpu_freq == 0xffff)
                        {
                                cpu_freq = (short)((((ip27config_t *)(IP27CONFIG_ADDR_NODE(l->brd_nasid)))->freq_cpu)/1000000);
                        }
                        for(i = 0; i < cur_rv->size; i++)
                        {
                                if(cur_rv->freq[i] == cpu_freq)
                                {
                                        cur_rv->count[i] ++;
                                        break;
                                }
                        }
                        if(i >= 16)
                                return 1;
                        if(i == cur_rv->size)
                        {
                                cur_rv->size ++;
                                cur_rv->freq[i] = cpu_freq;
                                cur_rv->count[i] = 1;
                        }
                        if (!(KLCONFIG_INFO_ENABLED(kliptr)))
                        {
                                dis_cpus = (cur_rv->count[i] >> 16);
                                dis_cpus ++;
                                cur_rv->count[i] = (dis_cpus << 16) | (cur_rv->count[i] & 0xffff);
                        }
                }

        }
        return 1 ;
}


int
count_cpus(lboard_t *l, void *ap, void *rp)
{
        klinfo_t        *kliptr ;
        int             cur_rv = *(int *)rp ;
        int             total_cpus = (cur_rv&0xffff),
                        dis_cpus   = (cur_rv >> 16) ;

        if (l->brd_type == KLTYPE_IP27) {
                kliptr = NULL ;
                while(kliptr = find_component(l, kliptr, KLSTRUCT_CPU)) {
                        total_cpus++ ;
                        if (!(KLCONFIG_INFO_ENABLED(kliptr)))
                                dis_cpus ++ ;
                }

                cur_rv = (dis_cpus << 16) | (total_cpus) ;

                *(int *)rp = cur_rv ;
        }
        return 1 ;
}

int
kl_get_num_cpus()
{
        int             num_cpus = 0 ;
        visit_lboard(count_cpus, NULL, &num_cpus) ;
        return (num_cpus & 0xffff) ;
}

int
hinv_get_num_cpus(cpu_list_t *cpu_list)
{
        int i, cpu_cnt;
        cpu_list->size = 0;
        visit_lboard(fill_cpu_list, NULL, cpu_list) ;

        return 0;
}


/*
 * match_lb
 *     visit_lboard callback.
 *     Get a lboard_t with matching module and slotid.
 */

int
match_lb(lboard_t *lb, void *ap, void *rp)
{
    lboard_t    *lb1 = (lboard_t *)ap ;
    lboard_t    *lb2 = (lboard_t *)rp ;

    /* 
     * Two lboards match according to the following criteria:
     *		1. They should be in the same module
     *		2. If the system is a normal Origin200 then
     *		   the boards should belong to the same class.
     *		3. If the system is an Origin2000 or a Origin200
     *		   with an xbox then the lboard's slotids should
     *		   also match. Note that this check encompasses
     * 		   the check that the lboards belong to the same
     * 		   class since the boards of different classes
     *		   are supposed to have different slotids.
     */
    if ((lb->brd_module == lb1->brd_module) &&
        ((SN00 && !is_xbox_config(get_nasid())) || 
	 (lb->brd_slot   == lb1->brd_slot))) {
	/* 
	 * For SN00, since we match only on modid, it
	 * will match for both CPU and IO boards. So
	 * we need to match the class also.
	 */
        if (SN00)
            if (KLCLASS(lb->brd_type) != KLCLASS(lb1->brd_type))
                return 1 ;

            *(lboard_t **)rp = lb ;
            return 0 ;
    }
    return 1 ;
}

lboard_t *
get_match_lb(lboard_t *lb, int flag)
{
    lboard_t	*rlb = 0 ;
    klinfo_t	*k;

    visit_lboard(match_lb, (void *)lb, (void *)&rlb) ;

    /*if ((!rlb) || flag) {
	n = get_dis_node_brd_nasid(	lb->brd_module, 
					(lb->brd_slot&KLTYPE_MASK)) ;
	if (n != INVALID_NASID) {
		lb->brd_nasid = n ;
		return lb ;
	}
    }*/

    /* hack to update nasid on disabled boards */
    if(rlb && !(rlb->brd_flags & ENABLE_BOARD) && (k = find_component(rlb,NULL,KLSTRUCT_HUB)))
    {
	rlb->brd_nasid = k->physid;
    }

    return rlb ;
}

#define MIDPLANE_NAME  "MPLN"

int
check_brd_nic(lboard_t *lb, void *ap, void *rp)
{
    klhub_t *kh ;
    char    *sp, *tmp ;
    nic_t   newnic = 0, inic = *(nic_t *)ap, *onicp = (nic_t *)rp ;

    if (lb->brd_type == KLTYPE_IP27) {
        if (kh = (klhub_t *)find_component(lb, NULL, KLSTRUCT_HUB))
            if (kh->hub_mfg_nic) {
		tmp = (char *)NODE_OFFSET_TO_K1(lb->brd_nasid, 
				kh->hub_mfg_nic) ;
		sp = (sp = strstr(tmp, "MODULEID")) ? sp : 
		      strstr(tmp, MIDPLANE_NAME) ;
                if (sp)
                    if (sp = strstr(sp, "Laser:")) {
                        newnic = strtoull(sp+6, NULL, 16) ;
#ifdef DEBUG
                        db_printf("sys nic %lx\n", newnic) ;
#endif
                        if (!inic) {
                            *onicp = newnic ;
                            return 0 ;
                        } else if (inic == newnic) {
                            *onicp = 1 ;
                            return 0 ;
                        } else return 1 ;
                    }
            }
    }
    return 1 ;
}

/*
 * check_sys_nic
 *     checks if the given nic 
 *     can be found anywhere in the system.
 */

/* ARGSUSED */
nic_t
check_sys_nic(nic_t nic)
{
    nic_t rv = 0 ;

    /* No problem with SN00. The master NIC is part of the
       mother board. */

    if (SN00)
        return 1 ;

    visit_lboard(check_brd_nic, (void *)&nic, (void *)&rv) ;
    return rv ;
}

/*
 * get_sys_nic
 *     gets the nic of the midplane in one of the modules in the system
 */

nic_t
get_sys_nic()
{
    nic_t rv = 0, nic = 0 ;
    if (SN00)
        return 1 ;
    visit_lboard(check_brd_nic, (void *)&nic, (void *)&rv) ;
    return rv ;
}

void check_prev_part_info(void) ;

char *
make_serial(flag)
{
    char sname[SLOTNUM_MAXLENGTH] ;
    char sav_cons_path[NVLEN_SAV_CONSOLE_PATH] ;
    char *mvmsg = 
    "***WARNING: Console has moved from %s to %s\n"
    "Check for LINK errors or setenv ConsolePath to new value.\n" ;
    char *hwmod = "/hw/module" ;
    extern struct string_list environ_str;
    char *cp ;

    bzero(sav_cons_path, NVLEN_SAV_CONSOLE_PATH) ;

    /* Get the current ConsolePath */
    get_slotname(master_baseio_slotid, sname) ;
    sprintf(master_console_path, "/hw/module/%d/slot/%s",
	    master_baseio_modid, sname) ;

    /* Get the saved ConsolePath */
    cpu_get_nvram_buf(NVOFF_SAV_CONSOLE_PATH, 
		      NVLEN_SAV_CONSOLE_PATH, sav_cons_path) ;

    /* Init it if not initted */
    if (strncmp(sav_cons_path, hwmod, strlen(hwmod))) {
 	cpu_set_sav_cons_path(master_console_path) ;
	strcpy(sav_cons_path, master_console_path) ;
    }

    /* Complain if it differs from ConsolePath env var */
    if (strcmp(sav_cons_path, master_console_path)) {
	printf(mvmsg, sav_cons_path, master_console_path) ;
    }

    if (flag && master_baseio_modid) {
        cpu_set_nvram_offset(NVOFF_CONSOLE_PATH, 
			     NVLEN_CONSOLE_PATH, master_console_path) ;
	replace_str("ConsolePath", master_console_path, &environ_str) ;
    }

    if (!_symmon)
    	if ((cp = getenv("RestorePartEnv")) && (cp) && (*cp == 'y'))
    	    check_prev_part_info() ;

    return NULL ;
}

/*
 * The following is a set of routines that return
 * the default value of a few important prom variables
 * like the normal console, graphics console, keyboard
 * and mouse. On EVEREST and other systems, it was called
 * tty(0) or video(0) ... using the ARCS convention.
 * SN series of systems use the HWGRAPH convention. So,
 * these values look like file names. The file names like /dev/...
 * are aliases to the full hwgraph path names which also work
 * in place of these. The file names like /dev/... still exist
 * for backward compatibility. Older NVRAMs contain this path.
 */

#define CONSOLEIN_DEFAULT 	"/dev/tty/ioc30"
#define CONSOLEIN_DEFAULT_1 	"/dev/tty/ioc31"

/*
 * cpu_get_serial()
 * If console is d2, use tty port 1. rbaud, which is
 * associated with this, still does not work on SN0.
 */

char *
cpu_get_serial(void)
{
	char *p ;

	if ((p = getenv("console")) && (p) && 
	    (!strcmp(p,"d2")))
 		return CONSOLEIN_DEFAULT_1 ;
	else
 		return CONSOLEIN_DEFAULT ;
}

/*
 * cpu_get_disp_str
 */

char *
cpu_get_disp_str(void)
{
 	return "/dev/graphics/textport";
}

/*
 * cpu_get_kbd_str()
 */

char *
cpu_get_kbd_str(void)
{
	if (_symmon)
		return cpu_get_serial() ;

	return "/dev/input/ioc3pckm0";
}

/*
 * cpu_get_mouse()
 */

char *
cpu_get_mouse(void)
{
        return "/dev/input/ioc3ms0";
}

/*
 * cpu_errputc()
 *
 * Print a single character to the system's console.  The console
 * can be switched between the Manufacturing mode console, which
 * hangs off the the System Controller, and the System console, which
 * hangs off of the serial port.  We switch dynamically based on
 * an as yet undecided global.
 */

volatile uart_reg_t *cntrl ;

void
cpu_errputc(char c)
{
    int timeleft;

    if (sk_sable)
	timeleft = 1;
    else
	timeleft = 10000;

#ifndef SABLE
    {

	if (cntrl == NULL) {
		cntrl = (uart_reg_t *)
			(KL_CONFIG_CH_CONS_INFO(get_nasid())->uart_base) ;
		/* be absolutely sure nasid is OR'ed in */
		cntrl = (volatile uart_reg_t *) 
			TO_NODE(get_nasid(), (__psunsigned_t) cntrl); 
	}

	if (cntrl) {
        	while (!(RD_REG(cntrl, LINE_STATUS_REG) & 
					LSR_XMIT_BUF_EMPTY)) {
			us_delay(timeleft) ;
		}

        	WR_REG(cntrl, XMIT_BUF_REG, c);
		us_delay(timeleft) ;
	}

	return ;
    }
#endif

#if defined(SABLE)
    while (timeleft-- && !((*(volatile long *)KL_UART_CMD) & I8251_TXRDY))
	    /* spin */ ;
    *(volatile long *)KL_UART_DATA = c;
#endif /* SABLE */

}

int cpu_errpoll(void)
{
	uchar_t		status;

	cntrl = (uart_reg_t *)(KL_CONFIG_CH_CONS_INFO(get_nasid())->uart_base) ;
	cntrl = (volatile uart_reg_t *) TO_NODE(get_nasid(), (__psunsigned_t) cntrl); /* be absolutely sure nasid is OR'ed in */

	if (cntrl) {
		status = RD_REG(cntrl, LINE_STATUS_REG);

		if (status & (LSR_OVERRUN_ERR | LSR_PARITY_ERR | 
			LSR_FRAMING_ERR))
			return 0;
		return ((status & LSR_DATA_READY) != 0);
	}

	return 0;
}

int cpu_errgetc(void)
{
	cntrl = (uart_reg_t *)(KL_CONFIG_CH_CONS_INFO(get_nasid())->uart_base) ;
	cntrl = (volatile uart_reg_t *) TO_NODE(get_nasid(), (__psunsigned_t) cntrl); /* be absolutely sure nasid is OR'ed in */

	if (!cntrl)
		return 0;

	while (!cpu_errpoll());

	return RD_REG(cntrl, RCV_BUF_REG);
}

/*
 * cpu_mem_init()
 */

void
cpu_mem_init(void)
{
    unsigned int numfreepages;
    extern int _ftext[];
    extern int _end[];


    /* Allocate space for the IO6 PROM.  And its various pieces (GFXPROM,
     * ENET bufs, etc.  We use the _ftext and _end defines so that we
     * can move the PROM around in memory without having to recompile.
     */
	/* Add 2 pages for Exception and SysParam Block. This has
 	   been split somewhere else. 
		0      - 0x1000 -> 1 page
		0x1000 - 0x2000 -> 1 page
	*/
    md_add(TO_NODE(get_nasid(), PHYS_RAMBASE), 2, FirmwareTemporary);

	/* IO6 PROM Text and Stack.
	   0x01e01000 - ?? -> 0436 pages
	*/

    md_add(kvtophys(_ftext),
	   arcs_btop(kvtophys(_end) - kvtophys(_ftext)),
	   FirmwareTemporary);

	/* 0x1300000 - 0x1400000 -> 256 pages */

    md_add(TO_NODE(get_nasid(), K0_TO_PHYS(FLASHBUF_BASE)), 
	arcs_btop(FLASHPROM_SIZE), FirmwareTemporary);

	/* 0x01500000 - 0x1700000  512 pages - 2MB */

    md_add(TO_NODE(get_nasid(), K0_TO_PHYS(DIAG_BASE)), 
	   arcs_btop(DIAG_SIZE), FirmwareTemporary);

	/* 0x01800000 - 0x1a00000  512 pages - 2MB */

    md_add(TO_NODE(get_nasid(), K0_TO_PHYS(ROUTE_BASE)), 
	   arcs_btop(ROUTE_SIZE), FirmwareTemporary);

	/* 0x01f80000 - ??         32  pages - 128 K */

    md_add(TO_NODE(get_nasid(), K0_TO_PHYS(ENETBUFS_BASE)), 
	   arcs_btop(ENETBUFS_SIZE), FirmwareTemporary);

    /* Free all of the space between the beginning of the NODEBUGUNIX 
     * address and the base of the flash prom buffer.
     * and from top of flashprom buffer to base of 3rd party prom driver space.
     */

    md_add(TO_NODE(get_nasid(), K0_TO_PHYS(NODEBUGUNIX_ADDR)), 
	   arcs_btop(MISC_PROM_BASE - NODEBUGUNIX_ADDR), FreeMemory);

    md_add(TO_NODE(get_nasid(), K0_TO_PHYS(FLASHBUF_BASE + FLASHPROM_SIZE)), 
	   arcs_btop(MEG), FreeMemory);

#if 0
    md_add(K0_TO_PHYS(SLAVESTACK_BASE), arcs_btop(SLAVESTACK_SIZE), 
	   FirmwareTemporary);
#endif

    /* Put a permanent memory descriptor in for the IP27 PROM 
     */

	/* 0x01a00000 - ?? -> 256 pages = 1 MEG */

    md_add(TO_NODE(get_nasid(), K0_TO_PHYS(IP27PROM_BASE)), 
	arcs_btop(IP27PROM_SIZE_MAX), FirmwarePermanent);

    /* Put in a memory descriptor for the space occupied by the 
     * debugging PROM.
     */
    md_add(TO_NODE(get_nasid(), K0_TO_PHYS(IO6DPROM_BASE)), 
	arcs_btop(IO6DPROM_SIZE), FreeMemory);

#if 0
    /* ON SN0, since memory is segmented, the value returned by
       cpu_get_memsize cannot be used in the way below. It has
       to be combined with the mem layout info to either build
       the correct mem descs or just assume that each node will
       have only 32mb for the prom. This #if 0 does this.
       Also, we may have some of the memory banks disabled.
    */
    /* Finally, all remaining memory above 32 meg (if any exists) is
     * available for use.  Because the PROM is limited by the size of
     * Kseg0, we never allow more than 100,000 free pages.
     */

    numfreepages = (unsigned int)cpu_get_memsize() 
		    - (unsigned int)arcs_btop(K0_TO_PHYS(FREEMEM_BASE));
    if (numfreepages > 100000)
	numfreepages = 100000;

    if (numfreepages  > 0)
	md_add(TO_NODE(get_nasid(), K0_TO_PHYS(FREEMEM_BASE)), 
		numfreepages, FreeMemory);
#endif
}



/*
 * cpu_acpanic()
 */

void
cpu_acpanic(char *str)
{
    printf("Cannot malloc %s component of config tree\n", str);
    panic("Incomplete config tree -- cannot continue.\n");
}


/*
 * cpu_savecfg()
 */

LONG
cpu_savecfg(void)
{
    return ENOSPC;
}


int
sn0_getcpuid(void)
{
	return(cpuid()) ;
}

/*
 * Clear all appropriate error latches in the system.
 * To be used mainly in conjunction with "nofault"
 * accesses.
 */
void
cpu_clear_errors(void)
{
	/* TBD */
}


/*
 * Print all appropriate error state in system.
 */
void
cpu_print_errors(void)
{
	/* TBD */ }


/*
 * EPC support
 */

static COMPONENT ioc3tmpl = {
	AdapterClass,			/* Class */
	MultiFunctionAdapter,		/* Type */
	(IDENTIFIERFLAG)0,		/* Flags */
	SGI_ARCS_VERS,			/* Version */
	SGI_ARCS_REV,			/* Revision */
	0,				/* Key */
	0x01,				/* Affinity */
	0,				/* ConfigurationDataSize */
	9,				/* IdentifierLength */
	"IOC3-1.0"			/* Identifier */
};

/* Move to ioc3.c? */
void
ioc3_install(COMPONENT *root)
{
	printf("fixme: ioc3_install should not be called\n") ;
	root = AddChild(root,&ioc3tmpl,(void *)NULL);
	if (root == (COMPONENT *)NULL) cpu_acpanic("IOC3");

	return;
}

#define	TOD_SGS_M48T35		1
#define TOD_DALLAS_DS1386	2

static __psunsigned_t  	nvram_base ;
static int		tod_chip_type ;

static int
get_tod_chip_type(void)
{
	char	testval ;

	PCI_OUTB(RTC_DAL_CONTROL_ADDR, RTC_DAL_UPDATE_DISABLE) ;
	PCI_OUTB(RTC_DAL_DAY_ADDR, 0xff) ;
	PCI_OUTB(RTC_DAL_CONTROL_ADDR, RTC_DAL_UPDATE_ENABLE) ;

	testval = PCI_INB(RTC_DAL_DAY_ADDR) ;
	if (testval == 0xff)
		tod_chip_type = TOD_SGS_M48T35 ;
	else
		tod_chip_type = TOD_DALLAS_DS1386 ;

	return tod_chip_type ;
}

/*
 * cpu_get_tod -- get current time-of-day from RTC and prom info, and 
 * return it in ARCS TIMEINFO format.  the libsc routine get_tod will 
 * convert it to seconds for those who need it in that form.
 */
void
cpu_get_tod(TIMEINFO *t)
{
        int month, day, hours, mins, secs, year;

	if (nvram_base == NULL) {
    		nvram_base = kl_ioc3nvram_base(get_nasid());
		if (!nvram_base)
			return ; /* Actually we should PANIC !!! here */
		tod_chip_type = get_tod_chip_type() ;
	}

	switch (tod_chip_type) {
		case TOD_SGS_M48T35:
        		PCI_OUTB(RTC_SGS_CONTROL_ADDR, RTC_SGS_READ_PROTECT);
        		secs = BCD_TO_INT(PCI_INB(RTC_SGS_SEC_ADDR));
        		mins = BCD_TO_INT(PCI_INB(RTC_SGS_MIN_ADDR));
        		hours = BCD_TO_INT(PCI_INB(RTC_SGS_HOUR_ADDR));
        		day = BCD_TO_INT(PCI_INB(RTC_SGS_DATE_ADDR));
        		month = BCD_TO_INT(PCI_INB(RTC_SGS_MONTH_ADDR));
        		year =  BCD_TO_INT(PCI_INB(RTC_SGS_YEAR_ADDR));
        		PCI_OUTB(RTC_SGS_CONTROL_ADDR, 0);
			break ;

		case TOD_DALLAS_DS1386:
        		PCI_OUTB(RTC_DAL_CONTROL_ADDR, RTC_DAL_UPDATE_DISABLE);
        		secs = BCD_TO_INT(PCI_INB(RTC_DAL_SEC_ADDR));
        		mins = BCD_TO_INT(PCI_INB(RTC_DAL_MIN_ADDR));
        		hours = BCD_TO_INT(PCI_INB(RTC_DAL_HOUR_ADDR));
        		day = BCD_TO_INT(PCI_INB(RTC_DAL_DATE_ADDR));
        		month = BCD_TO_INT(PCI_INB(RTC_DAL_MONTH_ADDR));
        		year =  BCD_TO_INT(PCI_INB(RTC_DAL_YEAR_ADDR));
        		PCI_OUTB(RTC_DAL_CONTROL_ADDR, RTC_DAL_UPDATE_ENABLE);
			break ;

		default:
			break ;
	}

#if 0
        PCI_OUTB(RTC_CONTROL_ADDR, RTC_READ_PROTECT);
        secs = BCD_TO_INT(PCI_INB(RTC_SEC_ADDR));
        mins = BCD_TO_INT(PCI_INB(RTC_MIN_ADDR));
        hours = BCD_TO_INT(PCI_INB(RTC_HOUR_ADDR));
        day = BCD_TO_INT(PCI_INB(RTC_DATE_ADDR));
        month = BCD_TO_INT(PCI_INB(RTC_MONTH_ADDR));
        year =  BCD_TO_INT(PCI_INB(RTC_YEAR_ADDR));
        PCI_OUTB(RTC_CONTROL_ADDR, 0);
#endif

	year += YRREF ;

#if 0
	printf("cpu_get_tod: %d:%d:%d %d/%d/%d\n",
		hours,mins,secs, year,month,day);
#endif

        t->Month 	= month;
        t->Day 		= day;
        t->Year 	= year ;
        t->Hour 	= hours;
        t->Minutes 	= mins;
        t->Seconds 	= secs;
        t->Milliseconds = 0 ;

    	return;
}

/*ARGSUSED*/
void
spunlock(lock_t lock, int ospl)
{
/* TBD */
}

/*
 * ecc_error_decode()
 *	Does something, don't know exactly what.
 */

/*ARGSUSED*/
void
ecc_error_decode(u_int ecc_error_reg)
{
#if 0
    printf("ecc_error_decode: Not Yet Implemented.\n");
#endif
}

/*
 * kl_scsiedtinit()
 * 	Do SN0-specific SCSI probe and initialization code.
 */

void
kl_scsiedtinit(COMPONENT *c)
{
    printf("kl_scsiedtinit should not be called for SN0\n");
}

/*
 * master_io6()
 *	Finds the master IO6 board in the system and returns its slot
 *	number.
 */

static unsigned int
master_io6(void)
{
    printf("fixme: master_io6\n"); /* XXX */
    return 0;
}

int
flush_iocache(int slot)
{
    /* Flush the processor data caches */
    flush_cache();
    return 0;
}

void
kl_flush_caches(void)
{
    flush_iocache(0);
}

void
FlushAllCaches(void)
{
    kl_flush_caches();
}

/*
 * Make an entry in the TLB to map the IOspace corresponding to IO6 'window '
 * and adapter 'anum' into K2 space address
 */
/*
 * Function : tlbentry
 * Description :
 * Make an entry in the TLB for the given window and adapter No,
 * and return the virtual address mapped.
 * This is a simpleton tlb mapping, and is used mostly to
 * map the diagnostic accesses to the Large window address.
 *
 * Caution : User of tlbentry should make sure to remove this entry
 *           after its use. Otherwise the table may overflow.
 */

/*
 * NOTE :
 * Graphics cannot use this routine since it needs a fixed address to 
 * be remapped a few times to different physical address .. 
 * Graphics now uses address 0xf0000000 and DONT use that address 
 */
#define	FIRST_TLB_ENTRY	2
#define	MAX_TLBSZ	8

#define	VADDR_BASE	( IOMAP_BASE + 0x20000000 )

unsigned tlb_entries[MAX_TLBSZ];

caddr_t
tlbentry(int window, int anum, unsigned offset)
/* offset should be < IOMAP_VPAGESIZE */
{
    unsigned    vpn2 ,old_pgmask, pfno;
    pde_t	pfnevn, pfnodd;
    int         i;

    /* Get the address we can use.      */
    for (i=0; i < MAX_TLBSZ; i++){
        if (tlb_entries[i] == 0){
            tlb_entries[i] = 1;
            break;
        }
    }

    if (i == MAX_TLBSZ)
        panic("TLB entries exhausted...\n");
   

    vpn2  = (unsigned) (VADDR_BASE + ( i << (IOMAP_VPAGESHIFT + 1)));
    tlb_entries[i] = vpn2;

#if 1
    /* printf("fixme: getcachesz\n"); XXX */
    pfno = -1;
#else
    pfno  = LWIN_PFN(window, anum) + (offset >> 12);
#endif
    pfnevn.pgi = (pfno << 6 ) | 0x17; /* UNCACHED|MODIFIED|VALID|GLOBAL */
    pfno  += (IOMAP_VPAGESIZE >> 12);
    pfnodd.pgi = (pfno << 6 ) | 0x17;
    old_pgmask = get_pgmask();
    set_pgmask(IOMAP_TLBPAGEMASK);
    tlbwired(i+FIRST_TLB_ENTRY, 0, (caddr_t)(__psunsigned_t)vpn2, pfnevn.pgi, pfnodd.pgi);
    set_pgmask(old_pgmask);

    return((caddr_t)(__psunsigned_t)vpn2);
}

int
tlbrmentry(caddr_t vaddr)
{
    __scunsigned_t  i;
    
    i = ((__scunsigned_t)vaddr - VADDR_BASE) >> (IOMAP_VPAGESHIFT + 1) ;

    if (tlb_entries[i] == 0)  
        return(-1);

    invaltlb((int)(i+FIRST_TLB_ENTRY));
    tlb_entries[i] = 0;

    return (0);
}

/*
 * just call in an ascending loop till returns != 0
 * added for ide - don't want ide to know about internals over here
 */
int
inval_tlbentry(uint slot)
{
    if (slot >= MAX_TLBSZ)
        return(-1);

    invaltlb(slot+(int)FIRST_TLB_ENTRY);
    tlb_entries[slot] = 0;

    return (0);
}

void
tlbrall(void)
{
    int i;

    /* zero all tlb entries */
    for (i=0; i < MAX_TLBSZ; i++) {
	invaltlb(i+(int)FIRST_TLB_ENTRY);
	tlb_entries[i] = 0;
    }
}

void
cpu_scandevs(void)
{
}

/* check if address is inside a "protected" area */
/*ARGSUSED*/
int
is_protected_adrs(unsigned long low, unsigned long high)
{
    return 0;	/* none */
}

/*
 * Make a PCI DMA Address on SN0.
 */

__psunsigned_t
get_pci64_dma_addr(__psunsigned_t addr, ULONG key)
{
        /* XXX Add attribute bits */

        nasid_t nasid = GET_HSTNASID_FROM_KEY(key) ;
        __psunsigned_t  dmaaddr = 0 ;
        unsigned char   hubwid = GET_HUBWID_FROM_KEY(key) ;

        addr = kvtophys((void *) addr) ; /* Phys addr only */
        dmaaddr = addr | NODE_OFFSET(nasid) ;
        dmaaddr |= ((__psunsigned_t)hubwid << PCI_64_TARGID_SHFT) ;
	dmaaddr |= (PCI64_ATTR_PREC|PCI64_ATTR_BAR);
        return(dmaaddr) ;
}

#if 0

#include <saio.h>

extern IOBLOCK *net_iob;

/*VARARGS1*/
int
netprintf(const char *fmt, ...)
{
	va_list	ap;
	char	scanset = 0;
	int	rc;

	return 1;

#if 0

	if (net_iob && (net_iob->Flags & F_SCAN)) {
		net_iob->Flags &= ~F_SCAN;
		scanset = 1;
	}

	va_start(ap, fmt);
	vprintf(fmt, (char *)ap);
	va_end(ap);

	if (scanset) {
		net_iob->Flags |= F_SCAN;
	}

	return  rc;
#endif
}

#endif

klinfo_t *
kl_find_type(CONFIGTYPE type, void *prev)
{
        char                    tmp_buf[32] ;
        graph_error_t           graph_err ;
        vertex_hdl_t            vhdl, vhdl1, *pvhdl  ;
        char                    *remain ;
        klinfo_t                *kliptr ;
        graph_edge_place_t      eplace = GRAPH_EDGE_PLACE_NONE ;
	DECLARE_HWGHDL

	CHOOSE_GHDL
	CHOOSE_RVHDL

        pvhdl = (vertex_hdl_t *)prev ;
        strcpy(tmp_buf, "/dev/") ;
        switch (type) {
                case NetworkController:
                        strcat(tmp_buf, "ethernet") ;
                        type = KLSTRUCT_IOC3ENET ;
                break ;

                case CDROMController:
                        strcat(tmp_buf, "cdrom") ;
                        type = KLSTRUCT_CDROM ;
                break ;

                case TapeController:
                        strcat(tmp_buf, "tape") ;
                        type = KLSTRUCT_TAPE ;
                break ;

                default:
                        return 0 ;

        }

        if ((graph_err = hwgraph_path_lookup(rvhdl, tmp_buf,
                                &vhdl, &remain)) != GRAPH_SUCCESS) {
                printf("kl_find_type: Cannot find device of type %s, %d\n",
                                tmp_buf, graph_err) ;
                return 0 ;
        }

        if ((pvhdl) && (*pvhdl)) { /* search for the previous edge */
            do {
                graph_err = graph_edge_get_next(ghdl,
                                            vhdl,
                                            tmp_buf,
                                            &vhdl1, &eplace) ;
                if (graph_err != GRAPH_SUCCESS) {
                    return 0 ;
                }
            }
            while (*pvhdl != vhdl1) ;
        }

        graph_err = graph_edge_get_next(ghdl,
                                        vhdl,
                                        tmp_buf,
                                        &vhdl1, &eplace) ;
        if (graph_err != GRAPH_SUCCESS) {
            return 0 ;
        }

        if (pvhdl)
            *(vertex_hdl_t *)pvhdl = vhdl1 ;

        graph_err = graph_info_get_LBL(ghdl, vhdl1,
                                        INFO_LBL_KLCFG_INFO,NULL,
                                        (arbitrary_info_t *)&kliptr) ;
        if (graph_err != GRAPH_SUCCESS) {
                 printf("kl_find_type: Cannot find info for device %s\n", tmp_buf) ;
                 return 0 ;
        }

        if (kliptr->struct_type == type)
                return (kliptr) ;
        else
                return 0 ;
}

int
kl_count_type(CONFIGTYPE type)
{
        char tmp_buf[32] ;
        graph_error_t   graph_err ;
        vertex_hdl_t    vhdl, vhdl1  ;
        char            *remain ;
        klinfo_t        *kliptr ;
        graph_edge_place_t      eplace = GRAPH_EDGE_PLACE_NONE ;
        DECLARE_HWGHDL

        CHOOSE_GHDL
        CHOOSE_RVHDL

        strcpy(tmp_buf, "/dev/") ;
        switch (type) {
                case NetworkController:
                        strcat(tmp_buf, "ethernet") ;
                        type = KLSTRUCT_IOC3ENET ;
                break ;

                case CDROMController:
                        strcat(tmp_buf, "cdrom") ;
                        type = KLSTRUCT_CDROM ;
                break ;

                case TapeController:
                        strcat(tmp_buf, "tape") ;
                        type = KLSTRUCT_TAPE ;
                break ;

                default:
                        return 0 ;

        }

        if ((graph_err = hwgraph_path_lookup(rvhdl, tmp_buf,
                                &vhdl, &remain)) != GRAPH_SUCCESS) {
                printf("kl_find_type: %s not found in HWGRAPH %d\n",
                                tmp_buf, graph_err) ;
                return 0 ;
        }

        graph_err = graph_edge_get_next(ghdl,
                                        vhdl,
                                        tmp_buf,
                                        &vhdl1, &eplace) ;
        if (graph_err != GRAPH_SUCCESS) {
                  return 0 ;
        }

        graph_err = graph_info_get_LBL(ghdl, vhdl1,
                                        INFO_LBL_KLCFG_INFO,NULL,
                                        (arbitrary_info_t *)&kliptr) ;
        if (graph_err != GRAPH_SUCCESS) {
                 printf("kl_find_type: Target node has no INFO label\n") ;
                 return 0 ;
        }

        if (kliptr->struct_type == type)
                return (1) ;
        else
                return 0 ;


}

#define MAXLIST 26

void
kl_showlist(void *root, CONFIGTYPE type, void *list, int gui)
{
    klinfo_t    *kliptr ;
    cfgnode_t   *ac ;
    int                 i ;
    COMPONENT   **newlist ;
    char            buf[128] ;
    vertex_hdl_t        vhdl = NULL ;

    newlist = (COMPONENT **)list ;

    /* XXX for now, find the first device. later integrate
       kl_find_type and this routine.
    */

    while (kliptr = kl_find_type(type, (void *)&vhdl)) {

        ac = (cfgnode_t *)malloc(sizeof(cfgnode_t)) ;
        bzero(ac, sizeof(cfgnode_t)) ;

        ac->parent = (cfgnode_t *)malloc(sizeof(cfgnode_t)) ;
        bzero(ac->parent, sizeof(cfgnode_t)) ;

        bcopy(kliptr->arcs_compt, ac, sizeof(COMPONENT)) ;

        ac->comp.Type = type ;
        if (type == CDROMController)
                ac->comp.Key = (kliptr->arcs_compt->Key) ;
        else
        if ((type == TapeController) || (type == TapePeripheral))
                ac->comp.Key = (kliptr->arcs_compt->Key) ;
        else
                ac->comp.Key = 0 ;
        ac->parent->comp.Type = type ;

        if (gui) {
            struct _radioButton *b;
            sn0_dev2eng(&ac->comp,buf) ;
            b = appendRadioList(list,buf,0);
            if (b) b->userdata = (void *)&ac->comp ;
        }

        for (i=0 ; i < MAXLIST; i++)
                if (newlist[i] == 0) {
                        newlist[i] = (COMPONENT *)ac;
                        break;
                }
    } /* while */

}

void
sn0_dev2eng(COMPONENT *c, char *buf)
{
	klinfo_t	*kliptr ;
	char		*host = getenv("netinsthost");
	char		*file = getenv("netinstfile");

        if (!(kliptr = kl_find_type(c->Type, NULL))) {
        	*buf = 0;
		return ;
	}

        switch (c->Type) {
        case CDROMController:
                sprintf(buf,"Local SCSI CD-ROM drive %d, on controller %d", 
		(kliptr->arcs_compt->Key&0xff), 
		(kliptr->arcs_compt->Key>>8)) ;
		break ;
        case NetworkController:
                sprintf(buf,"Remote directory %s from server %s.",
                                file,host);
                break ;
        case TapeController:
                sprintf(buf,"Local Tape drive %d, on controller %d",
                (kliptr->arcs_compt->Key&0xff),
                (kliptr->arcs_compt->Key>>8)) ;
                break ;
        default:
                *buf = 0;
                break ;

	}
}

extern int iscdrom ;

char *
kl_fixupname(COMPONENT *tape)
{
        static char name[256] ;

	name[0] = 0 ; 

        if (iscdrom = (tape->Type == CDROMController ||
            ((COMPONENT *)((cfgnode_t *)tape)->parent)->Type 
				== CDROMController)) {
		sprintf(name, "scsi(%d)cdrom(%d)partition(8)\0",
			(uchar)(tape->Key >> 8), (uchar)tape->Key) ;
	}

        if (tape->Type == NetworkController) {
                char *cp;

                strcat(name,"bootp()");

                cp = getenv("netinsthost");
                if (cp) {
                        strcat(name,cp);
                        cp = getenv("netinstfile");
                        if (cp) {
                                strcat(name,":");
                                strcat(name,cp);
                        }
                }
        }

        if ((tape->Type == TapeController) || 
	    (tape->Type == TapePeripheral) ||
            (((COMPONENT *)((cfgnode_t *)tape)->parent)->Type
                                == TapeController) || 
	    (((COMPONENT *)((cfgnode_t *)tape)->parent)->Type
                                == TapePeripheral)) {
                sprintf(name, "scsi(%d)tape(%d)\0",
                        (uchar)(tape->Key >> 8), (uchar)tape->Key) ;
	}

        return(name);
}

char *
kl_inv_find()
{
        graph_error_t   graph_err ;
        char            *label_name ;
        DECLARE_HWGHDL

        CHOOSE_GHDL
        CHOOSE_RVHDL

        graph_err = graph_info_get_LBL(ghdl, rvhdl,
                                       INFO_LBL_VERTEX_NAME, NULL,
                                       (arbitrary_info_t *)&label_name) ;
        if (graph_err == GRAPH_SUCCESS) 
		return label_name ;
	else
		return NULL ;
}

char *
copy_nic_field(char *n, char *h)
{
	while (*n != ';')
		*h++ = *n++ ;
	*h = 0 ;

	return ++n ;
}

#ifndef SN_PDI
void  	*
pg_get_lbl(vertex_hdl_t vhdl, char *label)
{
	graph_error_t           gerr ;
	void 			*k ;

        gerr = graph_info_get_LBL(prom_graph_hdl, vhdl,
                                       label, NULL,
                                       (arbitrary_info_t *)&k) ;
	if (gerr == GRAPH_SUCCESS)
		return k ;
	return NULL ;

}

int
pg_add_lbl(vertex_hdl_t vhdl, char *label, void *info)
{
    graph_error_t gerr ;

    gerr = graph_info_add_LBL(prom_graph_hdl, vhdl,
                              label, NULL,
                              (arbitrary_info_t) info) ;
    return gerr ;
}
#endif /* SN_PDI */

int
is_ConsoleIn_valid()
{
	char *s1, *s2 ;

	s1 = getenv("ConsoleIn") ;
	s2 = cpu_get_kbd_str() ;
	if (!strcmp(s1, s2))
		return 1 ;
	return 0 ;
}


int 
pg_add_nic_info(klinfo_t *k, vertex_hdl_t vhdl)
{
    char            *mfg_nic ;
    klconf_off_t    mfg_off ;
    graph_error_t   gerr ;

    switch(k->struct_type) {
        case KLSTRUCT_BRI:
            mfg_off = ((klbri_t *)k)->bri_mfg_nic ;
        break;

        case KLSTRUCT_HUB:
            mfg_off = ((klhub_t *)k)->hub_mfg_nic ;
        break;

        case KLSTRUCT_ROU:
            mfg_off = ((klrou_t *)k)->rou_mfg_nic ;
        break;

        case KLSTRUCT_GFX:
            mfg_off = ((klgfx_t *)k)->gfx_mfg_nic ;
        break;
	
	case KLSTRUCT_TPU:
	    mfg_off = ((kltpu_t *)k)->tpu_mfg_nic ;
	break;

	case KLSTRUCT_GSN_A:
	case KLSTRUCT_GSN_B:
	    mfg_off = ((klgsn_t *)k)->gsn_mfg_nic ;
	break;

        case KLSTRUCT_XTHD:
            mfg_off = ((klxthd_t *)k)->xthd_mfg_nic ;
        break;

        default:
        break ;
    }

    mfg_nic = (char *)NODE_OFFSET_TO_K1(k->nasid, mfg_off) ;
    if (gerr = pg_add_lbl(vhdl, INFO_LBL_NIC, (void *)mfg_nic)) {
        printf("%d: add LBL NIC error %d\n", k->nasid, gerr);
        return 0;
    }
    return 1 ;
}

#if 0
/* support for completely disabling node boards */

#include <sn_macros.h>
#include <sys/SN/SN0/ip27log.h>

/* struct to remember the nasids of node boards that are
 * completely disabled. This means no memory on board too.
 * In such a situation, this brd has no klconfig and is
 * invisible except to the prom, using promlog variables.
 */

static nasid_t	last_nasid, trying_nasid ;

typedef struct dis_nbrd_s {
        moduleid_t      mod ;
        slotid_t        slot ;
} dis_nbrd_t ;

dis_nbrd_t	dis_node_brd_table[MAX_NASIDS/2] ;

void
do_dis_node_brds(void)
{
	char		buf[128] ;
	int             mod, slot = 0 ;
	char		*tmp = NULL ;
        jmp_buf         new_buf;
        void            *old_buf;

	if (_symmon)
		return ;

	last_nasid = 0 ;
        if (setfault(new_buf, &old_buf)) {
                last_nasid = trying_nasid + 1;
        }

        for (trying_nasid = last_nasid; trying_nasid < MAX_NASIDS;
                                        trying_nasid++) {
                volatile __uint64_t     sr0;

                sr0 = REMOTE_HUB_L(trying_nasid, NI_SCRATCH_REG0);
                REMOTE_HUB_S(trying_nasid, NI_SCRATCH_REG0, sr0);

		if (ip27log_getenv(	trying_nasid, 
					DISABLE_NODE_BRD, buf, NULL, 0) < 0)
			continue ;

		if (strncmp("/hw/module/", buf, 11))
			continue ;
		mod  = strtoull(&buf[11], &tmp, 10) ;
		if (!(SN00)) {
			if (!tmp)
				continue ;
			if (strncmp("/slot/n", tmp, 7))
				continue ;
			slot = strtoull(tmp+7, NULL, 10) ;
		}

		dis_node_brd_table[trying_nasid].mod = mod ;
		dis_node_brd_table[trying_nasid].slot = slot ;
	}

        restorefault(old_buf);
}

nasid_t
get_dis_node_brd_nasid(moduleid_t mod, slotid_t slot)
{
	int	i ;

	for (i=0;i<MAX_NASIDS/2;i++)
		if ((dis_node_brd_table[i].mod == mod) &&
		   (dis_node_brd_table[i].slot == slot)) {
			return i ;
		   }
	return INVALID_NASID ;
}

void
hinv_print_dis_node_brds(void)
{
        int     i ;
	char 	slot_name[32] ;

        for (i=0;i<MAX_NASIDS/2;i++)
		if (dis_node_brd_table[i].mod) {
			if (SN00)
				strcpy(slot_name, "MotherBoard") ;
			else
				sprintf(slot_name, "n%d", 
					dis_node_brd_table[i].slot) ;

                	printf(	"IP27 Node Board, Module %d, Slot %s, "
				"(nasid %d) ** disabled\n",
                			dis_node_brd_table[i].mod,
                   			slot_name, i) ;
		}
}

#endif

/* Support for reading extra NICs */

#define MAX_EXTRA_NICS 32
#define NIC_STRING_LEN 1024

struct en_s {
    lboard_t *lb ;
    char     *nic ;
} ;

struct {
    int         cur ;
    struct en_s enic[MAX_EXTRA_NICS] ;
} extra_nic ;

#define ENICP(i)        (&extra_nic.enic[i])
#define NEW_ENICP       (&extra_nic.enic[extra_nic.cur])
#define INCR_ENIC       (extra_nic.cur++)
#define ENIC_LIMIT	(extra_nic.cur >= MAX_EXTRA_NICS)
#define CHECK_IOC3_NIC(l, p)    ((!IS_MIO_IOC3(l, p)) &&                \
                                 ((l->brd_type == KLTYPE_PCI) ||        \
                                  ((l->brd_type == KLTYPE_BASEIO) &&    \
                                   (SN00) && (p > 3))))

void
init_extra_nic(void)
{
    struct en_s *en ;
    int         i ;

    extra_nic.cur = 0 ;
    en = NEW_ENICP ;
    for(i=0;i<MAX_EXTRA_NICS;i++,en++) {
        en->lb  = 0 ;
        en->nic = 0 ;
    }
}

__psunsigned_t 
init_ioc3_for_nic(lboard_t *lb, klinfo_t *k)
{
    __psunsigned_t      base ;

    base = NODE_SWIN_BASE(lb->brd_nasid, k->widid) ;
    base = base + BRIDGE_DEVIO(k->physid) ;
    *(__int32_t *)(base + IOC3_GPCR_S) |= 0x00200000 ;
    return(base+IOC3_MCR) ;
}

/* visit_lboard call back */
int
read_extra_nic_lb(lboard_t *lb, void *ap, void *rp)
{
    struct en_s *en ;
    klinfo_t    *k  = NULL ;
    klhub_t *hubp;
    __psunsigned_t      base ;

    if(!(lb->brd_flags & ENABLE_BOARD) && (k = find_component(lb,k,KLSTRUCT_HUB)))
    {
	hubp = (klhub_t *)k;
	if(hubp->hub_mfg_nic)
	{
	    if (ENIC_LIMIT)
		return 1;
            en = NEW_ENICP ;
            en->nic = malloc(NIC_STRING_LEN) ;
            en->lb  = lb ;
            if (en->nic) {
		strcpy(en->nic,(char *)NODE_OFFSET_TO_K1(lb->brd_nasid,hubp->hub_mfg_nic));
		INCR_ENIC;
	    }
	}
    }
    if (!(k = find_component(lb, k, KLSTRUCT_IOC3)))
        return 1 ;

    do {
        if (CHECK_IOC3_NIC(lb, k->physid)) {
	    if (ENIC_LIMIT)
		break ;
            en = NEW_ENICP ;
            en->nic = malloc(NIC_STRING_LEN) ;
            en->lb  = lb ;
            if (en->nic) {
                base = init_ioc3_for_nic(lb, k) ;
                klcfg_get_nic_info((nic_data_t)base, en->nic) ;
                INCR_ENIC ;
            }
        }
        k = find_component(lb, k, KLSTRUCT_IOC3) ;
    } while(k) ;
    return 1 ;
}

void
read_extra_nic(void)
{
    init_extra_nic() ;
    visit_lboard(read_extra_nic_lb, NULL, NULL) ;
}


void
get_hwg_name(lboard_t *lb, char *s)
{
    char        sname[SLOTNUM_MAXLENGTH];

    get_slotname(lb->brd_slot, sname) ;
    sprintf(s, "/hw/module/%d/slot/%s",
                lb->brd_module, sname) ;
}

void
print_extra_nic(int flags)
{
    struct en_s *en ;
    int         i ;
    char        lname[64];

    en = ENICP(0) ;
    for (i=0; i<extra_nic.cur; i++, en++) {
        get_hwg_name(en->lb, lname) ;
        printf("location: %s\n", lname) ;
        if (flags&HINV_MVV)
            printf("%s\n", en->nic) ;
        else
            prnic_fmt(en->nic) ;
        printf("\n") ;
    }
}

/* End support for reading extra NICs */

#define	GOOD_COMP	0x0
#define	BAD_NODE	0x1
#define	BAD_CPU_A	0x2
#define	BAD_CPU_B	0x4
#define	BAD_MEM		0x8
#define	BAD_ROU		0x10
#define	BAD_HUBII	0x20

/*
 * Function:            dump_scsi_diags -> checks for diagnostic errors on
 *                      specified scsi device
 * Args:                board pointer, scsi component
 * Returns:             -1 on failure, 0 on success
 */
int dump_scsi_diags(lboard_t *brd_ptr, klscsi_t *scsi)
{
    int numpci, pciid;
    unsigned short diag = scsi->scsi_info.diagval;

    if (scsi && diag != KLDIAG_PASSED) {
	printf("\t\t/hw/module/%d/slot/io%d: SCSI failed diag\n",
	       brd_ptr->brd_module, SLOTNUM_GETSLOT(brd_ptr->brd_slot));

	numpci = (sizeof(diag) * NBBY / NUM_SCSI_FAIL_CODES);

	for (pciid=0; pciid<numpci; pciid++) {
	    if (diag & SCSI_MEM_FAIL_MASK)
		printf("\t\t\tReason: pci id %d %s\n",
		       pciid, get_diag_string(KLDIAG_SCSI_SSRAM_FAIL));
	    if (diag & SCSI_DMA_FAIL_MASK)
		printf("\t\t\tReason: pci id %d %s\n",
		       pciid, get_diag_string(KLDIAG_SCSI_DMA_FAIL));
	    if (diag & SCSI_STEST_FAIL_MASK)
		printf("\t\t\tReason: pci id %d %s\n",
		       pciid, get_diag_string(KLDIAG_SCSI_STEST_FAIL));

	    diag >>= NUM_SCSI_FAIL_CODES;
	}
	return -1;
    }
    return 0;
}


/*
 * Function:		sn0_dump_diag -> dumps complete system cfg and diag
 *			results
 * Args:		diag_mode -> DIP switch settings for diag mode
 * Returns:		Nothing
 * Note:		Must be called only after gda, etc. are inited
 */
void sn0_dump_diag(int diag_mode)
{
    int		i, bad, disable, bank, all_disabled;
    int		ncpu = 0, ncpu_dis = 0, mem = 0, mem_dis = 0;
    int		node = 0, node_dis = 0, nrou = 0, nrou_dis = 0;
    int		num_bad_nasid = 0;
    char	bad_comp[MAX_NASIDS];
    lboard_t	*brd_ptr;
    klcpu_t	*cpu;
    klhub_t	*hub;
    klmembnk_t	*membank;
    klrou_t	*rou;
    klioc3_t	*ioc3;
    klscsi_t	*scsi;
    gda_t       *gdap;
    char	mem_disable[MD_MEM_BANKS + 1], disable_sz[MD_MEM_BANKS + 1];

    gdap = (gda_t *)GDA_ADDR(get_nasid());

    printf("\n**** System Configuration and Diagnostics Summary ****\n");

    for (i = 0; i < MAX_NASIDS; i++)
       bad_comp[i] = GOOD_COMP;

    for (i = 0; i < MAX_COMPACT_NODES; i++) {
        nasid_t		d_nasid;

        if (gdap->g_nasidtable[i] == INVALID_CNODEID)
            continue;

        d_nasid = gdap->g_nasidtable[i];

        brd_ptr = (lboard_t *) find_lboard((lboard_t *)KL_CONFIG_INFO(d_nasid),
			KLTYPE_IP27);
        if (!brd_ptr) {
            printf("\tWARNING: Could not find brd_ptr for nasid = %d\n", 
			d_nasid);
            continue;
        }

        if (brd_ptr->brd_flags & DUPLICATE_BOARD)
            continue;

        if ((!(brd_ptr->brd_flags & ENABLE_BOARD)) || 
			(brd_ptr->brd_flags & FAILED_BOARD)) {
            bad_comp[d_nasid] |= BAD_NODE;
            node_dis++;
        }
        else
            node++;

        hub = (klhub_t *) find_first_component(brd_ptr, KLSTRUCT_HUB);

        if (hub->hub_info.diagval != KLDIAG_PASSED)
		bad_comp[d_nasid] |= BAD_HUBII;

        cpu = (klcpu_t *) find_component(brd_ptr, (klinfo_t *) NULL, 
			KLSTRUCT_CPU);

        if (!cpu) {
            printf("\tWARNING: Could not find a single CPU on nasid = %d\n",
			d_nasid);
            continue;
        }

        if ((!(cpu->cpu_info.flags & KLINFO_ENABLE)) ||
			(cpu->cpu_info.flags & KLINFO_FAILED)) {
            bad_comp[d_nasid] |= (cpu->cpu_info.physid ? 
			BAD_CPU_B : BAD_CPU_A);
            ncpu_dis++;
        }
        else
            ncpu++;

        cpu = (klcpu_t *) find_component(brd_ptr, (klinfo_t *) cpu,
			KLSTRUCT_CPU);

        if (cpu) {
            if ((!(cpu->cpu_info.flags & KLINFO_ENABLE)) ||
			(cpu->cpu_info.flags & KLINFO_FAILED)) {
                bad_comp[d_nasid] |= (cpu->cpu_info.physid ? 
			BAD_CPU_B : BAD_CPU_A);
                ncpu_dis++;
            }
            else
                ncpu++;
        }

        membank = (klmembnk_t *) find_component(brd_ptr, (klinfo_t *) NULL,
			KLSTRUCT_MEMBNK);

        disable = (ip27log_getenv(d_nasid, "DisableMemMask", mem_disable, 0, 0)
		>= 0);

        if (disable) {
	    ip27log_getenv(d_nasid, "DisableMemSz", disable_sz, 0, 0);
	    bad_comp[d_nasid] |= BAD_MEM;
	}

        if (membank) {
            if ((!(membank->membnk_info.flags & KLINFO_ENABLE)) ||
			(membank->membnk_info.flags & KLINFO_FAILED)) {
                bad_comp[d_nasid] |= BAD_MEM;
                mem_dis += membank->membnk_memsz;
            }
            else
                mem += membank->membnk_memsz;
        }
        else
            printf("\tNo Memory found on nasid = %d\n", d_nasid);

        if (disable) {
	    /*if(membank)
		membank->membnk_info.flags &= ~KLINFO_ENABLE;*/
	    for (bank = 0; bank < MD_MEM_BANKS; bank++)
		/* If DIP_NODISABLE is active, the bank will not actually
		 * be disabled, so don't count the memory as disabled. */
	        if (strchr(mem_disable, '0' + bank) &&
		    membank->membnk_bnksz[bank] == 0)
		{
	            mem_dis += MD_SIZE_MBYTES(disable_sz[bank] - '0');
		   /* membank->membnk_info.flags |= 0x1 << (bank + 8);*/
		}	 
	}
	brd_ptr = KLCF_NEXT(brd_ptr);
	
	while((brd_ptr) && (brd_ptr = find_lboard(brd_ptr,KLTYPE_IP27)))
	{
		int nasid;
        	hub = (klhub_t *) find_first_component(brd_ptr, KLSTRUCT_HUB);
		if(hub)
			nasid = ((klinfo_t *)hub)->physid;
		else
			continue;
        	membank = (klmembnk_t *) find_component(brd_ptr, (klinfo_t *) NULL,
			KLSTRUCT_MEMBNK);

        	disable = (ip27log_getenv(nasid, "DisableMemMask", mem_disable, 0, 0)
		>= 0);

        	if (disable) {
	    	ip27log_getenv(nasid, "DisableMemSz", disable_sz, 0, 0);
		}

        	if (membank) {
            	if ((!(membank->membnk_info.flags & KLINFO_ENABLE)) ||
			(membank->membnk_info.flags & KLINFO_FAILED)) {
                	mem_dis += membank->membnk_memsz;
            	}
            	else
                mem += membank->membnk_memsz;
        	}
        	else
            	printf("\tNo Memory found on nasid = %d\n", d_nasid);

        	if (disable) {
		/*if(membank)
			membank->membnk_info.flags &= ~KLINFO_ENABLE;*/

		/* If DIP_NODISABLE is active, the bank will not actually
		 * be disabled, so don't count the memory as disabled. */
	    	for (bank = 0; bank < MD_MEM_BANKS; bank++)
	        	if (strchr(mem_disable, '0' + bank) &&
			    membank->membnk_bnksz[bank] == 0)
			{
	            	mem_dis += MD_SIZE_MBYTES(disable_sz[bank] - '0');
		
			}
		}
		brd_ptr = KLCF_NEXT(brd_ptr);
	}
        brd_ptr = (lboard_t *) find_lboard((lboard_t *) KL_CONFIG_INFO(d_nasid),
			KLTYPE_ROUTER);

        if (!brd_ptr || (brd_ptr->brd_flags & DUPLICATE_BOARD))
            continue;

        if ((!(brd_ptr->brd_flags & ENABLE_BOARD)) || 
			(brd_ptr->brd_flags & FAILED_BOARD)) {
            bad_comp[d_nasid] |= BAD_ROU;
            nrou_dis++;
            continue;
        }

        rou = (klrou_t *) find_component(brd_ptr, (klinfo_t *) NULL,
			 KLSTRUCT_ROU);

        if (rou) {
            if ((!(rou->rou_info.flags & KLINFO_ENABLE)) ||
			(rou->rou_info.flags & KLINFO_FAILED)) {
                bad_comp[d_nasid] |= BAD_ROU;
                nrou_dis++;
            }
            else
                nrou++;
        }

        brd_ptr = (lboard_t *) find_lboard((lboard_t *) KL_CONFIG_INFO(d_nasid),
			KLTYPE_META_ROUTER);

        if (!brd_ptr || (brd_ptr->brd_flags & DUPLICATE_BOARD))
            continue;

        if ((!(brd_ptr->brd_flags & ENABLE_BOARD)) || 
			(brd_ptr->brd_flags & FAILED_BOARD)) {
            bad_comp[d_nasid] |= BAD_ROU;
            nrou_dis++;
            continue;
        }

        rou = (klrou_t *) find_component(brd_ptr, (klinfo_t *) NULL,
			 KLSTRUCT_ROU);

        if (rou) {
            if ((!(rou->rou_info.flags & KLINFO_ENABLE)) ||
			(rou->rou_info.flags & KLINFO_FAILED)) {
                bad_comp[d_nasid] |= BAD_ROU;
                nrou_dis++;
            }
            else
                nrou++;
        }
    }

    printf("CONFIG:\n");

    printf("\t\tNo. of NODEs enabled \t= %d\n", node);
    printf("\t\tNo. of NODEs disabled \t= %d\n", node_dis);
    printf("\t\tNo. of CPUs enabled \t= %d\n", ncpu);
    printf("\t\tNo. of CPUs disabled \t= %d\n", ncpu_dis);
    printf("\t\tMem enabled \t\t= %d MB\n", mem);
    printf("\t\tMem disabled \t\t= %d MB\n", mem_dis);
    printf("\t\tNo. of RTRs enabled \t= %d\n", nrou);
    printf("\t\tNo. of RTRs disabled \t= %d\n", nrou_dis);

    printf("\nDIAG RESULTS:\n");

    if (GET_DIAG_MODE(diag_mode) == DIAG_MODE_NONE) {
        printf("\t\tNO DIAGS WERE RUN!\n");
        goto done;
    }

    bad = 0;

    for (i = 0; i < MAX_NASIDS; i++) {
        if (bad_comp[i] == GOOD_COMP)
            continue;

        brd_ptr = find_lboard((lboard_t *) KL_CONFIG_INFO(i),
			KLTYPE_IP27);

        if (bad_comp[i] & BAD_NODE) {
            bad = 1;
            printf("\t\t/hw/module/%d/slot/n%d: NODE BOARD disabled\n", 
			brd_ptr->brd_module, SLOTNUM_GETSLOT(brd_ptr->brd_slot));
            printf("\t\t\tReason: %s\n", get_diag_string(brd_ptr->brd_diagval));
        }

        if (bad_comp[i] & BAD_HUBII) {
	    bad = 1;
	    hub = (klhub_t *) find_first_component(brd_ptr, KLSTRUCT_HUB);
            printf("\t\t/hw/module/%d/slot/n%d: HUB IO down\n",
			brd_ptr->brd_module, SLOTNUM_GETSLOT(brd_ptr->brd_slot));
            printf("\t\t\tReason: %s\n", get_diag_string(hub->hub_info.diagval));
	}

        if (bad_comp[i] & BAD_CPU_A) {
            bad = 1;
            cpu = (klcpu_t *) find_component(brd_ptr, (klinfo_t *) NULL, 
			KLSTRUCT_CPU);
            printf("\t\t/hw/module/%d/slot/n%d/node/cpu/%d: CPU A disabled\n",
		brd_ptr->brd_module, SLOTNUM_GETSLOT(brd_ptr->brd_slot), cpu->cpu_info.physid);
            printf("\t\t\tReason: %s\n", get_diag_string(cpu->cpu_info.diagval));
        }

        if (bad_comp[i] & BAD_CPU_B) {
            bad = 1;
            cpu = (klcpu_t *) find_component(brd_ptr, (klinfo_t *) NULL, 
			KLSTRUCT_CPU);
            cpu = (klcpu_t *) find_component(brd_ptr, (klinfo_t *) cpu, 
			KLSTRUCT_CPU);
            printf("\t\t/hw/module/%d/slot/n%d/node/cpu/%d: CPU B disabled\n",
		brd_ptr->brd_module, SLOTNUM_GETSLOT(brd_ptr->brd_slot), cpu->cpu_info.physid);
            printf("\t\t\tReason: %s\n", get_diag_string(cpu->cpu_info.diagval));
        }

        if (bad_comp[i] & BAD_MEM) {
            char	disable_bank[32], disable_reason[32];
	    int		j, found = 0;

            bad = 1;
            membank = (klmembnk_t *) find_component(brd_ptr, (klinfo_t *) 
			NULL, KLSTRUCT_MEMBNK);

            if (ip27log_getenv(i, "DisableMemMask", disable_bank, 0, 0) < 0)
                disable_bank[0] = 0;
	    else {
		char	tmp_buf[32];
		int	n = 0;

		/* 
		 * loop through disabled banks.  If DIP_NODISABLE is
		 * turned on, we don't want to record the bank as
		 * disabled, regardless of what the promlog variable says.
		 * Check this by checking the size - if DIP_NODISABLE is
		 * set, mdir_disable_bank() is not called, so the size is
		 * not set to 0.
		 */
		for (j = 0; j < strlen(disable_bank); j++) {
		    if(!membank->membnk_bnksz[disable_bank[j] - '0']) 
		   {
		    found = 1;
		    tmp_buf[n++] = disable_bank[j];
		    tmp_buf[n++] = ' ';
		   }
		}

		tmp_buf[n] = 0;
		strcpy(disable_bank, tmp_buf);
	    }
	    if(found) {
               printf("\t\t/hw/module/%d/slot/n%d/node/mem: "
		   "MEMBANK(S) %s disabled\n", brd_ptr->brd_module, 
		   SLOTNUM_GETSLOT(brd_ptr->brd_slot), disable_bank);

	       ip27log_getenv(i, "MemDisReason", disable_reason, "00000000",
			0);

	       printf("\t\t\tReason:\n");

	       for (j = 0; j < MD_MEM_BANKS; j++) {
		   if (strchr(disable_bank, '0' + j)) {
                       if (disable_reason[j] == '0')
			   printf("\t\t\t\tBank %d: Unknown\n", j);
		       else if (disable_reason[j] == '0' + MEM_DISABLE_USR)
			   printf("\t\t\t\tBank %d: %s\n", j, 
				get_diag_string(KLDIAG_MEMORY_DISABLED));
		       else if (disable_reason[j] == '0' + MEM_DISABLE_AUTO)
			   printf("\t\t\t\tBank %d: %s\n", j, 
				get_diag_string(KLDIAG_MEMTEST_FAILED));
		       else if (disable_reason[j] == '0' + MEM_DISABLE_256MB)
			   printf("\t\t\t\tBank %d: %s\n", j, 
				get_diag_string(KLDIAG_IP27_256MB_BANK));
                   }
	       }
	    }
        }

        if (bad_comp[i] & BAD_ROU) {
            brd_ptr = (lboard_t *) find_lboard((lboard_t *) KL_CONFIG_INFO(i),
			KLTYPE_ROUTER);

            while (brd_ptr) {
                if (!(brd_ptr->brd_flags & DUPLICATE_BOARD)) {
                    rou = (klrou_t *) find_first_component(brd_ptr, 
				KLSTRUCT_ROU);

                    if (!(brd_ptr->brd_flags & ENABLE_BOARD) || 
	    			(brd_ptr->brd_flags & FAILED_BOARD) ||
				!(rou->rou_info.flags & KLINFO_ENABLE) ||
				(rou->rou_info.flags & KLINFO_FAILED)) {
                        printf("\t\t/hw/module/%d/router/r%d: ROUTER disabled"
				"\n", brd_ptr->brd_module, 
				SLOTNUM_GETSLOT(brd_ptr->brd_slot));

                        if (!(brd_ptr->brd_flags & ENABLE_BOARD) ||
				(brd_ptr->brd_flags & FAILED_BOARD))
                            printf("\t\t\tReason: %s\n", 
				get_diag_string(brd_ptr->brd_diagval));
                        else
                            printf("\t\t\tReason: %s\n", 
				get_diag_string(rou->rou_info.diagval));
                    }
                }

                brd_ptr = (lboard_t *) find_lboard(brd_ptr, KLTYPE_ROUTER);
            }

            brd_ptr = (lboard_t *) find_lboard((lboard_t *) KL_CONFIG_INFO(i),
			KLTYPE_META_ROUTER);

            while (brd_ptr) {
                if (!(brd_ptr->brd_flags & DUPLICATE_BOARD)) {
                    rou = (klrou_t *) find_first_component(brd_ptr, 
				KLSTRUCT_ROU);

                    if (!(brd_ptr->brd_flags & ENABLE_BOARD) || 
	    			(brd_ptr->brd_flags & FAILED_BOARD) ||
				!(rou->rou_info.flags & KLINFO_ENABLE) ||
				(rou->rou_info.flags & KLINFO_FAILED)) {
                        printf("\t\t/hw/module/%d/router/r%d: ROUTER disabled"
				"\n", brd_ptr->brd_module, 
				SLOTNUM_GETSLOT(brd_ptr->brd_slot));

                        if (!(brd_ptr->brd_flags & ENABLE_BOARD) ||
				(brd_ptr->brd_flags & FAILED_BOARD))
                            printf("\t\t\tReason: %s\n", 
				get_diag_string(brd_ptr->brd_diagval));
                        else
                            printf("\t\t\tReason: %s\n", 
				get_diag_string(rou->rou_info.diagval));
                    }
                }

                brd_ptr = (lboard_t *) find_lboard(brd_ptr, KLTYPE_META_ROUTER);
            }
        }
    }

    /* Do serial_dma, ql & enet summary here */
    for (i = 0; i < MAX_COMPACT_NODES; i++) {
        nasid_t		d_nasid;

        d_nasid = gdap->g_nasidtable[i];

        if (d_nasid == INVALID_CNODEID)
            continue;

        brd_ptr = (lboard_t *) find_lboard((lboard_t *) KL_CONFIG_INFO(d_nasid),
			KLTYPE_WEIRDIO);


	if(brd_ptr)
        if ((!(brd_ptr->brd_flags & ENABLE_BOARD)) || 
			(brd_ptr->brd_flags & FAILED_BOARD)) {
            printf("\t\t/hw/module/%d/slot/io%d: IO link bad\n",
		brd_ptr->brd_module, SLOTNUM_GETSLOT(brd_ptr->brd_slot));
            bad = 1;
            continue;
        }

        brd_ptr = (lboard_t *) find_lboard((lboard_t *) KL_CONFIG_INFO(d_nasid),
			KLTYPE_BASEIO);

        if (!brd_ptr || (brd_ptr->brd_flags & DUPLICATE_BOARD))
            continue;

        if ((!(brd_ptr->brd_flags & ENABLE_BOARD)) || 
			(brd_ptr->brd_flags & FAILED_BOARD)) {
            printf("\t\t/hw/module/%d/slot/io%d: BASEIO disabled\n",
		brd_ptr->brd_module, SLOTNUM_GETSLOT(brd_ptr->brd_slot));
            printf("\t\t\tReason: %s\n", get_diag_string(brd_ptr->brd_diagval));
            bad = 1;
            continue;
        }
        
        ioc3 = (klioc3_t *) find_component(brd_ptr, (klinfo_t *) NULL,
			 KLSTRUCT_IOC3);

        if (ioc3 && ioc3->ioc3_info.diagval != KLDIAG_PASSED) {
            printf("\t\t/hw/module/%d/slot/io%d: IOC3 failed diag\n",
		brd_ptr->brd_module, SLOTNUM_GETSLOT(brd_ptr->brd_slot));
            printf("\t\t\tReason: %s\n", get_diag_string(ioc3->ioc3_info.diagval));
            bad = 1;
        }

        if (ioc3 && ioc3->ioc3_enet.diagval != KLDIAG_PASSED) {
            printf("\t\t/hw/module/%d/slot/io%d: IOC3 ENET failed diag\n",
		brd_ptr->brd_module, SLOTNUM_GETSLOT(brd_ptr->brd_slot));
            printf("\t\t\tReason: %s\n", get_diag_string(ioc3->ioc3_enet.diagval));
            bad = 1;
        }

	scsi = (klscsi_t *) find_component(brd_ptr, (klinfo_t *) NULL,
					   KLSTRUCT_SCSI);
	if (dump_scsi_diags(brd_ptr, scsi) < 0)
	    bad = 1;

	scsi = (klscsi_t *) find_component(brd_ptr, (klinfo_t *) scsi,
					   KLSTRUCT_SCSI);
	if (dump_scsi_diags(brd_ptr, scsi) < 0)
	    bad = 1;


	brd_ptr = (lboard_t *) find_lboard((lboard_t *) KL_CONFIG_INFO(d_nasid),
					   KLTYPE_MSCSI);

	if ((!brd_ptr) || (brd_ptr->brd_flags & DUPLICATE_BOARD))
	    continue;

	if ((!(brd_ptr->brd_flags & ENABLE_BOARD)) ||
	    (brd_ptr->brd_flags & FAILED_BOARD)) {
	    printf("\t\t/hw/module/%d/slot/io%d: MSCSI disabled\n",
		   brd_ptr->brd_module, SLOTNUM_GETSLOT(brd_ptr->brd_slot));
	    printf("\t\t\tReason: %s\n", get_diag_string(brd_ptr->brd_diagval));
	    bad = 1;
	    continue;
	}

	ioc3 = (klioc3_t *) find_component(brd_ptr, (klinfo_t *) NULL,
					   KLSTRUCT_IOC3);

	if (ioc3 && ioc3->ioc3_info.diagval != KLDIAG_PASSED) {
	    printf("\t\t/hw/module/%d/slot/io%d: IOC3 failed diag\n",
		   brd_ptr->brd_module, SLOTNUM_GETSLOT(brd_ptr->brd_slot));
	    printf("\t\t\tReason: %s\n",
		   get_diag_string(ioc3->ioc3_info.diagval));
	    bad = 1;
	}

	if (ioc3 && ioc3->ioc3_enet.diagval != KLDIAG_PASSED) {
	    printf("\t\t/hw/module/%d/slot/io%d: IOC3 ENET failed diag\n",
		   brd_ptr->brd_module, SLOTNUM_GETSLOT(brd_ptr->brd_slot));
	    printf("\t\t\tReason: %s\n",
		   get_diag_string(ioc3->ioc3_enet.diagval));
	    bad = 1;
	}

	scsi = (klscsi_t *) find_component(brd_ptr, (klinfo_t *) NULL,
					   KLSTRUCT_SCSI);
	if (dump_scsi_diags(brd_ptr, scsi) < 0)
	    bad = 1;

	while ((scsi = (klscsi_t *) find_component(brd_ptr, (klinfo_t *) scsi,
					   KLSTRUCT_SCSI)) != NULL)
	    if (dump_scsi_diags(brd_ptr, scsi) < 0)
		bad = 1;
    }


    if (!bad)
        printf("\t\tALL DIAGS PASSED.\n");

done:

    /*
     * Check if all CPUs were disabled!
     */

    all_disabled = 1;

    for (i = 0; i < MAX_COMPACT_NODES; i++) {
        nasid_t		nasid;

        if ((nasid = gdap->g_nasidtable[i]) == INVALID_CNODEID)
            continue;

        if ((ip27log_getenv(nasid, "DisableA", 0, 0, 0) < 0) ||
		(ip27log_getenv(nasid, "DisableB", 0, 0, 0) < 0)) {
            all_disabled = 0;
            break;
        }
    }

    if (all_disabled)
        printf("\t\t\tWARNING: All CPUs disabled. NODISABLE DIP switch (9)\n"
		"\t\t\t\toverrode disabling.\n");

    printf("**** End System Configuration and Diagnostics Summary ****\n");
}


char* find_board_name(lboard_t *l)
{
#if 0
     if(l->brd_type == KLTYPE_IP27)
     {
	if(strstr(l->brd_name, "IP"))
		return l->brd_name;
     }
#endif
     return ((char *)BOARD_NAME(l->brd_type));
}

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

void 
swap_some_memory_banks()
{
    __uint64_t          mc0, mc1; 
    int			bank, tmpsz, i;
    volatile __uint64_t mc;
    lboard_t *          lb;
    klmembnk_t *        mem;
    nasid_t             nasid;
    gda_t *		gdap;
    cnodeid_t		cnode;
    unchar junk;

    /* 
     * This nvram variable is treated a bit strangely.  In general, any
     * new IO prom should set this variable to 'y' because the new IOprom
     * has the capability of swapping all banks (the correct behavior).
     * However, if for some reason the proms must be flashed downrev, from
     * a new version which sets this nvram variable to an older version
     * which doesn't set it correctly, the nvram variable will still be
     * set to 'y'.  This will cause the kernel to assume the IO prom has
     * set up the banks in the new formation, and will cause a hang.
     *
     * To avoid this (admittedly pathological) case, we set the value of
     * the nvram variable to 0 here and only change it to 'y' if any set
     * of banks is actually swapped.
     */

    cpu_set_nvram_offset(NVOFF_SWAP_ALL_BANKS, 1, "");

    gdap = (gda_t *)GDA_ADDR(get_nasid());
    if (gdap->g_magic != GDA_MAGIC) {
	printf("swap_some_memory_banks: Invalid GDA MAGIC\n");
	return;
    }

    /*
     * loop through all boards in the system - this routine is only called
     * by the global master (called in IO6prom)
     */

    for (cnode = 0; cnode < MAX_COMPACT_NODES; cnode ++) {
	nasid = gdap->g_nasidtable[cnode];
	if (nasid == INVALID_NASID)
	    continue;

	lb = (lboard_t *)find_lboard((lboard_t *)KL_CONFIG_INFO(nasid),
				     KLTYPE_IP27);

	while (lb) {
	    /* Only swap more banks if banks 0 and 1 are swapped */
	    mc = REMOTE_HUB_L(nasid, MD_MEMORY_CONFIG);
	    if ((mc & MMC_DIMM0_SEL_MASK) >> MMC_DIMM0_SEL_SHFT) {
		/* tell the kernel that we are swapping all pairs */
		cpu_set_nvram_offset(NVOFF_SWAP_ALL_BANKS, 1, "y");

		/* 
		 * At this point, banks 0 and 1 are still swapped.  Just
		 * re-swap the rest.
		 */

		for (bank = 2; bank < MD_MEM_BANKS; bank += 2) {
		    mc0 = (mc & MMC_BANK_MASK(bank)) >> MMC_BANK_SHFT(bank);
		    mc1 = (mc & MMC_BANK_MASK(bank+1)) >> MMC_BANK_SHFT(bank+1);

		    mc &= ~(MMC_BANK_MASK(bank));
		    mc &= ~(MMC_BANK_MASK(bank+1));

		    mc |= (mc0 << MMC_BANK_SHFT(bank+1));
		    mc |= (mc1 << MMC_BANK_SHFT(bank));
		}
		

		REMOTE_HUB_S(nasid, MD_MEMORY_CONFIG, mc);	

		/* 
		 * Also swap the relevant klconfig info.  The old
		 * behavior is for klconfig to hold data exactly in
		 * sync with the register - i.e.  membnk_bnksz[0]
		 * holds the size of whatever bank is in location 0 of
		 * the MD_MEMORY_CONFIG register (the size of
		 * locational bank 0 if dimm0_sel is not set (no swap
		 * case) or size of locational bank 1 if dimm0_sel is
		 * set (swap case)).  Note that the remaining banks (2
		 * and up) will automatically be in locational form
		 * because, in the old case, those were never swapped.
		 * 
		 * The new behavior must have _all_ bank information in
		 * "locational" form.  In order to create the new
		 * behavior, banks 0 and 1 must be swapped back into
		 * "locational" form.
		 */

		if (mem = (klmembnk_t *)find_component(lb, NULL, 
						       KLSTRUCT_MEMBNK)) {
		    tmpsz = mem->membnk_bnksz[0];
		    mem->membnk_bnksz[0] = mem->membnk_bnksz[1];
		    mem->membnk_bnksz[1] = tmpsz;
		}
	    }

	    lb = KLCF_NEXT(lb);
	    if (lb)
		lb = find_lboard(lb, KLTYPE_IP27);
	}
    }
}

#if 0
/*
 * Propagates each module's serial number to all
 * MIDPLANE boards.
 */

extern int num_modules ;
extern struct mod_tab   module_table[] ;

int
propagate_serial_num()
{
    int		    i ;
    cnodeid_t       cnode ;
    nasid_t         nasid ;
    lboard_t        *lbptr ;
    gda_t           *gdap;
    klmod_serial_num_t *snump ;
    char 	    snum[MAX_SERIAL_NUM_SIZE] ;

    if (SN00)
	return 1 ;

    gdap = (gda_t *)GDA_ADDR(get_nasid());
    if (gdap->g_magic != GDA_MAGIC) {
        printf("fix_serial_num: Invalid GDA MAGIC\n") ;
        return 0 ;
    }

    /* For all the modules */
    for (i = 1; i <= num_modules; i++) {
	/* Not found a serial number yet */
	*snum = 0 ;
	/* For all the nodes in the system */
        for (cnode = 0; cnode < MAX_COMPACT_NODES; cnode ++) {
            nasid = gdap->g_nasidtable[cnode];
            if (nasid == INVALID_NASID)
                continue;

            lbptr = (lboard_t *)KL_CONFIG_INFO(nasid) ;
	    /* Is it our module? */
	    if (lbptr->brd_module != module_table[i].module_id)
	        continue ;

            if (lbptr = find_lboard(lbptr, KLTYPE_MIDPLANE)) {
		if (snump = GET_SNUM_COMP(lbptr)) {
		    if (snump->snum.snum_str[0]) {
			strcpy(snum, snump->snum.snum_str) ;
printf("Found serial number for module %d, %s\n", module_table[i].module_id, snum) ;
			break ;
		    }
		}
            }
        } /* for cnode */

	/* No serial num for this module */
	if (!(*snum))
	    continue ;

	/* Update the serial number found above to all the XBOWs 
           in our module */

        for (cnode = 0; cnode < MAX_COMPACT_NODES; cnode ++) {
            nasid = gdap->g_nasidtable[cnode];
            if (nasid == INVALID_NASID)
                continue;

            lbptr = (lboard_t *)KL_CONFIG_INFO(nasid) ;
            if (lbptr->brd_module != module_table[i].module_id)
                continue ;

            if (lbptr = find_lboard(lbptr, KLTYPE_MIDPLANE)) {
                if (snump = GET_SNUM_COMP(lbptr)) {
                    if (!snump->snum.snum_str[0]) {
			if (*snum) {
                            strcpy(snump->snum.snum_str, snum) ;
printf("Copied serial number %s in module %d\n", snum,  module_table[i].module_id) ;
			}
		    }
                }
            }
        } /* for cnode */

    } /* for all modules */

    return 1 ;
}
#endif
