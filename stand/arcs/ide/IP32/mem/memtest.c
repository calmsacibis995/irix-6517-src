#ident	"IP22diags/mem/memtest.c:  $Revision: 1.3 $"
 
#include "mem.h"
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <sys/types.h>

static void
memtest_usage(char *progname) {
	msg_printf(ERR, "usage: %s [-t n] [RANGE]\n", progname);
}

/* test local memory, access through TLB if necessary */
int
memory_test(int argc, char *argv[])
{
	int		cflag = 0;
	u_int		count;
	u_int		cpuctrl0;
	int		error = 0;
	u_int		last_phys_addr;
	extern u_int	memsize;
	struct range	range;
	void 		map_mem(int);
	u_int		loop_count = 1;
	int		tflag = 0;		/* t for tornes */
	int orion = ((get_prid()&0xFF00)>>8) == C0_IMP_R4600;
	int r4700 = ((get_prid()&0xFF00)>>8) == C0_IMP_R4700;
	int r5000 = ((get_prid()&0xFF00)>>8) == C0_IMP_R5000;
	u_int                   cpuctrl0_cpr;

	if (memsize > MAX_LOW_MEM)
		return(0);	

	run_cached();

	/* set up default */
	if (memsize <= MAX_LOW_MEM)
		range.ra_base = FREE_MEM_LO;
	else
		range.ra_base = KDM_TO_PHYS(FREE_MEM_LO);

	count = memsize - (KDM_TO_PHYS(range.ra_base) - PHYS_RAMBASE);
	range.ra_size = sizeof(u_int);
	range.ra_count = count / range.ra_size;
	last_phys_addr = KDM_TO_PHYS(range.ra_base) + count;

	if (argc > 4) {
		memtest_usage(argv[0]);
		return -1;
	}

	if (argc > 2) {
		if (strcmp(argv[1], "-t") != 0) {
			memtest_usage(argv[0]);
			return -1;
		}
		tflag = 1;
		(void)atob(argv[2], &loop_count);	
	}

	if (argc == 2 || argc == 4) {
		if (!(parse_range(argv[argc - 1], sizeof(u_int), &range))) {
			memtest_usage(argv[0]);
			return -1;
		}

		cflag = IS_KSEG0(range.ra_base) ? 1 : 0;
		count = range.ra_size * range.ra_count;
		last_phys_addr = KDM_TO_PHYS(range.ra_base) + count;
#ifdef NOTDEF
		if (last_phys_addr - PHYS_RAMBASE > memsize) {
			msg_printf(ERR, "%s: address out of range\n", argv[0]);
			return -1;
		}
#endif	/* NOTDEF */
	}

	msg_printf(VRB, "CPU memory test, Base: 0x%08x, Size: 0x%08x bytes\n",
		range.ra_base, count);

	if (last_phys_addr > MAX_LOW_MEM_ADDR) {
		range.ra_base = KDM_TO_PHYS(range.ra_base);
		map_mem(cflag);
	}

	if (tflag) {
		while (loop_count--)
			error += do_uncached(range.ra_base,
				range.ra_size * range.ra_count);
	} else {
		if (!memaddruniq(&range, BIT_TRUE, RUN_UNTIL_DONE, memerr)
	    	    || !memwalkingbit(&range, BIT_TRUE, RUN_UNTIL_DONE, memerr))
			error = -1;
	}

	tlbpurge();
	if (error) {
		sum_error("CPU local memory");
		return -1;
	} else {
		okydoky();
		return 0;
	}
}




#define GRX_RST_N 0x400

do_uncached(base, byte_count)
u_int *base;
u_int byte_count;
{
	volatile unsigned int *ptr;
	volatile unsigned int read_data;
	unsigned int write_data;
	int i;
	unsigned int cpuctrl0;
	unsigned int status;
	unsigned int addr_e;
	int foo;
	int error = 0;

	write_data = 0x58585858;

	ptr = base;
	msg_printf(VRB, "start addr 0x%x, byte_count 0x%x\n", ptr, byte_count);

	/* write data and read it back right away so dram is still */
	/* in page mode */
	for(i = 0; i < byte_count/4; i++, ptr++) {
		*ptr = write_data;
		read_data = *ptr;
		if(read_data != write_data) {
			error = 1;
			/* read address of error */
			msg_printf(ERR,
				"Error at 0x%x, data wr 0x%x, rd 0x%x, again 0x%x\n",
				addr_e, write_data, read_data, *ptr);
		}
		/* take rams out of page mode by doing a gio operation */
		foo = *(volatile unsigned int *)(0xbfc00000);
	}

	ptr = base;
	for(i = 0; i < byte_count/4; i++, ptr++)
		*ptr = 0x0;

	return error;
}

/*
 * map both low/high local memory for testing.  the high memory can only
 * be accessed through TLB.  there are 256 MB in low local memory and IP22
 * supports only 128 MB in high local memory
 */
void
map_mem(int cflag)		/* cflag == 0 -> uncached */
{
	int		i, j;
	u_int		old_pgmask = get_pgmask();
	u_int		paddr;
	u_int		tlblo_attribute;
	u_int		tlblo0;
	u_int		tlblo1;
	u_int		vaddr;
	u_int		addr_increment;
	u_int		memconfig[2];
	u_int		base;
	u_int		size;
	u_int		high_mem_size = 0;
	u_int		low_mem_size = 0;
	extern u_int	mc_rev;
	struct	pagemask_table {
		u_int	pagesize, pagemask;
	};
	static struct pagemask_table pagemask_table[] = {
		{0x1000000,	0x01ffe000},		/* 16M */
		{0x400000,	0x007fe000},		/* 4M */
		{0x100000,	0x001fe000},		/* 1M */
		{0x40000,	0x0007e000},		/* 256K */
		{0x10000,	0x0001e000},		/* 64K */
		{0x4000,	0x00006000},		/* 16K */
		{0x1000,	0x00000000},		/* 4K */
	};
#define	PAGEMASK_TABLE_SIZE	(sizeof(pagemask_table) / sizeof(struct pagemask_table))

	
	if (cflag)
		tlblo_attribute = TLBLO_D | TLBLO_V | TLBLO_G | TLBLO_NONCOHRNT;
	else
		tlblo_attribute = TLBLO_D | TLBLO_V | TLBLO_G | TLBLO_UNCACHED;
			
	/* map low local memory */
	paddr = vaddr = PHYS_RAMBASE;
	for (i = 0, j = 0; j < PAGEMASK_TABLE_SIZE; j++) {
		set_pgmask(pagemask_table[j].pagemask);

		while (low_mem_size >= pagemask_table[j].pagesize) {
			addr_increment = pagemask_table[j].pagesize;

			low_mem_size -= pagemask_table[j].pagesize;
			tlblo0 = (paddr >> 6) | tlblo_attribute;
			paddr += pagemask_table[j].pagesize;

			if (low_mem_size >= pagemask_table[j].pagesize) {
				low_mem_size -= pagemask_table[j].pagesize;
				addr_increment += pagemask_table[j].pagesize;
				tlblo1 = (paddr >> 6) | tlblo_attribute;
				paddr += pagemask_table[j].pagesize;
			} else
				tlblo1 = 0x0;

			tlbwired(i++, 0, vaddr, tlblo0, tlblo1);
			vaddr += addr_increment;
		}
	}

	/* map high local memory */
	paddr = 0x20000000;
	for (j = 0; j < PAGEMASK_TABLE_SIZE; j++) {
		set_pgmask(pagemask_table[j].pagemask);

		while (high_mem_size >= pagemask_table[j].pagesize) {
			addr_increment = pagemask_table[j].pagesize;

			high_mem_size -= pagemask_table[j].pagesize;
			tlblo0 = (paddr >> 6) | tlblo_attribute;
			paddr += pagemask_table[j].pagesize;

			if (high_mem_size >= pagemask_table[j].pagesize) {
				high_mem_size -= pagemask_table[j].pagesize;
				addr_increment += pagemask_table[j].pagesize;
				tlblo1 = (paddr >> 6) | tlblo_attribute;
				paddr += pagemask_table[j].pagesize;
			} else
				tlblo1 = 0x0;

			tlbwired(i++, 0, vaddr, tlblo0, tlblo1);
			vaddr += addr_increment;
		}
	}

	set_pgmask(old_pgmask);
}
