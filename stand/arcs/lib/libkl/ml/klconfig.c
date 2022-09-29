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

/*
 * File: klconfig.c
 * klconfig.c - Routines to init the KLCONFIG struct for each
 *              type of board.
 *
 *
 * Runs in MP mode, on all processors. These routines write in their
 * local memory areas.
 */

#include <sys/types.h>
#include <sys/mips_addrspace.h>
#include <sys/graph.h>
#include <sys/nic.h>
#include <sys/SN/addrs.h>
#include <sys/SN/promcfg.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/SN0/klhwinit.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/error.h>
#include <sys/SN/xbow.h>
#include <sys/SN/nvram.h>
#include <sys/SN/slotnum.h>
#include <sys/SN/kldiag.h>
#include <sys/SN/router.h>
#include <sys/PCI/bridge.h>
#include <arcs/cfgdata.h>
#include <libsc.h>
#include <libkl.h>
#include <libsk.h>
#include <report.h>
#include <prom_msgs.h>
#include <klcfg_router.h>
#include <prom_leds.h>
#include <sys/sbd.h>

#ifndef SABLE
#define LDEBUG		0		/* Set for 1 for debug compilation */
#endif

#if defined(LD)
#undef LD
#endif

#define	LD(x)		(*(volatile __int64_t  *) (x))
#define	LW(x)		(*(volatile __int32_t  *) (x))

/* These are defined in different places in the IO6prom and IP27prom. */
extern int get_cpu_irr(void);
extern int get_fpc_irr(void);

extern off_t kl_config_info_alloc(nasid_t, int, unsigned int);
extern void init_config_malloc(nasid_t);
extern int config_find_nic_router_all(pcfg_t *, nic_t, lboard_t **,
                                        klrou_t **, partid_t, int) ;

/*
 * Local function prototype declarations.
 */
int klconfig_port_update(pcfg_t *, partid_t);
lboard_t *kl_config_alloc_board(nasid_t);
klinfo_t *kl_config_alloc_component(nasid_t, lboard_t *);
lboard_t *kl_config_add_board(nasid_t, lboard_t *);
static lboard_t * init_klcfg_routerb_hl(pcfg_t 		*p		,
		   			nasid_t 	nasid		,
		   			pcfg_router_t 	*pcfg_routerp	,
		   			int 		router_type	,
					partid_t	partition) ;

int setup_hub_port(pcfg_hub_t *, pcfg_router_t *);
int setup_router_port_to_hub(pcfg_t *, pcfg_router_t *, pcfg_hub_t *, int);
int setup_router_port_to_router(pcfg_t *, pcfg_router_t *, pcfg_router_t *,int);

extern int get_hub_nic_info(nic_data_t, char *) ;

#define GET_PCI_VID(_id)	(_id & 0xffff)
#define GET_PCI_DID(_id)	((_id >> 16) & 0xffff)

void
init_klcfg(nasid_t nasid)
{
	init_config_malloc(nasid);
}


/* BOARDS */

/* IP27 and other NODE Boards */

/*
 * Init a klcfg struct for a ip27 board.
 * Fill all routine fields as usual. Get hardware info
 * when required by calling respective hardware routines.
 */

lboard_t *
init_klcfg_ip27(__psunsigned_t hub_base, int flag)
{
	lboard_t *brd_ptr;
	nasid_t nasid;
	short cpunum;
	klhub_t *hubp;
	char *c = NULL;

	nasid = NASID_GET(hub_base);

	if ((brd_ptr = kl_config_alloc_board(nasid)) == NULL) {
		printf("init_klcfg_ip27: Cannot allocate board struct\n");
		return NULL;
	}

	/* Since there is only one module in the 12P 4IO config
	 * hardwire the brd module number to 1 
	 */
	if (CONFIG_12P4I)
		brd_ptr->brd_module = 1;       
	brd_ptr->brd_type = KLTYPE_IP27;
	/* 
	 * We're going from 2 to 3 to accomadate the addition of
	 * the CRB_B registers to the hubio error struct
	 */
	if (brd_ptr->brd_sversion <= 2)
		brd_ptr->brd_sversion = 3;
	brd_ptr->brd_brevision = (unsigned char)
	    ((LD(hub_base + NI_STATUS_REV_ID) & NSRI_REV_MASK) >>
	     NSRI_REV_SHFT);

	if (kl_sable) {
#ifdef SABLE
	    brd_ptr->brd_slot = (uchar)get_my_slotid() ;
#endif
	}
	else  {
		char		buf[8];

		brd_ptr->brd_slot = (unsigned char)
		hub_slotbits_to_slot(LD(hub_base + MD_SLOTID_USTAT) & 
							MSU_SLOTID_MASK);
	}

	/*
	 * Who am I?
	 */
	cpunum = LD(hub_base + PI_CPU_NUM);

	/*
	 * setup a cpu component for each cpu found. This should match
	 * what the ip27 prom has already found...
	 */
	if (LD(hub_base + PI_CPU_PRESENT_A))
	    init_klcfg_cpu(hub_base, brd_ptr, IP27_CPU0_INDEX, (cpunum == 0));

	if (LD(hub_base + PI_CPU_PRESENT_B))
	    init_klcfg_cpu(hub_base, brd_ptr, IP27_CPU1_INDEX, (cpunum == 1));

	init_klcfg_hub(hub_base, brd_ptr, IP27_HUB_INDEX) ;
	init_klcfg_membnk(hub_base, brd_ptr, IP27_MEM_INDEX) ;
	if (hubp = (klhub_t *)find_component(brd_ptr, NULL,
				KLSTRUCT_HUB)) {
	    	    if (hubp->hub_mfg_nic)
			c = (char *)NODE_OFFSET_TO_K1(
				nasid,
				hubp->hub_mfg_nic);
			}
	if(ip31_pimm_psc(nasid,NULL,c) == 0)
		strcpy(brd_ptr->brd_name,"IP31");
	else
		strcpy(brd_ptr->brd_name,"IP27");

	return (kl_config_add_board(nasid, brd_ptr));
}

/* NODE BOARD COMPONENTS */

void
add_klcfg_cpuinfo(nasid_t nasid, klcpu_t *cpuptr)
{
	int divisor;
	/* Get frequency from the prom config bits.  Shift it into megahertz. */
	cpuptr->cpu_speed = (short)((((ip27config_t *)(IP27CONFIG_ADDR_NODE(nasid)))->freq_cpu)/1000000);
	/* Get the scache size in bytes (from config reg), shift it to MB */
	cpuptr->cpu_scachesz = ((0x1 << (((((ip27config_t *)(IP27CONFIG_ADDR_NODE(nasid)))->r10k_mode)>>16) & 0x7))>> 1);
	divisor = ((((ip27config_t *)(IP27CONFIG_ADDR_NODE(nasid)))->r10k_mode)>>19) & 0x3;
	cpuptr->cpu_scachespeed = (cpuptr->cpu_speed * 2)/(divisor + 1);
#define MAX_T5_SPEED 	250
	if( nasid == get_nasid()) {
		cpuptr->cpu_prid = get_cpu_irr();
		cpuptr->cpu_fpirr = get_fpc_irr();
	} else { /* The cpu's are both disabled ...  headless node */
		if(cpuptr->cpu_speed > MAX_T5_SPEED) {
			cpuptr->cpu_prid = C0_IMP_R12000 << 8;
			cpuptr->cpu_fpirr = C0_IMP_R12000 << 8;
		} else {
			cpuptr->cpu_prid = C0_IMP_R10000 << 8;
			cpuptr->cpu_fpirr = C0_IMP_R10000 << 8;
		}
	}
	cpuptr->cpu_info.struct_version = CPU_STRUCT_VERSION ;

}

/*
 * Allow slave processors to add infromation to their klconfig structures.
 */
int
update_klcfg_cpuinfo(nasid_t nasid, int slice)
{
	lboard_t *brd;
	klcpu_t *acpu;

	if (nasid == INVALID_NASID)
		nasid = get_nasid();

		if (!(brd = find_lboard((lboard_t *)KL_CONFIG_INFO(nasid),
				   KLTYPE_IP27))) {

			printf("update_klcfg_cpuinfo: Can't find IP27 on nasid %d\n",
				nasid);
			return -1;
		}

		if (!(acpu = (klcpu_t *)find_first_component(brd, KLSTRUCT_CPU))) {
			printf("update_klcfg_cpuinfo: Can't find any CPU structures\n");
			return -1;
		}

		do {
			if ((acpu->cpu_info.physid) == slice) {
				add_klcfg_cpuinfo(nasid, acpu);
				return 0;
			}
		} while (acpu = (klcpu_t *)find_component(brd,
							  (klinfo_t *)acpu,
							  KLSTRUCT_CPU));

		printf("update_klcfg_cpuinfo: Couldn't find my structure.\n");

		return -1;
}

void
init_klcfg_cpu(__psunsigned_t hub_base, lboard_t *brd_ptr, int cpu_ndx,
		int updateme)
{
	klcpu_t		*cpuptr ;
	nasid_t		nasid;
	__psunsigned_t	x ;

	nasid = KL_CONFIG_BOARD_NASID(brd_ptr);

	if ((cpuptr = (klcpu_t *)kl_config_alloc_component(nasid,
							   brd_ptr)) == NULL) {
		printf("init_klcfg_cpu: Cannot add cpu component.Node %d\n",
		       nasid);
		return;
	}
	cpuptr->cpu_info.struct_type = KLSTRUCT_CPU ;
	x = (cpu_ndx == IP27_CPU0_INDEX) ? PI_CPU_ENABLE_A : PI_CPU_ENABLE_B ;
	cpuptr->cpu_info.flags = (LD(REMOTE_HUB(NASID_GET(hub_base), x)) ? KLINFO_ENABLE : 0) ;
        if (cpuptr->cpu_info.flags != KLINFO_ENABLE) {
                char		*key, disable_why[64], *ptr;

                key = (cpu_ndx == IP27_CPU0_INDEX) ? "DisableA" : "DisableB";
		if (ip27log_getenv(NASID_GET(hub_base), key, disable_why, 0, 0) >= 0) {
                	cpuptr->cpu_info.flags = KLINFO_FAILED;
                        if (ptr = strchr(disable_why, ':')) {
                        	*ptr = 0;
                        	cpuptr->cpu_info.diagval = atoi(disable_why);
			}
			else
				cpuptr->cpu_info.diagval = KLDIAG_TBD;
                }
		else {
			/*
			 * We really don't know why this CPU failed. The LED
			 * value is stashed away in its PI_RT_COMPARE_X and
			 * we need to change it to a kldiag value
			 */

			__psunsigned_t	rt_comp;
			unsigned short	kldg;
			char    	led, buf[64];
			char    	*key = ((cpu_ndx == IP27_CPU0_INDEX) ?
					"DisableA" : "DisableB");

			rt_comp = (cpu_ndx == IP27_CPU0_INDEX) ? 
				PI_RT_COMPARE_A : PI_RT_COMPARE_B;

			led = (char) REMOTE_HUB_L(NASID_GET(hub_base), rt_comp);
			kldg = hled_get_kldiag(led);

			sprintf(buf, "%d: %x; %s", kldg, led,
				get_diag_string(kldg));

			if (!hled_tdisable(led))
			{
			    if(led == FLED_MODEBIT)
				ed_cpu_mem(NASID_GET(hub_base), key, buf, buf, 1, 0);
			    else
				ed_cpu_mem(NASID_GET(hub_base), key, buf, buf, 0, 0);
			}
			cpuptr->cpu_info.diagval = kldg;
		}
        }

	cpuptr->cpu_info.physid = cpu_ndx ;
	cpuptr->cpu_info.virtid = -1;

	if (updateme) {
		add_klcfg_cpuinfo(NASID_GET(hub_base),cpuptr);
	} else {  /* cpu present but disabled or cpu not present */
		cpuptr->cpu_prid = 0xffff;
		cpuptr->cpu_fpirr = 0xffff;
		cpuptr->cpu_speed = -1 ;	  /* XXX get these from cfgp */
	}

	/*
	 * allocate the cpu error structure for future use.
	 */
	cpuptr->cpu_info.errinfo = kl_config_info_alloc(nasid, ERRINFO_STRUCT,
							sizeof(klcpu_err_t));
	if (cpuptr->cpu_info.errinfo == -1) {
		printf("init_klcfg_cpu: err struct alloc failed %d node\n",
		       nasid);
	}

	return;
}


void
init_klcfg_hub(__psunsigned_t hub_base, lboard_t *brd_ptr, int hub_ndx)
{
	klhub_t		*hubptr ;
	nasid_t		nasid;
#ifdef SABLE
	klhub_uart_t 	*hub_uart_ptr;
#endif
	char		hub_nic_info[1024] ;

	*hub_nic_info = 0 ;
	nasid = KL_CONFIG_BOARD_NASID(brd_ptr);
	if ((hubptr = (klhub_t *)kl_config_alloc_component(nasid,
							   brd_ptr)) == NULL) {
		printf("init_klcfg_hub: Cannot add hub component.Node %d\n",
		       nasid);
		return;
	}

	hubptr->hub_info.struct_type = KLSTRUCT_HUB ;
	if(nasid == NASID_GET(hub_base))
		hubptr->hub_info.flags = KLINFO_ENABLE ;
	else
		hubptr->hub_info.flags = 0 ;

	hubptr->hub_port.port_nasid = INVALID_NASID;
	hubptr->hub_info.physid = NASID_GET(hub_base) ;
	hubptr->hub_info.nasid =  nasid;
	hubptr->hub_info.virtid = -1;
	hubptr->hub_info.revision = (unsigned char)
	    ((LD(hub_base + NI_STATUS_REV_ID) & NSRI_REV_MASK) >>
	     NSRI_REV_SHFT);

	/* Allocate and read the MFG NIC header */

	get_hub_nic_info((nic_data_t)REMOTE_HUB(NASID_GET(hub_base), MD_MLAN_CTL), 
			  hub_nic_info) ;
	if (hubptr->hub_mfg_nic = klmalloc_nic_off(nasid, strlen(hub_nic_info)))
		strcpy((char *)NODE_OFFSET_TO_K1(nasid, hubptr->hub_mfg_nic), 
				hub_nic_info) ;
	hubptr->hub_speed = 
	 (((ip27config_t *)(IP27CONFIG_ADDR_NODE(nasid)))->freq_hub);

#ifdef SABLE
	if ((hub_uart_ptr =
	     (klhub_uart_t *)kl_config_alloc_component(nasid,
						       brd_ptr)) == NULL) {
		printf("init_klcfg_hub:Cannot add uart component.Node %d\n",
		       nasid);
		return;
	}
	hub_uart_ptr->hubuart_info.struct_type = KLSTRUCT_HUB_UART ;
	hub_uart_ptr->hubuart_info.flags =
		KLINFO_ENABLE|KLINFO_CONTROLLER|KLINFO_INSTALL ;
	hub_uart_ptr->hubuart_info.virtid = 0; /* if you change this
                                                * you are looking for
                                                * trouble.
                                                */
	hub_uart_ptr->hubuart_info.physid = 0 ;
	hub_uart_ptr->hubuart_info.nasid = hubptr->hub_info.nasid ;
	hub_uart_ptr->hubuart_info.widid = hubptr->hub_info.widid ;
	hub_uart_ptr->hubuart_info.module = hubptr->hub_info.module ;
#endif /* SABLE */

	/*
	 * allocate the cpu error structure for future use.
	 */
	hubptr->hub_info.errinfo = kl_config_info_alloc(nasid, ERRINFO_STRUCT,
							sizeof(klhub_err_t));
	if (hubptr->hub_info.errinfo == -1) {
		printf("init_klcfg_hub: err struct alloc failed %d node\n",
		       nasid);
	}
        else if (((HUB_COMP_ERROR(brd_ptr, &hubptr->hub_info))->he_extension =
		kl_config_info_alloc(nasid, ERRINFO_STRUCT,
		sizeof(klhub_err_ext_t))) == -1)
		printf("init_klcfg_hub: err struct alloc failed %d node\n",
			nasid);

	/*
	 * Do we need error info for hubuart??
	 */
	return;
}


void
init_klcfg_membnk(__psunsigned_t hub_base, lboard_t *brd_ptr, int n)
{
	klmembnk_t	*mem_ptr ;
	int	i;
	nasid_t	nasid;
	__uint64_t	mem_cfg;
        char		buf[8];
	u_short		prem_mask;

	nasid = KL_CONFIG_BOARD_NASID(brd_ptr);
	if ((mem_ptr =
	     (klmembnk_t *)kl_config_alloc_component(nasid,
						     brd_ptr)) == NULL) {
		printf("init_klcfg_membnk: Cannot add memory component.Node %d\n",
		       nasid);
		return;
	}

	mem_ptr->membnk_info.struct_type = KLSTRUCT_MEMBNK ;
	mem_ptr->membnk_info.struct_version = MEMORY_STRUCT_VERSION ;
	mem_ptr->membnk_info.flags = KLINFO_ENABLE ;
	mem_ptr->membnk_info.physid = n ; /* XXX */
	mem_ptr->membnk_info.virtid = -1;
	mem_ptr->membnk_info.nasid = NASID_GET(hub_base);

        if (ip27log_getenv(NASID_GET(hub_base), "DisableMemMask", 0, 0, 0) >= 0)
		mem_ptr->membnk_info.diagval = KLDIAG_NOT_SET;
	else
		mem_ptr->membnk_info.diagval = KLDIAG_PASSED;

	mem_ptr->membnk_memsz = 0 ;
	mem_ptr->membnk_attr = 0;

	/*
	 * The ip27 prom has programmed the memory config register. Read it
	 * and setup the dimm select and bank size information.
	 */
	mem_cfg = translate_hub_mcreg(LD(hub_base + MD_MEMORY_CONFIG));
	mem_ptr->membnk_dimm_select =
	    (mem_cfg & MMC_DIMM0_SEL_MASK) >> MMC_DIMM0_SEL_SHFT;

	for (i = 0; i < MD_MEM_BANKS; i++) {
		mem_ptr->membnk_bnksz[i] =
		    MD_SIZE_MBYTES((mem_cfg & MMC_BANK_MASK(i))
				   >> MMC_BANK_SHFT(i));
#ifdef SABLE
		if (mem_ptr->membnk_bnksz[i] > 16) {
#if 0
			printf("Reducing %d MB bank to 16 MB\n",
				mem_ptr->membnk_bnksz[i]);
#endif
			mem_ptr->membnk_bnksz[i] = 16;
		}
#endif
		mem_ptr->membnk_memsz += mem_ptr->membnk_bnksz[i] ;
	}

	if (mem_cfg & MMC_DIR_PREMIUM)
	    mem_ptr->membnk_attr |= (MEMBNK_PREMIUM << MD_MEM_BANKS );

	ip27log_getenv(NASID_GET(hub_base),PREMIUM_BANKS,buf,"00",0);
	prem_mask = (u_short)strtoull(buf,0,16);
	mem_ptr->membnk_attr |= prem_mask;
	/*
	 * allocate the membnk error structure for future use.
	 */
	mem_ptr->membnk_info.errinfo =
	    kl_config_info_alloc(nasid, ERRINFO_STRUCT,
				 sizeof(klgeneric_error_t));
	if (mem_ptr->membnk_info.errinfo == -1) {
		printf("init_klcfg_membnk: err alloc failed %d node\n",
		       nasid);
	}
	return;
}


/* IO BOARDS */

lboard_t	*
init_klcfg_baseio(__psunsigned_t widget_base)
{
	lboard_t *brd_ptr ;
	nasid_t	nasid ;
	int	wid ;

	nasid = NASID_GET(widget_base) ;

	if ((brd_ptr = kl_config_alloc_board(nasid)) == NULL) {
		printf("init_klcfg_baseio: Cannot allocate board struct\n");
		return NULL;
	}

	brd_ptr->brd_type = KLTYPE_BASEIO ;

	wid = WIDGETID_GET(widget_base);
	if (SN00 && !is_xbox_config(nasid)) 
		brd_ptr->brd_slot = get_node_slotid(nasid);
	else if (CONFIG_12P4I)
		brd_ptr->brd_slot = 
			SLOTNUM_XTALK_CLASS | 
			SLOTNUM_GETSLOT(get_node_slotid(nasid));
	else
		brd_ptr->brd_slot = 
			xbwidget_to_xtslot(get_node_crossbow(nasid), wid);

	return (kl_config_add_board(nasid, brd_ptr));
}

static int
HUB2METAROUTER_MODID(pcfg_t *p, int hubnum)
{
#define MAX_META_ROUTER_MODID 99
  static int nhubs = 0;
  int i, offset, subval;

  if(!nhubs)
  {
    for(i = 0; i < p->count; i++)
    {
      if(p->array[i].any.type == PCFG_TYPE_HUB)
        nhubs++;
    }
  }

  if(nhubs <= 64)
  {
    /* 128p or less */
    if(hubnum & 0x1)
      return(MAX_META_ROUTER_MODID);
    return(MAX_META_ROUTER_MODID-1);
  }

  /* else 256p or more */
  offset = ((hubnum-1) / 20) * 4;
  subval = (hubnum-1) % 4;
  return(MAX_META_ROUTER_MODID - offset - subval);
}

/*
 * This routine has been verified to work
 */
void
upd_rou_mod_meta_hubless(pcfg_t *p, pcfg_router_t *rou_cf, lboard_t *rou_ptr)
{
    int 		peer_rtr, r1, r2 ;

    if (!IS_META(rou_cf)) { /* Hubless */
	if ((peer_rtr = rou_cf->port[6].index) == PCFG_INDEX_INVALID)
	    return ;

	if (((r1 = p->array[peer_rtr].router.port[4].index) 
	    	!= PCFG_INDEX_INVALID) && 
	     !(p->array[r1].hub.flags & PCFG_HUB_HEADLESS)) {
	    rou_ptr->brd_module = p->array[r1].hub.module;
	}
	else 
	if (((r2 = p->array[peer_rtr].router.port[5].index)
	     	!= PCFG_INDEX_INVALID) &&
             !(p->array[r2].hub.flags & PCFG_HUB_HEADLESS)) {
	    rou_ptr->brd_module = p->array[r2].hub.module;
	}
	else
	    printf("***Warning: Could not find module number for router nic = 0x%x\n",
			rou_ptr->brd_nic) ;

    }
    else { 	/* META ROUTERS */
        int	port, done = 0, odd;

        /* 
         * XXX: HACK!!! If meta-rtr is connected to a odd numbered module
         * moduleid = 1; else moduelid = 2 Refer to Jim Ammon's picture!
         */
	/* The problem with this scheme is that it clashes with normal
	 * modules with id 1 and 2. Everyone will normally have their
	 * modules starting with 1 and 2. This will make meta routers in
	 * slots 1 and 2 looks as if they are on the same brd as routers
	 * in slots 1 and 2. Also if we want to distinguish between
	 * 2 routers by their module ids, this will not allow it.
	 * The fix is to start meta router module ids from a high value
	 * downwards and hope that the system does not get too big that
	 * they will clash.
	 */
         for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
             int	index;

             if (((index = rou_cf->port[port].index) != PCFG_INDEX_INVALID) &&
                (p->array[index].any.type == PCFG_TYPE_ROUTER) &&
                IS_RESET_SPACE_RTR(&p->array[index].router)) {

                 int	r_port, r_index;

                 for (r_port = 1; r_port <= MAX_ROUTER_PORTS; r_port++)
                     if (((r_index = p->array[index].router.port[r_port].index)
			!= PCFG_INDEX_INVALID) && 
                        (p->array[r_index].any.type == PCFG_TYPE_HUB) &&
                        !(p->array[r_index].hub.flags & PCFG_HUB_HEADLESS)) {


                         if (!done) {

                           int hubmod = p->array[r_index].hub.module;

			   odd = hubmod & 0x1;
			   rou_ptr->brd_module = HUB2METAROUTER_MODID(p,
			                                              hubmod);
                           done = 1;
                         }
                         else if ((odd && !(p->array[r_index].hub.module & 0x1))
				|| (!odd && (p->array[r_index].hub.module & 
				0x1)))
                             printf("*** Meta rtr NIC 0x%lx moduleid can't be "
				"allocated. Check moduleids or cabling\n",
				rou_cf->nic);
                     }
             }
         }
     }
}

int
hubless_rtr(pcfg_t *p, pcfg_router_t *rou_cf, int flag)
{
    	int		port;
    	pcfg_hub_t	*ph ;
	int		index ;

	ForEachValidRouterPort(rou_cf,port) {
		index = rou_cf->port[port].index ;
		if (pcfgIsHub(p,index)) {
			ph = pcfgGetHub(p,index) ;
			if (!flag)
	    			if (!(IS_HUB_MEMLESS(ph)))
        				return 0;
			else
                                if (!(IS_HUB_HEADLESS(ph)))
                                        return 0;
    		}
	}

    	return 1;
}

void
update_brd_module(lboard_t *brd_ptr)
{
	char	buf[8];

	ip27log_getenv(NASID_GET(brd_ptr), IP27LOG_MODULE_KEY, buf, "0", 0);
	brd_ptr->brd_module = (moduleid_t) atoi(buf);
}

lboard_t	*
init_klcfg_routerb(pcfg_t *p,
		   nasid_t nasid,
		   pcfg_router_t *pcfg_routerp,
		   int router_type)
{
	lboard_t *brd_ptr;
	char buf[8];

	if ((brd_ptr = kl_config_alloc_board(nasid)) == NULL) {
		return NULL;
	}

	brd_ptr->brd_type = router_type;
	brd_ptr->brd_nic  = pcfg_routerp->nic;
	brd_ptr->brd_slot = pcfg_routerp->slot | SLOTNUM_ROUTER_CLASS;

	if (!IS_META(pcfg_routerp))
            brd_ptr->brd_partition = pcfg_routerp->partition;

        if (IS_META(pcfg_routerp) || hubless_rtr(p, pcfg_routerp, 0))
            upd_rou_mod_meta_hubless(p, pcfg_routerp, brd_ptr);
        else
	    brd_ptr->brd_module = pcfg_routerp->module;

	if (router_type != KLTYPE_NULL_ROUTER)
	    init_klcfg_routerc(pcfg_routerp, brd_ptr, 0, 0);

	return (kl_config_add_board(nasid, brd_ptr));
}


klbri_t *
init_klcfg_bridge(__psunsigned_t bridge_base, lboard_t *brd_ptr, int n)
{
	klbri_t	*briptr ;
	nasid_t		nasid;

	nasid = KL_CONFIG_BOARD_NASID(brd_ptr);
	if ((briptr = (klbri_t *)
			kl_config_alloc_component(nasid,
						  brd_ptr)) == NULL) {
		printf("init_klcfg_bridge: Cannot add component. Node %d\n",
		       nasid);
		return (klbri_t *)NULL;
	}
	briptr->bri_info.struct_type = KLSTRUCT_BRI ;
	briptr->bri_info.struct_version = BRIDGE_STRUCT_VERSION ;
	briptr->bri_info.flags = KLINFO_ENABLE ;
	briptr->bri_info.nic = brd_ptr->brd_nic ;

	briptr->bri_info.nasid	= brd_ptr->brd_nasid ;
	briptr->bri_info.physid = n ;
	briptr->bri_info.virtid = -1 ;
	briptr->bri_info.widid	= WIDGETID_GET(bridge_base) ;
	briptr->bri_info.revision = 
			(LW(bridge_base + BRIDGE_WID_ID) >> 28) & 0xf;
/* XXX */
	briptr->bri_eprominfo = -1 ;
	briptr->bri_bustype = KLSTRUCT_PCI ;
/* XXX */
	/*
	 * allocate the bridge error structure for future use.
	 */
	briptr->bri_info.errinfo =
	    kl_config_info_alloc(nasid, ERRINFO_STRUCT,
				 sizeof(klbridge_err_t));

	if (briptr->bri_info.errinfo == -1) {
		printf("init_klcfg_bridge: err alloc failed %d node\n",
		       nasid);
	}
	return briptr;
}

void
init_klcfg_routerc(pcfg_router_t *router_base, lboard_t *brd_ptr, int v, int p)
{
	klrou_t	*rouptr ;
	klrouter_err_t *err_info;
	int i;
	nasid_t		nasid;

	nasid = KL_CONFIG_BOARD_NASID(brd_ptr);
	if ((rouptr = (klrou_t *)kl_config_alloc_component(nasid,
							   brd_ptr)) == NULL) {
		printf("init_klcfg_routerc: Cannot add component.Node %d\n",
		       nasid);
		return;
	}
	rouptr->rou_info.struct_type = KLSTRUCT_ROU ;
	rouptr->rou_info.struct_version = ROUTER_VECTOR_VERS;
	rouptr->rou_info.flags = KLINFO_ENABLE ;
	rouptr->rou_info.nic = router_base->nic ;
	rouptr->rou_info.physid = p ;
	rouptr->rou_info.virtid = v ;
	rouptr->rou_flags = router_base->flags ;
#if 0
	rouptr->rou_box_nic = router_base->boxid ;
#endif

	for (i=1; i<=MAX_ROUTER_PORTS; i++)
		rouptr->rou_port[i].port_nasid = INVALID_NASID ;

	rouptr->rou_mfg_nic = NULL ;
	/*
	 * allocate the router error structure for future use.
	 */
	rouptr->rou_info.errinfo =
	    kl_config_info_alloc(nasid, ERRINFO_STRUCT,
				 sizeof(klrouter_err_t));

	if (rouptr->rou_info.errinfo == -1) {
		printf("init_klcfg_router: err alloc failed %d node\n",
		       nasid);
	}
	else {
		err_info = ROU_COMP_ERROR(brd_ptr, (&(rouptr->rou_info)));
        	for (i = 1; i <= MAX_ROUTER_PORTS; i++)
			err_info->re_status_error[i] = router_base->port[i].error;
	}
	return;
}

klinfo_t *
init_klcfg_ioc3(__psunsigned_t bridge_base, lboard_t *brd_ptr, int npci)
{
	klioc3_t	*ioc3p ;
	nasid_t		nasid;
	klinfo_t	*ktmp ;
	unsigned char	ioc3_flags = 0 ;
	console_t	*cons ;

	nasid = KL_CONFIG_BOARD_NASID(brd_ptr);
	if ((ioc3p = (klioc3_t *)kl_config_alloc_component
			(nasid, brd_ptr)) == NULL) {
		printf("init_klcfg_ioc3: Cannot add component.Node %d\n",
		       nasid);
		return NULL ;
	}

	/* Find if this ioc3 is a valid console device */

	cons = KL_CONFIG_CH_CONS_INFO(get_nasid()) ;
	ioc3_flags = KLINFO_ENABLE|KLINFO_CONTROLLER;
	if ((cons->uart_base) && (cons->wid == WIDGETID_GET(bridge_base))) {
		db_printf("*** Local Master IO6 is on widget 0x%lx\n",
			  WIDGETID_GET(bridge_base)) ;
#if 0
		ioc3_flags = KLINFO_ENABLE|KLINFO_CONTROLLER|KLINFO_INSTALL ;
#endif
		brd_ptr->brd_flags |= LOCAL_MASTER_IO6 ;
	} else {
		db_printf("*** Non-Master IO6 found on widget 0x%lx\n",
			  WIDGETID_GET(bridge_base)) ;
	}

	/* IOC3 */
	ktmp = (klinfo_t *)&ioc3p->ioc3_info ;

	ktmp->struct_type = KLSTRUCT_IOC3 ;
	ktmp->flags       = ioc3_flags ;
	ktmp->nic         = (nic_t) -1 ;
	ktmp->physid      = npci ;
	ktmp->revision    = LW(bridge_base + BRIDGE_TYPE0_CFG_DEV(npci) + 8) & 0xff ;
	ktmp->nasid	  = brd_ptr->brd_nasid  ;
	ktmp->widid	  = WIDGETID_GET(bridge_base) ;

	/* SUPERIO */
	ktmp = (klinfo_t *)&ioc3p->ioc3_superio ;

	ktmp->struct_type = KLSTRUCT_IOC3UART ;
	ktmp->flags       = ioc3_flags ;
	ktmp->nic	  = (nic_t) -1 ;
	ktmp->physid      = 0 ; /* XXX UART's physid is 0 */
	ktmp->virtid      = -1 ;
	ktmp->diagval     = KLDIAG_PASSED ;
	ktmp->nasid       = brd_ptr->brd_nasid ;
	ktmp->widid       = ioc3p->ioc3_info.widid ;
	ktmp->revision    = -1 ;

	/* ETHERNET */
	ktmp = (klinfo_t *)&ioc3p->ioc3_enet ;

	ktmp->struct_type = KLSTRUCT_IOC3ENET ;
	ktmp->flags       = ioc3_flags ;
	ktmp->nic         = (nic_t) -1 ;
	ktmp->physid	  = 0 ;
	ktmp->virtid	  = -1 ;
	ktmp->diagval     = KLDIAG_PASSED ;
	ktmp->nasid	  = brd_ptr->brd_nasid ;
	ktmp->widid	  = ioc3p->ioc3_info.widid ;
	ktmp->revision    = ioc3p->ioc3_info.revision ;

	ktmp->errinfo     = kl_config_info_alloc(nasid, ERRINFO_STRUCT,
				 sizeof(klgeneric_error_t));
	if (ktmp->errinfo == -1) {
		printf("init_klcfg_ioc3: err alloc failed %d node\n",
		       nasid);
	}

	return ((klinfo_t *)ioc3p) ;
}


void
init_klcfg_qscsi(__psunsigned_t bridge_base, lboard_t *brd_ptr, int npci)
{
	klscsi_t	*scsip ;
	int		i ;
	nasid_t		nasid;

	nasid = KL_CONFIG_BOARD_NASID(brd_ptr);
	if ((scsip = (klscsi_t *)kl_config_alloc_component(nasid,
							   brd_ptr)) == NULL) {
		printf("init_klcfg_qscsi: Cannot add component. Node %d\n",
		       nasid);
		return;
	}
	scsip->scsi_info.struct_type = KLSTRUCT_SCSI ;
	scsip->scsi_info.flags =
		KLINFO_ENABLE|KLINFO_CONTROLLER|KLINFO_INSTALL ;
	scsip->scsi_info.nasid = nasid ;
	scsip->scsi_info.nic = (nic_t) -1 ;
	scsip->scsi_info.physid = npci ;
	scsip->scsi_info.widid	= WIDGETID_GET(bridge_base) ;
	scsip->scsi_info.revision = 
		LW(bridge_base + BRIDGE_TYPE0_CFG_DEV(npci) + 8) & 0xff ;
	scsip->scsi_info.arcs_compt = 0 ;

#if 0 /* Printfs for debug */
	scsip->scsi_info.virtid = brd_ptr->brd_nasid*MAX_PORT_NUM + npci ;
	printf("init_klcfg_qscsi: nasid=%d, widget=%d, pciid=%d\n",
	       scsip->scsi_info.nasid,
	       scsip->scsi_info.widid,
	       scsip->scsi_info.physid) ;
#endif

	scsip->scsi_numdevs = 0;
	for (i = 0; i<MAX_SCSI_DEVS; i++)
		scsip->scsi_devinfo[i] = NULL ;
	/* add driver info */

	/*
	 * allocate the scsi error structure for future use.
	 */
	scsip->scsi_info.errinfo =
	    kl_config_info_alloc(nasid, ERRINFO_STRUCT,
				 sizeof(klgeneric_error_t));
	if (scsip->scsi_info.errinfo == -1) {
		printf("init_klcfg_qscsi: err alloc failed %d node\n",
		       nasid);
	}
	return;
}

#if defined (SABLE)
void
init_klcfg_sscsi(__psunsigned_t bridge_base, lboard_t *brd_ptr, int npci)
{
	klscsi_t	*scsip ;
	int		i;
	nasid_t		nasid;

	nasid = KL_CONFIG_BOARD_NASID(brd_ptr);
	if ((scsip = (klscsi_t *)kl_config_alloc_component(nasid,
							   brd_ptr)) == NULL) {
		printf("init_klcfg_sscsi: Cannot add component. Node %d\n",
		       nasid);
		return;
	}
	scsip->scsi_info.struct_type = KLSTRUCT_SCSI ;
	scsip->scsi_info.flags =
		KLINFO_ENABLE|KLINFO_CONTROLLER|KLINFO_INSTALL ;
	scsip->scsi_info.nasid = nasid ;
	scsip->scsi_info.nic = (nic_t) 0xABfEDDDD ;
	scsip->scsi_info.physid = npci ;
#if 0
	scsip->scsi_info.virtid = brd_ptr->brd_nasid*MAX_PORT_NUM + npci ;
#endif
	scsip->scsi_info.widid	= WIDGETID_GET(bridge_base) ;
	scsip->scsi_numdevs = 0;
	for (i = 0; i<MAX_SCSI_DEVS; i++)
		scsip->scsi_devinfo[i] = NULL ;
	/* add driver info */
}
#endif /* SABLE */


void
kl_disable_board(lboard_t *brd_ptr)
{
	brd_ptr->brd_flags &= ~ENABLE_BOARD;
}

/* IO BOARD COMPONENTS */

void
init_klcfg_xbow(__psunsigned_t xbow_base, lboard_t *brd_ptr)
{
	volatile __psunsigned_t	wid_base;
	klxbow_t	*xbowptr ;
	nasid_t		nasid;
	reg32_t		wid_id;

	nasid = KL_CONFIG_BOARD_NASID(brd_ptr);

	if ((xbowptr = (klxbow_t *)kl_config_alloc_component(nasid,
							     brd_ptr))==NULL){
		printf("init_klcfg_xbow: Cannot add component. Node %d\n",
		       nasid);
		return;
	}

	wid_base = SN0_WIDGET_BASE(nasid, 0);
	wid_id = LD32(wid_base + WIDGET_ID);
	

	xbowptr->xbow_info.struct_type = KLSTRUCT_XBOW ;
	xbowptr->xbow_info.nic = -1 ;
	xbowptr->xbow_info.virtid = 0 ;
	xbowptr->xbow_info.revision = XWIDGET_REV_NUM(wid_id) ;
	xbowptr->xbow_master_hub_link = xbow_get_master_hub(xbow_base);

	/*
	 * allocate the xbow error structure for future use.
	 */
	xbowptr->xbow_info.errinfo =
	    kl_config_info_alloc(nasid, ERRINFO_STRUCT, sizeof(klxbow_err_t));

	if (xbowptr->xbow_info.errinfo == -1) {
		printf("init_klcfg_xbow: err alloc failed %d node\n",
		       nasid);
	}
	return;
}


/* this function should be kept in sync with the copy in ml/SN0/module_info.c
   and the decode routine in the same file */
#define SN0_SERIAL_FUDGE	0x6e
void
encode_str_serial(const char *src, char *dest) {
	int i;

	for (i = 0; i < MAX_SERIAL_NUM_SIZE; i++) {

		dest[i] = src[MAX_SERIAL_NUM_SIZE/2 + 
			       ((i%2) ? ((i/2 * -1) - 1) : (i/2))] + 
			SN0_SERIAL_FUDGE;
	}
}

lboard_t	*
init_klcfg_midplane(__psunsigned_t widget_base, lboard_t *node_lbptr)
{
	lboard_t 	*mpl_lbptr;
	nasid_t 	nasid;
	klmod_serial_num_t *ser_num ;
	klhub_t		*hubp ;
	char 		*c ;

	nasid = NASID_GET(widget_base) ;
	if ((mpl_lbptr = kl_config_alloc_board(nasid)) == NULL) {
		printf("init_klcfg_midplane:Cannot alloc board struct\n");
		return NULL;
	}

	mpl_lbptr->brd_type = KLTYPE_MIDPLANE8 ;
	/* chosen arbitrarily. 0-3 cpu slots. 8-f are widget ids */
	mpl_lbptr->brd_slot = SLOTNUM_MIDPLANE_CLASS;
	/*
	 * This does not belong here! We need to discover each xbow, since
	 * we can have more than one xbow here.
	 * Normal speedo & 12p4i config donot have a crossbow. Whereas
	 * a speedo with a xbox does have a xbow.
	 */
	if ((!(SN00) && !(CONFIG_12P4I)) || is_xbox_config(nasid)) 
		init_klcfg_xbow(widget_base, mpl_lbptr) ;

	/* At Early Access time, the xbow does not have the NIC yet.
           It is available off the first node board's NIC. Assuming
	   that this is already read. Process that info here. */

	ser_num = (klmod_serial_num_t *)init_klcfg_compt(
			widget_base, mpl_lbptr, 0, KLSTRUCT_MOD_SERIAL_NUM, 0) ;
	ser_num->snum_info.flags = 0 ; /* No flags needed */
	/* Decrease the number of compts by 1 so that this is
 	   is not visible */
	mpl_lbptr->brd_numcompts -= 1 ; /* for Compatibility */
	if (!(SN00)) {
		if ((hubp = (klhub_t *)find_component(node_lbptr, NULL,
				KLSTRUCT_HUB))) {
	    	    if (hubp->hub_mfg_nic)
			if (c = strstr((char *)NODE_OFFSET_TO_K1(
				node_lbptr->brd_nasid,
				hubp->hub_mfg_nic), "MODULEID")) {
				        int i;
					char temp[MAX_SERIAL_NUM_SIZE];

					c = strstr(c, "Serial:") ;
					
					for (i = 0;i<MAX_SERIAL_NUM_SIZE &&
						     (*(c+7+i) != ';');i++) {
						temp[i] = *(c+7+i);
					}
					if (i<MAX_SERIAL_NUM_SIZE)
						temp[i] = '\0';

                                        DB_PRINTF(("Serial Number: %s\n",
                                               temp)) ;
					encode_str_serial(temp,ser_num->snum.snum_str);
					if (c)
						c = strstr(c,"MPLN");
					if (c)
						c = strstr(c,"Laser:");
					if (c)
						c += 6;
					if (c)
						mpl_lbptr->brd_nic = strtoull(c, 0, 16);
			}
		}
	} /* SN00 */

	return (kl_config_add_board(nasid, mpl_lbptr));
}

void
add_klcfg_xbow_port(lboard_t *xbow_lbptr, 
		    int link, 
		    int wid_type,
		    lboard_t *widget_lbptr)
{
	klxbow_t *xbow_p;
	nasid_t	  nasid = NASID_GET((__psunsigned_t)(xbow_lbptr));
	nasid_t port_nasid;
	int	xb_link = link - BASE_XBOW_PORT;

	if (wid_type == HUB_WIDGET_PART_NUM) {
		port_nasid = NASID_GET((__psunsigned_t)(widget_lbptr));	
		widget_lbptr = find_lboard((lboard_t *)
				       KL_CONFIG_INFO(nasid), 
				       KLTYPE_IP27);
	}
	else port_nasid = 0;
	
	xbow_p = (klxbow_t *)find_component(xbow_lbptr, NULL, KLSTRUCT_XBOW);
	
	xbow_p->xbow_port_info[xb_link].port_nasid = port_nasid;
	xbow_p->xbow_port_info[xb_link].port_offset = K0_TO_NODE_OFFSET(widget_lbptr);
	if (wid_type == HUB_WIDGET_PART_NUM) {
		xbow_p->xbow_port_info[xb_link].port_flag = XBOW_PORT_HUB;
	}
	else {
		xbow_p->xbow_port_info[xb_link].port_flag = XBOW_PORT_IO;
	}
	xbow_p->xbow_port_info[xb_link].port_flag |= XBOW_PORT_ENABLE;
}


lboard_t	*
init_klcfg_4chscsi(__psunsigned_t widget_base)
{
	lboard_t *brd_ptr;
	nasid_t nasid;
	klscsi_t	*qscsip ;
        int     wid ;

	nasid = NASID_GET(widget_base) ;
	if ((brd_ptr = kl_config_alloc_board(nasid)) == NULL) {
		printf("init_klcfg_4chscsi:Cannot allocate board struct\n");
		return NULL;
	}

	brd_ptr->brd_type = KLTYPE_4CHSCSI ;

	wid = WIDGETID_GET(widget_base);
	if (SN00 && !is_xbox_config(nasid))
		brd_ptr->brd_slot = get_node_slotid(nasid);
	else if (CONFIG_12P4I)
		brd_ptr->brd_slot = 
			SLOTNUM_XTALK_CLASS | 
			SLOTNUM_GETSLOT(get_node_slotid(nasid));
	else
		brd_ptr->brd_slot = 
			xbwidget_to_xtslot(get_node_crossbow(nasid), wid);

	return (kl_config_add_board(nasid, brd_ptr));
}

lboard_t	*
init_klcfg_tpu(__psunsigned_t widget_base)
{
	lboard_t *brd_ptr;
	nasid_t nasid;
        int     wid ;
	klinfo_t	*tpuci;
        kltpu_t       	*tpuc;

	nasid = NASID_GET(widget_base) ;
	if ((brd_ptr = kl_config_alloc_board(nasid)) == NULL) {
		printf("init_klcfg_tpu: Cannot allocate board struct\n");
		return NULL;
	}

	brd_ptr->brd_type = KLTYPE_TPU ;

	wid = WIDGETID_GET(widget_base);
	if (SN00 && !is_xbox_config(nasid))
		brd_ptr->brd_slot = get_node_slotid(nasid);
	else if (CONFIG_12P4I)
		brd_ptr->brd_slot = 
			SLOTNUM_XTALK_CLASS | 
			SLOTNUM_GETSLOT(get_node_slotid(nasid));
	else
		brd_ptr->brd_slot = 
			xbwidget_to_xtslot(get_node_crossbow(nasid), wid);

        if ((tpuc = (kltpu_t *)kl_config_alloc_component(nasid, brd_ptr))==NULL){
                printf("init_klcfg_tpu: Cannot add component. Node %d\n",
                       nasid);
                return NULL;
        }


	tpuci = &tpuc->tpu_info;
        tpuci->struct_type = KLSTRUCT_TPU ;
        tpuci->flags = KLINFO_ENABLE;
        tpuci->nic = -1 ;

        tpuci->physid = brd_ptr->brd_slot; /* To get unique locator names */
        tpuci->revision = (LW(widget_base + WIDGET_ID) >> 28) & 0xf;
        tpuci->virtid = -1; 
        tpuci->widid = wid;
        tpuci->nasid = nasid ;

	return (kl_config_add_board(nasid, brd_ptr));
}


lboard_t	*
init_klcfg_gsn(__psunsigned_t widget_base, int gsntype)
{
	lboard_t *brd_ptr;
	nasid_t nasid;
        int     wid ;
	klinfo_t	*gsnci;
        klgsn_t       	*gsnc;

	nasid = NASID_GET(widget_base) ;
	if ((brd_ptr = kl_config_alloc_board(nasid)) == NULL) {
		printf("init_klcfg_gsn: Cannot allocate board struct\n");
		return NULL;
	}

	brd_ptr->brd_type = (gsntype == KLSTRUCT_GSN_A) ? 
					KLTYPE_GSN_A : KLTYPE_GSN_B;

	wid = WIDGETID_GET(widget_base);
	if (SN00 && !is_xbox_config(nasid))
		brd_ptr->brd_slot = get_node_slotid(nasid);
	else if (CONFIG_12P4I)
		brd_ptr->brd_slot = 
			SLOTNUM_XTALK_CLASS | 
			SLOTNUM_GETSLOT(get_node_slotid(nasid));
	else
		brd_ptr->brd_slot = 
			xbwidget_to_xtslot(get_node_crossbow(nasid), wid);

        if ((gsnc = (klgsn_t *)kl_config_alloc_component(nasid, brd_ptr))==NULL){
                printf("init_klcfg_gsn: Cannot add component. Node %d\n",
                       nasid);
                return NULL;
        }


	gsnci = &gsnc->gsn_info;
        gsnci->struct_type = gsntype;
        gsnci->flags = KLINFO_ENABLE;
        gsnci->nic = -1 ;

        gsnci->physid = brd_ptr->brd_slot; /* To get unique locator names */
        gsnci->revision = (LW(widget_base + WIDGET_ID) >> 28) & 0xf;
        gsnci->virtid = -1; 
        gsnci->widid = wid;
        gsnci->nasid = nasid ;

	return (kl_config_add_board(nasid, brd_ptr));
}



void
init_klcfg_uvme(__psunsigned_t bridge_base, lboard_t *brd_ptr, int npci)
{
	printf("Fixme: init_klcfg_uvme: unimplemented\n");
	printf("Fixme: init_klcfg_uvme: br base %x lbrd %x pci %x\n",
	       bridge_base, brd_ptr, npci);
}


/* called from gfx_widget_discover() for each gfx widget */
lboard_t *
init_klcfg_graphics(__psunsigned_t widget_base)
{
	lboard_t 	*lb;
	klgfx_t		*klg;
	klinfo_t	*gi;
	nasid_t		nasid;
	int		wid;

	nasid = NASID_GET(widget_base);
	wid = WIDGETID_GET(widget_base);

	if ((lb = kl_config_alloc_board(nasid)) == NULL) {
		printf("init_klcfg_graphics: Cannot allocate board struct\n");
		return NULL;
	}

	lb->brd_type = KLTYPE_GFX;
	lb->brd_slot = xbwidget_to_xtslot(get_node_crossbow(nasid), wid);

        if ((klg = (klgfx_t *)kl_config_alloc_component(nasid, lb)) == NULL){
                printf("init_klcfg_graphics: Cannot add component. Node %d\n", nasid);
                return NULL;
        }

	gi = &klg->gfx_info;
	gi->struct_type = KLSTRUCT_GFX;
	/* set both controller and device flags for gfx */
	gi->flags = KLINFO_ENABLE|KLINFO_CONTROLLER|KLINFO_DEVICE;
	gi->nic = -1;

        gi->physid = 0;
	/* XXX where does 16 come from???? */
        gi->virtid = nasid * 16; /* To get unique locator names */
        gi->widid = wid;
        gi->nasid = nasid ;

	klg->cookie = KLGFX_COOKIE;
	klg->old_gndevs = klg->old_gdoff0 = 0;
	klg->gfx_mfg_nic = NULL;
	/*
	 *  [sigh] this can't be set until later, as brd_module doesn't
	 *  get set until after this board is discovered.
	 *
	 *  klg->moduleslot = (lb->brd_module << 8) | SLOTNUM_GETSLOT(lb->brd_slot);
	 */
	klg->moduleslot = 0;
	klg->gfx_next_pipe = NULL;
	klg->gfx_specific = NULL;

	return (kl_config_add_board(nasid, lb));
}

lboard_t *
init_klcfg_xthd(__psunsigned_t widget_base)
{
	lboard_t 	*lb;
	klinfo_t	*gi;
        klxthd_t        *klg;
	nasid_t		nasid;
	int		wid;

	nasid = NASID_GET(widget_base);
	wid = WIDGETID_GET(widget_base);

	if ((lb = kl_config_alloc_board(nasid)) == NULL) {
		printf("init_klcfg_xthd: Cannot allocate board struct\n");
		return NULL;
	}

	lb->brd_type = KLTYPE_XTHD;
	lb->brd_slot = xbwidget_to_xtslot(get_node_crossbow(nasid), wid);

        if ((klg = (klxthd_t *)kl_config_alloc_component(nasid, lb)) == NULL){
                printf("init_klcfg_xthd: Cannot add component. Node %d\n", nasid);
                return NULL;
        }

	gi = &klg->xthd_info;
	gi->struct_type = KLSTRUCT_XTHD;
	/* set both controller and device flags for gfx */
	gi->flags = KLINFO_ENABLE|KLINFO_CONTROLLER|KLINFO_DEVICE;
	gi->nic = -1;

        gi->physid = 0;
	/* XXX where does 16 come from???? */
        gi->virtid = nasid * 16; /* To get unique locator names */
        gi->widid = wid;
        gi->nasid = nasid ;

	return (kl_config_add_board(nasid, lb));
}

lboard_t	*
init_klcfg_brd(__psunsigned_t bridge_base, int type)
{
	lboard_t *brd_ptr ;
	nasid_t	nasid;
	int	wid ;

	nasid = NASID_GET(bridge_base);

	if ((brd_ptr = kl_config_alloc_board(nasid)) == NULL) {
		printf("init_klcfg_brd: Cannot allocate board struct\n");
		return NULL;
	}

	brd_ptr->brd_type = type;

	wid = WIDGETID_GET(bridge_base) ;
	if (SN00 && !is_xbox_config(nasid))
		brd_ptr->brd_slot = get_node_slotid(nasid);
	else if (CONFIG_12P4I)
		brd_ptr->brd_slot = 
			SLOTNUM_XTALK_CLASS | 
			SLOTNUM_GETSLOT(get_node_slotid(nasid));
	else
		brd_ptr->brd_slot = 
			xbwidget_to_xtslot(get_node_crossbow(nasid), wid);

	return (kl_config_add_board(nasid, brd_ptr));
}

klinfo_t *
init_klcfg_compt(	__psunsigned_t cbase 	,
			lboard_t *bp		, 
			int physid, int type, int flag)
{
        klcomp_t        *compt ;
	klinfo_t	*ki ;
        nasid_t         nasid;

	/* Get the nasid from lboard_t */
        nasid = KL_CONFIG_BOARD_NASID(bp);

        if ((compt = (klcomp_t *)kl_config_alloc_component(nasid,
                                                         bp))==NULL){
                printf("init_klcfg_compt: Cannot add component. Node %d\n",
                       nasid);
                return NULL ;
        }

	/* take a typical member of the union */
	ki = &compt->kc_ioc3.ioc3_info ;
        ki->struct_type = type ;
	/* XXX different for CPU HUB etc. */
        ki->flags =
                KLINFO_ENABLE | KLINFO_CONTROLLER|KLINFO_INSTALL ;
        ki->nic = -1 ;

        ki->physid = physid ;
        ki->virtid = nasid * 16 ; /* To get unique locator names */
        ki->widid = WIDGETID_GET(cbase) ;
        ki->nasid = nasid ;
	if (flag)
		ki->revision = 
                LW(cbase + BRIDGE_TYPE0_CFG_DEV(physid) + 8) & 0xff ;

        ki->errinfo =
            kl_config_info_alloc(nasid, ERRINFO_STRUCT, 
					sizeof(klgeneric_error_t));
        if (ki->errinfo == -1) {
                printf("init_klcfg_compt: errinfo alloc failed on %d node\n",
                       nasid);
        }

        return ki ;
}

lboard_t	*
init_klcfg_menet(__psunsigned_t bridge_base)
{
	lboard_t *brd_ptr ;
	nasid_t	nasid;
	int	wid ;

	nasid = NASID_GET(bridge_base);

	if ((brd_ptr = kl_config_alloc_board(nasid)) == NULL) {
		printf("init_klcfg_menet: Cannot allocate board struct\n");
		return NULL;
	}

	brd_ptr->brd_type = KLTYPE_ETHERNET;

	wid = WIDGETID_GET(bridge_base) ;
	if (SN00 && !is_xbox_config(nasid))
		brd_ptr->brd_slot = get_node_slotid(nasid);
	else if (CONFIG_12P4I)
		brd_ptr->brd_slot = 
			SLOTNUM_XTALK_CLASS | 
			SLOTNUM_GETSLOT(get_node_slotid(nasid));
	else
		brd_ptr->brd_slot = 
			xbwidget_to_xtslot(get_node_crossbow(nasid), wid);

	return (kl_config_add_board(nasid, brd_ptr));
}



void
master_xbow_port_update(klxbow_t *xb, nasid_t nasid)
{
    int j;

    for (j = 0; j < MAX_XBOW_PORTS; j++) {		
	    xb->xbow_port_info[j].port_nasid = nasid;
    }
    return;
}

void
master_xbow_port_slave(klxbow_t *xb, int link, nasid_t nasid)
{
    link -= BASE_XBOW_PORT;
    xb->xbow_port_info[link].port_nasid = nasid;

    return;
}

		
void
slave_xbow_port_update(klxbow_t *xb, nasid_t nasid, 
		       klxbow_t *pxb, nasid_t pnasid, int plink)
{
    int hlink = hub_xbow_link(nasid);
    int j;

    hlink -= BASE_XBOW_PORT;
    plink -= BASE_XBOW_PORT;
    
    for (j = 0; j < MAX_XBOW_PORTS; j++) {		
	if (j != hlink) {
	    xb->xbow_port_info[j].port_nasid = pnasid;
	    xb->xbow_port_info[j].port_flag = pxb->xbow_port_info[j].port_flag;
	    xb->xbow_port_info[j].port_offset = 
		pxb->xbow_port_info[j].port_offset;
	}
	else
	    xb->xbow_port_info[j].port_nasid = nasid;
    }
}


int
klconfig_xbow_update(nasid_t nasid, int flag)
{
    int link, hub_link = hub_xbow_link(nasid);
    klxbow_t *xbow_p;
    lboard_t *brd;
    __psunsigned_t xbow_base;

    xbow_base = SN0_WIDGET_BASE(nasid, 0);

    if (!flag)
        xbow_reset_arb_register(xbow_base, hub_link);

    if (config_find_xbow(nasid, &brd, &xbow_p)) 
	    return 0;

    if (xbow_p->xbow_master_hub_link != hub_link) {
	klxbow_t *master_xbow_p;
	lboard_t *master_brd;
	nasid_t   master_nasid;
	int master_link;

	master_link = xbow_p->xbow_master_hub_link;
	master_nasid = xbow_get_link_nasid(xbow_base, master_link);
	if (master_nasid != INVALID_NASID) {
	    if (config_find_xbow(master_nasid, &master_brd, &master_xbow_p))
		return -1;
	}
	else {
	    printf("xbow_update: Could not get master nasid\n");
	    return -1;
	}
	db_printf("xbow_update: slave xbow 0x%lx master xbow 0x%lx\n",
		  xbow_p, master_xbow_p);
	slave_xbow_port_update(xbow_p, nasid, 
			       master_xbow_p, master_nasid, master_link);
	return 0;
    }

    master_xbow_port_update(xbow_p, nasid);
    for (link = BASE_XBOW_PORT; link < MAX_PORT_NUM; link++) {
	klxbow_t *slave_xbow_p;
	lboard_t *slave_brd;
	nasid_t   slave_nasid;
	    
	if (link == hub_link) continue;
	if ((xbow_p->xbow_port_info[link - BASE_XBOW_PORT].port_flag &
	     XBOW_PORT_HUB) == 0) continue;
	slave_nasid = xbow_get_link_nasid(xbow_base, link);
	if (slave_nasid != INVALID_NASID) {
	    if (config_find_xbow(slave_nasid, &slave_brd, &slave_xbow_p)) {
		    printf("xbow_update: could not get slave xbow board\n");
		continue;
	    }
	}
	else {
	    printf("xbow_update: Could not get slave nasid\n");
	    continue;
	}
	db_printf("xbow_update: updating slave nasid %d on link %d\n",
		     slave_nasid, link);
	master_xbow_port_slave(xbow_p, link, slave_nasid);
    }
    return 0;
}

		

/*
 * Function	: update_board_nasid
 * Parameters	: brd_ptr -> pointer to board config structure.
 *		  nasid	  -> nasid to be set
 * Purpose	: Update the nasid on the board and its components.
 * Assumptions	: none.
 * Returns	: none.
 */

void
update_board_nasid(lboard_t *brd_ptr, nasid_t nasid)
{
	klinfo_t *kl_comp;
	int i;

#if LDEBUG
	printf("update_board_nasid: setting board nasid to 0x%x\n", nasid);
#endif
	brd_ptr->brd_nasid = nasid;
	for (i = 0; i < KLCF_NUM_COMPS(brd_ptr); i++) {
	    kl_comp = KLCF_COMP(brd_ptr, i);
	    kl_comp->nasid = nasid;
	}
}

void
klconfig_nasid_update(nasid_t nasid, int flag)
{
    lboard_t *brd_ptr;
    console_t *cons;

    cons = KL_CONFIG_CH_CONS_INFO(nasid);

    if (cons->uart_base)
        set_console_node(cons, nasid);
    
    brd_ptr = (lboard_t *)KL_CONFIG_INFO(nasid);
    while (brd_ptr) {
	if (!KLCF_REMOTE(brd_ptr)) {
	    update_board_nasid(brd_ptr, nasid);
            if (brd_ptr->brd_parent)
                brd_ptr->brd_parent = (lboard_t *)
		((__psunsigned_t)brd_ptr->brd_parent |
		((__psunsigned_t)(nasid) << NASID_SHFT)) ;
	}
	brd_ptr = KLCF_NEXT(brd_ptr);
    }
    klconfig_xbow_update(nasid, flag);
}

/*
 * Function	: update_board_nic
 * Parameters	: brd_ptr -> pointer to board config structure.
 *		  nic	  -> nic to be set
 * Purpose	: Update the nic on the board and  the hub.
 * Assumptions	: none.
 * Returns	: none.
 */

void
update_board_nic(lboard_t *brd_ptr, nic_t nic)
{
        klinfo_t *kl_comp;
	int i;

#if LDEBUG
	printf("update_board_nic: brd_ptr=0x%016llx, nic=%d\n",
	       brd_ptr, nic);
#endif
       
	brd_ptr->brd_nic = nic;
       for(i=0;i<brd_ptr->brd_numcompts;i++)
	{
	    kl_comp = KLCF_COMP(brd_ptr,i);
	    if (kl_comp->struct_type == KLSTRUCT_HUB)
		kl_comp->nic = nic;
	}

#if LDEBUG
	printf("update_board_nic: return\n");
#endif
}

/*
 * Function:		klcfg_meta_port_hub_indx -> returns the next hub
 *			indx connected to the meta rtr or connected to the
 *			rtr connected to the meta rtr
 * Args:		p -> pcfg_t ptr
 *			rou_indx -> meta rtr pcfg indx
 *			meta_port -> meta rtr port to explore
 *			last_port -> last port explored; initted to -1
 * Returns:		hub indx, if found
 *			-1 otherwise
 */

static int
klcfg_meta_port_hub_indx(pcfg_t *p,
			int rou_indx,
			int meta_port,
			int *last_port)
{
	int	port, start, port_indx;

	port_indx = p->array[rou_indx].router.port[meta_port].index;

	if (port_indx == -1)
		return -1;

	/* There might never be a hub connected to a meta rtr, but why 
	 * take a chance */
	if (p->array[port_indx].any.type == PCFG_TYPE_HUB) {
		if (*last_port == -2)
			return -1;
		else {
			*last_port = -2;
			return port_indx;
		}
	}

	/* Router connected to a meta rtr */

	if (IS_META((&p->array[port_indx].router)))
		return -1;

	if (*last_port == -1)
		start = 1;
	else
		start = *last_port + 1;

	for (port = start; port <= MAX_ROUTER_PORTS; port++) {
		int	index;

		index = p->array[port_indx].router.port[port].index;

		if (index == -1)
			continue;

		if (p->array[index].any.type == PCFG_TYPE_HUB) {
			*last_port = port;
			return index;
		}
	}

	return -1;
}

/*
 * Function:		klcfg_add_meta_router -> Adds meta rtr to klconfig
 * Args:		p -> pcfg_t ptr
 *			rou_indx -> meta rtr indx
 *			partition -> Add only to partition
 * Returns:		Non-duplicate brd added
 * Logic:		Adds a brd structure to all first hubs found on 
 *			each of the ports; Flags all except the first as
 *			duplicate
 */

lboard_t *
klcfg_add_meta_router(pcfg_t *p, int rou_indx, partid_t partition)
{
	int		port;
	lboard_t	*rou_brd, *first_brd = NULL;

	for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
		int	indx, last_port = -1;

                if (p->array[rou_indx].router.port[port].index == 
			PCFG_INDEX_INVALID)
                    continue;

		while ((indx = klcfg_meta_port_hub_indx(p, rou_indx, port,
						&last_port)) != -1) {
			pcfg_hub_t *ph = (pcfg_hub_t *)&p->array[indx].hub ;

			if (p->array[indx].hub.partition != partition)
				continue;
			if (IS_HUB_MEMLESS(ph))
				continue ;

			rou_brd = init_klcfg_routerb(p,
				p->array[indx].hub.nasid, 
				&p->array[rou_indx].router, KLTYPE_META_ROUTER);

			if (!rou_brd) {
				db_printf("WARNING: Can't add meta router brd"
					" on nasid %d\n", 
					p->array[indx].hub.nasid);
			} else {
                                rou_brd->brd_partition = partition;

				if (!first_brd)
					first_brd = rou_brd;
				else
					rou_brd->brd_flags |= 
						DUPLICATE_BOARD;
			}
		}
	}

	return first_brd;
}

/*
 * Function	: klconfig_discovery_update
 * Parameters	: none.
 * Purpose	: After the network discovery, the master cpu assigns the
 *		  nasid to each node, and calls into this routine.
 *		  The nasids on all boards system wide is updated.
 *		  Also, if any router is found that is not to be connected
 *		  to any hub, that router's config is setup.
 *		  Since a router's board exists on each of the nodes it is
 *		  connected to, all except the first router is marked
 *		  as duplicate.
 * Assumptions	: Runs on master node.
 * Returns	: 0 or -1 (on failure)
 */

int
klconfig_discovery_update(pcfg_t *p, partid_t partition)
{
    	pcfg_hub_t 		*hub_cf;
    	pcfg_router_t 		*rou_cf;
    	int 			i, j, found_hub, orig_hub;
    	lboard_t 		*brd_ptr, *rou_brd;

	ForAllPcfg(p,i) {
		switch (pcfgGetType(p,i)) {
			case PCFG_TYPE_HUB:
	    			hub_cf = pcfgGetHub(p,i) ;
            			if ((hub_cf->partition != partition) ||
					(IS_HUB_MEMLESS(hub_cf)))
                				continue ;

#if LDEBUG
            printf("\tdisc upd: HUB: nasid %d, part %d\n", hub_cf->nasid, 
							hub_cf->partition);
#endif

				/* For a good hub, update nic, part and mod */

	    			brd_ptr = (lboard_t *)KL_CONFIG_INFO(
							hub_cf->nasid);
	    			while (brd_ptr) {
					if (brd_ptr->brd_type == KLTYPE_IP27) {
		    				update_board_nic(brd_ptr, 
							hub_cf->nic);
		    				update_brd_module(brd_ptr);
                    				brd_ptr->brd_partition = 
							hub_cf->partition;
                			}
					brd_ptr = KLCF_NEXT(brd_ptr);
	    			}
	    			break;

			case PCFG_TYPE_ROUTER:
	    			rou_cf = pcfgGetRouter(p,i) ;
#if LDEBUG
	    			printf("\tdisc upd: %s %s router: 0x%lx %d\n",
				(IS_META(rou_cf) ? "META" : "NORMAL"), 
				(hubless_rtr(p, rou_cf, 0)? "hubless" : ""),
					rou_cf->nic, rou_cf->partition);
#endif

            			if (	IS_META(rou_cf) || 
					hubless_rtr(p, rou_cf, 0) || 
					(rou_cf->partition != partition))
                			break;

				/* For a good Router, update dup brd flag */
				/* nic, mod, part updated elsewhere. */

	    			found_hub = 0;
	    			ForEachValidRouterPort(rou_cf,j) {
					int index = rou_cf->port[j].index ;
#if LDEBUG
					printf("\t\tport %d, index=%d\n", j, 
						index);
#endif
					if (!pcfgIsHub(p,index))
		    				continue;

					hub_cf = pcfgGetHub(p,index) ;

					if (IS_HUB_MEMLESS(hub_cf))
						continue ;

					if (found_hub) {
						if (config_find_nic_router(
								hub_cf->nasid, 
								rou_cf->nic,
								&brd_ptr, 
								NULL) == 0) 
						 	brd_ptr->brd_flags |= 
							DUPLICATE_BOARD;
					}
					found_hub++;
	    			}
	    			break;

			default:
	    			break;
		}
	}

    	klconfig_port_update(p, partition);

    	return 0;
}

int
meta_in_partition(pcfg_t *p,
                pcfg_router_t *rou_cf,
                partid_t partition,
                __uint64_t *port_mask)
{
    int         port, yes = 0;
    int         index;
    partid_t    next_rtr_partid;

    *port_mask = 0;

    for (port = 1; port <= MAX_ROUTER_PORTS; port++) {
        index = rou_cf->port[port].index;
        if (index == PCFG_INDEX_INVALID) {
                continue;
        }
        /*
         * If this meta router is connected to other meta-routers
         * follow the links to the next level of 8p routers
         * to find the partition IDs.
         */
        if (IS_META(&p->array[index].router)) {
	     int i;
	     int next_index;
             pcfg_router_t *next_meta = &p->array[index].router;
    	     for (i = 1; i <= MAX_ROUTER_PORTS; i++) {
		next_index = next_meta->port[i].index;
		if (next_index == PCFG_INDEX_INVALID) 
			continue;
		if (!IS_META(&p->array[next_index].router)) {
                   next_rtr_partid =
                      p->array[next_index].router.partition;
		   break;
	        }
	     }
             db_printf("meta->meta connection found on port %d\n", port);
             db_printf("meta port %d connects to module %d, slot %d\n",
                     port, p->array[next_index].router.module,
                     p->array[next_index].router.slot);
        } else {
                next_rtr_partid = p->array[index].router.partition;
        }
        if (next_rtr_partid == partition) {
            *port_mask |= (1 << (port - 1));
            yes = 1;
        }
    }

    return yes;
}

/*
 * Function	: klconfig_port_update
 * Parameters	: none.
 * Purpose	: Update all the ports on the hubs and routers in the system.
 * Assumptions	: network discover complete, and config info built.
 * Returns	: 0 or -1 on error.
 */

int
klconfig_port_update(pcfg_t *p, partid_t partition)
{
    pcfg_hub_t *hub_cf;
    pcfg_router_t *rou_cf;
    int i, j, type;

#if LDEBUG
    printf("klconfig_port_update: part = %d. entered\n", partition);
#endif

    for (i=0; i < p->count; i++) {
	switch (p->array[i].any.type) {
	case PCFG_TYPE_HUB:
	    hub_cf = &p->array[i].hub;
#if LDEBUG
	    printf("\tport_update: entry %d, found HUB nasid %d pttn %d\n", i,
			hub_cf->nasid, hub_cf->partition);
#endif
            if (hub_cf->partition != partition)
                break;

	    if (IS_HUB_MEMLESS(hub_cf))
		break ;

	    if ((hub_cf->port.index != PCFG_INDEX_INVALID) &&
			!(hub_cf->port.flags & PCFG_PORT_FENCE_MASK)) {
		rou_cf = &p->array[hub_cf->port.index].router;
		/*
		 * update this hub's port to point to the connected
		 * hardware (router or hub)
		 */
		setup_hub_port(hub_cf, rou_cf);
	    }
	    break;

	case PCFG_TYPE_ROUTER:
	    rou_cf = &p->array[i].router;
#if LDEBUG
	    printf("\tport_update: entry %d, found RTR NIC 0x%lx %s pttn %d\n", 
		i, rou_cf->nic, (IS_META(rou_cf) ? "META" : "NORMAL"),
		rou_cf->partition);
#endif

            if (rou_cf->partition != partition)
	        break;

	    for (j = 1; j <= MAX_ROUTER_PORTS; j++) {
		if ((rou_cf->port[j].index == PCFG_INDEX_INVALID) ||
			(rou_cf->port[j].flags & PCFG_PORT_FENCE_MASK))
                    continue;

		type = p->array[rou_cf->port[j].index].any.type;

		if (type == PCFG_TYPE_HUB) {
		    pcfg_hub_t *conn_cf;
		    conn_cf = &p->array[rou_cf->port[j].index].hub;
            	    if (IS_HUB_MEMLESS(conn_cf))
                	continue ;
		    /*
		     * update the router's current port to point to the hub.
		     */
		    setup_router_port_to_hub(p, rou_cf, conn_cf, j);
		}
		else if (type == PCFG_TYPE_ROUTER) {
		    pcfg_router_t *conn_cf;
		    conn_cf = &p->array[rou_cf->port[j].index].router;
		    /*
		     * update each instance of this router board's current
		     * port to point to any one instance of the connected
		     * router.
		     */
		    setup_router_port_to_router(p, rou_cf, conn_cf, j);
		}
		else
		    printf("\tError: neither hub nor router on router port\n");
	    }
	    break;

	default:
	    break;
	}
    }

#if LDEBUG
    printf("klconfig_port_update: return\n");
#endif

    return 0;
}


/*
 * Function	: setup_hub_port
 * Parameters	: hub_cf -> prom config for hub.
 *		  rou_cf -> prom config to connected component.
 * Purpose	: point this hub's port to the node and offset of the
 *		  board it is connected to.
 * Assumptions	: none.
 * Returns	: 0 or -1 on failure.
 */

int
setup_hub_port(pcfg_hub_t *hub_cf, pcfg_router_t *rou_cf)
{
    lboard_t *node_brd;
    lboard_t *connect_brd = NULL ;
    pcfg_hub_t *ph;
    klhub_t *hub_comp;
    int rc;

#if LDEBUG
    printf("\t\tsetup_hub_port: hub nasid=%d\n", hub_cf->nasid);
#endif

    if (config_find_nic_hub(hub_cf->nasid, hub_cf->nic, &node_brd, &hub_comp)){
	    printf("setup_hub_port: cannot find node config, node %d\n",
		   hub_cf->nasid);
	    return -1;
    }

    switch (rou_cf->type) {
    case PCFG_TYPE_ROUTER:
#if LDEBUG
	printf("\t\tsetup_hub_port: connected to Router\n");
#endif
#if 0
	connect_brd = find_nic_type_lboard(hub_cf->nasid, KLTYPE_ROUTER2,
					   rou_cf->nic);
#endif
        if (config_find_nic_router(hub_cf->nasid, rou_cf->nic,
				       &connect_brd, NULL)) {
            if(config_find_nic_router_all(PCFG(get_nasid()), rou_cf->nic, &connect_brd, 
                                        NULL,rou_cf->partition, 0)) {
		printf("setup_hub_port: no connecting router board! Node %d index %d\n",
                   hub_cf->nasid, hub_cf->port.index);
		   return -1;
		}
        }
	break;
    case PCFG_TYPE_HUB:
#if LDEBUG
        printf("\t\tsetup_hub_port: connected to hub\n");
#endif

	ph = (pcfg_hub_t *)rou_cf;
	if (IS_HUB_MEMLESS(ph))
	    return 0;
	connect_brd = find_nic_type_lboard(ph->nasid, KLTYPE_IP27, ph->nic);
	break;
    default:
	connect_brd = NULL;
	break;
    }

    if (connect_brd) {
	hub_comp->hub_port.port_nasid =	connect_brd->brd_nasid;
	hub_comp->hub_port.port_offset = K0_TO_NODE_OFFSET(connect_brd);
        if (hub_cf->port.flags & PCFG_PORT_FENCE_MASK)
            hub_comp->hub_port.port_flag |= SN0_PORT_FENCE_MASK;
	rc = 0;
    }
    else {
	printf("setup_hub_port: no connect board! Node %d index %d\n",
	       hub_cf->nasid, hub_cf->port.index);
	rc = -1;
    }
    
#if LDEBUG
    printf("\t\tsetup_hub_port: hub 0x%lx, router 0x%lx return %d\n", 
	   node_brd, connect_brd, rc);
#endif

    return rc;
}


/*
 * Function	: setup_router_port_to_hub
 * Parameters	: rou_cf -> prom config for router
 *		  hub_cf -> prom config to connected hub.
 *		  port   -> port number where hub is connected on router.
 * Purpose	: point this routers port to the node and offset of the
 *		  hub board it is connected to.
 * Assumptions	: none.
 * Returns	: 0 or -1 on failure.
 */
int
setup_router_port_to_hub(pcfg_t *p,
			 pcfg_router_t *rou_cf,
			 pcfg_hub_t *hub_cf,
			 int port)
{
    lboard_t *node_brd;
    lboard_t *router_brd;
    klrou_t  *rou_comp;
    int i;
    pcfg_hub_t *chub_cf;
#if LDEBUG
    printf("\t\tsetup_router_port_to_hub: port=%d\n", port);
#endif

    if (config_find_nic_hub(hub_cf->nasid, hub_cf->nic, &node_brd, NULL)) {
	    printf("setup_hub_port: cannot find node config, node %d\n",
		   hub_cf->nasid);
	    return -1;
    }

    /*
     * Now walk each port of this router, and for each hub found, find the
     * instance of this router on that node. Setup the connection between
     * the router board and the connecting component.
     */
    for (i = 1; i <= MAX_ROUTER_PORTS; i++) {
	if ((rou_cf->port[i].index == PCFG_INDEX_INVALID) ||
			(rou_cf->port[i].flags & PCFG_PORT_FENCE_MASK))
		continue;

	if (p->array[rou_cf->port[i].index].any.type == PCFG_TYPE_HUB) {
	    chub_cf = &p->array[rou_cf->port[i].index].hub;
	    if (IS_HUB_MEMLESS(chub_cf))
		continue ;
            if (config_find_nic_router(chub_cf->nasid, rou_cf->nic,
				       &router_brd, &rou_comp)) {
                if(config_find_nic_router_all(p, rou_cf->nic, &router_brd, 
                                        &rou_comp,rou_cf->partition, 0)) {
		    printf("setup_router_port_to_hub: router missing %d\n",
			   chub_cf->nasid);
		    return -1;
                }
	    }
#if LDEBUG
            printf("\t\tsetup_router_port_to_hub: hub 0x%lx rtr 0x%lx\n",
		node_brd, router_brd);
#endif
	    rou_comp->rou_port[port].port_nasid = hub_cf->nasid;
	    rou_comp->rou_port[port].port_offset = K0_TO_NODE_OFFSET(node_brd);
	}
    }
	

#if LDEBUG
    printf("\t\tsetup_router_port_to_hub: return 0\n");
#endif

    return 0;
}


/*
 * Function	: setup_router_port_to_router
 * Parameters	: rou_cf -> prom config for router.
 *		  conn_cf -> prom config to connected component (router)
 *		  port   -> port on this router where component is connected.
 * Purpose	: point this routers's port to the node and offset of the
 *		  router it is connected to.
 *		  Since this router potentially has multiple instances of
 *		  the board, one on each node it is connected to, update
 *		  each instance of the router to point to any one instance
 *		  of the connected board config.
 * Assumptions	: Runs on master node.
 * Returns	: 0 or -1 on failure.
 */

int
setup_router_port_to_router(pcfg_t *p,
			    pcfg_router_t *rou_cf,
			    pcfg_router_t *conn_cf,
			    int port)
{
    lboard_t *router_brd;
    lboard_t *connect_brd = NULL;
    pcfg_hub_t *hub_cf;
    klrou_t *rou_comp;
    int i, hub_found, rc = 1;
    int remote_router = 0;

#if LDEBUG
    printf("\t\tsetup_router_port_to_router: port=%d\n", port);
    printf("\t\t\tFrom rou NIC 0x%lx %s pttn %d %s\n", rou_cf->nic, 
	(IS_META(rou_cf) ? "META" : "NORMAL"), rou_cf->partition, 
	(IS_RESET_SPACE_RTR(rou_cf) ? "RESET" : "NON_RESET"));
    printf("\t\t\tTo rou NIC 0x%lx %s pttn %d %s\n", conn_cf->nic, 
	(IS_META(conn_cf) ? "META" : "NORMAL"), conn_cf->partition, 
	(IS_RESET_SPACE_RTR(conn_cf) ? "RESET" : "NON_RESET"));
#endif

    /*
     * first walk all the links of the component we are connecting to.
     * Find its first hub and locate an instance of the connecting routers
     * board on that node.
     * Meta rtrs have a lboard struct on each of the hubs that are 
     * connected to the normal rtr connected to the meta rtr
     * If no hub is connected to that router, that router must be hubless 
     * router.
     * Find the connecting router's board on the master node (which is
     * assumed to be this node.)
     */

    for (i = 1; i <= MAX_ROUTER_PORTS; i++) {
        int	indx = conn_cf->port[i].index;

	if ((indx == PCFG_INDEX_INVALID) || 
		(conn_cf->port[i].flags & PCFG_PORT_FENCE_MASK))
            continue;

	if (p->array[indx].any.type == PCFG_TYPE_HUB) {
	    hub_cf = &p->array[indx].hub;
	    if (IS_HUB_MEMLESS(hub_cf))
		continue ;

	    rc = config_find_nic_router(hub_cf->nasid, conn_cf->nic,
				       &connect_brd, NULL);
	}

	if (IS_META(conn_cf) && (p->array[indx].any.type == PCFG_TYPE_ROUTER)) {
            int		j;

            for (j = 1; j <= MAX_ROUTER_PORTS; j++) {
                int		rtr_indx;
                pcfg_hub_t	*p_hub;

                rtr_indx = p->array[indx].router.port[j].index;

		if (rtr_indx == PCFG_INDEX_INVALID)
                    continue;

                if (p->array[rtr_indx].any.type == PCFG_TYPE_HUB) {
                    p_hub = &p->array[rtr_indx].hub;
		    if (IS_HUB_MEMLESS(p_hub))
			continue ;

                    rc = config_find_nic_router(p_hub->nasid, conn_cf->nic,
				&connect_brd, NULL);
                    if (!rc)
                        break;
                }
            }
        }

        if (!rc)
            break;
    }

    if (rc && config_find_nic_router_all(p, conn_cf->nic,
			       &connect_brd, NULL, conn_cf->partition, 0)) {
	return -1;
    }

#if LDEBUG
    printf("\t\tsetup_router_port_to_router: connect_brd 0x%lx\n", connect_brd);
#endif

    hub_found = 0;

    /*
     * Now walk each port of this router, and for each hub found, find the
     * instance of this router on that node. Setup the connection between
     * the router board and the connecting component.
     * If rou_cf is a meta rtr, update all lboards on all the hubs connected
     * to a normal rtr connected to a meta rtr
     */

    for (i = 1; i <= MAX_ROUTER_PORTS; i++) {
        int	indx = rou_cf->port[i].index;

#if 0
	if ((indx == PCFG_INDEX_INVALID) || 
		(rou_cf->port[i].flags & PCFG_PORT_FENCE_MASK))
            continue;
#endif
	/* For meta router systems, we were ignoring the router port
	 * that is connected to the meta router. Need to
	 * fill in rou_port_nasid for the ports that are connected
	 * to meta routers. Works and needs further investigation.
 	 */
	if ((indx == PCFG_INDEX_INVALID) || 
		((rou_cf->port[i].flags & PCFG_PORT_FENCE_MASK) 
			&& (!IS_META(pcfgGetRouter(p,indx)))))
            continue;

	/* If a meta router has been fenced off, it does not
	 * does not get into the normal router's klcfg. This
	 * is incomplete information. Under this special case
	 * get the port nasid, and offset.
 	 */

	if (IS_META(pcfgGetRouter(p,indx)) &&
		(rou_cf->port[i].flags & PCFG_PORT_FENCE_MASK)) {
		lboard_t	*rlb = NULL, *myrlb = NULL ;
		klrou_t		*myrkr = NULL ;
		pcfg_router_t	*rpcfg ;
		int 		rc ;

		rpcfg = pcfgGetRouter(p,indx) ;

		/* get my router's klcfg to fill up */

		rc = config_find_nic_router_all(p, rou_cf->nic, &myrlb,
						&myrkr, rou_cf->partition, 0) ;
		if (!rc && myrlb) {

		/* Find the meta router's valid klcfg. */
		config_find_nic_router_all(p, rpcfg->nic,
			       &rlb, NULL, rou_cf->partition, 0) ;
		if (!rc && rlb && myrkr) {

#if LDEBUG
			printf("In %llx,port %d filled nasid %d, offset %llx\n",
				myrlb->brd_nic, i, rlb->brd_nasid, 
				K0_TO_NODE_OFFSET(rlb)) ;
#endif

			myrkr->rou_port[i].port_nasid = rlb->brd_nasid ;
			myrkr->rou_port[i].port_offset= K0_TO_NODE_OFFSET(rlb);

			/* Fill the DUP also */
			rc = config_find_nic_router_all(p, rou_cf->nic, &myrlb,
                                                &myrkr, rou_cf->partition, 1) ;
			if (!rc && myrlb && myrkr) {
			myrkr->rou_port[i].port_nasid = rlb->brd_nasid ;
			myrkr->rou_port[i].port_offset= K0_TO_NODE_OFFSET(rlb);
#if LDEBUG
			printf("filled in dup for %llx\n", myrlb->brd_nic) ;
#endif
			}
		}

		}
		continue ;
	}

	if (p->array[indx].any.type == PCFG_TYPE_HUB) {
	    hub_cf = &p->array[indx].hub;

	    if (IS_HUB_MEMLESS(hub_cf))
		continue ;
	    hub_found++;
	    if (config_find_nic_router(hub_cf->nasid, rou_cf->nic,
				       &router_brd, &rou_comp)) {
#if 0
		    printf("setup_router_port_to_router: router missing %d port %d\n",
			   hub_cf->nasid, port);
#endif
		remote_router = 1;
		   
	    }
            else {
#if LDEBUG
                printf("\t\tsetup_router_port_to_router: Updating rou_brd "
			"0x%lx\n", router_brd);
#endif
	        rou_comp->rou_port[port].port_nasid = connect_brd->brd_nasid;
	        rou_comp->rou_port[port].port_offset =
			K0_TO_NODE_OFFSET(connect_brd);

                if (rou_cf->port[port].flags & PCFG_PORT_FENCE_MASK)
                    rou_comp->rou_port[port].port_flag |= SN0_PORT_FENCE_MASK;
            }
	}

        if (IS_META(rou_cf) && (p->array[indx].any.type == PCFG_TYPE_ROUTER)) {
            int		j;

            for (j = 1; j <= MAX_ROUTER_PORTS; j++) {
                int		rtr_indx;
                pcfg_hub_t	*p_hub;

                rtr_indx = p->array[indx].router.port[j].index;

                if (rtr_indx == PCFG_INDEX_INVALID)
                    continue;

                if (p->array[rtr_indx].any.type == PCFG_TYPE_HUB) {
                    p_hub = &p->array[rtr_indx].hub;

	    	    if (IS_HUB_MEMLESS(p_hub))
			    continue ;
       	            hub_found++;

                    if (config_find_nic_router(p_hub->nasid, rou_cf->nic,
                        &router_brd, &rou_comp)) {
#if 0
		        printf("setup_router_port_to_router: router missing"
				" %d\n", hub_cf->nasid);
#endif
		    }
                    else {
#if LDEBUG
                        printf("\t\tsetup_router_port_to_router: Updating "
				"rou_brd 0x%lx\n", router_brd);
#endif
	                rou_comp->rou_port[port].port_nasid = 
					connect_brd->brd_nasid;
	                rou_comp->rou_port[port].port_offset =
					K0_TO_NODE_OFFSET(connect_brd);

                        if (rou_cf->port[port].flags & PCFG_PORT_FENCE_MASK)
                            rou_comp->rou_port[port].port_flag |=
					SN0_PORT_FENCE_MASK;
                    }
                }
            }
        }
    }

    if (!hub_found || remote_router) {
	if (config_find_nic_router_all(p, rou_cf->nic, 
				   &router_brd, &rou_comp, rou_cf->partition, 0)) {
	    return -1;
	}
	rou_comp->rou_port[port].port_nasid = connect_brd->brd_nasid;
	rou_comp->rou_port[port].port_offset = K0_TO_NODE_OFFSET(connect_brd);

        if (rou_cf->port[port].flags & PCFG_PORT_FENCE_MASK)
            rou_comp->rou_port[port].port_flag |= SN0_PORT_FENCE_MASK;
    }

#if LDEBUG
    printf("\t\tsetup_router_port_to_router: return\n");
#endif

    return 0;
}

int
add_router_config(pcfg_t *p, partid_t partition)
{
    pcfg_hub_t *hub_cf;
    pcfg_router_t *rou_cf = NULL;
    int i;
    nasid_t	nasid ;

#if LDEBUG
    printf("add_router_config: count=%d\n", p->count);
#endif

    /*
     * Add a router config for all router brds
     * If HUB: add rtr brds for all rtrs directly connected
     * If RTR: If meta, call klcfg_add_meta_router
     *         else if (hubless && in reset space) add to gmaster
     *         XXX: This must really be pmaster!
     *	       Since this routine is called by pmaster in main.c
     *         it is OK to do get_nasid!!
     * Skip remote partition HUBs.
     */

    for (i = 0; i < p->count; i++) {
	if ((p->array[i].any.type == PCFG_TYPE_HUB) &&
		(p->array[i].hub.partition == partition)) {
	    hub_cf = &p->array[i].hub;
	    if (IS_HUB_MEMLESS(hub_cf))
		continue ;
	    if (hub_cf->port.index != PCFG_INDEX_INVALID &&
                p->array[hub_cf->port.index].any.type == PCFG_TYPE_ROUTER) {

		lboard_t	*rou_b;

		rou_cf = &p->array[hub_cf->port.index].router;

#if LDEBUG
		printf("add_router_config: adding rou lboard HUB nasid %d rou "
			"NIC 0x%lx\n", hub_cf->nasid, rou_cf->nic);
#endif

		nasid = hub_cf->nasid ;
		if (!(rou_b = init_klcfg_routerb_hl(p, nasid, rou_cf,
				       KLTYPE_ROUTER, rou_cf->partition))) {
		    printf("*** WARNING: Can't add module %d, slot r%d rtr to nasid %d\n", rou_cf->module, rou_cf->slot, nasid);
		    return -1;
		}
		else
                    rou_b->brd_partition = rou_cf->partition;
            }
	}
        else if (p->array[i].any.type == PCFG_TYPE_ROUTER) {
            if (IS_META(&p->array[i].router)) {
                db_printf("Adding meta router NIC 0x%lx\n", 
			p->array[i].router.nic);

		/*
		 * On 256p, we may not have a meta router connection
		 * to this partition.  So we don't necessarily want
		 * to see a warning message in this case.
		 * 
		 * Only print a warning if we try to add a brd but can't.
		 * This is done within klcfg_add_meta_router() so we
		 * don't have to do it here.
		 */
                klcfg_add_meta_router(p, i, partition);
            }
            else if ((p->array[i].router.partition == partition) && 
				hubless_rtr(p, &p->array[i].router, 0)) {
                lboard_t	*rou_brd;

                db_printf("Adding HUBless rtr NIC 0x%lx\n",
			p->array[i].router.nic);

                if (!(rou_brd = init_klcfg_routerb_hl(p, get_nasid(),
			&p->array[i].router, KLTYPE_ROUTER, partition))) {
                    db_printf("*** WARNING: Can't add HUBless rtr NIC 0x%lx to"
			"nasid %d\n", p->array[i].router.nic, get_nasid());
		} else {
                    klrou_t	*rou_info;

                    rou_brd->brd_partition = p->array[i].router.partition;

                    rou_info = (klrou_t *) find_first_component(rou_brd,
				KLSTRUCT_ROU);
                    rou_info->rou_info.flags |= KLINFO_HEADLESS;
                }
            }
        }
    }

#if LDEBUG
    printf("add_router_config: return\n");
#endif

    return 0;
}

/*
 * Function:		klconfig_update_failure -> Update klconfig of node brds'
 *			unexpected failure after discovery
 */

int
klconfig_failure_update(pcfg_t *p, partid_t partition)
{
    int		i;
    jmp_buf	new_buf;
    void	*old_buf;

    if (setfault(new_buf, &old_buf)) {
        printf("*** Exception occurred while accessing remote node!\n");
        restorefault(old_buf);
        return -1;
    }

    for (i = 0; i < p->count; i++)
        if ((p->array[i].any.type == PCFG_TYPE_HUB) &&
		(p->array[i].hub.partition == partition) &&
		(p->array[i].hub.flags & PCFG_HUB_EXCP)) {
            lboard_t	*brd;
            int		flagged = 0;
            klcpu_t	*cpu1, *cpu2;

            if (!(brd = find_lboard((lboard_t *) 
		KL_CONFIG_INFO(p->array[i].hub.nasid), KLTYPE_IP27)))
                continue;

             cpu1 = (klcpu_t *) find_first_component(brd, KLSTRUCT_CPU);
             cpu2 = (klcpu_t *) find_component(brd, (klinfo_t *) cpu1,
			KLSTRUCT_CPU);

            /*
             * Update klconfig if brd hasn't been flagged
             */

            if (!(brd->brd_flags & ENABLE_BOARD) || 
		(brd->brd_flags & FAILED_BOARD))
                flagged = 1;
            else if (cpu1 && (!(cpu1->cpu_info.flags & KLINFO_ENABLE) ||
			(cpu1->cpu_info.flags & KLINFO_FAILED)))
                    flagged = 1;
            else if (cpu2 && (!(cpu2->cpu_info.flags & KLINFO_ENABLE) ||
			(cpu2->cpu_info.flags & KLINFO_FAILED)))
                    flagged = 1;

            if (!flagged) {
                net_vec_t	vec;
                __uint64_t	ni_status;
                nasid_t		nasid;
                char		led, buf[64];

                printf("*** /hw/module/%d/slot/n%d: CPU(s) took unexpected "
			"exception.\n", p->array[i].hub.module, 
			p->array[i].hub.slot);
                printf("*** One or both CPUs might be bad. Disabling both.\n");

                REMOTE_HUB_S(p->array[i].hub.nasid, PI_CPU_ENABLE_A, 0);
                REMOTE_HUB_S(p->array[i].hub.nasid, PI_CPU_ENABLE_B, 0);

                if (cpu1) {
                    cpu1->cpu_info.flags &= ~KLINFO_ENABLE;
                    cpu1->cpu_info.diagval = KLDIAG_UNEXPEC_EXCP;
                }

                if (cpu2) {
                    cpu2->cpu_info.flags &= ~KLINFO_ENABLE;
                    cpu2->cpu_info.diagval = KLDIAG_UNEXPEC_EXCP;
                }

                /* XXX: This LED value is useless ? */
                led = (char) REMOTE_HUB_L(p->array[i].hub.nasid, 
			PI_RT_COMPARE_A);
                sprintf(buf, "%d: %x; %s\n", KLDIAG_UNEXPEC_EXCP, led,
			get_diag_string(KLDIAG_UNEXPEC_EXCP));

                ed_cpu_mem(p->array[i].hub.nasid, DISABLE_CPU_A, 
			buf, buf, 0, 0);
                ed_cpu_mem(p->array[i].hub.nasid, DISABLE_CPU_B, 
			buf, buf, 0, 0);

                klconfig_nasid_update(p->array[i].hub.nasid, 1);
            }
    }

    restorefault(old_buf);
    return 0;
}

void
klcfg_hubii_stat(nasid_t nasid)
{
    int		link_stat;
    lboard_t	*l;
    klhub_t	*h;

    link_stat = GET_FIELD(REMOTE_HUB_L(nasid, IIO_LLP_CSR),
		IIO_LLP_CSR_LLP_STAT);

    l = find_lboard((lboard_t *) KL_CONFIG_INFO(nasid), KLTYPE_IP27);
    
    h = (klhub_t *) find_first_component(l, KLSTRUCT_HUB);
    
    if ((link_stat == 0) || (link_stat == 1))
        h->hub_info.diagval = KLDIAG_HUB_LLP_DOWN;
    else
        h->hub_info.diagval = KLDIAG_PASSED;
}

/*
 * Support for maintaining a flag of all nasids searched.
 * Note: This is really not needed as nasids are unique
 * and when we go thro pcfg, we only search each nasid once.
 */

#define element_of_array        __uint64_t
#define bits_per_unit           8
#define unit_per_element        sizeof(element_of_array) /* units */
#define num_bits_per_element    (unit_per_element * bits_per_unit)

static struct {
        element_of_array        bit_flag[(MAX_NASIDS/num_bits_per_element)+1] ;
} nasid_flag ;

#define num_elements    (sizeof(nasid_flag)/sizeof(element_of_array))

#define MASK(_n)        (((__uint64_t)1) << (num_bits_per_element - \
                                (_n%num_bits_per_element) - 1)) 

#define INDEX(_n)       (_n/num_bits_per_element)

#define NASID_SET(_n)   if (_n < MAX_NASIDS)                    \
                            nasid_flag.bit_flag[INDEX(_n)] |=   \
                                MASK(_n) ;

#define NASID_ISSET(_n) (((INDEX(_n) + 1) <= num_elements) ?            \
                        (nasid_flag.bit_flag[INDEX(_n)] & MASK(_n)) : 0)

/*
 * init_klcfg_routerb_hl
 *	wrapper for init_klcfg_routerb. Meta routers, or hubless
 * 	routers may not find a place for themselves on the global
 * 	master. Allocate them on the next nearest node.
 */
static lboard_t *
init_klcfg_routerb_hl( 	pcfg_t 		*p		,
		   	nasid_t 	preferred_nasid	,
		   	pcfg_router_t 	*pcfg_routerp	,
		   	int 		router_type	,
			partid_t	partition)
{
	lboard_t 	*lb = NULL ;
	int		i ;

	if (lb = init_klcfg_routerb(p, preferred_nasid, 
					pcfg_routerp, router_type))
		return lb ;

	bzero((char *)&nasid_flag, sizeof(nasid_flag)) ;
	NASID_SET(preferred_nasid) ;	/* finished searching this */
	/* get the next nasid */
	ForAllPcfg(p,i) {
        	if (pcfgIsHub(p,i)) {
                   	if ((p->array[i].hub.partition 	== partition)	&&
				(!NASID_ISSET(p->array[i].hub.nasid))) {
				lb = init_klcfg_routerb(p, 
						p->array[i].hub.nasid,
						pcfg_routerp, 
						router_type) ;
				if (lb) break ;
				NASID_SET(p->array[i].hub.nasid) ;
			}
		}
	}
#if LDEBUG
	if (lb) 
		printf("looking for space on nasid %d, found it on nasid %d\n", 
			preferred_nasid, p->array[i].hub.nasid) ;
#endif
	return lb ;
}

/*
 * config_find_nic_router_all
 * 	wrapper for config_find_nic_router.
 * 	As the router klcfg may  be present on any node,
 * 	search all nasids.
 */
int
config_find_nic_router_all(	pcfg_t 		*p	,
				nic_t 		nic	,
				lboard_t 	**lbp	,
				klrou_t 	**krp	,
				partid_t	partition,
				int		dupflg)
{
	int 		i ;
    	pcfg_hub_t      *ph ;

        ForAllPcfg(p,i) {
                if (pcfgIsHub(p,i)) {
			ph = pcfgGetHub(p,i) ;
                    	if (	((ph->partition == partition) ||
				 (partition == INVALID_PARTID)) &&
				(!IS_HUB_MEMLESS(ph)))
				if (config_find_nic_router(ph->nasid, 
						nic, lbp, krp)==0)
					if (*lbp)
					   if (dupflg) {
						if (KL_CONFIG_DUPLICATE_BOARD(*lbp))
							return 0 ;
					    } else {
						if (!(KL_CONFIG_DUPLICATE_BOARD(*lbp)))
						return 0 ;
					    }
		}
	}
	return -1 ;
}

lboard_t *
init_klcfg_ip27_disabled(__psunsigned_t hub_base, int flag, nasid_t good_nasid)
{
	lboard_t *brd_ptr;
	nasid_t nasid;
	char	buf[8];
	char 	*c = NULL;
	klhub_t *hubp;
	

	nasid = NASID_GET(hub_base);

	if ((brd_ptr = kl_config_alloc_board(good_nasid)) == NULL) {
		printf("init_klcfg_ip27: Cannot allocate board struct\n");
		return NULL;
	}

	/* Since there is only one module in the 12P 4IO config
	 * hardwire the brd module number to 1 
	 */
	if (CONFIG_12P4I)
		brd_ptr->brd_module = 1;       
	brd_ptr->brd_type = KLTYPE_IP27;
	kl_disable_board(brd_ptr);
	brd_ptr->brd_brevision = (unsigned char)
	    ((LD(hub_base + NI_STATUS_REV_ID) & NSRI_REV_MASK) >>
	     NSRI_REV_SHFT);

	if (kl_sable) {
#ifdef SABLE
	    brd_ptr->brd_slot = (uchar)get_my_slotid() ;
#endif
	}
	else  {

		brd_ptr->brd_slot = (unsigned char)
		hub_slotbits_to_slot(LD(hub_base + MD_SLOTID_USTAT) & 
							MSU_SLOTID_MASK);
	}
	brd_ptr->brd_slot = get_node_slotid(nasid);
	ip27log_getenv(nasid, IP27LOG_MODULE_KEY, buf, "0", 0);
	brd_ptr->brd_module = (moduleid_t) atoi(buf);

	/*
	 * Who am I?
	 */

	/*
	 * setup a cpu component for each cpu found. This should match
	 * what the ip27 prom has already found...
	 */
	/*if (LD(hub_base + PI_CPU_PRESENT_A))*/
	    init_klcfg_cpu(hub_base, brd_ptr, IP27_CPU0_INDEX, 1);

	/*if (LD(hub_base + PI_CPU_PRESENT_B))*/
	    init_klcfg_cpu(hub_base, brd_ptr, IP27_CPU1_INDEX, 1);

	init_klcfg_hub(hub_base, brd_ptr, IP27_HUB_INDEX) ;
	init_klcfg_membnk(hub_base, brd_ptr, IP27_MEM_INDEX) ; 
	if (hubp = (klhub_t *)find_component(brd_ptr, NULL,
				KLSTRUCT_HUB)) {
	    	    if (hubp->hub_mfg_nic)
			c = (char *)NODE_OFFSET_TO_K1(
				good_nasid,
				hubp->hub_mfg_nic);
			}
	if(ip31_pimm_psc(nasid,NULL,c) == 0)
		strcpy(brd_ptr->brd_name,"IP31");
	else
		strcpy(brd_ptr->brd_name,"IP27");

	return (kl_config_add_board(good_nasid, brd_ptr));
}


