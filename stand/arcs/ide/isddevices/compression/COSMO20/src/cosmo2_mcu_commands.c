
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

extern void putCMD (UWORD *) ;
extern UBYTE getCMD (UWORD *) ;

_CTest_Info  *pTestInfo = cgi1_info+18 ;



void mcu_write_cmd ( UWORD type, 
					UWORD seqno, 
					__uint32_t addr, 
					UWORD num, 
					UBYTE *data )
{
        UWORD buf[512], *ptr ;

        ptr = buf ;

#if BUG_IN_PROM
	if (addr & 0x8000) addr = addr  + 0x10000 ;
#endif

        *ptr++ = CMD_SYNC ; 			
		msg_printf(DBG, " mcu_write_cmd: 0x%x :", *(ptr-1));
        *ptr++ = (type<<8) + (5+(num+1)/2) ; 	
		msg_printf(DBG, " 0x%x :", *(ptr-1));
        *ptr++ = COSMO2_CMD_WRBYTES ;		
		msg_printf(DBG, " 0x%x :", *(ptr-1));
        *ptr++ = seqno; 			
		msg_printf(DBG, "0x%x:", *(ptr-1));
        *ptr++ = (UWORD) ((addr & 0xffff0000) >> 16) ;	 
		msg_printf(DBG, " 0x%x", *(ptr-1));
        *ptr++ = (UWORD) (addr & 0xffff) ;		 
		msg_printf(DBG, " 0x%x :", *(ptr-1));
        *ptr++ = num;					 
		msg_printf(DBG, " 0x%x \n", *(ptr-1));
	
			msg_printf(DBG, " data: ");
	while (num)
         if (num!=1) {
			msg_printf(DBG, " 0x%x%x \n", *data, *(data+1));

			*ptr++ = (*data << 8 ) + *(data+1);
			 num-=2 ; data+=2;		 
			msg_printf(DBG, "  data 0x%x \n", *(ptr-1));
		} else {
			msg_printf(DBG, " %x \n", *data);
			*ptr = ((UWORD) *data) << 8 ; 	
			msg_printf(DBG, "  data 0x%x num %d \n", *ptr, num);
			num-- ;
			}
        putCMD(buf);

}

void DwnLdXilinx ( 	UWORD reclen ,
                  	UBYTE *data ,
			    	UWORD seqno ,
					UBYTE select_code,
					UBYTE pck_type)
{
        UWORD buf[100], *ptr ;

        ptr = buf ;

        *ptr++ = CMD_SYNC ;
        msg_printf(DBG, " DwnLdXilinx: 0x%x :", *(ptr-1));
        *ptr++ = (COSMO2_CMD_OTHER<<8) + (4+(reclen+1)/2) ;
        msg_printf(DBG, " 0x%x :", *(ptr-1));
        *ptr++ = DOWN_LOAD_XILINX ;
        msg_printf(DBG, "cmd 0x%x :", *(ptr-1));
        *ptr++ = seqno ;
        msg_printf(DBG, "SN 0x%x :", *(ptr-1));
        *ptr++ = select_code;
        msg_printf(DBG, "SC 0x%x ", *(ptr-1));
        *ptr++ = pck_type;
        msg_printf(DBG, "pty 0x%x \n", *(ptr-1));

        while (reclen) { 
           *ptr++ = (*data << 8) + *(data+1) ;
			reclen -= 2 ;
			data += 2 ;
		msg_printf(DBG, "%x ", *(ptr-1) ); 

				if (reclen == 1) {
					*ptr = (*data << 8) ;
					reclen--;
				}
		}

		msg_printf(DBG, "\n " ); 
        putCMD(buf);

}



void mcu_cc1_write_cmd ( UWORD type, 
					UWORD seqno, 
					__uint32_t addr, 
					UWORD num, 
					UBYTE *data )
{
        UWORD buf[512], *ptr ;

        ptr = buf ;

        *ptr++ = CMD_SYNC ;
		msg_printf(DBG, " mcu_cc1_write_cmd: 0x%x :", *(ptr-1));
        *ptr++ = (type<<8) + (5+(num+1)/2) ;    
		msg_printf(DBG, " 0x%x :", *(ptr-1));
        *ptr++ = CC1_FB_WRITE_CMD ;           
		msg_printf(DBG, " 0x%x :", *(ptr-1));
        *ptr++ = seqno;
		msg_printf(DBG, "0x%x:", *(ptr-1));
        *ptr++ = (UWORD) ((addr & 0xffff0000) >> 16) ;
		msg_printf(DBG, " 0x%x", *(ptr-1));
        *ptr++ = (UWORD) (addr & 0xffff) ;
		msg_printf(DBG, " 0x%x :", *(ptr-1));
        *ptr++ = num;
		msg_printf(DBG, " 0x%x \n", *(ptr-1));

        while (num)
         if (num != 1) {
                   msg_printf(DBG, "%d 0x%x%x \n",num,  *data, *(data+1));
                   *ptr++ = (((UWORD)*data) << 8) + *(data+1) ;
                   num-=2 ; data+=2;
                } else {
                    msg_printf(DBG, "num %x %x \n", num, *data);
                    *ptr = ((UWORD) *data) << 8 ;
                    num-- ;
                }
        putCMD(buf);

}

void mcu_read_cmd (  UBYTE type, UWORD seqno, __uint32_t addr, UWORD num )
{

        UWORD buf[512],  *ptr;

        ptr = buf ;

        *ptr++ = CMD_SYNC ;
	msg_printf(DBG, " mcu_read_cmd: 0x%x :", *(ptr-1));
        *ptr++ = (type<<8) + 5 ;
	msg_printf(DBG, "  0x%x :", *(ptr-1));
        *ptr++ = COSMO2_CMD_RDBYTES ;
	msg_printf(DBG, "  0x%x :", *(ptr-1));
        *ptr++ = seqno ;
	msg_printf(DBG, "  0x%x :", *(ptr-1));
        *ptr++ = (UWORD)((addr & 0xffff0000) >> 16) ;
	msg_printf(DBG, "  0x%x :", *(ptr-1));
        *ptr++ = (UWORD)(addr & 0xffff) ;
	msg_printf(DBG, "  0x%x :", *(ptr-1));
        *ptr++ =  num ;
	msg_printf(DBG, "  0x%x :\n", *(ptr-1));

        putCMD(buf);
	/* us_delay(0); */

}

void mcu_cc1_read_cmd (  UBYTE type, UWORD seqno, __uint32_t addr, UWORD num )
{

        UWORD buf[512],  *ptr;

        ptr = buf ;

        *ptr++ = CMD_SYNC ;
        msg_printf(DBG, " mcu_cc1_read_cmd: 0x%x :", *(ptr-1));
        *ptr++ = (type<<8) + 5 ;
        msg_printf(DBG, "  0x%x :", *(ptr-1));
        *ptr++ = CC1_FB_READ_CMD ;
        msg_printf(DBG, "  0x%x :", *(ptr-1));
        *ptr++ = seqno ;
        msg_printf(DBG, "  0x%x :", *(ptr-1));
        *ptr++ = (UWORD)((addr & 0xffff0000) >> 16) ;
        msg_printf(DBG, "  0x%x :", *(ptr-1));
        *ptr++ = (UWORD)(addr & 0xffff) ;
        msg_printf(DBG, "  0x%x :", *(ptr-1));
        *ptr++ =  num ;
        msg_printf(DBG, "  0x%x :\n", *(ptr-1));

        putCMD(buf);

}


void SendHardwareReadRevisionCmd (UWORD type, UWORD seqno, UWORD code )
{

        UWORD buf[512],  *ptr ;

        ptr = buf ;

        *ptr++ = CMD_SYNC ;
	msg_printf(DBG, " mcu_read_rev: sync 0x%x \n", *(ptr-1));
        *ptr++ = (type<<8) + 0x2 ;
	msg_printf(DBG, " mcu_read_rev: type,len 0x%x \n", *(ptr-1));
	*ptr++ = code ;
	msg_printf(DBG, " mcu_read_rev: code 0x%x \n", *(ptr-1));
	*ptr++ = seqno ;
	msg_printf(DBG, " mcu_read_rev: seqno 0x%x \n", *(ptr-1));

	putCMD(buf);

}

cos2_CheckHwareRev( void )
{
        UWORD buf[512]  ;

        SendHardwareReadRevisionCmd (COSMO2_CMD_OTHER, cmd_seq++, COSMO2_CMD_RDBOARDREV );
        DelayFor(10000);
        getCMD(buf);

	CGI1_COMPARE ("cosmo2 revision test:cmd code ", COSMO2_CMD_RDBOARDREV, buf[4], HARDWARE_REVISION, 0xffff);

        if (G_errors)  {
			msg_printf(SUM, " Hardware Revision Test Failed \n");
			return (0) ;
			}
        else {
			msg_printf(SUM, " Hardware Revision Test Passed \n");
			return (1) ;
			}

        
}

#if 0 
cos2_McuFifoLPB ( )
{

        UWORD buf[512], *Wptr, Wcnt;
        UBYTE *Bptr, i, Bcnt, temp;
        /* Write cmd at the MCU_FIFO_base itself */
        _Test_Info      *pTestInfo = cgi1_info+10;
	


        Bcnt = 2*sizeof(walk1s0s_16)/sizeof(walk1s0s_16[0]) ;

        mcu_write_cmd ( COSMO2_CMD_OTHER, cmd_seq, MCU_FIFO_BASE, Bcnt, (UBYTE *)walk1s0s_16 );

        Wptr = buf ; /* fill the buffer with the data from the FIFO */

        Bptr = (UBYTE *) walk1s0s_16 ;

	DelayFor(10000);

        while  (((__uint32_t) cgi1_read_reg(cosmobase+cgi1_interrupt_status_o)) & cgi1_CosmoToGIONotEmpty ) {
        	 *Wptr++ = (UWORD) cgi1_fifos_read_mcu (cgi1ptr) ;
                CGI1_COMPARE (pTestInfo->TestStr, MCU_FIFO_BASE, *(Wptr-1), *Bptr, _8bit_mask);
		Bptr++;
		
	}

        REPORT_PASS_OR_FAIL (pTestInfo, G_errors);

}
#endif


void is_board_happy_cmd ( UWORD type, UWORD seqno, UWORD code )
{
        UWORD buf[512],  *ptr;

        ptr = buf ;

        *ptr++ = CMD_SYNC ;
        msg_printf(DBG, " mcu_is_happy: sync 0x%x \n", *(ptr-1));
        *ptr++ = (type<<8) + 0x2 ;
        msg_printf(DBG, " mcu_is_happy: type,len 0x%x \n", *(ptr-1));
        *ptr++ = code ;
        msg_printf(DBG, " mcu_is_happy: code 0x%x \n", *(ptr-1));
        *ptr++ = seqno ;
        msg_printf(DBG, " mcu_is_happy: seqno 0x%x \n", *(ptr-1));

	putCMD(buf);
}

void is_board_happy( void )
{
	is_board_happy_cmd(5, 0xb, COSMO2_CMD_BOARDHAPPY ) ;
}

void
 reset_video_channel_cmd (UWORD type, UWORD seqno, UWORD code,  UWORD chnl, UWORD mask)
{
	UWORD buf[512],  *ptr ;
	ptr = buf ;

	*ptr++ = CMD_SYNC ;
	msg_printf(DBG, " reset_video_chnl: sync 0x%x \n", *(ptr-1));
        *ptr++ = (type<<8) + 0x4 ;
	msg_printf(DBG, " reset_video_chnl: type,len 0x%x \n", *(ptr-1));
	*ptr++ = code ;
	msg_printf(DBG, " reset_video_chnl: code 0x%x \n", *(ptr-1));
	*ptr++ = seqno ;
	msg_printf(DBG, " reset_video_chnl: seqno 0x%x \n", *(ptr-1));
	*ptr++ = chnl ;
	msg_printf(DBG, " reset_video_chnl: chnl 0x%x \n", *(ptr-1));
	*ptr++ = mask ;
	msg_printf(DBG, " reset_video_chnl: mask 0x%x \n", *(ptr-1));

	putCMD(buf);
}

void 
setup_video_scaler_cmd (UWORD type, UWORD len, UWORD seqno, UWORD code, UWORD chnl, UWORD params, UWORD *data) 
{
	UWORD buf[512],  *ptr, i;
	ptr = buf ;

	*ptr++ = CMD_SYNC ;
        msg_printf(DBG, " setup_video_scaler_cmd: sync 0x%x \n", *(ptr-1));
	*ptr++ = (type<<8) + len ;
	msg_printf(DBG, " setup_video_scaler_cmd: type,len 0x%x \n", *(ptr-1));
	*ptr++ = code ;
	msg_printf(DBG, " setup_video_scaler_cmd: code 0x%x \n", *(ptr-1));
	*ptr++ = seqno ;
	msg_printf(DBG, " setup_video_scaler_cmd: seqno 0x%x \n", *(ptr-1));
	*ptr++ = chnl ;
	msg_printf(DBG, " setup_video_scaler_cmd: chnl 0x%x \n", *(ptr-1));
	*ptr++ = params ;
	msg_printf(DBG, " setup_video_scaler_cmd: chnl 0x%x \n", *(ptr-1));
	
	i = params;
	while (i--)
	*ptr++ = *data++ ;


	putCMD(buf);

}

void mcu_WRITE_WORD (__uint32_t addr, UWORD value16)
{
	UWORD val;
	val = value16;
	mcu_write_cmd(COSMO2_CMD_OTHER, cmd_seq, addr, 2, (UBYTE *) &val);
}

UWORD mcu_READ_WORD(__uint32_t addr)
{
        UWORD buf[10] ;
        UWORD value ;

        mcu_read_cmd(COSMO2_CMD_OTHER, cmd_seq, addr, 2);
	DelayFor(10000);
        getCMD (buf);
        value = buf[7] ;
        return ( value );
}


void mcu_WRITE(__uint32_t addr, UBYTE value)
{
        UBYTE val;
        val = value;
		us_delay(1000);
        mcu_write_cmd(COSMO2_CMD_OTHER, cmd_seq++, addr, 1, &val);
}

UBYTE
mcu_READ(__uint32_t addr)
{
        UWORD buf[10] ;
        UBYTE value ;
        mcu_read_cmd(COSMO2_CMD_OTHER, cmd_seq, addr, 1);
		/*us_delay(100000);*/
		us_delay(1000);
        getCMD (buf);
	if (buf[3] != cmd_seq) {
		msg_printf(SUM, "ERROR reading addr %x cmd_seq %x \n",
			addr, cmd_seq);
		msg_printf(SUM,"mcu_READ 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n",
				buf[0], buf[1], buf[2], buf[3], buf[4], buf[5],
				buf[6], buf[7]);
			return (-1);
		}

        value = (buf[7] & 0xff00) >> 8 ;
        return ( value );
}

void cos2_LedOn ( )
{
        UBYTE val ;

        val = mcu_READ (PWMC);
        mcu_WRITE (PWMC, val | 1);

}

void cos2_LedOff ( )
{
        UBYTE val ;

        val = mcu_READ (PWMC);
        msg_printf(DBG, " val returned before LED off %x \n", val);
        mcu_WRITE (PWMC, val & 0xfe );
        val = mcu_READ (PWMC);
        msg_printf(DBG, " val returned after LED off %x \n", val);
}

void cos2_SetMcuAt16MHZ( )
{

        UWORD buf[10] ;

        mcu_read_cmd (COSMO2_CMD_OTHER, cmd_seq++, SYNCR, 2);
        DelayFor(1000);
        getCMD(buf);
        buf[7] = buf[7]  | SIXTEENMHZ ;
        mcu_write_cmd(COSMO2_CMD_OTHER, cmd_seq, SYNCR, 2, (UBYTE *) (buf+7));

}



void mcu_fifo_loopback()
{

    UWORD buf[512],  *ptr ;
    ptr = buf ;

        *ptr++ = CMD_SYNC ;
        msg_printf(DBG, " mcu_fifo_loopback: 0x%x :", *(ptr-1));
        *ptr++ = (5<<8) + 2 ;
        msg_printf(DBG, " 0x%x :", *(ptr-1));
        *ptr++ = MCU_FIFO_LB ;
        msg_printf(DBG, " 0x%x :", *(ptr-1));
        *ptr++ = cmd_seq;

	putCMD(buf);

}

void ResetAviAvo (void)
{
	UBYTE tmp1, tmp2;

	tmp1 = mcu_READ(QPDR);
	tmp2 = ~PQS7;
	tmp1 = tmp1 & tmp2;
	mcu_WRITE(QPDR, tmp1);

	tmp1 = mcu_READ(QPDR);
	tmp1 = tmp1 | PQS7 ;
	mcu_WRITE(QPDR, tmp1);

}
 
extern name_tbl_t cbar_t[] ;

void dump_cbar_regs()
{

	UBYTE val8;
	name_tbl_t *ptr;
	ptr = cbar_t;
    for ( ; ptr->name[0] !=  '\0'; ptr++ )  {
	val8 = mcu_READ(ptr->offset);
	msg_printf(SUM, " offset %x val %d \n", ptr->offset, val8);
	}
}


void cbar_init ()
{
	mcu_WRITE (CBAR_BASE, 1);
    mcu_WRITE (CBAR_VO_SRC + CBAR_BASE,    0);
    mcu_WRITE (CBAR_VC1_SRC + CBAR_BASE,  0);
    mcu_WRITE (CBAR_VC2_SRC + CBAR_BASE, 0);
    mcu_WRITE (CBAR_GALA_SRC + CBAR_BASE,0);
    mcu_WRITE (CBAR_GALB_SRC + CBAR_BASE,0 );
    mcu_WRITE (CBAR_GALC_SRC + CBAR_BASE,0    );         
    mcu_WRITE (CBAR_GALA_RND_MOD + CBAR_BASE,0   );     
    mcu_WRITE (CBAR_GALB_RND_MOD + CBAR_BASE,0      ); 
    mcu_WRITE (CBAR_GALD_RND_MOD + CBAR_BASE,0      );

    mcu_WRITE (VI_HOR_START + CBAR_BASE,     0     );
    mcu_WRITE (VI_HOR_START + CBAR_BASE + 1, 0    );

    mcu_WRITE (VI_HOR_LEN + CBAR_BASE,       0   );
    mcu_WRITE (VI_HOR_LEN + CBAR_BASE + 1,   0  );

    mcu_WRITE (VI_VERT_START + CBAR_BASE, 0);    

    mcu_WRITE (VI_VERT_LEN + CBAR_BASE,   0   );
    mcu_WRITE (VI_VERT_LEN + CBAR_BASE+1, 0  );

    mcu_WRITE (VI_FIELD_DELAY + CBAR_BASE,0 );
    mcu_WRITE (VO_VERT_START + CBAR_BASE, 0    );        

    mcu_WRITE (VO_ODD_VERT_LEN + CBAR_BASE,  0    );    
    mcu_WRITE (VO_ODD_VERT_LEN + CBAR_BASE + 1, 0    );

        mcu_WRITE (VO_EVEN_VERT_LEN + CBAR_BASE,0       );
        mcu_WRITE (VO_EVEN_VERT_LEN + CBAR_BASE + 1, 0 );

       mcu_WRITE (VC1_HOR_SYNC_START + CBAR_BASE,    0);
       mcu_WRITE (VC1_HOR_SYNC_START + CBAR_BASE + 1,0 );

        mcu_WRITE (VC1_HOR_SYNC_LEN + CBAR_BASE, 0 );  
        mcu_WRITE (VC1_HOR_SYNC_LEN + CBAR_BASE + 1,0 );

       mcu_WRITE (VC1_ODD_SYNC_START + CBAR_BASE,   0  );
       mcu_WRITE (VC1_ODD_SYNC_START + CBAR_BASE + 1,0  );

        mcu_WRITE (VC1_ODD_SYNC_LEN + CBAR_BASE,   0 );
        mcu_WRITE (VC1_ODD_SYNC_LEN + CBAR_BASE + 1,0 );

      mcu_WRITE (VC1_EVEN_SYNC_START + CBAR_BASE, 0 );
    mcu_WRITE (VC1_EVEN_SYNC_START + CBAR_BASE + 1,0 );

    mcu_WRITE (VC1_EVEN_SYNC_LEN + CBAR_BASE,   0 );
    mcu_WRITE (VC1_EVEN_SYNC_LEN + CBAR_BASE + 1,0 );

    mcu_WRITE (VC1_ODD_DATA_START + CBAR_BASE,   0  );
    mcu_WRITE (VC1_ODD_DATA_START + CBAR_BASE + 1,0  );

    mcu_WRITE (VC1_EVEN_DATA_START + CBAR_BASE,  0  );
    mcu_WRITE (VC1_EVEN_DATA_START + CBAR_BASE + 1,0 );

    mcu_WRITE (VC1_HOR_DATA_LEN + CBAR_BASE,     0  );
    mcu_WRITE (VC1_HOR_DATA_LEN + CBAR_BASE + 1, 0   );

    mcu_WRITE (VC1_VERT_DATA_LEN + CBAR_BASE,  0  );
    mcu_WRITE (VC1_VERT_DATA_LEN + CBAR_BASE + 1,0 );

    mcu_WRITE (VC2_HOR_SYNC_START + CBAR_BASE,   0  );
    mcu_WRITE (VC2_HOR_SYNC_START + CBAR_BASE + 1,0  );

    mcu_WRITE (VC2_HOR_SYNC_LEN + CBAR_BASE,    0 );
    mcu_WRITE (VC2_HOR_SYNC_LEN + CBAR_BASE + 1,0  );

    mcu_WRITE (VC2_ODD_SYNC_START + CBAR_BASE,  0   );
    mcu_WRITE (VC2_ODD_SYNC_START + CBAR_BASE + 1,0  );

    mcu_WRITE (VC2_ODD_SYNC_LEN + CBAR_BASE,    0 );
    mcu_WRITE (VC2_ODD_SYNC_LEN + CBAR_BASE + 1,0  );

    mcu_WRITE (VC2_EVEN_SYNC_START + CBAR_BASE, 0   );
    mcu_WRITE (VC2_EVEN_SYNC_START + CBAR_BASE + 1,0 );

    mcu_WRITE (VC2_EVEN_SYNC_LEN + CBAR_BASE,   0 );
    mcu_WRITE (VC2_EVEN_SYNC_LEN + CBAR_BASE + 1,0 );

    mcu_WRITE (VC2_ODD_DATA_START + CBAR_BASE,0 );     
    mcu_WRITE (VC2_ODD_DATA_START + CBAR_BASE + 1,0 );

    mcu_WRITE (VC2_EVEN_DATA_START + CBAR_BASE,  0  );
    mcu_WRITE (VC2_EVEN_DATA_START + CBAR_BASE + 1,0 );

    mcu_WRITE (VC2_HOR_DATA_LEN + CBAR_BASE,    0 );
    mcu_WRITE (VC2_HOR_DATA_LEN + CBAR_BASE + 1,0  );

    mcu_WRITE (VC2_VERT_DATA_LEN + CBAR_BASE,   0   );
    mcu_WRITE (VC2_VERT_DATA_LEN + CBAR_BASE + 1, 0  );

}
