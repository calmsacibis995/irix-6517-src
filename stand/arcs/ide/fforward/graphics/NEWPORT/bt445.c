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
 *  DAC Diagnostics
 *  $Revision: 1.25 $
 */


#include "sys/ng1hw.h"
#include <sys/types.h>
#include <sys/sema.h>
#include <sys/gfx.h>
#include "sys/ng1.h"
#include "sys/vc2.h"
#include "sys/ng1_cmap.h"
#include "sys/xmap9.h"
#include "ide_msg.h"
#include "local_ng1.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

extern struct ng1_info ng1_ginfo[];
extern Rex3chip *REX;
extern int REXindex;

#define CMAP0 0
#define CMAP1 1
#define DAC 2
#define VRINTWAIT(REX)  while (!(REX->p1.set.status & VRINT))
static char *chipname[] = {"CMAP0:U5", "CMAP1:U8", "DAC"};


#ifndef PROM

int ng1wbt445(int argc, char **argv)
{
	int crs, reg, data;

	if(!ng1checkboard())
		return -1;

	if (argc < 4) {
		msg_printf(ERR, "usage: %s crs reg data\n", argv[0]);
		return -1;
	}
	atohex(argv[1], &crs);
	atohex(argv[2], &reg);
	atohex(argv[3], &data);

	msg_printf(DBG, "Writing Bt445 reg %#01x with %#02x\n", reg, data);
	Bt445Set(REX, (crs << DCB_CRS_SHIFT), reg, data);
	return 0;
}


int ng1rbt445(int argc, char **argv)
{
	int crs, reg, data;

	if(!ng1checkboard())
		return -1;
	if (argc < 3) {
		msg_printf(ERR, "usage: %s crs reg\n", argv[0]);
		return -1;
	}
	atohex(argv[1], &crs);
	atohex(argv[2], &reg);
	Bt445Get (REX, (crs << DCB_CRS_SHIFT), reg, data);
	msg_printf(DBG,"Reading Bt445 reg %#01x returns %#02x\n", reg, data);
	return 0;
}


int ng1wcmap(int argc, char **argv)
{
        int reg;                /* cmap register offset */
        int data;               /* cmap register data */

	if(!ng1checkboard())
		return -1;
        if (argc != 3) {
                printf("usage: %s cmap_reg data\n", argv[0]);
                return -1;
        }
        atohex(argv[1], &reg);
        atohex(argv[2], &data);

	if (reg < 4 || reg == 7) {
        msg_printf(DBG,"Writing cmap reg %d with %#04x\n", reg, data);
        cmapSetReg(REX, (reg << DCB_CRS_SHIFT), data);
	}
	else
		printf("Cmap reg %d is read only register.\n", reg);

        return 0;
}


int ng1rcmap(int argc, char **argv)
{
        int chip;       /* which cmap, 0 or 1 */
        int reg;        /* cmap register index */
        int data;       /* cmap register data */

	if(!ng1checkboard())
		return -1;
        if (argc < 3) {
                printf("usage: %s cmap_reg chip[0|1]\n", argv[0]);
                return -1;
        }
        atohex(argv[1], &reg);
        atohex(argv[2], &chip);

        cmapGetReg (REX, (reg << DCB_CRS_SHIFT), chip, data);
        msg_printf(DBG,"Reading cmap%d reg %d returns %#02x\n", chip, reg, data);
        return 0;
}

get_palette_rgb ( int chip, char *r, char *g, char *b)
{
        if (chip == DAC) {
                Bt445GetRGB (REX,*r, *g, *b);
        } else {
                cmapGetRGB (REX, *r, *g, *b);
        }
}

set_palette_addr( int chip,  int i)
{
        if (chip == DAC) {
                Bt445SetAddr( REX, i );
        } else {
                cmapSetAddr( REX, i );
        }
}

set_palette_read_addr( int chip,  int i)
{
        if (chip == DAC) {
                Bt445SetAddr( REX, i );
        } else {
                cmapSetReadAddr( REX, chip, i );
        }
}

set_palette_rgb( int chip, int r, int g, int b)
{
        if (chip == DAC) {
                Bt445SetRGB (REX,r, g, b);
        } else {
                cmapSetRGB (REX,r, g, b);
        }
}

#endif /* !PROM */

rgberror ( int chip, char *c, int i, int expect, int got )
{
	msg_printf(VRB, "Error reading from (%s) %s address 0x%04x, returned: %#02x, expected: %#02x\n", chipname[chip], c, i, expect, got);
}

/*
 * Test CMAP color palette or Bt445 color palette 
 * (used for gamma correction only)
 * 
 * XXX Add test of cursor and overlay palettes ?
 */

int 
ng1test_palette( int chip, int cmaprev)
{
	int i, tmp;
	char *r, *g, *b;
	char *rsave, *gsave, *bsave;
	int errors = 0;
	int data, rdata, gdata, bdata;
	int palettesize;

	VRINTWAIT(REX);
	VRINTWAIT(REX);

	if (chip == DAC)
		palettesize = 256;
	else
		palettesize = 8192;

	if ((r = (char*)malloc (palettesize * 3)) == NULL) {
		msg_printf (ERR," Out of memory in palette test\n");
		return -1;
	}
	g = r + palettesize;
	b = g + palettesize;

	if((rsave = (char*)malloc (256 *3)) == NULL) {
		msg_printf (ERR, "Out of memory in palette test\n");
		return -1;
	}
	gsave = rsave + 256;
	bsave = gsave + 256;

	/* Save the 256 entries of the current color table */

	BFIFOWAIT(REX);
	VRINTWAIT(REX);
	cmapFIFOEMPTY(REX);
	set_palette_read_addr (chip, 0);
	if(cmaprev >= 2) {
		/*
		 * Enable 'diag read'.
		 */
		cmapSetOneReg(REX, CMAP_CRS_DIAG_READ, chip, i);
		/*
		 * Reset dcbmode for cmap palette read
		 */
		REX->set.dcbmode = ((chip) ? DCB_CMAP1 : DCB_CMAP0) |
		DCB_CMAP_PROTOCOL | CMAP_CRS_PAL_BUF | DCB_DATAWIDTH_3;
	}
	for (i = 0; i < 256; i++) {
		get_palette_rgb (chip, rsave + i, gsave + i, bsave + i);
	}
	if(cmaprev >= 2) {
		/*
		 * Disable 'diag read'.
		 */
		cmapGetReg(REX, CMAP_CRS_DIAG_READ, chip, tmp);
	}

	msg_printf(7, "Loading color table for 8bit\n");
	for (data = 0; data < 0x100; data += 0x55) {
		rdata = data;
		gdata = data ^ 0x55;
		bdata = data ^ 0xff;

		msg_printf(7, "Loading pattern Rdata=%#02x, Gdata=%#02x, Bdata=%#02x\n", rdata, gdata, bdata);
		/*
		 * Load the color table
		 */
		for (i = 0; i < palettesize; i++) {
			if ( ( i & 0x0F ) == 0x0F ) 
				BFIFOWAIT( REX );
			/* Check cmapFIFOWAITCHIP every 31 entry. Do not change
			 * this. The test fails at random location.
			 * We need to set the address twice because there is
			 * a h/w problem when the bus is changed from write
			 * to read to write again, the address may be lost.
			 */
			if ( ( i & 0x1F ) == 0x1F ) {
				cmapFIFOWAITCHIP(REX, chip);
				set_palette_addr (chip, i);
				set_palette_addr (chip, i);
			}
			/* Need to disable CBLANK before write and then 
			 * enable immediately. We are not using the
			 * the autoincrement feature for address. This
			 * is a macro problem and not a h/w problem.
			 */
			cmapSetReg (REX, CMAP_CRS_CMD_REG, 0x07); 
			set_palette_addr (chip, i);
			set_palette_rgb (chip, rdata, gdata, bdata);
			cmapSetReg (REX, CMAP_CRS_CMD_REG, 0x03); 
		}

		/*
		 * Read the color table. 
		 */
		BFIFOWAIT(REX);
		VRINTWAIT(REX);
		cmapFIFOEMPTY(REX);
		/* First read operation immediately after a write is not
		 * guaranteed. So we start read at 8191 and ignore the first
		 * read. The counter automatically resets to 0 after 8K.
		 * XXX Does the diag palette read feature in cmaprev 2
		 * fix this problem or not ?  Spec doesn't say ...
	         */
		set_palette_read_addr (chip, 8191);
		if(cmaprev >= 2) {
			cmapSetOneReg(REX, CMAP_CRS_DIAG_READ, chip, i);
			/*
			 * Reset dcbmode for cmap palette read
			 */
                        REX->set.dcbmode = ((chip) ? DCB_CMAP1 : DCB_CMAP0) |
                        DCB_CMAP_PROTOCOL | CMAP_CRS_PAL_BUF | DCB_DATAWIDTH_3;
		}
		get_palette_rgb (chip, r , g , b );	/* dummy read */
		for (i = 0; i < palettesize; i++) {
			get_palette_rgb (chip, r + i, g + i, b + i);
		}

		if(cmaprev >= 2) {
			/* Turn off diag palette read mode */
			cmapGetReg(REX, CMAP_CRS_DIAG_READ, chip, tmp);
		}

		/*
	 	 * Compare
	 	 */
		for (i = 0; i < palettesize; i++) {
			if (r[i] != rdata) {
				rgberror(chip, "red", i, r[i], rdata);
				errors++;
			}
			if (g[i] != gdata) {
				rgberror(chip, "green", i, g[i], gdata);
				errors++;
			}
			if (b[i] != bdata) {
				rgberror(chip, "blue", i, b[i], bdata);
				errors++;
			}
		}
	}

	/*
	 * Load the color table.  This time, r,g,b
	 */
	msg_printf(7,"Loading color table with address & data same.\n");
	for (i = 0; i < palettesize; i++) {
		data = (i & 0xff);
		if ( ( i & 0x0F ) == 0x0F )
			BFIFOWAIT( REX );
		if ( ( i & 0x1F ) == 0x1F ) {
			cmapFIFOWAITCHIP(REX,chip);
                        set_palette_addr (chip, i);
                        set_palette_addr (chip, i);
		}

		cmapSetReg (REX, CMAP_CRS_CMD_REG, 0x07); 
		set_palette_addr (chip, i);
		set_palette_rgb (chip, data, data ^ 0x55, data ^ 0xff);
		cmapSetReg (REX, CMAP_CRS_CMD_REG, 0x03); 
		msg_printf(7, "Loading pattern Rdata=%#02x, Gdata=%#02x, Bdata=%#02x\n", data, data ^ 0x55, data ^ 0xff);
	}

	/*
	 * Read the color table
	 */
	BFIFOWAIT(REX);
	VRINTWAIT(REX);
	cmapFIFOEMPTY(REX);
	set_palette_read_addr (chip, 8191);
	if(cmaprev >= 2) {
		cmapSetOneReg(REX, CMAP_CRS_DIAG_READ, chip, i);
		/*
		 * Reset dcbmode for cmap palette read
		 */
                REX->set.dcbmode = ((chip) ? DCB_CMAP1 : DCB_CMAP0) |
                DCB_CMAP_PROTOCOL | CMAP_CRS_PAL_BUF | DCB_DATAWIDTH_3;
	}
	get_palette_rgb (chip, r , g , b );	/* dummy read */
	for (i = 0; i < palettesize; i++) {
		get_palette_rgb (chip, r + i, g + i, b + i);
	}

	if(cmaprev >= 2) {
		cmapGetReg(REX, CMAP_CRS_DIAG_READ, chip, tmp);
	}
	/*
	 * Compare
	 */
	for (i = 0; i < palettesize; i++) {
		data = (i & 0xff);
		if (r[i] != data) {
			rgberror(chip, "red", i, r[i], data);
			errors++;
		}
		if (g[i] != (data ^ 0x55)) {
			rgberror(chip, "green", i, g[i], data ^ 0x55);
			errors++;
		}
		if (b[i] != (data ^ 0xff)) {
			rgberror(chip, "blue", i, b[i], data ^ 0xff);
			errors++;
		}
	}

	 /* Restore the original color table */
	for (i = 0; i < 256; i++) {
		if ( ( i & 0x0F ) == 0x0F ) 
			BFIFOWAIT( REX );
		if ( ( i & 0x1F ) == 0x1F ) {
			cmapFIFOWAITCHIP(REX,chip);
                        set_palette_addr (chip, i);
                        set_palette_addr (chip, i);
		}
		cmapSetReg (REX, CMAP_CRS_CMD_REG, 0x07); 
		set_palette_addr (chip, i);
		set_palette_rgb (chip, *(rsave + i), *(gsave + i), *(bsave + i));
		cmapSetReg (REX, CMAP_CRS_CMD_REG, 0x03); 
	}
	free (r);
	free (rsave);
	return errors;
}
int 
ng1test_dac_palette( int chip )
{
	int i, tmp;
	char *r, *g, *b;
	char *rsave, *gsave, *bsave;
	int errors = 0;
	int data, rdata, gdata, bdata;
	int palettesize;

	if (chip == DAC)
		palettesize = 256;
	else
		palettesize = 8192;

	if ((r = (char *)malloc (palettesize * 3)) == NULL) {
		msg_printf (ERR," Out of memory in palette test\n");
		return -1;
	}
	g = r + palettesize;
	b = g + palettesize;

	if((rsave = (char*)malloc (256 *3)) == NULL) {
		msg_printf (ERR, "Out of memory in palette test\n");
		return -1;
	}
	gsave = rsave + 256;
	bsave = gsave + 256;

	/* Save the 256 entries of the current color table */

	set_palette_read_addr (chip, 0);
	for (i = 0; i < 256; i++) {
		get_palette_rgb (chip, rsave + i, gsave + i, bsave + i);
	}

	for (data = 0; data < 0x100; data += 0x55) {
		rdata = data;
		gdata = data ^ 0x55;
		bdata = data ^ 0xff;

		/*
		 * Load the color table
		 */
		set_palette_addr (chip, 0);
		for (i = 0; i < palettesize; i++) {
			if ( ( i & 0x0F ) == 0x0F ) {
				BFIFOWAIT( REX );
			}
			set_palette_rgb (chip, rdata, gdata, bdata);
		}

		/* Read the color table.  */
		set_palette_read_addr (chip, 0);
		for (i = 0; i < palettesize; i++) {
			get_palette_rgb (chip, r + i, g + i, b + i);
		}

		/*
	 	 * Compare
	 	 */
		for (i = 0; i < palettesize; i++) {
			if (r[i] != rdata) {
				rgberror(chip, "red", i, r[i], rdata);
				errors++;
			}
			if (g[i] != gdata) {
				rgberror(chip, "green", i, g[i], gdata);
				errors++;
			}
			if (b[i] != bdata) {
				rgberror(chip, "blue", i, b[i], bdata);
				errors++;
			}
		}
	}

	/*
	 * Load the color table.  This time, r,g,b
	 */
	set_palette_addr (chip, 0);
	for (i = 0; i < palettesize; i++) {
		data = (i & 0xff);
		if ( ( i & 0x0F ) == 0x0F )
			BFIFOWAIT( REX );
		set_palette_rgb (chip, data, data ^ 0x55, data ^ 0xff);
	}

	/*
	 * Read the color table
	 */
	set_palette_read_addr (chip, 0);
	for (i = 0; i < palettesize; i++) {
		get_palette_rgb (chip, r + i, g + i, b + i);
	}

	/*
	 * Compare
	 */
	for (i = 0; i < palettesize; i++) {
		data = (i & 0xff);
		if (r[i] != data) {
			rgberror(chip, "red", i, r[i], data);
			errors++;
		}
		if (g[i] != (data ^ 0x55)) {
			rgberror(chip, "green", i, g[i], data ^ 0x55);
			errors++;
		}
		if (b[i] != (data ^ 0xff)) {
			rgberror(chip, "blue", i, b[i], data ^ 0xff);
			errors++;
		}
	}

	 /* Restore the original color table */
	set_palette_addr (chip, 0);
	for (i = 0; i < 256; i++) {
		if ( ( i & 0x0F ) == 0x0F ) {
			BFIFOWAIT( REX );
		}
		set_palette_rgb (chip, *(rsave + i), *(gsave + i), *(bsave + i));
	}
	free (r);
	free (rsave);
	return errors;
}
/*
 * Test cmap registers
 */

int test_cmap_register(uint reg, uint pattern, uint chip)
{
        uint got;

        cmapSetReg (REX, reg, pattern);
        cmapGetReg (REX, reg, chip, got);

        if (got != pattern) {
                msg_printf(VRB, "Error reading from cmap(%d):U%d reg %x returned: %#02x, expected: %#02x\n", chip,(chip & 1) ? 8 : 5, reg, got, pattern);
                return 1;
        }
        return 0;
}


/*
 * Test bt445 registers
 */

int test_bt445_register(uint crs, uint reg, uint pattern)
{
        uint got;

        if (crs == RDAC_CRS_ADDR_REG) {
                Bt445SetAddr (REX, pattern);
                Bt445GetAddr (REX, got);
        }
        else {
                Bt445Set (REX, crs, reg, pattern);
                Bt445Get (REX, crs, reg, got);
        }

        if (got != pattern) {
                msg_printf(VRB, "Error reading from Bt445 reg %x returned: %#02x, expected: %#02x\n", (crs << 8) | reg, got, pattern);
                return 1;
        }
        return 0;
}


int ng1bt445test()
{
	int i, j, k;
	int errors = 0;
	int pattern;
	int rgbctlmask[4] = 	{0x1f, 0x7, 0xff, 0xff};
	int setupmask[16] = 	{0, 0x3f, 0xbf, 0x3f,
				 0, 0x3f, 0xcf, 0xff,
				 0x1c, 0x7f, 0xbd, 0x3f,
				 0, 0x1f, 0x1f, 0};
				
	if(!ng1checkboard())
		return -1;
	msg_printf(VRB, "Starting DAC tests ...\n");

	/* Check address register, 8 bits */

	for (i = 0; i < 256; i++) 
		errors += test_bt445_register (RDAC_CRS_ADDR_REG, 0, i);

	/* Test the primary color palette (Newport uses it as a gamma ramp) */

#ifndef PROM
	errors += ng1test_dac_palette(DAC);
#endif /* !PROM */

	/* Test the r,g,b pixel format registers */

#if 0
Notyet - must restore bt445 clock BEFORE calling Ng1RegisterInit
	for (i = 0; i < 0x18; i+= 8)
		for (j = 0; j < 4; j++)
			for (pattern = 0; pattern < 0x100; pattern += 0x55)
				errors += 
				   test_bt445_register(RDAC_CRS_RGB_CTRL,i+j,
					pattern & rgbctlmask[j]);

	/* Test the setup  registers */

	for (i = 0; i < 0x10; i++)
		for (pattern = 0; pattern < 0x100; pattern += 0x55)
			if (setupmask[i])
				errors += test_bt445_register(RDAC_CRS_SETUP,i,
					pattern & setupmask[i]);
#else
msg_printf (VRB, "Skipping bt445 clock setup regs test\n");
#endif


	/* Restore backend to sanity */

	Ng1RegisterInit(REX, &ng1_ginfo[REXindex]);

	if (errors) {
		msg_printf(SUM,"Total errors in DAC test: %d\n", errors);
		return -1;
	}
	else {
		msg_printf(INFO, "All of the DAC tests have passed.\n");
		return 0;
	}
}
int ng1cmaptest()
{
	int i;
	int errors = 0;
	int pattern;
	int saveerrors=0;
	int savereport;
	int test_pass=0;

	if(!ng1checkboard())
		return -1;
	rex3init();

	msg_printf(VRB, "Starting CMAP tests ...\n");

        for (i = 0;i < 2; i++)          /* test cmaps individually */
        for (pattern = 0; pattern <= 0xff; pattern += 0x55) {
                errors += test_cmap_register(CMAP_CRS_ADDR_LO, pattern, i);
                errors += test_cmap_register(CMAP_CRS_ADDR_HI, pattern & 0x1f, i);
	}

	if(!(ng1_ginfo[REXindex].cmaprev))
		msg_printf(VRB, "Skipping CMAP Palette test for CMAP version 000.\n");
	else {
		errors += ng1test_palette(CMAP0, ng1_ginfo[REXindex].cmaprev);
		errors += ng1test_palette(CMAP1, ng1_ginfo[REXindex].cmaprev);
	}

        Ng1RegisterInit(REX, &ng1_ginfo[REXindex]);

	rex3init();

	if (errors) {
		msg_printf(SUM,"Total errors in CMAP test: %d\n", errors);
		return -1;
	}
	else {
		msg_printf(INFO, "All of the CMAP tests have passed.\n");
		return 0;
	}
}
