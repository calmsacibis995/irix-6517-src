/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#if defined(IP30)

#include <arcs/errno.h>
#include <arcs/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <string.h>
#include <libsk.h>
#include <libsc.h>
#include <ksys/cacheops.h>
#include <sys/mips_addrspace.h>
#include <sys/RACER/sflash.h>
#include <sys/RACER/IP30nvram.h>

typedef struct pds_space_usage_s {
	int	space;
	int	inuse_env_ent;
	int	inuse_env_bytes;
	int	inuse_log_ent;
	int	inuse_log_bytes;
	int	inuse_dat_ent;
	int	inuse_dat_bytes;
	int	invalid_env_ent;
	int	invalid_env_bytes;
	int	invalid_log_ent;
	int	invalid_log_bytes;
	int	invalid_dat_ent;
	int	invalid_dat_bytes;
	int	free;
} pds_space_usage_t;

#define PDS_INUSE			\
	(pds_usage.inuse_log_bytes +	\
	 pds_usage.inuse_env_bytes +	\
	 pds_usage.inuse_dat_bytes )

static pds_space_usage_t pds_usage;

void flash_pds_space_usage(pds_space_usage_t *use);

static void flash_io_reloc(void);
typedef flash_err_t (flash_io_f)(int, vu_short *, __uint64_t, __uint64_t, __uint64_t);
typedef flash_err_t (flash_program_f)(flash_io_f *, int, uint16_t *, vu_short *, vu_short *);

#define BSS_FIO_SIZE 		2048
char bss_flash_io[BSS_FIO_SIZE];

flash_program_f *flash_program_p;
flash_io_f *flash_io_p;

__psunsigned_t flash_mem_base;
static vu_short *flash_pds_freep;
static int pds_log_init;
static int power_disabled;

SETUP_TIME

#ifdef FDEBUG
#define FPR_FLG		(FPR_FLG_MSG |FPR_FLG_ERR | FPR_FLG_PERF)
unsigned int fpr_flg =	FPR_FLG | FPR_FLG_PDS;		/* normal mode */
#endif /* FDEBUG */

struct reg_values csr7[] = {
	{ 0,	"Busy" },
	{ 1,	"Ready" },
};

static struct reg_desc csr_desc[] = {
	/* mask      	  shift  name     format   values */
	{ SFLASH_CSR_WSMS,  0,	"Ready",    NULL,   NULL },
	{ SFLASH_CSR_ESS,   0,	"EraseSusp",NULL,   NULL },
	{ SFLASH_CSR_ES,    0,	"EraseErr", NULL,   NULL },
	{ SFLASH_CSR_DWS,   0,	"WriteErr", NULL,   NULL },
	{ SFLASH_CSR_VPPS,  0,	"VppLow",   NULL,   NULL },
	{ SFLASH_CSR_RSV2,  0,	"rsvd2",    NULL,   NULL },
	{ SFLASH_CSR_RSV1,  0,	"rsvd1",    NULL,   NULL },
	{ SFLASH_CSR_RSV0,  0,	"rsvd0",    NULL,   NULL },
	{ 0,  		    0,	NULL,       NULL,   NULL },
};

static struct reg_desc gsr_desc[] = {
	/* mask      	  shift  name     format   values */
	{ SFLASH_GSR_WSMS,  0,	"Ready",    NULL,   NULL },
	{ SFLASH_GSR_OSS,   0,	"OpSusp",   NULL,   NULL },
	{ SFLASH_GSR_DOS,   0,	"OpErr",    NULL,   NULL },
	{ SFLASH_GSR_DSS,   0,	"inSleep",  NULL,   NULL },
	{ SFLASH_GSR_QS,    0,	"Qfull",    NULL,   NULL },
	{ SFLASH_GSR_PBAS,  0,	"PgBAvail", NULL,   NULL },
	{ SFLASH_GSR_PBS,   0,	"PgBReady", NULL,   NULL },
	{ SFLASH_GSR_PBSS,  0,	"PgB1Sel",  NULL,   NULL },
	{ 0,  		    0,	NULL,       NULL,   NULL },
};

static struct reg_desc bsr_desc[] = {
	/* mask      	  shift  name     format   values */
	{ SFLASH_BSR_WSMS,  0,	"Ready",    NULL,   NULL },
	{ SFLASH_BSR_BLS,   0,	"Unlocked", NULL,   NULL },
	{ SFLASH_BSR_BOS,   0,	"OpErr",    NULL,   NULL },
	{ SFLASH_BSR_BOAS,  0,	"Aborted",  NULL,   NULL },
	{ SFLASH_BSR_QS,    0,	"Qfull",    NULL,   NULL },
	{ SFLASH_BSR_VPPS,  0,	"VppLow",   NULL,   NULL },
	{ SFLASH_BSR_RSV1,  0,	"rsvd1",    NULL,   NULL },
	{ SFLASH_BSR_RSV0,  0,	"rsvd0",    NULL,   NULL },
	{ 0,  		    0,	NULL,       NULL,   NULL },
};

#define HEART_MSEC_TICKS(ms)	\
	(((heartreg_t)(ms)*1000000)/(heartreg_t)HEART_COUNT_NSECS)

/* BEGIN BSS-RELOCATION Block */
/*
 * CAUTION: Add functions for BSS relocation ONLY after this line
 *	    The code inside this will be relocated to the bss_flash_io
 *	    array and will be called by function pointers set to
 *	    offsets within the bss array.
 *	    You must make sure the code inside this block DOES NOT
 *	    (A) make function calls (i.e. only LEAF fcts allowed) and
 *	    (B) make references any data outside of the scope of the
 *	    functions (rodata, data, bss-data). This means any global,
 *	    local or static local data structures.
 */

static flash_err_t
_flash_io(int cmd, vu_short *fcmd, __uint64_t a1, __uint64_t a2, __uint64_t a3)
{
	bridge_t *bp = BRIDGE_K1PTR;
	ioc3_mem_t *ioc3 = (ioc3_mem_t *)IOC3_PCI_DEVIO_K1PTR;
	bridgereg_t ctrl_sav;
	flash_err_t rv = 0;

	/* enable Bridge bit for prom write */
	ctrl_sav = bp->b_wid_control;
	bp->b_wid_control |= BRIDGE_CTRL_FLASH_WR_EN;
	bp->b_wid_control;		/* inval addr bug war */

	switch (cmd) {
	/*
	 * cmd		- (I) flash command, SFLASH_CMD_ERASE
	 * fcmd		- (I) command address, begin of block to be erased 
	 */
	case SFLASH_CMD_ERASE: {
	    volatile heartreg_t ht_end, ht_blink, ht_count;
	    vu_short csr;

	    ioc3->gppr_0 = 0; ioc3->gppr_1 = 1;	/* AMBER */
	    ht_blink = HEART_MSEC_TICKS(500);	/* .5 sec */
	    ht_blink += HEART_PIU_K1PTR->h_count;

	    *fcmd = cmd;
	    *fcmd = SFLASH_CMD_ERASE_CONFIRM;

	    *fcmd = SFLASH_CMD_STATUS_READ;
	    ht_end = HEART_MSEC_TICKS(4000);	/* 4 sec max wait */
	    ht_end += HEART_PIU_K1PTR->h_count;
	    do {
		ht_count = HEART_PIU_K1PTR->h_count;
		if (ht_count > ht_blink) {
		    ioc3->gppr_0 = 0; ioc3->gppr_1 = 0;	/* OFF */
		    ht_blink = ht_count;
		}
		csr = *fcmd & 0xff;
	    } while (!(csr & SFLASH_STATUS_READY) && (ht_count < ht_end));

	    if (csr != SFLASH_STATUS_READY) {
		/* clear flash status regs, abort current op */
		*fcmd = SFLASH_CMD_STATUS_CLEAR;
		HEART_PIU_K1PTR->h_count;	/* flush bus */
		rv = FLASH_ERR_ERASE_TO;
	    }
	    break;
	}

	/*
	 * cmd		- (I) flash command, SFLASH_CMD_WRITE
	 * fcmd		- (I) command address, any flash memory
	 * a1		- (I) source address for 16 bit word
	 * a2           - (I) destination address for 16 bit word
	 * a3           - (I) 16bit word write length (bytes/2)
	 *		      < 0 means to reverse copy for |a3|
	 * NOTE: a1, a2 must start on half wd boundary
	 */
	case SFLASH_CMD_WRITE: {
	    vu_short *src = (vu_short *)a1;
	    vu_short *dst = (vu_short *)a2;
	    int len = (int)a3;
	    __uint64_t *lsp = (__uint64_t *)a1;
	    __uint64_t *ldp = (__uint64_t *)a2;
	    int not_dw_aligned = (a1 & 0x7 || a2 & 0x7);

	    volatile heartreg_t ht_end;
	    vu_short csr;

	    ioc3->gppr_0 = 0; ioc3->gppr_1 = 0;	/* OFF */
	    if (len < 0) {
		dst -= len;
		src -= len;
	    }

	    while (len) {
	    	*fcmd = cmd;
		if (len > 0)
		    *dst++ = *src++; 
		else
		    *--dst = *--src;

		/*
		 * spec sez max 8 usecs for write
		 * but we will wait 1 msec max for
		 * each write to complete
		 */
		*fcmd = SFLASH_CMD_STATUS_READ;
		ht_end = HEART_MSEC_TICKS(1);
		ht_end += HEART_PIU_K1PTR->h_count;
		do {
		    if ((int)a3 > SFLASH_SEG_SIZE ||
			(int)a3 < -SFLASH_SEG_SIZE) {
			if (((__psint_t)dst & 0xffff) == 0x8000)
			    ioc3->gppr_0 = 0; ioc3->gppr_1 = 0;	/* OFF */
			if (((__psint_t)dst & 0xffff) == 0)
			    ioc3->gppr_0 = 1; ioc3->gppr_1 = 0;	/* GREEN */
		    }
		    csr = *fcmd & 0xff;	/* read flash status */
		} while (!(csr & SFLASH_STATUS_READY) &&
			 HEART_PIU_K1PTR->h_count < ht_end);

		if (csr != SFLASH_STATUS_READY) {
		    /* clear flash status regs, abort current op */
		    *fcmd = SFLASH_CMD_STATUS_CLEAR;
		    HEART_PIU_K1PTR->h_count;	/* flush bus */
		    rv = FLASH_ERR_WRITE_TO;
		    break;
		}
		(len > 0) ? --len : ++len;
	    }
	    if (rv)
		break;
	    /*
	     * verify the copy, reload src, dst, len
	     */
	    ioc3->gppr_0 = 0; ioc3->gppr_1 = 0;	/* OFF */
	    *fcmd = SFLASH_CMD_READ;    /* return to read mode */
	    len = (int)a3;
	    if (len < 0)
		len *= -1;
	    if ( !not_dw_aligned) {
		while (len > 3) {
		    len -= 4;
		    if (*lsp != *ldp) {
			rv = (flash_err_t)ldp;
			break;
		    }
		    lsp++, ldp++;
		}
	    }
	    if (len && !rv) {
		src = (vu_short *)lsp;
		dst = (vu_short *)ldp;

		while (len) {
		    if (*src != *dst) {
			rv = (flash_err_t)dst;
			break;
		    }
		    src++, dst++, len--;
		}
	    }
	    break;
	}

	/*
	 * cmd		- (I) flash command, SFLASH_CMD_STATUS_READ
	 * fcmd		- (I) command address, any flash memory
	 * a1		- (O) Common Status Register, 8 bits
	 * a2           - (O) Global Status Register, 8 bits
	 * a3           - (O) Block  Status Register, 8 bits
	 */
	case SFLASH_CMD_STATUS_READ: {
	    vu_short *csrp = (vu_short *)a1;
	    vu_short *gsrp = (vu_short *)a2;
	    vu_short *bsrp = (vu_short *)a3;

	    *fcmd = cmd;
	    *csrp = 0xff & *fcmd;

	    *fcmd = SFLASH_CMD_ESR_READ;
	    *gsrp = 0xff & fcmd[2];
	    *fcmd = SFLASH_CMD_ESR_READ;
	    *bsrp = 0xff & fcmd[1];
	    
	    break;
	}

	/*
	 * cmd		- (I) flash command, SFLASH_CMD_ID_READ
	 * fcmd		- (I) command address, any flash memory
	 * a1		- (O) Manufactureer ID, 8 bits
	 * a2           - (O) Device ID, 8 bits
	 */
	case SFLASH_CMD_ID_READ: {
	    vu_short *mfgid = (vu_short *)a1;
	    vu_short *devid = (vu_short *)a2;

	    *fcmd = cmd;
	    /*
	     * mfg and dev id's are read off of different 
	     * offsets from the base of flash memory
	     */
	    *mfgid = 0xffff & fcmd[0];
	    *devid = 0xffff & fcmd[1];
	    break;
	}

	default:
	    rv = FLASH_ERR_UNKOWN_CMD;
	    break;
	}

	*fcmd = SFLASH_CMD_READ;    /* return to read mode */
	/* disable Bridge bit for prom write */
	bp->b_wid_control = ctrl_sav;
	bp->b_wid_control;		/* inval addr bug war */

	if (rv) {
	    ioc3->gppr_0 = 0; ioc3->gppr_1 = 1;	/* AMBER */
	} else {
	    ioc3->gppr_0 = 1; ioc3->gppr_1 = 0;	/* GREEN */
	}

	return rv;
}

#define POST_FLASH_RESET	0x1
#define REVERSE_PROGRAM		0x2

static flash_err_t
_flash_program(flash_io_f *fiop, int flag,
	       uint16_t *src, vu_short *beg_segp, vu_short *end_segp)
{
	vu_short *segp;
	__int64_t hlen;
	flash_err_t frv;

	for (segp = beg_segp; segp <= end_segp;
	     segp += SFLASH_SEG_SIZE/sizeof(uint16_t)) {

	    (*fiop)(SFLASH_CMD_ERASE, segp, 0, 0, 0);
	}
	hlen = (__int64_t)(segp - beg_segp);
	if (flag & REVERSE_PROGRAM)
	    hlen *= -1;

	frv = (*fiop)(SFLASH_CMD_WRITE, beg_segp,
			(__uint64_t)src,
			(__uint64_t)beg_segp,
			(__uint64_t)hlen); 

	if (flag & POST_FLASH_RESET) {
	    /*
	     * the contents of this if block must mirror
	     * the reset procedures in cpu_hardreset()
	     * we'd like to call cpu_hardreset() but
	     * it had just been erased and re-flashed
	     */
	    HEART_PIU_K1PTR->h_mode |= HM_SW_RST;
	    HEART_PIU_K1PTR->h_mode;	/* flush bus */
	}
	return(frv);
}

/*
 * CAUTION: Add functions for BSS relocation before this line
 *	    and flash_io() must follow immediately after because
 *	    its used as the END marker when calculating the
 *	    relocation size.
 */
/* END BSS-RELOCATION Block */

static flash_err_t
flash_io(int cmd, vu_short *fcmd, __uint64_t a1, __uint64_t a2, __uint64_t a3)
{
	int reenable = 0;
	flash_err_t frv;

	if (!flash_io_p)
	    flash_io_reloc();

	if (!power_disabled) {
	    ip30_disable_power();	/* disable power button */
	    power_disabled = 1;
	    reenable = 1;
	}

	frv = (*flash_io_p)(cmd, fcmd, a1, a2, a3);

	if (reenable) {
	    ip30_setup_power();	/* re-enable power button */
	    power_disabled = 0;
	}
	return(frv);
}

/* CAUTION: Add functions NON-BSS relocation (ie. regular) code after this line */

/*
 * The sections headers for rprom, fprom and PDS must be invalidated before we
 * reprogram them. Since fprom and PDS have the headers at beginning of the addr
 * space the copy must be reverse (so the header magic will be written last).
 */
flash_err_t
flash_program(uint16_t *src, int reset, unsigned beg_seg, unsigned end_seg)
{
	vu_short *begp, *endp;
	int cnt;
	int reenable = 0;
	flash_err_t frv;

	if (!flash_program_p)
	    flash_io_reloc();

	if (reset)
	    reset = POST_FLASH_RESET;

	begp = SFLASH_SEG_ADDR(beg_seg);
	endp = SFLASH_SEG_ADDR(end_seg);

	if (!power_disabled) {
	    ip30_disable_power();	/* disable power button */
	    power_disabled = 1;
	    reenable = 1;
	}

	if (SFLASH_FPROM_SEG >= beg_seg && SFLASH_FPROM_SEG <= end_seg) {
	    cnt = flash_get_nv_cnt(SFLASH_FPROM_SEG) + 1;
	    flash_set_nv_cnt(SFLASH_FPROM_SEG, &cnt);
	    flash_header_invalidate((void *)SFLASH_FPROM_HDR_ADDR);

	    flash_set_nv_rpromflg(flash_get_nv_rpromflg() | RPROM_FLG_FVP);
	    reset |= REVERSE_PROGRAM;
	}
	if (SFLASH_RPROM_SEG >= beg_seg && SFLASH_RPROM_SEG <= (int)end_seg) {
	    cnt = flash_get_nv_cnt(SFLASH_RPROM_SEG) + 1;
	    flash_set_nv_cnt(SFLASH_RPROM_SEG, &cnt);
	    flash_header_invalidate((void *)SFLASH_RPROM_HDR_ADDR);
	}
	if (SFLASH_PDS_SEG >= beg_seg && SFLASH_PDS_SEG <= end_seg) {
	    cnt = flash_get_nv_cnt(SFLASH_PDS_SEG) + 1;
	    flash_set_nv_cnt(SFLASH_PDS_SEG, &cnt);
	    flash_header_invalidate((void *)SFLASH_PDS_ADDR);
	    reset |= REVERSE_PROGRAM;
	}

	frv = (*flash_program_p)(flash_io_p, reset, src, begp, endp);

	if (reenable) {
	    ip30_setup_power();	/* re-enable power button */
	    power_disabled = 0;
	}
	return(frv);
}

static int
running_cached(void)
{
	ulong pc, ppc;
	extern ulong get_pc(void);

	pc = get_pc();

	if (IS_KSEG0(pc) || IS_COMPATK0(pc))
	    return(1);
	else
	    return(0);

}

static void
flash_io_reloc(void)
{
	__psunsigned_t reloc_size;
	__psunsigned_t bss_size;
	__psunsigned_t dst = (__psunsigned_t)bss_flash_io;

	reloc_size = (__psunsigned_t)flash_io - (__psunsigned_t)_flash_io;

	/*
	 * Align to scache at both ends to avoid sharing the cache line
	 * with other data both at the beginning and at the end of the
	 * relocation array.
	 */
	dst = (dst + CACHE_SLINE_SIZE - 1) & CACHE_SLINE_MASK;
	bss_size = (__psunsigned_t)&bss_flash_io[BSS_FIO_SIZE] - dst;
	bss_size &= CACHE_SLINE_MASK;
	if (running_cached())
	    __dcache_wb_inval((void *)dst, reloc_size);
	/*
	 * BUG: XXX we can only run from bss as uncached.
	 * we currently cannot run from K0
	 */
	dst = PHYS_TO_K1(KDM_TO_PHYS(dst));

	if (reloc_size > bss_size) {
	    FPR_ERR(("flash_io is too big for bss_flash_io, reloc_size\n",
	    	reloc_size));
	    return;
	}
	bcopy((void *)_flash_io, (void *)dst, reloc_size);
	FPR_HI(("bss_flash_io  %#x dst %#x reloc_size %#x(%d) \n",
		bss_flash_io, dst, reloc_size, reloc_size));

	flash_io_p = (flash_io_f *)dst;
	flash_program_p = (flash_program_f *)(dst +
		((__psunsigned_t)_flash_program - (__psunsigned_t)_flash_io));	
	FPR_HI(("flash_io_p %#x flash_program_p %#x, bss_size %#x(%d)\n",
		flash_io_p, flash_program_p, bss_size, bss_size));
}

void
flash_print_status(int seg)
{
	vu_short *fcmd, *regp, gsr, bsr, csr;
	flash_err_t frv;

	if (seg < 0 && seg > SFLASH_MAX_SEGS)
	    seg = 0;
	frv = flash_io(SFLASH_CMD_STATUS_READ, SFLASH_SEG_ADDR(seg),
			(__uint64_t)&csr, (__uint64_t)&gsr, (__uint64_t)&bsr);

	if (frv) {
	    flash_print_err(frv);
	} else {
	    FPR_MSG(("    CSR: %R\n", csr, csr_desc));
	    FPR_MSG(("    GSR: %R\n", gsr, gsr_desc));
	    FPR_MSG(("    BSR[seg=%d]: %R\n", seg, bsr, bsr_desc));
	}
}

flash_err_t
flash_cp(uint16_t *src, vu_short *dst, int hlen)
{
	return flash_io(SFLASH_CMD_WRITE, dst,
		(__uint64_t)src, (__uint64_t)dst, (__uint64_t)hlen);
}


void
flash_id(unsigned short *mfg, unsigned short *dev)
{ 
	flash_io(SFLASH_CMD_ID_READ, SFLASH_FCMD_ADDR,
		(__uint64_t)mfg, (__uint64_t)dev, 0);
}

int
flash_ok(void)
{
	vu_short * p;
	unsigned short mfgid, devid;

	mfgid = devid = 0;
	flash_id(&mfgid, &devid);
	return(mfgid == SFLASH_MFG_ID && devid == SFLASH_DEV_ID);
}

flash_err_t
flash_erase(int segment)
{
	flash_err_t rv;

	START_TIME;
	rv = flash_io(SFLASH_CMD_ERASE, SFLASH_SEG_ADDR(segment), 0, 0, 0);
	STOP_TIME("flash erase ");
	return(rv);
}

void
flash_print_err(flash_err_t err)
{
	switch (err) {
	case 0:
	    FPR_MSG(("flash operation successful\n"));
	    break;

	case FLASH_ERR_ERASE_TO:
	    FPR_ERR(("Erase timeout Failure, Operation Aborted\n"));
	    break;

	case FLASH_ERR_WRITE_TO:
	    FPR_ERR(("Write timeout Failure, Operation Aborted\n"));
	    break;

	case FLASH_ERR_PDS_ADDR:
	    FPR_ERR(("Bad PDS Address\n"));
	    break;

	case FLASH_ERR_PARANOID:
	    FPR_ERR(("PARANOID check failed bogus data abound!\n"));
	    break;

	case FLASH_ERR_PDS_NEW:
	    FPR_ERR(("Cannot allocate new pds entry\n"));
	    break;

	case FLASH_ERR_STR_TOO_LONG:
	    FPR_ERR(("pds string too long\n"));
	    break;

	case FLASH_ERR_ENT_TOO_LONG:
	    FPR_ERR(("pds data entry too long\n"));
	    break;

	case FLASH_ERR_OUT_OF_MEM:
	    FPR_ERR(("out of memory\n"));
	    break;

	default:
	    FPR_ERR(("verify failure at flash address 0x%x\n", err));
	    break;
	}
}

int
flash_pds_hdr_ok(flash_pds_hdr_t *h)
{
    return(h->magic == FPDS_HDR_MAGIC &&
	   (0xffff & ~_cksum1((void *)h, sizeof(*h), 0)) == 0);
}

/*
 * will be called in very early RPROM, do *NOT* use printfs nor
 * should you assume you're running cached
 */
int
flash_header_ok(flash_header_t *h)
{
    return(h->magic == FLASH_PROM_MAGIC &&
	   (0xffff & ~_cksum1((void *)h, sizeof(*h), 0)) == 0);
}

/*
 * will be called in very early RPROM, do *NOT* use printfs nor
 * should you assume you're running cached
 */
int
flash_prom_ok(char *buf, flash_header_t *h)
{
    if (flash_header_ok(h)) {
	if (flash_cksum_r((char *)buf, h->datalen, 0) == h->dcksum)
	    return(1);
    }
    return(0);
}

/*
 * Called by RPROM for decision on whether to jump to FPROM
 * or to continue in RPROM and do the FPROM recovery.
 *
 * rv	0 do not jump to fprom, continue RPROM start recovery
 *	1 jump to fprom continue PROM boot
 *
 * this fct called in very early RPROM, do *NOT* use printfs nor
 * should you assume you're running cached
 */
int
flash_fprom_reloc_chk(void)
{
#if XXX
currently having problems accessing the nvram this early

    ushort rpromflg = flash_get_nv_rpromflg();
pon_puts("flash_fprom_reloc_chk ");
pon_puthex(rpromflg);
pon_puts("\r\n");
    /*
     * RPROM test flag on, unconditionally continue
     * RPROM, do NOT jump to FPROM
     */
    if (rpromflg & RPROM_FLG_TEST) {
pon_puts("RPROM_FLG_TEST set\r\n");
	/*
	 * unset the flag so we can return to fprom
	 * on next boot, if all goes well
	 */
	flash_set_nv_rpromflg(rpromflg & ~RPROM_FLG_TEST);
	return(0);
    }
#endif

    if (jumper_off()) {

#if XXX
pon_puts("jumper off\r\n");
	/*
	 * jumper off, and FPROM Verify Pending bit on
	 * FPROM failed to clear the FVP bit, FPROM is in
	 * trouble, reprogram with known good fprom image
	 */
	if (rpromflg & RPROM_FLG_FVP)
	    return(0);

pon_puts("full fprom chk\r\n");
#endif
	/*
	 * jumper off, and FPROM Verify Pending bit off
	 * do full prom check sum
	 */
	return(flash_prom_ok((char *)SFLASH_FIXED_FPROM_ADDR,
			     (flash_header_t *)SFLASH_FIXED_FPROM_HDR));
    } else {
	/*
	 * jumper in on, Normal operating condition
	 * do the abridged FPROM check
	 */
	return(flash_header_ok((flash_header_t *)SFLASH_FIXED_FPROM_HDR));
    }
}

/*
 * invalidate the headers
 * zero out the 1st 32bit wd of header
 * which must be the non-zero magic word
 */
int
flash_header_invalidate(void *hdr)
{
	vu_short zero = 0;
	vu_short *dst = hdr;

    	return(flash_io(SFLASH_CMD_WRITE, dst,
			(__uint64_t)&zero,
			(__uint64_t)dst, sizeof(uint32_t)/sizeof(uint16_t)));
}

/* initialized to 0 */
static int bad_nv_cnt;

/*
 * only rprom should be calling this fct and ONLY if the
 * dallas part has bad checksum
 */
void
flash_nv_cntr_bad(void)
{
    bad_nv_cnt = 1;
}


int
flash_get_nflashed(int seg)
{
	flash_header_t *h;

	switch (seg) {
	case SFLASH_RPROM_SEG: {
	    h = SFLASH_RPROM_HDR_ADDR;
	    if (flash_header_ok(h))
		return(h->nflashed);
	    break;
	}

	case SFLASH_FPROM_SEG: {
	    h = SFLASH_FPROM_HDR_ADDR;
	    if (flash_header_ok(h))
		return(h->nflashed);
	    break;
	}

	case SFLASH_PDS_SEG: {
	    flash_pds_hdr_t *pds = (flash_pds_hdr_t *)SFLASH_PDS_ADDR;

	    if (flash_pds_hdr_ok(pds))
		return(pds->erases);
	    break;
	}

	default:
	    FPR_MSG(("flash_get_nflashed: Bogus seg! %d\n", seg));
	    break;
	}
	return(0);
}

unsigned short
flash_get_nv_rpromflg(void)
{
    char *cp;

    cp = cpu_get_nvram_offset(NVOFF_RPROMFLAGS, NVLEN_RPROMFLAGS);
    return((ushort)((cp[0] << 8) | cp[1]));
}

void
flash_set_nv_rpromflg(ushort flg)
{
    cpu_set_nvram_offset(NVOFF_RPROMFLAGS, NVLEN_RPROMFLAGS, (char *)&flg);
}

int
flash_get_nv_cnt(int seg)
{
    int cnt;
    char *cp;

    /*
     * if dallas nvram is inaccessible try to
     * use the backups in flash headers
     */
    if (bad_nv_cnt)
	return(flash_get_nflashed(seg));

    switch (seg) {
    case SFLASH_RPROM_SEG:
	cp = cpu_get_nvram_offset(NVOFF_NFLASHEDRP, NVLEN_NFLASHEDRP);
	break;

    case SFLASH_FPROM_SEG:
    	cp = cpu_get_nvram_offset(NVOFF_NFLASHEDFP, NVLEN_NFLASHEDFP);
	break;

    case SFLASH_PDS_SEG:
	cp = cpu_get_nvram_offset(NVOFF_NFLASHEDPDS, NVLEN_NFLASHEDPDS);
	break;

    default:
    	FPR_MSG(("flash_get_nv_cnt: Bogus seg! %d\n", seg));
	return(0);
    }
    bcopy(cp, (char *)&cnt, sizeof(cnt));

    return(cnt);
}

void
flash_set_nv_cnt(int seg, int *val)
{
    char *cp = (char *)val;

    if (bad_nv_cnt)
	return;

    switch (seg) {
    case SFLASH_RPROM_SEG:
	cpu_set_nvram_offset(NVOFF_NFLASHEDRP, NVLEN_NFLASHEDRP, cp);
	break;

    case SFLASH_FPROM_SEG:
	cpu_set_nvram_offset(NVOFF_NFLASHEDFP, NVLEN_NFLASHEDFP, cp);
	break;

    case SFLASH_PDS_SEG:
	cpu_set_nvram_offset(NVOFF_NFLASHEDPDS, NVLEN_NFLASHEDPDS, cp);
	break;
    
    default:
    	FPR_MSG(("flash_set_nv_cnt: Bogus seg! %d\n", seg));
	break;
    }
}

/*
 * update nu_hdr for  rprom/fprom based on current hdr in flash memory
 */
void
flash_upd_prom_hdr(int seg, __psunsigned_t nu_hdr)
{
	flash_header_t *nh = (flash_header_t *)nu_hdr;
	TIMEINFO *t;

	/*
	 * use the dallas nvram to keep track of number of flashes
	 */
	nh->nflashed = flash_get_nv_cnt(seg) + 1;

	cpu_get_tod((TIMEINFO *)&nh->timestamp);
	nh->hcksum = 0;
	nh->hcksum = (uint16_t)~_cksum1((void *)nh, sizeof(*nh), 0);
}

void
flash_print_prom_info(flash_header_t *h, u_short *prom)
{
	u_short cksum;
	int dallas_nflashed;

	if (h == SFLASH_RPROM_HDR_ADDR) 
	    dallas_nflashed = flash_get_nv_cnt(SFLASH_RPROM_SEG);
	else
	    dallas_nflashed = flash_get_nv_cnt(SFLASH_FPROM_SEG);    

	FPR_PR(("Header @ 0x%x: ", h));
	if ( !flash_header_ok(h)) {
	    FPR_PR(("BAD\n"));
	    FPR_PR(("num erases: (dallas %d)\n", dallas_nflashed));
	    dump_bytes((void *)h, sizeof(*h), 1);
	    return;
	}
	FPR_PR(("OK\n"));
	cksum = flash_cksum_r((char *)prom, h->datalen, 0);
	FPR_PR(("magic: \"%s\" (0x%x)\n", &h->magic, h->magic));
	FPR_PR(("num erases: %d (dallas %d)\n", h->nflashed, dallas_nflashed));
	FPR_PR(("last flashed at: %d/%d/%d %02d:%02d:%02d.%03d GMT\n",
		h->timestamp.Month, h->timestamp.Day,
		h->timestamp.Year,
		h->timestamp.Hour, h->timestamp.Minutes,
		h->timestamp.Seconds, h->timestamp.Milliseconds));
	FPR_PR(("version: %d.%d\n", h->version>>8, h->version&0xff));
	FPR_PR(("image cksum 0x%x (%d bytes) in header is %s (actual 0x%x)\n",
		h->dcksum, h->datalen,
		(cksum!=h->dcksum)?"BAD":"OK", cksum));
}

/**********************************************************************
 * Flash Persistant Data Storage Section
 **********************************************************************/

/*
 * PDS Segment Initialization and Maintenance Functions
 */

#define ERASED_UINT64 	0xffffffffffffffff

static flash_err_t
flash_setup_pds_segment(flash_pds_hdr_t *hdr, int len)
{
	uint64_t *fdp     = (uint64_t *)SFLASH_PDS_ADDR;
	uint64_t *fdp_end = (uint64_t *)SFLASH_PDS_END_ADDR;
	TIMEINFO *t;
	int i;
	flash_err_t rv;
	
	FPR_PDS(("flash_setup_pds_segment\n"));
	/*
	 * check if erased already and if not then erase segment(s)
	 */
	while (fdp < fdp_end) {

	    if (*fdp++ == ERASED_UINT64)
		continue;

	    for (i = 0; i < SFLASH_PDS_NSEGS; i++) {
		rv = flash_erase(SFLASH_PDS_SEG + i);
		if (rv) {
		    FPR_ERR(("Flash PDS(%d) Erase failed\n", i));
		    return(rv);
		}
	    } /* for */
	} /* while */
				  
	hdr->magic = FPDS_HDR_MAGIC;
	hdr->erases = flash_get_nv_cnt(SFLASH_PDS_SEG) + 1;
	/* increment the counter in dallas part */
	flash_set_nv_cnt(SFLASH_PDS_SEG, (int *)&hdr->erases);
	cpu_get_tod((TIMEINFO *)&hdr->last_flashed);
	hdr->cksum = 0;

	hdr->cksum = ~_cksum1((void *)hdr, sizeof(*hdr), 0);
	us_delay(100);
	rv = flash_io(SFLASH_CMD_WRITE, SFLASH_PDS_ADDR,
			(__uint64_t)hdr,
			(__uint64_t)SFLASH_PDS_ADDR,
			len/sizeof(uint16_t));
	us_delay(100);
	return(rv);
}

/*
 * reset the PDS flash memory area (erase and set header)
 * we will loose all entries
 */
static flash_err_t 
flash_pds_reset(void)
{
	flash_pds_hdr_t new_hdr;

	new_hdr.erases = flash_get_nv_cnt(SFLASH_PDS_SEG);
	return(flash_setup_pds_segment(&new_hdr, sizeof(new_hdr)));
}

/*
 * Flash PDS being accessed for the first time since reset/compress or
 * bootup, check the header and set free ptr
 */
vu_short *
flash_pds_init(int resetpds)
{
	flash_pds_hdr_t *hdr = (flash_pds_hdr_t *)SFLASH_PDS_ADDR;
	flash_pds_ent_t *p_end = (flash_pds_ent_t *)SFLASH_PDS_END_ADDR;
	flash_pds_ent_t *p;
	u_short cksum = 1;

	FPR_PDS(("flash_pds_init:\n"));

	if (!resetpds) {
	    if (hdr->magic == FPDS_HDR_MAGIC)
		cksum = 0xffff & ~_cksum1((void *)hdr, sizeof(*hdr), 0);
	    if (cksum) {
		flash_err_t frv;

		FPR_ERR(("WARNING: Bad/Uninitialized "
			 "Flash Persistant Data Storage Header\n"));
		FPR_ERR(("         Re-Initializing PDS\n"));
	    }
	}

	if (cksum || resetpds) {
	    flash_err_t frv;

	    pds_log_init = 0;
	    frv = flash_pds_reset();
	    if (frv) {
		flash_print_err(frv);
		return(0);
	    }
	}

	p = (flash_pds_ent_t *)(hdr + 1);
	while (p->pds_type_len != FPDS_TYPELEN_FREE && p < p_end)
	    FPDS_NEXT_ENT(p);
	if (p >= p_end)
	    p = 0;
	flash_pds_freep = (vu_short *)p;
	/*
	 * re-calcuate the current usage
	 */
	flash_pds_space_usage(&pds_usage);

	FPR_PDS(("flash_pds_init: flash_pds_freep 0x%x\n", p));
	return(flash_pds_freep);
}

void
flash_pds_init_free(void)
{
	/*REFERENCED*/
	vu_short *rv;

	if ( !flash_pds_freep)
	    rv = flash_pds_init(0);
	FPR_LO(("flash_pds_init_free rv = 0x%x\n", rv));
}

/*
 * compress the pds block
 */
vu_short *
flash_pds_compress(int flags)
{
	flash_pds_hdr_t *hdr = (flash_pds_hdr_t *)SFLASH_PDS_ADDR;
	flash_pds_hdr_t *nu_hdr;
	flash_pds_ent_t *p = 0;
	u_short *nu_p;
	uint16_t *buf;
	int hlen, len;
	int env_flg;
	pds_space_usage_t use;
	flash_err_t frv;

	FPR_PDS(("flash_pds_compress\n"));
	
	buf = (uint16_t *)align_malloc(SFLASH_PDS_SIZE, sizeof(uint64_t));
	if (!buf) {
	    FPR_ERR(("flash_pds_compress: out of malloc\n"));
	    return(0);
	}
	nu_hdr = (flash_pds_hdr_t *)buf;
	nu_p = (u_short *)(nu_hdr + 1);

	/* only info needed for new from previous hdr */
	nu_hdr->erases = hdr->erases;

	/*
	 * if recoverable space is < 10 % of flash
	 * save only the ENV types
	 */
	env_flg = FPDS_ENT_ENV_TYPE | FPDS_ENT_DAT_TYPE;
	if (PDS_INUSE < FPDS_HI_WATER_MARK)
	    env_flg |= FPDS_ENT_LOG_TYPE;
	else
	    pds_log_init = 0;		/* we are scrubbing logs re-init */

	while (p = flash_pds_ent_next(p, env_flg)) {
	    hlen = p->pds_hlen + FPDS_ENT_DATA_OFFS;
	    bcopy((void *)p, (void *)nu_p, hlen*sizeof(uint16_t)); 
	    nu_p += hlen;
	}

	len = (__psunsigned_t)nu_p - (__psunsigned_t)nu_hdr;
	frv = flash_setup_pds_segment(nu_hdr, len);
	if (frv) {
	    flash_print_err(frv);
	    free(buf);
	    return(0);
	}
	free(buf);
	return(flash_pds_init(0));
}

/*
 * PDS entry Service functions
 */

static flash_err_t
flash_pds_write(void *pds_src, vu_short *dst, int hlen)
{
	flash_err_t rv;

	/* bounds check before flash write */
	if (dst < SFLASH_PDS_ADDR || (dst+hlen) >= SFLASH_PDS_END_ADDR) {
	    FPR_ERR(("pds_write: bogus pds addr 0x%x hlen %d\n",
	    	dst, hlen));
	    return(FLASH_ERR_PDS_ADDR);
	}
	rv = flash_io(SFLASH_CMD_WRITE, dst,
		    (__uint64_t)pds_src, (__uint64_t)dst, hlen);
	return(rv);
}

/*
 * get the next pds entry base on the filter starting from
 * entry at p
 * filter bits:
 * 	FPDS_ENT_ENV_TYPE	valid env entry
 *	FPDS_ENT_LOG_TYPE	valid log entry
 *	FPDS_FILTER_ALL		any valid or invalid entry
 *
 * stop at end of pds seg space or at pds free space and return 0
 */
flash_pds_ent_t *
flash_pds_ent_next(flash_pds_ent_t *p, u_short filter)
{
	flash_pds_hdr_t *hdr = (flash_pds_hdr_t *)SFLASH_PDS_ADDR;
	flash_pds_ent_t *p_end = (flash_pds_ent_t *)SFLASH_PDS_END_ADDR;
	u_short typelen, valid;

	/*
	 * If caller wants to start from beginning p==0 and we set out ptr
	 * otherwise we start at the NEXT entry of p unless p points to 
	 * free space.
	 */
	if (!p) {
	    /*
	     * start from the beginning
	     */
	    p = (flash_pds_ent_t *)(hdr + 1);
	} else {
	    /*
	     * check if we are at PDS freespace 
	     */
	    typelen = p->pds_type_len;
	    if (typelen == FPDS_TYPELEN_FREE)
		return(0);

	    FPDS_NEXT_ENT(p);

	    /* PARANOID */
	    if (p >= p_end) {
		FPR_ERR(("Flash: Ptr 0x%x out of PDS bounds 0x%x\n",
		    p, p_end));
		return(0);	    
	    }

	}
	/*
	 * we are either the beginning or next entry after given p
	 * and this is were we start looking for entries matching filter
	 * Before we go into a loop check to see if we point to
	 * free space, and if the use wants any entry
	 */
	typelen = p->pds_type_len;
	if (typelen == FPDS_TYPELEN_FREE)
	    return(0);

	if (filter == FPDS_FILTER_ALL)
	    return(p);

	do {
	    valid = p->valid;
	    if ((valid == FPDS_ENT_VALID &&
		    (typelen & FPDS_ENT_TYPE_MASK) & filter)) {
		return(p);
	    }
	    FPDS_NEXT_ENT(p);

	    /* PARANOID */
	    if (p >= p_end) {
		FPR_ERR(("Flash: Ptr 0x%x out of PDS bounds 0x%x\n",
			p, SFLASH_PDS_END_ADDR));
		return(0);	    
	    }

	    typelen = p->pds_type_len;
	} while (typelen != FPDS_TYPELEN_FREE);
	return(0);
}

flash_err_t
flash_pds_ent_inval(flash_pds_ent_t *p)
{
	vu_short zero = 0;
	u_short type = FPDS_ENT_TYPE_MASK & p->pds_type_len;
	u_short len  =(FPDS_ENT_HLEN_MASK & p->pds_type_len)*sizeof(uint16_t);

	FPR_LO(("flash_pds_ent_inval: 0x%x\n", p));
	/* PARNOID error case check for valid or free entry */

	if (p->valid != FPDS_ENT_VALID  ||
	    p->pds_type_len == FPDS_TYPELEN_FREE) {
		FPR_ERR(("flash_pds_ent_inval: invalid entry @x0%x\n", p));
		return FLASH_ERR_PARANOID;
	}

	if (type == FPDS_ENT_ENV_TYPE) {
	    pds_usage.invalid_env_ent++;
	    pds_usage.invalid_env_bytes += len;
	    pds_usage.inuse_env_ent--;
	    pds_usage.inuse_env_bytes -= len;
	} else if (type == FPDS_ENT_LOG_TYPE) {
	    pds_usage.invalid_log_ent++;
	    pds_usage.invalid_log_bytes += len;
	    pds_usage.inuse_log_ent--;
	    pds_usage.inuse_log_bytes -= len;
	} else {
	    pds_usage.invalid_dat_ent++;
	    pds_usage.invalid_dat_bytes += len;
	    pds_usage.inuse_dat_ent--;
	    pds_usage.inuse_dat_bytes -= len;
	}
	/* invalidate the entry */
	return(flash_pds_write((void *)&zero, (void *)&p->valid, 1));
}

/*
 * reserve space in the free flash memory section
 * check for segment intialization and that we have enough space
 */
static vu_short *
flash_pds_ent_new(int hlen)
{
	flash_pds_hdr_t *hdr = (flash_pds_hdr_t *)SFLASH_PDS_ADDR;
	vu_short *p_end = SFLASH_PDS_END_ADDR;
	vu_short *p;

	FPR_LO(("flash_pds_ent_new: p 0x%x, hlen %d\n", flash_pds_freep, hlen));

	/* make sure pds is initialized */
	flash_pds_init_free();
	p = flash_pds_freep;

	if ((p + hlen) >= p_end) {
	    /*
	     * XXX compression level/mode flag
	     * what should we toss ?
	     */
	    flash_pds_compress(0/*flags*/);
	    /*
	     * flash_pds_freep has been reset, refresh p
	     */
	    p = flash_pds_freep;
	}

	flash_pds_freep += hlen;
	return(p);
}

/*
 * take data of len bytes, build a pds entry of type 
 * allocate pds space
 * and finally commit to flash pds seg in 2 writes, skipping over the
 * half word flag for valid
 */
flash_err_t
flash_pds_ent_write(u_short *data, int type, int len)
{
	flash_pds_ent_t new;
	vu_short *p;
	int ent_hlen;
	flash_err_t frv;

	FPR_PDS(("flash_pds_ent_write: p 0x%x, type 0x%x, len %d\n",
		data, type, len));
	ent_hlen = (len + 1)/sizeof(uint16_t); /* round up to even bytes */
	new.pds_type_len = ent_hlen & FPDS_ENT_HLEN_MASK;
	new.pds_type_len |= type & FPDS_ENT_TYPE_MASK;

	ent_hlen += FPDS_ENT_DATA_OFFS;		/* add hdr len */
	p = flash_pds_ent_new(ent_hlen);
	if (!p)
	    return(FLASH_ERR_PDS_NEW);

	frv = flash_pds_write((void *)&new, p, FPDS_ENT_VALID_OFFS);
	if (frv)
	    goto bail;

	/* skip over the valid field */
	p += FPDS_ENT_DATA_OFFS;
	frv = flash_pds_write((void *)data, p, new.pds_hlen); 
	if (frv)
	    flash_print_err(frv);

bail:
	len = ent_hlen * sizeof(uint16_t);
	if (type == FPDS_ENT_ENV_TYPE) {
	    pds_usage.inuse_env_bytes += len;
	    pds_usage.inuse_env_ent++;
	} else if (type == FPDS_ENT_LOG_TYPE) {
	    pds_usage.inuse_log_bytes += len;
	    pds_usage.inuse_log_ent++;
	} else {
	    pds_usage.inuse_dat_bytes += len;
	    pds_usage.inuse_dat_ent++;
	}
	pds_usage.free -= len;
	return(frv);
}

/**********
 * PDS ENV
 **********/
flash_pds_ent_t *
flash_findenv(flash_pds_ent_t *ent, char *var)
{
	int len = strlen(var);
	char *cp;

	FPR_PDS(("flash_findenv:\n"));
	while(ent = flash_pds_ent_next(ent, FPDS_ENT_ENV_TYPE)) {
	    if (strncasecmp(var, (char *)ent->data, len) == 0) {
		cp = (char *)ent->data;
	    	if (cp[len] == '=')
		    break;
	    }
	}
	return(ent);
}

/*
 * Setup access to flash memory
 */
int
flash_init(void)
{
	flash_mem_base = FLASH_MEM_BASE;
	if (!flash_ok()) {
	    flash_mem_base = FLASH_MEM_ALT_BASE;
	    if (!flash_ok()) {
		FPR_ERR(("flash part not found at 0x%x or 0x%x\n",
		    FLASH_MEM_BASE, FLASH_MEM_ALT_BASE));
		return(0);
	    }
	}
	FPR_HI(("flash_init: flash @ 0x%x = 0x%x\n", flash_mem_base));
	return(1);
}

/*
 * go through valid pds env entries and call the putstr
 * fct to build the string list for the current enviroment.
 * putstr should be new_str2()
 */
int
flash_loadenv(int (*putstr)(char *, char *, struct string_list *),
	     struct string_list *env)
{
	flash_pds_ent_t *ent = 0;
	char buf[FPDS_ENT_MAX_DATA_LEN];
	char *cp;
	int rv;

	FPR_PDS(("flash_loadenv:\n"));

	/* make sure pds is initialized */
	flash_pds_init_free();

	/* make sure there is a putstr */
	if (!putstr)
	    return(0);

	while(ent = flash_pds_ent_next(ent, FPDS_ENT_ENV_TYPE)) {
	    int i;

	    FPR_PDS(("env ent hlen %d (%s):\n", ent->pds_hlen, ent->data));
	    /*
	     * call the env stringlist call back routine with
	     * var-name and var-str parts as separate strings.
	     * This is kinda stupid since the fct (new_str2) puts
	     * back together just like we have it here as a single
	     * string. new_str1 would be best but
	     * the generic i/f uses new_str2 ..sigh
	     */
	    strcpy(buf, (char *)ent->data);
	    i = 0;
	    cp = buf;
	    while(i<FPDS_ENT_MAX_DATA_LEN && *cp != '=')
	    	cp++, i++;
	    if (*cp != '=') {
		FPR_ERR(("Bad PDS env entry at 0x%x\n", ent));
		continue;
	    }
	    *cp++ = 0;		/* terminate the var name part of string */

	    /* resetting env code calls w/o env set-up to initialize */
	    rv = (*putstr)(buf, cp, env);
	    if (rv)
		return(rv);
	}
	return(0);
}

/*
 * resetenv cmd, invalidate all pds env vars
 */
void
flash_resetenv(void)
{
	flash_err_t frv;
	flash_pds_ent_t *ent = 0;

	FPR_PDS(("flash_resetenv:\n"));
	while(ent = flash_pds_ent_next(ent, FPDS_ENT_ENV_TYPE)) {
	    frv = flash_pds_ent_inval(ent);
	    if (frv)
		flash_print_err(frv);
	}
}

int
flash_setenv(char *var, char *str)
{
	flash_pds_ent_t *p;
	u_short pds_data[FPDS_ENT_MAX_DATA_HLEN];
	int type;
	int rv = EINVAL;
	int len;
	int unset = (str == 0) || (*str == '\0');

	FPR_PDS(("flash_setenv: var=%s str=%s\n", var, (str)?str:"NULL"));
	if ( !unset) {
	    len = sprintf((char *)pds_data, "%s=%s", var, str);
	    len++;		/* for the null byte string terminator */
	    if (len > FPDS_ENT_MAX_DATA_LEN) {
		FPR_ERR(("string too long (%d), > %d\n",
			len, FPDS_ENT_MAX_DATA_LEN));
		return(rv);
	    }
	}

	/*
	 * look for valid pds env/penv entry matching var
	 * If there is a valid entry and str is different
	 * invalidate it.
	 */
	if (p = flash_findenv(0, var)) {
	    flash_err_t frv;

	    if (!unset) {
		char *cp = (char *)p->data;

		while(*cp && *cp != '=')
		    cp++;
	    	if (!strcmp(str, ++cp))
		    return 0;
	    }
	    frv = flash_pds_ent_inval(p);
	    if (frv)
		return(EIO);
	    rv = 0;
	}
#ifdef FDEBUG
	if (rv == 0) {
	    /* PARANOID make sure there is only 1 valid var */
	    p = 0;
	    while (p = flash_findenv(p, var)) {
		 FPR_PDS(("BOGUS: multiple env var %s @0x%x invalidating ...\n",
		    var, p));
		 flash_pds_ent_inval(p);
	    }
	}
#endif
	if (unset)
	    return(0);

	if (flash_pds_ent_write(pds_data, FPDS_ENT_ENV_TYPE, len))
	    rv = EIO;
	return(rv);
}

/**********
 * PDS LOG
 **********/
/*
 * log some data to the flash pds
 * log types:
 * PDS_LOG_TYPE0:
 * 	special entry for the first log. This entry will 
 *	enter the current system tick, timeofday timestamp,
 *	and tick to usecs multiplier-divider
 */
flash_err_t
flash_pds_write_log(uint64_t log_type,
		    uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4)
{
	flash_pds_log_data_t log_data;
	int len;

	if (pds_log_init == 0) {
	    flash_err_t frv;

	    pds_log_init = 1;
	    frv = flash_pds_write_log(PDS_LOG_TYPE0, 0, 0, 0, 0);
	    if (frv)
		return(frv);
	}
	log_data.log_hdr = PDS_LOG_TICK_MASK & HEART_PIU_K1PTR->h_count |
			   PDS_LOG_TYPE_MASK & (log_type << PDS_LOG_TYPE_SHFT);

	switch (log_type) {
	case PDS_LOG_TYPE0: {
		/*
		 * current tod
		 */
		cpu_get_tod(&log_data.u.init.tod);
		/*
		 * tick to usecs multiplier/divider
		 */
		log_data.u.init.tick_mult = HEART_COUNT_NSECS;
		log_data.u.init.tick_div = 1000;
		len = sizeof(TIMEINFO) + sizeof(log_data.u.w[0]) * 2;
		break;
	}
	case PDS_LOG_TYPE1: {
		char *s = (char *)a1;

		len = strlen(s);
		if (len >= FPDS_ENT_MAX_DATA_LEN) {
		    len = FPDS_ENT_MAX_DATA_LEN;
		    s[FPDS_ENT_MAX_DATA_LEN-1] = 0;
		} else
		    len++;		/* NULL byte str terminator */
		bcopy(s, log_data.u.c, len);
		break;
	}
	case PDS_LOG_TYPE2:
	default: {
		log_data.u.d[0] = a1;
		log_data.u.d[1] = a2;
		log_data.u.d[2] = a3;
		log_data.u.d[3] = a4;

		len = sizeof(uint64_t)*4;
		break;
	}
	}
	len += sizeof(log_data.log_hdr);
	return(flash_pds_ent_write((u_short *)&log_data,
				   FPDS_ENT_LOG_TYPE, len));
}

#include <stdarg.h>
void
flash_pds_prf(char *fmt, ...)
{
	va_list ap;
	char buf[256];

	/*CONSTCOND*/
	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);

	flash_pds_log(buf);
}

void
flash_pds_log(char *str)
{
	flash_err_t frv;

    	frv = flash_pds_write_log(PDS_LOG_TYPE1, (uint64_t)str, 0, 0, 0);
	if (frv) {
	    FPR_ERR(("flash_pds_log: flash_pds_write_log err\n"));
	    flash_print_err(frv);
	}
}

flash_pds_ent_t *
flash_pds_get_log(flash_pds_ent_t *p, char **data, int *len, int inval)
{
	flash_pds_hdr_t *hdr = (flash_pds_hdr_t *)SFLASH_PDS_ADDR;
    	flash_pds_ent_t *p_end = (flash_pds_ent_t *)SFLASH_PDS_END_ADDR;
	flash_pds_ent_t *cur; 

	FPR_LO(("flash_pds_get_log:\n"));
	if (p) {
	    u_short type;

	    /*
	     * do the best we can to ensure a good entry ptr
	     * addr range, valid & type check
	     */
	    if (p < (flash_pds_ent_t *)(hdr + 1) || p >= p_end) {
		FPR_ERR(("pds_get_log: bogus pds addr 0x%x\n", p));
		return(0);
	    }
	    if (p->pds_type_len == FPDS_TYPELEN_FREE)
		return(0);

	    type = p->pds_type_len & FPDS_ENT_TYPE_MASK;
	    if (p->valid != FPDS_ENT_VALID ||
		(type != FPDS_ENT_ENV_TYPE && type != FPDS_ENT_LOG_TYPE)) {
		FPR_ERR(("pds_get_log: bad entry - valid=0x%x type=0x%x\n",
			p->valid, type));
		return(0);
	    }
	} else {
	    p = flash_pds_ent_next(p, FPDS_ENT_LOG_TYPE);
	    if (!p)
		return(0);
	}
	*data = (char *)p->data;
	*len = p->pds_hlen*sizeof(uint16_t);
	if (inval)
	    flash_pds_ent_inval(p);
	return(flash_pds_ent_next(p, FPDS_ENT_LOG_TYPE));
}

/*
 * take a ptr to PDS log entry, and print it in a reasonable format.
 * NOTE: this is the simplest print, it certainly can be made much nicer
 */
void
flash_print_log(void *data, int len)
{
	flash_pds_log_data_t log_data;
	uint64_t us, us_mod;

	bcopy(data, (void *)&log_data, len);

	/*
	 * we can use the tick_mult and tick_div in PDS_LOG_TYPE0
	 * but since we know what H/W take the short cut.
	 */
	us = (log_data.log_hdr & PDS_LOG_TICK_MASK) * HEART_COUNT_NSECS; 
	us += 500;		/* rounding */
	us /= 1000;		/* in rounded usecs */
	us_mod = us%1000000;	/* usec in modulo seconds */
	FPR_PR(("%7d.%03d,%03d: ", us/1000000,us_mod/1000,us_mod%1000)); 

	len -= sizeof(log_data.log_hdr);
	switch (log_data.log_hdr >> PDS_LOG_TYPE_SHFT) {
	case PDS_LOG_TYPE0: {
	    TIMEINFO *t = (TIMEINFO *)&log_data.u.init.tod;
	    int mult = log_data.u.init.tick_mult;
	    int div = log_data.u.init.tick_div;

	    FPR_PR(("Log Init: %d/%d/%d %02d:%02d:%02d.%03d GMT\n",
		    t->Month, t->Day, t->Year,
		    t->Hour, t->Minutes, t->Seconds, t->Milliseconds));
	    FPR_PR(("                     log tick multiplier(%d)/divider(%d)\n",
		    mult, div));
	    break;
	}
	case PDS_LOG_TYPE1: {
	    FPR_PR(("T1: %s\n", log_data.u.c));
	    break;
	}
	case PDS_LOG_TYPE2:
	default: {
	    FPR_PR(("T2: 0x%x 0x%x 0x%x 0x%x\n",
		    log_data.u.d[0], log_data.u.d[1],
		    log_data.u.d[2], log_data.u.d[3]));
	    break;
	}
	}
}
/**********
 * PDS DATA
 **********/
/*
 * Each PDS entry can hold max 512 bytes not counting
 * the 4 byte overhead (typelen and valid fields).
 * The max size of the PDS DATA type entry is 16 Kbytes.
 * To accomodate the large sized PDS DATA entries,
 * there PDS DATA entries will be one of 2 types, DA0 or DA1
 * these entries must be layed out in the PDS segment in
 * DA0 [ DA1 [ DA1 [ ... ]]] manner.
 */
/*
 * find the PDS entry containing the first entry (DA0) of
 * the <key> data entry(s) (DA0 & DA1s) starting from ent,
 * return the pds ent ptr.
 * if key==0, then first DA0 starting from ent will be returned 
 * if ent==0, then the search will start from beginning of PDS
 * ent MUST point to a PDS entry.
 * 
 * If there are addition DA1 type entries in use to
 * hold this data then they should be located
 * right after this entry in sequence.
 */
flash_pds_ent_t *
flash_pds_find_data(char *key, flash_pds_ent_t *ent)
{
	flash_pds_da0_t *da0;
	char *ekey;

    	while(ent = flash_pds_ent_next(ent, FPDS_ENT_DA0_TYPE)) {
	    da0 = (flash_pds_da0_t *)ent->data;
	    ekey = (char *)(da0 + 1);
	    FPR_PDS(("flash_pds_find_data: find %s, entry %s(%d)\n",
	    	key, ekey, da0->datalen));
	    if (key == 0)
		break;
	    if (strcmp(key, ekey) == 0)
		break;
	}
	return(ent);
}

/*
 * invalidate the pds data type entry by finding the first
 * (DA0) type entry and then all following sequential DA1 type entry(s). 
 */
void
flash_pds_inval_data(char *key)
{
    	flash_pds_ent_t *old;
    	flash_pds_ent_t *ent = flash_pds_find_data(key, 0);
    	flash_pds_ent_t *p_end = (flash_pds_ent_t *)SFLASH_PDS_END_ADDR;

	if (!ent)
	    return;

	do {
	    old = ent;
	    FPDS_NEXT_ENT(ent);
	    flash_pds_ent_inval(old);
	} while(ent < p_end && FPDS_ENT_TYPE(ent) == FPDS_ENT_DA1_TYPE);
	FPR_PDS(("PDS DATA: invalidated %s\n", key));
}

/*
 * copy the data from pds data entry matching the key string
 * if cksum on verify the cksum and return w/o copying the data
 * The caller MUST have buf space large enough to accomdate
 * the datalen bytes.
 */
int
flash_pds_copy_data(char *key, char *buf, int cksum)
{
	char *datap = buf;
	char *cptr;
    	int len, datalen, cplen;
	flash_pds_da0_t *da0;
    	flash_pds_ent_t *ent0 = flash_pds_find_data(key, 0);
    	flash_pds_ent_t *ent = ent0;
    	flash_pds_ent_t *p_end = (flash_pds_ent_t *)SFLASH_PDS_END_ADDR;

	if (ent0 == 0)
	    return(0);

	/*
	 * prepare to copy the data out of DA0, skipping the
	 * da0 hdr and key
	 */
	da0 = (flash_pds_da0_t *)ent0->data;
	if (da0->datalen > FPDS_ENT_DAT_MAX_LEN) {
	    FPR_ERR(("Bad PDS data size, %d > %d",
	    	da0->datalen, FPDS_ENT_DAT_MAX_LEN));
	    return(0);
	}
	cptr = (char *)(da0 + 1);
	/*
	 * skip the key str and possible byte pad
	 */
	cptr += strlen(key);
	if ((__psunsigned_t)cptr & 1)
	    cptr += 1;
	else
	    cptr += 2;
	cplen = FPDS_ENT_LEN(ent);
	cplen -= (int)(cptr - (char *)da0);
	if (cplen - 1 == da0->datalen)
	    cplen -= 1;
	    
	datalen = 0;

    	do {
	    if (datalen >= da0->datalen)	/* PARANOIA */
		break;
	    /*
	     * the PDS entries are always stored in even bytes, 16bit
	     * boundaries. So with odd datalen there is 1 extra byte of pad
	     * which we want to subtract when the actual data is copied to
	     * user buffer (key is already padded out to even bytes).
	     */
	    if (cplen - 1 == da0->datalen - datalen)
		cplen -= 1;
	    FPR_PDS(("copy: 0x%x -> 0x%x %d bytes\n", cptr, datap, cplen));
	    bcopy(cptr, datap, cplen);
	    datap += cplen; 
	    datalen += cplen; 

	    FPDS_NEXT_ENT(ent);
	    cplen = FPDS_ENT_LEN(ent);
	    cptr = (char *)ent->data;

	} while(ent < p_end && FPDS_ENT_TYPE(ent) == FPDS_ENT_DA1_TYPE);

	if (datalen != da0->datalen) {
	    FPR_ERR(("PDS datalen error %d, actual %d\n",
	    	da0->datalen, datalen));
	}

	if (cksum) {
	    if (da0->datasum != flash_cksum_r(buf, datalen, 0)) {
		FPR_ERR(("PDS data cksum error, for %s (%d bytes)\n",
		    key, datalen));
		datalen = 0;
	    }
	}
	return(datalen);
}

/*
 * create PDS data entry(s) for arbitaray data of datalen bytes
 * each PDS data entry can be 512 bytes max, and these
 * can be used for a max datalen of 16K bytes.
 */
flash_err_t
flash_pds_set_data(char *key, char *data, int datalen)
{
	int entlen, keylen, len, actual_use;
	flash_pds_ent_t *ent;
	char *datap;
	u_short *buf, sum;
	flash_pds_da0_t *da0;
	flash_err_t ferr;

	/*
	 * check lengths
	 */
	if (datalen > FPDS_ENT_DAT_MAX_LEN) {
	    return(FLASH_ERR_ENT_TOO_LONG);
	}
	keylen = strlen(key);
	if (keylen > FPDS_ENT_MAX_DATA_LEN) {
	    return(FLASH_ERR_STR_TOO_LONG);
	}
	/*
	 * ensure total space for key is even bytes
	 */
	if (keylen & 1)
	    keylen += 1;	/* null terminator only */
	else
	    keylen += 2;	/* null terminator and byte pad */
	len = keylen + datalen;
	entlen = sizeof(flash_pds_da0_t);	/* da0 datalen and cksum */
	len += entlen;				/* add da0 header */
	/*
	 * check if there is space, for total and pds entry overhead
	 * minus the amount we will recover if we need to invalidate an
	 * exiting entry with the same key
	 */
	actual_use = (len/FPDS_ENT_MAX_LEN) + 1;	/* num entries needed */
	actual_use *= FPDS_ENT_DATA_OFFS * sizeof(uint16_t);	/* overhead */

	actual_use += PDS_INUSE + len;
	/*
	 * if we are to recover any space subtract that
	 */
	if (ent = flash_pds_find_data(key, 0)) {
	    actual_use -= ((flash_pds_da0_t *)ent->data)->datalen;
	    if (actual_use > FPDS_HI_WATER_MARK) {
		return(FLASH_ERR_OUT_OF_MEM);
	    } 
	} 

	buf = (u_short *)malloc(FPDS_ENT_MAX_LEN);
	if (!buf) {
	    return(FLASH_ERR_OUT_OF_MEM);
	}
	/*
	 * if an entry exist for key, invalidate it
	 */
	flash_pds_inval_data(key);
	/*
	 * the first PDS entry DA0 is special, extra header
	 * and the key str need to be prepended to the data
	 */
	da0 = (flash_pds_da0_t *)buf;
	da0->datalen = datalen;
	da0->datasum = flash_cksum_r(data, datalen, 0);		/* cksum */
	datap = (char *)(da0 + 1);

	strcpy(datap, key);
	datap += keylen;
	entlen += keylen;

	if (len <= FPDS_ENT_MAX_LEN) {
	    bcopy(data, datap, datalen);
	    entlen = len;
	    len = 0;
	} else {
	    bcopy(data, datap, FPDS_ENT_MAX_LEN-entlen);
	    datap = data + (FPDS_ENT_MAX_LEN-entlen);
	    entlen = FPDS_ENT_MAX_LEN;
	    len -= FPDS_ENT_MAX_LEN;
	}
	ferr = flash_pds_ent_write(buf, FPDS_ENT_DA0_TYPE, entlen);
	free(buf);
	if (ferr) {
	    return(ferr);
	}

	while (len) {
	    if (len > FPDS_ENT_MAX_LEN) {
		entlen = FPDS_ENT_MAX_LEN;
		len -= FPDS_ENT_MAX_LEN;
	    } else {
		entlen = len;
		len = 0;
	    }
	    ferr = flash_pds_ent_write((void *)datap,FPDS_ENT_DA1_TYPE,entlen);
	    if (ferr) {
		return(ferr);
	    }
	    datap += entlen;
	}
	return(0);
}

/**********************************************************************
 * Flash Misc fcts
 **********************************************************************/
/*
 * caculate current pds space usage
 */
void
flash_pds_space_usage(pds_space_usage_t *use)
{
	flash_pds_ent_t *ent;
	__psunsigned_t pds_end = (__psunsigned_t)SFLASH_PDS_END_ADDR;

	bzero((void *)use, sizeof(*use));

	use->free = (__psunsigned_t)pds_end - (__psunsigned_t)flash_pds_freep;
	ent = 0;
	while (ent = flash_pds_ent_next(ent, FPDS_FILTER_ALL)) {

	    switch (ent->pds_type_len & FPDS_ENT_TYPE_MASK) {
	    case FPDS_ENT_ENV_TYPE:
		if (ent->valid) {
		    use->inuse_env_ent++;
		    use->inuse_env_bytes += (ent->pds_hlen +
		    		FPDS_ENT_DATA_OFFS) * sizeof(uint16_t);
		} else {
		    use->invalid_env_ent++;
		    use->invalid_env_bytes += (ent->pds_hlen +
		    		FPDS_ENT_DATA_OFFS) * sizeof(uint16_t);
		}
		break;

	    case FPDS_ENT_LOG_TYPE:
		if (ent->valid) {
		    use->inuse_log_ent++;
		    use->inuse_log_bytes += (ent->pds_hlen +
				FPDS_ENT_DATA_OFFS) * sizeof(uint16_t);
		} else {
		    use->invalid_log_ent++;
		    use->invalid_log_bytes += (ent->pds_hlen +
				FPDS_ENT_DATA_OFFS) * sizeof(uint16_t);
		}
		break;

	    case FPDS_ENT_DA0_TYPE:
	    case FPDS_ENT_DA1_TYPE:
		if (ent->valid) {
		    use->inuse_dat_ent++;
		    use->inuse_dat_bytes += (ent->pds_hlen +
				FPDS_ENT_DATA_OFFS) * sizeof(uint16_t);
		} else {
		    use->invalid_dat_ent++;
		    use->invalid_dat_bytes += (ent->pds_hlen +
				FPDS_ENT_DATA_OFFS) * sizeof(uint16_t);
		}
		break;
	    }
	}
}

void
flash_pds_print_usage(pds_space_usage_t *use)
{
	int unfree;

	FPR_PR(("\nPDS usage stats: PDS SIZE :%d\n", SFLASH_PDS_SIZE));
	FPR_PR(("  env entries inuse         :%3d (%d bytes)\n",
		use->inuse_env_ent, use->inuse_env_bytes));
	FPR_PR(("  log entries inuse         :%3d (%d bytes)\n",
		use->inuse_log_ent, use->inuse_log_bytes));
	FPR_PR(("  dat entries inuse         :%3d (%d bytes)\n",
		use->inuse_dat_ent, use->inuse_dat_bytes));
	FPR_PR(("  env entries retired       :%3d (%d bytes)\n",
		use->invalid_env_ent, use->invalid_env_bytes));
	FPR_PR(("  log entries retired       :%3d (%d bytes)\n",
		use->invalid_log_ent, use->invalid_log_bytes));
	FPR_PR(("  dat entries retired       :%3d (%d bytes)\n",
		use->invalid_dat_ent, use->invalid_dat_bytes));

	FPR_PR(("  total pds entries inuse   :%3d (%d bytes)\n",
		use->inuse_env_ent +
		use->inuse_log_ent + use->inuse_dat_ent,
		use->inuse_env_bytes +
		use->inuse_log_bytes + use->inuse_dat_bytes));

	unfree = use->inuse_env_bytes +
		 use->inuse_log_bytes +
		 use->inuse_dat_bytes;

	FPR_PR(("  total pds entries retired :%3d (%d bytes)\n",
		use->invalid_env_ent +
		use->invalid_log_ent + use->invalid_dat_ent,
		use->invalid_env_bytes +
		use->invalid_log_bytes + use->invalid_dat_bytes));

	unfree += use->invalid_env_bytes +
		  use->invalid_log_bytes +
		  use->invalid_dat_bytes;

	FPR_PR(("free space %d, used space %d, hdr %d, total %d\n",
		use->free, unfree, sizeof(flash_pds_hdr_t),
		use->free + unfree + sizeof(flash_pds_hdr_t) ));
}

void
flash_print_pds_status(int check)
{
	flash_pds_hdr_t *hdr = (flash_pds_hdr_t *)SFLASH_PDS_ADDR;
	pds_space_usage_t usage;
	int unfree;
	int hdr_ok;
	int dallas_nflashed;
	char *cp;

	dallas_nflashed = flash_get_nv_cnt(SFLASH_PDS_SEG);

	FPR_PR(("PDS Header: is "));
	if (!flash_pds_hdr_ok(hdr)) {
	    FPR_PR(("BAD\n"));
	    FPR_PR(("num erases: (dallas: %d)\n", dallas_nflashed));
	    dump_bytes((char *)hdr, sizeof(*hdr), 1);
	    return;
	}

	FPR_PR(("OK\n"));
	FPR_PR(("num erases: %d (dallas: %d)\n",
		 hdr->erases, dallas_nflashed));
	FPR_PR(("magic: \"%s\" (0x%x)\n", &hdr->magic, hdr->magic));
	FPR_PR(("last flashed at: %d/%d/%d %02d:%02d:%02d.%03d GMT\n",
		hdr->last_flashed.Month, hdr->last_flashed.Day,
		hdr->last_flashed.Year,
		hdr->last_flashed.Hour, hdr->last_flashed.Minutes,
		hdr->last_flashed.Seconds, hdr->last_flashed.Milliseconds));

	flash_pds_print_usage(&pds_usage);

	if (check) {
	    FPR_PR(("\nUsage stats check\n"));
	    flash_pds_space_usage(&usage);
	    flash_pds_print_usage(&usage);
	}
}

/*
 * cksum the bytes in sum -r style
 */
unsigned short
flash_cksum_r(char *cp, int len, unsigned short sum)
{
	while (len--) {
	    if (sum & 0x0001)
		sum = (sum >> 1) | 0x8000;
	    else
		sum >>= 1;
	    sum += *cp++;
	}
	return(sum);
}

/*
 * dump data in hex/ascii format
 */
void
dump_bytes(char *ptr, int count, int flag /*show addr*/)
{
#ifdef FDEBUG
#define DOT 0x2e	/* "." */
#define SP 0x20		/* " " */

	u_int n = 0, hx;
	char *end, nlx=0;
	char aline[32], hexline[80];

	aline[16] = 0;

	end = ptr + count;

	if ((__psunsigned_t)ptr & 0xf) {
	    if(flag)
		hx = sprintf(hexline , "%08x:", (__psunsigned_t)ptr & ~0xf);
	    else
		hx = 0;
	    while (n != ((__psunsigned_t)ptr & 0xf)) {
		if ((n & 3) == 0)
		    sprintf(&hexline[hx], ":  ");
		else
		    sprintf(&hexline[hx], "   ");
		hx += 3;
		aline[(n & 0xf)] = SP;
		n++;
	    }
	    nlx = '\n';
	}
	while (ptr != end) {
	    char c;

	    if ((n & 0xf) == 0) {
		if(flag)
		    hx = sprintf(hexline, "%c%08x:", nlx, (__psunsigned_t)ptr);
		else
		    hx = sprintf(hexline, "%c", nlx);
		nlx = '\n';
	    }
	    c = *ptr++;
	    if (c >= 0x20 && c <= 0x7e)
		aline[(n & 0xf)] = c;
	    else
		aline[(n & 0xf)] = DOT;
	    if ((n & 3) == 0)
		sprintf(&hexline[hx], ":%02x", c);
	    else
		sprintf(&hexline[hx], " %02x", c);
	    hx += 3;
	    if ((n & 0xf) == 0xf)
		printf("%s    %s", hexline, aline);
	    n++;
	}
	if (n & 0xf) {
	    while (n & 0xf) {
		sprintf(&hexline[hx], "   ");
		hx += 3;
		aline[(n & 0xf)] = SP;
		n++;
	    }
	    printf("%s    %s", hexline, aline);
	}
	printf("\n");
#endif /* FDEBUG */
}
#endif	/* defined(IP30) */
