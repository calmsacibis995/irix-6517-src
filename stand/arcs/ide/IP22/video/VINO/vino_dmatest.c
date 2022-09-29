#ident	"IP22diags/video/vino_dmatest.c:  $Revision: 1.7 $"

/*
 * DMA tests for VINO
 * 
 * CAVETS - checks for interrupts only by polling vino intr_status
 * register NOT intr's from the system. doesn't check to see that
 * dmaBuf is written correctly or at all
 */

#include "sys/types.h"
#include "sys/immu.h"
#include "uif.h"
#include "bstring.h"

#include "./vino_diags.h"
#include "vino.h"
#include "libsk.h"
#include "libsc.h"

/*
 * MAX_PAGES is the number of pages for one PAL frame (768x576)
 * of 4:2: YUV (4 bytes/pixel). It is also the number of descriptors
 * allocated.
 */
#define MAX_PAGES 432
/* ONE_PAGE is a 4Kbyte page of memory */
#define ONE_PAGE 0x1000

/* size of a quad-word (16 bytes) */
#define ONE_QWRD 0x10

extern	int	vino_probe();

extern	uchar	board_initialized;
extern	uchar	mc_initialized;
extern	uchar	CDMC_initialized;
extern	uchar	DMSD_initialized;
extern	uchar	DMSD_no_sources;

extern	uchar	vino_pal_input;
extern	uchar	vino_ntsc_input;

extern	int		num_boards;	    /* Number of Vino boards */
extern	VinoBoard	vino_boards[];
extern	VinoRegs	regs_base;
extern	caddr_t		phys_base;

extern	int	vino_7191_init(void);

ulong	*descTbl;	/*
			 * This is a quad-word aligned array of
			 * descriptors, pointers to 4K aligned
			 * pages in memory. The program attempts
			 * to allocate MAX_PAGES nuber of descriptors
			 * but stops as soon as memory runs out.
			 */

ulong	descTblTop;	/* 
			 * this is the number of pages or the index
			 * into the jump to desc[0]
			 */

/*
 * initializeDMABuffers
 * 
 * allocate as many 4Kbyte aligned pages as possible. NOTE
 * ide has a static 512 Kbyte arena from which to allocate
 * all user memory. Using align_malloc, the most 4K pages
 * one can expect is 64 as align_malloc allocates twice as
 * much space as you need, and then indexes into that space
 * according to the specified allignment.
 */
static void
initializeDMABuffers(void)

{
    long    loop, loop2;
    ulong   *desc;

    descTblTop = 0L;

    descTbl = (ulong *) align_malloc(MAX_PAGES*sizeof(long), ONE_QWRD);
    msg_printf(DBG, "descTbl 0x%x\n", descTbl);

    for (loop=0L; loop<MAX_PAGES; loop++) {
	desc = (ulong *) align_malloc (ONE_PAGE, ONE_PAGE);
	descTbl[loop] = (ulong) K1_TO_PHYS (desc);
	if (descTbl[loop] == NULL) {
	    msg_printf(DBG, "NOTE : only able to allocate %d pages\n", loop);
	    break;
	}
	else {
	    /*
	     * zero out desc buffer
	     */
	    bzero(desc, ONE_PAGE);
	}
    }

    descTblTop = loop;
    msg_printf(DBG, "descTbl 0x%x descTblTop %d\n", descTbl, descTblTop);
}


/*
 * Free allocated memory. Becareful not to free jump descriptors
 * as they may have already been freed. Most functions assign NULL
 * to the jump descriptors at the end of the particular test though.
 * 
 * 06.16.93 - added ability to free buffer from some point to the
 * end. This is used mainly by vino_dma5(). 
 */
static void
freeDMABuffers(long from)

{
    long    loop;
    ulong   desc;

    if (descTbl) {
	for (loop=from; loop<descTblTop; loop++)
	    if (descTbl[loop] != NULL)
		if (descTbl[loop] & (JUMP_BIT))
		    descTbl[loop] = NULL;
		else
		    align_free ((void *)PHYS_TO_K1(descTbl[loop]&CLEAR_STP_JMP));
	
	align_free ((void *)descTbl);
	descTbl = NULL;
	descTblTop = 0L;
    }
}

/*
 * vinoLoadDescTbl - loads descriptor table with enough
 * descriptors to DMA one full frame of video in the PAL
 * or NTSC format.
 * 
 * 05.26.93 - modified to zero out dmaBuf
 *	    - added clean up at end of tests
 * 
 * NOTE - descTbl must be initialized proir to
 * this function.
 * 
 * 06.02.93 since ide only malloc's 512K, the
 * dma buffers must all be circular. so, set
 * a jump bit at the end of NTSC and PAL back
 * to descTbl[0]
 * 
 * 06.11.93 - jump descriptors do not work. may want to
 * think about repeating available descriptors until the
 * limit MAX_PAGES is satisfied.
 * 
 * 06.14.93 - added a fix for jumps. involves not letting
 * n4decs do any of the work.
 */
static void
vinoLoadDescTbl(uchar format)

{
    long    loop;
    long    oldTop;
    
    if (format == TEST_CONST) {  /* load 4 desc for test */
	descTbl[0] = (descTbl[0]&CLEAR_STP_JMP) | (STOP_BIT);
    }

    else {
	/*
	 * this is a work around for a hardware bug. to jump
	 * to a decsriptor with a smaller address, the next_
	 * descriptor register needs to be by-passed. this is
	 * done by replacing each fourth descriptor with a
	 * jump descriptor to the next sequential descriptor.
	 * the final descriptor can then jump backwards to the
	 * initial descriptor.
	 */
	for (loop=3; loop<descTblTop; loop+=4) {
	    descTbl[descTblTop] = descTbl[loop];    /* move unused desc to the end */
	    descTblTop++;			    /* and remember to increment descTblTop */
	    descTbl[loop] = (JUMP_BIT) | (K1_TO_PHYS(&(descTbl[loop+1])));
	}
	
	oldTop = descTblTop;
	descTblTop = descTblTop - (descTblTop%4);   /* quad word align jump to descTbl[0] */
	descTbl[oldTop] = descTbl[descTblTop];
	descTbl[descTblTop] = (JUMP_BIT) | (K1_TO_PHYS(&(descTbl[0])));
	
	/* free unused descriptors */
	for (loop=descTblTop+1; loop<=oldTop; loop++)
	    if (descTbl[loop] != NULL)
		if (descTbl[loop]&(JUMP_BIT))  /* don't free jump descriptors */
		    descTbl[loop] = NULL;
		else	/* it's a real descriptor, so free it */
		    align_free ((void *)PHYS_TO_K1(descTbl[loop]&CLEAR_STP_JMP));
    }
}

static void
vinoLoadDescTblInterleave(void)

{
    ulong   loop;
    ulong   numRealDesc;
    
    numRealDesc = descTblTop;
    for (loop=descTblTop; loop<MAX_PAGES; loop++)
	descTbl[loop] = descTbl[loop-numRealDesc];
    descTbl[loop] = STOP_BIT;
    
    descTblTop = loop;
    msg_printf(DBG, "descTblTop is %d\n", descTblTop);
}

int
dumpDesc(void)

{
    ulong   loop;
    
    msg_printf(INFO, "descTbl[%d]\n", descTblTop);
    for (loop=0L; loop<descTblTop; loop++)
	msg_printf(INFO, "%d\t0x%x\t%x\n", loop, &(descTbl[loop]), descTbl[loop]);
}

/*
 * vinoDescStop - set stop bit of descriptor pageNum
 */
static void
vinoDescStop(ulong *theDescTbl, ulong pageNum)

{
    theDescTbl[pageNum] =
	(theDescTbl[pageNum]&CLEAR_STP_JMP) | (STOP_BIT);
}

/*
 * vinoDescJump - make descriptor at pageNum jump to
 * descriptor jumpTo
 */
static void
vinoDescJump(ulong *theDescTbl, ulong pageNum, ulong jumpTo)

{
    theDescTbl[descTblTop] = theDescTbl[pageNum]&CLEAR_STP_JMP;
    descTblTop++;
    theDescTbl[descTblTop] =
	    ((K1_TO_PHYS(&(theDescTbl[0]))&CLEAR_STP_JMP) | (JUMP_BIT));
    theDescTbl[pageNum] =
	    ((K1_TO_PHYS(&(theDescTbl[jumpTo]))&CLEAR_STP_JMP) | (JUMP_BIT));
}

/*
 * parseCmdLine - self explanitory
 * 
 * 05.26.93 dmaToActivate changed to OR in changes (allows
 *	    both channels to be set up and enables all
 *	    interrupts on the selected channel
 * 
 * 06.11.93 line_size is the number of bytes - 8
 * 
 * 06.22.93 - this suite of tests expects a PAL signal on the
 * composite input and an NTSC signal on the S Video input
 *
 * 07.20.93 - got rid of 06.22.93 requirement
 */
static int
parseCmdLine(int argc, char **argv, VinoChannel **channel,
	    ulong *dmaToActivate, uchar *format, ulong *lengthOfLine
	    , uchar *dmaToIntr)

{
    int	    errcount = 0;
    
    *dmaToActivate = 0L;

    /*
     * parse command line to pick up which channel (A || B) and
     * which format (NTSC || PAL)
     */
    argc--; argv++;
    if (argc < 1)
	errcount++;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
	switch (argv[0][1]) {
	    case 'a':
	    case 'A':
		*channel = &(regs_base->a);
		/* enable dma, allow all interrupts, sync enable */
		*dmaToActivate |= (1<<7) | (7<<1) | (1<<9);
		*dmaToIntr = 0x0;
		break;
	    case 'b':
	    case 'B':
		*channel = &(regs_base->b);
		/* enable dma, allow all interrupts, sync enable */
		*dmaToActivate |= (1<<19) | (7<<4) | (1<<21);
		*dmaToIntr = 0x3;
		break;
	    case 'n':	    /* set bit in frame rate register for NTSC */
	    case 'N':
		*format = 0<<0;
		*lengthOfLine = (640*4)-8;	/* 640 pixels * 4bytes/pixel - 8 */
		vino_7191_inputs(vino_ntsc_input);
		break;
	    case 'p':	    /* set bit in frame rate register for PAL */
	    case 'P':
		*format = 1<<0;
		*lengthOfLine = (768*4)-8;	/* 768 pixels * 4bytes/pixel - 8 */
		vino_7191_inputs(vino_pal_input);
		break;
	    default:
		errcount++;
		break;
	}
	argc--; argv++;
    }
    if (argc || errcount){
	msg_printf(SUM,"Usage: vino_dmaX [-a | -A] [-b | -B] [-n | -N] || [-p | -P]\n");
	errcount=-1;
    }

    return errcount;
}

/*
 * created 05.26.93
 * 
 * Set up DMA - since all dma's in these test need to be set up
 * in a common way, it makes sense to put the code all in one
 * place to facilitate code maintanance.
 * 
 * 05.27.93 added correct version of framerate control. frame_rate
 * is a mask, NOT a value
 * 
 * 06.07.93 - added desc_table_ptr initialization for interleaving.
 * this shouldn't effect non-interleaved mode
 * 
 * 06.07.93 - NOTE format is the bit in the frame-rate register
 * 
 * 06.15.93 - Changed msg_printf from ERR to INFO
 * 
 * 06.17.93 - moved load of page_index to before load of n4desc
 * because the load of page_index was causing the descriptors to
 * shift up one place.
 */

static void
setUpDMA(   VinoChannel *channel, 
	    ulong length, 
	    ulong clip_s, 
	    ulong clip_e, 
	    uchar format, 
	    ulong threshold)

{
    VINO_REG_WRITE(regs_base, control, 0x0);
    VINO_REG_WRITE(channel, page_index, 0x0);
    VINO_REG_WRITE(regs_base, a.next_4_desc, (ulong) K1_TO_PHYS (descTbl));
    VINO_REG_WRITE(regs_base, b.next_4_desc, (ulong) K1_TO_PHYS (descTbl));
    
    /*
     * sometimes the interrupt register starts with
     * an interrupt set. make sure this isn't around
     * before you waste lots of time debugging this
     * problem. ;-)
     */
    VINO_REG_WRITE(regs_base, intr_status, 0x0);
    if (0x0 != VINO_REG_READ(regs_base, intr_status)) {
	msg_printf(INFO, "WARNING : intr_status register (0x10) set (0x%x). DMA will not work.\n",
		    VINO_REG_READ(regs_base, intr_status));
    }
    
    VINO_REG_WRITE(channel, desc_table_ptr, (ulong) K1_TO_PHYS (descTbl));
    VINO_REG_WRITE(channel, line_size, length);

    /*
     * These registers must be initialize because they don't reset
     */
    VINO_REG_WRITE(channel, alpha, 0x0);
    VINO_REG_WRITE(channel, clip_start, clip_s);
    VINO_REG_WRITE(channel, clip_end, clip_e);

    VINO_REG_WRITE(channel, frame_rate, (FRAMERATE_FULL<<1) | format);
    VINO_REG_WRITE(channel, fifo_threshold, threshold);
}

/*
 * Clean up routines go here
 * 
 * created 05.26.93
 */
static void
doCleanUp(void)

{
    /*
     * Clean up by disabling DMA and I2C
     */
    VINO_REG_WRITE(regs_base, control, 0x0);
    VINO_REG_WRITE(regs_base, i2c_control, 0x0);
}


/*
 * Search buffer for a non-zero entry. Since dmaBuf
 * gets zeroed out before use, a non-zero entry, in
 * theory, means that data was transfered from some-
 * where to dmaBuf
 * 
 * 07.14.93 - need to be checking the contents of buf
 * not buf itself to check for a non-zero value
 * 
 * 07.20.93 - Philips chip floats when no video source
 * is connected giving the false impression that data
 * is coming through the chip. added a check to see
 * if a source is connected. if not, the test automatically
 * fails.
 */
static int
dmaOccuredCorrectly(ulong pageNum)

{
    ulong   length, *buf;
    ulong   loop;
    int	    errcount = 0;
    int	    statusReg = 0;
        
    msg_printf(DBG, "in dmaOccuredCorrectly\n");
    if (DMSD_no_sources) {
	msg_printf(ERR, "dmaOccuredCorrectly : no video source\n");
	return 0;   /* can't test what isn't there */
    }

    for (loop=pageNum; loop<descTblTop; loop++) {
	length = ONE_PAGE/sizeof(ulong);
	if ((descTbl[loop] == NULL) | (descTbl[loop]&STOP_BIT) |
		(descTbl[loop]&JUMP_BIT)) {
	    msg_printf(DBG, "dmaOccuredCorrectly : skipping descTbl[%d] = 0x%x\n",
			    loop, descTbl[loop]);
	    continue;	/* this is not a valid address to check */
	}

	buf = (ulong *)PHYS_TO_K1(descTbl[loop]);
	msg_printf(DBG, "dmaOccuredCorrectly : checking addr (%x)\n", buf);
	while (length-- && !(*buf))
	    /* keep looking */
	    buf++;

	if (length > 0) {
	    msg_printf(DBG, "found a non-zero entry in page %d\n", loop);
	    return 1;	/* we found an entry that changed */
	}
    }
    
    return 0;	/* no entries changed */
}

/*
 * Initialize MC
 * created 05.25.93
 * 
 * set GIO64_ARB register of MC s.t. D4 (EISA_SIZE is 64 bits) and
 * D9 (EISA_MST is a master) are set to logical 1
 * MC is addr 0x1fa00000
 * GIO64_ARB is addr 0x84 + MC
 * 
 * check that D0 of LOCK_MEMORY (0x1fa0010c) is set
 * check that D0 of EISA_LOCK   (0x1fa00114) is set
 */
int
vino_initmc(void)

{
    ulong *gio64_arb = (ulong *)PHYS_TO_K1(MC_BASE | GIO64_ARB_ADDR);
    ulong *lock_memory = (ulong *)PHYS_TO_K1(LOCK_MEMORY);
    ulong *eisa_lock = (ulong *)PHYS_TO_K1(EISA_LOCK);
    ulong *cpuctrl0 = (ulong *)PHYS_TO_K1(MC_BASE | MC_CPUCTRL0);
    ulong *sysid = (ulong *)PHYS_TO_K1(MC_BASE | MC_SYSID);
    
    msg_printf(DBG, "gio64_arb is 0x%x\n", *gio64_arb);
    *gio64_arb |= 1<<4;
    *gio64_arb |= 1<<9;
    msg_printf(DBG, "gio64_arb is 0x%x\n", *gio64_arb);

    msg_printf(DBG, "CPUCTRL0  is 0x%x\n", *cpuctrl0);
    
    msg_printf(DBG, "MC_SYSID is 0x%x\n", *sysid);
    
    if (!(*lock_memory))
	*lock_memory = 0x1;

    if (!(*eisa_lock))
	*eisa_lock = 0x1;
    
    okydoky ();
    mc_initialized = TRUE;
    return 0;
}


/*
 * DMA Test 1
 * 
 * first four descriptors read in correctly
 * next_desc incremented correctly
 * stop bit generates an interrupt
 * 
 * 06.14.93 - This test will normally print an error
 * message saying that the interrupt status register
 * is set and dma will not work. Since this is what
 * we want to happen, don't worry. be happy.
 */
int
vino_dma1(int argc, char** argv)

{
    int		errcount = 0;
    int		timeout;
    VinoChannel	*channel;
    ulong	dmaToActivate;
    uchar	format;
    ulong	lengthOfLine;
    uchar	dmaToIntr;
    ulong	next_desc;
    
    if (DMSD_initialized == FALSE)
	vino_7191_init();

    errcount = parseCmdLine(argc, argv, &channel, &dmaToActivate,
			    &format, &lengthOfLine, &dmaToIntr);
    if (errcount != 0) {
	msg_printf(DBG, "vino_dma1 - parse error\n");
	sum_error ("VINO DMA 1");
	return(errcount);
    }
    
    if (!descTbl)
	initializeDMABuffers();

    /*
     * Load descriptor table
     * TEST_CONST sets up five descriptors with
     * first descriptor being a stop descriptor
     */
    vinoLoadDescTbl(TEST_CONST);

    if (format == PAL_CONST)
	setUpDMA(channel, lengthOfLine, START_CLIP_0, END_CLIP_PAL, 
		format, STAND_THRESHOLD);
    else    /* we have NTSC */
	setUpDMA(channel, lengthOfLine, START_CLIP_0, END_CLIP_NTSC, 
		format, STAND_THRESHOLD);

    /*
     * Now see if everything turned out ok
     * start with descriptors
     */

    /*
     * shouldn't have to clear stop bit of descTbl[0] since
     * it should appear in desc_0
     */
    if (VINO_REG_READ(channel, desc_0) != descTbl[0]) {
	msg_printf(ERR, "vino_dma1 desc_0 (%x) did not read in ok (%x)\n", 
		VINO_REG_READ(channel, desc_0), descTbl[0]);
	errcount++;
	return (errcount);
    }
    if (VINO_REG_READ(channel, desc_1) != descTbl[1]) {
	msg_printf(ERR, "vino_dma1 desc_1 did not read in ok\n");
	errcount++;
    }
    if (VINO_REG_READ(channel, desc_2) != descTbl[2]) {
	msg_printf(ERR, "vino_dma1 desc_2 did not read in ok\n");
	errcount++;
    }
    if (VINO_REG_READ(channel, desc_3) != descTbl[3]) {
	msg_printf(ERR, "vino_dma1 desc_3 did not read in ok\n");
	errcount++;
    }

    /*
     * Check that next_desc is in the right place
     */
    next_desc = VINO_REG_READ(channel, next_4_desc);
    next_desc -= ONE_QWRD;
    if (next_desc != (ulong) K1_TO_PHYS (descTbl)) {
	msg_printf(ERR, "vino_dma1 NEXT_DESC incorrectly incremented 0x%x (0x%x)\n", 
			VINO_REG_READ(channel, next_4_desc), next_desc);
	errcount++;
    }

    /*
     * Enable dma and see that end of desc table intr occurs
     * 
     * 06.07.93 - NOTE as soon as the descriptor with a stop
     * bit is written to desc_0 the interrupt flag will be set.
     * so enabling dma to see if the interrupt is unnecessary
     * but not wrong.
     */
    VINO_REG_WRITE(regs_base, control, dmaToActivate);
    timeout = WAIT_TIMEOUT;
    while (timeout-- && (0 == VINO_REG_READ(regs_base, intr_status)))
	DELAY(1);

    doCleanUp();

    if (!(VINO_REG_READ(regs_base, intr_status)&(INTR_EODT<<dmaToIntr))) {
	msg_printf(ERR, "vino_dma1 END OF DESC TBL interrupt not generated\n");
	msg_printf(VRB, "vino_dma1 : timeout is %d\n", timeout);
	errcount++;
    }
    else { /* clear the offending stop bit */
	VINO_REG_WRITE(channel, desc_0, descTbl[0]&CLEAR_STP_JMP);
	VINO_REG_WRITE(regs_base, intr_status, 0x0);
    }

    freeDMABuffers(0L);
    
    if (errcount != 0)
	sum_error ("VINO DMA 1");
    else
	okydoky ();

    return (errcount);
    /*
     * VINO DMA Test 1
     * 
     */
}


/*
 * There is no DMA Test 2. The functionality of DMA Test 2
 * has be incorporated into DMA Test 5
 */


/*
 * DMA Test 3 - Burst capture of A/B, full frame, NTSC/PAL, 4:2:2 YUV
 * 
 * desc_N shifting up correctly
 * correctly fetching n4desc
 */
int
vino_dma3(int argc, char**argv)

{
    int		errcount = 0;
    int		timeout;
    VinoChannel	*channel;
    ulong	dmaToActivate;
    uchar	dmaToIntr;
    uchar	format;	    /* NTSC or PAL */
    ulong	lengthOfLine;
    
    if (DMSD_initialized == FALSE)
	vino_7191_init();

    errcount = parseCmdLine(argc, argv, &channel, &dmaToActivate,
			    &format, &lengthOfLine, &dmaToIntr);
    if (errcount != 0) {
	sum_error ("VINO DMA 3");
	return(errcount);
    }
    
    if (!descTbl)
	initializeDMABuffers();

    /*
     * Load descriptor table
     */
    vinoLoadDescTbl(format);
    
    /*
     * Set the stop bit of descriptors 2 and 4 to
     * check that a) desc are being shifted up correctly
     * and b) n4desc are getting loaded correctly
     * NOTE counting is zero based
     */
    vinoDescStop((ulong *)descTbl, 2);
    vinoDescStop((ulong *)descTbl, 4);
    
    if (format == PAL_CONST)
	setUpDMA(channel, lengthOfLine, START_CLIP_0, END_CLIP_PAL, 
		format, STAND_THRESHOLD);
    else    /* we have NTSC */
	setUpDMA(channel, lengthOfLine, START_CLIP_0, END_CLIP_NTSC, 
		format, STAND_THRESHOLD);

    /*
     * enable DMA
     */
    VINO_REG_WRITE(regs_base, control, dmaToActivate);
    
    /*
     * Catch first interrupt
     * verify that descriptors are being shifted up
     */
    timeout = WAIT_TIMEOUT;
    while (timeout-- && (0 == VINO_REG_READ(regs_base, intr_status)))
	DELAY(1);
    
    doCleanUp();

    if (!(VINO_REG_READ(regs_base, intr_status)&(INTR_EODT<<dmaToIntr))) {
	msg_printf(ERR, "vino_dma3 END OF DESC TBL interrupt not generated\n");
	errcount++;
    }
    else { /* clear the offending stop bit */
	VINO_REG_WRITE(channel, desc_0, descTbl[2]&CLEAR_STP_JMP);
	VINO_REG_WRITE(regs_base, intr_status, 0x0);
    }
    
    if (VINO_REG_READ(channel, desc_1) != VINO_REG_READ(channel, desc_3)) {
	msg_printf(ERR, "vino_dma3 DESC not shifting up 1(%x) 3(%x)\n", 
		    VINO_REG_READ(channel, desc_1),
		    VINO_REG_READ(channel, desc_3));
	errcount++;
    }

    /*
     * Enable dma again and wait for next intr
     * verify that the next four descriptors were read
     * in correctly
     */
    VINO_REG_WRITE(regs_base, control, dmaToActivate);
    timeout = WAIT_TIMEOUT;
    while (timeout-- && (0 == VINO_REG_READ(regs_base, intr_status)))
	DELAY(1);
    
    doCleanUp();
    
    if (!(VINO_REG_READ(regs_base, intr_status)&(INTR_EODT<<dmaToIntr))) {
	msg_printf(ERR, "vino_dma3 END OF DESC TBL interrupt not generated\n");
	errcount++;
    }
    else { /* clear the offending stop bit */
	VINO_REG_WRITE(channel, desc_0, descTbl[4]&CLEAR_STP_JMP);
	VINO_REG_WRITE(regs_base, intr_status, 0x0);
    }
    
    if (VINO_REG_READ(channel, desc_0) != (descTbl[4]&CLEAR_STP_JMP)) {
	msg_printf(ERR, "vino_dma3 desc_0 did not read in ok 0(%x) 4(%x)\n", 
		    VINO_REG_READ(channel, desc_0),
		    descTbl[4]);
	errcount++;
    }
    if (VINO_REG_READ(channel, desc_1) != descTbl[5]) {
	msg_printf(ERR, "vino_dma3 desc_1 did not read in ok 1(%x) 5(%x)\n", 
		    VINO_REG_READ(channel, desc_0),
		    descTbl[5]);
	errcount++;
    }
    if (VINO_REG_READ(channel, desc_2) != descTbl[6]) {
	msg_printf(ERR, "vino_dma3 desc_2 did not read in ok 2(%x) 6(%x)\n", 
		    VINO_REG_READ(channel, desc_0),
		    descTbl[6]);
	errcount++;
    }
    if (VINO_REG_READ(channel, desc_3) != descTbl[7]) {
	msg_printf(ERR, "vino_dma3 desc_3 did not read in ok 3(%x) 7(%x)\n", 
		    VINO_REG_READ(channel, desc_0),
		    descTbl[7]);
	errcount++;
    }
    
    if (!dmaOccuredCorrectly(0L)) {
	errcount++;
	msg_printf(ERR, "vino_dma3 dmaBuf didn't get over-written\n");
    }

    freeDMABuffers(0L);
    
    if (errcount != 0)
	sum_error ("VINO DMA 3");
    else
	okydoky ();

    return (errcount);
    /*
     * VINO DMA Test 3
     * 
     */
}


/*
 * DMA Test 4 - Continuous capture of A/B, full frame, NTSC/PAL
 * jmp bit causes n4desc to be read
 */
int
vino_dma4(int argc, char**argv)

{
    int		errcount = 0;
    int		timeout;
    VinoChannel	*channel;
    ulong	dmaToActivate;
    uchar	dmaToIntr;
    uchar	format;	    /* NTSC or PAL */
    ulong	lengthOfLine;
    
    if (DMSD_initialized == FALSE)
	vino_7191_init();

    errcount = parseCmdLine(argc, argv, &channel, &dmaToActivate,
			    &format, &lengthOfLine, &dmaToIntr);
    if (errcount != 0) {
	sum_error ("VINO DMA 4");
	return(errcount);
    }

    if (!descTbl)
	initializeDMABuffers();

     /*
     * Load descriptor table
     */
    vinoLoadDescTbl(format);
    
    vinoDescJump(descTbl, 1L, 8L);
    vinoDescJump(descTbl, 8L, 4L);
    vinoDescStop(descTbl, 4L);

    if (format == PAL_CONST)
	setUpDMA(channel, lengthOfLine, START_CLIP_0, END_CLIP_PAL, 
		format, STAND_THRESHOLD);
    else    /* we have NTSC */
	setUpDMA(channel, lengthOfLine, START_CLIP_0, END_CLIP_NTSC, 
		format, STAND_THRESHOLD);

    /*
     * enable DMA
     */
    VINO_REG_WRITE(regs_base, control, dmaToActivate);
    
    /*
     * Catch interrupt
     * verify that jump descriptor causes desc_N to be
     * read in correctly
     */
    timeout = WAIT_TIMEOUT;
    while (timeout-- && (0 == VINO_REG_READ(regs_base, intr_status)))
	DELAY(1);

    doCleanUp();

    if (!(VINO_REG_READ(regs_base, intr_status)&(INTR_EODT<<dmaToIntr))) {
	msg_printf(ERR, "vino_dma4 END OF DESC TBL interrupt not generated\n");
	errcount++;
    }
    else { /* clear the offending stop bit */
	VINO_REG_WRITE(channel, desc_0,
		VINO_REG_READ(channel, desc_0)&CLEAR_STP_JMP);
	VINO_REG_WRITE(regs_base, intr_status, 0x0);
    }
    
    if (VINO_REG_READ(channel, desc_0) != (descTbl[4]&CLEAR_STP_JMP)) {
	msg_printf(ERR, "vino_dma4 desc_0 did not read in ok 0(%x) 4(%x)\n", 
		VINO_REG_READ(channel, desc_0), (descTbl[4]&CLEAR_STP_JMP));
	errcount++;
    }
    if (VINO_REG_READ(channel, desc_1) != descTbl[5]) {
	msg_printf(ERR, "vino_dma4 desc_1 did not read in ok\n");
	errcount++;
    }
    if (VINO_REG_READ(channel, desc_2) != descTbl[6]) {
	msg_printf(ERR, "vino_dma4 desc_2 did not read in ok\n");
	errcount++;
    }
    if (VINO_REG_READ(channel, desc_3) != descTbl[7]) {
	msg_printf(ERR, "vino_dma4 desc_3 did not read in ok\n");
	errcount++;
    }
    
    if (!dmaOccuredCorrectly(0L)) {
	errcount++;
	msg_printf(ERR, "vino_dma4 dmaBuf didn't get over-written\n");
    }
    
    descTbl[4] = descTbl[4]&CLEAR_STP_JMP;
    descTbl[1] = NULL;
    freeDMABuffers(0L);

    if (errcount != 0)
	sum_error ("VINO DMA 4");
    else
	okydoky ();

    return (errcount);
    /*
     * VINO DMA Test 4
     * 
     */
}


/*
 * DMA Test 5 - Burst capture of A/B, full frame, NTSC/PAL
 * check that interleaving works
 * when catch INTR_EOFLD, check that next_desc == start_desc and
 * page_index == line_size
 * 
 * 06.17.93 - must check page_index before disabling dma
 */
int
vino_dma5(int argc, char **argv)

{
    int		errcount = 0;
    int		timeout;
    VinoChannel	*channel;
    ulong	dmaToActivate;
    uchar	dmaToIntr;
    uchar	format;	    /* NTSC or PAL */
    ulong	lengthOfLine;
    ulong	next_desc;
    ulong	uniqueDesc = 0L;
    
    if (DMSD_initialized == FALSE)
	vino_7191_init();

    errcount = parseCmdLine(argc,argv, &channel, &dmaToActivate,
			    &format, &lengthOfLine, &dmaToIntr);
    if (errcount != 0) {
	sum_error("VINO DMA 5");
	return errcount;
    }

    if (!descTbl)
	initializeDMABuffers();

    /*
     * Load descriptor table but first save the number of unique
     * descriptors allocated.
     */
    uniqueDesc = descTblTop;
    vinoLoadDescTblInterleave();

    if (format == PAL_CONST)
	setUpDMA(channel, lengthOfLine, START_CLIP_0, END_CLIP_PAL, 
		format, STAND_THRESHOLD);
    else    /* we have NTSC */
	setUpDMA(channel, lengthOfLine, START_CLIP_0, END_CLIP_NTSC, 
		format, STAND_THRESHOLD);
    dmaToActivate |= ENABLE_INTERLEAVE;
    msg_printf(DBG, "vino_dma5 : dmaToActivate (%x) format (%x) length (%x) dmaToIntr (%x)\n", 
		dmaToActivate, format, lengthOfLine, dmaToIntr);

    /*
     * enable DMA
     */
    VINO_REG_WRITE(regs_base, control, dmaToActivate);
    
    /*
     * Catch interrupt
     * verify that jump descriptor causes n4desc to be
     * read in correctly
     */
    timeout = WAIT_TIMEOUT;
    while (timeout-- && (0 == VINO_REG_READ(regs_base, intr_status)))
	DELAY(1);
    
    /*
     * remember to add back the 8 that was subtracted from line_size
     */
    if (VINO_REG_READ(channel, page_index) !=
		(0x8+VINO_REG_READ(channel, line_size))) {
	msg_printf(ERR, "vino_dma5 page_index (%x) not equal to line_size (%x)\n", 
	    VINO_REG_READ(channel, page_index), VINO_REG_READ(channel, line_size));
	errcount++;
    }

    doCleanUp();
    
    if (!(VINO_REG_READ(regs_base, intr_status)&(INTR_EOFLD<<dmaToIntr))) {
	msg_printf(ERR, "vino_dma5 END OF FIELD interrupt not generated (%x)\n", 
		VINO_REG_READ(regs_base, intr_status));
	if (timeout <= 0)
	    msg_printf(ERR, "vino_dma5 interrupt catch TIMEOUT\n");
	errcount++;
    }
    else {
	VINO_REG_WRITE(regs_base, intr_status, 0x0);
    }
    
    next_desc = VINO_REG_READ(channel, next_4_desc);
    next_desc -= ONE_QWRD;
    if (next_desc != VINO_REG_READ(channel, desc_table_ptr)) {
	msg_printf(ERR, "vino_dma5 next_4_desc (%x) not equal to desc_table_ptr (%x)\n", 
		next_desc, VINO_REG_READ(channel, desc_table_ptr));
	errcount++;
    }

    if (!dmaOccuredCorrectly(0L)) {
	errcount++;
	msg_printf(ERR, "vino_dma5 dmaBuf didn't get over-written\n");
    }
    
    freeDMABuffers(descTblTop-uniqueDesc);
    
    if (errcount != 0)
	sum_error ("VINO DMA 5");
    else
	okydoky ();

    return (errcount);
    /*
     * VINO DMA Test 5
     * 
     */
}


/*
 * DMA Test 6 - A/B, NTSC/PAL
 * 
 * Generate FIFO overflow. On a scale of one to bad,  bad being bad
 * FIFO overflow ranks a definite bad
 */
int
vino_dma6(int argc, char**argv)

{
    int		errcount = 0;
    int		timeout;
    VinoChannel	*channel;
    ulong	dmaToActivate;
    uchar	dmaToIntr;
    uchar	format;	    /* NTSC or PAL */
    ulong	lengthOfLine;
    
    if (DMSD_initialized == FALSE)
	vino_7191_init();

    errcount = parseCmdLine(argc,argv, &channel, &dmaToActivate,
			    &format, &lengthOfLine, &dmaToIntr);
    if (errcount != 0) {
	sum_error("VINO DMA 6");
	return errcount;
    }

    if (!descTbl)
	initializeDMABuffers();

    /*
     * Load descriptor table
     */
    vinoLoadDescTbl(format);
    
    if (format == PAL_CONST)
	setUpDMA(channel, lengthOfLine, START_CLIP_0, END_CLIP_PAL, 
		format, STAND_THRESHOLD);
    else    /* we have NTSC */
	setUpDMA(channel, lengthOfLine, START_CLIP_0, END_CLIP_NTSC, 
		format, STAND_THRESHOLD);
    
    /*
     * We want to get a FIFO overflow, so set fifo_threshold
     * to MAX_THRESHOLD
     */
    VINO_REG_WRITE(channel, fifo_threshold, MAX_THRESHOLD);

    /*
     * enable DMA but first set to ignore EOFld
     */
    dmaToActivate = dmaToActivate&INTR_NO_EOFLD;
    VINO_REG_WRITE(regs_base, control, dmaToActivate);
    
    /*
     * Catch interrupt
     */
    timeout = WAIT_TIMEOUT;
    while (timeout-- && (0 == VINO_REG_READ(regs_base, intr_status)))
	DELAY(1);
    
    doCleanUp();
    
    if (!(VINO_REG_READ(regs_base, intr_status)&(INTR_FOF<<dmaToIntr))) {
	msg_printf(ERR, "vino_dma6 FIFO OVERFLOW interrupt not generated\n");
	errcount++;
    }
    else {
	VINO_REG_WRITE(regs_base, intr_status, 0x0);
    }
    
    if (!dmaOccuredCorrectly(0L)) {
	errcount++;
	msg_printf(ERR, "vino_dma6 dmaBuf didn't get over-written\n");
    }
    
    freeDMABuffers(0L);

    if (errcount != 0)
	sum_error ("VINO DMA 6");
    else
	okydoky ();

    return (errcount);
    /*
     * VINO DMA Test 6
     * 
     */
}


/*
 * DMA Test 7 - A and B, NTSC/PAL
 * 
 * Continuous capture of ChA to first half of dmaBuf
 * and ChB to second half of dmaBuf
 * 
 * NOTE -a AND -b MUST BE SET otherwise is only a
 * single channel test
 */
int
vino_dma7(int argc, char**argv)

{
    int		errcount = 0;
    int		timeout;
    VinoChannel	*channel;
    ulong	dmaToActivate;
    uchar	dmaToIntr;
    uchar	format;	    /* NTSC or PAL */
    ulong	lengthOfLine;
    ulong	middle;	    /* middle of dma buffer */
    ulong	theIntr;
        
    if (DMSD_initialized == FALSE)
	vino_7191_init();

    errcount = parseCmdLine(argc,argv, &channel, &dmaToActivate,
			    &format, &lengthOfLine, &dmaToIntr);
    
    if (errcount != 0) {
	sum_error("VINO DMA 7");
	return errcount;
    }

    
    if (!descTbl)
	initializeDMABuffers();

    /*
     * Load descriptor table
     */
    vinoLoadDescTbl(format);
    
    /*
     * Set-up two continuous buffers within dmaBuf
     * using jmp bit
     * 
     * 06.08.93 - remember to make the middle fall
     * on a double word boundary
     */
    middle = (descTblTop/2);
    middle = middle + (ONE_QWRD - (middle%ONE_QWRD));
    descTbl[middle-1] = (JUMP_BIT) | (K1_TO_PHYS(&(descTbl[0])));
    
    /*
     * end of descTbl should already be on a quad-word boundary
     */
    descTbl[descTblTop] = (JUMP_BIT) | (K1_TO_PHYS(&(descTbl[middle])));
    
    if (format == PAL_CONST) {
	setUpDMA(&(regs_base->a), lengthOfLine, START_CLIP_0, END_CLIP_PAL, 
		format, STAND_THRESHOLD);
	setUpDMA(&(regs_base->b), lengthOfLine, START_CLIP_0, END_CLIP_PAL, 
		format, STAND_THRESHOLD);
    }
    else {   /* we have NTSC */
	setUpDMA(&(regs_base->a), lengthOfLine, START_CLIP_0, END_CLIP_NTSC, 
		format, STAND_THRESHOLD);
	setUpDMA(&(regs_base->b), lengthOfLine, START_CLIP_0, END_CLIP_NTSC, 
		format, STAND_THRESHOLD);
    }
    /*
     * the standard setUpDMA() would set both ChA and ChB to
     * write to the same locations. we need to override this
     * action by manually assigning b.next_4_desc
     */
    VINO_REG_WRITE(regs_base, b.next_4_desc, (ulong) K1_TO_PHYS (&(descTbl[middle])));
    msg_printf(DBG, "descTbl[middle] 0x%x\n", &(descTbl[middle]));
    msg_printf(DBG, "vino_dma5 : dmaToActivate (%x) format (%x) length (%x) dmaToIntr (%x)\n", 
		dmaToActivate, format, lengthOfLine, dmaToIntr);

    /*
     * enable DMA
     */
    VINO_REG_WRITE(regs_base, control, dmaToActivate);
    
    /*
     * Catch interrupt
     */
    timeout = WAIT_TIMEOUT*5;
    while (timeout-- && (0 == VINO_REG_READ(regs_base, intr_status)))
	DELAY(1);

    doCleanUp();

    theIntr = VINO_REG_READ(regs_base, intr_status);
    /*
     * check the various combos of intr's : a, b, a&b which are
     * 0x1, 0x8, and 0x9
     */
    if ((theIntr != 0x1) && (theIntr != 0x8) && (theIntr != 0x9)) {
	msg_printf(ERR, "vino_dma7 END OF FIELD interrupt not generated (%x)\n", 
		theIntr);
	if (timeout <= 0)
	    msg_printf(ERR, "vino_dma7 interrupt catch TIMEOUT\n");
	errcount++;
    }
    else {
	VINO_REG_WRITE(regs_base, intr_status, 0x0);
    }

    /*
     * need to check dmaBuf starting at front AND middle
     * because in this case, dmaBuf is actually two buffers
     */
    if (!dmaOccuredCorrectly(0L)) {
	errcount++;
	msg_printf(ERR, "vino_dma7 dmaBuf ChA didn't get over-written\n");
    }
    if (!dmaOccuredCorrectly(middle)) {
	errcount++;
	msg_printf(ERR, "vino_dma7 dmaBuf ChB didn't get over-written\n");
    }

    freeDMABuffers(0L);
 
    if (errcount != 0)
	sum_error ("VINO DMA 7");
    else
	okydoky ();

    return (errcount);
    /*
     * VINO DMA Test 7
     * 
     */
}
