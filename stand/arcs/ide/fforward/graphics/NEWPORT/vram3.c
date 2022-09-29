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
 *  VRAM Diagnostics
 *  $Revision: 1.18 $
 */

#include "sys/cpu.h"
#include "sys/sbd.h"
#include <sys/param.h>
#include "ide_msg.h"
#include "gl/rex3macros.h"
#include "sys/ng1hw.h"
#include <sys/types.h>
#include <sys/sema.h>
#include <sys/gfx.h>
#include <sys/ng1.h>
#include "sys/ng1hw.h"

#include "sys/cmn_err.h"
#include "sys/invent.h"
#include "sys/ng1_cmap.h"
#include "sys/vc2.h"
#include "sys/xmap9.h"
#include "local_ng1.h"
#include "libsk.h"
#include "libsc.h"
#include "uif.h"

extern Rex3chip *REX;
extern int REXindex;
/*extern int *Reportlevel;*/

extern int dm1;
extern struct ng1_info ng1_ginfo[];


/* check_bits()
 * This function compares the bits of expect & got. For each
 * bit which is different it calls check_chip() to find the
 * vram chip_no for that bit.
 */
int
check_bits(unsigned int expect, unsigned int got, int bank_no) 
{
	int i;
	int code;
	int cbit;
	unsigned int result;
	int chip_no=0;

	result = expect ^ got;
	for(i=0; i<24; i++) {
		if((result >> i) & 0x0001) {
			if (i <= 7){
				code = 1;
				cbit = i;
			}
			else if ((i > 7) && (i <= 15)) {
				code = 2;
				cbit = i - 8;
			}
			else {
				code = 3;
				cbit = i - 16;
			}
		chip_no = check_chip(cbit, code);
		print_chip(chip_no, bank_no);
		}
	}
}

/* check_chip()
 * This function takes cbit (0-7) for each color code (r,g,b).
 * It returns the vram chip_no (1: Lower, 2: Middle & 3: Upper)
 * for the color bit.
 */
int
check_chip(int cbit, int code)
{
	int chip_no=0;

	if((cbit == 0) || (cbit == 1))
		chip_no = 3;
	else if((cbit == 3) || (cbit == 4))
		chip_no = 2;
	else if((cbit == 6) || (cbit == 7))
		chip_no = 1;
	else if(cbit == 2) {
		if((code == 1) || (code == 3))
			chip_no = 3;
		else
			chip_no = 2;
	}
	else if(cbit == 5) {
		if((code == 1) || (code == 2))
			chip_no = 1;
		else
			chip_no = 2;
	}
	return chip_no;
}

/* check_bank()
 * This function returns the vram sub bank_no for
 * the pixel on the screen at location x & y.
 */
int
check_bank(unsigned int x, unsigned int y)
{
	unsigned int xmod, ymod;
	unsigned int bank_no=0;

	xmod = x & 0x0007;
	ymod = y & 0x0003;

	switch (ymod) {
		case 0:
			bank_no = (xmod + 8) & 0x0007;
			break;
		case 1:
			bank_no = (xmod + 2) & 0x0007;
			break;
		case 2:
			bank_no = (xmod + 4) & 0x0007;
			break;
		case 3:	
			bank_no = (xmod + 6) & 0x0007;
			break;
	}
	return(bank_no);
}

/* print_bank()
 * Converts the sub bank_no to sub bank name & prints it.
 */
int
print_bank(int bank_no)
{
	char *bank_name;

	switch (bank_no) {
		case 0:
			bank_name = "A0";
			break;
		case 1:
			bank_name = "A1";
			break;
		case 2:
			bank_name = "B0";
			break;
		case 3:
			bank_name = "B1";
			break;
		case 4:
			bank_name = "C0";
			break;
		case 5:
			bank_name = "C1";
			break;
		case 6:
			bank_name = "D0";
			break;
		case 7:
			bank_name = "D1";
			break;
	}
	msg_printf(VRB, "Sub Bank= %s\n", bank_name);
}

/* print_chip()
 * Converts the chip_no to chip name & prints it.
 */
int
print_chip(int chip_no, int bank_no) 
{
	char *chip_name;
        if(bank_no==0 || bank_no==1) {
        switch (chip_no) {
                case 1:
                        chip_name = "Lower:U17";
                        break;
                case 2:
                        chip_name = "Middle:U14";
                        break;
                case 3:
                        chip_name = "Upper:U11";
                        break;
        }
        }
        else if(bank_no==2 || bank_no==3) {
        switch (chip_no) {
                case 1:
                        chip_name = "Lower:U18";
                        break;
                case 2:
                        chip_name = "Middle:U15";
                        break;
                case 3:
                        chip_name = "Upper:U12";
                        break;
        }
        }
        else if(bank_no==4 || bank_no==5) {
        switch (chip_no) {
                case 1:
                        chip_name = "Lower:U518";
                        break;
                case 2:
                        chip_name = "Middle:U512";
                        break;
                case 3:
                        chip_name = "Upper:U506";
                        break;
        }
        }
        else if(bank_no==6 || bank_no==7) {
        switch (chip_no) {
                case 1:
                        chip_name = "Lower:U517";
                        break;
                case 2:
                        chip_name = "Middle:U511";
                        break;
                case 3:
                        chip_name = "Upper:U505";
                        break;
        }
        }
	msg_printf(VRB, "Vram Chip= %s\n", chip_name);
}


int ng1test_vram(int argc, char **argv)
{
	int x, y;
	int i, j, k;
	unsigned int color;
	unsigned int r, g, b;
	unsigned int quad;
	unsigned int got;
	int errors = 0, totalerrors = 0;
	int xinc;
	int ysize;
	int rb24;
	int tmp_dm1;
	uint bank_no=0;							
	int stop_at_50;
	int fk;

	stop_at_50 = console_is_gfx();

	ysize = NG1_YSIZE/16;
	if(!ng1checkboard())
		return -1;
	rex3init();
	if (ng1_ginfo[REXindex].bitplanes == 24)
		rb24 = 1;
	else
		rb24 = 0;

	msg_printf(VRB, "Starting %d bit VRAM tests...\n", rb24 ? 24 : 8);

	tmp_dm1 = dm1;
	dm1 = DM1_RGBPLANES | DM1_COLORCOMPARE | DM1_ENPREFETCH | DM1_LO_SRC;
	if (rb24) {
		dm1 |= DM1_RGBMODE | DM1_DRAWDEPTH24 |
		    DM1_HOSTDEPTH32 | DM1_RWPACKED;
		ng1_writemask (0xffffff);
		ng1_setvisual(5);
		xinc = 1;
	}
	else {
		dm1 |= DM1_DRAWDEPTH8 | DM1_HOSTDEPTH8 | DM1_RWPACKED;
		ng1_writemask (0xff);
		ng1_setvisual(0);
		xinc = 4;
	}

	for (i = 3; i >= 0 && totalerrors < 50; i--) {

		if (rb24) {
			r = (0x55 * i) & 0xff;
			g = r ^ 0x55;
			b = r ^ 0xff;
			color = (b << 16) | (g << 8) | r;
			ng1_rgbcolor(r, g, b);
		}
		else {
 			color = 0x55*i;
			ng1_color(color);
		}
		msg_printf(VRB, "Filling VRAM with %x...", color);

		/* Fill the frame buffer from color registers */
		ng1_block(0, 0, NG1_XSIZE-1 , NG1_YSIZE-1, 0, 0);

		/* Set up a block read from the frame buffer thru hostrw0 */
		ng1_block(0, 0, NG1_XSIZE-1 , NG1_YSIZE-1, 
		    DM0_COLORHOST, 0);

		REX->set.drawmode1 = dm1;
		REX->go.drawmode0 = DM0_READ | DM0_BLOCK |
		    DM0_DOSETUP | DM0_COLORHOST;
		 REX3WAIT(REX);
		if (rb24){
		    for (y = 0; y < NG1_YSIZE; y++) {
			for (x = 0; x < NG1_XSIZE; x += xinc) {
				quad = REX->go.hostrw0;
					bank_no=0;
					if (quad != color) {
						if(*Reportlevel > 4)
							bank_no=check_bank(x, y);
						msg_printf(VRB, "Error reading VRAM x %x y %x, "
		    				"read: %#06x, expected: %#06x\n",
		    				x, y, got, color);
						if(*Reportlevel > 4) {
							print_bank(bank_no);
							check_bits(color, got, bank_no);
							}
					errors++;
					if (stop_at_50 && errors > 50) { 
						x=NG1_XSIZE; 
						y=NG1_YSIZE;
						}
					}
				}
				if(!(y % 100))
				busy(1);
				}
			} else {
		    for (y = 0; y < NG1_YSIZE; y++) {
			for (x = 0; x < NG1_XSIZE; x += xinc) {
				quad = REX->go.hostrw0;
			bank_no=0;							
			for(fk=0; fk<4; fk++){						
				got = quad & 0xff;					
				if (got != color) {					
				    if(*Reportlevel > 4) bank_no=check_bank(x+3-k, y);		
				msg_printf(VRB, "Error reading VRAM x %x y %x, "
			    	"read: %#02x, expected: %#02x\n",x+3-k, y, got, color);
			        if(*Reportlevel > 4) {				
				     print_bank(bank_no);			
				     print_chip(1, bank_no); 		
			             }						
		                errors++;					
				if (stop_at_50 && errors > 50) { 
					x=NG1_XSIZE; 
					y=NG1_YSIZE;
					}
		                }							
		        quad = quad >> 8;					
	        	}								
					}
				if(!(y % 100))
				busy(1);
				}
			}
	
		msg_printf (VRB, " %d errors this pass\n",errors);
		totalerrors += errors;
	}
	msg_printf (VRB, "Testing screen with walking 1's\n");
	for (i = 0; i < 8 && totalerrors < 50; i++) {

                if (rb24) {
                        r = (0x1 << i) & 0xff;
                        g = r ^ 0x55;
                        b = r ^ 0xff;
                        color = (b << 16) | (g << 8) | r;
                        ng1_rgbcolor(r, g, b);
                }
                else {
                        color = 0x1 << i;
                        ng1_color(color);
                }
              msg_printf(VRB, "Filling VRAM with %x...", color);

                /* Fill the frame buffer from color registers */
                ng1_block(0, 0, NG1_XSIZE-1 , NG1_YSIZE-1, 0, 0);

                /* Set up a block read from the frame buffer thru hostrw0 */
                ng1_block(0, 0, NG1_XSIZE-1 , NG1_YSIZE-1,
                    DM0_COLORHOST, 0);

                REX->set.drawmode1 = dm1;
                REX->go.drawmode0 = DM0_READ | DM0_BLOCK |
                    DM0_DOSETUP | DM0_COLORHOST;
		REX3WAIT(REX);
                if (rb24){
                    for (y = 0; y < NG1_YSIZE; y++) {
                        for (x = 0; x < NG1_XSIZE ; x += xinc) {
                                quad = REX->go.hostrw0;
					bank_no=0;
					if (quad != color) {
						if(*Reportlevel > 4)
							bank_no=check_bank(x, y);
						msg_printf(VRB, "Error reading VRAM x %x y %x, "
		    				"read: %#06x, expected: %#06x\n",
		    				x, y, got, color);
						if(*Reportlevel > 4) {
							print_bank(bank_no);
							check_bits(color, got, bank_no);
							}
					errors++;
					if (stop_at_50 && (errors+totalerrors) > 50) { 
						x=NG1_XSIZE; 
						y=NG1_YSIZE;
						}
					}
				}
				if(!(y % 100))
				busy(1);
			}
                } else {
                    for (y = 0; y < NG1_YSIZE; y++) {
                        for (x = 0; x < NG1_XSIZE ; x += xinc) {
                                quad = REX->go.hostrw0;
			bank_no=0;							
			for(k=0; k<4; k++){						
				got = quad & 0xff;					
				if (got != color) {					
				    if(*Reportlevel > 4) bank_no=check_bank(x+3-k, y);		
				msg_printf(VRB, "Error reading VRAM x %x y %x, "
			    	"read: %#02x, expected: %#02x\n",x+3-k, y, got, color);
			        if(*Reportlevel > 4) {				
				     print_bank(bank_no);			
				     print_chip(1, bank_no); 		
			             }						
		                errors++;					
				if (stop_at_50 && (errors+totalerrors) > 50) { 
					x=NG1_XSIZE; 
					y=NG1_YSIZE;
					}
		                }							
		        quad = quad >> 8;					
	        	}								
                                }
			if(!(y % 100))
				busy(1);
                        }
		     }
                msg_printf (VRB, " %d errors in walking 1's\n",errors);
                totalerrors += errors;
                errors = 0;
        }


	msg_printf (VRB, "Testing small chunks for some reason\n");

   if (totalerrors < 50) {   /* a */
	for (y = 0; y < NG1_YSIZE; y += ysize) {  /* b */
		for (x = 0; x < NG1_XSIZE ; x += 64) {  /* c */

			if (rb24) {
				r = x/5; g = y/4; b = r^g;
				    color = (b << 16) | (g << 8) | r;
				    ng1_rgbcolor(r, g, b);
			} else {
				r = x/80; g = y/64;
				    color = (r << 4) | g;
				    ng1_color(color);
			}

			ng1_block(x, y, x+63, y+ysize-1, 0, 0);

			ng1_block(x, y, x+63, y+ysize-1, DM0_COLORHOST, 0);
			REX->go.drawmode0 = DM0_READ | DM0_BLOCK |
			    			DM0_DOSETUP | DM0_COLORHOST;
			REX3WAIT(REX);
			if (rb24){  /*d */
			for (j=x; j< x+64; j+=xinc) {
				for (k = y; k < y+ysize; k++) {
					quad = REX->go.hostrw0;
					bank_no=0;
					if (quad != color) {
						if(*Reportlevel > 4)
							bank_no=check_bank(x, y);
						msg_printf(VRB, "Error reading VRAM x %x y %x, "
		    				"read: %#06x, expected: %#06x\n",
		    				x, y, got, color);
						if(*Reportlevel > 4) {
							print_bank(bank_no);
							check_bits(color, got, bank_no);
							}
					errors++;
					if (stop_at_50 && ( (errors+totalerrors) > 50) ) { 
						x=NG1_XSIZE; 
						y=NG1_YSIZE;
						}
					} /* not color */
				    }  /* for k */
				if(!(y % 100))
				busy(1);
				} /* for j */
			} else {   /* d */
			for (j=x; j< x+64; j+=xinc) {
				for (k = y; k < y+ysize; k++) {
                                	quad = REX->go.hostrw0;

					/* previously function call: v8err */
					bank_no=0;							
					for(fk=0; fk<4; fk++){						
						got = quad & 0xff;					
						if (got != color) {					
				    			if(*Reportlevel > 4) bank_no=check_bank(x+3-k, y);		
							msg_printf(VRB, "Error reading VRAM x %x y %x, "
			    				"read: %#02x, expected: %#02x\n",x+3-k, y, got, color);
			        			if(*Reportlevel > 4) {				
				     				print_bank(bank_no);			
				     				print_chip(1, bank_no); 		
			             				}						
		                			errors++;					
							if (stop_at_50 && ( (errors+totalerrors) > 50) ) { 
								x=NG1_XSIZE; 
								y=NG1_YSIZE;
								}
		                			}							
		        			quad = quad >> 8;					
	        				}  /* for k */								
					/* end of function call */
					}
				if(!(y % 100))
				busy(1);
				}
			} /* d */
		}  /* c */
	}  /* b */
	msg_printf (VRB, "%d errors in small chunks\n", errors);
	totalerrors += errors;
    } /* a */
    busy(0);
    dm1 = tmp_dm1;
    if (totalerrors) {
		msg_printf(SUM,"Total errors in VRAM test: %d\n", totalerrors);
		return -1;
	} else {
		msg_printf(INFO, "All of the VRAM tests have passed.\n");
		return 0;
	}
}


int
ng1fbdepth (int argc, char **argv)
{
	int depth;
	int xtmp, rtmp;

	if(!ng1checkboard())
		return -1;

	if (argc < 2) {
		printf("Usage: %s 8 or 24\n", argv[0]);
		return -1;
	}
	atob(argv[1], &depth);
	
	BFIFOWAIT (REX);
	REX3WAIT (REX);

	xtmp = xmapConfig (REX);
	
	if (depth == 8) {
		xtmp |= XM9_8_BITPLANES;
	}
	else if (depth == 24) {
		xtmp &= ~XM9_8_BITPLANES;
	}
	else {
		printf ("ng1fbdepth: depth must be 8 or 24, not %d\n",depth);
		return -1;
	}
	xmap9LoadConfig (REX, xtmp);
	REX3WAIT (REX);
	ng1_ginfo[REXindex].bitplanes = depth;
}

int
ng1displayfb()
{
	if(!ng1checkboard())
		return -1;

	msg_printf(3, "Using fbdepth bitplanes = %d\n", ng1_ginfo[REXindex].bitplanes);
}

int
ng1buswidth (int argc, char **argv)
{
        int width;
        int xtmp;
	int mask = GIO64_ARB_GRX_SIZE_64 << Ng1Index(REX);
	extern u_int _gio64_arb;
	u_int gio64_arb_save;

	if(!ng1checkboard())
		return -1;

        if (argc < 2) {
                printf("Usage: %s 32 or 64\n", argv[0]);
                return -1;
        }
        atob(argv[1], &width);

        BFIFOWAIT (REX);
        REX3WAIT (REX);

        xtmp = REX->p1.set.config;

        if (width == 32) {
		xtmp &= ~BUSWIDTH;
        }
        else if (width == 64) {
		xtmp |= BUSWIDTH;
        }
        else {
                printf ("buswidth: width must be 32 or 64, not %d\n",width);
                return -1;
        }

	REX->p1.set.config = xtmp;
	gio64_arb_save = _gio64_arb;
	if (xtmp & BUSWIDTH)
		_gio64_arb |= mask;
	else
		_gio64_arb &= ~mask;
	*(unsigned int *)(PHYS_TO_K1(GIO64_ARB)) = _gio64_arb;

}
