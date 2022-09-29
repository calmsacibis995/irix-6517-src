/*
 * $Revision: 1.20 $
 * This file has source for all the tests added for debugging the
 * newport graphics board (ng1). These tests were added especially
 * for manufacturing R/A area. These tests are not part of field.ide.
 */

#include "sys/types.h"
#include "sys/sema.h"
#include "sys/gfx.h"
#include "sys/ng1.h"
#include "sys/ng1hw.h"
#include "sys/cpu.h"
#include "sys/sbd.h"
#include "sys/param.h"
#include "sys/cmn_err.h"
#include "sys/invent.h"
#include "sys/ng1_cmap.h"
#include "sys/vc2.h"
#include "sys/xmap9.h"
#include "ide_msg.h"
#include "local_ng1.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"


extern struct ng1_info ng1_ginfo[];
extern Rex3chip *REX;
extern int REXindex;
extern int dm1;
int ng1_testaddr8(int rw);
int ng1_testaddr24(int rw);
int ng1_fastclear(int);

#define CMAP0 0
#define CMAP1 1
#define DAC 2
#define VRINTWAIT(REX)  while (!(REX->p1.set.status & VRINT))
static char *chipname[] = {"CMAP0:U5", "CMAP1:U8", "DAC"};

struct {char *name; int type;} 
timings[] = {	

	"New Sony multiscan 19 inch 76Hz",	 1,
	"New Sony multiscan 16 inch 76Hz",	 2,
	"New Wyse 15 inch single scan 70 Hz",	 6,
	"Dual-sync 19 inch Mitsubishi 72 Hz",	 9,
	"16 inch Mitsubishi 60 Hz",		10,
	"Dual scan Sony 19 inch, 60 Hz",	12,
	"Dual scan Sony 16 inch, 60 Hz",	13,
	"Any 1280x1024 @ 60 Hz",		15,
	"Any 1280x1024 @ 50 Hz",		16,
	"Any 1024x768 @ 50 Hz",			17,
};

int
v8err (uint x, uint y, uint quad, uint expect)
{
	int k;
	uint got;
	uint bank_no=0;

	for(k=0; k<4; k++){
		got = quad & 0xff;
		if (got != expect) {
			if(*Reportlevel > 4)
				bank_no=check_bank(x+3-k, y);	
			msg_printf(VRB, "Error reading VRAM x %x y %x, "
			    "read: %#02x, expected: %#02x\n",
			    x+3-k, y, got, expect);
			if(*Reportlevel > 4) {
				print_bank(bank_no);
				print_chip(1, bank_no); /* Always lower chip for 8bit */
			}
			return 1;
		}
		quad = quad >> 8;
	}
	return 0;
}

int
v24err (uint x, uint y, uint got, uint expect)
{
	int bank_no=0;
	if (got != expect) {
		if(*Reportlevel > 4)
			bank_no=check_bank(x, y);
		msg_printf(VRB, "Error reading VRAM x %x y %x, "
		    "read: %#06x, expected: %#06x\n",
		    x, y, got, expect);
		if(*Reportlevel > 4) {
			print_bank(bank_no);
			check_bits(expect, got, bank_no);
		}
		return 1;
	}
	return 0;
}

ng1setvt (int argc, char **argv)
{
	int ntypes = sizeof (timings) / sizeof (timings[0]);
	int i, monid;

	if(!ng1checkboard())
                return -1;
	if (argc < 2) {
		printf ("Usage : %s [0-%d]\n", argv[0], ntypes - 1);
		printf ("Current Monitor Type = %d\n", ng1_ginfo[REXindex].monitortype);
		for (i = 0; i < ntypes; i++) 
			printf ("%d: %s\n", i, timings[i].name);
		return -1;
	}
	atob (argv[1], &monid);
	if (monid < 0) monid = 0;
	else if (monid > ntypes - 1) monid = ntypes - 1;

	ng1_ginfo[REXindex].monitortype = timings[monid].type;
	printf ("setting video timing for %s type %d\n", 
		timings[monid].name, ng1_ginfo[REXindex].monitortype);
	Ng1RegisterInit(REX, &ng1_ginfo[REXindex]);
		
	return 0;
}

/*
 *  User must set bus width before running test.
 */
ng1giobustest(int argc, char **argv)
{
	int b, w;
	int tmp_dm1 = dm1;
	int errors = 0;

	if(!ng1checkboard())
                return -1;
	ng1_setvisual(0);
	/* Configure drawmode1 to read/write double iff 64-bit bus configured.*/
	if (REX->p1.set.config & BUSWIDTH) {
		dm1 |= DM1_RWDOUBLE;
		dm1 |= DM1_RWPACKED;
	}
	else {
		dm1 &= ~DM1_RWDOUBLE;
		dm1 &= ~DM1_RWPACKED;
	}
	/* Walk 1 across lo half of bus & log errors. */
	for (b = 0; b < 32; b++)
	{
		w = 1 << b;
		REX->set.hostrw1 = w;
		if (REX->set.hostrw1 != w) {
			errors++;
			msg_printf(VRB, "GIO Bus error bit %d\n", b);
		}
	}

	/* Walk 1 across hi half of bus & log errors. */
	for (b = 0; b < 32; b++)
	{
		w = 1 << b;
		REX->set.hostrw0 = w;
		if (REX->set.hostrw0 != w) {
			errors++;
			msg_printf(VRB, "GIO Bus error bit %d\n", b + 32);
		}
	}

	/* Restore original drawmode1. */
	dm1 = tmp_dm1;
	if(errors)
		msg_printf(INFO, "Total errors found in giobustest = %d\n", errors);
	else
		msg_printf(INFO, "All of the giobustest passed\n");
}

int ng1rvram(int argc, char **argv)
{
        int tmp_dm1;
        int x, y;
        unsigned int quad;
        unsigned char *got;
	int loop = 1;
	int bitmode=0;

	if(!ng1checkboard())
		return -1;

        if (argc < 4) {
                printf("usage: %s x y bitmode[8/24] [nloops]\n", argv[0]);
                return -1;
        }
        tmp_dm1=dm1;
        atob(argv[1], &x);
        atob(argv[2], &y);
	atob(argv[3], &bitmode);
	if (argc > 4)
                atohex(argv[4], &loop);

	if(bitmode == 24) {
        	dm1 = DM1_RGBPLANES | DM1_COLORCOMPARE | DM1_LO_SRC;
		dm1 |= DM1_RGBMODE | DM1_DRAWDEPTH24 | DM1_HOSTDEPTH32 | DM1_RWPACKED;
		ng1_writemask (0xffffff);
		ng1_setvisual(5);
	}
	else {
        	dm1 = DM1_RGBPLANES | DM1_LO_SRC;
        	dm1 |= DM1_DRAWDEPTH8 | DM1_HOSTDEPTH8 | DM1_RWPACKED;
        	ng1_writemask (0xff);
        	ng1_setvisual(0);
	}
        REX->set.drawmode1 = dm1;
        REX->set.drawmode0 = DM0_READ | DM0_BLOCK | DM0_DOSETUP | DM0_COLORHOST;
        while(loop-- > 0) {
        REX->set.xystarti = x << 16 | y;
        REX->set.xyendi = x << 16 | y;
        REX3WAIT(REX);
        quad = REX->go.hostrw0;
	if(bitmode == 24)
		msg_printf(5, "Reading pattern=0x%x at location x=%d y=%d\n", quad,x, y);
	else {
        	got = (unsigned char *)(&quad);
        	msg_printf(5, "Reading pattern=0x%x at location x=%d y=%d\n", *got,x, y);
        }
	}
	dm1=tmp_dm1;
	return 0;
}

int ng1wvram(int argc, char **argv)
{
        int x, y;
        unsigned int color;
	int loop = 1;
	int bitmode=0;

	if(!ng1checkboard())
		return -1;
        if (argc < 5) {
                printf("usage: %s x y pattern bitmode[8/24] [nloops]\n", argv[0]);
                return -1;
        }
        atob(argv[1], &x);
        atob(argv[2], &y);
        atohex(argv[3], (int*)&color);
	atob(argv[4], &bitmode);
	
	if (argc > 5)
                atohex(argv[5], &loop);

	if(bitmode == 24) {
	ng1_writemask (0xffffff);
	ng1_setvisual(5);
	REX->set.colori = color;
	}
	else {
        ng1_writemask (0xff);
        ng1_setvisual(0);
        ng1_color(color);
	}
        REX->set.drawmode1 = dm1;

        while(loop-- > 0) {
        REX->set.xystarti = (x << 16 | y);
        REX->go.drawmode0 = DM0_DRAW | DM0_BLOCK;
        msg_printf(5, "Writing pattern=0x%x at location x=%d & y=%d\n", color, x , y);
        REX3WAIT(REX);
        }
	return 0;
}

int ng1test_vram_addr(int argc, char **argv)
{
        int errors = 0, totalerrors = 0;
        int rb24;
        int tmp_dm1;
	/*int ysize = NG1_YSIZE/16;  never used*/

	if(!ng1checkboard())
                return -1;

        rex3init();
        if (ng1_ginfo[REXindex].bitplanes == 24)
                rb24 = 1;
        else
                rb24 = 0;

        msg_printf(VRB, "Starting %d bit VRAM Address Bit tests...\n", rb24 ? 24 : 8);

        tmp_dm1 = dm1;

        dm1 = DM1_RGBPLANES | DM1_COLORCOMPARE | DM1_ENPREFETCH | DM1_LO_SRC;

        if (rb24) {
                dm1 |= DM1_RGBMODE | DM1_DRAWDEPTH24 |
                    DM1_HOSTDEPTH32 | DM1_RWPACKED;
                ng1_writemask (0xffffff);
                ng1_setvisual(5);
                errors=ng1_testaddr24(1);
                errors=ng1_testaddr24(0);
        }
        else {
                dm1 |= DM1_DRAWDEPTH8 | DM1_HOSTDEPTH8 | DM1_RWPACKED;
                ng1_writemask (0xff);
                ng1_setvisual(0);
                errors=ng1_testaddr8(1);
                errors=ng1_testaddr8(0);
        }

        dm1 = tmp_dm1;
        totalerrors += errors;
        if (totalerrors) {
                msg_printf(SUM,"Total errors in VRAM Address Bit test: %d\n", totalerrors);
                return -1;
        } else {
                msg_printf(INFO, "All of the VRAM Address Bit tests have passed.\n");
                return 0;

        }
}

int ng1_testaddr8(int rw)
{
        int x, y;
        uint data;
        unsigned int quad;
        unsigned char datasb;
        unsigned char *datard;
        int errors=0;
        int tmp_dm1;
	int bank_no=0;

        tmp_dm1=dm1;
        if(rw) {
                data=0; errors=0;
                REX->set.drawmode1 = dm1;
                for(y=0; y<NG1_YSIZE; y++) {
                        for(x=0; x <NG1_XSIZE; x++) {
                                ng1_color(data);
                                REX->set.xystarti = (x << 16 | y);
                                REX->go.drawmode0 = DM0_DRAW | DM0_BLOCK;
                                msg_printf(7, "Writing Data = 0x%x at location X=%d and Y=%d\n",data, x, y);
                                if((!(x % 255)) && (x != 0)) {
                                        data= data+2;
                                }
                                else {
                                        data++;
                                }
                        if(!(y % 100))
                                busy(1);
                        REX3WAIT(REX);
                        }
                }
        }
        else {
                data=0; errors=0;
                dm1 = DM1_RGBPLANES | DM1_COLORCOMPARE | DM1_LO_SRC;
                dm1 |= DM1_DRAWDEPTH8 | DM1_HOSTDEPTH8 | DM1_RWPACKED;
                REX->set.drawmode1 = dm1;
                REX->set.drawmode0 = DM0_READ | DM0_BLOCK | DM0_DOSETUP | DM0_COLORHOST;
                for(y=0; y<NG1_YSIZE; y++) {
                        for(x=0; x <NG1_XSIZE; x++) {
                                REX->set.xystarti = x << 16 | y;
                                REX->set.xyendi = x << 16 | y;
                                REX3WAIT(REX);
                                quad = REX->go.hostrw0;
                                datard= (unsigned char *) &quad;
                                datasb =  data & 0xff;
                                if(datasb != *datard) {
                                        errors++;
					bank_no=check_bank(x,y);
                                        msg_printf(7, "Address Error Datasb = 0x%x and Dataread = 0x%x at location X=%d and Y=%d\n",datasb, *datard, x, y);
					print_bank(bank_no);
					print_chip(1, bank_no); /* Always lower chip for 8bit */
                                }
                                if((!(x % 255)) && (x != 0))
                                        data= data+2;
                                else
                                        data++;
                        if(!(y % 100))
                                busy(1);
                        }
                }
        }
        busy(0);
        dm1=tmp_dm1;
        return errors;
}


int ng1_testaddr24(int rw)
{
        int x, y;
        uint data;
        unsigned int quad;
        uint datard;
        int errors=0;
        int tmp_dm1;
        int r, g, b;
        uint color;
	int bank_no=0;

        tmp_dm1=dm1;
        if(rw) {
                data=0; errors=0;
/*              dm1 = DM1_RGBPLANES | DM1_LO_SRC | DM1_RGBMODE | DM1_DRAWDEPTH24 | DM1_HOSTDEPTH32 | DM1_RWPACKED; */
                REX->set.drawmode1 = dm1;
                for(y=0; y<NG1_YSIZE; y++) {
                        for(x=0; x <NG1_XSIZE; x++) {
                                color = y << 12 | x;
                        /*      r= data & 0x00ff0000 >> 16;
                                g= data & 0x0000ff00 >> 8;
                                b= data & 0xff;
                                color=(b << 16) | (g << 8) | r; */
                                /* ng1_rgbcolor(r,g,b); */
                                REX->set.colori = data;
                                REX->set.xystarti = (x << 16 | y);
                                REX->go.drawmode0 = DM0_DRAW | DM0_BLOCK;
                                msg_printf(7, "Writing Data = 0x%x at location X=%d and Y=%d\n",data, x, y);
                                data++;
                        if(!(y % 100))
                                busy(1);
                        REX3WAIT(REX);
                        }
                }
        }
        else {
                data=0; errors=0;
                dm1 = DM1_RGBPLANES | DM1_COLORCOMPARE | DM1_LO_SRC;
                dm1 |= DM1_RGBMODE | DM1_DRAWDEPTH24 |
                   DM1_HOSTDEPTH32 | DM1_RWPACKED;
                REX->set.drawmode1 = dm1;
                REX->set.drawmode0 = DM0_READ | DM0_BLOCK | DM0_DOSETUP | DM0_COLORHOST;
                for(y=0; y<NG1_YSIZE; y++) {
                        for(x=0; x <NG1_XSIZE; x++) {
                                /* r= data & 0x00ff0000 >> 16;
                                g= data & 0x0000ff00 >> 8;
                                b= data & 0xff;
                                color=(b << 16) | (g << 8) | r;
                                */
                                color = y << 12 | x;

                                REX->set.xystarti = x << 16 | y;
                                REX->set.xyendi = x << 16 | y;
                                REX3WAIT(REX);
                                quad = REX->go.hostrw0;
                                datard = quad & 0xffffffff;
                                if(data != datard) {
                                        errors++;
					bank_no=check_bank(x, y);
                                        msg_printf(7, "Address Error Datasb = 0x%x and Dataread = 0x%x at location X=%d and Y=%d\n",data, datard, x, y);
					print_bank(bank_no);
					check_bits(data, datard, bank_no);
                                }
                                data++;
                        if(!(y % 100))
                                busy(1);
                        }
                }
        }
        busy(0);
        dm1=tmp_dm1;
        return errors;
}

#ifdef PVRAM

/*
 * ng1print_vram()
 *
 *	Read a 64 by 20 block of VRAM, mask off the high nibble
 *	and print to the terminal.
 */
static unsigned char ar[] = {
	'.', '1', '2', '3', '4', '5', '6', '7',
	'8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

int ng1print_vram(void)
{
	int x, y;
	int x0, y0;
	unsigned int c;
	int savex, savey;
	int savexy;

	printf("xywin = %x  planes = %x\n", f_xywin, f_planes);
	x0 = (f_xywin >> 16) - 0x800;
	y0 = (f_xywin & 0xffff) - 0x800;
	savex = REX->set.xstart.word;
	savey = REX->set.ystart.word;
	REX->set.xendi = 0x7ff;
	REX->set.yendi = 0x7ff;
	for (y = 19; y >= 0; y--) {
		if ((y+y0) % 5 == 0)
			printf("%4d+", y+y0);
		else
			printf("    |");
		REX->set.ystarti = y;
		for (x = 0; x < 64; x += 4) {
			REX->set.xstarti = x;
			REX->go.command = QUADMODE | rex_LDPIXEL;
			REX->set.command = 0;
			c = REX->set.hostrw0;
			printf("%c%c%c%c", ar[(c>>24)&0xf], ar[(c>>16)&0xf],
			    ar[(c>>8)&0xf], ar[c&0xf]);
		}
		printf("\n");
	}
	printf("    *");
	for (x = 0; x < 64; x++)
		if ((x+x0) % 5 == 0)
			printf("+");
		else
			printf("-");
	printf("\n");
	printf("     ");
	x = 0;
	while ((x+x0) % 5 != 0) {
		printf(" ");
		x++;
	}
	for (;x < 64; x += 5)
		printf("%-5d", x+x0);
	printf("\n");
	REX->set.xstart.word = savex;
	REX->set.ystart.word = savey;
	REX->go.command = rex_NOP;
}


int ng1print_vram1(void)
{
	int x, y;
	int x0, y0;
	unsigned int c;
	int tmp;
	int savex, savey;

	printf("xywin = %x  planes = %x\n", f_xywin, f_planes);
	x0 = (f_xywin >> 16) - 0x800;
	y0 = (f_xywin & 0xffff) - 0x800;
	savex = REX->set.xstart.word;
	savey = REX->set.ystart.word;
	REX->set.xendi = 0x7ff;
	REX->set.yendi = 0x7ff;
	for (y = 19; y >= 0; y--) {
		if ((y+y0) % 5 == 0)
			printf("%4d+", y+y0);
		else
			printf("    |");
		REX->set.ystarti = y;
		for (x = 0; x < 64; x++) {
			REX->set.xstarti = x;
			REX->set.command = rex_LDPIXEL;
			tmp = REX->go.hostrw0;
			c = REX->set.hostrw0;
			printf("%c", ar[(c>>24)&0xf]);
		}
		printf("\n");
	}
	printf("    *");
	for (x = 0; x < 64; x++)
		if ((x+x0) % 5 == 0)
			printf("+");
		else
			printf("-");
	printf("\n");
	printf("     ");
	x = 0;
	while ((x+x0) % 5 != 0) {
		printf(" ");
		x++;
	}
	for (;x < 64; x += 5)
		printf("%-5d", x+x0);
	printf("\n");
	REX->set.xstart.word = savex;
	REX->set.ystart.word = savey;
	REX->go.command = rex_NOP;
}

#endif

int
ng1sptest_palette( int chip, int cblink, int nloop)
{
	int i, tmp;
	int j, k;
	char *r, *g, *b;
	char *rsave, *gsave, *bsave;
	int errors = 0;
	int data, rdata, gdata, bdata;
	int palettesize;

	VRINTWAIT(REX);
	VRINTWAIT(REX);

	palettesize = 8192;

	if ((r = malloc (palettesize * 3)) == NULL) {
		msg_printf (ERR," Out of memory in palette test\n");
		return -1;
	}
	g = r + palettesize;
	b = g + palettesize;

	if((rsave = malloc (256 *3)) == NULL) {
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
	for (i = 0; i < 256; i++) {
		get_palette_rgb (chip, rsave + i, gsave + i, bsave + i);
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
			if ((i & 0x1F) == 0x1F) {
				cmapFIFOWAITCHIP(REX, chip);
				set_palette_addr (chip, i);
				set_palette_addr (chip, i);
			}
		if(cblink)
			cmapSetReg (REX, CMAP_CRS_CMD_REG, 0x07);
			set_palette_addr (chip, i);
			set_palette_rgb (chip, rdata, gdata, bdata);
		if(cblink)
 			cmapSetReg (REX, CMAP_CRS_CMD_REG, 0x03); 
		}
		/*
		 * Read the color table.  Must turn
		 * off pixel clock before reading cmap palette.
		 */
		BFIFOWAIT(REX);
		VRINTWAIT(REX);
		cmapFIFOEMPTY(REX);
		/* Dummy Read */
		set_palette_read_addr (chip, 8191);
                get_palette_rgb (chip, r , g , b );

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
		/* Write location 1100 to 1356 for nloop times */
		msg_printf(3,"Writing location 1100 to 1356 for %d times\n", nloop);
                for (j = 0; j < nloop; j++) {
			for(k=0; k <256; k++) {
                        	if ( ( k & 0x0F ) == 0x0F )
                                	BFIFOWAIT( REX );
                        	if ( ( k & 0x1F ) == 0x1F ) {
					cmapFIFOWAITCHIP(REX, chip);
                			set_palette_addr (chip, 1100 + k);
                			set_palette_addr (chip, 1100 + k);
                        	}
			if(cblink)
 				cmapSetReg (REX, CMAP_CRS_CMD_REG, 0x07);
                	set_palette_addr (chip, 1100 + k);
                        set_palette_rgb (chip, k & 0xff, k ^ 0x55, k ^ 0xff);
			if(cblink)
                		cmapSetReg (REX, CMAP_CRS_CMD_REG, 0x03);
			}
		}

	/* Read the whole 8K color table */

                BFIFOWAIT(REX);
                VRINTWAIT(REX);
                cmapFIFOEMPTY(REX);
		/* Dummy Read */
                set_palette_read_addr (chip, 8191);
                get_palette_rgb (chip, r, g, b);

                for (i = 0; i < palettesize; i++) {
                        get_palette_rgb (chip, r + i, g + i, b + i);
                }

/* Compare the data from location 0x0 to 1100 and from 1356 to 0x8192 */

		for (i = 0; i < 1100; i++) {
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
		for (i = 1356; i < palettesize; i++) {
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

	 /* Restore the original color table */
	for (i = 0; i < 256; i++) {
		if ( ( i & 0x0F ) == 0x0F ) 
			BFIFOWAIT( REX );
		if ( ( i & 0x1F ) == 0x1F ) {
			cmapFIFOWAITCHIP(REX, chip);
			set_palette_addr (chip, i);
			set_palette_addr (chip, i);
		}
		if(cblink)
			cmapSetReg (REX, CMAP_CRS_CMD_REG, 0x07);
		set_palette_addr (chip, i);
		set_palette_rgb (chip, *(rsave + i), *(gsave + i), *(bsave + i));
		if(cblink)
			cmapSetReg (REX, CMAP_CRS_CMD_REG, 0x03);
	}
	free (r);
	free (rsave);
	return errors;
}
int ng1spcmaptest(int argc, char **argv)
{
	int i;
	int errors = 0;
	int nloop = 1000;
	int cblink = 0;

	if(!ng1checkboard())
		return -1;

        if (argc < 2) {
                printf("usage: %s cblank[1/0] [nloop]\n",argv[0]);
		printf("cblank=1 (set). Default nloop=1000\n");
                return -1;
        }

	atob(argv[1], &cblink);
	if (argc > 2)
		atob(argv[2], &nloop);

	rex3init();

	msg_printf(VRB, "Starting CMAP Special test ...\n");

	if(!(ng1_ginfo[REXindex].cmaprev))
		msg_printf(VRB, "Skipping CMAP Special Palette test for CMAP version 000.\n");
	else {
		errors += ng1sptest_palette(CMAP0, cblink, nloop);
		errors += ng1sptest_palette(CMAP1, cblink, nloop);
	}

        Ng1RegisterInit(REX, &ng1_ginfo[REXindex]);

	rex3init();

	if (errors) {
		msg_printf(SUM,"Total errors in CMAP Special test: %d\n", errors);
		return -1;
	}
	else {
		msg_printf(INFO, "All of the CMAP Special tests have passed.\n");
		return 0;
	}
}

/* This function saves the first 256 entries of the color table and 
   loads it back depending on the flag rw.
*/
save_cmaptbl(int chip, int rw)
{
	char *rsave, *gsave, *bsave;
	int i;
	
	VRINTWAIT(REX);
	if(rw) {
		if((rsave = malloc (256 *3)) == NULL) {
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

		for (i = 0; i < 256; i++) {
			get_palette_rgb (chip, rsave + i, gsave + i, bsave + i);
		}
	}
	else {
		/* Restore the original color table */
		cmapSetReg (REX, CMAP_CRS_CMD_REG, 7);
		set_palette_addr (chip, 0);
		for (i = 0; i < 256; i++) {
			if ( ( i & 0x0F ) == 0x0F ) {
				BFIFOWAIT( REX );
			}
			set_palette_rgb (chip, *(rsave + i), *(gsave + i), *(bsave + i));
		}
		cmapSetReg (REX, CMAP_CRS_CMD_REG, 3);
		free (rsave);
	}

}

int
ng1rdtest_palette( int chip )
{
        int i, tmp;
        int j;
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

        if ((r = malloc (palettesize * 3)) == NULL) {
                msg_printf (ERR," Out of memory in palette test\n");
                return -1;
        }
        g = r + palettesize;
        b = g + palettesize;

        if((rsave = malloc (256 *3)) == NULL) {
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
        for (i = 0; i < 256; i++) {
                get_palette_rgb (chip, rsave + i, gsave + i, bsave + i);
        }

        msg_printf(7, "Loading color table for 8bit\n");
        for (data = 0; data < 0x100; data += 0x55) {
                rdata = data;
                gdata = data ^ 0x55;
                bdata = data ^ 0xff;

                msg_printf(5, "Loading pattern Rdata=%#02x, Gdata=%#02x, Bdata=%#02x for CMAP%d\n", rdata, gdata, bdata, chip);
                /*
                 * Load the color table
                 */
                for (i = 0; i < palettesize; i++) {
                        if ( ( i & 0x0F ) == 0x0F ) 
                                BFIFOWAIT( REX );
                        if ( ( i & 0x1F ) == 0x1F ) {
                        	cmapFIFOWAITCHIP(REX, chip);
                        	set_palette_addr (chip, i);
                        	set_palette_addr (chip, i);
                        }
                cmapSetReg (REX, CMAP_CRS_CMD_REG, 0x07);
                set_palette_addr (chip, i);
                set_palette_rgb (chip, rdata, gdata, bdata);
                cmapSetReg (REX, CMAP_CRS_CMD_REG, 0x03);
                }

                /*
                 * Read the color table.  Must turn
                 * off pixel clock before reading cmap palette.
                 */
                for(j=0; j<5; j++) {
                	msg_printf(7,"Reading Color Table = %d\n", j);
                	BFIFOWAIT(REX);
                	VRINTWAIT(REX);
                	cmapFIFOEMPTY(REX);
                	set_palette_read_addr (chip, 8191);
                	get_palette_rgb (chip, r , g , b );
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
                	if(!errors)
                        	break;
                }

	}
         /* Restore the original color table */
        for (i = 0; i < 256; i++) {
                if ( ( i & 0x0F ) == 0x0F ) 
                        BFIFOWAIT( REX );
                if ( ( i & 0x1F ) == 0x1F ) {
                	cmapFIFOWAITCHIP(REX, chip);
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

int ng1rdcmaptest()
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

        msg_printf(VRB, "Starting CMAP Special Read tests ...\n");

        if(!(ng1_ginfo[REXindex].cmaprev))
                msg_printf(VRB, "Skipping CMAP Palette test for CMAP version 000.\n");
        else {
                errors += ng1rdtest_palette(CMAP0);
                errors += ng1rdtest_palette(CMAP1);
        }

        Ng1RegisterInit(REX, &ng1_ginfo[REXindex]);

        rex3init();

        if (errors) {
                msg_printf(SUM,"Total errors in CMAP special Read test: %d\n", errors);
                return -1;
        }
        else {
                msg_printf(INFO, "All of the CMAP Special read tests have passed.\n");
                return 0;
        }
}

int ng1debugprobe(int argc, char **argv) {
	extern int debug_probe;
	int i;

	if (argc < 2) {
		printf ("Usage : %s 0|1\n", argv[0]);
                return 1;
        }
        atob (argv[1], &i);
	debug_probe=i;
	return 0;
}

int ng1init(int argc, char **argv)
{
	if(!ng1checkboard())
                return -1;

	if (argc < 2) {
                printf("usage: %s chipname[bt445|vc2|xmap9|cmap|rex3|planes|cursor]\n",argv[0]);
                return -1;
	}


	if (!strcmp("bt445", argv[1]))
		Ng1DacInit(REX, &ng1_ginfo[REXindex]);
	else if (!strcmp("vc2", argv[1]))
		Ng1VC2Init(REX, &ng1_ginfo[REXindex]);
	else if (!strcmp("xmap9", argv[1]))
		Ng1XmapInit(REX, &ng1_ginfo[REXindex]);
	else if (!strcmp("cmap", argv[1]))
		Ng1CmapInit(REX, &ng1_ginfo[REXindex]);
	else if (!strcmp("rex3", argv[1]))
		Ng1RegisterInit(REX, &ng1_ginfo[REXindex]);
	else if (!strcmp("planes", argv[1]))
		rex3Clear(REX, &ng1_ginfo[REXindex]);
	else if (!strcmp("cursor", argv[1]))
		Ng1CursorInit(REX, 0xff, 0, 0, 0xff, 0xff, 0xff);

	else
		msg_printf(INFO,"Unrecognized chipname: %s\n", argv[1]);
}

int
ng1spfastclear(int argc, char **argv)
{
	int tmp_dm1;
	int x0, y0, x1, y1;
	unsigned int colorvram;
	int xinc;
	int x, y;
	unsigned int quad;
	int errors = 0;
	int rb24;

	if(!ng1checkboard())
		return -1;

	if (argc < 6) {
		printf("usage: %s colorvram x0 y0 x1 y1\n", argv[0]);
		return -1;
	}
	tmp_dm1 = dm1;

	atobu(argv[1], &colorvram);
	atob(argv[2], &x0);
	atob(argv[3], &y0);
	atob(argv[4], &x1);
	atob(argv[5], &y1);

	if (ng1_ginfo[REXindex].bitplanes == 24)
		rb24 = 1;
	else
		rb24 = 0;

	 dm1 = DM1_RGBPLANES | DM1_COLORCOMPARE | DM1_ENPREFETCH | DM1_LO_SRC; 
	if (rb24) {
                dm1 |= DM1_RGBMODE | DM1_DRAWDEPTH24 |
                    DM1_HOSTDEPTH32 | DM1_RWPACKED;
                ng1_writemask (0xffffff);
                ng1_setvisual(5);
                xinc = 1;
        }
        else {
		if(colorvram > 255) {
		msg_printf(VRB, "On 8-bit systems use colorvram value lower than 255.\n");
		return 0;
		
		}
                dm1 |= DM1_DRAWDEPTH8 | DM1_HOSTDEPTH8 | DM1_RWPACKED;
                ng1_writemask (0xff);
                ng1_setvisual(0);
                xinc = 4;

        }

	/* Fill the rectangle */

	REX->set.drawmode1 = dm1;
	REX->set.colorvram = colorvram; 
	ng1_fastclear(1);
	REX->set.drawmode1 = dm1;
	REX->set.drawmode0 = DM0_DRAW | DM0_BLOCK | DM0_DOSETUP | DM0_STOPONXY ;
	REX->set.xyendi = x1 << 16 | y1;
	REX->go.xystarti = x0 << 16 | y0;
	REX3WAIT(REX);

	/* Read the data back and compare */
	ng1_fastclear(0);
	REX->set.drawmode1 = dm1;
	REX->go.drawmode0 = DM0_READ | DM0_BLOCK | DM0_DOSETUP | DM0_COLORHOST;
	REX3WAIT(REX);
        REX->set.xystarti = x0 << 16 | y0;
	REX->set.xyendi = x1 << 16 | y1;

	        /* Dummy read for Prefetch */
        REX->go.hostrw0;
        for (y = y0; y < y1; y++) {
                for (x = x0; x < x1; x += xinc) {
                        quad = REX->go.hostrw0;
                        if (rb24) {
				if(v24err(x,y,quad,colorvram))
					errors++;
                        } else {
				if (v8err(x,y,quad,colorvram))
					errors++;
                        }
                }
	}
	dm1 = tmp_dm1;
	if(errors) {
		msg_printf(SUM,"Total errors in FASTCLEAR test: %d\n", errors);
		return -1;
	} else {
		msg_printf(INFO, "FASTCLEAR test passed.\n");
		return 0;
	}
}
