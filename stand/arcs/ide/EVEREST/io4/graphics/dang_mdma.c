/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/************************************************************************
 *									*
 *	dang_mdma.c - dang chip master dma module xfer/int tests	*
 *									*
 ************************************************************************/

#ident  "arcs/ide/EVEREST/io4/graphics/dang_mdma.c $Revision: 1.7 $"

#include <sys/types.h>
#include <sys/sbd.h>
#include <sys/fault.h>
#include <fault.h>
#include <sys/EVEREST/everest.h>
#include <sys/EVEREST/evintr.h>
#include <sys/EVEREST/everror.h>
#include <sys/EVEREST/addrs.h>
#include <sys/EVEREST/evmp.h>
#include <sys/EVEREST/io4.h>
#include <sys/EVEREST/dang.h>
#include <sys/EVEREST/evconfig.h>
#include <sys/gr2.h>
#include <sys/gr2hw.h>
#include <setjmp.h>
#include <uif.h>
#include <io4_tdefs.h>
#include <everr_hints.h>
#include <setjmp.h>
#include "danglib.h"

/* x is the field address, y is the gr2 pointer */
#define GIO_PHYSADDR(x,y) (((long)x)-((long)y)+0x1f000000)
#define DELAY_5_SECONDS		5000000

/*
 * types of transfer data 
 *
 * for now, patterns and address tests.  marching 1's seems rather silly
 * since we are testing the dma engine, not the data path (dang_gr2ram
 * does that)
 */
#define	DATA_PATTERN		0
#define	DATA_ADDR		1
#define DATA_RADDR		2

/*
 * the DP_ defines and the Dang_Xferinfo structure lets me build a table
 * of tests, so that I can quickly add or change test types
 */

#define DP_5			0x55
#define DP_A			0xAA
#define DP_F			0xFF

typedef struct dang_xferinfo{
    char *	testname;		/* name of this subtest */
    int		datatype;		/* defined above */
    unsigned char fillpat;		/* fill pattern if DATA_PATTERN*/
    int		lines;			/* number of lines to xfer */
    int		bcount;			/* bytes xfer per line */
    int		stride;			/* stride per line */
    int		offset;			/* data offset from line start */
    int		direct;			/* 1: htog 0:gtoh */
    int		saddr;			/* 1: static GIO address */
    int		sync;			/* 1: dma sync */
    int		cmplint;		/* 1: interrupt on xfer complete */
}Dang_Xferinfo;

/*
 * this is the test table itself
 *
 * NOTE - transfers of less than 128 bytes cause problems - either the
 * DANG or the gr2 boardset want xfers to be at least one cache line
 * in size
 *
 * Also, do not use static mode for address tests - the entire host memory
 * will be compared to one location on the GR2
 */
static Dang_Xferinfo test_array[] =
{
    /* one line static tests */
    { "small static pattern 1", DATA_PATTERN, DP_5, 1, 128, 0, 0, 1, 1, 0, 1 },
    { "small static pattern 2", DATA_PATTERN, DP_A, 1, 128, 0, 0, 0, 1, 0, 1 },

    /* one line data tests */
    { "small pattern 1", DATA_PATTERN, DP_5, 1, 128, 0, 0, 1, 0, 0, 1 },
    { "small pattern 2", DATA_PATTERN, DP_A, 1, 128, 0, 0, 1, 0, 0, 1 },
    { "small pattern 3", DATA_PATTERN, DP_F, 1, 128, 0, 0, 1, 0, 0, 1 },
    { "small pattern 4", DATA_PATTERN, DP_5, 1, 128, 0, 0, 0, 0, 0, 1 },
    { "small pattern 5", DATA_PATTERN, DP_A, 1, 128, 0, 0, 0, 0, 0, 1 },
    { "small pattern 6", DATA_PATTERN, DP_F, 1, 128, 0, 0, 0, 0, 0, 1 },

    /* one line static tests */
    { "small static pattern, partial cache line 1", DATA_PATTERN, DP_5, 1, 133, 0, 0, 1, 1, 0, 1 },
    { "small static pattern, partial cache line 2", DATA_PATTERN, DP_A, 1, 133, 0, 0, 0, 1, 0, 1 },

    /* one line data tests */
    { "small pattern, partial cache line 1", DATA_PATTERN, DP_5, 1, 133, 0, 0, 1, 0, 0, 1 },
    { "small pattern, partial cache line 2", DATA_PATTERN, DP_A, 1, 133, 0, 0, 1, 0, 0, 1 },
    { "small pattern, partial cache line 3", DATA_PATTERN, DP_F, 1, 133, 0, 0, 1, 0, 0, 1 },
    { "small pattern, partial cache line 4", DATA_PATTERN, DP_5, 1, 133, 0, 0, 0, 0, 0, 1 },
    { "small pattern, partial cache line 5", DATA_PATTERN, DP_A, 1, 133, 0, 0, 0, 0, 0, 1 },
    { "small pattern, partial cache line 6", DATA_PATTERN, DP_F, 1, 133, 0, 0, 0, 0, 0, 1 },

    /* one line address tests */
    { "small address 1", DATA_ADDR, 0, 1, 128, 0, 0, 1, 0, 0, 1 },
    { "small address 2", DATA_RADDR, 0, 1, 128, 0, 0, 1, 0, 0, 1 },
    { "small address 3", DATA_ADDR, 0, 1, 128, 0, 0, 0, 0, 0, 1 },
    { "small address 4", DATA_RADDR, 0, 1, 128, 0, 0, 0, 0, 0, 1 },

    /* one line address tests */
    { "small address, partial cache line 1", DATA_ADDR, 0, 1, 128, 0, 0, 1, 0, 0, 1 },
    { "small address, partial cache line 2", DATA_RADDR, 0, 1, 128, 0, 0, 1, 0, 0, 1 },
    { "small address, partial cache line 3", DATA_ADDR, 0, 1, 128, 0, 0, 0, 0, 0, 1 },
    { "small address, partial cache line 4", DATA_RADDR, 0, 1, 128, 0, 0, 0, 0, 0, 1 },

    /* medium buffer data tests, using stride and offset */
    { "stride pattern 1", DATA_PATTERN, DP_5, 8, 512, 512, 0, 1, 0, 0, 1 },
    { "stride pattern 2", DATA_PATTERN, DP_A, 8, 512, 512, 0, 0, 0, 0, 1 },
    { "stride+offset pattern 1", DATA_PATTERN, DP_5, 8, 512, 512, 256, 1, 0, 0, 1 },
    { "stride+offset pattern 2", DATA_PATTERN, DP_A, 8, 512, 512, 256, 0, 0, 0, 1 },

    /* medium buffer address tests, using stride and offset */
    { "stride address 1", DATA_ADDR, 0, 8, 512, 512, 0, 1, 0, 0, 1 },
    { "stride address 2", DATA_RADDR, 0, 8, 512, 512, 0, 0, 0, 0, 1 },
    { "stride+offset address 1", DATA_ADDR, 0, 8, 512, 512, 256, 1, 0, 0, 1 },
    { "stride+offset address 2", DATA_RADDR, 0, 8, 512, 512, 256, 0, 0, 0, 1 },

    /* full buffer static tests */
    { "large static pattern 1", DATA_PATTERN, DP_5, 128, 1024, 0, 0, 1, 1, 0, 1 },
    { "large static pattern 2", DATA_PATTERN, DP_A, 128, 1024, 0, 0, 0, 1, 0, 1 },

    /* full buffer data tests */
    { "large pattern 1", DATA_PATTERN, DP_5, 128, 1024, 0, 0, 1, 0, 0, 1 },
    { "large pattern 2", DATA_PATTERN, DP_A, 128, 1024, 0, 0, 0, 0, 0, 1 },

    /* full buffer address tests */
    { "large address 1", DATA_ADDR, 0, 128, 1024, 0, 0, 1, 0, 0, 1 },
    { "large address 2", DATA_RADDR, 0, 128, 1024, 0, 0, 0, 0, 0, 1 },

    /* end record - only the testname == 0x0 is looked at */
    { (char *)0x0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
};

extern jmp_buf dangbuf;

static int ide_dang_mdma(int, int);

static int
setup_xfertest(char*, gdmada_t *, unsigned *, unsigned *, Dang_Xferinfo *);

static int
do_xfertest(
	int, int, volatile long long *,
	gdmada_t *, struct gr2_hw *, struct gr2_info *, Dang_Xferinfo *);

static int check_xferdata(char *, char *, Dang_Xferinfo *);

static int loc[3];

/* use local buffer rather than align_malloc() - broken in 6.X */
char dma_bigbuf[(IDE_PGSPERSHOT + 2) * DIAG_GFX_PAGESIZE];

int
dang_mdma(int argc, char** argv)
{
    int retval;

    msg_printf(INFO, "\ndang_mdma- dang chip master DMA module test\n");

    if (console_is_gfx())
    {
	msg_printf(INFO, "Test skipped. Can run only on an ASCII console\n");
	return(TEST_SKIPPED);
    }

    retval = test_adapter(argc, argv, IO4_ADAP_DANG, ide_dang_mdma);

    return (retval);
}


static int ide_dang_mdma(int slot, int adap)
{
    int window, retval, ivect, rbvect, jval;
    volatile long long * adptr;
    struct gr2_hw * gio_va;
    struct gr2_info * gio_info;
    Dang_Xferinfo *table;
    char * databuf;
    gdmada_t *dmahead;
    int bufsize, i;

    retval = 0;
    gio_va = NULL;
    loc[0] = slot; loc[1] = adap; loc[2] = -1;
    
    /*
     * get the window # of the current io board
     */
    window = io4_window(slot);
    adptr = (long long *) SWIN_BASE(window, adap);

    msg_printf(INFO, "dang_mdma- looking at slot %x, adap %x\n", slot, adap);

    /*
     * Get the current interrupt mask contents and disable all interrupt
     * sources - if cannot disable interrupts, exit
     */
    if (setjmp(dangbuf)) {
	msg_printf(ERR, "Exception Accessing DANG chip!\n");
	clear_nofault();
	return (1);
    }
    else {
	set_nofault(dangbuf);
	setup_err_intr(slot, adap);
	clear_nofault();
    }

    /*
     * initialize the dang chip and the large window virtual map to access it
     */
    if (!(gio_va = (struct gr2_hw *)ide_dang_init(slot, adap))) {
	msg_printf(VRB,"\tunable to initialize dang & buffers\n");
	return(TEST_SKIPPED);
    }
    else if (!ide_Gr2Probe((struct gr2_hw *)gio_va)) {
	msg_printf(VRB,"\tunable to locate Gr2 at address 0x%lx\n",
		(long) gio_va);
	return(TEST_SKIPPED);
    }

    bufsize = IDE_PGSPERSHOT * DIAG_GFX_PAGESIZE;

    databuf = (char *)(((__psunsigned_t)dma_bigbuf + DIAG_GFX_PAGESIZE) &
		~(DIAG_GFX_PAGESIZE - 1));
    dmahead = (gdmada_t *)((__psunsigned_t)databuf+bufsize);
    gio_info = (struct gr2_info *)((__psunsigned_t)dmahead+DIAG_GFX_PAGESIZE);

    msg_printf(DBG, "data buffer: 0x%llx\n", (long long) databuf);
    msg_printf(DBG, "dma header:  0x%llx\n", (long long) dmahead);

    /*
     * setup the GR2
     *
    ide_Gr2Config((long long *) adptr, gio_va, gio_info);

    /*
     * run all the test cases in the table
     */
    for (table = &test_array[0]; table->testname != 0; table++)
    {
	/*
	 * set up the transfer buffers, build the dma header, etc
	 */
	if (setup_xfertest (databuf, dmahead,
			(unsigned int *)&gio_va->shram[0],
			(unsigned int *)gio_va,
			table)
	    )
	{
	    msg_printf(ERR, "data setup problem: %s\n",table->testname);
	    return (TEST_SKIPPED);
	}

	/*
	 * to the actual transfer and verify the xfer completed, the amount
	 * of data sent was correct, etc.
	 */
	retval += do_xfertest(
	    slot, adap, adptr, dmahead, gio_va, gio_info, table);

	/*
	 * verify the contents of the data buffer against the original
	 * data
	 */
	retval += check_xferdata(databuf, (char *)&gio_va->shram[0], table);
    }

    clear_nofault();
    io_err_clear(slot, 1 << adap);
    ide_init_tlb();

    return (retval);
}

/*
 * set up the data in the xmit buffer (host or gr2 memory), clear the rcv
 * buffer, build the dma header
 */
static
int setup_xfertest (
    char * dbuf,		/* host buffer address */
    gdmada_t *dhead,		/* dma header address */
    unsigned *gaddr,		/* gr2 buffer address */
    unsigned *gbase,		/* gr2 buffer base address */
    Dang_Xferinfo *info)	/* table information on xfer type, etc */
{
    unsigned char *cp;
    unsigned int *ip;
    int npages, chunksiz, i;
    int cline;

    /* print subtest name and direction */
    msg_printf(VRB, "\t%s (%s)\n",
	        info->testname, (info->direct ? "H to G" : "G to H"));
    
    /*
     * figure the total memory usage and the number of pages needed
     * there is a limit on this routine - for now, coded to allow
     * a maximum of IDE_PGSPERSHOT (32) pages at one time.  This can
     * be bumped if need be, but should be enough for testing
     */
    chunksiz = info->lines * (info->bcount + info->stride);
    npages = (chunksiz / DIAG_GFX_PAGESIZE);
    if (chunksiz & (DIAG_GFX_PAGESIZE-1))
	npages++;

    /*
     * do some simple sanity checking - this is for test case debug
     */
    if (npages > IDE_PGSPERSHOT)
    {
	msg_printf(DBG, "data setup error: %s data size (%d pages) too large\n",
		    info->testname, npages);
	return(1);
    }
    if (info->offset > info->stride)
    {
	msg_printf(DBG, "data setup error: %s has offset > stride\n",
		    info->testname);
	return(1);
    }

    /*
     * zero the host data buffer
     */
    for(i = 0, cp = dbuf; i < chunksiz; i++)
	*(char *)cp++ = 0;
    
    /*
     * zero the gr2 buffer - only one location needed if static mode
     */
    if (info->saddr)
	*gaddr = 0;
    else
    {
	for(i = 0, ip = gaddr; i < (chunksiz/sizeof(unsigned));i++)
	    *ip++ = 0;
    }

    /*
     * find the starting address (set up if gtoh w/static address)
     */
    if (info->direct)	/* host to gr2 */
    {
	cp = dbuf + info->offset;
    }
    else		/* gr2 to host */
    {
	cp = gaddr + info->offset;

	/*
	 * do setup & exit if static address mode
	 */
	if (info->saddr)	/* static address mode */
	{
	    switch (info->datatype)
	    {
		case DATA_ADDR:
		    *(unsigned char *)cp = (unsigned char) cp & 0xFF;
		    break;
		
		case DATA_RADDR:
		    *(unsigned char *)cp = ~((unsigned char) cp & 0xFF);
		    break;
		
		case DATA_PATTERN:
		default:
		    *(unsigned char *)cp = info->fillpat;
	    }
	    goto MKHEAD;
	}
    }

    /*
     * given starting address, data-per-line, skip-per-line, and linecount,
     * fill the rest of the buffer
     */
    for (cline = 0; cline < info->lines; cline++)
    {
	for (i = 0; i < info->bcount; i++)
	{
	    switch (info->datatype)
	    {
		case DATA_ADDR:
		    *(unsigned char *)cp = (unsigned char)cp & 0xFF;
		    break;
		
		case DATA_RADDR:
		    *(unsigned char *)cp = ~((unsigned char) cp & 0xFF);
		    break;
		
		case DATA_PATTERN:
		default:
		    *(unsigned char *)cp = info->fillpat;
	    }
	}
	cp += (info->bcount + info->stride);
    }

MKHEAD:

    /*
     * set up the dma header now
     */
    ide_Gr2mkudmada(dbuf, dhead,
		    info->lines, info->bcount, info->stride, info->offset,
		    info->direct, info->saddr, info->sync, info->cmplint,
		    (void *)GIO_PHYSADDR(gaddr, gbase));

    /*
     * print out the header info - this is for debugging routines
     */
    print_gdmada_t(dhead);

    return (0);
}

/*
 * verify the data after the xfer
 */
static
int check_xferdata(
    char * dbuf,		/* host buffer address */
    char * gaddr,		/* gr2 buffer address */
    Dang_Xferinfo *info)	/* table information on xfer type, etc */
{
    unsigned char *sp, *dp;
    int cline, cbyte, retval;

    retval = 0;

    /*
     * given starting address, data-per-line, skip-per-line, linecount,
     * data direction (host to gio/gio to host), and if using the gio
     * static addressing mode, verify source vs destination data
     */
    if (info->direct)
    {
	sp = dbuf;
	dp = gaddr;
    }
    else
    {
	sp = gaddr;
	dp = dbuf;
    }

    if (info->saddr)
    {
	if (info->direct)
	    sp += info->offset;
	else
	    dp += info->offset;
    }
    else
    {
	sp += info->offset;
	dp += info->offset;
    }
    

    for (cline = 0; cline < info->lines; cline++)
    {
	for (cbyte = 0; cbyte < info->bcount; cbyte++)
	{
	    /*
	     * data error?
	     */
	    if (*sp != *dp)
	    {
		err_msg(DANG_BADDDATA,
		    loc, info->testname, (info->direct ? "H to G" : "G to H"),
		    cline, cbyte, (int) *sp, (int) *dp);
		retval++;
	    }
	}

	/*
	 * bump the source and destination addresses
	 * there is a little workaround here for static address mode
	 * both htog and gtoh
	 */
	if (info->saddr)
	{
	    if (info->direct)
		sp += (info->bcount + info->stride);
	    else
		dp += (info->bcount + info->stride);
	}
	else
	{
	    sp += (info->bcount + info->stride);
	    dp += (info->bcount + info->stride);
	}
    }

    return (retval);
}
/*
 * do the actual DMA xfer, and analyse any DMA failures
 */
static int
do_xfertest(
    int slot,
    int adap,
    volatile long long * adptr,
    gdmada_t *dmahead,
    struct gr2_hw * gio_va,
    struct gr2_info * gio_info,
    Dang_Xferinfo *info)	/* table information on xfer type, etc */
{
    int jval, retval, int_mask, readback, ivect, rbvect;
    long long longbuf, longbuf2;

    retval = 0;

    /*
     * do simple DMA xfer here
     */
    if((jval =setjmp(dangbuf))){

	/* save the current hardware state */
#if !defined(IP21) 
	io_err_log(slot, 1 << adap);
#endif
	dang_log_status(slot, adap);
	longbuf = load_double((long long*)&adptr[DANG_DMAM_PTE_ADDR]);

	set_SR(get_SR() & ~SR_IMASK);


	/* was this the correct interrupt vector? */
	rbvect = (unsigned) EV_GET_LOCAL(EV_HPIL);
	if (rbvect != ivect)
	{
	    err_msg(DANG_DMAVECT, loc, info->testname,
		(info->direct ? "H to G" : "G to H"), 
		       (int) rbvect, (int) ivect);
	    retval++;
	}

	/* Was this the correct dma interrupt */
	if (!(d_mdmastat & (long long) DANG_DMAM_STAT_CMP)){
	    err_msg(DANG_DMACOMP, loc, info->testname,
		(info->direct ? "H to G" : "G to H")); 
	    msg_printf(DBG,"Current PTE Register was 0x%llx\n",
		    (unsigned long long)longbuf);
	    retval++;
	}

	/* if failed, what is the dma state? */
	if (retval)
	{
#if !defined(IP21) 
	    io_err_show(slot, 1 << adap);
#endif
	    dang_show_status(slot, adap);
	}

   	/* clean up interrupt */
	clear_nofault();
	io_err_clear(slot, 1 << adap);
	clear_err_intr(slot, adap);
    }
    else{
	set_nofault(dangbuf);

	/* reset the GIO bus */
        store_double((long long *)&adptr[DANG_GIORESET],(long long)0);
        us_delay(10);
        store_double((long long *)&adptr[DANG_GIORESET],(long long)1);

	/*
	 * DANG interrupts will all use the same handler in the
	 * standalones, all the interrupt vectors point to the
	 * same location 
	 */
	ivect = DANG_INTR_LEVEL(1);

	/* set up interrupt vectors */
	setup_err_intr(slot, adap);

	/* enable dma interrupt(s) */
	int_mask =  DANG_ISTAT_DMAM_CMP | DANG_ISTAT_DCHIP |
		    DANG_ISTAT_DSYNC | DANG_ISTAT_GFXDLY |
		    DANG_IENA_SET; 
	store_double((long long*)&adptr[DANG_INTR_ENABLE],
                             (long long) int_mask);

	/* Enable interrupts on the R4k processor */
	set_SR(get_SR() | SRB_DEV | SR_IE);

	/* call dma go routine */
	ide_GR2DMAtrigger((long long *)adptr, gio_va, (void *)dmahead);

	/* delay for dma setup to go */
	us_delay(DELAY_5_SECONDS);

	/* disable interrupts again */
	set_SR(get_SR() & ~SR_IMASK);

	/*
	 * grab the current status of the DANG
	 */
#if !defined(IP21) 
	io_err_log(slot, 1 << adap);
#endif
	dang_log_status(slot, adap);
	longbuf = load_double((long long*)&adptr[DANG_DMAM_PTE_ADDR]);

	/* if got here, no interrupt */
	err_msg(DANG_BADDMA, loc, info->testname,
		(info->direct ? "H to G" : "G to H"));
       	retval++;

	/*
	 * DEBUG stuff
         */
        if (cpu_intr_pending() == 0)
        {
            msg_printf(DBG, "No ebus interrupt\n");
            readback = (unsigned) EV_GET_LOCAL(EV_HPIL);
            msg_printf(DBG,
                "Pending Interrupt Level was %x\n", readback);
        }
        else
        {
            msg_printf(DBG, "ebus interrupt, no CPU interrupt\n");
            readback = (unsigned) EV_GET_LOCAL(EV_HPIL);
            msg_printf(DBG,
                "Pending Interrupt Level was %x\n", readback);

            readback = (unsigned) get_SR();
            msg_printf(DBG,
                "Interrupt Status Register was %x\n", readback);
	}

	/* did dma xfer occur? */
	/* print dma status registers */
#if !defined(IP21) 
	io_err_show(slot, (1 << adap));
#endif
	dang_show_status(slot, adap);
	msg_printf(DBG,"Current PTE Register was 0x%llx\n",
	    (unsigned long long)longbuf);

	clear_nofault();
    }

    clear_nofault();
    io_err_clear(slot, 1 << adap);
}
