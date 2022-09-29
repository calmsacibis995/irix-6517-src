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
 * File: SN0.c
 *	Contains cpu dependent functions for SN0.
 */

#include <sys/types.h>
#include <sys/R10k.h>
#include <sys/SN/addrs.h>
#include <sys/SN/SN0/ip27config.h>
#include <sys/SN/SN0/ip27log.h>
#include <sys/SN/klconfig.h>
#include <sys/SN/promcfg.h>
#include <sys/SN/xbow.h>
#include <sys/PCI/bridge.h>
#include <sys/PCI/ioc3.h>
#include <sys/SN/slotnum.h>
#include <sys/SN/SN0/klhwinit.h>
#include <rtc.h>
#include <setjmp.h>
#include <libkl.h>
#include <libsc.h>
#include <klcfg_router.h>
#include <sys/SN/gda.h>
#include <sys/SN/SN0/hubmd.h>
#include <sys/SN/nvram.h>
#include <hub.h>

/*
 * These defines are here till SABLE is fixed with support for
 * all hub registers and bridge, xbow etc.
 */

#define SABLE_BRIDGE_WID  8
#define SABLE_IP27_SLOTID 2

#if !defined (SABLE)
int kl_sable = 0;
#else
int kl_sable = 1;
#endif

extern int nic_get_eaddr(__int32_t *, __int32_t *, char *);

static cpuid_t
hubcpuid(void)
{
	return (cpuid_t)(*LOCAL_HUB(PI_CPU_NUM));
}

cnodeid_t
get_cnodeid(void)
{
	gda_t *gdap;
	int i;

	gdap = GDA;

	for (i = 0; i < MAX_COMPACT_NODES; i++)
	    if (gdap->g_nasidtable[i] == get_nasid())
		return i;

	return -1;
}

/*
 * cpuid() returns logical ID of executing processor, as
 * set up by prom.
 */
int
cpuid(void)
{
	nasid_t nasid = get_nasid();
	klcpu_t *acpu;
	int sl, slice;

	slice = hubcpuid();
	for (sl = 0; sl < CPUS_PER_NODE; sl++) {
		acpu = nasid_slice_to_cpuinfo(nasid, sl);
		if (acpu && acpu->cpu_info.physid == slice)
		    return acpu->cpu_info.virtid;
	}
	return -1;
}



void
microsec_delay(int us)
{
	if (kl_sable) return;
	
	rtc_sleep(us);

	return;
}



int
hub_widget_id(nasid_t nasid)
{
	slotid_t slot;
	xwidgetnum_t wid;
#if !defined (SABLE)
	slot = get_node_slotid(nasid);
	wid = hub_slot_to_widget(slot);
	return wid;

#else /* SABLE */
#ifdef HUB_MD_FIXED_IN_SABLE
        slot = GET_FIELD64((hub_base + MD_SLOTID_USTAT), SLOT_ID_MASK, 0);
#else /* HUB_MD_FIXED_IN_SABLE */
	slot = SABLE_IP27_SLOTID ;
#endif /* HUB_MD_FIXED_IN_SABLE */

        return(slot + 7);
#endif /* SABLE */	
}


int
hub_xbow_link(nasid_t nasid)
{
	hubii_wcr_t ii_wcr;

	ii_wcr.wcr_reg_value = LD(REMOTE_HUB(nasid, IIO_WCR));
	return 	ii_wcr.wcr_fields_s.wcr_widget_id;
}

	
#if defined (HUB_ERR_STS_WAR)

/*
 * the hub runs into serious problems if we ever have a WRB error when the
 * error status register is cleared. Force a RRB timeout so that the error
 * status register is set.
 */

int
do_hub_errsts_war(void)
{
    jmp_buf		fault_buf;	
    void	       *old_buf;
    __uint64_t		tmp;
    pi_err_stat0_t	err_sts0;
    __psunsigned_t	error_addr;
    int			cpunum;
	
    if (cpunum = LD(LOCAL_HUB(PI_CPU_NUM))) {
	err_sts0.pi_stat0_word = *LOCAL_HUB(PI_ERR_STATUS0_B);
    }
    else {
	err_sts0.pi_stat0_word = *LOCAL_HUB(PI_ERR_STATUS0_A);
    }

    error_addr = err_sts0.pi_stat0_fmt.s0_addr;

    /*
     * If the address is correct, and the valid bit is set, return.
     */
    if ((error_addr == (ERR_STS_WAR_PHYSADDR >> ERR_STAT0_ADDR_SHFT)) &&
	(err_sts0.pi_stat0_fmt.s0_valid)) {
#ifdef DEBUG
	printf("nasid %d: error_addr is OK (0x%llx)\n", get_nasid(),
		error_addr);
#endif /* DEBUG */
	return 1;
    }

    if (setfault(fault_buf, &old_buf)) {
	restorefault(old_buf);
	*LOCAL_HUB(PI_ERR_INT_PEND) = 
	    PI_ERR_UNCAC_UNCORR_A | PI_ERR_UNCAC_UNCORR_B;
	return 1;
    }
    else {
	/*
	 * Clear the bogus previous contents and force a bus error.
	 * See explanation of HUB_ERR_STS_WAR or
	 * PV# 526920.
	 */
	if (cpunum) {
#ifdef DEBUG
	    printf("Nasid %d: PI_ERR_STATUS0_B 0x%llx, PI_ERR_STATUS1_B 0x%llx\n",
		    get_nasid(),
		    LD(LOCAL_HUB(PI_ERR_STATUS0_B)),
		    LD(LOCAL_HUB(PI_ERR_STATUS1_B)));
#endif /* DEBUG */
	    SD(LOCAL_HUB(PI_ERR_STATUS0_B_RCLR), 0);
	} else {
#ifdef DEBUG
	    printf("Nasid %d: PI_ERR_STATUS0_A 0x%llx, PI_ERR_STATUS1_A 0x%llx\n",
		    get_nasid(),
		    LD(LOCAL_HUB(PI_ERR_STATUS0_A)),
		    LD(LOCAL_HUB(PI_ERR_STATUS1_A)));
#endif /* DEBUG */
	    SD(LOCAL_HUB(PI_ERR_STATUS0_A_RCLR), 0);
	}
		
	LD(ERR_STS_WAR_ADDR);

        restorefault(old_buf);
#if DEBUG
	printf("Failed: read 0x%llx fine\n", ERR_STS_WAR_ADDR);
#endif /* DEBUG */
	return 1;
    }
}

#endif /* defined (HUB_ERR_STS_WAR)  */

#define SN00_SERIAL_FUDGE 	0x3b1af409d513c2
void
encode_int_serial(unsigned long long src,unsigned long long *dest) {
	unsigned long long val;
	int i;

	val = src + SN00_SERIAL_FUDGE;


	for (i = 0; i < sizeof(long long); i++) {
		((char*)dest)[i] = 
		  ((char*)&val)[sizeof(long long)/2 + 
			       ((i%2) ? ((i/2 * -1) - 1) : (i/2))];
	}
}

void
sn00_copy_mod_snum(__psunsigned_t bridge_base, int npci)
{
        lboard_t 		*lbptr ;
        klmod_serial_num_t 	*snum_ptr ;
	__psunsigned_t		ioc3_base ;
	char 			eaddr[MAX_SERIAL_NUM_SIZE] ;
	nasid_t 		nasid = NASID_GET(bridge_base) ;
	int ret_val;

        if (SN00) {
	 	ioc3_base = bridge_base + BRIDGE_DEVIO(npci) ;
		ret_val = nic_get_eaddr((__int32_t *)(ioc3_base + IOC3_MCR), 
					(__int32_t *)(ioc3_base + IOC3_GPCR_S),
					eaddr) ;

                lbptr = KL_CONFIG_INFO(nasid) ;
                lbptr = find_lboard(lbptr, KLTYPE_MIDPLANE) ;
                /* for SN00 look at component 1. There should
                   be no other component */
                if (lbptr && (lbptr->brd_compts[0])) {
                        snum_ptr = GET_SNUM_COMP(lbptr) ;
                        if (snum_ptr->snum_info.struct_type ==
                                KLSTRUCT_MOD_SERIAL_NUM) {
				int i;
				unsigned long long snum_val = 0;

				if (ret_val == 0) {
					/* if we found the serial number */
					for (i = 0; i < 6; i++) {
						snum_val <<= 8;
						snum_val |= eaddr[i];
					}
				}
				else {
					printf("Warning: found invalid serial "
					       "number, possible missing "
					       "or invalid NIC\n");
				}
				
				encode_int_serial(snum_val,
						  &snum_ptr->snum.snum_int);
                                return ;
                        }
                        else
                                printf("Could not find serial num struct\n") ;
                }
                else
                        printf("Warning: Could not register serial number.\n")
;
        }
}

/* 
 * The MD_MEMORY_CONFIG hub register contains configuration information
 * about memory banks.  Memory is required to be present at physical
 * address 0, for exception vector placement.  If the memory bank in DIMM
 * 0 (i.e. the dimm in slot 0 of the board, hereafter referred to as
 * "locational bank 0") is disabled for any reason, the DIMM0_SEL field of
 * MD_MEMORY_CONFIG can be set to torque the addresses so that physical
 * address 0 maps to locational bank 1.  The hardware XOR's the value of
 * the DIMM0_SEL field with the address bits that generate the low order
 * physical bank select signals.  Consequently, the configuration
 * information for each bank must be swapped in the MD_MEMORY_CONFIG
 * register.
 *
 * This routine exists to address PV 669589.  In older prom versions, the
 * mdir_swap_bank0() routine (which has since been replaced by
 * swap_memory_banks()) would only swap the configuration information for
 * banks 0 and 1, not for the other pairs of banks.  This caused a kernel
 * panic if bank 0 was disabled and there were an uneven number of banks
 * (i.e. bank 2 was populated but bank 3 was not).  To fix this, we swap
 * all pairs of banks in the register.  However, routines that provide
 * information to the user need to use numbers which describe the
 * _location_ of the bank which is disabled.  This routine translates the
 * values in MD_MEMORY_CONFIG from the software-visible swizzled format 
 * into non-swizzled locational format.
 *
 * NOTE: a copy of this routine is used in the kernel - keep in sync with
 * the version in irix/kern/ml/SN/SN0/FRU/sn0_fru_analysis.c
 *
 * NOTE 2: This routine, and all the other swap-related routines, assume
 * that the DIMM0_SEL field is always set to either 0 or 2.  If this
 * changes, much trouble will ensue.
 */

__uint64_t
translate_hub_mcreg(__uint64_t mc)
{
    __uint64_t          mc0, mc1, bank;

    if (((mc & MMC_DIMM0_SEL_MASK) >> MMC_DIMM0_SEL_SHFT) > 0) {
	for (bank = 0; bank < MD_MEM_BANKS; bank += 2) {

	    mc0 = (mc & MMC_BANK_MASK(bank)) >> MMC_BANK_SHFT(bank) ;
	    mc1 = (mc & MMC_BANK_MASK(bank+1)) >> MMC_BANK_SHFT(bank+1) ;

	    mc &= ~(MMC_BANK_MASK(bank));
	    mc &= ~(MMC_BANK_MASK(bank+1));

	    mc |= (mc0 << MMC_BANK_SHFT(bank+1));
	    mc |= (mc1 << MMC_BANK_SHFT(bank));
	}
    }

    return mc;
}

/*
 * If memory bank 0 is absent or disabled, memory bank 1 can be treated as
 * physical address 0 by changing the DIMM0_SEL field in MD_MEMORY_CONFIG
 * and swapping the configuration information for each pair of banks.
 * Note: this routine replaces stand/arcs/IP27prom/mdir.c's
 * mdir_swap_bank0(), which had a misleading name.  The routine was moved
 * to this file to be near similar routines. 
 */

void 
swap_memory_banks(nasid_t nasid, int new_bank)
{
    __uint64_t          mc0, mc1, bank;
    volatile __uint64_t mc = REMOTE_HUB_L(nasid, MD_MEMORY_CONFIG);

    mc &= ~MMC_DIMM0_SEL_MASK;
    mc |= ((__uint64_t) new_bank << MMC_DIMM0_SEL_SHFT);

    for (bank = 0; bank < MD_MEM_BANKS; bank += 2) {
	mc0 = (mc & MMC_BANK_MASK(bank)) >> MMC_BANK_SHFT(bank) ;
	mc1 = (mc & MMC_BANK_MASK(bank+1)) >> MMC_BANK_SHFT(bank+1) ;

	mc &= ~(MMC_BANK_MASK(bank));
	mc &= ~(MMC_BANK_MASK(bank+1));

	mc |= (mc0 << MMC_BANK_SHFT(bank+1));
	mc |= (mc1 << MMC_BANK_SHFT(bank));
    }

    REMOTE_HUB_S(nasid, MD_MEMORY_CONFIG, mc);
}

/* 
 * PV 669589.  If memory banks 0 and 1 are swapped, the correct behavior
 * is for the prom to also swap all other pairs of banks.  New versions of
 * the proms know how to do this.  The IO prom must be forwards and
 * backwards compatible with all versions of the CPU prom.  If we are
 * loading an older version of the IO prom, it expects the old behavior
 * (only banks 0 and 1 swapped).  The CPU prom must unswap the other banks
 * to create the expected behavior.  Newer versions of the IO prom know to
 * swap the other banks back to the new behavior.
 *
 * unswap_memory_nasid() is a supporting routine for
 * unswap_some_memory_banks() below (the latter is called by each CPU
 * prom)
 */ 

void
unswap_memory_nasid(nasid_t nasid, lboard_t *lb)
{
    __uint64_t          mc0, mc1; 
    int 		bank, tmpsz;
    volatile __uint64_t mc;
    klmembnk_t *	mem;

    /* Only unswap banks if they were originally swapped */
    mc = REMOTE_HUB_L(nasid, MD_MEMORY_CONFIG);
    if ((mc & MMC_DIMM0_SEL_MASK) >> MMC_DIMM0_SEL_SHFT) {

	/* 
	 * Don't reset the dimm0_sel field and only un-swap
	 * banks 2 and up.  Banks 0 and 1 remain swapped 
	 */

	for (bank = 2; bank < MD_MEM_BANKS; bank += 2) {
	    mc0 = (mc & MMC_BANK_MASK(bank)) >> MMC_BANK_SHFT(bank);
	    mc1 = (mc & MMC_BANK_MASK(bank+1)) >> 
		MMC_BANK_SHFT(bank+1) ;

	    mc &= ~(MMC_BANK_MASK(bank));
	    mc &= ~(MMC_BANK_MASK(bank+1));

	    mc |= (mc0 << MMC_BANK_SHFT(bank+1));
	    mc |= (mc1 << MMC_BANK_SHFT(bank));
	}

	REMOTE_HUB_S(nasid, MD_MEMORY_CONFIG, mc);

	/* 
	 * Also unswap the relevant klconfig info.  The old
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
	 * The new behavior has _all_ bank information in
	 * "locational" form.  In order to recreate the old
	 * behavior, banks 0 and 1 must be swapped back into
	 * software form.
	 */

	if (mem = (klmembnk_t *)find_component(lb, NULL, 
					       KLSTRUCT_MEMBNK)) {
	    tmpsz = mem->membnk_bnksz[0];
	    mem->membnk_bnksz[0] = mem->membnk_bnksz[1];
	    mem->membnk_bnksz[1] = tmpsz;
	}
    }
}
 

/* 
 * Called by each cpu prom to do any needed unswapping of memory bank
 * configuration.  See comment above in unswap_memory_nasid()
 */
void
unswap_some_memory_banks()
{
    __uint64_t          mc0, mc1; 
    int 		bank, tmpsz, i;
    volatile __uint64_t mc;
    lboard_t *		lb; 
    klmembnk_t *	mem;
    nasid_t		nasid = get_nasid();

    /* unswap the memory banks on our own node board */

    unswap_memory_nasid(nasid, (lboard_t *)find_lboard((lboard_t *)
						       KL_CONFIG_INFO(nasid),
						       KLTYPE_IP27));

     /* 
     * Global master must also correct the behavior on any headless nodes
     * it may have initialized.
     */

    if (hub_global_master()) {
	pcfg_t *p = PCFG(nasid);

	for (i = 0; i < p->count; i++) {
	    if (p->array[i].any.type == PCFG_TYPE_HUB &&
		IS_RESET_SPACE_HUB(&p->array[i].hub) &&
		(p->array[i].hub.flags & PCFG_HUB_HEADLESS)) {

		pcfg_hub_t *ph = pcfgGetHub(p,i);
		if (IS_HUB_MEMLESS(ph))
		    continue ;

		lb = (lboard_t *)find_lboard((lboard_t *)
					     KL_CONFIG_INFO(ph->nasid), 
					     KLTYPE_IP27);
		unswap_memory_nasid(get_actual_nasid(lb), lb);
	    }
	}
    }
}

int
ed_cpu_mem(nasid_t nasid,
           char *key,
           char *disable_str,
           char *reason,
           int temporary,
           int enable)
{
    char	buf[64], unit[64], mod[4], slot[16], *sn, *comp;
    int		rc, seqnum, module;

    if (strcmp(key, DISABLE_MEM_MASK)) {
        sn = CPU_LOG_SN;
        comp = "CPU";

        if (strcmp(key, DISABLE_CPU_A))
            strcpy(unit, "B");
        else
            strcpy(unit, "A");
    }
    else {
        sn = MEM_LOG_SN;
        comp = "MemBnk(s)";

        if (enable) {
            if (!disable_str || !strlen(disable_str))
                ip27log_getenv(nasid, key, unit, 0, 0);
            else {
                int     i, n = 0;
                char    dis[16];

                if (ip27log_getenv(nasid, key, dis, 0, 0) < 0)
                    strcpy(unit, disable_str);
                else {
                    for (i = 0; i < strlen(dis); i++)
                        if (!strchr(disable_str, dis[i]))
                            unit[n++] = dis[i];

                    unit[n] = 0;
                }
            }
        }
        else
            strcpy(unit, disable_str);
    }

    rc = ip27log_getenv(nasid, sn, buf, "0", 0);

    seqnum = atoi(buf);
    seqnum++;

    sprintf(buf, "%d", seqnum);
    rc = ip27log_setenv(nasid, sn, buf, 0);

    ip27log_getenv(nasid, IP27LOG_MODULE_KEY, mod, "0", 0);
    module = atoi(mod);
    get_slotname(get_node_slotid(nasid), slot);

    ip27log_printf_nasid(nasid, IP27LOG_INFO, "ED: %d: /hw/module/%d/slot/%s: "
	"%s %sabled %s %s Reason: %s\n", seqnum, module, slot, 
	(temporary ? "Temporarily" : "Permanently"), (enable ? "En" : "Dis"),
	comp, unit, reason);

    if (temporary) {
        if (!strcmp(comp, "CPU")) {
            REMOTE_HUB_S(nasid,
		strcmp(key, DISABLE_CPU_A) ? PI_CPU_ENABLE_B : PI_CPU_ENABLE_A,
		enable ? 1 : 0);
        }

        rc = 0;
    }
    else {
        if (enable && (!disable_str || !strlen(disable_str))) {
            rc = ip27log_unsetenv(nasid, key, IP27LOG_VERBOSE);
        }
        else {
            rc = ip27log_setenv(nasid, key, disable_str, IP27LOG_VERBOSE);
        }
    }

    return rc;
}

void
pcfg_syslog_info(pcfg_t *p)
{
        char            bank_sz[MD_MEM_BANKS+1] ;
        int             i ;
        pcfg_hub_t      *ph ;
        klinfo_t        *kliptr ;
        klmembnk_t      *mem_ptr ;
	int		dismem = 0, disable, bank;
	int		total_cpus = 0, dis_cpus = 0 ;
	lboard_t	*lb ;
	nasid_t		nasid ;
    	char        	mem_disable[MD_MEM_BANKS + 1], 
			disable_sz[MD_MEM_BANKS + 1];

        ForAllPcfg(p,i) {
                if (pcfgIsHub(p,i) && IS_RESET_SPACE_HUB(&p->array[i].hub)) {
                        ph = pcfgGetHub(p,i) ;
			if (IS_HUB_MEMLESS(ph))
				continue ;
			lb = (lboard_t *)find_lboard((lboard_t *)
					KL_CONFIG_INFO(ph->nasid), KLTYPE_IP27);
			if (!lb)
				continue ;
			do {

			nasid = get_actual_nasid(lb) ;
			kliptr = NULL ;
                	while(kliptr = find_component(lb, kliptr, 
				KLSTRUCT_CPU)) {
                        	total_cpus++ ;
                        	if (!(KLCONFIG_INFO_ENABLED(kliptr)))
                                	dis_cpus ++ ;
                	}

                	mem_ptr = (klmembnk_t *)find_component(lb,
                		NULL, KLSTRUCT_MEMBNK) ;
			if ((mem_ptr) && 
            			((!(mem_ptr->membnk_info.flags & KLINFO_ENABLE))
			      || (mem_ptr->membnk_info.flags & KLINFO_FAILED)))
				dismem += mem_ptr->membnk_memsz ;

        		disable = (ip27log_getenv(nasid, "DisableMemMask", 
					mem_disable, 0, 0) >= 0);
        		if (disable) {
            			ip27log_getenv(nasid, "DisableMemSz", 
				disable_sz, 0, 0);

            			for (bank = 0; bank < MD_MEM_BANKS; bank++)
                			if (strchr(mem_disable, '0' + bank)) {
                    				dismem += MD_SIZE_MBYTES(
						disable_sz[bank] - '0');
                		}
        		}

        		lb = KLCF_NEXT(lb) ;
        		if (lb)
                		lb = find_lboard(lb,KLTYPE_IP27);
        		else
                		break ;

        		} while (lb) ;
		}
	}
	if (dis_cpus)
        	ip27log_printf_nasid(get_nasid(), IP27LOG_INFO,
                	"SYS-DEGRADED: %d CPUs disabled\n", dis_cpus) ;

	if (dismem)
        	ip27log_printf_nasid(get_nasid(), IP27LOG_INFO,
                	"SYS-DEGRADED: %d MB Memory disabled\n", dismem) ;
}

void
make_hwg_path(moduleid_t m, slotid_t s, char *path)
{
    	char 	sname[SLOTNUM_MAXLENGTH] ;

    	get_slotname(s, sname) ;
	sprintf(path, "/hw/module/%d/slot/%s\0", m, sname) ;
}

static char *SysClkDivTbl[] = {
	"Rsvd",
	"1",
	"1.5",
	"2",
	"2.5",
	"3",
	"3.5",
	"4",
	"Rsvd",
	"Rsvd",
	"Rsvd",
	"Rsvd",
	"Rsvd",
	"Rsvd",
	"Rsvd",
	"Rsvd"
};

static char *SCClkDivTbl[] = {
	"Rsvd",
	"1",
	"1.5",
	"2",
	"2.5",
	"3",
	"Rsvd",
	"Rsvd"
};

void
dump_r10k_mode(__uint64_t mode, int config)
{
	int	tmp;

	printf("\t<2:0>   Kseg0CA\t\t= %d\n", (mode & R10000_KSEG0CA_MASK) >> 
		R10000_KSEG0CA_SHFT);
	printf("\t<4:3>   DevNum\t\t= %d\n", (mode & R10000_DEVNUM_MASK) >> 
		R10000_DEVNUM_SHFT);
	printf("\t<5>     CohPrcReqTar\t= %d\n", (mode & R10000_CRPT_MASK) >> 
		R10000_CRPT_SHFT);
	printf("\t<6>     PrcElmReq\t= %d\n", (mode & R10000_PER_MASK) >> 
		R10000_PER_SHFT);
	printf("\t<8:7>   PrcReqMax\t= %d\n", (mode & R10000_PRM_MASK) >> 
		R10000_PRM_SHFT);
	tmp = (mode & R10000_SCD_MASK) >> R10000_SCD_SHFT;
	printf("\t<12:9>  SysClkDiv\t= %d (%s)\n", tmp, SysClkDivTbl[tmp]);
	tmp = (mode & R10000_SCBS_MASK) >> R10000_SCBS_SHFT;
	printf("\t<13>    SCBlkSize\t= %d (%d)\n", tmp, (tmp ? 32 : 16));
	printf("\t<14>    SCCorEn\t\t= %d\n", (mode & R10000_SCCE_MASK) >> 
		R10000_SCCE_SHFT);
	printf("\t<15>    MemEnd\t\t= %d\n", (mode & R10000_ME_MASK) >> 
		R10000_ME_SHFT);
	tmp = (mode & R10000_SCS_MASK) >> R10000_SCS_SHFT;
	printf("\t<18:16> SCSize\t\t= %d (%d MB)\n", tmp, (1 << (tmp - 1)));
	tmp = (mode & R10000_SCCD_MASK) >> R10000_SCCD_SHFT;
	printf("\t<21:19> SCClkDiv\t= %d (%s)\n", tmp, SCClkDivTbl[tmp]);

	/*
	 * CONFIG register interesting bits end here
	 */
	if (config)
		return;

	printf("\t<28:25> SCClkTap\t= %d\n", (mode & R10000_SCCT_MASK) >> 
		R10000_SCCT_SHFT);
	printf("\t<30>    ODrainSys\t= %d\n", (mode & R10000_ODSYS_MASK) >> 
		R10000_ODSYS_SHFT);
	printf("\t<31>    CTM\t\t= %d\n", (mode & R10000_CTM_MASK) >> 
		R10000_CTM_SHFT);
}
