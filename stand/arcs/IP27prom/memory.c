#include <sys/types.h>
#include <sys/sbd.h>

#include <libkl.h>		/* For Hub 1 WAR */
#include <report.h>

#include <sys/SN/agent.h>
#include <sys/SN/kldiag.h>
#include <sys/SN/SN0/ip27log.h>

#include "libasm.h"
#include "ip27prom.h"
#include "mdir.h"
#include "mtest.h"
#include "memory.h"
#include "hub.h"
#include "libc.h"
#include "cache.h"

/*
 * memory_init
 *
 *   Processes an address range of uncached RAM, preparing it for use.
 *   Initializes the back door directory entries and ECC as required.
 *   Performs memory tests depending on the diag_mode.  If successful,
 *   clears the memory to zeroes.
 *
 *   name:         Descriptive name of area being initialized
 *   base:         Address of memory to test
 *   length:       Length of memory to test
 *	           (Internally rounded up to a multiple of 4096)
 *
 *   If anything goes wrong, an error message is printed and the routine
 *   returns -1.  Otherwise, it returns 0.
 *
 *   This routine locks the hub when outputting messages and displays
 *   the CPU letter with each message.
 */

#define MAXERR		4

int memory_init(char *name,
		__uint64_t base, __uint64_t length,
		int premium, int diag_mode)
{
    mtest_fail_t	fail;
    int			rc	= -1;
    __uint64_t		mc, ignore_ecc;
    nasid_t		nasid;

    nasid = (nasid_t) NASID_GET(base);

    /*
     * ECC must be temporarily disabled while testing and clearing memory.
     * If ECC is bad in memory and IGNORE_ECC is on, the bad ECC will
     * just be overwritten (which is what we want).  If IGNORE_ECC is off,
     * the bad ECC will be changed to 0xff (forced bad ECC) and an
     * uncached error will result.
     */

    mc = LD(REMOTE_HUB(nasid, MD_MEMORY_CONFIG));
    ignore_ecc = mc & MMC_IGNORE_ECC;
    if (ignore_ecc == 0)
	SD(REMOTE_HUB(nasid, MD_MEMORY_CONFIG), mc | MMC_IGNORE_ECC);

    length = (length + 0xfffUL) & ~0xfffUL;


    /*
     * If our CALIAS_SIZE is set, and we access some other nodes 
     * low memory, we seem to hang. Skip the first page for non-local
     * nodes (headless nodes) for now. The page is not used anyway.
     */
    if ((nasid != get_nasid()) && (TO_NODE_ADDRSPACE(base) == 0)) {
	    base += 0x1000;
	    length -= 0x1000;
    }

    hub_lock(HUB_LOCK_PRINT);
    db_printf("Init %s (0x%lx), len 0x%lx\n", name, base, length);
    hub_unlock(HUB_LOCK_PRINT);

    if (diag_mode != DIAG_MODE_NONE) {
	hub_lock(HUB_LOCK_PRINT);
	db_printf("\tTesting dir/prot\n");
	hub_unlock(HUB_LOCK_PRINT);

#if (!defined(SABLE) || SABLE_MEM_TEST)
	if (mtest_dir(premium,
		      base, base + length,
		      diag_mode, &fail, MAXERR) < 0)
	    goto done;
#endif /* SABLE */

#if 0
	hub_lock(HUB_LOCK_PRINT);
	db_printf("\tTesting ECC\n");
	hub_unlock(HUB_LOCK_PRINT);

#if (!defined(SABLE) || SABLE_MEM_TEST)
	if (mtest_ecc(premium,
		      base, base + length,
		      diag_mode, &fail, MAXERR) < 0)
	    goto done;
#endif /* SABLE */
#endif
    }

    hub_lock(HUB_LOCK_PRINT);
    db_printf("\tInitializing dir/prot\n");
    hub_unlock(HUB_LOCK_PRINT);

#if (!defined(SABLE) || SABLE_MEM_INIT)
    mdir_init_bddir(premium, base, base + length);
#endif /* SABLE */

#if 0
    hub_lock(HUB_LOCK_PRINT);
    db_printf("\tInitializing ECC\n");
    hub_unlock(HUB_LOCK_PRINT);

#if (!defined(SABLE) || SABLE_MEM_INIT)
    mdir_init_bdecc(premium, base, base + length);
#endif /* SABLE */
#endif

    if (diag_mode != DIAG_MODE_NONE) {
	hub_lock(HUB_LOCK_PRINT);
	db_printf("\tTesting memory\n");
	hub_unlock(HUB_LOCK_PRINT);

#if (!defined(SABLE) || SABLE_MEM_TEST)
	if (mtest(base, base + length, diag_mode, &fail, MAXERR) < 0)
	    goto done;
#endif /* SABLE */
    }

    hub_lock(HUB_LOCK_PRINT);
    db_printf("\tClearing memory\n");
    hub_unlock(HUB_LOCK_PRINT);

    /*
     * Clear memory with IGNORE_ECC to cause all ECC to be updated.
     */

    memset((char *) base, 0, length);

    if (((__uint64_t) base >> 56 & 0xff) == 0xa8)	/* XXX */
	cache_flush();

    /*
     * Reset IGNORE_ECC so that we actually catch any future memory errors.
     */

    if (ignore_ecc == 0)
	SD(REMOTE_HUB(nasid, MD_MEMORY_CONFIG), mc);

    mdir_error_get_clear(nasid, 0);

    rc = 0;

 done:

    return rc;
}

/*
 * memory_copy_prom
 *
 *   Prepare the area of memory where the PROM code (text/data) will be,
 *   and copy it there.  If memory tests are turned on, the area of memory
 *   is pre-tested, and the results of the copy are verified and summed.
 */

int memory_copy_prom(int diag_mode)
{
    mtest_fail_t	fail;
    __uint64_t		csum;
    int			premium;
    int			rc	= -1;
    nasid_t		nasid;
    void	       *prom, *ram;

    nasid	= get_nasid();
    premium	= (LD(REMOTE_HUB(nasid,
				 MD_MEMORY_CONFIG)) & MMC_DIR_PREMIUM) != 0;
    prom	= (void *) (nasid == get_nasid() ?
			    LBOOT_BASE : NODE_RBOOT_BASE(nasid));

    ram		= (void *) TO_NODE(nasid, K0_TO_K1(IP27PROM_BASE));

    if (memory_init("PROM text/data",
		    (__uint64_t) ram, ROUND4K(TEXT_LENGTH),
		    premium, diag_mode) < 0)
	goto done;

    db_printf("Copy PROM (0x%lx) to RAM (0x%lx), len 0x%lx\n",
	   prom, ram, TEXT_LENGTH);

    mdir_copy_prom(prom, ram, TEXT_LENGTH);

    if (diag_mode != DIAG_MODE_NONE) {
	db_printf("Verifying copy\n");

	if (mtest_compare_prom(prom, ram, TEXT_LENGTH, &fail) < 0) {
	    mtest_print_fail(&fail, 0, 0);
	    goto done;
	}

	db_printf("Calculating checksum from RAM (0x%lx), len 0x%lx\n",
	       ram, TEXT_LENGTH);

	csum = memsum(ram, TEXT_LENGTH);

	if ((csum & 0xff) != 0) {
	    /*
	     * Maybe the data was bad in the PROM to begin with.
	     * Check for this case.
	     */

	    printf("*** RAM checksum is incorrect (0x%02lx), sum=%y\n",
		   csum & 0xff, csum);

	    printf("Recalculating checksum from PROM (0x%lx), len 0x%lx\n",
		   prom, TEXT_LENGTH);

	    csum = memsum(prom, TEXT_LENGTH);

	    if ((csum & 0xff) == 0)
		result_fail("prom_checksum", KLDIAG_PROM_MEMCPY_BAD,
			    "PROM checksum is zero--memory copy bad.");
	    else {
		result_fail("prom_checksum", KLDIAG_PROM_CHKSUM_BAD,
			    "PROM checksum is incorrect.");
		printf("PROM checksum itself is incorrect (0x%02lx), sum=%y\n",
		csum & 0xff, csum);
	    }

	    goto done;
	}
    }

    db_printf("Done\n");

    rc = 0;

 done:

    return rc;
}

/*
 * memory_clear_prom
 *
 *   Prepare the area of memory where the PROM bss, stack, console
 *   structure, ELSC structure, and other important data will be
 *   (IP27PROM_INIT_START through IP27PROM_INIT_END), and zero the BSS.
 *
 *   This is a separate routine from memory_copy_prom so it doesn't have
 *   to be done while we're still running from the PROM (VERY slow).
 *
 *   If memory tests are turned on, the area of memory is pre-tested.
 */

int memory_clear_prom(int diag_mode)
{
    int			premium;
    int			rc	= -1;
    mtest_fail_t	fail;
    nasid_t		nasid;
    __uint64_t		sa, ea;

    nasid	= get_nasid();

    premium	= (LD(REMOTE_HUB(nasid,
				 MD_MEMORY_CONFIG)) & MMC_DIR_PREMIUM) != 0;

    /*
     * NB: Careful not to trash last page of text.  The beginning portion
     *     of BSS up to a 4k boundary has already been cleared since the
     *     text size was rounded up in memory_copy_prom.
     */

    sa		= TO_NODE(nasid, ROUND4K(K0_TO_K1(IP27PROM_BASE +
						  TEXT_LENGTH)));

    ea		= TO_NODE(nasid, ROUND4K(K0_TO_K1(IP27PROM_BASE +
						  TOTAL_LENGTH)));;

    if (memory_init("PROM bss",
		    sa, ea - sa,
		    premium, diag_mode) < 0)
	goto done;

    sa		= TO_NODE(nasid, K0_TO_K1(IP27PROM_INIT_START));
    ea		= TO_NODE(nasid, K0_TO_K1(IP27PROM_INIT_END));

    if (memory_init("PROM stack/structures",
		    sa, ea - sa,
		    premium, diag_mode) < 0)
	goto done;

    db_printf("Done\n");

    rc = 0;

 done:

    return rc;
}

/*
 * memory_init_low
 *
 *   Initializes all memory in the first N MB of memory, except for
 *   one hole where the PROM text/bss resides (at IP27PROM_BASE), and
 *   one hole where the stack and other important information resides
 *   (IP27PROM_INIT_START through IP27PROM_INIT_END).
 */

int memory_init_low(int diag_mode)
{
    int			premium;
    nasid_t		nasid;
    __uint64_t		sa, ea;

    nasid	= get_nasid();

    premium	= (LD(REMOTE_HUB(nasid,
				 MD_MEMORY_CONFIG)) & MMC_DIR_PREMIUM) != 0;

    sa		= TO_NODE(nasid, TO_CAC(0));
    ea		= TO_NODE(nasid, TO_CAC(IP27PROM_BASE));

    if (memory_init("bank 0 before text",
		    sa, ea - sa,
		    premium, diag_mode) < 0)
	return -1;

    sa		= TO_NODE(nasid, ROUND4K(TO_CAC(IP27PROM_BASE) + TOTAL_LENGTH));
    ea		= TO_NODE(nasid, TO_CAC(IP27PROM_INIT_START));

    if (memory_init("bank 0 end of BSS to structs",
		    sa, ea - sa,
		    premium, diag_mode) < 0)
	return -1;

    sa		= TO_NODE(nasid, TO_CAC(IP27PROM_INIT_END));
    ea		= TO_NODE(nasid, TO_CAC(MIN_BANK_SIZE));

    if (memory_init("bank 0 end of structs to " MIN_BANK_STRING " MB",
		    sa, ea - sa,
		    premium, diag_mode) < 0)
	return -1;

    return 0;
}

/*
 * memory_init_all
 *
 *   Initializes all memory that's configured in the MD_MEMORY_CONFIG
 *   register.  If skip_low is true, skips the first N megabytes of
 *   bank 0, assuming it has already been initialized.
 *   Skips banks specified in mem_disable. This is the string assigned
 *   to the plog variable 'DisableMemMask'.
 *   Appends bad banks to the end of mem_disable, if any
 *   Also checks for the special 256 MB case
 */

int memory_init_all(nasid_t nasid, int skip_low, char *mem_disable, int diag_mode)
{
    int			n, bank, sw_bank, bad = 0;
    int			premium, dimm0_sel;
    int 		holed;

    premium	= (LD(REMOTE_HUB(nasid,
				 MD_MEMORY_CONFIG)) & MMC_DIR_PREMIUM) != 0;

    n = strlen(mem_disable);

    dimm0_sel = MDIR_DIMM0_SEL(nasid);

    /*
     * Initialize the rest of memory, including the rest of bank 0
     * and all of the other banks present.
     */

    for (bank = 0; bank < MD_MEM_BANKS; bank++) {
	__uint64_t	init_base, init_size;
	char		msg[40];

	printf(".");

	/* 
	 * skip_low refers to the memory at physical address 0.  The bank
	 * which corresponds to physical address 0 is not necessarily the
	 * same as the bank in DIMM slot 0 (location 0).
	 */
	sw_bank = bank ^ (dimm0_sel / 2);

	if (strchr(mem_disable, '0' + bank) && !(DIP_NODISABLE()))
	    continue;

	mdir_bank_get(nasid, bank, &init_base, &init_size);
        holed = (init_size == 256 * 0x100000) ? 1 : 0;

	if (skip_low && sw_bank == 0) {
	    init_base += MIN_BANK_SIZE;
	    init_size -= MIN_BANK_SIZE;
	}

	if ((__int64_t) init_size > 0) {
	    if (skip_low && sw_bank == 0)
		sprintf(msg, "bank %d after %s MB", bank, MIN_BANK_STRING);
	    else
		sprintf(msg, "bank %d", bank);

	   if(!holed) {
	    if (memory_init(msg,
			    TO_NODE_CAC(nasid, init_base),
			    init_size,
			    premium, diag_mode) < 0) {

                bad = 1;
		mem_disable[n++] = '0' + bank;
            }
           } else {
    	    int 		cpu_speed;

	    /*
	     * Need to know if this is an IP27 or IP31 board when
	     * testing for 256MB dimm banks.  256MB dimms are
	     * not supported on IP27 node boards and should
	     * be disabled if found.  This check is here to 
	     * handle the headless node cases.  In the normal case
	     * this bank will have already been disabled prior to
	     * to running mtest_basic().
	     */
	    cpu_speed = (int)(IP27CONFIG_NODE(nasid).freq_cpu / 1000000);

	    if (cpu_speed == 180 || cpu_speed == 195) {  /* IP27 node board */
	     	int module;
		int slot = get_node_slotid(nasid);

		if (nasid != get_nasid()) {
		   elsc_t              remote_elsc;
		   elsc_init(&remote_elsc, nasid);
		   module = elsc_module_get(&remote_elsc);
		} else {
	   	   module = GET_FIELD(LOCAL_HUB_L(NI_SCRATCH_REG1),ADVERT_MODULE);
		}
		ip27log_printf(IP27LOG_INFO, "*** WARNING: module %d slot %d: Memory Bank %d: 256 MB dimm banks not allowed on IP27 node boards\n",
			module, slot, bank);
		printf("\n*** WARNING: Module %d Slot %d: Memory Bank %d: 256 MB dimm banks not allowed on IP27 node boards\n",
			module, slot, bank);
		if (!(DIP_NODISABLE())) {
			printf("*** WARNING: Disabling memory bank %d in module %d slot %d\n",
				bank, module, slot);
                }

		bad = 1;
		mem_disable[n++] = '0' + bank;

		continue;
	    }

	    if ((memory_init(msg,
			    TO_NODE_CAC(nasid, init_base),
			    init_size - 3*64* 0x100000,
			    premium, diag_mode) < 0) || 
	     (memory_init(msg,
			    TO_NODE_CAC(nasid, init_base + init_size - 2*64* 0x100000),
			    64 * 0x100000,
			    premium, diag_mode) < 0) || 
	     (memory_init(msg,
			    TO_NODE_CAC(nasid, init_base + init_size),
			    64 * 0x100000,
			    premium, diag_mode) < 0) || 
	     (memory_init(msg,
			    TO_NODE_CAC(nasid, init_base + init_size + 2*64* 0x100000),
			    64 * 0x100000,
			    premium, diag_mode) < 0)) {

                bad = 1;
		mem_disable[n++] = '0' + bank;
            }
	  }
	}
    }

    mem_disable[n] = 0;

    if (bad)
        return -1;
    else
        return 0;
}

static int
my_cmp(char *str1, char *str2)
{
    int         i, s1;

    if ((s1 = strlen(str1)) != strlen(str2))
        return 1;

    for (i = 0; i < s1; i++)
        if (!strchr(str2, str1[i]))
            return 1;

    return 0;
}

/*
 * set_mem_plog
 *     Must be called after every call to memory_disable
 *     readjusts all memory related promlog variables
 */

int set_mem_plog(nasid_t nasid, char *disable_mask)
{
    char	dis_bank_size[MD_MEM_BANKS + 1], 
		dis_reason[MD_MEM_BANKS + 1],
		bank_sz[MD_MEM_BANKS],
		old_dis_mask[MD_MEM_BANKS + 1],
		bank_str[MD_MEM_BANKS + 1] ;
    int		bank, rc, dimm0_sel ;
    int 	cpu_speed;
    

    ip27log_getenv(nasid, DISABLE_MEM_SIZE, dis_bank_size, "00000000", 0);
    ip27log_getenv(nasid, DISABLE_MEM_REASON, dis_reason, "00000000", 0);

    dis_bank_size[MD_MEM_BANKS] = 0;
    dis_reason[MD_MEM_BANKS] = 0;

    mdir_sz_get(nasid, bank_sz);

    dimm0_sel = MDIR_DIMM0_SEL(nasid);

    ip27log_getenv(nasid, DISABLE_MEM_MASK, old_dis_mask, 0, 0);

    cpu_speed = (int)(IP27CONFIG_NODE(nasid).freq_cpu / 1000000);

    for (bank = 0; bank < MD_MEM_BANKS; bank++) {
	int             is256MB = 0;
	__uint64_t      base, length;

        if (strchr(disable_mask, '0' + bank)) {
            if (strchr(old_dis_mask, '0' + bank))
                dis_bank_size[bank] = dis_bank_size[bank];
            else
                dis_bank_size[bank] = '0' + bank_sz[bank];


	    is256MB = (MD_SIZE_MBYTES(bank_sz[bank]) == 256) ? 1 : 0;

	    /*
	     * check for disabled 256MB dimm on IP27 board
	     */
	    if (is256MB && (cpu_speed == 180 || cpu_speed == 195)) { 
                   dis_reason[bank] = '0' + MEM_DISABLE_256MB;
	    } else if (dis_reason[bank] == '0') {
                   dis_reason[bank] = '0' + MEM_DISABLE_AUTO;
	    }
	} else {
	    /* Clean up old disable state.  We only care about banks
	     * listed in disable_mask, so clear any others. */
	    dis_bank_size[bank] = '0';
	    dis_reason[bank] = '0';
	}
    }

    if (strlen(disable_mask)) {
	if (my_cmp(disable_mask, old_dis_mask)) {
	    rc = ed_cpu_mem(nasid, DISABLE_MEM_MASK, disable_mask,
			    get_diag_string(KLDIAG_MEMTEST_FAILED), 0, 0);
	}
	ip27log_setenv(nasid, DISABLE_MEM_SIZE, dis_bank_size, 0);
	ip27log_setenv(nasid, DISABLE_MEM_REASON, dis_reason, 0);
    } else {
	ip27log_unsetenv(nasid, DISABLE_MEM_MASK, 0);
	ip27log_unsetenv(nasid, DISABLE_MEM_SIZE, 0);
	ip27log_unsetenv(nasid, DISABLE_MEM_REASON, 0);
    }

    if (dimm0_sel >= 2) {
        sprintf(bank_str, "%d", dimm0_sel);
        ip27log_setenv(nasid, SWAP_BANK, bank_str, 0);
    }

    return rc ;
}

/*
 * memory_disable
 * disable the banks specified in the disable_mask string
 * by making the size of the bank in MD_MEMORY_CONFIG register 0.
 * if strlen of 'disable_mask' is 0, UNSET all 3 mem config plog vars.
 * else SET the 3 plog vars.
 * To prevent blowing away of any previously set DisableMemMask 
 * string, the caller needs to always provide the current value of 
 * DisableMemMask in disable_mask.
 */

int memory_disable(nasid_t nasid, char *disable_mask)
{
    int 	bank, rc;

    rc = set_mem_plog(nasid, disable_mask) ;

    for (bank = 0; bank < MD_MEM_BANKS; bank++)
        if (strchr(disable_mask, '0' + bank))
            mdir_disable_bank(nasid, bank);

    return rc;
}

int memory_empty_enable(nasid_t nasid)
{
    char        disable_mask[MD_MEM_BANKS + 1],
                mem_dis_sz[MD_MEM_BANKS + 1],
                tmp_mem_disable[MD_MEM_BANKS + 1];
    int         i, n = 0, rc = 0;

    if (ip27log_getenv(nasid, "DisableMemMask", tmp_mem_disable, 0, 0) < 0)
        return 0;

    ip27log_getenv(nasid, "DisableMemSz", mem_dis_sz, "00000000", 0);

    disable_mask[0] = 0;
    for (i = 0; i < strlen(tmp_mem_disable); i++)
        if (mem_dis_sz[tmp_mem_disable[i] - '0'] - '0' != MD_SIZE_EMPTY)
            disable_mask[n++] = tmp_mem_disable[i];

    if (strlen(disable_mask) != strlen(tmp_mem_disable)) {
        disable_mask[n] = 0;
        rc = ed_cpu_mem(nasid, "DisableMemMask", disable_mask,
                "Enabling empty bank(s)", 0, 1);
    }

    return rc;
}
