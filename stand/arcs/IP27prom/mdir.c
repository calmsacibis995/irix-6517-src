/***********************************************************************\
*	File:		mdir.c						*
*									*
*	Contains code for initializing and managing the memory and	*
*	directory section of the SN0 hub.				*
*									*
\***********************************************************************/

#ident "$Revision: 1.84 $"

#define TRACE_FILE_NO		2

#include <sys/types.h>
#include <sys/sbd.h>

#include <libkl.h>
#include <rtc.h>
#include <sys/SN/SN0/ip27log.h>

#include "ip27prom.h"
#include "mdir.h"
#include "libasm.h"
#include "hub.h"
#include "pod.h"
#include "prom_leds.h"

#if defined(RTL) || defined(GATE)
#define RTL_GATE	1
#else
#define RTL_GATE	0
#endif

#define VERBOSE		0

/*
 * Syndrome to bit mapping
 *
 *   A syndrome of zero indicates no errors.  A syndrome in the table below
 *   indicates a single check bit or data bit correctable error.  A syndrome
 *   not in the table indicates a multiple-bit uncorrectable error.
 */

typedef struct synent_s {
    uchar_t		syn, bit;
} synent_t;

static synent_t synmap_mem[] = {
    /* Data bits */
    { 0xc8, 71 }, { 0xc4, 70 }, { 0xc2, 69 }, { 0xc1, 68 },
    { 0xf4, 67 }, { 0x8f, 66 }, { 0xe0, 65 }, { 0xb0, 64 },
    { 0x0e, 63 }, { 0x0b, 62 }, { 0xf2, 61 }, { 0x1f, 60 },
    { 0x86, 59 }, { 0x46, 58 }, { 0x26, 57 }, { 0x16, 56 },
    { 0x38, 55 }, { 0x34, 54 }, { 0x32, 53 }, { 0x31, 52 },
    { 0xa8, 51 }, { 0xa4, 50 }, { 0xa2, 49 }, { 0xa1, 48 },
    { 0x98, 47 }, { 0x94, 46 }, { 0x92, 45 }, { 0x91, 44 },
    { 0x58, 43 }, { 0x54, 42 }, { 0x52, 41 }, { 0x51, 40 },
    { 0x8a, 39 }, { 0x4a, 38 }, { 0x2a, 37 }, { 0x1a, 36 },
    { 0x89, 35 }, { 0x49, 34 }, { 0x29, 33 }, { 0x19, 32 },
    { 0x85, 31 }, { 0x45, 30 }, { 0x25, 29 }, { 0x15, 28 },
    { 0x8c, 27 }, { 0x4c, 26 }, { 0x2c, 25 }, { 0x1c, 24 },
    { 0x68, 23 }, { 0x64, 22 }, { 0x62, 21 }, { 0x61, 20 },
    { 0xf8, 19 }, { 0x4f, 18 }, { 0x70,	17 }, { 0xd0, 16 },
    { 0x07, 15 }, { 0x0d, 14 }, { 0xf1,	13 }, { 0x2f, 12 },
    { 0x83, 11 }, { 0x43, 10 }, { 0x23,	 9 }, { 0x13,  8 },
    /* Check bits */
    { 0x80, 128 | 7 }, { 0x40, 128 | 6 }, { 0x20, 128 | 5 },
    { 0x10, 128 | 4 }, { 0x08, 128 | 3 }, { 0x04, 128 | 2 },
    { 0x02, 128 | 1 }, { 0x01, 128 | 0 },
    /* End marker */
    { 0x00,  0 },
};

static synent_t synmap_pdir[] = {
    /* Data bits */
    { 0x70, 47 }, { 0x0b, 46 }, { 0x07, 45 }, { 0x43, 44 },
    { 0x7c, 43 }, { 0x1f, 42 }, { 0x45, 41 }, { 0x4c, 40 },
    { 0x51, 39 }, { 0x61, 38 }, { 0x46, 37 }, { 0x4a, 36 },
    { 0x64, 35 }, { 0x23, 34 }, { 0x73, 33 }, { 0x6e, 32 },
    { 0x25, 31 }, { 0x29, 30 }, { 0x31, 29 }, { 0x26, 28 },
    { 0x2a, 27 }, { 0x62, 26 }, { 0x2c, 25 }, { 0x1a, 24 },
    { 0x5d, 23 }, { 0x2f, 22 }, { 0x13, 21 }, { 0x32, 20 },
    { 0x52, 19 }, { 0x16, 18 }, { 0x34, 17 }, { 0x54, 16 },
    { 0x15, 15 }, { 0x68, 14 }, { 0x0d, 13 }, { 0x1c, 12 },
    { 0x19, 11 }, { 0x0e, 10 }, { 0x38,	 9 }, { 0x58,  8 },
    { 0x49,  7 },
    /* Check bits */
    { 0x40, 128 | 6 }, { 0x20, 128 | 5 }, { 0x10, 128 | 4 },
    { 0x08, 128 | 3 }, { 0x04, 128 | 2 }, { 0x02, 128 | 1 },
    { 0x01, 128 | 0 },
    /* End marker */
    { 0x00,  0 },
};

static synent_t synmap_sdir[] = {
    /* Data bits */
    { 0x1f, 15 }, { 0x1a, 14 }, { 0x13, 13 }, { 0x07, 12 },
    { 0x0e, 11 }, { 0x0b, 10 }, { 0x16,	 9 }, { 0x15,  8 },
    { 0x19,  7 }, { 0x1c,  6 }, { 0x0d,	 5 },
    /* Check bits */
    { 0x10, 128 | 4 }, { 0x08, 128 | 3 }, { 0x04, 128 | 2 },
    { 0x02, 128 | 1 }, { 0x01, 128 | 0 },
    /* End marker */
    { 0x00,  0 },
};

static int size_check_256mb(__uint64_t base, int premium);
/*
 * mdir_xlate_syndrome
 *
 *   Translates a syndrome into a string. Input "type" should be 0 for
 *   memory, 1 for premium directory, or 2 for standard directory.
 *
 *   - For no errors, says "good".
 *
 *   - For a single bit error in the data, says "data00" to "data63", "dir00"
 *     to "dir40", or "dir00" to "dir10", according to type 0, 1, or 2,
 *     respectively.
 *
 *   - For a single bit error in check bits, says "check0" to "check7",
 *     "check0" to "check6", or "check0" to "check4", according to type
 *     0, 1, or 2, respectively.
 *
 *   - For a multiple-bit uncorrectable error, says "multi".
 *
 *   Returns the SIMM bit number for single bit memory errors (71 to 0).
 */

int mdir_xlate_syndrome(char *buf, int type, __uint64_t syn)
{
    int			r	= -1;
    synent_t	       *table;
    char	       *bitname;
    int			bitnum, bit;
    __uint64_t		forced, x;

    switch (type) {
    case 0:
	table = synmap_mem;
	bitname = "data";
	bitnum = 8;
	forced = 0xff;
	break;
    case 1:
	table = synmap_pdir;
	bitname = "dir";
	bitnum = 7;
	forced = 0x7f;
	break;
    case 2:
	table = synmap_sdir;
	bitname = "dir";
	bitnum = 5;
	forced = 0x1f;
	break;
    }

    sprintf(buf, "0x%02llx (", syn);
    while (*buf)
	buf++;

    if (syn == 0)
	strcpy(buf, "good");
    else if (syn == forced)
	strcpy(buf, "forced_multi");
    else {
	int		i;

	for (i = 0; (x = (__uint64_t) LBYTEU(&table[i].syn)) != 0; i++)
	    if (x == syn)
		break;

	bit = LBYTE(&table[i].bit);

	if (x == 0)
	    strcpy(buf, "multi");
	else if (bit & 128) {
	    r = bit & 127;
	    sprintf(buf, "check_%d", r);
	} else {
	    r = bit;
	    sprintf(buf, "%s_%02d", bitname, bit - bitnum);
	}
    }

    strcat(buf, ")");

    return r;
}

/*
 * mdir_xlate_addr_mem
 *
 *   Expects a physical memory address and a SIMM bit from 71:0, where bits
 *   71:8 correspond to data 63:0 and 7:0 correspond to the check bits.
 */

void mdir_xlate_addr_mem(char *buf, __uint64_t address, int bit)
{
    __uint64_t		bank_dimm, bank_lgcl, bitbase;

    bank_dimm = GET_FIELD(address, MD_BANK);
    bank_lgcl = (bit >= 36);

    bitbase = (((address & 8) ? 18 : 0) +
	       (bit >= 18 && bit < 36 || bit >= 54 ? 36 : 0));

    if (SN00) {
	if (bit < 0)
	    sprintf(buf, "SLOT%lld and/or SLOT%lld",
		    bank_dimm * 2 + 1, bank_dimm * 2 + 2);
	else
	    sprintf(buf, "SLOT%lld line %lld",
		    bank_dimm * 2 + bank_lgcl + 1,
		    bitbase + (bit % 18));
    } else {
	if (bit < 0)
	    sprintf(buf, "MM%cH%c and/or MM%cL%c",
		    bank_dimm < 4 ? 'X' : 'Y',
		    (int) (bank_dimm + '0'),
		    bank_dimm < 4 ? 'X' : 'Y',
		    (int) (bank_dimm + '0'));
	else
	    sprintf(buf, "MM%c%c%c line %lld",
		    bank_dimm < 4 ? 'X' : 'Y',
		    bank_lgcl ? 'H' : 'L',
		    (int) (bank_dimm + '0'),
		    bitbase + (bit % 18));
    }
}

/*
 * mdir_xlate_addr_pdir
 *
 *   Expects a physical memory address and a SIMM bit from 47:0, where bits
 *   47:7 correspond to data 40:0 and 6:0 correspond to the check bits.
 */

void mdir_xlate_addr_pdir(char *buf, __uint64_t address, int bit,
			  int dimm0_sel)
{
    __uint64_t		bank_dimm, bank_phys;

#ifdef N_MODE
    bank_dimm = (address >> 27) & 3;
#else
    bank_dimm = (address >> 27) & 7;
#endif

    bank_phys = (address >> 20 ^ address >> 18 ^
		 address >> 10 ^ dimm0_sel >> 1) & 1;

    if (bit < 0)
	sprintf(buf, "MM%c%c%c and/or DIR%c%c",
		bank_dimm < 4 ? 'X' : 'Y',
		bank_phys ? 'H' : 'L',
		(int) (bank_dimm + '0'),
		bank_dimm < 4 ? 'X' : 'Y',
		(int) (bank_dimm + '0'));
    else if (bit < 16)
	sprintf(buf, "MM%c%c%c line %d",
		bank_dimm < 4 ? 'X' : 'Y',
		bank_phys ? 'H' : 'L',
		(int) (bank_dimm + '0'),
		bit);
    else
	sprintf(buf, "DIR%c%c line %d",
		bank_dimm < 4 ? 'X' : 'Y',
		(int) (bank_dimm + '0'),
		bit - 16);
}

/*
 * mdir_xlate_addr_sdir
 *
 *   Expects a physical memory address and a SIMM bit from 15:0, where bits
 *   15:5 correspond to data 10:0 and 4:0 correspond to the check bits.
 */

void mdir_xlate_addr_sdir(char *buf, __uint64_t address, int bit,
			  int dimm0_sel)
{
    __uint64_t		bank_dimm, bank_phys;

#ifdef N_MODE
    bank_dimm = (address >> 27) & 3;
#else
    bank_dimm = (address >> 27) & 7;
#endif

    bank_phys = (address >> 20 ^ address >> 18 ^
		 address >> 10 ^ dimm0_sel >> 1) & 1;

    if (SN00) {
	if (bit < 0)
	    sprintf(buf, "SLOT%lld",
		    bank_dimm * 2 + bank_phys + 1);
	else
	    sprintf(buf, "SLOT%lld line %d",
		    bank_dimm * 2 + bank_phys + 1,
		    bit);
    } else {
	if (bit < 0)
	    sprintf(buf, "MM%c%c%c",
		    bank_dimm < 4 ? 'X' : 'Y',
		    bank_phys ? 'H' : 'L',
		    (int) (bank_dimm + '0'));
	else
	    sprintf(buf, "MM%c%c%c line %d",
		    bank_dimm < 4 ? 'X' : 'Y',
		    bank_phys ? 'H' : 'L',
		    (int) (bank_dimm + '0'),
		    bit);
    }
}

/*
 * mdir_init
 *
 *   Initializes the memory/directory section of the hub.
 */

#define MEM_SIMM_MODE		0x0029
#define DIR_SIMM_MODE		0x0029
#define REFRESH_NS		15620		/* Refresh time (nanosec) */

void mdir_init(nasid_t nasid, __uint64_t freq_hub)
{
    __uint64_t		simm, v, thresh;

    hub_led_set(PLED_MDIRINIT);

    /*
     * Set mode bits on each bank.  These registers are supposed to have
     * correct hardware interlock.  However, the current hub version hangs
     * without the delays, which are supposed to be at least 100 clocks.
     */

    for (simm = 0; simm < MD_MEM_BANKS * 2; simm++) {
	SD(REMOTE_HUB(nasid, MD_MEM_DIMM_INIT),
	   simm << MDI_SELECT_SHFT | MEM_SIMM_MODE);
	SD(REMOTE_HUB(nasid, MD_DIR_DIMM_INIT),
	   simm << MDI_SELECT_SHFT | DIR_SIMM_MODE);
    }

    /*
     * Turn on refresh.  We need to complete at least two refresh cycles
     * before doing any DRAM accesses.  We could do the first two cycles
     * with a fast threshold if we wanted to speed this up.  But instead
     * we'll just delay about 10 refresh cycles and not worry about it.
     */

    thresh = REFRESH_NS * freq_hub / 1000000000;

    SD(REMOTE_HUB(nasid, MD_REFRESH_CONTROL), MRC_ENABLE | thresh);

    rtc_sleep(10 * REFRESH_NS / 1000);

    /*
     * Initialize MOQ_SIZE
     */

    LD(REMOTE_HUB(nasid, MD_MOQ_SIZE));			/* XXX debug */
    SD(REMOTE_HUB(nasid, MD_MOQ_SIZE), MMS_RESET_DEFAULTS);

}

void mdir_error_get_clear(nasid_t nasid, hub_mderr_t *err)
{
    hub_mderr_t		e;

    e.hm_dir_err   = LD(REMOTE_HUB(nasid, MD_DIR_ERROR_CLR));
    e.hm_mem_err   = LD(REMOTE_HUB(nasid, MD_MEM_ERROR_CLR));
    e.hm_proto_err = LD(REMOTE_HUB(nasid, MD_PROTOCOL_ERROR_CLR));
    e.hm_misc_err  = LD(REMOTE_HUB(nasid, MD_MISC_ERROR_CLR));

    if (err)
	*err = e;
}

/*
 * mdir_error_check
 *
 *   Checks for MD errors and returns the number of them found.
 *   If err is NULL, the real MD registers are used.
 */

int mdir_error_check(nasid_t nasid, hub_mderr_t *err)
{
    md_dir_error_t	d;
    md_mem_error_t	m;
    md_proto_error_t	p;
    __uint64_t		x;
    int			count = 0;

    d.derr_reg = err ? err->hm_dir_err : LD(REMOTE_HUB(nasid, MD_DIR_ERROR));

    if (d.derr_fmt.uce_vld || d.derr_fmt.ae_vld || d.derr_fmt.ce_vld)
	count++;

    m.merr_reg = err ? err->hm_mem_err : LD(REMOTE_HUB(nasid, MD_MEM_ERROR));

    if (m.merr_fmt.uce_vld || m.merr_fmt.ce_vld)
	count++;

    p.perr_reg = err ? err->hm_proto_err : LD(REMOTE_HUB(nasid, MD_PROTOCOL_ERROR));

    if (p.perr_fmt.valid)
	count++;

    x = err ? err->hm_misc_err : LD(REMOTE_HUB(nasid, MD_MISC_ERROR));

    if ((x >> MMCE_ILL_MSG_SHFT	  ) & 2) count++;
    if ((x >> MMCE_ILL_REV_SHFT	  ) & 2) count++;
    if ((x >> MMCE_LONG_PACK_SHFT ) & 2) count++;
    if ((x >> MMCE_SHORT_PACK_SHFT) & 2) count++;
    if ((x >> MMCE_BAD_DATA_SHFT  ) & 2) count++;

    return count;
}

/*
 * mdir_error_decode
 *
 *   Decode MD errors and prints a summary using printf.
 *   If err is NULL, the real MD registers are used.
 *   Returns the number of errors decoded.
 */

static char *ovr[2] = { ", with overrun", "" };

int mdir_error_decode(nasid_t nasid, hub_mderr_t *err)
{
    char		buf[32];
    md_dir_error_t	d;
    md_mem_error_t	m;
    md_proto_error_t	p;
    __uint64_t		mc, x, ha;
    int			count = 0;
    int			bit, premium, dimm0_sel;

    mc		= LD(REMOTE_HUB(nasid, MD_MEMORY_CONFIG));
    premium	= (int) GET_FIELD(mc, MMC_DIR_PREMIUM);
    dimm0_sel	= (int) GET_FIELD(mc, MMC_DIMM0_SEL);

    d.derr_reg = (err ? err->hm_dir_err :
		  LD(REMOTE_HUB(nasid, MD_DIR_ERROR)));

    ha = d.derr_fmt.hspec_addr << 3;

    /*
     * Workaround:  If the directory error HSPEC address appears to be
     * munged due to Hub errata #402537, compensate by de-translating.
     */

    if ((ha & 0xf0000000) == 0x30000000)
	ha = BDADDR_IS_PRT(ha) ? BDPRT_TO_MEM(ha) : BDDIR_TO_MEM(ha);

    if (d.derr_fmt.uce_vld) {		/* Handle in priority order */
	printf("Uncorrectable directory ECC error%s\n",
	       ovr[d.derr_fmt.uce_ovr]);
	printf("   HSPEC address: 0x%lx\n", ha);
	bit = mdir_xlate_syndrome(buf, 2 - premium, d.derr_fmt.bad_syn);
	printf("   Bad syndrome : %s\n", buf);
	(premium ? mdir_xlate_addr_pdir : mdir_xlate_addr_sdir)
	    (buf, ha, bit, dimm0_sel);
	printf("   Physical loc : %s\n", buf);
	count++;
    } else if (d.derr_fmt.ae_vld) {
	printf("Access rights ECC error%s\n",
	       ovr[d.derr_fmt.ae_ovr]);
	printf("   HSPEC address: 0x%lx\n", ha);
	printf("   Bad prot bits: 0x%lx\n", d.derr_fmt.bad_prot);
	count++;
    } else if (d.derr_fmt.ce_vld) {
	printf("Correctable directory ECC error%s\n",
	       ovr[d.derr_fmt.ae_ovr]);
	printf("   HSPEC address: 0x%lx\n", ha);
	bit = mdir_xlate_syndrome(buf, 2 - premium, d.derr_fmt.bad_syn);
	printf("   Bad syndrome : %s\n", buf);
	(premium ? mdir_xlate_addr_pdir : mdir_xlate_addr_sdir)
	    (buf, ha, bit, dimm0_sel);
	printf("   Physical loc : %s\n", buf);
	count++;
    }

    m.merr_reg = (err ? err->hm_mem_err :
		  LD(REMOTE_HUB(nasid, MD_MEM_ERROR)));

    if (m.merr_fmt.uce_vld) {		/* Handle in priority order */
	printf("Uncorrectable memory ECC error%s\n",
	       ovr[m.merr_fmt.uce_ovr]);
	printf("   Address      : 0x%lx\n", m.merr_fmt.address << 3);
	bit = mdir_xlate_syndrome(buf, 0, m.merr_fmt.bad_syn);
	printf("   Bad syndrome : %s\n", buf);
	mdir_xlate_addr_mem(buf, m.merr_fmt.address << 3, bit);
	printf("   Physical loc : %s\n", buf);
	count++;
    } else if (m.merr_fmt.ce_vld) {
	printf("Correctable memory ECC error%s\n",
	       ovr[m.merr_fmt.ce_ovr]);
	printf("   Address      : 0x%lx\n", m.merr_fmt.address << 3);
	bit = mdir_xlate_syndrome(buf, 0, m.merr_fmt.bad_syn);
	printf("   Bad syndrome : %s\n", buf);
	mdir_xlate_addr_mem(buf, m.merr_fmt.address << 3, bit);
	printf("   Physical loc : %s\n", buf);
	count++;
    }

    p.perr_reg = (err ? err->hm_proto_err :
		  LD(REMOTE_HUB(nasid, MD_PROTOCOL_ERROR)));

    if (p.perr_fmt.valid) {
	printf("Memory protocol error%s\n",
	       ovr[p.perr_fmt.overrun]);
	printf("   Address      : 0x%lx (see MD_PROTOCOL_ERROR)\n",
	       p.perr_fmt.address << 3);
	count++;
    }

    x = (err ? err->hm_misc_err :
	 LD(REMOTE_HUB(nasid, MD_MISC_ERROR)));

    if ((x >> MMCE_ILL_MSG_SHFT) & 2) {
	printf("Illegal Message error%s\n",
	       ovr[x >> MMCE_ILL_MSG_SHFT & 1]);
	count++;
    }

    if ((x >> MMCE_ILL_REV_SHFT) & 2) {
	printf("Illegal Revision Address error%s\n",
	       ovr[x >> MMCE_ILL_REV_SHFT & 1]);
	count++;
    }

    if ((x >> MMCE_LONG_PACK_SHFT) & 2) {
	printf("Message Too Long error%s\n",
	       ovr[x >> MMCE_LONG_PACK_SHFT & 1]);
	count++;
    }

    if ((x >> MMCE_SHORT_PACK_SHFT) & 2) {
	printf("Message Too Short error%s\n",
	       ovr[x >> MMCE_SHORT_PACK_SHFT & 1]);
	count++;
    }

    if ((x >> MMCE_BAD_DATA_SHFT) & 2) {
	printf("Wrote Data and Marked Bad error%s\n",
	       ovr[x >> MMCE_BAD_DATA_SHFT & 1]);
	count++;
    }

    return count;
}

/*
 * mdir_init_bddir
 *
 *   Initializes the dir/prot memory through the back door, for a range
 *   of memory, using assembly routines for speed.
 *
 *   premium: true if predetermined directory type is premium
 *   saddr: page-aligned start physical address in back door memory
 *   eaddr: page-aligned end physical address (exclusive) in back door memory
 */

void
mdir_init_bddir(int premium, __uint64_t saddr, __uint64_t eaddr)
{
#if VERBOSE
    printf("mibd(%d,%lx,%lx)\n", premium, saddr, eaddr);
#endif

    TRACE(premium);
    TRACE(saddr);
    TRACE(eaddr);

    mdir_bddir_fill((__uint64_t) BDPRT_ENTRY(saddr, 0),
		    (__uint64_t) BDPRT_ENTRY(eaddr - 0x1000, 0) + 0x400,
		    premium ? MD_PDIR_INIT_LO	: MD_SDIR_INIT_LO,
		    premium ? MD_PDIR_INIT_HI	: MD_SDIR_INIT_HI,
		    premium ? MD_PDIR_INIT_PROT : MD_SDIR_INIT_PROT);

    TRACE(0);
}

/*
 * mdir_init_bdecc
 *
 *   Initializes the ECC memory through the back door, for a range
 *   of memory, using assembly routines for speed.
 *
 *   premium: true if predetermined directory type is premium
 *   saddr: page-aligned start physical address in back door memory
 *   eaddr: page-aligned end physical address (exclusive) in back door memory
 */

void
mdir_init_bdecc(int premium, __uint64_t saddr, __uint64_t eaddr)
{
#if VERBOSE
    printf("mibe(%d,%lx,%lx)\n", premium, saddr, eaddr);
#endif

    TRACE(premium);
    TRACE(saddr);
    TRACE(eaddr);

    mdir_bdecc_fill((__uint64_t) BDECC_ENTRY(saddr),
		    (__uint64_t) BDECC_ENTRY(eaddr - 0x20) + 8,
		    0x00000000);

    TRACE(0);
}

/*
 * bd_type (INTERNAL)
 *
 *   Determines width of back door memory at a specific location.
 *   Non-destructive.
 *
 *   ent: pointer to a location in back door space to be tested
 *
 *   Returns: -1 if there is no memory at that location (unpopulated)
 *             0 if there is standard back door space at that location
 *             1 if there is premium back door space at that location
 *
 *   Between the write and read of the probe value, a zero is written
 *   through the same data lines to be sure they're not just floating
 *   at the last value stored.
 */

#define TEST_PATTERN		0x5555555555555555

static int bd_type(__uint64_t ent)
{
#ifndef RTL_GATE
    __uint64_t		save0, save1;
#endif
    __uint64_t		t;
    int			rv;

    TRACE(ent);

#ifndef RTL_GATE
    save0 = LD(ent + 0);
    save1 = LD(ent + 8);
#endif

    SD(ent + 0, MD_DIR_FORCE_ECC | TEST_PATTERN);
    SD(ent + 8, MD_DIR_FORCE_ECC | 0);

    t = LD(ent + 0);

#ifndef RTL_GATE
    SD(ent + 0, MD_DIR_FORCE_ECC | save0);
    SD(ent + 8, MD_DIR_FORCE_ECC | save1);
#endif

    if ((t & MD_PDIR_MASK) == (TEST_PATTERN & MD_PDIR_MASK))
	rv = 1;
    else if ((t & MD_SDIR_MASK) == (TEST_PATTERN & MD_SDIR_MASK))
	rv = 0;
    else
	rv = -1;

#if VERBOSE
    printf("bt(%lx)=%d\n", ent, rv);
#endif

    TRACE(rv);

    return rv;
}

/*
 * bd_alias (INTERNAL)
 *
 *   Returns true if two back door entries are aliases for each other.
 *   That is, writing to the second entry affects the first entry.
 *   Works only if the accessed location is populated.  Non-destructive.
 *
 *   ent0: pointer to first entry in back door space
 *   ent1: pointer to second entry in back door space
 */

static int bd_alias(__uint64_t ent0, __uint64_t ent1)
{
    int			rv;
#ifndef RTL_GATE
    __uint64_t		save0, save1;
#endif

    TRACE(ent0);
    TRACE(ent1);

#ifndef RTL_GATE
    save0 = LD(ent0);
    save1 = LD(ent1);
#endif

    SD(ent0, MD_DIR_FORCE_ECC | 0x5555);
    SD(ent1, MD_DIR_FORCE_ECC | 0xaaaa);

    rv = ((LD(ent0) & 0xffff) != 0x5555);

#ifndef RTL_GATE
    SD(ent0, MD_DIR_FORCE_ECC | save0);
    SD(ent1, MD_DIR_FORCE_ECC | save1);
#endif

#if VERBOSE
    printf("ba(%lx,%lx)=%d\n", ent0, ent1, rv);
#endif

    TRACE(rv);

    return rv;
}

/*
 * size_back_door (INTERNAL)
 *
 *   Probes a bank's back door space to determine the size of the bank
 *   and its premiumness.  Non-destructive.
 *
 *   base: base memory address of bank to probe (*** not back door ***)
 *   premium: return value for premiumness
 *
 *   Returns one of MD_SIZE_xxx (possibly MD_SIZE_EMPTY)
 *
 *   NOTE:  It is possible that a premium directory SIMM could be plugged
 *   in that does not correspond to the size of the memory SIMMs.  In
 *   this case, the smaller size is assumed, and premium.
 */

static int size_back_door(__uint64_t base, int *premium)
{
    __uint64_t		save0, save1;
    __uint64_t		bd_base, bd_probe;
    int			sz;

    TRACE(base);

    bd_base = BDPRT_ENTRY(base, 0);

    *premium = bd_type(bd_base);

    if (*premium < 0)
	sz = MD_SIZE_EMPTY;
    else
	for (sz = MD_SIZE_512MB; sz >= MD_SIZE_32MB; sz--) {
	    bd_probe = BDPRT_ENTRY(base + MD_SIZE_BYTES(sz) / 2, 0);

	    if (! bd_alias(bd_base, bd_probe) && bd_type(bd_probe) == *premium)
		break;
	}

#if VERBOSE
    printf("sbd=%d prm=%d\n", sz, *premium);
#endif
   /* special check for holes inside 256 MB memory  */
   if(sz == MD_SIZE_512MB)
   {
	  int j;
	  j = size_check_256mb(base, *premium);
		if(j==1)
		sz--;
   }

    TRACE(sz);
    TRACE(*premium);

    return sz;
}


#define CHECKSZ		4096

static int size_check_256mb(__uint64_t base, int premium)
{
    __uint64_t		bp64;
    __uint64_t		mc_addr, mc;
    __uint64_t		ck_addr, ck;
    nasid_t		nasid;
    int			r;

    nasid	= NASID_GET(base);

    mc_addr	= (__uint64_t) REMOTE_HUB(nasid, MD_MEMORY_CONFIG);
    ck_addr	= (__uint64_t) REMOTE_HUB(nasid, PI_SYSAD_ERRCHK_EN);

    mc		= LD(mc_addr);
    ck		= LD(ck_addr);

    SD(mc_addr,
       (premium ? mc | MMC_DIR_PREMIUM : mc & ~MMC_DIR_PREMIUM) |
       MMC_IGNORE_ECC);

    SD(ck_addr, ck & ~PI_SYSAD_CHECK_ALL);

    bp64	= base + 64 * 0x100000;

    mdir_init_bddir(premium, base, base + CHECKSZ);
    mdir_init_bddir(premium, bp64, bp64 + CHECKSZ);

    memset((void *) base, 0, CHECKSZ);		/* Initialize ECC */
    memset((void *) bp64, 0, CHECKSZ);

    SD(base, 0x55);
    SD(bp64, 0xaa);

    r	= (LD(base) != 0x55);

    mdir_error_get_clear(nasid, 0);		/* Clear resultant ECC errs */

    SD(mc_addr, mc);		/* Restore state of IGNORE_ECC	       */
    SD(ck_addr, ck);		/* Restore state of PI_SYSAD_ERRCHK_EN */

    return r;
}

/*
 * size_check_32mb
 *
 *   Initializes the minimum amount of directory entries in order to
 *   probe if a bank has a 64 MB back door capacity but only 32 MB of
 *   real RAM, which is the case with Sn00 16 MB SIMMs (32 MB banks).
 *
 *   This is destructive to the directory/protection entries, as well
 *   as the real memory, for one page at base and one page at base+32MB.
 *
 *   base should be uncached bank base.  IGNORE_ECC must be on.
 */


static int size_check_32mb(__uint64_t base, int premium)
{
    __uint64_t		bp32;
    __uint64_t		mc_addr, mc;
    __uint64_t		ck_addr, ck;
    nasid_t		nasid;
    int			r;

    nasid	= NASID_GET(base);

    mc_addr	= (__uint64_t) REMOTE_HUB(nasid, MD_MEMORY_CONFIG);
    ck_addr	= (__uint64_t) REMOTE_HUB(nasid, PI_SYSAD_ERRCHK_EN);

    mc		= LD(mc_addr);
    ck		= LD(ck_addr);

    SD(mc_addr,
       (premium ? mc | MMC_DIR_PREMIUM : mc & ~MMC_DIR_PREMIUM) |
       MMC_IGNORE_ECC);

    SD(ck_addr, ck & ~PI_SYSAD_CHECK_ALL);

    bp32	= base + 32 * 0x100000;

    mdir_init_bddir(premium, base, base + CHECKSZ);
    mdir_init_bddir(premium, bp32, bp32 + CHECKSZ);

    memset((void *) base, 0, CHECKSZ);		/* Initialize ECC */
    memset((void *) bp32, 0, CHECKSZ);

    SD(base, 0x55);
    SD(bp32, 0xaa);

    r	= (LD(base) != 0x55);

    mdir_error_get_clear(nasid, 0);		/* Clear resultant ECC errs */

    SD(mc_addr, mc);		/* Restore state of IGNORE_ECC	       */
    SD(ck_addr, ck);		/* Restore state of PI_SYSAD_ERRCHK_EN */

    return r;
}

/*
 * mdir_config
 *
 *   Determines a node's memory configuration and sets the BANK0 through
 *   BANK7 and DIR_FLAVOR flags in the node's MD_MEMORY_CONFIG register.
 *   All other fields are left alone.
 *
 *   Each memory bank consists of three SIMMs slots: memory SIMM1, memory
 *   SIMM2, and directory SIMM.  For the standard back door area, only the
 *   memory are populated, because the standard back door area is included
 *   in the memory SIMMs.  For premium back door area, the directory SIMM
 *   slot is populated and contains the remaining 32 bits of width of the
 *   back door area.
 *
 *   Only the back door memory needs to be probed to determine the size
 *   of a bank.
 */

void mdir_config(nasid_t nasid, u_short *prem_mask)
{
    __uint64_t		old_cfg, new_cfg;
    int			bank;
    int			any_prem, any_std;
    __uint64_t		total;
    __uint64_t		cfg_reg;

    *prem_mask=0;
    hub_led_set(PLED_MDIRCONFIG);

    TRACE(nasid);

    cfg_reg = (__uint64_t) REMOTE_HUB(nasid, MD_MEMORY_CONFIG);

    TRACE(cfg_reg);

    /*
     * While probing, the hub must be configured with all banks set to
     * maximum size (512 MB) to prevent address exceptions from accessing
     * memory holes, and must be configured for premium SIMMs so the
     * upper 32 bits of back door space can be probed.
     */

    old_cfg = LD(cfg_reg);
    TRACE(old_cfg);
    new_cfg = old_cfg | MMC_BANK_ALL_MASK | MMC_DIR_PREMIUM;
    TRACE(new_cfg);
    SD(cfg_reg, new_cfg);
    TRACE(0);

#if VERBOSE
    printf("old_cfg: 0x%x new_cfg: 0x%x\n", old_cfg, new_cfg);
#endif /* VERBOSE */

    new_cfg	= old_cfg & ~(MMC_BANK_ALL_MASK | MMC_DIR_PREMIUM);

    any_prem	= 0;
    any_std	= 0;

    total	= 0;

    for (bank = 0; bank < MD_MEM_BANKS; bank++) {
	__uint64_t	base;
	int		size, testprem;

#if VERBOSE
	printf("size(%d,%d)\n", nasid, bank);
#endif

	base = NODE_UNCAC_BASE(nasid) + ((__uint64_t) bank << MD_BANK_SHFT);

	TRACE(base);

	size = size_back_door(base, &testprem);

	if (size == MD_SIZE_EMPTY)
	    continue;

	/*
	 * One special case must be handled.  32 MB banks have the same
	 * size back door space as 64 MB banks.  The actual memory must
	 * be probed if a 64 MB back door is found.
	 */

	if (size == MD_SIZE_64MB && size_check_32mb(base, testprem))
	    size = MD_SIZE_32MB;

	db_printf("Bank %d: %s MB (%s)\n",
	          bank, mdir_size_string(size),
	          testprem ? "premium" : "standard");

	if (testprem)
        {
	    any_prem = 1;
	    (*prem_mask) |= 1 << bank;
	    
	}
	else
	{
	    any_std  = 1;
	}

	new_cfg |= size << MMC_BANK_SHFT(bank);

	total += MD_SIZE_BYTES(size);
    }

    if (any_std) {
	if (any_prem) {
#if RTL_GATE
	    TRACE(0);
#else
	    printf("*** Mixed standard and premium memory:\n");
	    printf("*** Treating all as standard.\n");
#endif
	}
    } else if (any_prem)
	new_cfg |= MMC_DIR_PREMIUM;
    else {
#if RTL_GATE
	TRACE(0);
#else
	printf("*** No memory found.\n");
#endif
    }

    /*
     * Set the computed configuration in the config register.
     */

    TRACE(new_cfg);

    SD(cfg_reg, new_cfg);
    TRACE(0);

#if VERBOSE
    printf("mc(%lx)\n", new_cfg);
#endif

#if RTL_GATE
    TRACE(total);
#else
    db_printf("Local memory configured: %d MB (%s)\n",
	      (int) (total >> 20),
	      (new_cfg & MMC_DIR_PREMIUM) ? "premium" : "standard");
#endif
}

/*
 * mdir_bank_get
 *
 *   Returns info about the way a given bank is currently configured.
 */

void mdir_bank_get(nasid_t nasid, int bank,
		   __uint64_t *phys_base, __uint64_t *length)
{
    /* banks are passed in in locational format */
    __uint64_t	mc = translate_hub_mcreg(LD(REMOTE_HUB(nasid, 
						       MD_MEMORY_CONFIG)));
    /* 
     * the physical base address is based on the software bank, not the 
     * bank in a given location.  (if bank 0 is bad, physical address 0
     * corresponds to the bank in DIMM slot 1)
     */
    int sw_bank = bank ^ (MDIR_DIMM0_SEL(nasid)/2);

    if (phys_base)
	*phys_base = (__uint64_t) TO_NODE(nasid, MD_BANK_OFFSET(sw_bank));

    if (length)
	*length	   = MD_SIZE_BYTES(mc >> MMC_BANK_SHFT(bank) & 7);
}

/*
 * mdir_dir_decode
 */

static char *StateStrings[] = {
    "Shared",
    "Poison",
    "Excl",
    "BusySh",
    "BusyEx",
    "Wait",
    "Illegal",
    "Unowned"
};

void mdir_dir_decode(char *buf, int premium, __uint64_t lo, __uint64_t hi)
{
    __uint64_t		state;

    if (premium) {
	__uint64_t		fine;

	state = GET_FIELD(lo, MD_PDIR_STATE);
	fine  = GET_FIELD(lo, MD_PDIR_FINE);

	if (fine) {
	    /* Format A */
	    sprintf(buf,
		    "%-7s Prio=%ld Ax=%ld FineVec=%y OneCnt=%ld Oct=%ld",
		    StateStrings[MD_DIR_SHARED],
		    GET_FIELD(lo, MD_PDIR_PRIO),
		    GET_FIELD(lo, MD_PDIR_AX),
		    (hi & MD_PDIR_VECMSB_MASK) << (38 - MD_PDIR_VECMSB_SHFT) |
		    GET_FIELD(lo, MD_PDIR_VECLSB),
		    GET_FIELD(lo, MD_PDIR_ONECNT),
		    GET_FIELD(lo, MD_PDIR_OCT));
	} else if (state == MD_DIR_SHARED) {
	    /* Format B */
	    sprintf(buf,
		    "%-7s Prio=%ld Ax=%ld CoarseVec=%y OneCnt=%ld",
		    StateStrings[MD_DIR_SHARED],
		    GET_FIELD(lo, MD_PDIR_PRIO),
		    GET_FIELD(lo, MD_PDIR_AX),
		    (hi & MD_PDIR_VECMSB_MASK) << (38 - MD_PDIR_VECMSB_SHFT) |
		    GET_FIELD(lo, MD_PDIR_VECLSB),
		    GET_FIELD(lo, MD_PDIR_ONECNT));
	} else {
	    /* Format C */
	    sprintf(buf,
		    "%-7s Prio=%ld Ax=%ld CwOff=%ld Ptr=%#lx",
		    StateStrings[state],
		    GET_FIELD(lo, MD_PDIR_PRIO),
		    GET_FIELD(lo, MD_PDIR_AX),
		    GET_FIELD(lo, MD_PDIR_CWOFF),
		    GET_FIELD(lo, MD_PDIR_PTR));
	}
    } else {
	state = GET_FIELD(lo, MD_SDIR_STATE);

	if (state == MD_DIR_SHARED) {
	    /* Format A */
	    sprintf(buf,
		    "%-7s Prio=%ld Ax=%ld FineVec=%#04lx",
		    StateStrings[MD_DIR_SHARED],
		    GET_FIELD(lo, MD_SDIR_PRIO),
		    GET_FIELD(lo, MD_SDIR_AX),
		    (hi & MD_SDIR_VECMSB_MASK) << (11 - MD_SDIR_VECMSB_SHFT) |
		    GET_FIELD(lo, MD_SDIR_VECLSB));
	} else {
	    /* Format C */
	    sprintf(buf,
		    "%-7s Prio=%ld Ax=%ld CwOff=%ld Ptr=%#lx",
		    StateStrings[state],
		    GET_FIELD(lo, MD_SDIR_PRIO),
		    GET_FIELD(lo, MD_SDIR_AX),
		    GET_FIELD(lo, MD_SDIR_CWOFF),
		    GET_FIELD(lo, MD_SDIR_PTR));
	}
    }
}

/*
 * mdir_size_string
 *
 *   Converts MD_SIZE_xxx to a string.
 */

static char SizeStrings[] =
	"0\000   8\000   16\000  32\000  64\000  128\000 "
	"256\000 512\000 1024\0002048\0004096\000";

char *mdir_size_string(int size)
{
    if (size >= 0 && size < 11)
	return SizeStrings + size * 5;
    else
	return "??";
}


char *dir_states[] = {
    "   Shared   ",
    "  Poisoned  ",
    "  Exclusive ",
    " Busy Shared",
    "Busy Xlusive",
    "    Wait    ",
    "   Unknown  ",
    "   Unowned  ",
};

char *dir_owner[] = {
    "Cpu A",
    "Cpu B",
    "IO(G)",
    " IO  ",
};


int
mdir_pstate(int chk_state, __psunsigned_t addr, __uint32_t size)
{
    __uint32_t		off, page;
    md_pdir_low_t	elo;
    paddr_t		phys;
    int			state, node, owner, slice;
    int			lines = 0;

    printf("checking for premium directory state %d from 0x%llx, size %x\n",
	   chk_state, addr, size);

    phys = TO_PHYS(addr);
    for (off = 0; off < size; off += 4096, phys += 4096) {
	elo.pd_lo_val = *(volatile __uint64_t *) BDDIR_ENTRY_LO(phys);
	state = elo.pds_lo_fmt.pds_lo_state;

	if ((chk_state != -1) && (chk_state != state))
	    continue;

	slice = elo.pde_lo_fmt.pde_lo_ptr & 0x3;
	owner = elo.pde_lo_fmt.pde_lo_ptr >> 2;

	if (! more(&lines, 23))
	    return -1;

	printf("0x%02x   0x%05x   0x%02x/%s,  %s  (0x%lx)\n",
	       NASID_GET(phys),
	       (K0_TO_NODE_OFFSET(phys) >> 12),
	       owner, dir_owner[slice],
	       dir_states[state], elo.pd_lo_val);
    }

    return 0;
}

int
mdir_sstate(int chk_state, __psunsigned_t addr, __uint32_t size)
{
    __uint32_t		off, page;
    md_sdir_low_t	elo;
    paddr_t		phys;
    int			state, node, owner, slice;
    int			lines = 0, i;

    if (chk_state > 7) chk_state = -1;

    printf("checking for std directory state %d from 0x%llx, size %x\n",
	   chk_state, addr, size);

    phys = TO_PHYS(addr);
    for (off = 0; off < size; off += 4096, phys += 4096) {
	for (i = 0; i < 4096; i += CACHE_SLINE_SIZE) {
	    elo.sd_lo_val = *(volatile __uint64_t *) BDDIR_ENTRY_LO(phys + i);
	    state = elo.sds_lo_fmt.sds_lo_state;

	    if ((chk_state != -1) && (chk_state != state))
		continue;

	    slice = elo.sde_lo_fmt.sde_lo_ptr & 0x3;
	    owner = elo.sde_lo_fmt.sde_lo_ptr >> 2;

	    if (! more(&lines, 23))
		return -1;

	    printf("0x%02x   0x%08x   0x%02x/%s,  %s  (0x%lx)\n",
		   NASID_GET(phys), (K0_TO_NODE_OFFSET(phys + i)),
		   owner, dir_owner[slice], dir_states[state], elo.sd_lo_val);
	}
    }

    return 0;
}

void
mdir_dirstate_usage(void)
{
    printf("Possible directory states are:\n");
    printf("	0: 	Shared\n");
    printf("	1: 	Poisoned\n");
    printf("	2: 	Exclusive\n");
    printf("	3: 	Busy Shared\n");
    printf("	4: 	Busy Exclusive\n");
    printf("	5: 	Wait\n");

    printf("	6: 	Reserved\n");
    printf("	7: 	Unowned\n");
    printf("\n");
}

void mdir_disable_bank(nasid_t nasid, int bank)
{
    /* 
     * put memory config register into location format - the information
     * in location 0 of the register corresponds to physical bank 0 on the
     * board.  The 'bank' parameter is also in location format.  PV 669589
     */
     
    __uint64_t	mc = translate_hub_mcreg(REMOTE_HUB_L(nasid, MD_MEMORY_CONFIG));

    mc &= ~(MMC_BANK_MASK(bank));
    mc |= (MD_SIZE_EMPTY << MMC_BANK_SHFT(bank)); 

    /*
     * put memory config register back into hardware format.  Which bank's
     * information is stored in which location depends on whether or not the
     * banks are swapped 
     */

    mc = translate_hub_mcreg(mc);
    REMOTE_HUB_S(nasid, MD_MEMORY_CONFIG, mc);
}

int is_node_memless(nasid_t n)
{
    __uint64_t          mc = REMOTE_HUB_L(n, MD_MEMORY_CONFIG);

    return(!(mc & MMC_BANK_ALL_MASK)) ;
}

void mdir_sz_get(nasid_t nasid, char *bank_sz)
{
    int         bank;
    __uint64_t  mc = translate_hub_mcreg(REMOTE_HUB_L(nasid, MD_MEMORY_CONFIG));

    for (bank = 0; bank < MD_MEM_BANKS; bank++)
        bank_sz[bank] = (mc & MMC_BANK_MASK(bank)) >> MMC_BANK_SHFT(bank);
}

__uint64_t make_mask_from_str(char *str)
{
    int 	bank ;
    __uint64_t 	mask ;
    mask = 0;

    for (bank = 0; bank < MD_MEM_BANKS; bank++) {
        if (strchr(str, '0' + bank)) {
            mask = SET_DISABLE_BIT(mask);
            mask = SET_DISABLE_BANK(mask, bank);
        }
    }
    return mask ;
}


