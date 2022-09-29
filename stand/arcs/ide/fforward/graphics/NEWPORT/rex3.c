/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 *  rex Diagnostics
 *  $Revision: 1.27 $
 */

#include <sys/sbd.h>
#include <sys/param.h>
#include <sys/sbd.h>
#include "sys/ng1hw.h"
#include "ide_msg.h"
#include "ng1visuals.h"
#include "gl/rex3macros.h"
#include "local_ng1.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

int Ng1Probe(struct rex3chip *rex3, int idx);
int Ng1ProbeAll(char *base[]);

Rex3chip *REX;
int REXindex;

#ifndef PROM
int BoardSet=0;
extern int dm1;
#endif

/* 
 * Maximum 'safe' fifo levels = actual slots available
 * minus 10 (4 for interrupt latency, 6 for MC write buffer depth).
 * This is not very scientific, as MC write buffer depth is
 * programmable, and interrupt latency isn't known yet for
 * this cpu board.  I'm assuming that what worked for IP20
 * will work here.
 */

#define MAX_GFIFO_LEVEL (32 - 10)
#define MAX_BFIFO_LEVEL (16 - 10)
extern int bt445_xbias[];

/*
 * Basic initialization of the Rex chip.  Most of this
 * was already done in the boot prom, but don't let's
 * take chances.
 */

int
rex3init()
{
	extern int rex3_config_default;
	char *envstring;
	int gfifo_level = MAX_GFIFO_LEVEL;
	int bfifo_level = MAX_BFIFO_LEVEL;
	int tmp;

#ifndef PROM
	if(!ng1checkboard())
		return -1;
	if (envstring = (char *)getenv("IDE_GFIFO")) {
	    gfifo_level = atoi(envstring);
	    if((gfifo_level < 0) || (gfifo_level > MAX_GFIFO_LEVEL))
		gfifo_level = MAX_GFIFO_LEVEL;
	} 
#endif /* !PROM */
	gfifo_level = (gfifo_level << GFIFODEPTH_SHIFT) & GFIFODEPTH_MASK;

#ifndef PROM
	if (envstring = (char *)getenv("IDE_BFIFO")) {
	    bfifo_level = atoi(envstring);
	    if((bfifo_level < 0) || (bfifo_level > MAX_BFIFO_LEVEL))
		bfifo_level = MAX_BFIFO_LEVEL;
	} 
#endif
	bfifo_level = (bfifo_level << BFIFODEPTH_SHIFT) & BFIFODEPTH_MASK;

	tmp = (rex3_config_default & ~ (GFIFODEPTH_MASK|BFIFODEPTH_MASK)) |
		(bfifo_level | BFIFOABOVEINT | gfifo_level | GFIFOABOVEINT);
	REX->p1.set.config = tmp;	

	REX->set.lsmode = 0x0;
	REX->set.lspattern = 0x0;
	REX->set.lspatsave = 0x0;
	REX->set.zpattern = 0x0;
	REX->set.colorback = 0xdeadbeef;
	REX->set.colorvram = 0xffffff;
	REX->set.smask0x = 0x0;
	REX->set.smask0y = 0x0;
	REX->set.xsave = 0x0;
	REX->set.xymove = 0x0;
	REX->set.bresd.word = 0x0;
	REX->set.bress1.word = 0x0;
	REX->set.bresoctinc1 = 0x0;
	REX->set.bresrndinc2 = 0x0;
	REX3WAIT(REX);			/* TFP can fill the fifo here! */
	REX->set.brese1 = 0x0;
	REX->set.bress2 = 0x0;
	REX->set.aweight0 = 0x0;
	REX->set.aweight1 = 0x0;
	REX->set.colorred.word = 0x0;
	REX->set.coloralpha.word = 0x0;
	REX->set.wrmask = 0xffffff;
	REX->p1.set.smask1x = 0x0;
	REX->p1.set.smask1y = 0x0;
	REX->p1.set.smask2x = 0x0;
	REX->p1.set.smask2y = 0x0;
	REX->p1.set.smask3x = 0x0;
	REX->p1.set.smask3y = 0x0;
	REX->p1.set.smask4x = 0x0;
	REX->p1.set.smask4y = 0x0;
	REX->p1.set.xywin = ((0x1000 + bt445_xbias[Ng1Index(REX)]) << 16) | 0x1000;
	REX->p1.set.topscan = 0x3ff;
	REX->p1.set.clipmode = CM_CIDMATCH_MASK; /* No CID or smask checking */

#ifndef PROM
        dm1 = DM1_LO_SRC | DM1_PUPPLANES | DM1_DRAWDEPTH8 |
					DM1_HOSTDEPTH8 | DM1_COLORCOMPARE;
        REX->set.wrmask=0xcc;
	ng1_block(0, 0, 1280, 1024, 0, 0);
	dm1 = DM1_LO_SRC | DM1_RGBPLANES | DM1_DRAWDEPTH24 |
                            DM1_HOSTDEPTH32 | DM1_COLORCOMPARE | DM1_RGBMODE;
        REX->set.wrmask=0xffffff;
	ng1_rgbcolor(0, 0, 0);
	ng1_block(0, 0, 1280, 1024, 0, 0);
#endif /* !PROM */
}

#ifndef PROM

ng1delay(int argc, char **argv)
{
	int t;
	if(!ng1checkboard())
		return -1;
	if (argc < 2) {
		printf ("usage: %s  t (time in 1/100 of sec)\n", argv[0]);
		return -1;
	}
	atob(argv[1], &t);
	DELAY(t); /* Delay of 1sec if t = 100 */
}

int 
ng1setboard(int argc, char **argv)
{
	extern Rex3chip *rex3base[];
	int i, maxng1;

	maxng1 = 2;

	if (argc < 2) {
		printf("usage: %s %s\n", argv[0], (maxng1==1) ? "0": "0|1");
		return -1;
	}

	atob(argv[1], &i);

	if ( i <  0 || i > maxng1 ) {
		msg_printf(ERR, "Illegal NG1 board number %d\n",i);
		return 1;
	}

	if (Ng1Probe(rex3base[i], i)) {
		REX = rex3base[i];
		msg_printf(VRB, "NG1 board %d found.\n", i);
		/* Make the coprocessor available for floting point */
		SetSR(GetSR()|SR_CU0|SR_CU1);
		BoardSet=1;
		REXindex=i;
		return 1;
	}

	msg_printf(VRB, "NG1 board %d not found.\n", i);
	return 0;
}

int
ng1checkboard() {
	if(!BoardSet) {
		msg_printf(INFO, "Please set the graphics board address with ng1_setboard.\n");
		return 0;
	}
	else
		return 1;
}

int
ng1probeall(int argc, char **argv)
{
	char *tmp[4];
	
	return(Ng1ProbeAll(tmp));
}

/*
 * wrex() - load rex register
 *
 *	This is a tool to allow writing of arbitrary values to the various
 *	rex registers from the IDE command line.
 */
int 
ng1wrex(int argc, char **argv)
{
	__psint_t reg;  /* rex register offset */
	int val;  /* rex register data */
	int loop = 1;

	if(!ng1checkboard())
		return -1;
	if (argc < 3) {
		printf("usage: %s rex_reg_offset data [nloops]\n", argv[0]);
		return -1;
	}
        atohex((argv[1]), (int*)&reg);
        atohex((argv[2]), (int*)&val);
        if (argc > 3)
                atohex(argv[3], &loop);


	reg += (__psint_t)REX;

	msg_printf(DBG, "Writing location %#08lx with %#08lx\n", reg, val);

	while (loop-- > 0)
		*(int *)reg = val;
	return 0;
}


/*
 * rrex() - read rex register
 *
 *	This is a tool to allow reading of values from the various
 *	rex registers from the IDE command line.
 */
int 
ng1rrex(int argc, char **argv)
{
	__psint_t reg;  /* rex register offset */
	int loop = 1;
	int tmp;

	if(!ng1checkboard())
		return -1;

	if (argc < 2) {
		printf("usage: %s rex_reg_offset [nloops]\n", argv[0]);
		return -1;
	}
	
        atohex((argv[1]), (int*)&reg);
        if (argc > 2)
                atohex(argv[2], &loop);

	reg += (__psint_t)REX;

	while (loop-- > 0) {
		tmp = *(int *)reg;
		msg_printf(SUM,"Reading from %#08lx returns %#08x\n", reg, tmp);
	}
	return 0;
}

int
ng1regdump(void)
{

	if(!ng1checkboard())
		return -1;
#ifdef __STDC__
#define dumpi(r) msg_printf(SUM, "%s %x ", # r, REX->set. ## r);
#define dumpf(r) msg_printf(SUM, "%s %x ", # r, REX->set. ## r .word);
#define dumpp1(r) msg_printf(SUM, "%s %x ", # r, REX->p1.set. ## r);
#else /* __STDC__ */
#define dumpi(r) msg_printf(SUM, "%s %x ",  "r", REX->set./**/r);
#define dumpf(r) msg_printf(SUM, "%s %x ",  "r", REX->set./**/r .word);
#define dumpp1(r) msg_printf(SUM, "%s %x ", "r", REX->p1.set./**/r);
#endif /* __STDC__ */
	dumpi (drawmode1); dumpi (drawmode0);
	dumpi (lsmode); dumpi (lspattern); dumpi (lspatsave); dumpi (zpattern);
	dumpi (colorback); dumpi (colorvram); dumpi (alpharef);
	dumpi (smask0x); dumpi (smask0y);
	dumpf (_xstart); dumpf (_ystart); dumpf (_xend); dumpf (_yend);
	dumpi (xsave); dumpi (xymove);
	dumpf (bresd); dumpf (bress1); dumpi (bresoctinc1); dumpi (bresrndinc2);
	dumpi (brese1); dumpi (bress2);
	dumpi (aweight0); dumpi (aweight1);
	dumpf (colorred); dumpf (coloralpha); 
	dumpf (colorgrn); dumpf (colorblue);
	dumpf (slopered); dumpf (slopealpha);
	dumpf (slopegrn); dumpf (slopeblue);
	dumpi (wrmask); dumpi (colori); dumpf (colorx);
	dumpf (slopered1); dumpi (hostrw0); dumpi (hostrw1);

	dumpi (dcbmode); 

	dumpp1 (smask1x); dumpp1 (smask1y);
	dumpp1 (smask2x); dumpp1 (smask2y);
	dumpp1 (smask3x); dumpp1 (smask3y);
	dumpp1 (smask4x); dumpp1 (smask4y);
	dumpp1 (topscan); dumpp1 (xywin); dumpp1 (clipmode);
	dumpp1 (config); 

	return 0;
}

#endif /* !PROM */


/*
 * rex Register Tests
 *
 *      The following group of tests verifies that the correct data is
 *      being written to the specified register.
 */

int
test_rex3_register(int command, volatile unsigned int *wreg,
		      volatile unsigned int *rreg, char *name,
		      unsigned int data, unsigned int mask, int shift)
{
	unsigned int expect;
	unsigned int read;

	*wreg = data;
	REX->go.drawmode0 = command;
	read = *rreg;
	expect = (data & mask) << shift; 
	if (read != expect) {
#if !PROM
		msg_printf(ERR, "Error reading from rex register %s, "
				"returned: %#08x, expected: %#08x\n",
				name, read, expect);
#endif
		return 1;
	}
	return 0;
}




int 
test_rex3_slopecolor(
		int command,
		volatile unsigned int *reg,
		char *name,
		unsigned int data,
		unsigned int nbits	/* nbits read back, inc sign bit */
		)
{
	unsigned int expect;
	int sign = 1 << (nbits-1);
	int mask = sign - 1;

	*reg = data;
	REX->go.drawmode0 = command;
	if (data & 0x80000000)
		expect = sign | ((-data) & mask);
	else
		expect = (~sign) & (data & mask);

	if (*reg != expect) {
#if !PROM
		msg_printf(ERR, "Error reading from rex register %s, "
		"returned: %#08x, expected: %#08x\n", name, *reg, expect);
#endif
		return 1;
	}
	return 0;
}

#define NBITMASK(n)	((1 << n)-1)
#define NBITMASK_32	(~0)

int 
test_rex3(void)
{
	unsigned int pattern;
	int errors = 0;
	int i;

#ifndef PROM
	if(!ng1checkboard())
		return -1;
#endif /* !PROM */
	rex3init();	/* restore rex to sanity */

#if !PROM
	msg_printf(VRB, "Starting rex tests...\n");
#endif
	
	/*
	 * Silly macros to save some typing ...
	 */
#ifdef __STDC__
#define T(wreg,rreg,mask,shift) \
 errors += test_rex3_register (DM0_NOP, (uint *)&REX->set. ## wreg, \
				(uint *)&REX->set. ## rreg, \
 				# wreg, pattern, mask, shift)

#define TW(wreg,rreg,mask,shift) \
 errors += test_rex3_register (DM0_NOP, (uint *)&REX->set. ## wreg .word, \
				(uint *)&REX->set. ## rreg .word, \
 				# wreg, pattern, mask, shift)

#define TIW(wreg,rreg,mask,shift) \
 errors += test_rex3_register (DM0_NOP, (uint *)&REX->set. ## wreg , \
				(uint *)&REX->set. ## rreg .word, \
 				# wreg, pattern, mask, shift)
#define SW(wreg,nbits) \
 errors += test_rex3_slopecolor (DM0_NOP, (uint *)&REX->set. ## wreg .word, \
 				# wreg, pattern, nbits)

#define P(wreg,rreg,mask,shift) \
 errors += test_rex3_register (DM0_NOP, (uint *)&REX->p1.set. ## wreg, \
				(uint *)&REX->p1.set. ## rreg, \
 				# wreg, pattern, mask, shift)
#else /* __STDC__ */
#define T(wreg,rreg,mask,shift) \
 errors += test_rex3_register (DM0_NOP, (uint *)&REX->set./**/wreg, \
				(uint *)&REX->set./**/rreg, \
 				"wreg", pattern, mask, shift)
#define TW(wreg,rreg,mask,shift) \
 errors += test_rex3_register (DM0_NOP, (uint *)&REX->set./**/wreg .word, \
				(uint *)&REX->set./**/rreg .word, \
 				# wreg, pattern, mask, shift)

#define TIW(wreg,rreg,mask,shift) \
 errors += test_rex3_register (DM0_NOP, (uint *)&REX->set./**/wreg , \
				(uint *)&REX->set./**/rreg .word, \
 				# wreg, pattern, mask, shift)
#define SW(wreg,nbits) \
 errors += test_rex3_slopecolor (DM0_NOP, (uint *)&REX->set./**/wreg .word, \
 				"wreg", pattern, nbits)

#define P(wreg,rreg,mask,shift) \
 errors += test_rex3_register (DM0_NOP, (uint *)&REX->p1.set./**/wreg, \
				(uint *)&REX->p1.set./**/rreg, \
 				"wreg", pattern, mask, shift)
#endif /* __STDC__ */
	for (i = 3; i >= 0; i--) {
		pattern = i*0x55555555;

	/*
	 * Simple tests - just write something and 
	 * expect the same value back.
	 */

		T(lsmode, lsmode, NBITMASK(28), 0);
		T(lspattern, lspattern, NBITMASK_32, 0);
		T(lspatsave, lspatsave, NBITMASK_32, 0);
		T(zpattern, zpattern, NBITMASK_32, 0);
		T(colorback, colorback, NBITMASK_32, 0); 
		T(colorvram, colorvram, NBITMASK_32, 0);
		T(alpharef, alpharef, NBITMASK(8), 0);
		T(smask0x, smask0x, NBITMASK_32, 0);
		T(smask0y, smask0y, NBITMASK_32, 0);
		TW(_xstart, _xstart, NBITMASK(20) << 7, 0);
		TW(_ystart, _ystart, NBITMASK(20) << 7, 0);
		TW(_xend, _xend, NBITMASK(20) << 7, 0);
		TW(_yend, _yend, NBITMASK(20) << 7, 0);
		T(xsave, xsave, NBITMASK(16), 0);
		T(xymove, xymove, NBITMASK_32, 0);
		TW(bresd, bresd, NBITMASK(27), 0);
                TW(bress1, bress1, NBITMASK(17), 0);
                T(bresoctinc1, bresoctinc1, NBITMASK(27) & ~(0xf << 20), 0);
                T(bresrndinc2, bresrndinc2, NBITMASK_32  & ~(0x7 << 21), 0);
                T(brese1, brese1, NBITMASK(16), 0);
                T(bress2, bress2, NBITMASK(26), 0);
		T(aweight0, aweight0, NBITMASK_32, 0);
		T(aweight1, aweight1, NBITMASK_32, 0);
		TW(colorred, colorred, NBITMASK(24), 0);
		TW(coloralpha, coloralpha, NBITMASK(20), 0);
		TW(colorgrn , colorgrn , NBITMASK(20), 0);
		TW(colorblue, colorblue, NBITMASK(20), 0);
		SW(slopered, 24);
		SW(slopealpha, 20);
		SW(slopegrn, 20);
		SW(slopeblue, 20);
		T(wrmask, wrmask, NBITMASK(24), 0);
		T(hostrw0, hostrw0, NBITMASK_32, 0);
		T(hostrw1, hostrw1, NBITMASK_32, 0);

		P(smask1x, smask1x, NBITMASK_32, 0);
		P(smask1y, smask1y, NBITMASK_32, 0);
		P(smask2x, smask2x, NBITMASK_32, 0);
		P(smask2y, smask2y, NBITMASK_32, 0);
		P(smask3x, smask3x, NBITMASK_32, 0);
		P(smask3y, smask3y, NBITMASK_32, 0);
		P(smask4x, smask4x, NBITMASK_32, 0);
		P(smask4y, smask4y, NBITMASK_32, 0);
		P(topscan, topscan, NBITMASK(10), 0);
		P(xywin, xywin, NBITMASK_32, 0);
		P(clipmode, clipmode, NBITMASK(13), 0);

		/* 
		 * Check that values written to the read-only
		 * coordinate registers show up in the context
		 * switchable registers
		 * XXX Check the GL floating point registers too.
		 * XXX Check the Bresenham registers too.
		 */
		TIW(xstarti, _xstart, NBITMASK(16), 11);
		TIW(xystarti, _xstart, NBITMASK(16), 11);
		TIW(xystarti, _ystart, NBITMASK(16), 11);
		TIW(xyendi, _xend, NBITMASK(16), 11);
		TIW(xyendi, _xend, NBITMASK(16), 11);
	}

	rex3init();	/* restore rex to sanity */

	if (!errors) {
#ifndef PROM
		msg_printf(INFO, "All of the REX register r/w tests have passed.\n");
#endif
		return 0;
	} else {
#ifndef PROM
		msg_printf(ERR, "Total of %d errors detected in REX "
				"register r/w tests.\n", errors);
#endif
		return -1;
	}
}
