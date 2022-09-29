#ident	"$Revision: 1.31 $"
 
#include <sys/types.h>
#include <sys/cpu.h>
#include <sys/sbd.h>
#include <libsc.h>
#include <libsk.h>
#include "mem.h"

#ifdef _USE_MAPMEM
#include "fforward/tlb/tlb.h"
#endif

static int do_uncached(u_int *, unsigned long);

#define INT_EN 0x2000

static void
memtest_usage(char *progname) {
	msg_printf(ERR, "usage: %s [-t n] [RANGE]\n", progname);
}

/* test local memory, access through TLB if necessary */
int
memory_test(int argc, char *argv[])
{
	int		error = 0;
	struct range	range,range2;
	u_int		loop_count = 1;
	int		tflag = 0;		/* t for tornes */
	char		*rstr = argv[1];
#ifndef IP28
	u_int		cpuctrl0;
	u_int		cpuctrl0_cpr;
#ifdef IP22
	int orion = ((get_prid()&0xFF00)>>8) == C0_IMP_R4600;
	int r4700 = ((get_prid()&0xFF00)>>8) == C0_IMP_R4700;
	int r5000 = ((get_prid()&0xFF00)>>8) == C0_IMP_R5000;

	cpuctrl0_cpr = (orion|r4700|r5000) ? 0 : CPUCTRL0_CPR;
#else
	cpuctrl0_cpr = CPUCTRL0_CPR;
#endif
#endif

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
		(void)atobu(argv[2], &loop_count);	
		rstr = argv[3];			/* where range is */
		argc -= 2;
	}

#if IP26
	flush_cache();
#endif

	if (mem_setup(argc,argv[0],rstr,&range,&range2) != 0) {
		error = 1;
		goto done;
	}

	msg_printf(VRB, "CPU memory test, Base: 0x%08x, Size: 0x%08x bytes\n",
		range.ra_base, range.ra_count*range.ra_size);
#ifndef _USE_MAPMEM
	if (range2.ra_base)
		msg_printf(VRB, "Base: 0x%08x, Size: 0x%08x bytes\n",
			range2.ra_base, range2.ra_count*range2.ra_size);
#endif

#ifndef IP28
	cpuctrl0 = *(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0);
#endif

	if (tflag) {
#ifndef IP28
		*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = (cpuctrl0 |
			CPUCTRL0_GPR | cpuctrl0_cpr | CPUCTRL0_MPR |
			CPUCTRL0_R4K_CHK_PAR_N) & ~INT_EN;
#endif
		while (loop_count--) {
			error += do_uncached((u_int *)range.ra_base,
				range.ra_size * range.ra_count);
#ifndef _USE_MAPMEM
			if (range2.ra_base)
				error += do_uncached((u_int *)range2.ra_base,
					range2.ra_size * range2.ra_count);
#endif
		}
	}
	else {
#ifndef IP28
		*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = (cpuctrl0 |
			CPUCTRL0_GPR | CPUCTRL0_MPR | cpuctrl0_cpr) &
			~CPUCTRL0_R4K_CHK_PAR_N;
#endif
		if (!memaddruniq(&range, BIT_TRUE, RUN_UNTIL_DONE, memerr)
	    	    || !memwalkingbit(&range, BIT_TRUE, RUN_UNTIL_DONE, memerr))
			error = 1;
#ifndef _USE_MAPMEM
		if (range2.ra_base &&
		    (!memaddruniq(&range2,BIT_TRUE,RUN_UNTIL_DONE,memerr) ||
		     !memwalkingbit(&range2,BIT_TRUE,RUN_UNTIL_DONE,memerr)))
			error += 1;
#endif
	}

	tlbpurge();

#ifndef IP28
	*(volatile unsigned int *)PHYS_TO_K1(CPUCTRL0) = cpuctrl0;
#endif

done:
	if (error) {
		sum_error("CPU local memory");
		return -1;
	}

	okydoky();
	return 0;
}




#define GRX_RST_N 0x400

static int
do_uncached(u_int *base, unsigned long byte_count)
{
	volatile unsigned int *ptr;
	volatile unsigned int read_data;
	unsigned int write_data;
	int i;
	unsigned int status;
	unsigned int addr_e;
	volatile int foo;
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
			addr_e = *(volatile unsigned int *)PHYS_TO_K1(CPU_ERR_ADDR);
			/* read error status register */
			status = *(volatile unsigned int *)PHYS_TO_K1(CPU_ERR_STAT);
			*(volatile unsigned int *)PHYS_TO_K1(CPU_ERR_STAT) = 0;
			msg_printf(ERR,
				"Error at 0x%x, data wr 0x%x, rd 0x%x, again 0x%x, stat 0x%x\n",
				addr_e, write_data, read_data, *ptr, status);
		}
		/* take rams out of page mode by doing a gio operation */
#ifdef IP20
		foo = *(volatile unsigned int *)PHYS_TO_K1(0x1fc00000);
#else
		foo = *(volatile unsigned int *)PHYS_TO_K1(HPC_0_ID_PROM0);
#endif
	}

	ptr = base;
	for(i = 0; i < byte_count/4; i++, ptr++)
		*ptr = 0x0;

	return error;
}

#ifdef _USE_MAPMEM
/*
 * map both low/high local memory for testing.  the high memory can only
 * be accessed through TLB.  there are 256 MB in low local memory and IP22
 * supports only 128 MB in high local memory.  IP26 supports up to 512MB
 * in high local memory.
 */
void
map_mem(int cflag)		/* cflag == 0 -> uncached */
{
       int             i, j;
        u_int	        paddr;
        u_int           tlblo_attribute;
        u_int           tlblo0;
        u_int           tlblo1;
        caddr_t         vaddr;
        u_int           addr_increment;
        u_int           memconfig[2];
        u_int           base;
        u_int           size;
        u_int           high_mem_size = 0;
        u_int           low_mem_size = 0;
        extern u_int    mc_rev;
        struct  pagemask_table {
                u_int   pagesize, pagemask;
        };
        u_int           old_pgmask = get_pgmask();
        static struct pagemask_table pagemask_table[] = {
                {0x1000000,     0x01ffe000},            /* 16M */
                {0x400000,      0x007fe000},            /* 4M */
                {0x100000,      0x001fe000},            /* 1M */
                {0x40000,       0x0007e000},            /* 256K */
                {0x10000,       0x0001e000},            /* 64K */
                {0x4000,        0x00006000},            /* 16K */
                {0x1000,        0x00000000},            /* 4K */
        };
#define PAGEMASK_TABLE_SIZE     (sizeof(pagemask_table) / sizeof(struct pagemask_table))

        memconfig[0] = *(volatile u_int *)PHYS_TO_K1(MEMCFG0);
        memconfig[1] = *(volatile u_int *)PHYS_TO_K1(MEMCFG1);

        for (i = 0; i < 2; i++) {
                while (memconfig[i]) {
                        if (memconfig[i] & MEMCFG_VLD) {
                                base = (memconfig[i] & MEMCFG_ADDR_MASK) <<
                                        (mc_rev >= 5 ? 24 : 22);
                                size = ((memconfig[i] & MEMCFG_DRAM_MASK) + 0x0100) <<
                                        (mc_rev >= 5 ? 16 : 14);

                                if (base > MAX_LOW_MEM_ADDR)
                                        high_mem_size += size;
                                else
                                        low_mem_size += size;
                        }
                        memconfig[i] >>= 16;
                }
        }

        if (cflag)
                tlblo_attribute = TLBLO_D | TLBLO_V | TLBLO_G | TLBLO_NONCOHRNT;        else
                tlblo_attribute = TLBLO_D | TLBLO_V | TLBLO_G | TLBLO_UNCACHED;

        /* map low local memory */
        paddr = PHYS_RAMBASE;
	vaddr = (caddr_t) (__psunsigned_t)PHYS_RAMBASE;
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
#endif
