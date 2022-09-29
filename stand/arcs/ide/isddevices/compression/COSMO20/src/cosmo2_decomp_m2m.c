/*
 *
 * Cosmo2 gio 1 chip hardware definitions
 *
 * Copyright 1995, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#include "cosmo2_defs.h"
#include "cosmo2_mcu_def.h"

#include <arcs/io.h>
#include <arcs/errno.h>

extern UBYTE checkDataInFifo( UBYTE );

void cosmo2_setup_050_decomp ( __uint32_t addr)
{

	mcu_WRITE( addr+Z50_HARDWARE, Z50_MSTR );

	/* are we decompressing ? */
	mcu_WRITE(addr+Z50_MODE, 0 );
	
	mcu_WRITE(addr+Z50_OPTIIONS, 0 );
	
	mcu_WRITE(addr+Z50_INT_REQ_0, 0 );
	mcu_WRITE(addr+Z50_INT_REQ_1, 0 );

}

void cosmo2_setup_016_decomp ( 
		__uint32_t  addr , UWORD horz, UWORD vert, UBYTE lossless)
{
	UBYTE value;
    value = (UBYTE) (~MODE_COMPRESS) ;
    value = (DSPY_422PXOUT | MODE_422PXIN_422050) & value ;
    mcu_WRITE(addr + Z16_MODE_REG , value);

    DelayFor(10000);
    mcu_WRITE (addr + Z16_ADDR_PTR_REG, Z16_SETUP_PARAMS1);

    if (lossless)
    mcu_WRITE (addr + Z16_INDIR_DATA_REG, SETUP_RSTR );
    else
    mcu_WRITE (addr + Z16_INDIR_DATA_REG,   SETUP_CNTI  );

    DelayFor(10000);
    mcu_WRITE (addr + Z16_ADDR_PTR_REG, Z16_SETUP_PARAMS2);
    mcu_WRITE (addr + Z16_INDIR_DATA_REG, SETUP2_SYEN );

    DelayFor(10000);
    mcu_WRITE (addr + Z16_ADDR_PTR_REG, Z16_NAX_LO);
    mcu_WRITE (addr + Z16_INDIR_DATA_REG, 0);

    DelayFor(10000);
    mcu_WRITE (addr + Z16_ADDR_PTR_REG, Z16_NAX_HI);
    mcu_WRITE (addr + Z16_INDIR_DATA_REG, 0);

    DelayFor(10000);
    mcu_WRITE (addr + Z16_ADDR_PTR_REG, Z16_NAY_LO);
    mcu_WRITE (addr + Z16_INDIR_DATA_REG, 0);

    DelayFor(10000);
    mcu_WRITE (addr + Z16_ADDR_PTR_REG, Z16_NAY_HI);
    mcu_WRITE (addr + Z16_INDIR_DATA_REG, 0);

    DelayFor(10000);
    mcu_WRITE (addr + Z16_ADDR_PTR_REG, Z16_PAX_LO);
    value = horz & 0xff ;
    msg_printf(VRB, "horz LO %x \n", value);
    mcu_WRITE (addr + Z16_INDIR_DATA_REG, value);

    DelayFor(10000);
    mcu_WRITE (addr + Z16_ADDR_PTR_REG, Z16_PAX_HI);
    value = (horz & 0xff00) >> 8 ; ;
    msg_printf(VRB, "horz HI %x \n", value);
    mcu_WRITE (addr + Z16_INDIR_DATA_REG, value);

    DelayFor(10000);
    mcu_WRITE (addr + Z16_ADDR_PTR_REG, Z16_PAY_LO);
    value = vert & 0xff ;
    msg_printf(VRB, "vert LO %x \n", value);
    mcu_WRITE (addr + Z16_INDIR_DATA_REG, value);

    DelayFor(10000);
    mcu_WRITE (addr + Z16_ADDR_PTR_REG, Z16_PAY_HI);
    value = (vert & 0xff00) >> 8 ;
    msg_printf(VRB, "vert HI %x \n", value);
    mcu_WRITE (addr + Z16_INDIR_DATA_REG, value);

}

void cosmo2_setup_upc_decomp ( __uint32_t  addr, UWORD Horz)
{

	UBYTE value; UWORD horz;

    mcu_WRITE (addr + UPC_FLOW_CONTROL, 0);

    mcu_WRITE(addr + UPC_GENESIS_ACCESS, 0);

    mcu_WRITE_WORD ( addr + UPC_PIXEL_PER_LINE_REG,  (Horz - 1) );

    horz = mcu_READ_WORD(addr + UPC_PIXEL_PER_LINE_REG );
    msg_printf(VRB, "UPC_PXL_LINE %x%x\n", (horz&0xff00)>>8, (horz&0xff));

    mcu_WRITE (addr + UPC_FLOW_CONTROL, UPC_DIR_TOGIO);

    value = mcu_READ (addr + UPC_FLOW_CONTROL );
    value = value | UPC_RUN_ENB ;
    mcu_WRITE (addr + UPC_FLOW_CONTROL, value);

    mcu_WRITE (addr + CPC_CTL_STATUS, CPC_RUN_ENB);

}

void print_fbc_registers (__uint32_t  fbcbase)
{

	UWORD value;
    value = mcu_READ_WORD(FBC_H_ACTIVEG_END + fbcbase);
    msg_printf(VRB, " FBC_H_ACTIVEG_END %x \n", value);
    value = mcu_READ_WORD(FBC_V_ACTIVEG_END + fbcbase);
    msg_printf(VRB, " FBC_V_ACTIVEG_END %x \n", value);
    value = mcu_READ_WORD(FBC_H_PADDING_END + fbcbase);
    msg_printf(VRB, " FBC_H_PADDING_END %x \n", value);
    value = mcu_READ_WORD(FBC_V_PADDING_END + fbcbase);
    msg_printf(VRB, " FBC_V_PADDING_END %x \n", value);
    value = mcu_READ_WORD(FBC_H_BLANKING_END + fbcbase);
    msg_printf(VRB, " FBC_H_BLANKING_END %x \n", value);
    value = mcu_READ_WORD(FBC_V_BLANKING_END + fbcbase);
    msg_printf(VRB, " FBC_V_BLANKING_END %x \n", value);
}

void cosmo2_setup_fbc_decomp (__uint32_t  fbcbase, UWORD Horz, UWORD Vert,
                            UBYTE padright, UBYTE padleft,
                            UBYTE padtop, UBYTE padbot )

{
    
    

	/* padbot = 1; */
/* I am adding this padding for test purposes */
	mcu_WRITE(FBC_Y_PAD + fbcbase, 0x50);
	mcu_WRITE(FBC_U_PAD + fbcbase, 0x60);
	mcu_WRITE(FBC_V_PAD + fbcbase, 0x70);

    mcu_WRITE_WORD ( FBC_H_ACTIVE_START + fbcbase, padleft);

    mcu_WRITE_WORD ( FBC_V_ACTIVE_START + fbcbase, padtop);

    mcu_WRITE_WORD ( FBC_H_ACTIVEG_END + fbcbase, (Horz - 1 + padleft) );

    mcu_WRITE_WORD ( FBC_V_ACTIVEG_END + fbcbase, (Vert - 1  + padtop) );

    mcu_WRITE_WORD ( FBC_H_PADDING_END + fbcbase, 
					(Horz - 1 + padleft + padright) );

    mcu_WRITE_WORD( FBC_V_PADDING_END + fbcbase,
			(Vert - 1  + padtop + padbot) );


    mcu_WRITE_WORD( FBC_H_BLANKING_END+fbcbase,
			( Horz-1 + 16 +padleft + padright) );


    mcu_WRITE_WORD( FBC_V_BLANKING_END +fbcbase,
				(Vert - 1 + padtop  + padbot) );

	print_fbc_registers(fbcbase);

    mcu_WRITE(FBC_INT_EN + fbcbase, 0 ) ; 

    mcu_WRITE(FBC_FLOW + fbcbase, 8 ) ; /* decompress */

}

extern void reset_chnl0(void ),reset_chnl1(void ), reset_chnl2(void ), reset_chnl3(void );
extern void unreset_chnl0(void ), unreset_chnl1(void ), unreset_chnl2(void ),
		unreset_chnl3(void );
extern cos2_EnablePioMode(void);

cosmo2_decomp ( UBYTE chnl , UWORD Horz, UWORD Vert , char *ptr, char *fname)
{

	ULONG fd, cc;
	
	__uint32_t i;
	char *bp;
	UBYTE buf[10]  ;
    UWORD timeout = 0x30 ;
    UBYTE padtop, padbot, padright, padleft, val8   ;
    __uint64_t value64  ;
    __uint32_t fbcbase, upcbase, z50base, z16base, mode = 0 , cnt = 0;

    padtop = 0; padbot = 0; padright = 0; padleft = 0; 

	
    reset_chnl0(); reset_chnl1(); reset_chnl2(); reset_chnl3();
	DelayFor(10000);
    unreset_chnl0(); unreset_chnl1(); unreset_chnl2(); unreset_chnl3();

	i = 0;
	while ( i < 20) {
    	if (Open ((CHAR *)ptr, OpenReadOnly, &fd) != ESUCCESS) {
			if ( i == 19) {
    		msg_printf(ERR, "Error: Unable to access file %s\n", ptr);
			G_errors++;
    		return (0) ;
			}
			DelayFor(100000);
    }else
		i = 30;
		i++;
	}

    cos2_EnablePioMode();

    if (chnl == 0) {
    	fbcbase = VIDCH1_FBC_BASE; upcbase = VIDCH1_UPC_BASE;
    	z50base = VIDCH1_050_BASE; z16base = VIDCH1_016_BASE ;
    	chnl = 0;
    }
    else if (chnl == 1) {
    	fbcbase = VIDCH2_FBC_BASE; upcbase = VIDCH2_UPC_BASE;
    	z50base = VIDCH2_050_BASE; z16base = VIDCH2_016_BASE ;
    	chnl = 2;
    }

	bp = (char *) buf; i = 0; value64  = 0;
    while ((Read(fd, bp, 1, &cc) == ESUCCESS) && cc) {

		value64 = (value64 << 8) | (UBYTE) *bp ;
		i++;

		if ((i % 8) == 0) {
    		msg_printf(VRB, "%16llx\n", value64  );
			cgi1_write_reg(value64, cosmobase + cgi1_fifo_rw_o((chnl)) ) ;
			value64 = 0;
			i = 0;
		}	
    }

	if (( i < 8) && ( i > 0))
		{
			value64 = value64 << (8-i)*8 ;
			msg_printf(VRB, "%16llx\n", value64  );
			cgi1_write_reg(value64, cosmobase + cgi1_fifo_rw_o((chnl)) ) ;
		}

	cosmo2_setup_upc_decomp(upcbase, Horz);
	cosmo2_setup_050_decomp(z50base) ;
	cosmo2_setup_016_decomp(z16base, Horz, Vert, mode);

	cosmo2_setup_fbc_decomp(fbcbase, Horz, Vert,
                             padright, padleft , padtop , padbot);

    mcu_WRITE(z16base+Z16_GO_STOP_REG, Z16_GO);
    mcu_WRITE(z50base+Z50_GO, 0);
    val8 = mcu_READ(fbcbase + FBC_MAIN);
    mcu_WRITE(fbcbase + FBC_MAIN, val8 | GO /* | UC_PAD_EN */);

		DelayFor(100000);
        cos2_EnablePioMode( ) ;

        val8 = mcu_READ (fbcbase+FBC_MAIN);
        while (timeout && (val8 & GO)) {
            msg_printf(VRB, "Decomp-GO bit still set: %x \n", val8 );
        	val8 = mcu_READ(fbcbase+FBC_MAIN);
			timeout-- ;
        }

				if (strcmp(fname, "white.jfif")  == 0) 
			msg_printf(VRB, " file is %s \n", fname);
				if (strcmp(fname, "black.jfif")  == 0) 
			msg_printf(VRB, " file is %s \n", fname);

	if (timeout == 0) {
            msg_printf(SUM, "Decomp-GO bit still set: %x \n", val8 );
			G_errors++;
						Close(fd);
			return(0);
		}
		cnt = 0;	
            while ( checkDataInFifo(chnl+1) ) {
                value64 = cgi1_read_reg(cosmobase + cgi1_fifo_rw_o((chnl+1)) );
				if (strcmp(fname, "white.jfif")  == 0) {
					if ( value64 != 0xf080f080f080f080LL)
							{G_errors++; msg_printf(ERR, "%16llx\n", value64); }
                	 msg_printf(VRB, "%16llx\n", value64);
				}else{
				    if (strcmp(fname, "black.jfif") == 0  ){
					    if ( value64 != 0x1080108010801080LL)
							{G_errors++; msg_printf(ERR, "%16llx\n", value64); }
                	 msg_printf(VRB, "%16llx\n", value64);
				    }else{
						G_errors++;
						msg_printf(SUM, " File not found \n");
						Close(fd);
						return(0);
						}
			    }
			cnt++;
            }
                	     msg_printf(VRB, "byte count %d \n", cnt*sizeof(__uint64_t) );

		if ( cnt != (Horz*Vert*2/(sizeof(__uint64_t)) ))
							G_errors++;
			

	Close(fd);
	return (1) ;
}


cos2_jpeg_decomp(  int argc, char **argv  )
{

	
	char fname[100] , server[100] , str[300];
	__uint32_t chnl = 0, mode = 0 , flag;

	argc-- ; argv++ ;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {

    switch (argv[0][1]) {
            case 'c':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &chnl);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *) &chnl);
                }
            break;
            case 'm':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &mode);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *) &mode);
                }
            break;
        case 's' :
        if (argv[0][2]=='\0') { /* has white space */
            sprintf(server, "%s", &argv[1][0]);
            argc--; argv++;
        }
        else { /* no white space */
            sprintf(server, "%s", &argv[0][2]);
        }
		flag=1;
        break;

        case 'f' :
        if (argv[0][2]=='\0') { /* has white space */
            sprintf(fname, "%s", &argv[1][0]);
            argc--; argv++;
        }
        else { /* no white space */
            sprintf(fname, "%s", &argv[0][2]);
        }
        break;
        default  :
		break;
        }
        argc--; argv++;
    }

/*	str[0] = '\0' ; */
#if 1
		strcpy(str, "bootp()");	

	if (flag) {
		strcat(str, server);	
		strcat(str, ":");	
		}

		strcat(str, fname);	
#else
		strcat(str, fname);
		strcpy(fname, &fname[11]);
#endif

	cosmo2_decomp (chnl, 32, 32, str, fname);


	if (G_errors) {
		msg_printf(ERR, " Decompression failed in channel %d \n", chnl + 1);
		return (0);
		}
	else {
		msg_printf(SUM, " Decompression Passed in channel %d \n", chnl + 1);
		return (1);
	}
}
