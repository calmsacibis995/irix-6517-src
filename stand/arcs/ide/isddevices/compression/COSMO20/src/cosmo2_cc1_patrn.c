/*
 *
 * Cosmo2: cc1 test procedures 
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

#define SYS_CTL_MASK 	0x6f
#define FRMBUF_CTL_MASK 0x3f
#define FRMBUF_SEL_MASK 0xff

#define NTSC 1
#define NTSC_PIXELS 640

#define PAL 0
#define PAL_PIXELS 768 
#define PAL_FIRST_LINE 12
#define PAL_LINES 320
#define PAL_BLOCKS 32


#define BYTES_PER_PIXELS	4	
#define BYTES_PER_BLOCK 	BYTES_PER_PIXELS*12

#define NTSC_LINES	40 /* should be 243 */
#define NTSC_BLOCKS	26
#define NTSC_FIRST_LINE 13
#define BLOCKS	27

extern void mcu_WRITE (__uint32_t, UBYTE);
extern UBYTE mcu_READ (__uint32_t );
extern void cos2_LedOn ( ), cos2_LedOff ( ) ;

#define SLAVE_ADR     0x55
#define I2C_CLOCK     0x18 
#define IICDUMMYWAIT 12


void IICSendByte(UBYTE data)
{
   int loop0;

   for( loop0 = 0; loop0 < IICDUMMYWAIT; ++loop0);
   mcu_WRITE(IIC_DATA,data);
}

/*
 *  Sends a byte to PCF8584 with A0 = 1
 */
void IICSendContr( UBYTE data)
{
   int loop0;

   for( loop0 = 0; loop0 < IICDUMMYWAIT; ++loop0);
   	mcu_WRITE(IIC_S1,data);
}



void IIC_I2C_Init(void)
{

  /* write own slave address */
  IICSendContr( 0x00 );     /* write own slave address */
  IICSendByte( SLAVE_ADR ); /* write to own slave register */

  /* write clock register */
  IICSendContr( 0x20 );     /* write to control reg */
  IICSendByte( I2C_CLOCK ); /* write to clock reg */

}


void cc1_init( )
{
    UBYTE tmp;
    __uint32_t	timeout = 0;


    int cc1_indir_reg[] = {
         0, 0x0,         1, 0x9,         2, 0x1,         3, 0x9,    4, 0x0,
         8, 0x0,         9, 0x8f,       10, 0x4,        11, 0x0,   12, 0xe9,
        13, 0x0,        14, 0x82,       16, 0x0,        17, 0x0,   18, 0x10,
        19, 0x0,        20, 0x0,        21, 0x0,        22, 0x0,   24, 0x0,
        25, 0x0,        26, 0x10,       27, 0x0,        28, 0x0,   29, 0x0,
        30, 0x0,        32, 0xa,        33, 0x0,        34, 0x1e,  35, 0x4,
        36, 0x1,        37, 0x0,        38, 0xa0,       39, 0x0,   40, 0x0,
        41, 0x0,        42, 0x0,        43, 0x0,        44, 0x0,   45, 0x0,
        46, 0x0,        47, 0x0,        48, 0x0,        49, 0x0,   50, 0x0,
        51, 0x80,       52, 0x80,       53, 0x80,       54, 0x0,   55, 0x40,
        56, 0x40,       57, 0x0,        58, 0x0,        59, 0x5,   64, 0x0,
        65, 0x0,        66, 0x0,        67, 0x0,        68, 0x0,   69, 0x0,
        70, 0x0,        71, 0x0,        72, 0x0,        73, 0x0,   74, 0x0,
        75, 0x0,        76, 0x0,        77, 0x0,        78, 0x0,   79, 0x0,
        80, 0x0,
        -1, -1
    };

    tmp = 0 ;

    mcu_WRITE(CC1_BASE + CC1_SYS_CTL, 0x4);

    while ( cc1_indir_reg[tmp] != -1) {
        mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, cc1_indir_reg[tmp]);
        mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, cc1_indir_reg[tmp++]);
    }

    mcu_WRITE(CC1_BASE + CC1_INDIR_ADDR, 38);
    mcu_WRITE(CC1_BASE + CC1_INDIR_DATA, 0x2);

    do {
        tmp = mcu_READ(CC1_BASE + CC1_INDIR_DATA);
        timeout++;
        if (timeout == 10000) {
            msg_printf(ERR, "CCIND38 bit 6 is set \n");
#if UNIX_BUG_FIXED
            errors++;
            return(errors);
#endif
        }
    } while ((tmp & 0x40) && (timeout < 10000));

    mcu_WRITE(CC1_BASE + CC1_FB_CTL, 0x40);
    mcu_WRITE(CC1_BASE + CC1_FB_SEL, 0);
}


void cc1_slave()
{
	mcu_WRITE(CC1_BASE + CC1_SYS_CTL, 0);
	mcu_WRITE (CC1_BASE + CC1_FB_SEL, 0);
	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 33);
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 1);
	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 34);
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 53);
	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 35);
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 1);
	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 36);
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 0);
	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 48);
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 0);
	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 37);
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, CC1_GPO_SQ_DEC|CC1_GPO_SQ_ENC|CC1_GPO_SLAVE);
	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 0x26);
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, CC1_VIN_ENB);

	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 56);
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 0x30);

}

void cc1(void)
{
	msg_printf(VRB, " cc1 setup \n");
	mcu_WRITE (CC1_BASE + CC1_SYS_CTL, CC1_SYS_TIM_VIDOUT|CC1_SYS_TBCIN_ENB);
	mcu_WRITE (CC1_BASE + CC1_FB_CTL, 0);
    mcu_WRITE (CC1_BASE + CC1_FB_SEL, 0);
    mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 33);
    mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 1);
    mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 34);
    mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 53);
    mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 35);
    mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 1);
    mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 36);
    mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 0);
    mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 48);
    mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 0);
    mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 37);
    mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, CC1_GPO_SQ_DEC|CC1_GPO_SQ_ENC);
    mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 0x26);
    mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, CC1_VIN_ENB);

    mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 56);
    mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 0x30);

}
void cc1_ccir_pal()
{
	mcu_WRITE (CC1_BASE + CC1_SYS_CTL ,           0x2b);
	mcu_WRITE (CC1_BASE + CC1_FB_CTL ,            0x0);
	mcu_WRITE (CC1_BASE + CC1_FB_SEL ,            0x0);
	mcu_WRITE (CC1_BASE + CC1_FB_DATA ,           0x0);
	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, CC1_VO_TIM_HI);
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA,         0x61);
	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, CC1_VO_TIM_HOR_LO);
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA,       0x5c);
	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, CC1_VO_TIM_VERT_LO);
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA ,    0x70);
	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, CC1_VO_FST_UNBLANK);
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA ,    0x0);
	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, CC1_GPO );
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA ,     0x0);
	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, CC1_VIN_TIM );
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA,           0x20);
	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, CC1_FB_SETUP);
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA ,          0x0);
	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, CC1_HOST_RW_LINE);
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA ,      0x68);
	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, CC1_FBIN_VIDOUT);
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA,       0x30);
}

void cc1_ccir()
{
	mcu_WRITE (CC1_BASE + CC1_SYS_CTL,
		CC1_SYS_TIM_VIDOUT|CC1_SYS_TBCIN_ENB|CC1_SYS_NONSQUARE);
	mcu_WRITE (CC1_BASE + CC1_FB_CTL, 0); 
	mcu_WRITE (CC1_BASE + CC1_FB_SEL,0 );
	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 33 );
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 1 );
    mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 34);
    mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 53);
    mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 35);
    mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 1);
    mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 36);
    mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 0);
    mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 48);
    mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 0);
    mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 37);
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA,0 );
	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 0x26);
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, CC1_VIN_ENB);
    mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 56);
    mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 0x30);

 
}
void cc1_pal ()
{
	mcu_WRITE (CC1_BASE + CC1_SYS_CTL,
		CC1_SYS_TIM_VIDOUT|CC1_SYS_PAL);
    mcu_WRITE (CC1_BASE + CC1_FB_CTL, 0);
    mcu_WRITE (CC1_BASE + CC1_FB_SEL,0 );
    mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 33 );
    mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 1 );
    mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 34);
    mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 53);
    mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 35);
    mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 1);
    mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 36);
    mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 0);
    mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 48);
    mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 0);
    mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 37);
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA,CC1_GPO_SQ_DEC|CC1_GPO_SQ_ENC);
	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 0x26);
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, CC1_VIN_ENB);
	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 56);
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 0x30);
}


extern void avo(void);
void vo_internal (void);
void WAIT_FOR_TRANSFER_READY ( void );
extern void cosmo2_init (void);

void vo_internal(void)
{
cc1();
avo();
}

UBYTE cos2_cc1_revision ( )
{
	UBYTE recv ;
	mcu_WRITE ( CC1_BASE + CC1_INDIR_ADDR, 128 );
	recv = mcu_READ ( CC1_BASE + CC1_INDIR_DATA );
	if (recv != 0x20)	{
		msg_printf(SUM, " ERROR: InCorrect Revision of CC1 is %x \n", recv );
	return 0 ;
	}
	else {
		msg_printf(SUM, " CC1 Revision Passed\n" );
		return 1 ;
	}
}

void WRITE_CC1_FRAME_BUFFER_SETUP( UBYTE fld_frm_sel, 
								UWORD startline, 
								UWORD block) 
{
        __uint32_t fbsel ;
	UBYTE value ;
		msg_printf(DBG, " In the function cc1 write setup \n");
#if 0
        fbsel = fld_frm_sel >> 1;
        mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 56 );
        mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 0x34 );

        mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR,  48 );
        mcu_WRITE (CC1_BASE + CC1_INDIR_DATA,  0x6  ); 

        mcu_WRITE (CC1_BASE + CC1_FB_SEL,(fbsel<< 5)|(fld_frm_sel & 0x1 ) << 4 ); 
        mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR,  49 );
        mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, ((startline >> 1)  & 0xFF) ); 

        mcu_WRITE (CC1_BASE+CC1_FB_CTL,(((startline&0x1)<<5) | 0xC0) | block );
        mcu_WRITE (CC1_BASE + CC1_FB_CTL,       0 );
		msg_printf(DBG, " Leaving the function cc1 write setup \n");
#else
		fbsel = fld_frm_sel >> 1;
		mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 56 );
		mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 0x30);
		
		mcu_WRITE (CC1_BASE + CC1_FB_SEL,(fbsel<< 5));
		value = mcu_READ(CC1_BASE + CC1_FB_SEL);
		mcu_WRITE (CC1_BASE + CC1_FB_SEL, value | fld_frm_sel & 0x1 );
	
		mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 49);
		mcu_WRITE (CC1_BASE + CC1_INDIR_DATA,  ((startline >> 1)  & 0xFF)); 

		mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR,  48 );
		mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 2 );

		mcu_WRITE (CC1_BASE + CC1_FB_CTL, (((startline & 0x1) << 5) | 0xC0) | block );
		mcu_WRITE (CC1_BASE + CC1_FB_CTL,       0 );

#endif
}


void READ_CC1_FRAME_BUFFER_SETUP( UBYTE field, 
							UWORD startline,
							UWORD block) 
{
#if 0
        int fbsel;
		msg_printf(DBG, " In the function cc1 read setup \n");
        fbsel = fld_frm_sel >> 1;

        mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 56);
        mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 0x34 );

        mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR,  48);
        mcu_WRITE (CC1_BASE + CC1_INDIR_DATA,   0x6   );

        mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR,  49);
        mcu_WRITE (CC1_BASE + CC1_INDIR_DATA,  ((startline >> 1)  & 0xFF) ); 

        mcu_WRITE (CC1_BASE + CC1_FB_CTL,(((startline & 0x1) << 5)|0xC0)|block);
	
        mcu_WRITE (CC1_BASE + CC1_FB_CTL, 0 );

#else
	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 56);
        mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 0x0);

	(void)mcu_READ(CC1_BASE + CC1_FB_SEL);
	mcu_WRITE (CC1_BASE + CC1_FB_SEL, (field << 4 )); 

	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR,  49);
	mcu_WRITE (CC1_BASE + CC1_INDIR_DATA,  ((startline >> 1)  & 0xFF) );

    	mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR,  48);
        mcu_WRITE (CC1_BASE + CC1_INDIR_DATA,  0x6 );
		
	mcu_WRITE (CC1_BASE + CC1_FB_CTL, (((startline & 0x1)<<5)|0xC0));
	mcu_WRITE (CC1_BASE + CC1_FB_CTL, 0x0 );

#endif

}

void WAIT_FOR_TRANSFER_READY (void)
{
        __uint32_t tries=0; 
     while ((mcu_READ ( CC1_BASE + CC1_FB_CTL) & 0x80) == 0)  {
        ++tries; 
		msg_printf(DBG, " in wait transfer ready funtion \n");
         if (tries > 100) {
           msg_printf(ERR, "WAIT_FOR_TRANSFER_READY Time out transfer ~ready 0x%0X \n",CC1_BASE+CC1_FB_CTL); 
                        tries = 0; 
                        break;
                }
        }
}

extern mcu_cc1_write_cmd (UWORD, UWORD, __uint32_t, UWORD, UBYTE *);
extern mcu_cc1_read_cmd (UWORD, UWORD, __uint32_t, UWORD );

#define THERE_IS_NO_CC1_COMMAND_FOR_WRITE 0
#define THERE_IS_NO_CC1_COMMAND_FOR_READ 0

void send_buf(UBYTE *buf ) 
{

#if THERE_IS_NO_CC1_COMMAND_FOR_WRITE 
	UBYTE i;
	for ( i = 0 ; i < BYTES_PER_BLOCK ; i++)
	mcu_WRITE(CC1_BASE+CC1_FB_DATA, *(buf+i) );
#else
	mcu_cc1_write_cmd (COSMO2_CMD_OTHER, cmd_seq, 
					CC1_BASE+CC1_FB_DATA,  
					BYTES_PER_BLOCK, buf );
#endif

}


void recv_buf(UBYTE *buf )
{
	UWORD i, cnt;
	UWORD recv[BYTES_PER_BLOCK];
	UBYTE *ptr;

	for ( i = 0; i < BYTES_PER_BLOCK; i++)  {
		*(buf+i) =  0x0 ;
		recv[i]  =  0x0 ;
	}

#if THERE_IS_NO_CC1_COMMAND_FOR_READ 
	for ( i = 0; i < BYTES_PER_BLOCK; i++)  
		*(buf+i)  = mcu_READ (CC1_BASE+CC1_FB_DATA);	
#else
	mcu_cc1_read_cmd (COSMO2_CMD_OTHER, cmd_seq, 
				CC1_BASE+CC1_FB_DATA,  BYTES_PER_BLOCK  );

	DelayFor(1000);
	ptr = buf;
	if (getCMD(recv) == 0)
		return ;

	msg_printf(DBG, " sync %x ", recv[0] );
	msg_printf(DBG, " type %x ", recv[1] );
	msg_printf(DBG, " code %x ", recv[2] );
	msg_printf(DBG, " seqno %x ", recv[3] );
	msg_printf(DBG, " high addr %x ", recv[4] );
	msg_printf(DBG, " low addr %x ", recv[5] );
	msg_printf(DBG, " byte cnt %x\n ", recv[6] );
	cnt = recv[6]  ;

		for (i = 0; i < cnt / 2; i++) {
			*ptr++ = (recv[7+i] & 0xff00) >> 8 ;
			*ptr++  = recv[7+i] & 0xff ;
			msg_printf(DBG, " i %d 0x%x%x \n", i, *(ptr-2), *(ptr-1) );
			msg_printf(VRB, " recvd  0x%x \n", recv[7+i] );
		}

#endif
}

extern i2cl_init(void);
extern avi(void);

extern cosmo2_cbar_vivo_setup (UBYTE, UBYTE );
extern cosmo2_cbar_chan_setup (UBYTE, UBYTE );

void cosmo2_tim2cc1()
{
#if 0
    mcu_WRITE(CC1_BASE+CC1_INDIR_ADDR, 38) ;
    mcu_WRITE(CC1_BASE+CC1_INDIR_DATA, 0)  ;

#endif
    i2cl_init()   ;
    avi()        ;
    vo_internal();
	cosmo2_cbar_vivo_setup (0, 0);
    cosmo2_cbar_chan_setup (0, 0);


#if 0
    mcu_WRITE(CC1_BASE+CC1_INDIR_ADDR, 38) ;
    mcu_WRITE(CC1_BASE+CC1_INDIR_DATA, 0xa0)  ;
#endif
}


void
cos2_cc1_patrn_test ()
{

}


cos2_cc1_data_bus_test ( )
{

	UBYTE num, i , rcv ;

	num = sizeof(patrn8) / sizeof(patrn8[0]) ; 
	for ( i = 0; i < num ; i++ )  {
		mcu_WRITE ( CC1_BASE + CC1_FB_SEL, patrn8[i]);
		rcv = mcu_READ ( CC1_BASE + CC1_FB_SEL ) ;
		if ((rcv & FRMBUF_SEL_MASK) != (patrn8[i] & FRMBUF_SEL_MASK)) {
			msg_printf(ERR, "CC1: CC1_FB_SEL rcv %x , exp %x \n",
					rcv, patrn8[i]); 
			G_errors++;
			msg_printf (SUM, " CC1 Data Bus Failed \n");
			return (0);
		}
	}

	for ( i = 0; i < num ; i++ )  {
		mcu_WRITE ( CC1_BASE + CC1_FB_CTL , patrn8[i]);
		rcv = mcu_READ ( CC1_BASE + CC1_FB_CTL ) ;
		if ((rcv & FRMBUF_CTL_MASK) != (patrn8[i] & FRMBUF_CTL_MASK)) {
			msg_printf(ERR, "CC1:  CC1_FB_CTL rcv %x , exp %x \n",
                  rcv, patrn8[i]);
			G_errors++;
			msg_printf (SUM, " CC1 Data Bus Failed \n");
			return (0);
		}
	}

    for ( i = 0; i < num ; i++ )  {
                mcu_WRITE ( CC1_BASE + CC1_SYS_CTL , patrn8[i]);
                rcv = mcu_READ ( CC1_BASE + CC1_SYS_CTL ) ;
		if ((rcv & SYS_CTL_MASK) != (patrn8[i] & SYS_CTL_MASK)) {
		msg_printf(ERR, "CC1:   CC1_SYS_CTL rcv %x , exp %x \n",
					rcv, patrn8[i]);
		    G_errors++;
			msg_printf (SUM, " CC1 Data Bus Failed \n");
            return (0);
        }

    }
	msg_printf (SUM, " CC1 Data Bus Passed \n");
	return (1);
}

void cosmo2_cc1_write_setup ( UBYTE *data, __uint32_t stline, __uint32_t block,
					UBYTE field )
{

        mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 49);
        mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, (stline >> 1)  & 0xFF);

        mcu_WRITE (CC1_BASE + CC1_FB_CTL, 
				((stline & 0x1) << 5) | 0xC0 | (block & 0x1f) );
        mcu_WRITE (CC1_BASE + CC1_FB_CTL, 0 );


        		/* setup FB, wait for transfer and send */

            		WAIT_FOR_TRANSFER_READY () ;

            		DelayFor(10000) ;
            		send_buf(data) ;
        	
}

void cosmo2_cc1_read_setup( UBYTE *data, __uint32_t stline, __uint32_t block,
                    UBYTE field  )
{


        mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 49) ;
        mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, (stline >> 1)  & 0xFF);

        mcu_WRITE (CC1_BASE + CC1_FB_CTL, 
						((stline & 0x1) << 5) | 0xC0 | (block & 0x1f) );
        mcu_WRITE (CC1_BASE + CC1_FB_CTL, 0 );

         /* setup FB, wait for transfer and recv */
}

void cosmo2_check_cc1_block(UBYTE *recv, UBYTE *exp, UWORD line, UWORD block, UBYTE cnt )
{

	UBYTE i;
   		     for ( i = 0 ; i < cnt  ; i++ )
   			    if ( recv[i] != exp[i] ) {
   			       msg_printf(SUM,"Err ln %x blk %x pxl %x rcv %x exp %x \n",
                               line, block, i, recv[i], exp[i] );
				 G_errors++;
				}
}

void cosmo2_fill_buf (UBYTE *data, UWORD line, UWORD blk, UBYTE field)
{
		UBYTE j;

		if (field )
                for ( j = 0; j < BYTES_PER_BLOCK; j+=2 )  {
                       *(data+j) = line & 0xff ;
                       *(data+j+1) =  (blk*BYTES_PER_BLOCK + j) & 0xff ;
				}
		else
                for ( j = 0; j < BYTES_PER_BLOCK; j+=2 )  {
                       *(data+j) = line & 0xff ;
                       *(data+j+1) =  (blk*BYTES_PER_BLOCK + j + 1) & 0xff ;
                }
                
}

void cosmo2_cc1_write_fb ( UWORD slines, UWORD elines, UBYTE blocks, 
						UBYTE field , UBYTE pal)
{
	UWORD lines,  blk = 0; 
	UBYTE  buf[BYTES_PER_BLOCK] ;


	cmd_seq = 0;


	for (lines = slines;lines < elines;lines++)  {
	 		for ( blk = 0 ; blk < NTSC_BLOCKS ; blk++ ) {

				cosmo2_fill_buf (buf, lines, blk, field);
	   			cosmo2_cc1_write_setup (buf, lines, blk, field); /* even */
	   		}
		/*
		  In NTSC, SQ mode, we
		  need to write the last 16 pixels. but stuff 8 pixels at the
		  end of the 16 pixels becos cc1 takes only 48 bytes (or 24 pixels)
		*/
		if (pal == 0) {
				cosmo2_fill_buf (buf, lines, blk, field);
                cosmo2_cc1_write_setup (buf, lines, blk, field); /* even */
		}
 	}

}

void cosmo2_cc1_read_fb ( UWORD slines, UWORD elines, UBYTE blocks, 
						UBYTE field, UBYTE pal)
{
	UBYTE temp[BYTES_PER_BLOCK];
	UBYTE  buf[BYTES_PER_BLOCK];
	UWORD lines,  blk = 0; 


		/* even */

	cmd_seq = 0;

    for (lines = slines;lines < elines;lines++)  {
         blk = 0; cosmo2_cc1_read_setup (temp, lines, blk, field); 
            for ( blk = 0 ; blk < blocks ; blk++ ) {

            WAIT_FOR_TRANSFER_READY () ;
            DelayFor(10000) ;
            recv_buf(temp) ;

				cosmo2_fill_buf (buf,  lines, blk, field);
				cosmo2_check_cc1_block (temp, buf,lines, blk, BYTES_PER_BLOCK);
			if (G_errors)
				 ;

            }
        /*
          In NTSC, SQ mode, we
          need to read the first 16 pixels. so, ignore the last 8 pixels 
        */
		if (pal == 0) {
            WAIT_FOR_TRANSFER_READY () ;
            DelayFor(10000) ;
            recv_buf(temp) ;
				cosmo2_fill_buf (buf,  lines, blk, field);
				cosmo2_check_cc1_block (temp, buf, lines, blk, 32);
			if (G_errors)
				 ;
		}

    }
}


void cosmo2_cc1_fb_read_main ( int argc, char **argv )
{

	__uint32_t sline, eline , blocks, field = 0, pal = 0;
    argc--; argv++;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
    switch (argv[0][1]) {
            case 'p':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &pal);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *) &pal);
                }
            break;
            case 'f':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &field);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *) &field);
                }
            break;
            default:
                break ;
        }
        argc--; argv++;
    }

        mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 56) ;
        mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 0x33) ; /* video to FB */

        mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 48 ) ; /* FB to CPU */
        mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 6 /* it was 6 */  ) ;
	
	if (pal == 0) {
		sline = NTSC_FIRST_LINE;
		eline = NTSC_LINES+NTSC_FIRST_LINE ;
		blocks = NTSC_BLOCKS;
	} else {
		sline = PAL_FIRST_LINE;
		eline = PAL_LINES+PAL_FIRST_LINE ;
		blocks = PAL_BLOCKS;
	} ;
    mcu_WRITE (CC1_BASE + CC1_FB_SEL,  (UBYTE) ((field & 0x1) << 4)  );
	cosmo2_cc1_read_fb (sline, eline, blocks, field, pal); 

}

void cosmo2_cc1_fb_write_main ( int argc, char **argv  )
{
    __uint32_t sline, eline , blocks, field, pal = 0;
    argc--; argv++;

	cosmo2_init();

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
    switch (argv[0][1]) {
            case 'p':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &pal);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *) &pal);
                }
            break;
            case 'f':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &field);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *) &field);
                }
            break;
            default:
                break ;
        }
        argc--; argv++;
    }

		mcu_WRITE(CC1_BASE + CC1_SYS_CTL, 0x48);   /* NTSC */
        mcu_WRITE(CC1_BASE + CC1_INDIR_ADDR, 56 );
        mcu_WRITE(CC1_BASE + CC1_INDIR_DATA, 0x30); /* video to FB */ 
        mcu_WRITE(CC1_BASE + CC1_INDIR_ADDR,  48 ); /* cpu to FB */
        mcu_WRITE(CC1_BASE + CC1_INDIR_DATA, 2 );

    if (pal == 0) {
        sline = NTSC_FIRST_LINE;
        eline = NTSC_LINES+NTSC_FIRST_LINE ;
        blocks = NTSC_BLOCKS;
    } else {
        sline = PAL_FIRST_LINE;
        eline = PAL_LINES+PAL_FIRST_LINE ;
        blocks = PAL_BLOCKS;
    } ;

    mcu_WRITE (CC1_BASE + CC1_FB_SEL,  (UBYTE) ((field & 0x1) << 4)  );
	cosmo2_cc1_write_fb ( sline, eline, blocks, field, pal);

}


void cosmo2_read_block (  int argc, char **argv ) 
{
	UBYTE temp[BYTES_PER_BLOCK], i, val8;
    __uint32_t line, block, field;

    argc--; argv++;

    while (argc && argv[0][0]=='-' && argv[0][1]!='\0') {
    switch (argv[0][1]) {
            case 'b':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &block);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *) &block);
                }
            break;
            case 'l':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &line);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *) &line);
                }
            break;
            case 'f':
                if (argv[0][2]=='\0') {
                    atob(&argv[1][0], (int *) &field);
                    argc--; argv++;
                        } else {
                        atob(&argv[0][2], (int *) &field);
                }
            break;
            default:
                break ;
        }
        argc--; argv++;
    }

        mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 56) ;
        mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 0x33) ; /* video to FB */

        mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 48 ) ; /* FB to CPU */
        mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, 6 /* it was 6 */  ) ;

	    val8 = mcu_READ (CC1_BASE + CC1_FB_SEL);
		val8 = val8 & (~0x10);
	    mcu_WRITE (CC1_BASE + CC1_FB_SEL,  val8 | (UBYTE) ((field & 0x1) << 4)  );

        mcu_WRITE (CC1_BASE + CC1_INDIR_ADDR, 49) ;
        mcu_WRITE (CC1_BASE + CC1_INDIR_DATA, (line >> 1)  & 0xFF);

        mcu_WRITE (CC1_BASE + CC1_FB_CTL,
                        ((line & 0x1) << 5) | 0xC0 | (block & 0x1f) );
        mcu_WRITE (CC1_BASE + CC1_FB_CTL, 0 );

            WAIT_FOR_TRANSFER_READY () ;
            DelayFor(10000) ;
            recv_buf(temp) ;

	    for ( i = 0; i < 24 ; i++)
            msg_printf(SUM, "%x ", temp[i]) ; msg_printf(SUM, "\n") ;
        for ( i = 24; i < 48 ; i++)
            msg_printf(SUM, "%x ", temp[i]) ;  msg_printf(SUM, "\n") ;

}


