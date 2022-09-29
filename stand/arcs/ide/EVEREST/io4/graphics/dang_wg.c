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
 *	dang_wg.c - dang chip write gatherer module xfer/int tests	*
 *									*
 ************************************************************************/

#ident  "arcs/ide/EVEREST/io4/graphics/dang_wg.c $Revision: 1.9 $"

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
#define GIO_PHYSADDR(x,y) (((__uint32_t)x)-((__uint32_t)y)+0x1f000000)
#define DELAY_5_SECONDS		5000000

/*
 * these should match the current values in danglib - if the danglib values
 * change, these should change to match
 */
#define WG_LOINTR		32
#define WG_HIINTR		1024
#define WG_FULLINTR		0xfff

#define WG_MAXDEPTH		0x1000		  /* 4 k 32 bit words */
#define WG_ADMUX		(WG_MAXDEPTH>>1)  /* 2 k data+mux addr pairs */

/*
 * modified version of gfx kernal stuff
 */

#define SPLRETR spl3
#define CACHETYPE       0
#if defined(IP19)
#define FIFO(x,y) wg[(x)].pipeUnion.l = (y)
volatile struct _expPipeEntryRec *wg = (volatile struct _expPipeEntryRec *)EV_WGINPUT_BASE;
#elif defined(IP21)
#define EVEN_OFFSET     0
#define FLUSH_OFFSET    4
static volatile union _expPipeEntryRec *wg = (volatile union _expPipeEntryRec *)EV_WGINPUT_BASE;
#define FIFO(x,y)	{ wg[EVEN_OFFSET].i = (x); wg[EVEN_OFFSET].i = (y); }
#elif defined(IP25)
static volatile union _expPipeEntryRec *wg = (volatile union _expPipeEntryRec *)EV_WGINPUT_BASE;
#define FIFO(x,y)       { \
	volatile int foo;\
	wg[0].i = (x); wg[0].i = (y); }
#endif

#if defined(IP19)
#define SETWG(WINDOW,ADAPTER,PRIV,ADDRMODE) {\
        long long ll;\
        ll = 3;\
        EV_SET_LOCAL(EV_WGCNTRL,ll);\
        ll = (long long)LWIN_PFN(WINDOW,ADAPTER);\
        ll <<= 12;\
        ll |= (EV_GET_REG(EV_SPNUM)<<8)|((PRIV)<<15)|((ADDRMODE)<<16);\
        EV_SET_LOCAL(EV_WGDST,ll);\
}
#elif defined(IP21)
#define SETWG(WINDOW,ADAPTER,PRIV,ADDRMODE) {\
        long long ll;\
        ll = (long long)LWIN_PFN(WINDOW,ADAPTER);\
        ll <<= 12;\
        ll |= (((EV_GET_REG(EV_SPNUM) & EV_SPNUM_MASK))<<8)|((PRIV)<<15)|((ADDRMODE)<<16);\
        EV_SET_LOCAL(EV_WGDST,ll);\
}
#elif defined(IP25)
#define SETWG(WINDOW,ADAPTER,PRIV,ADDRMODE) {\
        long long ll;\
        ll = (long long)LWIN_PFN(WINDOW,ADAPTER);\
        ll <<= 12;\
        ll |= (((EV_GET_REG(EV_SPNUM) & EV_SPNUM_MASK))<<8)|((PRIV)<<15)|((ADDRMODE)<<16);\
        EV_SET_LOCAL(EV_GRDST,ll);\
}
#endif
#if defined(IP19)
#define FLUSHWG(WAIT,BDATA) {\
        volatile struct _expPipeEntryRec *wg = (volatile struct _expPipeEntryRec *)EV_WGINPUT_BASE;\
        wg[0xff].pipeUnion.l = 0;\
        while (WAIT && (*((volatile int *)(EV_WGCOUNT+4)) & 0x23f3f));\
}
#elif defined(IP21)
#define FLUSHWG(WAIT,BDATA) {\
        wg[FLUSH_OFFSET].i = 0;\
        if (WAIT) {\
                volatile unsigned int i;\
                i = gio_va->hq.mystery;\
                i = gio_va->hq.mystery;\
                us_delay(800);\
        }\
}
#elif defined(IP25)
#define FLUSHWG(WAIT,BDATA) {\
    volatile int foo;\
    foo = EV_GET_LOCAL(EV_RTC);\
    if (WAIT) {\
    foo = load_double_lo((long long *)&BDATA[DANG_WG_WDCNT]);\
    }\
}
#endif
#if defined(IP25)
#define WG_MAXCMD	0xf0		/* highest command value allowed */
#else
#define WG_MAXCMD	0xfe		/* highest command value allowed */
#endif
#define WG_MAXSTREAM	15		/* words to stream */
#define WG_MAXFSTREAM	16		/* (data+1 cnt) */

#define DRAINDANG(bdata) \
{       \
    int i=0;\
    volatile int fifocnt; \
    store_double((long long *)&bdata[DANG_WG_PAUSE], (long long)0); \
    do { \
	fifocnt = load_double_lo((long long *)&bdata[DANG_WG_WDCNT]); \
	if (fifocnt == 1) { \
	 store_double((long long *)&bdata[DANG_WG_POP_FIFO], (long long)0); \
	 fifocnt = load_double_lo((long long *)&bdata[DANG_WG_FIFO_RDADDR]); \
	 fifocnt = load_double_lo((long long *)&bdata[DANG_WG_FIFO_RDADDR]); \
	 store_double((long long *)&bdata[DANG_WG_STREAM_WDCNT],(long long)0); \
	 store_double((long long *)&bdata[DANG_WG_WDCNT],(long long)0); \
	} \
	us_delay(5); \
	if (++i > 100000) {\
	    break;\
	} \
    }while(fifocnt);\
}
#define WGPAUSE(bdata) \
{ \
    store_double((long long *)&bdata[DANG_WG_PAUSE], (long long)1); \
}

extern jmp_buf dangbuf;

extern char dma_bigbuf[];

static int ide_dang_wg(int, int);

/* fifo write-through tests */
static int do_fifowt(int slot, int adap, volatile long long * adptr,
		     struct gr2_hw * gio_va);
/* host wg function tests */
static int do_hostwg(int slot, int adap, volatile long long * adptr,
		     struct gr2_hw * gio_va);
/* wg interrupt generation tests */
static int do_wgintr(int slot, int adap, volatile long long * adptr,
		    struct gr2_hw *gio_va);

static __uint32_t test_patterns[] =
{ 0x55555555, 0xAAAAAAAA, 0xFFFFFFFF, 0x5a5a5a5a, 0xa5a5a5a5, 0 };

static int loc[3];

int
dang_wg(int argc, char** argv)
{
    int retval;

    msg_printf(INFO, "\ndang_wg - dang chip write gatherer data/interrupt test\n");

    if (console_is_gfx())
    {
	msg_printf(INFO, "Test skipped. Can run only on an ASCII console\n");
	return(TEST_SKIPPED);
    }

    retval = test_adapter(argc, argv, IO4_ADAP_DANG, ide_dang_wg);

    return (retval);
}


static int ide_dang_wg(int slot, int adap)
{
    int window, retval, ivect, rbvectjval;
    volatile long long * adptr;
    struct gr2_hw * gio_va;
    struct gr2_info * gio_info;
    int bufsize, i;

    retval = 0;
    gio_va = NULL;
    loc[0] = slot; loc[1] = adap; loc[2] = -1;
    
    /*
     * get the window # of the current io board
     */
    window = io4_window(slot);
    adptr = (long long *) SWIN_BASE(window, adap);

    msg_printf(INFO, "dang_wg - looking at slot %x, adap %x\n", slot, adap);

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

    /*
     * setup the GR2
     */
    gio_info = (struct gr2_info *)

    (((__psunsigned_t)dma_bigbuf + 16) & ~0xf);

    ide_Gr2Config((long long *) adptr, gio_va, gio_info);

    /*
     * try FIFO write-through mode - this is the simplest thing to test
     */
    retval = do_fifowt(slot, adap, adptr, gio_va);

    /*
     * try HOST WG mode
     */
    retval += do_hostwg(slot, adap, adptr, gio_va);

    /*
     * check WG HI/FULL/LOW interrupts
     */
    retval += do_wgintr(slot, adap, adptr, gio_va);

    io_err_clear(slot, 1 << adap);
    ide_init_tlb();

    return (retval);
}

/*
 * this test tests the FIFO ram using pio writethrough mode
 * assumes the WG is already set up by calling routine
 */
static int
do_fifowt(int slot, int adap, volatile long long *adptr, struct gr2_hw *gio_va)
{
    __uint32_t *ulptr, *tpat;
    __uint32_t readback;
    int i, retval = 0;

    msg_printf(VRB, "\twg fifo ram test using pio writethrough\n");

    /* make sure the fifo is empty */
    DRAINDANG(adptr);

    /* shorthand address */
    ulptr = (__uint32_t *)&gio_va->shram[0];
    msg_printf(DBG, "shram address is 0x%llx\n", (long long) ulptr);

    /* defines used by wg test code - send to gr2 shram */
    store_double((long long *)&adptr[DANG_WG_GIO_UPPER],
	(long long)0xf80);       /* Upper 15 bits of 0x1f000000 */

    /*************************************************************
     * run the bit patterns through the fifo 
     */
    msg_printf(VRB, "\t\tbit pattern test\n");
    tpat = &test_patterns[0];
    while (*tpat)
    {
	/*
	 * zero the destination data
	 */
	for (i = 0; i < WG_ADMUX; i++)
	    ulptr[i] = 0;
    
	/* make sure that the fifo is empty, here, too */
	DRAINDANG(adptr);
	readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);
	if (readback)
	{
	    msg_printf(ERR, "DANG wg fifo not empty - had %x words\n",
		    readback);
	    retval++;
	    goto FIFO_WTDONE ;
	}

	/*
	 * turn on write-through mode and enable the pause
	 */
	store_double((long long *)&adptr[DANG_PIO_WG_WRTHRU], (long long)1);
	WGPAUSE(adptr);

	/*
	 * fill the fifo with the pattern
	 */
	for (i = 0; i < WG_ADMUX; i++)
	    ulptr[i] = *tpat;

	/*
	 * check to make sure the fifo filled
	 */
	readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);
	if (readback != WG_MAXDEPTH)
	{
	    msg_printf(ERR,
		"DANG wg fifo: bad word count - was 0x%x, sb 0x%x\n",
		readback, WG_MAXDEPTH);
	    retval++;
	}

	/*
	 * disable pause so the fifo can drain
	 */
	DRAINDANG(adptr);
	readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);
	if (readback)
	{
	    msg_printf(ERR, "DANG wg fifo not empty - had %x words\n",
		    readback);
	    retval++;
	}

	/*
	 * turn off write-through mode
	 */
	store_double((long long *)&adptr[DANG_PIO_WG_WRTHRU], (long long)0);

	/*
	 * check the fifo data in the gr2 memory area if no errors yet
	 */
	for (i = 0; i < WG_ADMUX; i++)
	{
	    if (ulptr[i] != *tpat)
	    {
		msg_printf(ERR,
	"DANG fifo write through data error: addr 0x%x, was 0x%x sb 0x%x\n",
		    &ulptr[i], ulptr[i], *tpat);
		retval++;
		break;
	    }
	}

	/* exit if any error above */
	if (retval)
	    goto FIFO_WTDONE;

	tpat++;
    }

    /*************************************************************
     * Address-In-Address
     */

    msg_printf(VRB,"\t\taddress in address test\n");

    /*
     * zero the destination data
     */
    for (i = 0; i < WG_ADMUX; i++)
	ulptr[i] = 0;

    /* make sure that the fifo is empty */
    DRAINDANG(adptr);
    readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);
    if (readback)
    {
	msg_printf(ERR, "DANG wg fifo not empty - had %x words\n",
		readback);
	retval++;
	goto FIFO_WTDONE ;
    }

    /*
     * turn on write-through mode and enable the pause
     */
    store_double((long long *)&adptr[DANG_PIO_WG_WRTHRU], (long long)1);
    WGPAUSE(adptr);

    /*
     * fill the fifo with address data
     */
    for (i = 0; i < WG_ADMUX; i++)
	ulptr[i] = (__uint32_t)&ulptr[i];

    /*
     * check to make sure the fifo filled
     */
    readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);
    if (readback != WG_MAXDEPTH)
    {
	msg_printf(ERR,
	    "DANG wg fifo: bad word count - was 0x%x, sb 0x%x\n",
	    readback, WG_MAXDEPTH);
	retval++;
    }

    /*
     * disable pause so the fifo can drain
     */
    DRAINDANG(adptr);
    readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);
    if (readback)
    {
	msg_printf(ERR, "DANG wg fifo not empty - had %x words\n",
		readback);
	retval++;
    }

    /*
     * turn off write-through mode
     */
    store_double((long long *)&adptr[DANG_PIO_WG_WRTHRU], (long long)0);

    /*
     * check the fifo data in the gr2 memory area if no errors yet
     */
    for (i = 0; i < WG_ADMUX; i++)
    {
	if (ulptr[i] != (__uint32_t) &ulptr[i])
	{
	    msg_printf(ERR,
    "DANG fifo write through data error: addr 0x%x, was 0x%x sb 0x%x\n",
		&ulptr[i], ulptr[i], (__uint32_t)&ulptr[i]);
	    retval++;
	    break;
	}
    }

    /* exit if any error above */
    if (retval)
	goto FIFO_WTDONE;

    /*************************************************************
     * Inverse Address-In-Address
     */

    msg_printf(VRB,"\t\tinverse address in address test\n");

    /*
     * zero the destination data
     */
    for (i = 0; i < WG_ADMUX; i++)
	ulptr[i] = 0;

    /* make sure that the fifo is empty */
    DRAINDANG(adptr);
    readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);
    if (readback)
    {
	msg_printf(ERR, "DANG wg fifo not empty - had %x words\n",
		readback);
	retval++;
	goto FIFO_WTDONE ;
    }

    /*
     * turn on write-through mode and enable the pause
     */
    store_double((long long *)&adptr[DANG_PIO_WG_WRTHRU], (long long)1);
    WGPAUSE(adptr);

    /*
     * fill the fifo with address data
     */
    for (i = 0; i < WG_ADMUX; i++)
	ulptr[i] = (__uint32_t)(~((unsigned)&ulptr[i]));

    /*
     * check to make sure the fifo filled
     */
    readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);
    if (readback != WG_MAXDEPTH)
    {
	msg_printf(ERR,
	    "DANG wg fifo: bad word count - was 0x%x, sb 0x%x\n",
	    readback, WG_MAXDEPTH);
	retval++;
    }

    /*
     * disable pause so the fifo can drain
     */
    DRAINDANG(adptr);
    readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);
    if (readback)
    {
	msg_printf(ERR, "DANG wg fifo not empty - had %x words\n",
		readback);
	retval++;
    }

    /*
     * turn off write-through mode
     */
    store_double((long long *)&adptr[DANG_PIO_WG_WRTHRU], (long long)0);

    /*
     * check the fifo data in the gr2 memory area if no errors yet
     */
    for (i = 0; i < WG_ADMUX; i++)
    {
	if (ulptr[i] != (__uint32_t)(~((unsigned)&ulptr[i])))
	{
	    msg_printf(ERR,
    "DANG fifo write through data error: addr 0x%x, was 0x%x sb 0x%x\n",
		&ulptr[i], ulptr[i], (__uint32_t)(~((unsigned)&ulptr[i])));
	    retval++;
	    break;
	}
    }

FIFO_WTDONE:
    DRAINDANG(adptr);
    store_double((long long *)&adptr[DANG_PIO_WG_WRTHRU], (long long)0);
    return (retval);
}

/*
 * verify that the host-wg modes of operation are working
 * this concentrates on the DANG functionality, not the CPU
 * WG circuitry. Since the pio writearound test does ram testing
 * these tests just do address-in-address testing to verify that
 * offset, absolute address, etc are all OK
 */
static int
do_hostwg(int slot, int adap, volatile long long *adptr, struct gr2_hw *gio_va)
{
    __uint32_t *ulptr, *tpat;
    __uint32_t readback;
    int i, window, retval = 0;

    msg_printf(VRB, "\thost wg to dang fifo test\n");

    /*
     * get the window # of the current io board
     */
    window = io4_window(slot);

    /* make sure the fifo is empty */
    DRAINDANG(adptr);

    /* shorthand address */
    ulptr = (__uint32_t *)&gio_va->shram[0];

    /* defines used by wg test code - send to gr2 shram */
    store_double((long long *)&adptr[DANG_WG_GIO_UPPER],
	(long long)0xf80);       /* Upper 15 bits of 0x1f000000 */
    store_double((long long *)&adptr[DANG_WG_GIO_STREAM],
	(long long)0x1f000000);  /* GIO addr for streaming mode (32bits) */


    /*************************************************************
     * Address-In-Address - Relative Address mode
     */

    msg_printf(VRB,"\t\trelative address mode test\n");

    /*
     * zero the destination data
     */
    for (i = 0; i < WG_MAXDEPTH; i++)
	ulptr[i] = 0;


    /* make sure that the fifo is empty */
    DRAINDANG(adptr);

    readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);
    if (readback)
    {
	msg_printf(ERR, "DANG wg fifo not empty - had %x words\n",
		readback);
	retval++;
	goto HOST_WGDONE ;
    }

    /* set up the write gatherer */
    SETWG(window, adap, 1, 0);		/* 1, 0 is privileged, relative*/
    WGPAUSE(adptr);

    /*
     * fill the fifo with address data
     */
    for (i = 1; i <= WG_MAXCMD; i++) {
	FIFO(i,(__uint32_t)&ulptr[i]);
    }

    FLUSHWG(1,adptr);

    /*
     * check to make sure the fifo filled
     */
    readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);
    if (readback != (WG_MAXCMD<<1))
    {
	msg_printf(ERR,
	    "DANG wg fifo: bad word count - was 0x%x, sb 0x%x\n",
	    readback, (WG_MAXCMD<<1));
	retval++;
    }

    /*
     * disable pause so the fifo can drain
     */
    DRAINDANG(adptr);
    readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);
    if (readback)
    {
	msg_printf(ERR, "DANG wg fifo not empty - had %x words\n",
		readback);
	retval++;
    }

    /*
     * check the fifo data in the gr2 memory area if no errors yet
     */
    for (i = 1; i <= WG_MAXCMD; i++)
    {
	if (ulptr[i] != (__uint32_t) &ulptr[i])
	{
	    msg_printf(ERR,
    "DANG host wg data error: addr 0x%x, was 0x%x sb 0x%x\n",
		&ulptr[i], ulptr[i], &ulptr[i]);
	    msg_printf(DBG, "DANG host wg command #%d\n", i);
	    retval++;
	}
    }

#if !defined(IP21) && !defined(IP25)
    /*************************************************************
     * Inverse Address-In-Address - Absolute Address mode
     */

    msg_printf(VRB,"\t\tabsolute address mode test\n");

    /*
     * zero the destination data
     */
    for (i = 0; i < WG_MAXDEPTH; i++)
	ulptr[i] = 0;

    /* make sure that the fifo is empty */
    DRAINDANG(adptr);

    readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);
    if (readback)
    {
	msg_printf(ERR, "DANG wg fifo not empty - had %x words\n",
		readback);
	retval++;
	goto HOST_WGDONE ;
    }

    /* set up the write gatherer */
    SETWG(window, adap, 1, 1);		/* 1, 1 is privileged, absolute*/
    WGPAUSE(adptr);

    /*
     * fill the fifo with address data - write command word(address), then data
     */
    for (i = 0; i < WG_ADMUX; i++) {
	FIFO(0,(__uint32_t)GIO_PHYSADDR(&ulptr[i],ulptr));
	FIFO(0, (__uint32_t)~((__uint32_t)&ulptr[i]));
    }

    FLUSHWG(1,adptr);

    /*
     * check to make sure the fifo filled
     */
    readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);
    if (readback != WG_MAXDEPTH)
    {
	msg_printf(ERR,
	    "DANG wg fifo: bad word count - was 0x%x, sb 0x%x\n",
	    readback, WG_MAXDEPTH);
	retval++;
    }

    /*
     * disable pause so the fifo can drain
     */
    DRAINDANG(adptr);
    readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);
    if (readback)
    {
	msg_printf(ERR, "DANG wg fifo not empty - had %x words\n",
		readback);
	retval++;
    }

    /*
     * check the fifo data in the gr2 memory area if no errors yet
     */
    for (i = 0; i < WG_ADMUX; i++)
    {
	if (ulptr[i] != ~((__uint32_t)&ulptr[i]))
	{
	    msg_printf(ERR,
    "DANG host wg data error: addr 0x%x, was 0x%x sb 0x%x\n",
		&ulptr[i], ulptr[i], ~((__uint32_t)&ulptr[i]));
	    retval++;
	}
    }

    /*************************************************************
     * Streaming Always Address Mode 
     */

    msg_printf(VRB,"\t\tdang streaming always mode test\n");

    /*
     * zero the destination data
     * one location, since streaming is fixed-address
     */
    ulptr[0] = 0;

    /* make sure that the fifo is empty */
    FLUSHWG(1,adptr);
    DRAINDANG(adptr);

    /* set up the write gatherer */
    SETWG(window, adap, 1, 0);		/* 1, 0 is privileged, relative */
    WGPAUSE(adptr);
    store_double((long long *)&adptr[DANG_WG_STREAM_ALWAYS], (long long)1);

    /*
     * fill the fifo with data
     */
    for (i = 1; i <= WG_MAXSTREAM; i++)
	FIFO(0, 0x5a5a5a5a);

    FLUSHWG(1,adptr);

    /*
     * check to make sure the fifo filled
     */
    readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);
    if (readback != WG_MAXFSTREAM)
    {
	msg_printf(ERR,
	    "DANG wg fifo: bad word count - was 0x%x, sb 0x%x\n",
	    readback, WG_MAXFSTREAM);
	retval++;
    }

#ifdef NEVER
    /* DEBUG read the info from the fifo */
    do {
	readback=load_double_lo((long long *)&adptr[DANG_WG_FIFO_RDADDR]);
	printf("0x%x\n", readback);
	readback=load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);
    }while(readback);
#endif

    /*
     * disable pause so the fifo can drain
     */
    DRAINDANG(adptr);
    readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);
    if (readback)
    {
	msg_printf(ERR, "DANG wg fifo not empty - had %x words\n",
		readback);
	retval++;
    }

    /*
     * check the fifo data in the gr2 memory area if no errors yet
     * because of express shram latching only first data word of
     * stream, can't do ascending pattern here
     */
    if (ulptr[0] != 0x5a5a5a5a)
    {
	msg_printf(ERR,
	    "DANG host wg data error: addr 0x%x, was 0x%x sb 0x%x\n",
	    &ulptr[0], ulptr[0], 0x5a5a5a5a);
	retval++;
    }
    /* turn off stream always mode */
    store_double((long long *)&adptr[DANG_WG_STREAM_ALWAYS], (long long)0);

    /*************************************************************
     * Streaming Address Mode 
     */

    msg_printf(VRB,"\t\tstreaming address mode test\n");

    /*
     * zero the destination data
     * one location, since streaming is fixed-address
     */
    ulptr[0] = 0;

    /* make sure that the fifo is empty */
    FLUSHWG(1,adptr);
    DRAINDANG(adptr);

    readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);
    if (readback)
    {
	msg_printf(ERR, "DANG wg fifo not empty - had %x words\n",
		readback);
	retval++;
	goto HOST_WGDONE ;
    }

    /* set up the write gatherer */
    SETWG(window, adap, 1, 0);		/* 1, 0 is privileged, relative*/
    WGPAUSE(adptr);

    /* send count to dang, nonsense value in upper digits */
    FIFO(0, (0x80000000 | WG_MAXSTREAM));

    /*
     * fill the fifo with data
     */
    for (i = 1; i <= WG_MAXSTREAM; i++)
	FIFO(0, 0xa5a5a5a5);

    FLUSHWG(1,adptr);

    readback = load_double_lo((long long *)&adptr[DANG_WG_STREAM_WDCNT]);
    if (readback != 0)
    {
	msg_printf(ERR,
		"DANG streaming wordcount after wg flush - was 0x%x, sb 0x%x\n",
		readback, 0);
	retval++;
	    goto HOST_WGDONE ;
	}

	/*
	 * check to make sure the fifo filled
	 */
	readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);
	if (readback != WG_MAXFSTREAM)
	{
	    msg_printf(ERR,
		"DANG wg fifo: bad word count - was 0x%x, sb 0x%x\n",
		readback, WG_MAXFSTREAM);
	    retval++;
	}

    #ifdef NEVER
	/* DEBUG read the info from the fifo */
	do {
	    readback=load_double_lo((long long *)&adptr[DANG_WG_FIFO_RDADDR]);
	    printf("0x%x\n", readback);
	    readback=load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);
	}while(readback);
    #endif

	/*
	 * disable pause so the fifo can drain
	 */
	DRAINDANG(adptr);
	readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);
	if (readback)
	{
	msg_printf(ERR, "DANG wg fifo not empty - had %x words\n",
		readback);
	retval++;
    }

    /*
     * check the fifo data in the gr2 memory area if no errors yet
     * because of express shram latching only first data word of
     * stream, can't do ascending pattern here
     */
    if (ulptr[0] != 0xa5a5a5a5)
    {
	msg_printf(ERR,
	    "DANG host wg data error: addr 0x%x, was 0x%x sb 0x%x\n",
	    &ulptr[0], ulptr[0], 0xa5a5a5a5);
	retval++;
    }

#endif	/*not IP21*/

HOST_WGDONE:
    /* set write gatherer to the default values */
    SETWG(window, adap, 1, 0);		/* 1, 0 is privileged, relative*/
    FLUSHWG(1,adptr);
    DRAINDANG(adptr);
    return (retval);
}

/*
 * do the WG interrupt tests here
 */
static int
do_wgintr(
    int slot,
    int adap,
    volatile long long * adptr,
    struct gr2_hw * gio_va)
{
    int i, window, jval, retval, tval, int_mask, ivect, rbvect;
    __uint32_t readback;
    volatile __uint32_t *ulptr;

    msg_printf(VRB, "\twg interrupt test \n");

    retval = 0;

    /*
     * get the window # of the current io board
     */
    window = io4_window(slot);

    /* shorthand address */
    ulptr = (__uint32_t *)&gio_va->shram[0];

    /*
     * set the write gatherer up and drain the buffers
     */
    SETWG(window, adap, 1, 0);		/* 1, 0 is privileged, relative*/
    FLUSHWG(1,adptr);
    DRAINDANG(adptr);

    /*************************************************************
     * do WG HI interrupt here
     */
    msg_printf(VRB,"\t\tfifo hi interrupt test\n");
    tval=0;
    if((jval = setjmp(dangbuf))){

    msg_printf(DBG, "dangbuf address = 0x%lx\n",dangbuf);

	/* save the current hardware state */
#if !defined(IP21) && !defined(IP25)
	io_err_log(slot, 1 << adap);
#endif
	dang_log_status(slot, adap);
	readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);

	set_SR(get_SR() & ~SR_IMASK);

	/* was this the correct interrupt vector? */
	rbvect = (unsigned) EV_GET_LOCAL(EV_HPIL);
	if (rbvect != ivect)
	{
	    msg_printf(ERR,
		"wrong dang wg interrupt level - was 0x%x, sb 0x%x\n",
		rbvect, ivect);
	    tval++;
	}

	/* Was this the correct WG interrupt? */
	if (!(d_intstat & (long long) DANG_ISTAT_WG_FHI))
	{
	    msg_printf(ERR,
		"dang wg interrupt bit bad - was 0x%x, sb 0x%x\n",
		(int) d_intstat, (int) DANG_ISTAT_WG_FHI);
	    tval++;
	}

	/* was the word count for the interrupt correct? */
	/* can't be exact here - the wg sends the info in packets */
	if (readback < WG_HIINTR)
	{
	    msg_printf(ERR,
		"wrong wg word count - was 0x%x, sb 0x%x\n",
		readback, WG_HIINTR);
	    tval++;
	}


	/* if failed, what is the dang state? */
	if (tval)
	{
#if !defined(IP21) && !defined(IP25)
	    io_err_show(slot, (1 << adap));
#endif
	    dang_show_status(slot, adap);
	}

   	/* clean up interrupt */
	clear_nofault();
	io_err_clear(slot, 1 << adap);
	clear_err_intr(slot, adap);
    }
    else	/* first pass */
    {
	set_nofault(dangbuf);

	/*
	 * DANG interrupts will all use the same handler in the
	 * standalones, all the interrupt vectors point to the
	 * same location 
	 */
	ivect = EVINTR_LEVEL_DANG_ERROR;

	FLUSHWG(1,adptr);
	DRAINDANG(adptr);

	/* set up interrupt vectors */
	setup_err_intr(slot, adap);

	/* enable wg hi interrupt */
	int_mask =  DANG_ISTAT_WG_FHI | DANG_IENA_SET; 

	store_double((long long*)&adptr[DANG_INTR_ENABLE],
                             (long long) int_mask);

	/* Enable interrupts on the R4k processor */
	set_SR(get_SR() | SRB_DEV | SR_IE);

	/*
	 * set up CPU WG
	 */
	WGPAUSE(adptr);

	/* fill the buffer to cause the interrupt */
	for (i=0; i < ((WG_HIINTR+5)>>1); i++)
	    FIFO(1, (__uint32_t) &ulptr[i]);
	FLUSHWG(1,adptr);

	/*
	 * grab the current status of the DANG
	 */
	readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);

#if !defined(IP21) && !defined(IP25)
	io_err_log(slot, 1 << adap);
#endif
	dang_log_status(slot, adap);

	/* disable interrupts again */
	set_SR(get_SR() & ~SR_IMASK);

	/* if got here, no interrupt */
	msg_printf(ERR, "no wg fifo hi interrupt!\n");
	msg_printf(DBG, "DANG wg fifo had 0x%x words\n", readback);

       	tval++;

	/*
	 * DEBUG stuff
         */
        if (cpu_intr_pending() == 0)
        {
            msg_printf(DBG, "No ebus interrupt\n");
            readback = (__uint32_t) EV_GET_LOCAL(EV_HPIL);
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

	/* print dang status registers */
#if !defined(IP21) && !defined(IP25)
	io_err_show(slot, (1 << adap));
#endif
	dang_show_status(slot, adap);

	clear_nofault();
    }

    retval += tval;

    /*************************************************************
     * do WG FULL interrupt here
     */
    msg_printf(VRB,"\t\tfifo full interrupt test\n");
    tval = 0;
    if((jval = setjmp(dangbuf))){

	/* save the current hardware state */
#if !defined(IP21) && !defined(IP25)
	io_err_log(slot, 1 << adap);
#endif
	dang_log_status(slot, adap);
	readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);

	set_SR(get_SR() & ~SR_IMASK);

	/* was this the correct interrupt vector? */
	rbvect = (unsigned) EV_GET_LOCAL(EV_HPIL);
	if (rbvect != ivect)
	{
	    msg_printf(ERR,
		"wrong dang wg interrupt level - was 0x%x, sb 0x%x\n",
		rbvect, ivect);
	    tval++;
	}

	/* Was this the correct WG interrupt? */
	if (!(d_intstat & (long long) DANG_ISTAT_WG_FULL))
	{
	    msg_printf(ERR,
		"dang wg interrupt bit bad - was 0x%x, sb 0x%x\n",
		(int) d_intstat, (int) DANG_ISTAT_WG_FULL);
	    tval++;
	}

	/* was the word count for the interrupt correct? */
	/* can't be exact here - the wg sends the info in packets */
	if (readback < WG_FULLINTR)
	{
	    msg_printf(ERR,
		"wrong wg word count - was 0x%x, sb 0x%x\n",
		readback, WG_FULLINTR);
	    tval++;
	}


	/* if failed, what is the dang state? */
	if (tval)
	{
#if !defined(IP21) && !defined(IP25)
	    io_err_show(slot, (1 << adap));
#endif
	    dang_show_status(slot, adap);
	}

   	/* clean up interrupt */
	clear_nofault();
	io_err_clear(slot, 1 << adap);
	clear_err_intr(slot, adap);
    }
    else	/* first pass */
    {
	set_nofault(dangbuf);

	/*
	 * DANG interrupts will all use the same handler in the
	 * standalones, all the interrupt vectors point to the
	 * same location 
	 */
	ivect = EVINTR_LEVEL_DANG_ERROR;

	FLUSHWG(1,adptr);
	DRAINDANG(adptr);

	/* set up interrupt vectors */
	setup_err_intr(slot, adap);

	/* enable wg full interrupt */
	int_mask =  DANG_ISTAT_WG_FULL | DANG_IENA_SET; 

	store_double((long long*)&adptr[DANG_INTR_ENABLE],
                             (long long) int_mask);

	/* Enable interrupts on the R4k processor */
	set_SR(get_SR() | SRB_DEV | SR_IE);

	/*
	 * set up CPU WG
	 */
	WGPAUSE(adptr);

	/* fill the buffer to cause the interrupt */
	for (i=0; i < ((WG_FULLINTR+1)>>1); i++)
	    FIFO(1, (__uint32_t) &ulptr[i]);
	FLUSHWG(1,adptr);
	
	/*
	 * grab the current status of the DANG
	 */
	readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);

#if !defined(IP21) && !defined(IP25)
	io_err_log(slot, 1 << adap);
#endif
	dang_log_status(slot, adap);

	/* disable interrupts again */
	set_SR(get_SR() & ~SR_IMASK);

	/* if got here, no interrupt */
	msg_printf(ERR, "no wg fifo full interrupt!\n");
	msg_printf(DBG, "DANG wg fifo had 0x%x words\n", readback);

       	tval++;

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

	/* print dang status registers */
#if !defined(IP21) && !defined(IP25)
	io_err_show(slot, (1 << adap));
#endif
	dang_show_status(slot, adap);

	clear_nofault();
    }

    retval += tval;

    /*************************************************************
     * do WG LO interrupt here
     */
    msg_printf(VRB,"\t\tfifo lo interrupt test\n");
    tval = 0;
    if((jval = setjmp(dangbuf))){

	/* save the current hardware state */
#if !defined(IP21) && !defined(IP25)
	io_err_log(slot, 1 << adap);
#endif
	dang_log_status(slot, adap);
	readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);

	set_SR(get_SR() & ~SR_IMASK);

	/* was this the correct interrupt vector? */
	rbvect = (unsigned) EV_GET_LOCAL(EV_HPIL);
	if (rbvect != ivect)
	{
	    msg_printf(ERR,
		"wrong dang wg interrupt level - was 0x%x, sb 0x%x\n",
		rbvect, ivect);
	    tval++;
	}

	/* Was this the correct WG interrupt? */
	if (!(d_intstat & (long long) DANG_ISTAT_WG_FLOW))
	{
	    msg_printf(ERR,
		"dang wg interrupt bit bad - was 0x%x, sb 0x%x\n",
		(int) d_intstat, (int) DANG_ISTAT_WG_FLOW);
	    tval++;
	}

	/* was the word count for the interrupt correct? */
	/* can't be exact here - the wg sends the info in packets */
	if (readback > WG_LOINTR)
	{
	    msg_printf(ERR,
		"wrong wg word count - was 0x%x, sb 0x%x\n",
		readback, WG_LOINTR);
	    tval++;
	}


	/* if failed, what is the dang state? */
	if (tval)
	{
#if !defined(IP21) && !defined(IP25)
	    io_err_show(slot, (1 << adap));
#endif
	    dang_show_status(slot, adap);
	}

   	/* clean up interrupt */
	clear_nofault();
	io_err_clear(slot, 1 << adap);
	clear_err_intr(slot, adap);
    }
    else	/* first pass */
    {
	set_nofault(dangbuf);

	/*
	 * DANG interrupts will all use the same handler in the
	 * standalones, all the interrupt vectors point to the
	 * same location 
	 */
	ivect = EVINTR_LEVEL_DANG_ERROR;

	FLUSHWG(1,adptr);
	DRAINDANG(adptr);

	/* set up interrupt vectors */
	setup_err_intr(slot, adap);

	/* enable wg lo interrupt */
	int_mask =  DANG_ISTAT_WG_FLOW | DANG_IENA_SET; 

	store_double((long long*)&adptr[DANG_INTR_ENABLE],
                             (long long) int_mask);

	/*
	 * set up CPU WG
	 */
	WGPAUSE(adptr);

	/* fill the buffer above the hi line */
	for (i=0; i < ((WG_HIINTR+5)>>1); i++)
	    FIFO(1, (__uint32_t) &ulptr[i]);
	FLUSHWG(1,adptr);

	/* drain fifo to clear the fhi and cause flow interrupt */
	DRAINDANG(adptr);
	do {
	    readback=load_double_lo((long long *)&adptr[DANG_INTR_STATUS]);
	}while(readback & DANG_ISTAT_WG_FHI);

	/* Enable interrupts on the R4k processor now fhi clear */
	set_SR(get_SR() | SRB_DEV | SR_IE);

	us_delay(10000);

	/*
	 * grab the current status of the DANG
	 */
	readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);

#if !defined(IP21) && !defined(IP25)
	io_err_log(slot, 1 << adap);
#endif
	dang_log_status(slot, adap);

	/* disable interrupts again */
	set_SR(get_SR() & ~SR_IMASK);

	/* if got here, no interrupt */
	msg_printf(ERR, "no wg fifo lo interrupt!\n");
	msg_printf(DBG, "DANG wg fifo had 0x%x words\n", readback);

       	tval++;

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

	/* print dang status registers */
#if !defined(IP21) && !defined(IP25)
	io_err_show(slot, (1 << adap));
#endif
	dang_show_status(slot, adap);

	clear_nofault();
    }

    retval += tval;

    /*************************************************************
     * do WG privilege interrupt here
     */
    msg_printf(VRB,"\t\twg privilege interrupt test\n");
    tval = 0;
    if((jval = setjmp(dangbuf))){

	/* save the current hardware state */
#if !defined(IP21) && !defined(IP25)
	io_err_log(slot, 1 << adap);
#endif
	dang_log_status(slot, adap);

	set_SR(get_SR() & ~SR_IMASK);

	/* was this the correct interrupt vector? */
	rbvect = (unsigned) EV_GET_LOCAL(EV_HPIL);
	if (rbvect != ivect)
	{
	    msg_printf(ERR,
		"wrong dang wg interrupt level - was 0x%x, sb 0x%x\n",
		rbvect, ivect);
	    tval++;
	}

	/* Was this the correct WG interrupt? */
	if (!(d_intstat & (long long) DANG_ISTAT_WG_PRIV))
	{
	    msg_printf(ERR,
		"dang wg interrupt bit bad - was 0x%x, sb 0x%x\n",
		(int) d_intstat, (int) DANG_ISTAT_WG_PRIV);
	    tval++;
	}

	FLUSHWG(1,adptr);
	DRAINDANG(adptr);
	readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);

	/* was the word count for the interrupt correct? */
	/* should be NO words in write gatherer fifo */
	if (readback)
	{
	    msg_printf(ERR,
		"wrong wg word count - was 0x%x, sb 0x%x\n",
		readback, 0);
	    tval++;
	}


	/* if failed, what is the dang state? */
	if (tval)
	{
#if !defined(IP21) && !defined(IP25)
	    io_err_show(slot, (1 << adap));
#endif
	    dang_show_status(slot, adap);
	}

   	/* clean up interrupt */
	clear_nofault();
	io_err_clear(slot, 1 << adap);
	clear_err_intr(slot, adap);
    }
    else	/* first pass */
    {
	set_nofault(dangbuf);

	/*
	 * DANG interrupts will all use the same handler in the
	 * standalones, all the interrupt vectors point to the
	 * same location 
	 */
	ivect = EVINTR_LEVEL_DANG_ERROR;

	/*
	 * set up CPU WG
	 */
	SETWG(window, adap, 0, 0);	/* 0, 0 is unprivileged, relative*/

	FLUSHWG(1,adptr);
	DRAINDANG(adptr);

	/* set up interrupt vectors */
	setup_err_intr(slot, adap);

	/* enable wg privileged interrupt */
	int_mask =  DANG_ISTAT_WG_PRIV | DANG_IENA_SET; 

	store_double((long long*)&adptr[DANG_INTR_ENABLE],
                             (long long) int_mask);

	/* Enable interrupts on the R4k processor */
	set_SR(get_SR() | SRB_DEV | SR_IE);

	/* fill the buffer to cause the interrupt */
	for (i=1; i <= WG_MAXCMD; i++)
	    FIFO(i, (__uint32_t) &ulptr[i]);
	FLUSHWG(1,adptr);
	
	/*
	 * grab the current status of the DANG
	 */
	readback = load_double_lo((long long *)&adptr[DANG_WG_WDCNT]);

#if !defined(IP21) && !defined(IP25)
	io_err_log(slot, 1 << adap);
#endif
	dang_log_status(slot, adap);

	/* disable interrupts again */
	set_SR(get_SR() & ~SR_IMASK);

	/* if got here, no interrupt */
	msg_printf(ERR, "no wg privileged interrupt!\n");
	msg_printf(DBG, "DANG wg fifo had 0x%x words\n", readback);

       	tval++;

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

	/* print dang status registers */
#if !defined(IP21) && !defined(IP25)
	io_err_show(slot, (1 << adap));
#endif
	dang_show_status(slot, adap);

	clear_nofault();
    }

    retval += tval;

WG_INTRDONE:
    /* set write gatherer to the default values */
    SETWG(window, adap, 1, 0);		/* 1, 0 is privileged, relative*/
    FLUSHWG(1,adptr);
    DRAINDANG(adptr);
    clear_nofault();
    io_err_clear(slot, 1 << adap);
    return (retval);
}
