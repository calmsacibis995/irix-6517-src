/**************************************************************************
 *									  *
 * 		 Copyright (C) 1989-1993 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.91 $"

#include <sys/types.h>
#include <sys/buf.h>
#include <sys/cmn_err.h>
#include <sys/conf.h>
#include <sys/debug.h>
#include <sys/dump.h>
#include <sys/errno.h>
#include <ksys/exception.h>
#include <ksys/vfile.h>
#include <sys/fpu.h>
#include <sys/immu.h>
#include <sys/ksignal.h>
#include <sys/kucontext.h>
#include <sys/map.h>
#include <sys/param.h>
#include <sys/pcb.h>
#include <sys/pda.h>
#include <sys/proc.h>
#include <sys/reg.h>
#include <sys/sbd.h>
#include <ksys/sthread.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/var.h>
#include <sys/pfdat.h>
#include <sys/page.h>
#include <sys/nodepda.h>
#include <sys/uadmin.h>
#include <sys/utsname.h>
#include <sys/runq.h>
#include <string.h>
#include "os/proc/pproc_private.h"	/* XXX bogus */

#if SN
#include <sys/cpu.h>
#include <sys/SN/gda.h>
#include <sys/SN/nmi.h>
#endif /* SN */

#if SN0
#include <sys/SN/SN0/ip27log.h>
#define	BOUNDARY_ENTRY	2
#define	BOUNDARIES	( 28 * BOUNDARY_ENTRY )
#define	HUB_REG_PACE		sizeof(hubreg_t)
static int64_t	boundary[] = {
	PI_BASE,		/* 0x000000 */
	PI_ERR_STATUS0_A,	/* 0x000430 */
	PI_ERR_STATUS1_A,	/* 0x000440 */
	PI_ERR_STATUS1_A,	/* 0x000440 */
	PI_ERR_STATUS0_B,	/* 0x000450 */
	PI_ERR_STATUS0_B,	/* 0x000450 */
	PI_ERR_STATUS1_B,	/* 0x000460 */
	PI_ERR_STATUS1_B,	/* 0x000460 */
	PI_SPOOL_CMP_A,		/* 0x000470 */
	PI_NACK_CMP,		/* 0x0004b8 */
	MD_BASE,		/* 0x200000 */
	MD_MIG_CANDIDATE,	/* 0x200040 */
	MD_DIR_ERROR,		/* 0x200050 */
	MD_DIR_ERROR,		/* 0x200050 */
	MD_PROTOCOL_ERROR,	/* 0x200060 */
	MD_PROTOCOL_ERROR,	/* 0x200060 */
	MD_MEM_ERROR,		/* 0x200070 */
	MD_MEM_ERROR,		/* 0x200070 */
	MD_MISC_ERROR,		/* 0x200080 */
	MD_MISC_ERROR,		/* 0x200080 */
	MD_MEM_DIMM_INIT,	/* 0x200090 */
	MD_MLAN_CTL,		/* 0x2000a8 */
	CORE_MD_ALLOC_BW,	/* 0x2000b0 */
	CORE_FIFO_DEPTH,	/* 0x2000e0 */
	MD_BASE_PERF,		/* 0x210000 */
	MD_PERF_CNT5,		/* 0x210038 */
	MD_BASE_JUNK,		/* 0x220000 */
	MD_UREG1_15,		/* 0x2200f8 */
	IIO_BASE,		/* 0x400000 */
	IIO_WSTAT,		/* 0x400008 */
	IIO_WCR,		/* 0x400020 */
	IIO_WCR,		/* 0x400020 */
	IIO_ILAPR,		/* 0x400100 */
	IIO_IBCN,		/* 0x400200 */
	IIO_IPCA,		/* 0x400300 */
	IIO_PRTE_0 + ( IIO_NUM_PRTES * HUB_REG_PACE )
		- HUB_REG_PACE,	/* 0x400340 */
	IIO_IPDR,		/* 0x400388 */
	IIO_ICTP,		/* 0x4003c0 */
	IIO_ICRB_0,		/* 0x400400 */
	IIO_ICRB_0 + ( IIO_NUM_CRBS * IIO_NUM_PC_CRBS *
			HUB_REG_PACE ) - HUB_REG_PACE,
	IIO_BASE_BTE0,		/* 0x410000 */
	IIO_IBIA_0,		/* 0x410028 */
	IIO_BASE_BTE1,		/* 0x420000 */
	IIO_IBIA_1,		/* 0x420028 */
	IIO_BASE_PERF,		/* 0x430000 */
	IIO_IPPR,		/* 0x430008 */
	NI_BASE,		/* 0x600000 */
	NI_VECTOR_READ_DATA,	/* 0x600310 */
	NI_IO_PROTECT,		/* 0x600400 */
	NI_AGE_IO_PIO,		/* 0x600538 */
	NI_PORT_PARMS,		/* 0x608000 */
	NI_PORT_ERROR,		/* 0x608008 */
	NI_META_TABLE0,		/* 0x638000 */
	NI_META_TABLE0 + ( ( NI_META_ENTRIES 
			      + NI_LOCAL_ENTRIES - 1 )
			      * HUB_REG_PACE ), 
	-1, -1
};
static int dump_hub_data(void);
static int dump_hub_dir_mem(void);
static int dump_hub_registers(void);
static int write_hub_dir_mem(pfn_t pfn);
extern void sysctlr_keepalive(void);
static dir_mem_entry_t	dir_mem_entry[NBPP/MD_PAGE_SIZE];
extern int 		dump_hub_info;
extern int		heartbeat_enable;
#endif /* SN0 */
 
#if IP30
#include <sys/cpu.h>
#endif

#define DUMP_DOTBYTES	0x100000
#define DUMP_DOTPAGES	(DUMP_DOTBYTES / NBPP)

#define PAGE_DUMPED_TAG		((void *) 0xfeadbeefUL)

int	dump_in_progress = 0;
int	dumpmag = 0x8fca0101;	/* magic number for savecore */
int	dumpsize = 0;		/* also for savecore */
long	dumpstack[DUMP_STACK_SIZE / sizeof(long)];	/* Dump stack. */
sthread_t *dumpsthread = 0;	/* kthread used if curkthread == NULL */
char    *dumpdevname;		/* Allocated and set in main. */
static	label_t dumpjmp;
#if	defined(IP27)
extern	int	maxdmasz;
#endif

/* Tell dbx how we're compiled. */
#if _MIPS_SIM != _ABI64
uint	dumpkflags = DUMP_K32 | DUMP_U64;
char	*dumpvaddr;		/* Set to a nice k2 space addr in main. */
#else
uint	dumpkflags = DUMP_K64 | DUMP_U64;
#endif
#if	defined(IP27)
extern int is_fine_dirmode(void );
extern int memory_present ( paddr_t );
extern int memory_read_accessible ( paddr_t );
extern int memory_write_accessible ( paddr_t );
extern void page_allow_access ( pfn_t );
extern poison_state_alter_range ( __psunsigned_t, int, int );
extern cnodeid_t get_compact_nodeid(void);
#endif
extern unsigned int putbufndx;
extern unsigned int putbufsz;
extern char *putbuf;
extern unsigned int errbufndx;
extern unsigned int errbufsz;
extern char *errbuf;
extern int dump_level;

k_machreg_t	dumpregs[NJBREGS];	/* area where panic regs are saved */
struct	proc	*dumpproc;	/* current process at time of panic */

static struct bdevsw *dump_bdevsw;

static void real_dumpsys(void);

/* Variables for dumping */
static char	dump_buffer[DUMP_BLOCKSIZE];
static char	*dump_buffer_ptr;
static int	dump_buffer_size	= DUMP_BLOCKSIZE;
static char	compr_buffer[NBPP];
#if defined (SN)
static char	data_buffer[NBPP];
#endif
static int	dump_buflen		= 0;
static int	dump_sctr;
static int	dump_dotpage;
static int	dump_pages;
static dump_hdr_t	dump_hdr;	/* Dump header for new dumps. */
static long physaddr;			/* Start of phys memory */

#if (NBPP == 16384)
static char zeroes_block[] = {
    0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0,
    0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0,
    0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0,
    0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0,
    0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0,
    0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0,
    0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0,
    0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0,
    0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0,
    0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0,
    0, 255, 0, 0, 255, 0, 0, 255, 0, 0, 255, 0 
};
#endif

static int dump_write(char *, int);
static int dump_flush(void);
static int dump_dir_ent(unsigned long, int, int);
static int compress_block(char *, int, char *);
static int compress_and_write_block(unsigned long, char *, int, char *, int);
static int dump_page(unsigned long, int);
static void dumpvmcore(void);
#if DISCONTIG_PHYSMEM
static int dump_pfdats(void);
#endif
static int dump_range(caddr_t, int);
static void dump_btinfo(void);
#if	defined(IP27)
static void move_buffer(void);
static cpuid_t	remote_cpu_id;
#endif
static int pfd_scan(int, int);
static int dump_lowmem(void);

#define SCAN_KERN_NONBULK	0
#define SCAN_KERN_BULK		1
#define SCAN_INUSE		2
#define SCAN_FREE		3
#define SCAN_UNDUMPED		4

void
dumperror(int err)
{
	printf("\nError during dump: ");
	switch (err) {
	  case ENXIO:
		printf("device bad\n");
		break;

	  case EFAULT:
		printf("device not ready\n");
		break;

	  case EINVAL:
		printf("area improper\n");
		break;

	  case EIO:
		printf("i/o error\n");
		break;

	  default:
		printf("unknown error (%d)\n", err);
		break;
	}
}

/*
 * cmn_err(C_PANIC) comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */
void
dumpsys(void)
{
	/*
	 * As useful as this code is, it can hang
	 * too easily due to mucho lock acquisition
	 */

	(void) setjmp(dumpregs);
	dumpproc = curprocp;

	(void) setjmp(dumpjmp);

	dumpjmp[JB_SP] = (__psint_t)dumpstack + DUMP_STACK_SIZE - 8;
	dumpjmp[JB_PC] = (__psint_t)real_dumpsys;
	if (!private.p_curkthread) {
		private.p_curkthread = ST_TO_KT(dumpsthread);
	}
	(void)setmustrun(cpuid());
#if TFP
	/*
	 * TFP uses the FPU for bcopy. If the user gets into the debugger,
	 * symmon may clear CU1 and the command "goto dumpsys" will not work.
	 */
	dumpjmp[JB_SR] |= SR_CU1;
#endif

	longjmp(dumpjmp, 0);

	/* Shouldn't ever get here! */
	ASSERT(0);
}

static void real_dumpsys(void)
{
	register int err;

	spl7();

	/* We have to make sure that we are not panicking
	 * even before the dumpdev is setup.
	 */
	if (dumpdev == NODEV || dump_level < 0)
		mdboot(AD_HALT,NULL);
	if (dumpsize == 0)
		panic("Dump failed: dumpsize = 0\n");

	dump_bdevsw = get_bdevsw(dumpdev);
	if (!bdstatic(dump_bdevsw) || !dump_bdevsw->d_dump) {
		panic("\nno dump procedure for dev 0x%x\n", dumpdev);
		/*NOTREACHED*/
	}

	dump_in_progress = 1;

	enter_dump_mode();

	if (dumpdevname)
		printf("\nDumping to %s at block %d, space: 0x%x pages\n",
		       dumpdevname, dumplo, dumpsize);
	else
		printf("\nDumping to dev 0x%x at block %d, "
		       "space: 0x%x pages\n",
		       dumpdev, dumplo, dumpsize);

	/* tell driver to initialize itself */
	if (err = bddump(dump_bdevsw, dumpdev, DUMP_OPEN, 0, 0, 0)) {
		dumperror(err);
		panic("\nDriver dump routine failed\n");
		/*NOTREACHED*/
	}

	physaddr = ctob (pmem_getfirstclick());
	dump_buffer_ptr = (char *) MAPPED_KERN_RW_TO_PHYS(dump_buffer);

	dumpvmcore();

	exit_dump_mode();

	dump_in_progress = 0;

#if SN0
	heartbeat_enable = 0;
	sysctlr_keepalive();		/* Turn off heartbeat monitoring */
#endif

	/* Go somewhere safe. */
	mdboot(AD_HALT, NULL);

	/* just in case prom_reboot doesn't work...sit and spin */
	for (;;)
		;
}


/*
 * dump_page
 *
 * The pfn at which we have to start mapping and copying down pages
 * of memory unless we are a 64-bit kernel.
 * Small note here - the processor doing this is still taking timer
 * interrupts and hence still doing the TLB flushes/n ticks. There
 * is a chance for non _ABI64 kernels, the mapping setup by the
 * tlbdropin is flushed before its use is over.
 */

#if _MIPS_SIM != _ABI64
#define BIG_PFN		SMALLMEM_K0_PFNMAX
#endif

static int dump_page(unsigned long pfn, int flags)
{
	char			*copy_addr;
	int			rc;		/* return code */

#if SN0
	sysctlr_keepalive();

	/*
	 * Pages may be protected/poisoned/discarded for various reasons:
	 * hardware errors, page migration, low memory protection, etc.
	 * Skip them. We don't want to take cache or protection errors
	 * when reading the page trying to compress it.
	 *
	 * NOTE: On platforms other than SN0, not all pfns have a pfdat.
	 *
	 * FIXME: We should put some sort of marker in the dump file.
	 */

	if (page_isdiscarded(ctob(pfn))) {
#if DEBUG
		printf("Skipping page 0x%x (discarded)\n", pfn);
#endif
		return 0;
	}

	if (page_ispoison(ctob(pfn))) {
#if DEBUG
		printf("Skipping page 0x%x (poisoned)\n", pfn);
#endif
		return 0;
	}

	if (!page_read_accessible(pfn)) {
#if DEBUG
		printf("Skipping page 0x%x (not readable)\n", pfn);
#endif
		return 0;
	}

	if (page_iscalias(pfn)) {
#if DEBUG
		printf("Skipping page 0x%x (calias)\n", pfn);
#endif
		return 0;
	}		

#endif /* SN0 */

#if IP30 && !HEART1_ALIAS_WINDOW_WAR
	/* First page of memory on IP30 is only readable @ 0 */
	if (pfn < btoc(PHYS_RAMBASE + SYSTEM_MEMORY_ALIAS_SIZE)) {
		pfn = btoc(ctob(pfn) - PHYS_RAMBASE);
	}
#endif

#if _MIPS_SIM != _ABI64
	if (pfn >= BIG_PFN) {
		pde_t tmp_pte;

		tmp_pte.pgi = mkpde(PG_VR|PG_G|PG_SV|PG_M|pte_cachebits(), pfn);
		kvtokptbl(dumpvaddr)->pgi = tmp_pte.pgi;
#ifdef DEBUG_DUMP
		printf("Dropping in pfn = 0x%x, pte = 0x%x\n",
		       pfn, tmp_pte.pgi);	
#endif
		tlbdropin(0, dumpvaddr, tmp_pte.pte);
		copy_addr = dumpvaddr;
	} else
#endif
	{
		copy_addr = (char *)PHYS_TO_K0(pfn << BPCSHIFT);
	}
#if defined (SN)
        {
		extern int dumpcopy(void *, void *, int);
	
		if (dumpcopy(copy_addr, data_buffer, NBPP) < 0)
		    return 0;

		copy_addr = data_buffer;
	}
#endif
	rc = compress_and_write_block(pfn, copy_addr, NBPP, compr_buffer, flags);
	if ( rc ) {
		return ( rc );
	}

	return 0;
}


/*
 * write_out_header-
 *	Update page count in dump header.
 *	Write the dump header to the appopriate place on disk.
 */
static int
write_out_header(void)
{
	int err = 0;

	/* How many pages have we dumped? */
	dump_hdr.dmp_pages = dump_pages;

	printf("\n");
	if (err = bddump(dump_bdevsw, dumpdev, DUMP_WRITE, dumplo,
			 MAPPED_KERN_RW_TO_PHYS(&dump_hdr), 1)) {
		dumperror(err);
	}

	return err;
}


static void
dumpvmcore(void)
{
	register unsigned long pfn;
	int  rc;
	char *src, *dst;
#if SN0
	extern nasid_t master_nasid;
#endif
#ifdef IP32
	/*
	 * Disable ECC checking to avoid errors during core dumps
	 */
	ecc_disable();
#endif

	/*
	 * Must do this, even on Everests to avoid VCEs.
	 */
	flush_cache();

	/*
	 * Set up dump header
	 */

	dump_hdr.dmp_magic = DUMP_MAGIC;
	dump_hdr.dmp_version = DUMP_VERSION;
	/* We are dumping the putbuf & errbuf immediately after
	 * the dump header as of version 3. 
	 * Logically the dump header now includes the 
	 * 	dump header structure + putbuf + errbuf.
	 */
	/* Setup the putbuf,errbuf offsets & sizes  
	 * Right now the order of dumping is
	 * 	dump_header structure --> putbuf --> errbuf
	 */
	dump_hdr.dmp_putbuf_offset = sizeof(dump_hdr_t);
	dump_hdr.dmp_putbuf_size = putbufsz;
	dump_hdr.dmp_errbuf_offset = sizeof(dump_hdr_t) + putbufsz;
	dump_hdr.dmp_errbuf_size = errbufsz;
	dump_hdr.dmp_hdr_sz = sizeof(dump_hdr_t) + putbufsz + errbufsz;
	dump_hdr.dmp_pg_sz = NBPP;
	dump_hdr.dmp_physmem_start = physaddr;
	dump_hdr.dmp_dir_mem_sz = 0;

#ifdef SN0
	if ( dump_hub_info ) {
		dump_hdr.dmp_dir_mem_sz = (sizeof(dir_mem_entry_t)) *
					(NBPP/MD_PAGE_SIZE);
	}

	dump_hdr.dmp_master_nasid = master_nasid;
#endif


	/* We correct this when we know how big the dump will
	 * really be.  We currently rewrite the dfirst block of the header
	 * after we dump each section.
	 */
	dump_hdr.dmp_size = dumpsize;
	dump_hdr.dmp_crash_time = time;

	/* Assume the header will be written. */
	dump_hdr.dmp_pages = (sizeof(dump_hdr_t) + dump_buffer_size - 1)
							/ dump_buffer_size;

	/* physmem size in megabytes */
	dump_hdr.dmp_physmem_sz = physmem >> (20 - BPCSHIFT);

	/* Copy the uname into the header. */	
	strcpy(dump_hdr.dmp_uname, utsname.sysname);
	strcat(dump_hdr.dmp_uname, " ");
	strcat(dump_hdr.dmp_uname, utsname.nodename);
	strcat(dump_hdr.dmp_uname, " ");
	strcat(dump_hdr.dmp_uname, utsname.release);
	strcat(dump_hdr.dmp_uname, " ");
	strcat(dump_hdr.dmp_uname, utsname.version);
	strcat(dump_hdr.dmp_uname, " ");
	strcat(dump_hdr.dmp_uname, utsname.machine);

	if (panicstr) {
		dst = dump_hdr.dmp_panic_str;
		src = panicstr;
		do {
			if (*src == '\n') {
				*dst = '\0';
				break;
			} else if (*src == '\0') {
				src++;
			} else {
				*dst++ = *src++;
			}
		} while (src < &panicstr[DUMP_PANIC_LEN]);
	}

	bcopy(putbuf, dump_hdr.dmp_putbuf, MIN(putbufsz, DUMP_PUTBUF_LEN));
	dump_hdr.dmp_putbufndx = putbufndx;
	dump_hdr.dmp_putbufsz = MIN(putbufsz, DUMP_PUTBUF_LEN);

	/* dumpsize == swap space available for dumping in pages. */

	dump_sctr = dumplo;
		/* dumplo = where to start the dump.  May need to rethink
		 * this.  It starts at the top of swap space.  It's
		 * theoretically possible for us to require more than
		 * the physical space available for these dumps.
		 * We could start at the top and write downward if
		 * that's better...  Why favor the end anyway?
		 * see main.c.
		 */

	dump_dotpage = DUMP_DOTPAGES;
	dump_pages = 0;

#if SN0
	ip27log_printf(IP27LOG_INFO, "System dump started");
#endif

	/* Dump the whole header. */
	dump_write((char *)&dump_hdr, sizeof(dump_hdr_t));

	/* Dump the putbuf */
	dump_write(putbuf,putbufsz);
	
	/* Dump the errbuf */
	dump_write(errbuf,errbufsz);

	/*
	 * On Everest machines, it's really important to get the first
	 * 16k of memory dumped to allow the FRU analyzer to run.
	 * On other platforms, it does no harm.
	 */
	printf("Dumping low memory...");

	for (pfn = btoc (physaddr); pfn < btoc (physaddr + 0x4000); pfn++) {
		if ( ( rc =  dump_page(pfn, DUMP_DIRECT) ) > 0 ) { 
			
			/*
			 *   the available disk space has been filled:
			 *   write an updated header and return.
			 */

			write_out_header();
	        	printf("\n");
			return;

		} else if ( rc < 0 ) {
			break;
		}
        }


	/*
	 * Write out the first block of header (contains the dump size).
	 * Exaggerate dump_pages here to make sure we get the error dump
	 * structure on EVEREST machines.
	 *
	 * We will periodically update the dump header in case we crash
	 * while dumping.
	 */

	dump_pages++;
	write_out_header();
	dump_pages--;

	if (dump_level >= 1) {
	    printf("Dumping static kernel pages...");
	    if ( dump_lowmem() > 0 ) {
		
		/*
		 *   the available disk space has been filled:
		 *   write an updated header and return.
		 */

		write_out_header();
	        printf("\n");
		return;
	    }

	    write_out_header();

#if DISCONTIG_PHYSMEM
	    /*
	     * Make sure pfdats and key backtrace info are dumped.  
	     * Since PDAs, kthreads, stacks, etc. are allocated
	     * dynamically, we could run out of room before
	     * dumping them on large systems.
	     */

	    printf("Dumping pfdat pages...");
	    if ( dump_pfdats() > 0 ) {
		
		/*
		 *   the available disk space has been filled:
		 *   write an updated header and return.
		 */

		write_out_header();
	        printf("\n");
		return;
	    }

	    write_out_header();
#endif /* DISCONTIG_PHYSMEM */
	    printf("Dumping backtrace pages...");
	    dump_btinfo();
	    write_out_header();

	    printf("Dumping dynamic kernel pages...");
	    if ( pfd_scan(SCAN_KERN_NONBULK, DUMP_SELECTED) > 0 ) {
		
		/*
		 *   the available disk space has been filled:
		 *   write an updated header and return.
		 */

		write_out_header();
	        printf("\n");
		return;
	    }

	    write_out_header();
	}

#if	defined(IP27)
	move_buffer();
#endif


#if !defined (_VCE_AVOIDANCE)	/* No room for BULKDATA flag on IP22 */
	if (dump_level >= 2) {
	    printf("Dumping buffer pages...");
	    if ( pfd_scan(SCAN_KERN_BULK, DUMP_SELECTED) > 0 ) {
		
		/*
		 *   the available disk space has been filled:
		 *   write an updated header and return.
		 */

		write_out_header();
	        printf("\n");
		return;
	    }

	    write_out_header();
	}
#endif

	if (dump_level >= 3) {
	    printf("Dumping remaining in-use pages...");
	    if ( pfd_scan(SCAN_INUSE, DUMP_INUSE) > 0 ) {
		
		/*
		 *   the available disk space has been filled:
		 *   write an updated header and return.
		 */

		write_out_header();
	        printf("\n");
		return;
	    }

	    write_out_header();
	}

	if (dump_level >= 4) {
	    printf("Dumping free pages...");
	    if ( pfd_scan(SCAN_FREE, DUMP_FREE) > 0 ) {
		
		/*
		 *   the available disk space has been filled:
		 *   write an updated header and return.
		 */

		write_out_header();
	        printf("\n");
		return;
	    }

	    if ( pfd_scan(SCAN_UNDUMPED, DUMP_FREE) > 0 ) {
		
		/*
		 *   the available disk space has been filled:
		 *   write an updated header and return.
		 */

		write_out_header();
	        printf("\n");
		return;
	    }

	    printf("\n");
	}

#if SN0
	if ( dump_hub_info ) {
		printf("Dumping hub information ...");
		if ( dump_hub_data() > 0 ) {
			write_out_header();
			printf("\n");
			return;
		}
	}
	printf("\n");
#endif

	printf("Flushing out buffers...");
	dump_dir_ent(0, 0, DUMP_END);

	/* Flush out anything in our write buffer */
	if (dump_buflen > 0)
		dump_flush();

	printf("\nUpdating dump header...");

	if (!write_out_header()) {
		printf("Dump complete.\n");
#if SN0
		ip27log_printf(IP27LOG_INFO, "System dump completed");
#endif
	}
}


#if DISCONTIG_PHYSMEM
/*
 * Dump the pfdats associated with each physical memory range.
 */

static int
dump_pfdats()
{
	cnodeid_t	node;
	pfd_t		*pfdp;
	pfn_t		pfn;
	int		pgs;
	int		rc;

	for (node = 0; node < numnodes; node++) {
		pfdp = PFD_LOW(node);

		while ((pfdp = pfdat_probe(pfdp, &pgs)) != NULL) {
			pfn  = kvatopfn(pfdp);
			pfdp += pgs;
			pgs  = btoc(pgs * sizeof(pfd_t));

			while (pgs--) {
				if ( rc = dump_page(pfn, DUMP_SELECTED) )
					return ( rc );

				pfn++;
			}
		}
	}

#ifdef SN0
	/* Dump the kptbl, which isn't in lowmem on SN0.  See startup.c */
	if ( rc = dump_range((caddr_t)kptbl,
		       (syssegsz + btoc(MAPPED_KERN_SIZE)) * sizeof(pde_t)) )
		return ( rc );
#endif /* SN0 */
	return ( 0 );
}
#endif /* DISCONTIG_PHYSMEM */


#define MARK_DUMPED(pfn) pfntopfdat(pfn)->pf_tag = PAGE_DUMPED_TAG
#define IS_DUMPED(pfn) ((pfn) < \
	pfdattopfn(PFD_LOW(NASID_TO_COMPACT_NODEID(pfn_nasid(pfn)))) || \
	pfntopfdat(pfn)->pf_tag == PAGE_DUMPED_TAG)
#define PTR_OK(ptr) ((IS_SMALLVFN_K0(vatovfn(ptr)) || 		\
		      IS_KSEG2(ptr) && pnum((ptr) - K2SEG) < syssegsz) \
		     && ((__psunsigned_t)(ptr) & 0x7) == 0)

static int
dump_range(caddr_t addr, int len)
{
	pfn_t	pfn;
	caddr_t	a;
	int  	pcnt;

	if (!PTR_OK(addr) || !PTR_OK((__psunsigned_t)(addr+len-1)&(~0x7))) {
#ifdef DEBUG_DUMP
		printf("Skipping bad range: 0x%x len 0x%x\n", addr, len);
#endif
		return 0;
	}

	for (pcnt = 0, a = addr; pcnt < numpages(addr, len); 
	     pcnt++, a += NBPP) {
		pfn = kvatopfn(a);
		if (IS_DUMPED(pfn))
			continue;
		if (dump_page(pfn, DUMP_SELECTED))
			return 1;
		MARK_DUMPED(pfn);
	}
	return 0;
}

/*
 * Dump critical info. for backtraces, including the PDAs,
 * current kthreads, and their stacks.  I.e. whatever is needed for
 * icrash to generate its standard report.
 */

static void
dump_btinfo()
{
	cpuid_t		cpu;
	pfn_t		pfn;
	kthread_t	*kt;
	pda_t		*pda;
	extern int	intstacksize;
	extern k_machreg_t intstack[];

/* Avoid dumping previously dumped pages */
#define DUMP_PAGE(pfn) \
	if (!IS_DUMPED(pfn)) {					\
		if (dump_page((pfn), DUMP_SELECTED))		\
			return;					\
		MARK_DUMPED(pfn);				\
	}

	for (cpu = 0; cpu < MAX_NUMBER_OF_CPUS(); cpu++) {
		if (pdaindr[cpu].CpuId == -1)
			continue;

		/* Dump the PDA page, and the boot stack within it */
		pda = pdaindr[cpu].pda;
		if (!PTR_OK(pda)) {
#ifdef DEBUG_DUMP
		    printf("cpu %d PDA is bad: 0x%x\n", cpu, pda);
#endif
		    continue;
		}
		pfn = kvatopfn(pda);
		DUMP_PAGE(pfn);

		/* And the interrupt stack */
		if (dump_range((caddr_t)pda->p_intstack, intstacksize))
			return;

#if EXTKSTKSIZE == 1
		/* Get any stack extension pages */
		if (pda->p_stackext.pgi) {
			pfn = pdetopfn(&pda->p_stackext);
			DUMP_PAGE(pfn);
		}
		if (pda->p_bp_stackext.pgi) {
			pfn = pdetopfn(&pda->p_bp_stackext);
			DUMP_PAGE(pfn);
		}
#endif /* EXTKSTKSIZE */

#if SN0
		/* If we decide to ensure that single-bit error info is
		   saved early on, NODEPDA(cputocnode(cpu))->sbe_info
		   could be dumped at this point. */
#endif /* SN0 */

		/* Dump the kthread structure */
		if ((kt = pda->p_curkthread) == NULL)
			continue;
		if (!PTR_OK(kt)) {
#ifdef DEBUG_DUMP
		    printf("cpu %d kthread is bad: 0x%x\n", cpu, kt);
#endif
		    continue;
		}
		if (dump_range((caddr_t)kt, sizeof(kthread_t)))
			return;			

		if (KT_ISUTHREAD(kt)) {
			int i;
			uthread_t *ut = KT_TO_UT(kt);

			/* Uthreads have mapped kernel stack pages */
			for (i = 0; i < sizeof(ut->ut_kstkpgs)/sizeof(pde_t); i++) {
				pfn = pdetopfn(&ut->ut_kstkpgs[i]);
				if (!pfn)
					continue;
				DUMP_PAGE(pfn);
			}

			/* Also dump its exception info and proc structure */
			if (dump_range((caddr_t)ut->ut_exception,
				       sizeof(struct exception)))
				return;
			if (dump_range((caddr_t)ut->ut_proc, sizeof(struct proc)))
				return;			
		}
		else {
			/* Dump the thread's stack. */
			if (dump_range(kt->k_stack, kt->k_stacksize))
				return;
		}
	}
#undef DUMP_PAGE
}
#undef PTR_OK
#undef IS_DUMPED
#undef MARK_DUMPED

/*
 * Dump low kernel memory.  This is memory that was statically allocated
 * at boot and therefore there are no pfdats associated with this memory.
 * This low memory extends from the base of physical memory on each node
 * up to the first page in the node for which there is a pfdat.
 */

static int
dump_lowmem()
{
	cnodeid_t	node;
	pfn_t		pfn, last;
	int		rc;

	for (node = 0; node < numnodes; node++) {
		/* First 16k of node 0 memory was already dumped. */
		if (node == 0)
			pfn = btoc(physaddr + 0x4000);
		else
			pfn = mkpfn(COMPACT_TO_NASID_NODEID(node), 0);

		last = pfdattopfn(PFD_LOW(node)) - 1;


		for (; pfn <= last; pfn++) {
#if SN0
			/* While dumping static kernel pages
			 * check to see if we are dumping 
			 * unnecessary replicated text 
			 */
			if (!page_dumpable(node,pfn)) {
				continue;
			}
#endif /* SN0 */	
			if ( rc = dump_page(pfn, DUMP_DIRECT) )
				return ( rc );
		}
	}
	return ( 0 );
}

/*
 * pfd_scan
 *
 * Scan all the pfdats on the system and dump the associated pages
 * whose pfdats meet the required criteria according to scan_type.
 * Write special value to pf_tag to indicate page already dumped.
 * (The real pf_tag field has already been dumped).
 */

static int
pfd_scan(int scan_type, int dump_flag)
{
	cnodeid_t	node;
	pfd_t		*pfdp;
	pfn_t		pfn;
	int		pgs, f, dump;
	int		rc;

	for (node = 0; node < numnodes; node++) {
	    pfdp = PFD_LOW(node);

	    while ((pfdp = pfdat_probe(pfdp, &pgs)) != NULL) {
		for (; pgs; pfdp++, pgs--) {
		    if (PG_ISHOLE(pfdp))
			continue;

		    pfn = pfdattopfn(pfdp);
		    f = pfdp->pf_flags;
		    dump = 0;

		    switch (scan_type) {
		    case SCAN_KERN_NONBULK:
			if ((f & (P_DUMP|P_BULKDATA)) == P_DUMP) {
			    dump = 1;
			}
			break;
#if !defined (_VCE_AVOIDANCE)	/* No room for BULKDATA flag on IP22 */
		    case SCAN_KERN_BULK:
			if ((f & (P_DUMP|P_BULKDATA)) == (P_DUMP|P_BULKDATA)) {
			    dump = 1;
			}
			break;
#endif
		    case SCAN_INUSE:
			if ((f & (P_DUMP|P_QUEUE|P_SQUEUE)) == 0) {
			    dump = 1;
			}
			break;
		    case SCAN_FREE:
			if ((f & (P_QUEUE|P_SQUEUE)) != 0) {
			    dump = 1;
			}
			break;
		    case SCAN_UNDUMPED:
			dump = 1;
			break;
		    }
		    if (dump && pfdp->pf_tag != PAGE_DUMPED_TAG) {
			if ( rc = dump_page(pfn, dump_flag) )
			    return ( rc );
			pfdp->pf_tag = PAGE_DUMPED_TAG;
		    }
		}
	    }
	}
	return ( 0 );
}

static int
compress_block(char *old_addr, int len, char *new_addr)
{
    int read_index;
    int count;
    unsigned char value;
    unsigned char cur_byte;
    int write_index;

    write_index = read_index = 0;

#if (NBPP == 16384)
    /*
     * Optimization for 16K pages that are all zeroes
     */

    if (((__psunsigned_t) old_addr & 7) == 0) {
        __psunsigned_t  *ap;

      for (ap = (__psunsigned_t *) old_addr;
           ap < (__psunsigned_t *) (old_addr + NBPP); ap++)
          if (*ap != 0)
              break;

      if (ap == (__psunsigned_t *) (old_addr + NBPP)) {
          bcopy(zeroes_block, new_addr, sizeof (zeroes_block));
          return sizeof (zeroes_block);
      }
  }
#endif

    while ( read_index < len) {
	if (read_index == 0) {
	    cur_byte = value = old_addr[read_index];
	    count = 0;
	} else {
	    if (count == 255) {
		if (write_index + 3 > len)
		    return len;
		new_addr[write_index++] = 0;
		new_addr[write_index++] = count;
		new_addr[write_index++] = value;
		value = cur_byte = old_addr[read_index];
		count = 0;
	    } else { 
		if ((cur_byte = old_addr[read_index]) == value) {
		    count++;
		} else {
		    if (count > 1) {
			if (write_index + 3 > len)
			    return len;
			new_addr[write_index++] = 0;
			new_addr[write_index++] = count;
			new_addr[write_index++] = value;
		    } else if (count == 1) {
			if (value == 0) {
			    if (write_index + 3 > len)
				return len;
			    new_addr[write_index++] = 0;
			    new_addr[write_index++] = 1;
			    new_addr[write_index++] = 0;
			} else {
			    if (write_index + 2 > len)
				return len;
			    new_addr[write_index++] = value;
			    new_addr[write_index++] = value;
			}
		    } else { /* count == 0 */
			if (value == 0) {
			    if (write_index + 2 > len)
				return len;
			    new_addr[write_index++] = value;
			    new_addr[write_index++] = value;
			} else {
			    if (write_index + 1 > len)
				return len;
			    new_addr[write_index++] = value;
			}
		    } /* if count > 1 */
		    value = cur_byte;
		    count = 0;
		} /* if byte == value */
	    } /* if count == 255 */
	} /* if read_index == 0 */
	read_index++;
    }
    if (count > 1) {
	if (write_index + 3 > len)
	    return len;
	new_addr[write_index++] = 0;
	new_addr[write_index++] = count;
	new_addr[write_index++] = value;
    } else if (count == 1) {
	if (value == 0) {
	    if (write_index + 3 > len)
		return len;
	    new_addr[write_index++] = 0;
	    new_addr[write_index++] = 1;
	    new_addr[write_index++] = 0;
	} else {
	    if (write_index + 2 > len)
		return len;
	    new_addr[write_index++] = value;
	    new_addr[write_index++] = value;
	}
    } else { /* count == 0 */
	if (value == 0) {
	    if (write_index + 2 > len)
		return len;
	    new_addr[write_index++] = value;
	    new_addr[write_index++] = value;
	} else {
	    if (write_index + 1 > len)
		return len;
	    new_addr[write_index++] = value;
	}
    } /* if count > 1 */
    value = cur_byte;
    count = 0;

    return write_index;
}


static int
compress_and_write_block(unsigned long	pfn,
			 char		*copy_addr,
			 int		copy_len,
			 char		*compr_buffer,
			 int		flags)
{
	char		*address;
	int		compr_len;		/* Length of compressed page */

	compr_len = compress_block(copy_addr, copy_len, compr_buffer);

#ifdef DEBUG_DUMP
	printf("compressed: compr_len: %d, blk_len: %d.\n", compr_len, copy_len);
#endif

	/*   if the space we need for the buffer, its directory entry,
	 *   and the "DUMP_END" directory entry is greater than the
	 *   space remaining, return 1.  All disk errors return -1.
	 */

	if ((compr_len + 2 * sizeof(dump_dir_ent_t)) >
			(((off64_t)(dumpsize - dump_pages) << BPCSHIFT)
			- dump_buflen) ) {
		printf("\nAvailable dump space depleted.\n");
		return 1;
	} else if ( compr_len < copy_len ) {
		flags  |= DUMP_RLE;
		address = compr_buffer;
	} else {
		flags  |= DUMP_RAW;
		address = copy_addr;
	}

#ifdef DEBUG_DUMP
	printf("Writing from pfn 0x%x: %s.\n", pfn,
			( compr_len < copy_len ) ? "COMPRESSED" :
						   "RAW");
#endif

	if ( dump_dir_ent(pfn, compr_len, flags) ) {
		return -1;
	} else if ( dump_write(address, compr_len) ) {
		return -1;
	}
	return 0;
}


static int
dump_dir_ent(unsigned long pfn, int new_size, int flags)
{
	dump_dir_ent_t entry;

#ifdef DEBUG_DUMP
	printf("Address == 0x%llx, size == 0x%x, flags == 0x%x\n",
	       pfn, new_size, flags);
#endif
	entry.flags   = flags;
	/* Bottom bits of pfn */
	entry.addr_lo = (pfn & (0x0fffff >> PGSHFTFCTR)) << PNUMSHFT;
	entry.addr_hi = (pfn >> (20 - PGSHFTFCTR));

#if SN0
 	if ( flags & ( DUMP_DIRECTORY | DUMP_REGISTERS ) ) {
		entry.addr_lo = pfn & 0xffffffff;
		entry.addr_hi = pfn >> 32;
	}
#endif

	entry.length  = new_size;

	if (dump_write((char *)&entry, sizeof(dump_dir_ent_t))) {
		return -1;
	} else {
		return 0;
	}
}


#if SN0

int
dump_hub_data()
{
	int		rc;

	if ( dump_hub_info >= 1 ) {

		/*
		 *   dump the hub registers.
		 */

		rc = dump_hub_registers();
		if ( rc ) {
			return rc;
		}
	}

	if ( dump_hub_info >= 2 ) {

		/*
		 *   dump the directory words ( high and low ) 
		 *   as well as the protection and page reference
		 *   word from the hub.
		 */

		rc = dump_hub_dir_mem();
		if ( rc ) {
			return rc;
		}
	}
	
	return 0;
}


int
dump_hub_dir_mem()
{
	pfn_t			last;
	cnodeid_t		node;
	pfd_t			*pfdp;
	pfn_t			pfn;
	int			pgs;
	int			rc;

	for ( node = 0; node < numnodes; node++ ) {
		pfn  = mkpfn(COMPACT_TO_NASID_NODEID(node), 0);
		last = pfdattopfn(PFD_LOW(node)) - 1;

		for (; pfn <= last; pfn++) {

			/* 
			 *   check to see if we have
			 *   bypassed replicated text.
			 */
			if (!page_dumpable(node,pfn)) {
				continue;
			}
			if ( rc = write_hub_dir_mem(pfn) )  {
				return rc;
			}
		}

		pfdp = PFD_LOW(node);

		while ((pfdp  = pfdat_probe(pfdp, &pgs)) != NULL) {
			for (; pgs; pfdp++, pgs--) {
				if ( PG_ISHOLE(pfdp) ) {
					continue;
				}
		    		else if ( pfdp->pf_tag != PAGE_DUMPED_TAG ) {
					continue;
				}

				pfn = pfdattopfn(pfdp);

				if ( rc = write_hub_dir_mem(pfn) ) {
					return rc;
				}
			}
		}
	}
	return 0;
}


int
write_hub_dir_mem(pfn_t pfn)
{
	uint64_t		addr_lo;
	uint64_t		*dir_addr_hi;
	uint64_t		*dir_addr_lo;
	uint64_t		flags;
	int			i;
	int			j;
	nasid_t			nasid;
	int			node;
	int			premium;
	int			rc;		/* return code */

	/*
	 *   because pages could have been skipped as they could have
	 *   been protected/poisoned/discarded for various reasons:
	 *   hardware errors, page migration, low memory protection, etc.
	 *   since these pages are not part of the dump, bypass their
	 *   directory memory pages also.
	 */

	if (page_isdiscarded(ctob(pfn))) {

#if DEBUG
		printf("Skipping dir mem for page 0x%x (discarded)\n", pfn);
#endif
		return 0;
	}

	if (page_ispoison(ctob(pfn))) {

#if DEBUG
		printf("Skipping dir mem for page 0x%x (poisoned)\n", pfn);
#endif
		return 0;
	}

	if (!page_read_accessible(pfn)) {

#if DEBUG
		printf("Skipping dir mem for page 0x%x (not readable)\n", pfn);
#endif
		return 0;
	}

	if (page_iscalias(pfn)) {

#if DEBUG
		printf("Skipping dir mem for page 0x%x (calias)\n", pfn);
#endif
		return 0;
	}		

	addr_lo   = ctob(pfn);
	nasid	  = pfn_nasid(pfn);
	premium   = (REMOTE_HUB_L(nasid, MD_MEMORY_CONFIG) &
					MMC_DIR_PREMIUM);

	if ( is_fine_dirmode() ) {
		node = 1<<NASID_TO_FINEREG_SHFT;
	} else {
		node = 1<<NASID_TO_COARSEREG_SHFT;
	}

	/*
	 *   There is a directory entry for each 4KB page in the
	 *   nodes main memory. Within each directory entry there
	 *   is for each cache line a directory word high and a
	 *   directory word low. Additionally each directory entry 
	 *   contains a page reference count for each region in
	 *   the system.
	 */

	for ( i = 0; i < NBPP/MD_PAGE_SIZE; i++ ) {

		/*
		 *   dependent on the number of nodes/regions in the system,
		 *   not all of the structure might be written: initialize
		 *   its contents.
		 */

		for ( j = 0; j < MAX_REGIONS; j++ ) {
			dir_mem_entry[i].prcpf[j] = DUMP_HUB_INIT_PATTERN;
		}

		for ( j = 0; j < numnodes; j += node ) {
			nasid = COMPACT_TO_NASID_NODEID(j/node);
			dir_mem_entry[i].prcpf[nasid] =
				*(__psunsigned_t *)BDPRT_ENTRY(addr_lo +
					(i * MD_PAGE_SIZE), nasid);
		}
		
		for ( j = 0; j < MD_PAGE_SIZE/CACHE_SLINE_SIZE; j++ ) {

			dir_addr_hi = (__uint64_t *)
				BDDIR_ENTRY_HI((paddr_t)addr_lo +
					(i * MD_PAGE_SIZE) + (j * CACHE_SLINE_SIZE));
			dir_addr_lo = (__uint64_t *)
				BDDIR_ENTRY_LO((paddr_t)addr_lo +
					(i * MD_PAGE_SIZE) + (j * CACHE_SLINE_SIZE));

			if ( premium ) {
				dir_mem_entry[i].directory_words[j].md_dir_high.
					md_pdir_high.pd_hi_val = *(dir_addr_hi);
				dir_mem_entry[i].directory_words[j].md_dir_low.
					md_pdir_low.pd_lo_val  = *(dir_addr_lo);
			} else {
				dir_mem_entry[i].directory_words[j].md_dir_high.
					md_sdir_high.sd_hi_val = *(dir_addr_hi);
				dir_mem_entry[i].directory_words[j].md_dir_low.
					md_sdir_low.sd_lo_val  = *(dir_addr_lo);
			}
		}
	}

	/*
	 *   compress the information.
	 */

	flags     = DUMP_DIRECTORY;
	flags    |= ( premium ? DUMP_PREMIUM_DIR : 0 );

	rc = compress_and_write_block((unsigned long)BDPRT_ENTRY(addr_lo, 0),
					(char *)&dir_mem_entry,
					(sizeof(dir_mem_entry_t)) * (NBPP/MD_PAGE_SIZE),
					compr_buffer, flags); 
	return rc;
}


int
dump_hub_registers()
{
	int			bn;		/* base number */
	int			bo;		/* base offset */
	uint64_t		buffer[NBPP/sizeof(hubreg_t)];
	int			cbn;		/* current base number */
	int			cpn;		/* current page number */
	uint64_t		flags;
	hubreg_t		hub_reg;
	int			i;
	int			j;
	nasid_t			nasid;
	cnodeid_t		node;
	volatile hubreg_t	*pa;
	int			pn;		/* page number */
	int			po;		/* page offset */
	int			rc;		/* return code */


	flags = DUMP_REGISTERS;

	for ( i = 0; i < NBPP/sizeof(hubreg_t); i++ ) {
		buffer[i] = DUMP_HUB_INIT_PATTERN;
	}

	for ( node = 0; node < numnodes; node++ ) {
	    nasid = COMPACT_TO_NASID_NODEID(node);

	    /*
	     *   read from table boundary the address of the first and last
	     *   hub register of a group of consecutive hub registers. If
	     *   only one register makes up a group of consecutive registers 
	     *   the first and last address are the same.
	     */

	    for ( j = 0; j < BOUNDARIES; j += BOUNDARY_ENTRY ) {

		if ( j == 0 ) {

			/*
			 *   first time through: initialize current
			 *   base number and current page number.
			 *   the base number is a value that could be
			 *   PI_BASE, MD_BASE, IIO_BASE or NI_BASE. 
			 *   the page number is the page number within
			 *   the base. if any of these base numbers
			 *   change or if the page number changes the
			 *   buffer under construction has to be 
			 *   written out.
			 */

			cbn = ( boundary[j] / MD_BASE ) * MD_BASE;
			cpn = ( boundary[j] % MD_BASE ) / NBPP;
		}

		if ( boundary[j] >= 0 ) {
			bn = ( boundary[j] / MD_BASE ) * MD_BASE;
			pn = ( boundary[j] % MD_BASE ) / NBPP;
		}

		if ( ( cbn != bn ) || ( cpn != pn ) || ( boundary[j] < 0 ) ) {
			
			/*
			 *   the current base number or the current
			 *   page number has changed or all information 
			 *   for one node has been completed: compress
			 *   the information in the buffer under
			 *   construction and write it out.
			 */

			rc = compress_and_write_block((unsigned long)pa,
					(char *)buffer, sizeof(buffer),
					compr_buffer, flags); 
			if ( rc ) {
				return rc;
			}
	
			/*
			 *   prepare the buffer for the next set of
			 *   registers.
			 */

			for ( i = 0; i < NBPP/sizeof(hubreg_t); i++ ) {
				buffer[i] = DUMP_HUB_INIT_PATTERN;
			}

			if ( boundary[j] <  0 ) {

				/*
				 *   continue with the next node.
				 */

				break;
			}

			cbn = bn;
			cpn = pn;
		}

		pa = REMOTE_HUB_ADDR(nasid,(bn + (pn * NBPP)));

		for ( i = boundary[j]; i <= boundary[j+1]; i += HUB_REG_PACE ) {

			hub_reg  = REMOTE_HUB_L(nasid,i);

			bn = ( i / MD_BASE ) * MD_BASE;
			bo = i % MD_BASE;
			po = bo % NBPP;
			pn = bo / NBPP;

			if ( ( cbn == bn ) && ( cpn == pn ) ) { 
				buffer[po/sizeof(hubreg_t)] = hub_reg;
				continue;
			}


			/*
			 *   the register number as calculated in
			 *   the for loop causes the base number
			 *   or the page number to be different
			 *   from its current value: compress the
			 *   information in the buffer and write
			 *   it out.
			 */

			rc = compress_and_write_block((unsigned long)pa,
					(char *)buffer, sizeof(buffer),
					compr_buffer, flags); 
			if ( rc ) {
				return rc;
			}
	
			cbn    = bn;
			pa    += ( pn - cpn ) * NBPP;
			cpn    = pn;

			for ( i = 0; i < NBPP/sizeof(hubreg_t); i++ ) {
				buffer[i] = DUMP_HUB_INIT_PATTERN;
			}

			buffer[po/sizeof(hubreg_t)] = hub_reg;
		}
	    }
	}
	return 0;
}
#endif /* SN0 */	


/*
 * Flush out data in our dump buffer
 */
static int
dump_flush(void)
{
	register int err;

#ifdef DEBUG_DUMP
	printf("Writing %d blocks to sector 0x%x\n",
	       (dump_buffer_size >> SCTRSHFT), dump_sctr);
#endif

	err = bddump(dump_bdevsw, dumpdev, DUMP_WRITE, dump_sctr,
			dump_buffer_ptr, (dump_buffer_size >> SCTRSHFT));
	if ( err ) {
		dumperror(err);
		return -1;
	}
	dump_sctr += (dump_buffer_size >> SCTRSHFT);
	dump_pages += dump_buffer_size / NBPP;
	if (dump_pages > dump_dotpage) {
		dump_dotpage += DUMP_DOTPAGES;
		/* Should not be in promlog mode at this point.
		 * Go into promlog mode temporarily since
		 * we don't want each and every dot to go 
		 * all the promlogs during a ip27log_panic_printf.
		 */
		enter_promlog_mode();
		printf(".");
		exit_promlog_mode();
#if IP27
		/* On IP27, flush the "." to the ELSC uart. */
		if (in_panic_mode() && private.p_elsc_portinfo) {
		    extern void du_down_flush(void*);
		    du_down_flush(private.p_elsc_portinfo);
		}
# endif
	}
	dump_buflen = 0;
	return 0;
}

static int
dump_write(char *buffer, int block_len)
{
	while (block_len > 0) {
		int amt = MIN(block_len, dump_buffer_size - dump_buflen);

		bcopy(buffer, (unsigned long *)PHYS_TO_K0(dump_buffer_ptr + dump_buflen), amt);

		buffer += amt;
		dump_buflen += amt;
		block_len -= amt;
		if (dump_buflen == dump_buffer_size && dump_flush()) {
			return -1;
		}
	}

        return 0;
}


#if	defined(IP27)

/*
 *	move_buffer()
 *
 * 	determine whether on a different node a relatively large
 *	piece of memory is available that could serve as a buffer
 *	to hold the dump information that is ready to be written
 *	to the disk. By using a larger sized buffer the number of
 *	disk accesses is reduced and therefore the time it takes
 *	to dump a system.
 */

static void
move_buffer()
{
	int		buffer_size;
	char		*bufferp;
	hubreg_t	calias;
	cpuid_t		cpu;
	int		error		= 0;
	long		first;
	int		i;
	int		j;
	nmi_t		*nmi_addr;
	cnodeid_t       node;
	pfn_t           pfn, last;
	long		ro_first;
	pfn_t		ro_last;

	if ( numnodes == 1 ) {
		return;
	}

	if ( dump_buflen >= 0 ) {

		/*
		 *   in case no enlarged buffer is
		 *   to be assigned, save specifics
		 *   of the currently assigned buffer.
		 */

		bufferp     = dump_buffer_ptr;
		buffer_size = dump_buffer_size;
	}

	/*
	 *   start with node 1: node 0 has global variables
	 *   assigned to it. If these are wiped out the ql
	 *   driver might behave unpredictably.
	 */


	for ( node = 1; node < numnodes; node++ ) {

		/*
		 *   do not consider as an enlarged buffer
		 *   the memory in our own node.
		 */

		if ( node == get_compact_nodeid() ) {
			continue;
		}

		/*
		 *   do not consider as an enlarged buffer the
		 *   memory in the node in which the replicated
		 *   text is located we are currently executing.
		 */

		else if ( COMPACT_TO_NASID_NODEID(node) ==
					nodepda->kern_vars.kv_ro_nasid ) {
			continue;
		}

#if defined (MAPPED_KERNEL)

		/*
		 *   do not consider as an enlarged buffer
		 *   the memory in a node in which no kernel
		 *   text is located.
		 */

		else if ( !( CPUMASK_TSTB(*(GDA->g_ktext_repmask), node) ) ) {
			continue;
		}
#endif	/* MAPPED_KERNEL */

		calias = REMOTE_HUB_L(COMPACT_TO_NASID_NODEID(node),
						PI_CALIAS_SIZE);
		first  = calias ? ( 0x800 << calias ) : 0;

#if defined (MAPPED_KERNEL)

		/*
		 *   obtain the starting address of the read-only 
		 *   part of the kernel.
		 */

		ro_first = ctob(pfn_nasidoffset(btoc(
				K0_TO_PHYS(MAPPED_KERN_RO_PHYSBASE(node)))));
		if ( first < ro_first ) {
			first = ro_first;
		} 
#endif	/* MAPPED_KERNEL */

		pfn  = mkpfn(COMPACT_TO_NASID_NODEID(node),
					btoc(first));

		last = pfdattopfn(PFD_LOW(node)) - 1;

#if defined (MAPPED_KERNEL)

		/*
		 *   obtain the last address of the read-only
		 *   part of the kernel.
		 */

		ro_last = mkpfn(COMPACT_TO_NASID_NODEID(node),
					btoc(ro_first + MAPPED_KERN_PAGE_SIZE));
		if ( last > ro_last ) {
			last = ro_last;
		}
#endif	/* MAPPED_KERNEL */

		dump_buffer_size = ctob(last - pfn);


		if ( dump_buffer_size >= 2 * DUMP_BLOCKSIZE ) {
			dump_buffer_size &= ~( ( ( DUMP_BLOCKSIZE / NBPP ) * NBPP ) - 1 );
		} else {
			dump_buffer_size  = buffer_size;
			return;
		}

		if ( dump_buffer_size > ctob(maxdmasz) ) {
			dump_buffer_size = ctob(maxdmasz);
		}

		dump_buffer_ptr  = (char *) PHYS_TO_K0(ctob(pfn));

		/*
		 *   check all pages in the newly assigned buffer
		 *   for write and read access. If an error is
		 *   returned change the page access permission
		 *   and try one more time to access the page.
		 *   if this fails, try the next node and if there
		 *   is no other node, use the previously assigned
		 *   buffer.
		 */

		for ( i = 0; i < dump_buffer_size; i += CACHE_SLINE_SIZE ) {
			if ( !memory_present( (paddr_t) (dump_buffer_ptr + i) ) ) {
				error ++;
				break;
			} else if ( !memory_write_accessible( (paddr_t) (dump_buffer_ptr + i) ) ) {
				if ( error ) {
					break;
				}
				page_allow_access( (paddr_t) (dump_buffer_ptr + i) );
				i -=  CACHE_SLINE_SIZE;
				error++;
				continue;
			} else if ( !memory_read_accessible( (paddr_t) (dump_buffer_ptr + i) ) ) {
				if ( error ) {
					break;
				}
				page_allow_access ( (paddr_t) (dump_buffer_ptr + i) );
				i -=  CACHE_SLINE_SIZE;
				error++;
				continue;
			}
		}

		if ( error ) {
			error = 0;
			continue;
		}
		break;
	}

	if ( node >= numnodes ) {
		dump_buffer_ptr  = bufferp;
		dump_buffer_size = buffer_size;
		return;
	}

	/*   the buffer has been established: nmi the cpus
	 *   that belong to this node and get them out of,
	 *   possibly, reading memory in panicspin().
	 */

	if ( ( cpu = CNODE_TO_CPU_BASE(node) ) == CPU_NONE ) {

		/* 
		 *   if there is any data left in the previously
		 *   used buffer, copy it out to the newly
		 *   assigned buffer.
		 */
		if ( dump_buflen > 0 ) {
			bcopy(bufferp, dump_buffer_ptr, dump_buflen);
		}
		return;
	}

	for ( i = 0; i < CNODE_NUM_CPUS(node); i++ ) {
		cpuid_t		cpusl;
		nasid_t		mynid = get_nasid();
		nasid_t		nid;
		paddr_t		pa;

		if ( pdaindr[cpu + i].CpuId == -1 ) {
			continue;
		}

		cpusl = cputoslice(cpu + i);
		nid   = COMPACT_TO_NASID_NODEID(node); 

		/*
		 *   turn on permission to write the modified
		 *   nmi handler ( flags field and parameter
		 *   address ), write it,
		 *   reset permission, and nmi the cpus.
		 */

		pa    = NODE_OFFSET(nid) + NMI_OFFSET(nid, cpusl);
		*(__psunsigned_t *)BDPRT_ENTRY(pa, mynid) = MD_PROT_RW;
		nmi_addr = (nmi_t *)NMI_ADDR(nid, cpusl);
		nmi_addr->call_parm = ( volatile addr_t * ) TO_NODE_UNCAC(mynid, (__psunsigned_t)&(remote_cpu_id));
#if DUMPDEBUG
		printf("\nvmdump.02: &(remote_cpu_id): 0x%llx, phys: 0x%llx.",
		&(remote_cpu_id), TO_NODE_UNCAC(mynid, (__psunsigned_t)&(remote_cpu_id)));
#endif
		*((nasid_t *)nmi_addr->call_parm) = -1;		/* preset to impossible */
		*(__psunsigned_t *)BDPRT_ENTRY(pa, mynid) = MD_PROT_RO;
		SEND_NMI( cputonasid(cpu + i), cputoslice(cpu + i) );
#if DUMPDEBUG
		printf("\nvmdump.03: j: %d, nmi_addr->call_parm: 0x%llx", j, nmi_addr->call_parm); 
		printf("\nvmdump.04:        *(nmi_addr->call_parm): 0x%llx", *(nmi_addr->call_parm)); 
#endif		
		/*
		 * Wait some time to see if prom (pod.c) has answered by writing
		 * something into the parameter address. If so, we are guaranteedd
		 * the CPU in question is out of sight and can safely proceed with
		 * core dump generation using the 'borrowed' memory from that CPU.
		 * But let's wait only certain amount of time, if we see nothing
		 * after that time, continue anyway, chances are core dump will be
		 * OK. (PV: 694136)
		 */

		for ( j = 0; j < 500000; j++ ) {
			if (*((nasid_t *)nmi_addr->call_parm) != -1 )
				break;
			DELAY ( 100 );		/* microseconds */
		}
#if DUMPDEBUG
		printf("\nvmdump.05: j: %d, nmi_addr->call_parm: 0x%llx", j, nmi_addr->call_parm); 
		printf("\nvmdump.06:        *(nmi_addr->call_parm): 0x%llx", *(nmi_addr->call_parm)); 
#endif
	}
	remote_cpu_id = 0;
#if DUMPDEBUG
	printf("\n");
#endif

	/* 
	 *   if there is any data left in the previously
	 *   used buffer, copy it out to the newly
	 *   assigned buffer.
	 */

	if ( dump_buflen > 0 ) {
		bcopy(bufferp, dump_buffer_ptr, dump_buflen);
	}
}

#endif	/* IP27 */


#ifdef DEBUG

void
go_down()
{
	panic("Forced panic for core dump\n");
}

#endif
