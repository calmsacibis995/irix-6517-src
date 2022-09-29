#include <sys/types.h>
#include <sys/pda.h>
#include <sys/immu.h>
#include <ksys/vproc.h>
#include <sys/cmn_err.h>
#include <sys/cpu.h>
#include <sys/sysmacros.h>
#include <sys/inst.h>
#include <sys/kabi.h>
#include <sys/systm.h>
#include <sys/debug.h>

/*
 * NOTE: All the pio operations on IP32 need to go through the 
 * pciio_pio_{read,write}{8,16,32,64}() interfaces. This is to workaround
 * the CRIME->MACE  phantom pio read problem. This header file contains
 * the prototypes for these routines.
 */
#include <sys/PCI/pciio.h>


extern int get_r4k_config(void);
extern int two_set_pcaches;
extern short kdebug;

_crmreg_t crm_mem_error_addr;
_crmreg_t crm_mem_error_stat;
_crmreg_t crm_cpu_error_addr;
_crmreg_t crm_cpu_error_stat;
_crmreg_t crm_vice_error_addr;
_crmreg_t crm_mem_error_ecc_chk;
_crmreg_t crm_mem_error_ecc_syn;
char bad_slot_string[256];
static int *bad_addr_pages = 0;

static int dobuserr_common(struct eframe_s *, inst_t *, uint);

#if ((! defined(R10000)) || defined(R4000))
/* tlb_to_phys
 * 
 * probe tlb for virtual address vaddr, returning its physical
 * address and whether it is cached or not.
 *
 * XXX check tlbvalid bit?
 *
 * return 1 if found in tlb, otherwise 0.
 */
extern int tlbgetlo(int,caddr_t);

int
tlb_to_phys(k_machreg_t vaddr, paddr_t *paddr, int *cached)
{
	int tlblo;
	uint pfn;
	int	pid;

	/* kernel direct */
	if (IS_KSEGDM(vaddr)) {
		*paddr = KDM_TO_PHYS(vaddr);
		*cached = IS_KSEG0(vaddr) ? 1 : 0;
		return 1;
	} else if (IS_KSEG2(vaddr)) /* kernel mapped */

		pid = 0;
	else			/* user mapped */
		pid = tlbgetpid();

	if ((tlblo = tlbgetlo(pid, (caddr_t) vaddr)) == -1)
		return 0;

	pfn = (tlblo & TLBLO_PFNMASK) >> TLBLO_PFNSHIFT;
	pfn >>= PGSHFTFCTR;
	*paddr = (paddr_t) (ctob(pfn) + poff(vaddr));
	*cached = ((((tlblo & TLBLO_CACHMASK) >> TLBLO_CACHSHIFT) == 
			TLBLO_UNCACHED) ? 0 : 1);
	return 1;
}

unsigned int
r_phys_word_erl(paddr_t x)
{
	return((unsigned int) *((volatile unsigned int *) x));
}

static int
ecc_cache_line_size(int cache_id)
{
	switch (cache_id) {
	case CACH_PI:
		return(1 << (4 + ((get_r4k_config() & CONFIG_IB) >> CONFIG_IB_SHFT)));

	case CACH_PD:
		return(1 << (4 + ((get_r4k_config() & CONFIG_DB) >> CONFIG_DB_SHFT)));

#ifndef R10000
	case CACH_SI:
#endif /* R10000 */
	case CACH_SD:
	default:
		return(1 << (4 + ((get_r4k_config() & CONFIG_SB) >> CONFIG_SB_SHFT)));
	}
}

static int
ecc_cache_block_mask(int cache_id)
{
	if ((get_r4k_config() & CONFIG_SC) == 0) /* 0 == scache present */
		return(~(ecc_cache_line_size(CACH_SD) - 1));
	switch (cache_id) {
	case CACH_PI:
#ifndef R10000
	case CACH_SI:
#endif /* R10000 */
		return(~(ecc_cache_line_size(CACH_PI) - 1));
	case CACH_PD:
	case CACH_SD:
	default:
		return(~(ecc_cache_line_size(CACH_PD) - 1));
	}
}


int
ecc_same_cache_block(int cache_id,paddr_t a, paddr_t b)
{
	int	line_mask = ecc_cache_block_mask(cache_id);

	return((a & line_mask) ==
	       (b & line_mask));
}


#define LOAD_INSTR  1
#define STORE_INSTR 2
#define	EFRAME_REG(efp,reg)	(((k_machreg_t *)(efp))[reg])
#define REGVAL(efp,x)       ((x)?EFRAME_REG((efp),(x)+EF_AT-1):0)

/* Decode enough of an instruction to determine the address it
 * references, the width of the access, and whether it's a load
 * or a store.
 *
 * Return 1 if instruction is decoded and one we can emulate, 0 if not.
 */
int
decode_inst(eframe_t *ep, int instruction, int *ldst, k_machreg_t *vaddr, int *width)
{
    union mips_instruction inst;
    k_machreg_t addr;

    inst.word = (unsigned) instruction;
    addr = REGVAL(ep, inst.i_format.rs) +
	    inst.i_format.simmediate;
    *vaddr = addr;

    switch (inst.i_format.opcode) {
    /* Loads */
    case ld_op:
	*width = 8;
	*ldst = LOAD_INSTR;
	break;
    case lwu_op:
	*width = 4;
	*ldst = LOAD_INSTR;
	break;
    case lw_op:
	*width = 4;
	*ldst = LOAD_INSTR;
	break;
    case lhu_op:
    case lh_op:
	*width = 2;
	*ldst = LOAD_INSTR;
	break;
    case lbu_op:
    case lb_op:
	*width = 1;
	*ldst = LOAD_INSTR;
	break;
    /* Stores */
    case sd_op:
	*width = 8;
	*ldst = STORE_INSTR;
	break;
    case sw_op:
	*width = 4;
	*ldst = STORE_INSTR;
	break;
    case sh_op:
	*width = 2;
	*ldst = STORE_INSTR;
	break;
    case sb_op:
	*width = 1;
	*ldst = STORE_INSTR;
	break;

    /* Cop1 instructions */
    case lwc1_op:
	*width = 4;
	*ldst = LOAD_INSTR;
	break;
    case ldc1_op:
	*width = 8;
	*ldst = LOAD_INSTR;
	break;
    case swc1_op:
	*width = 4;
	*ldst = STORE_INSTR;
	break;
    case sdc1_op:
	*width = 8;
	*ldst = STORE_INSTR;
	break;

    /* Unaligned load/stores */
    case ldl_op:
	*width = 8 - (*vaddr & 0x7);
	*ldst = LOAD_INSTR;
	break;
    case ldr_op:
	*width = (*vaddr & 0x7) + 1;
	*ldst = LOAD_INSTR;
	break;
    case lwl_op:
	*width = 4 - (*vaddr & 0x3);
	*ldst = LOAD_INSTR;
	break;
    case lwr_op:
	*width = (*vaddr & 0x3) + 1;
	*ldst = LOAD_INSTR;
	break;
    case sdl_op:
	*width = 8 - (*vaddr & 0x7);
	*ldst = STORE_INSTR;
	break;
    case sdr_op:
	*width = (*vaddr & 0x7) + 1;
	*ldst = STORE_INSTR;
	break;
    case swl_op:
	*width = 4 - (*vaddr & 0x3);
	*ldst = STORE_INSTR;
	break;
    case swr_op:
	*width = (*vaddr & 0x3) + 1;
	*ldst = STORE_INSTR;
	break;

    /* Load linked/store conditional */
    case lld_op:
    case scd_op:
    case ll_op:
    case sc_op:

    default:
	return 0;
    }

    return 1;
}
#endif /* ((! defined(R10000)) || defined(R4000)) */

caddr_t
map_physaddr(paddr_t paddr)
{
    caddr_t kvaddr;

    if (paddr < 0x20000000)		/* no need to map */
	return((caddr_t) PHYS_TO_K1(paddr));

    if (!bad_addr_pages)
	bad_addr_pages = kvalloc(cachecolormask+1, VM_VACOLOR|VM_UNCACHED, 0);

    kvaddr = (caddr_t)bad_addr_pages + (NBPP * colorof(paddr)) + poff(paddr);

    pg_setpgi(kvtokptbl(kvaddr),
	      mkpde(PG_VR|PG_G|PG_SV|PG_M|PG_N, btoct(paddr)));
    tlbdropin(0, kvaddr, kvtokptbl(kvaddr)->pte);

    return kvaddr;
}

/* Unmap a physical address. */
void
unmap_physaddr(caddr_t kvaddr)
{
    if (IS_KSEG1(kvaddr)) 		/* no need to unmap */
	return;

    pg_setpgi(kvtokptbl(kvaddr), mkpde(PG_G, 0));
    unmaptlb(0, btoct((unsigned long) kvaddr));

    return;
}

static void
err_save_regs(void)
{
	crm_cpu_error_stat = READ_REG64(PHYS_TO_K1(CRM_CPU_ERROR_STAT), 
					_crmreg_t);
	crm_cpu_error_addr = READ_REG64(PHYS_TO_K1(CRM_CPU_ERROR_ADDR),
					_crmreg_t);
	crm_mem_error_stat = READ_REG64(PHYS_TO_K1(CRM_MEM_ERROR_STAT),
					_crmreg_t);
	crm_mem_error_addr = READ_REG64(PHYS_TO_K1(CRM_MEM_ERROR_ADDR),
					_crmreg_t);
	crm_vice_error_addr = READ_REG64(PHYS_TO_K1(CRM_VICE_ERROR_ADDR),
					 _crmreg_t);
	crm_mem_error_ecc_chk = READ_REG64(PHYS_TO_K1(CRM_MEM_ERROR_ECC_CHK),
					   _crmreg_t);
	crm_mem_error_ecc_syn = READ_REG64(PHYS_TO_K1(CRM_MEM_ERROR_ECC_SYN),
					   _crmreg_t);

	WRITE_REG64(0LL, PHYS_TO_K1(CRM_CPU_ERROR_STAT), _crmreg_t);
	WRITE_REG64(0LL, PHYS_TO_K1(CRM_MEM_ERROR_STAT), _crmreg_t);
}


/*ARGSUSED*/
int 
ecc_error_correction(paddr_t paddr, _crmreg_t chkbits, 
		     _crmreg_t syn, _crmreg_t stat)
{
/*
 * since RMW errors are not dependably fixed, we need to 
 * scrub the location as if the error occured on a read.
 */
#define CORRECTABLE_ECC_ERROR (CRM_MEM_ERROR_SOFT_ERR | \
                               CRM_MEM_ERROR_MEM_ECC_RD | \
			       CRM_MEM_ERROR_MEM_ECC_RMW)
#define RD_ECC_ERR (CRM_MEM_ERROR_SOFT_ERR | CRM_MEM_ERROR_MEM_ECC_RD)
#define RMW_ECC_ERR (CRM_MEM_ERROR_SOFT_ERR | CRM_MEM_ERROR_MEM_ECC_RMW)

	if (stat & CRM_MEM_ERROR_HARD_ERR)
		return(0);

	/*
	 * double word align physical address and turn
	 * on bit 30 since paddr may (probably does) come
	 * from CRM_MEM_ERROR_ADDR which only reports bits
	 * 29:0 of the error address.
	 */
	paddr &= ~0x7;
	paddr |= 0x40000000;
	
	if (((stat & CORRECTABLE_ECC_ERROR) == RD_ECC_ERR) ||
	    ((stat & CORRECTABLE_ECC_ERROR) == RMW_ECC_ERR)) {
	        int s = splecc();
		caddr_t vaddr = map_physaddr(paddr);
		scrub_memory(vaddr);
		WRITE_REG64(0LL, PHYS_TO_K1(CRM_MEM_ERROR_STAT), _crmreg_t);
		splxecc(s);
		unmap_physaddr(vaddr);
		return(1);
	}

	/*
	 * it wasn't an error we could correct.
	 */
	return(0);
}

/*
 * This is the ECC syndrome bits table for R4000 processors.
 * See the R4000 User's Manual for the source of this table
 * and how to interpret it.
 */
static const unsigned char ecc_syn_data[64] = {		/* data bit: */
    0x13, 0x23, 0x43, 0x83,  0x2f, 0xf1, 0x0d, 0x07,	/* 00 - 07 */
    0xd0, 0x70, 0x4f, 0xf8,  0x61, 0x62, 0x64, 0x68,	/* 08 - 15 */
    0x1c, 0x2c, 0x4c, 0x8c,  0x15, 0x25, 0x45, 0x85,	/* 16 - 23 */
    0x19, 0x29, 0x49, 0x89,  0x1a, 0x2a, 0x4a, 0x8a,	/* 24 - 31 */

    0x51, 0x52, 0x54, 0x58,  0x91, 0x92, 0x94, 0x98,	/* 32 - 39 */
    0xa1, 0xa2, 0xa4, 0xa8,  0x31, 0x32, 0x34, 0x38,	/* 40 - 47 */
    0x16, 0x26, 0x46, 0x86,  0x1f, 0xf2, 0x0b, 0x0e,	/* 48 - 55 */
    0xb0, 0xe0, 0x8f, 0xf4,  0xc1, 0xc2, 0xc4, 0xc8	/* 56 - 63 */
};

/*
 * Look up this syndrome value in the ECC syndrome table.
 * The index in the table for this syndrome is the bit position
 * of the error.
 */
static int
syn_to_data_bit(int syn)
{
    int i;

    for (i = 0; i < 63; i++)
	if (ecc_syn_data[i] == syn)
	    return i;
    return -1;
}


/*
 * Determine the number of bits set in the word.
 */
static int
bitcount(int n)
{
    int count = 0;

    while (n) {
	count++;
	n &= ~(-n);	/* clear low order 1 bit */
    }
    return count;
}


/*
 * Find the position of the lowest bit set in the word.
 */
static int
lowbitset(int n)
{
    int b = 1, count = 0;

    if (n) {
	while ((n & b) == 0) {
	    count++;
	    b <<= 1;
	}
    }
    return count;
}


void
log_ecc_err(_crmreg_t addr, _crmreg_t syn, int hard)
{
    int bank, bank_slot, bit, synindex;
    char *bittype, *dimm_side;
    
    /* which crime bank */
    bank = addr_to_bank((paddr_t)addr & ~0x1f);
    /* which slot of bank pair */
    bank_slot = (syn & 0xffff) ? 0 : 1;

    if (bank < 0) {
	strcpy(bad_slot_string, "Slot Unknown");
    } else {
	strcpy(bad_slot_string, "Slot ?");
	bad_slot_string[5] = '1' + (bank & ~0x1) + bank_slot;
    }

    dimm_side = bank & 0x1 ? "back" : "front";

    if (!hard) {
	/*
	 * A memory word is 256 bits, or 4 groups of 64 bits.
	 * Each 64-bit group has 8 corresponding syndrome bits.
	 * Search for the first set of syndrome bits with an error.
	 */
	/*
	 * Since we figured out which even/odd slot of the
	 * bank pair had the error above, we want synindex
	 * to be relative to that slot.  Slide syn down
	 * if the error was in the upper half.
	 */
	if ((syn & 0xffff) == 0)
	    syn >>= 16;
	synindex = 0;
	while (syn && (syn & 0xff) == 0) {
	    syn >>= 8;
	    synindex++;
	}
	syn &= 0xff;	/* decode first error, ignore others */

	/*
	 * If more than 1 bit is set in the syndrome, then
	 * we have a data bit error.  Otherwise it is an
	 * error in the check bit.
	 */
	if (bitcount((int)syn) > 1) {
	    bittype = "data";
	    bit = syn_to_data_bit((int)syn) + synindex * 64;
	} else {
	    bittype = "check";
	    bit = lowbitset((int)syn) + synindex * 8;
	}

	cmn_err_tag(55,CE_ALERT, 
		    "Soft ECC Error in %s side of DIMM %s, %s bit %d\n",
		    dimm_side, bad_slot_string, bittype, bit);
    } else {
	cmn_err_tag(56,CE_ALERT, 
		    "Hard ECC Error in %s side of DIMM %s\n",
		    dimm_side, bad_slot_string);
    }
	
}


int
dobuserre(struct eframe_s *ep, inst_t *epc, uint flag)
{
	if (flag == 1)
		return(0); /* nofault */

	/*
	 * read registers not read in VEC_dbe or VEC_ibe.
	 */
	crm_mem_error_ecc_chk = READ_REG64(PHYS_TO_K1(CRM_MEM_ERROR_ECC_CHK),
					   _crmreg_t);
	crm_mem_error_ecc_syn = READ_REG64(PHYS_TO_K1(CRM_MEM_ERROR_ECC_SYN),
					   _crmreg_t);

	return dobuserr_common(ep,epc,flag);
}

int
dobuserr(struct eframe_s *ep, inst_t *epc, uint flag)
{
	err_save_regs();

	return dobuserr_common(ep, epc, flag);
}

#define CPU_BUSERR_REV0  (CRM_CPU_ERROR_CPU_INV_ADDR_RD | \
		     CRM_CPU_ERROR_CPU_II | CRM_CPU_ERROR_CPU_SYSCMD | \
		     CRM_CPU_ERROR_CPU_INV_ADDR_WR | \
		     CRM_CPU_ERROR_CPU_INV_REG_ADDR )
#define VICE_BUSERR_REV0 (CRM_CPU_ERROR_VICE_II | CRM_CPU_ERROR_VICE_SYSAD | \
		     CRM_CPU_ERROR_VICE_SYSCMD | CRM_CPU_ERROR_VICE_INV_ADDR )

#define CPU_BUSERR  (CRM_CPU_ERROR_CPU_ILL_ADDR | CRM_CPU_ERROR_CPU_WRT_PRTY)
#define VICE_BUSERR (CRM_CPU_ERROR_VICE_WRT_PRTY)

void 
print_buserr_rev1(_crmreg_t stat, _crmreg_t addr, 
		  int *cpu_buserr, int *vice_buserr)
{
    if (kdebug || (stat & CPU_BUSERR)) {
	cmn_err(CE_CONT, "CPU Error/Addr 0x%llx<%s%s>: 0x%llx\n",
		stat & CPU_BUSERR,
		stat & CRM_CPU_ERROR_CPU_ILL_ADDR ?
		"Invalid address " : "",
		stat & CRM_CPU_ERROR_CPU_WRT_PRTY ? 
		"Write parity error " : "",
		addr);
	if (stat & CPU_BUSERR)
	    *cpu_buserr = 1;
    }

    if (kdebug || (stat & VICE_BUSERR)) {
	cmn_err(CE_CONT, "VICE Error/Addr 0x%llx:<%s> 0x%llx\n",
		stat & VICE_BUSERR,
		stat & CRM_CPU_ERROR_VICE_WRT_PRTY ? 
		"Write parity error " : "",
		addr);
	if (stat & VICE_BUSERR)
	    *vice_buserr = 1;
    }
}

void
print_buserr_rev0(_crmreg_t stat, _crmreg_t addr, 
		  int *cpu_buserr, int *vice_buserr)
{
    if (kdebug || (stat & CPU_BUSERR_REV0)) {
	cmn_err(CE_CONT, "CPU Error/Addr 0x%llx<%s%s%s%s%s%s>: 0x%llx\n",
		stat & CPU_BUSERR_REV0,
		stat & CRM_CPU_ERROR_CPU_INV_ADDR_RD ?
		"Invalid read address " : "",
		stat & CRM_CPU_ERROR_CPU_II ? 
		"Illegal instruction " : "",
		stat & CRM_CPU_ERROR_CPU_SYSAD ? 
		"SysAD parity error " : "",
		stat & CRM_CPU_ERROR_CPU_SYSCMD ? 
		"SysCMD parity error " : "",
		stat & CRM_CPU_ERROR_CPU_INV_ADDR_WR ? 
		"Invalid write address " : "",
		stat & CRM_CPU_ERROR_CPU_INV_REG_ADDR ? 
		"Invalid register address " : "",
		addr);
	if (stat & CPU_BUSERR_REV0)
	    *cpu_buserr = 1;
    }

    if (kdebug || (stat & VICE_BUSERR_REV0)) {
	cmn_err(CE_CONT, "VICE Error/Addr 0x%llx:<%s%s%s%s> 0x%llx\n",
		stat & VICE_BUSERR_REV0,
		stat & CRM_CPU_ERROR_VICE_II ? 
		"Illegal instruction " : "",
		stat & CRM_CPU_ERROR_VICE_SYSAD ? 
		"SysAD parity error " : "",
		stat & CRM_CPU_ERROR_VICE_SYSCMD ? 
		"SysCMD parity error " : "",
		stat & CRM_CPU_ERROR_VICE_INV_ADDR ? 
		"Invalid address " : "",
		addr);
	if (stat & VICE_BUSERR_REV0)
	    *vice_buserr = 1;
    }
}

#define CRM_MEMERR  (CRM_MEM_ERROR_MACE_ACCESS | CRM_MEM_ERROR_VICE_ACCESS | \
		     CRM_MEM_ERROR_CPU_ACCESS | CRM_MEM_ERROR_GBE_ACCESS | \
		     CRM_MEM_ERROR_RE_ACCESS)

#define MEM_ADDR_ERR (CRM_MEM_ERROR_INV_MEM_ADDR_RD | \
		      CRM_MEM_ERROR_INV_MEM_ADDR_WR | \
		      CRM_MEM_ERROR_INV_MEM_ADDR_RMW)


static void
print_memerr(_crmreg_t stat, _crmreg_t addr)
{
    char *err_type;
    char *err_access;

    switch (stat & 0xe700000) {
    case CRM_MEM_ERROR_SOFT_ERR:
	err_type = (stat & 0x1000000) ? "SOFT ECC ERROR ON R/M/W" :
	    "SOFT ECC ERROR ON READ";
	break;
    case CRM_MEM_ERROR_HARD_ERR:
	err_type = (stat & 0x1000000) ? "HARD ECC ERROR ON R/M/W" :
	    "HARD ECC ERROR ON READ";
	break;
    case CRM_MEM_ERROR_MULTIPLE:
	err_type = (stat & 0x1000000) ? "MULTIPLE ECC ERRORS ON R/M/W" :
	    "MULTIPLE ECC ERRORS ON READ";
	break;
    case CRM_MEM_ERROR_INV_MEM_ADDR_RD:
	err_type = "INVALID MEMORY ADDRESS ON READ";
	break;
    case CRM_MEM_ERROR_INV_MEM_ADDR_WR:
	err_type = "INVALID MEMORY ADDRESS ON WRITE";
	break;
    case CRM_MEM_ERROR_INV_MEM_ADDR_RMW:
	err_type = "INVALID MEMORY ADDRESS ON R/M/W";
	break;
    default:
	err_type = "UNKNOWN ERROR TYPE";
	break;
    }

    switch (stat & 0x78080) {
    case CRM_MEM_ERROR_MACE_ACCESS:
	err_access = "MACE ACCESS";
	break;
    case CRM_MEM_ERROR_RE_ACCESS:
	err_access = "RE ACCESS";
	break;
    case CRM_MEM_ERROR_GBE_ACCESS:
	err_access = "GBE ACCESS";
	break;
    case CRM_MEM_ERROR_VICE_ACCESS:
	err_access = "VICE ACCESS";
	break;
    case CRM_MEM_ERROR_CPU_ACCESS:
	err_access = "CPU ACCESS";
	break;
    default:
	err_access = "UNKNOWN ACCESS TYPE";
	break;
    }

    cmn_err(CE_CONT, "MEMORY Error/Addr 0x%llx<%s during %s>: 0x%llx\n",
	    stat, err_type, err_access, addr);
}

static int
dobuserr_common(struct eframe_s *ep,
		inst_t *epc,
		uint flag)
{
	int cpu_buserr = 0;
	int memerr = 0;
	int vice_buserr = 0;
#ifdef R4600SC
	int r4600sc_scache_disabled = 1;

	if (two_set_pcaches && private.p_scachesize)
		r4600sc_scache_disabled = _r4600sc_disable_scache();
#endif
	

	/*
	 * cpu/vice error stat register has different layout
	 * between rev0 (petty crime) and rev1 (organized crime).
	 */
	if (!get_crimerev())
	    print_buserr_rev0(crm_cpu_error_stat, crm_cpu_error_addr,
			      &cpu_buserr, &vice_buserr);
	else
	    print_buserr_rev1(crm_cpu_error_stat, crm_cpu_error_addr,
			      &cpu_buserr, &vice_buserr);

	if (kdebug || (crm_mem_error_stat & CRM_MEMERR)) {
	    print_memerr(crm_mem_error_stat, crm_mem_error_addr);
	    if (crm_mem_error_stat & MEM_ADDR_ERR) {
		if (crm_mem_error_stat & CRM_MEM_ERROR_VICE_ACCESS)
		    vice_buserr = 1;
		else
		    cpu_buserr = 1;
	    } else if (!(crm_mem_error_stat & MEM_ADDR_ERR)) {
		memerr = 1;
	    }
	}

	/* 
	 * print bank, byte, and DIMM number if it's a parity error
	 */
	if (memerr) {
		log_ecc_err(crm_mem_error_addr, 
			    crm_mem_error_ecc_syn, 
			    crm_mem_error_stat & CRM_MEM_ERROR_HARD_ERR ? 1:0);

		/*
		 * Try and correct the error if possible.
		 */
		if (ecc_error_correction(crm_mem_error_addr, 
					 crm_mem_error_ecc_chk,
					 crm_mem_error_ecc_syn,
					 crm_mem_error_stat)) {
#ifdef R4600SC
			if (!r4600sc_scache_disabled)
				_r4600sc_enable_scache();
#endif
		    return(1);
		}

		/* print pretty message */
		if (flag == 2 && curvprocp) {
			cmn_err_tag(57,CE_ALERT,
"Process %d [%s] sent SIGBUS due to Memory Error in DIMM %s\n\tat Physical Address 0x%llx\n",
				current_pid(), get_current_name(),
				bad_slot_string, 
				crm_mem_error_addr);
		} else {
			/*
			 * Turn off ECC checking so we don't hit the
			 * same ECC error while dumping memory.
			 */
			ecc_disable();

			cmn_err_tag(58,CE_PANIC,
"IRIX Killed due to Memory Error in DIMM %s at Physical Address 0x%llx\nPC:0x%x ep:0x%x\n",
				bad_slot_string, crm_mem_error_addr, 
				epc, ep);
		}
	}

	if (cpu_buserr) {
		if (flag == 2 && curvprocp)
			cmn_err(CE_CONT,
"Process %d [%s] sent SIGBUS due to Bus Error\n",
				current_pid(), get_current_name());
		else
			cmn_err_tag(59,CE_PANIC,
"IRIX Killed due to Bus Error\n\tat PC:0x%x ep:0x%x\n", 
				epc, ep);
	}

	if (vice_buserr) {
	        if (vice_err(crm_vice_error_addr, crm_cpu_error_stat))
		        return(1);
		if (flag == 2 && curvprocp)
			cmn_err(CE_CONT,
"Process %d [%s] sent SIGBUS due to Bus Error\n",
				current_pid(), get_current_name());
		else
			cmn_err_tag(60,CE_PANIC,
"IRIX Killed due to Bus Error\n\tat PC:0x%x ep:0x%x\n", 
				epc, ep);
	}

#ifdef R4600SC
	if (!r4600sc_scache_disabled)
		_r4600sc_enable_scache();
#endif

	return(0);
}

/*
 * Should never receive an exception (other than interrupt) while
 * running on the idle stack.
 * Check for memory errors
 */
void
idle_err(inst_t *epc, uint cause, void *k1, void *sp)
{
	if ((cause & CAUSE_EXCMASK) == EXC_IBE ||
	    (cause & CAUSE_EXCMASK) == EXC_DBE)
		dobuserre(NULL, epc, 0);
	else if (cause & CAUSE_BERRINTR)
		dobuserr(NULL, epc, 0);

	cmn_err(CE_PANIC,
	"exception on IDLE stack k1:0x%x epc:0x%x cause:0x%w32x sp:0x%x badvaddr: 0x%x",
		k1, epc, cause, sp, getbadvaddr());
	/* NOTREACHED */
}

/*
 * earlynofault - handle very early global faults
 * Returns: 1 if should do nofault
 *	    0 if not
 */
earlynofault(struct eframe_s *ep, uint code)
{
	switch(code) {
	case EXC_DBE:
		dobuserre(ep, (inst_t *)ep->ef_epc, 1);
		break;
	default:
		return(0);
	}
	return(1);
}


/*
 * Error on the PCI bridge.
 * Print out a warning for the minor stuff, panic on catastrophic errors.
 */
/*ARGSUSED*/
void
pci_error_intr(eframe_t *ef, __psint_t mace_bridge)
{
	/*
	 * NOTE: All the pio operations on IP32 need to go through the 
	 * pciio_pio_{read,write}{8,16,32,64}() interfaces. This is to 
	 * workaround the CRIME->MACE  phantom pio read problem. 
	 */

	volatile uint32_t *pci_err_flags = 
		(volatile uint32_t *)(PHYS_TO_K1(PCI_ERROR_FLAGS));
	volatile uint32_t *pci_err_addr = 
		(volatile uint32_t *) (PHYS_TO_K1(PCI_ERROR_ADDR));
        
	uint32_t pci_err_flags_value = pciio_pio_read32(pci_err_flags);
	uint32_t pci_err_addr_value = pciio_pio_read32(pci_err_addr);

	if (pci_err_flags_value & (PERR_DATA_PARITY_ERR |
				   PERR_RETRY_ERR 	|
				   PERR_ILLEGAL_CMD 	|
				   PERR_SYSTEM_ERR 	|
				   PERR_PARITY_ERR))
		cmn_err(CE_PANIC, 
			"catastrophic error on PCI bus.flags = 0x%x addr 0x%x",
			pci_err_flags_value, pci_err_addr_value);

	pciio_pio_write32(0,pci_err_flags);

}


#ifdef DEBUG
#include <sys/errno.h>

void
set_ecc(paddr_t paddr, int eccbits)
{
    _crmreg_t ecc_enable, ecc_repl;
    int s = spl7();

    /* double word align */
    paddr &= ~(sizeof(long long) - 1);

    /* set replacement bits */
    ecc_repl = eccbits;
    WRITE_REG64(ecc_repl, PHYS_TO_K1(CRM_MEM_ERROR_ECC_REPL), _crmreg_t);

    /* enable use of replacement bits on next store */
    ecc_enable = READ_REG64(PHYS_TO_K1(CRM_MEM_CONTROL), _crmreg_t);
    WRITE_REG64(ecc_enable | CRM_MEM_CONTROL_USE_ECC_REPL,
		PHYS_TO_K1(CRM_MEM_CONTROL), _crmreg_t);

    /*
     * Have to be careful that the next store to memory
     * is our store to paddr since we have USE_ECC_REPL set
     * in CRIME.  All interrupts are off here.
     *
     * XXX This would be better in asm, since WRITE_REG64 and
     * scrub_memory are free to store all they want, they just
     * don't at the moment.
     */

    /* load,store double */
    scrub_memory((caddr_t)PHYS_TO_K1(paddr));

    /* disable use of replacement bits */
    WRITE_REG64(ecc_enable, PHYS_TO_K1(CRM_MEM_CONTROL), _crmreg_t);
    splx(s);
}


char ecc_target[256];
int ecc_gun_print = 0;

/*ARGSUSED3*/
int
ecc_gun(int cmd, void *arg, rval_t *rvp)
{
    extern int icache_size, dcache_size;
    paddr_t paddr;
    struct egun_cmd egun;

    if (copyin(arg, (void *)&egun, sizeof(egun)))
	return EFAULT;

    switch (cmd) {

    case EGUN_PHYS:
	paddr = (paddr_t)egun.addr;

	if (paddr >= ctob(physmem))
	    return EINVAL;
	if (PHYS_TO_K0(paddr) >= K1SEG)
	    return EINVAL;
	
	cache_operation((void *)K0SEG, dcache_size,
			CACH_DCACHE | CACH_INVAL | CACH_WBACK | CACH_NOADDR);
	cache_operation((void *)K0SEG, icache_size,
			CACH_ICACHE | CACH_INVAL | CACH_NOADDR);

	if (ecc_gun_print)
	    printf("ecc_gun: addr 0x%x ecc_repl 0x%x\n", paddr, egun.ecc_repl);

	set_ecc(PHYS_TO_K1(paddr), egun.ecc_repl);
	break;

    case EGUN_PHYS_WORD: {
	volatile char c;
	char *ecc_targ;
#define MEMWORD			0x20
#define MEMWORD_MASK		(MEMWORD-1)
#define MEMWORD_ALIGN(addr)	(((uint_t)(addr) + MEMWORD-1) & ~MEMWORD_MASK)
#define MEMWORD_ALIGNT(addr)	(((uint_t)(addr)) & ~MEMWORD_MASK)

	/* roundup to memory word */
	ecc_targ = (char *)MEMWORD_ALIGN(ecc_target);

	/* user specifies memory word offset */
	paddr = (paddr_t)egun.addr;

	if (ecc_targ + paddr >=
	    (char *)MEMWORD_ALIGNT(&ecc_target[sizeof(ecc_target)]))
	    return EINVAL;

	ecc_targ += paddr;

	/* flush it out */
	dki_dcache_wbinval ((void *)ecc_targ, 1);

	if (ecc_gun_print)
	    printf("ecc_gun: addr 0x%x ecc_repl 0x%x\n",
		   ecc_targ, egun.ecc_repl);

	/* bad ecc */
	set_ecc(K0_TO_PHYS(ecc_targ), egun.ecc_repl);

	/* read it from memory */
	c = *(char *)K0_TO_K1(ecc_targ);
	break;
    }

    default:
	return EINVAL;
    }
    return 0;
}
#endif /* DEBUG */
