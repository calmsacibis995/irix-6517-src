
/*
 *
 * Command format to be sent to mcu fifo
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
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */

#include "cosmo2_defs.h"
#include "cosmo2_mcu_def.h"

#define CGI1_COMPARE_NUM_ADDR(str, offset, recv, expt, mask) {                             \
        msg_printf(ERR, "COMPARE: name %s addr %x recv %x expt %x \n",\
		str, offset, mask & recv , mask & expt );			\
                G_errors++; \
};

extern name_tbl_t upc_1_t[] ;
extern name_tbl_t upc_2_t[] ;

extern name_tbl_t fbc_1_t[] ;
extern name_tbl_t fbc_2_t[] ;

extern name_tbl_t scaler_1_t[] ;
extern name_tbl_t scaler_2_t[] ;

extern name_tbl_t cpc_1_t[] ;
extern name_tbl_t cpc_2_t[] ;

extern name_tbl_t z050_1_t[] ;
extern name_tbl_t z050_2_t[] ;
extern name_tbl_t z016_1_t[] ;
extern name_tbl_t z016_2_t[] ;

extern name_tbl_t cbar_t[] ;

extern void putCMD (UWORD *) ;
extern UBYTE getCMD (UWORD *) ;
extern void mcu_write_cmd (UWORD, UWORD, __uint32_t, UWORD, UBYTE *);
extern void mcu_read_cmd (UBYTE, UWORD, __uint32_t, UWORD);



#define Z050_MARKER_BEGIN_1 0x500040
#define Z050_MARKER_END_1   0x5003ff


#define Z050_MARKER_BEGIN_2 0x510040
#define Z050_MARKER_END_2 0x5103ff


#define ZORAN_016_1_BEGIN       0x208001
#define ZORAN_016_1_END         0x208001
#define ZORAN_016_2_BEGIN       0x218001
#define ZORAN_016_2_END         0x218001

int
patrn_test( name_tbl_t *Nptr , UBYTE *patrn, int cnt)
{

        UWORD buf[512] ;
	__uint32_t i, addr;
        UBYTE Bptr ;

        /* Write cmd at the MCU_FIFO_base itself */

	
	for ( ; Nptr->name[0] != '\0'; Nptr++)  {
		msg_printf(DBG, "  reg name %s \n", Nptr->name); 
	for ( i = 0 ; i < cnt ; i++)  {
       	mcu_write_cmd ( COSMO2_CMD_OTHER, cmd_seq++, Nptr->offset, 1, (patrn+i) );
		msg_printf(DBG, " send read cmd \n" ); 
		mcu_read_cmd ( COSMO2_CMD_OTHER, cmd_seq, Nptr->offset, 1 );

		msg_printf(DBG, " get the read cmd response  \n" ); 
		getCMD (buf);

		msg_printf(DBG, " buf[7] %x \n", buf[7] ); 
		Bptr = (buf[7] & 0xff00) >> 8 ;
		msg_printf(DBG, " Bptr %x \n", Bptr ); 

      	CGI1_COMPARE (Nptr->name, Nptr->offset, Bptr, *(patrn+i), Nptr->mask);
		}
	}
	return 0;
}

int cos2_z050Walk ( int argc, char **argv ) 
{

        UWORD buf[512], cnt;
        __uint32_t i, j, k;
	__uint32_t addr, channel = 3, begin , end, last;
        UWORD Bptr  ;
	UBYTE temp[16], *ptr;
        _CTest_Info  *pTestInfo = cgi1_info+17 ;
	name_tbl_t *Nptr ;

	argc--; argv++ ;

		if ( argc ) {
			atob(&argv[0][0], (int *) &channel);
			argc--; argv++;
		}

	if ((channel > 3) || (channel < 1)){
		msg_printf(SUM, "incorrect channel number %d: \n", channel);
		msg_printf(SUM, "channel should be 1 or 2  \n" );
       		return (1) ;	
	}

	cnt = sizeof(walk1s0s_8)/sizeof(walk1s0s_8[0]) ;
	msg_printf(DBG, " cnt is %d \n", cnt);

	if (channel == 3) {
		Nptr = z050_1_t;
		msg_printf(DBG, "testing z050 control registers of channel 1 ....\n");
		patrn_test(Nptr,  walk1s0s_8, cnt);	

		Nptr = z050_2_t;
		msg_printf(DBG, "testing z050 control registers of channel 2 ....\n");
		patrn_test(Nptr, walk1s0s_8, cnt);	

	for ( k = 0; k < cnt; k++) {

		for (i = 0; i < cnt; i++)
			temp[i] = walk1s0s_8[(k+i)%cnt] ;

			last = Z050_MARKER_END_1 - cnt ;

		for ( addr = Z050_MARKER_BEGIN_1; addr < last ; addr += cnt) {
			msg_printf(DBG, " begin %x end %x \n", addr, addr + cnt);
			mcu_write_cmd ( COSMO2_CMD_OTHER, cmd_seq++, addr, cnt, temp );
			mcu_read_cmd ( COSMO2_CMD_OTHER, cmd_seq, addr, cnt );
               		DelayFor(10000);
               	 	getCMD (buf);

		for (i = 0; i < cnt; i++) {
			j = *((UBYTE *)(buf+7) + i);	
			if (j != temp[i])
			CGI1_COMPARE_NUM_ADDR("050_markers", (addr+i), j,temp[i], _8bit_mask);
		}
		}

			last = Z050_MARKER_END_2 - cnt ;
        	for ( addr = Z050_MARKER_BEGIN_2; addr < last; addr+=cnt) {
			msg_printf(DBG, " begin %x end %x \n", addr, addr+cnt);
       		         mcu_write_cmd ( COSMO2_CMD_OTHER, cmd_seq++, addr, cnt, temp );
       		         mcu_read_cmd ( COSMO2_CMD_OTHER, cmd_seq, addr, cnt );
       		         DelayFor(10000);
       		         getCMD (buf);

		for (i = 0; i < cnt; i++) {
			j = *((UBYTE *)(buf+7) + i);	
			if (j != temp[i])
			CGI1_COMPARE_NUM_ADDR("050_markers", (addr+i), j,temp[i], _8bit_mask);
		}
			/*CGI1_COMPARE_NUM_ADDR("050_markers", addr, (UBYTE *) (buf+7), temp, cnt,  _8bit_mask); */
		
		} /* addr for loop */
	  }	/* k for loop */
	} /* if (channel == 3) */
	else {
			if (channel == 1) { 
				Nptr = z050_1_t ; 
				begin = Z050_MARKER_BEGIN_1; 
				end = Z050_MARKER_END_1 ; 
				}
			if (channel == 2) { 
				Nptr = z050_2_t ; 
				begin = Z050_MARKER_BEGIN_2; 
				end = Z050_MARKER_END_2 ; 
				}

        	msg_printf(DBG, "testing z050 control registers of channel  %d....\n", channel);
        	patrn_test(Nptr, walk1s0s_8, cnt);   

        	for ( k = 0; k < cnt; k++) {		/* to cycle the patterns */

          		for (i = 0; i < cnt; i++)	/* copy the patterns in temp array */
                		temp[i] = walk1s0s_8[(k+i)%cnt] ;

			last = end - cnt ;
			for ( addr = begin ; addr < last ; addr+=cnt) {
				msg_printf(DBG, " begin %x end %x \n", addr, addr+cnt);
        	        	mcu_write_cmd ( COSMO2_CMD_OTHER, cmd_seq++, addr, cnt, temp );
        	        	mcu_read_cmd ( COSMO2_CMD_OTHER, cmd_seq, addr, cnt );
	                	DelayFor(10000);
                		getCMD (buf);
			
		for (i = 0; i < cnt; i++) {
			j = *((UBYTE *)(buf+7) + i);	
			if (j != temp[i])
			CGI1_COMPARE_NUM_ADDR("050_markers", (addr+i), j,temp[i], _8bit_mask);
		}
				/*CGI1_COMPARE_NUM_ADDR("050_markers", addr, (UBYTE *) (buf+7), temp, cnt,  _8bit_mask);*/
        		}
		}
	} /* else */

        if (G_errors)
          return  ( 0 ) ;
        else {
            msg_printf(SUM," z050 test passed \n");
            return ( 1 );
            }

       	
}


int cos2_z050Patrn ( int argc, char **argv ) 
{

        UWORD buf[512], cnt;
        __uint32_t i, j, k;
	__uint32_t addr, channel = 3, begin , end, last;
        UWORD Bptr  ;
	UBYTE temp[16], *ptr;
        _CTest_Info  *pTestInfo = cgi1_info+17 ;
	name_tbl_t *Nptr ;

	argc--; argv++ ;

		if ( argc ) {
			atob(&argv[0][0], (int *) &channel);
			argc--; argv++;
		}

	if ((channel > 3) || (channel < 1)){
		msg_printf(SUM, "incorrect channel number %d: \n", channel);
		msg_printf(SUM, "channel should be 1 or 2  \n" );
       		return(1) ;	
	}

	cnt = sizeof(patrn8)/sizeof(patrn8[0]) ;

	if (channel == 3) {
		Nptr = z050_1_t;
		patrn_test(Nptr,  patrn8, cnt);	

		Nptr = z050_2_t;
		patrn_test(Nptr, patrn8, cnt);	

	for ( k = 0; k < cnt; k++) {

		for (i = 0; i < cnt; i++) 
			temp[i] = patrn8[(k+i)%cnt] ;

			last = Z050_MARKER_END_1 - cnt ;

		for ( addr = Z050_MARKER_BEGIN_1; addr < last ; addr += cnt) {
			msg_printf(VRB, " Begin 0x%x End 0x%x \n", addr, addr + cnt);
			mcu_write_cmd (COSMO2_CMD_OTHER, cmd_seq++, addr, cnt, temp );
		msg_printf(VRB, "after the function\n");

			mcu_read_cmd (COSMO2_CMD_OTHER, cmd_seq, addr, cnt );
               		DelayFor(10000);
               	 	getCMD (buf);

		for (i = 0; i < cnt; i++) {
			temp[i] = patrn8[(k+i)%cnt] ;
			msg_printf(DBG, "0x%x ", temp[i]);
			}
		msg_printf(DBG, "\n");

		for (i = 0; i < cnt; i++) {
			j = *((UBYTE *)(buf+7) + i);	
			if (j != temp[i])
			CGI1_COMPARE_NUM_ADDR("050_markers", (addr+i), j,temp[i], _8bit_mask);
		}
			/*CGI1_COMPARE_NUM_ADDR("050_markers", addr, (UBYTE *) (buf+7), temp, cnt, _8bit_mask);*/
		}

			last = Z050_MARKER_END_2 - cnt ;
        	for ( addr = Z050_MARKER_BEGIN_2; addr < last; addr+=cnt) {
			msg_printf(DBG, " begin 0x%x end 0x%x \n", addr, addr+cnt);
       		         mcu_write_cmd ( COSMO2_CMD_OTHER, cmd_seq++, addr, cnt, temp );
       		         mcu_read_cmd ( COSMO2_CMD_OTHER, cmd_seq, addr, cnt );
       		         DelayFor(10000);
       		         getCMD (buf);

        for (i = 0; i < cnt; i++) {
            temp[i] = patrn8[(k+i)%cnt] ;
            msg_printf(DBG, "0x%x ", temp[i]);
            }
        msg_printf(DBG, "\n");


		for (i = 0; i < cnt; i++) {
			j = *((UBYTE *)(buf+7) + i);	
			if (j != temp[i])
			CGI1_COMPARE_NUM_ADDR("050_markers", (addr+i), j,temp[i], _8bit_mask);
		}
			/*CGI1_COMPARE_NUM_ADDR("050_markers", addr, (UBYTE *) (buf+7), temp, cnt,  _8bit_mask);*/
		
		} /* addr for loop */
	  }	/* k for loop */
	} /* if (channel == 3) */
	else {
			if (channel == 1) { 
				Nptr = z050_1_t ; 
				begin = Z050_MARKER_BEGIN_1; 
				end = Z050_MARKER_END_1 ; 
				}
			if (channel == 2) { 
				Nptr = z050_2_t ; 
				begin = Z050_MARKER_BEGIN_2; 
				end = Z050_MARKER_END_2 ; 
				}

        	patrn_test(Nptr, patrn8, cnt);   

        	for ( k = 0; k < cnt; k++) {		/* to cycle the patterns */

          		for (i = 0; i < cnt; i++)	/* copy the patterns in temp array */
                		temp[i] = patrn8[(k+i)%cnt] ;

			last = end - cnt ;
			for ( addr = begin ; addr < last ; addr+=cnt) {
				msg_printf(DBG, " begin %x end %x \n", addr, addr+cnt);
        	        	mcu_write_cmd ( COSMO2_CMD_OTHER, cmd_seq++, addr, cnt, temp );
        	        	mcu_read_cmd ( COSMO2_CMD_OTHER, cmd_seq, addr, cnt );
	                	DelayFor(10000);
                		getCMD (buf);

        for (i = 0; i < cnt; i++) {
            temp[i] = patrn8[(k+i)%cnt] ;
            msg_printf(DBG, "0x%x ", temp[i]);
            }
        msg_printf(DBG, "\n");
			
		for (i = 0; i < cnt; i++) {
			j = *((UBYTE *)(buf+7) + i);	
			if (j != temp[i])
			CGI1_COMPARE_NUM_ADDR("050_markers", (addr+i), j,temp[i], _8bit_mask);
		}
				/*CGI1_COMPARE_NUM_ADDR("050_markers", addr, (UBYTE *) (buf+7), temp, cnt,  _8bit_mask);*/
        		}
		}
	} /* else */

        if (G_errors)
          return  ( 0 ) ;
        else {
            msg_printf(SUM," z050 test passed in channel %d \n", channel );
            return ( 1 );
            }

       	
}

int
cos2_AddrUniq( name_tbl_t *Nptr )
{
        UWORD buf[512],  cnt;
        UBYTE Bptr[2]  ;

	for ( ; Nptr->name[0] != '\0' ; Nptr++) {
		Bptr[0] = (UBYTE) (Nptr->offset & 0xff) ;
 
		mcu_write_cmd ( COSMO2_CMD_OTHER, cmd_seq++, Nptr->offset, 1, Bptr); 
		mcu_read_cmd ( COSMO2_CMD_OTHER, cmd_seq, Nptr->offset, 1 );

		DelayFor(10000);

		getCMD (buf);
		Bptr[0] = (buf[7] & 0xff00) >> 8 ;

		CGI1_COMPARE (Nptr->name, Nptr->offset, *Bptr, (UBYTE) (Nptr->offset & 0xff), Nptr->mask);
	}
	return 0;
}

int cos2_z050AddrUniq1( void )
{
        UWORD buf[512],  cnt;
        UBYTE Bptr[2]  ;
        __uint32_t i, addr;
        name_tbl_t *Nptr ;
        _CTest_Info  *pTestInfo = cgi1_info+18 ;


    msg_printf(DBG, "address uniqness test in progress ....  \n");

    cos2_AddrUniq (z050_1_t) ;

        for ( addr = Z050_MARKER_BEGIN_1; addr < Z050_MARKER_END_1 ; addr++) {
        Bptr[0] = (UBYTE) (addr & 0xff) ;

                mcu_write_cmd ( COSMO2_CMD_OTHER, cmd_seq++, addr, 1, Bptr );
                mcu_read_cmd ( COSMO2_CMD_OTHER, cmd_seq, addr, 1 );

                DelayFor(10000);
                getCMD (buf);

                *Bptr = (buf[7] & 0xff00) >> 8 ;

                CGI1_COMPARE ("050_markers_uniqness", addr, *Bptr, (UBYTE) (addr & 0xff), _8bit_mask);
        }

        if (G_errors)
          return  ( 0 ) ;
        else {
            msg_printf(SUM," Z050 test passed in channel 1\n");
            return ( 1 );
            }


}

int cos2_z050AddrUniq2( void )
{
        UWORD buf[512],  cnt;
        UBYTE Bptr[2]  ;
        __uint32_t i, addr;
        name_tbl_t *Nptr ;
        _CTest_Info  *pTestInfo = cgi1_info+18 ;


	msg_printf(DBG, "address uniqness test in progress ....  \n");

	cos2_AddrUniq (z050_2_t) ;

        for ( addr = Z050_MARKER_BEGIN_2; addr < Z050_MARKER_END_2 ; addr++) {
                Bptr[0] = (UBYTE) (addr & 0xff) ;

                mcu_write_cmd ( COSMO2_CMD_OTHER, cmd_seq++, addr, 1, Bptr );
                mcu_read_cmd ( COSMO2_CMD_OTHER, cmd_seq, addr, 1 );

                DelayFor(10000);
                getCMD (buf);

                *Bptr = (buf[7] & 0xff00) >> 8 ;

                CGI1_COMPARE ("050_markers_uniqness", addr, *Bptr, (UBYTE) (addr & 0xff), _8bit_mask);
        }

        if (G_errors)
          return  ( 0 ) ;
        else {
            msg_printf(SUM," Z050 test passed in channel 2\n");
            return ( 1 );
            }
}

int cos2_z016Walk ( void )
{

        UBYTE Bptr, i;
        _CTest_Info  *pTestInfo = cgi1_info+20 ;
        UWORD buf[512] , addr, patrn;

        msg_printf(DBG, "testing z016 ....\n");

        for ( addr = 1; addr < 2 ; addr++)
            for ( patrn = 0 ; patrn < sizeof(walk1s0s_8)/sizeof(walk1s0s_8[0]); patrn++) {
                mcu_write_cmd(COSMO2_CMD_OTHER, cmd_seq, addr + VIDCH1_016_BASE, 1, (walk1s0s_8 + patrn) );
                mcu_read_cmd(COSMO2_CMD_OTHER, cmd_seq, addr + VIDCH1_016_BASE, 1  );
                DelayFor(10000);
                getCMD (buf);
                Bptr = (buf[7] & 0xff00) >> 8 ;

                CGI1_COMPARE ("Z016 patrn", addr+ VIDCH1_016_BASE, Bptr, *(walk1s0s_8 + patrn), _8bit_mask);            
	    }

        if (G_errors)
          return  ( 0 ) ;
        else {
            msg_printf(SUM," 016 test passed \n");
            return ( 1 );
            }


}


int cos2_z016Patrn1 ( void )
{

	UBYTE Bptr, i;
        _CTest_Info  *pTestInfo = cgi1_info+20 ;
	UWORD buf[512], addr, patrn;



	for ( addr = 1; addr < 2 ; addr++)
	    for ( patrn = 0 ; patrn < sizeof(patrn8)/sizeof(patrn8[0]); patrn++) {
		mcu_write_cmd(COSMO2_CMD_OTHER, cmd_seq, addr + VIDCH1_016_BASE, 1, (patrn8 + patrn) ); 
		mcu_read_cmd(COSMO2_CMD_OTHER, cmd_seq, addr + VIDCH1_016_BASE, 1  );
		DelayFor(10000);
		getCMD (buf);
		Bptr = (buf[7] & 0xff00) >> 8 ;

		CGI1_COMPARE ("Z016 patrn", addr + VIDCH1_016_BASE, Bptr, *(patrn8 + patrn), _8bit_mask);
	    }

        if (G_errors)
          return  ( 0 ) ;
        else {
            msg_printf(SUM," z016 test passed channel 2\n");
            return ( 1 );
            }
}

int cos2_z016Patrn2 ( void )
{

    UBYTE Bptr, i;
        _CTest_Info  *pTestInfo = cgi1_info+20 ;
    UWORD buf[512], addr, patrn;



    for ( addr = 1; addr < 2 ; addr++)
        for ( patrn = 0 ; patrn < sizeof(patrn8)/sizeof(patrn8[0]); patrn++) {
		    mcu_write_cmd(COSMO2_CMD_OTHER, cmd_seq, 
					addr + VIDCH2_016_BASE, 1, (patrn8 + patrn) );

        mcu_read_cmd(COSMO2_CMD_OTHER, cmd_seq, addr + VIDCH2_016_BASE, 1  );
        DelayFor(10000);
        getCMD (buf);
        Bptr = (buf[7] & 0xff00) >> 8 ;

        CGI1_COMPARE ("Z016 patrn", addr + VIDCH2_016_BASE, Bptr, *(patrn8 + patrn), _8bit_mask);
        }

        if (G_errors)
          return  ( 0 ) ;
        else {
            msg_printf(SUM," z016 test passed channel 2\n");
            return ( 1 );
            }

}

void
cos2_CpcPatrn ( void )
{
        msg_printf(DBG, "testing cpc\n");
        patrn_test(cpc_1_t, walk1s0s_8, sizeof(walk1s0s_8)/sizeof(walk1s0s_8[0]));
        patrn_test(cpc_1_t, patrn8, sizeof(patrn8)/sizeof(patrn8[0]));
}


int cos2_CpcAddrUniq ()
{

        _CTest_Info  *pTestInfo = cgi1_info+15 ;

        msg_printf(DBG, "cpc : address uniqness test in progress ....  \n");
        cos2_AddrUniq (cpc_1_t) ;
        cos2_AddrUniq (cpc_2_t) ;

        if (G_errors)
          return  ( 0 ) ;
        else {
            msg_printf(SUM," UPC Addr Uniq test passed \n");
            return ( 1 );
            }


}

int cos2_CbarPatrn( void )
{



       patrn_test(cbar_t, walk1s0s_8, sizeof(walk1s0s_8)/sizeof(walk1s0s_8[0]));
       patrn_test(cbar_t, walk1s0s_8, sizeof(walk1s0s_8)/sizeof(walk1s0s_8[0]));

       patrn_test(cbar_t, patrn8, sizeof(patrn8)/sizeof(patrn8[0]));
       patrn_test(cbar_t, patrn8, sizeof(patrn8)/sizeof(patrn8[0]));

        if (G_errors)
		  return  ( 0 ) ;
        else {
            msg_printf(SUM," CBAR  Patrn test passed \n");
            return ( 1 );
            }

}

int cos2_CbarAddrUniq()
{

        msg_printf(DBG, "cbar: address uniqness test in progress ....  \n");

        cos2_AddrUniq (cbar_t) ;
        cos2_AddrUniq (cbar_t) ;

        if (G_errors)
		  return  ( 0 ) ;
        else {
            msg_printf(SUM," CBAR  Addr Uniq Patrn test passed \n");
            return ( 1 );
            }
}

UBYTE cos2_upc_patrn1( void)
{
      patrn_test(upc_1_t, walk1s0s_8, sizeof(walk1s0s_8)/sizeof(walk1s0s_8[0]));
      patrn_test(cpc_1_t, walk1s0s_8, sizeof(walk1s0s_8)/sizeof(walk1s0s_8[0]));
        patrn_test(upc_1_t, patrn8, sizeof(patrn8)/sizeof(patrn8[0]));
        patrn_test(cpc_1_t, patrn8, sizeof(patrn8)/sizeof(patrn8[0]));


        if (G_errors)
		  return  ( 0 ) ;

        msg_printf(SUM, "upc: address uniqness test in progress ....  \n");
        cos2_AddrUniq (upc_1_t) ;

        if (G_errors)
          return  ( 0 ) ;
        else {
            msg_printf(SUM," UPC Patrn test passed in channel 1\n");
            return ( 1 );
            }

}

UBYTE cos2_upc_patrn2 ( void )
{

      patrn_test(upc_2_t, walk1s0s_8, sizeof(walk1s0s_8)/sizeof(walk1s0s_8[0]));
      patrn_test(cpc_2_t, walk1s0s_8, sizeof(walk1s0s_8)/sizeof(walk1s0s_8[0]));

        patrn_test(upc_2_t, patrn8, sizeof(patrn8)/sizeof(patrn8[0]));
        patrn_test(cpc_2_t, patrn8, sizeof(patrn8)/sizeof(patrn8[0]));

        if (G_errors)
          return  ( 0 ) ;

        msg_printf(SUM, "upc: address uniqness test in progress ....  \n");
        cos2_AddrUniq (upc_2_t) ;

        if (G_errors)
		  return  ( 0 ) ;
        else {
            msg_printf(SUM," UPC Patrn test passed in channel 2\n");
            return ( 1 );
            }

}


int cos2_UpcAddrUniq()
{
        _CTest_Info  *pTestInfo = cgi1_info+13 ;

        if (G_errors)
          return  ( 0 ) ;
        else {
            msg_printf(SUM," UPC Addr Uniq test passed \n");
            return ( 1 );
            }
	
}


UBYTE cos2_fbc_patrn2(void) 
{
	msg_printf(SUM, "testing FBC\n");

	patrn_test(fbc_2_t+6, patrn8, sizeof(patrn8)/sizeof(patrn8[0]));
	patrn_test(fbc_2_t+6, walk1s0s_8, sizeof(walk1s0s_8)/sizeof(walk1s0s_8[0]));

        if (G_errors)
          return  ( 0 ) ;

        msg_printf(SUM, "FBC address uniqness test in progress ....  \n");
    cos2_AddrUniq (fbc_2_t+6) ;

        if (G_errors)
          return  ( 0 ) ;
        else {
            msg_printf(SUM," FBC Patrn test passed in channel 2\n");
            return ( 1 );
            }
}



UBYTE cos2_fbc_patrn1(void)
{

	msg_printf(SUM, "testing FBC ....\n");

	patrn_test(fbc_1_t+6, walk1s0s_8, sizeof(walk1s0s_8)/sizeof(walk1s0s_8[0]));
	patrn_test(fbc_1_t+6, patrn8, sizeof(patrn8)/sizeof(patrn8[0]));


        if (G_errors)
          return  ( 0 ) ;

        msg_printf(SUM, "FBC address uniqness test in progress ....  \n");
    cos2_AddrUniq (fbc_1_t+6) ;

        if (G_errors)
          return  ( 0 ) ;
        else {
			msg_printf(SUM," FBC Patrn test passed in channel 1\n");
            return ( 1 );
			}

}

int cos2_FBC_AddrUniq()
{

        msg_printf(SUM, "upc: address uniqness test in progress ....  \n");


        if (G_errors)
          return  ( 0 ) ;
        else {
            msg_printf(SUM," FBC Addr Uniq test passed \n");
            return ( 1 );
            }


}


