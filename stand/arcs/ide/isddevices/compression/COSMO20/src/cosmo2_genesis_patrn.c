
/*
 * cgi1_rst.c
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

#define NUM_GEN_REG  18

extern void Reset_SCALER_UPC  (UWORD);
extern void Reset_050_016_FBC_CPC (UWORD);

extern name_tbl_t scaler_1_t[] ;
extern name_tbl_t scaler_2_t[] ;

int gvs_patrn ( name_tbl_t *Nptr, UBYTE *patrn, 
			__uint32_t size , 
			__uint32_t upc_base,
			__uint32_t GENESIS_BASE )
{

	__uint32_t  gvs_access; 
	UBYTE i, rcv, val ;

		gvs_access =  upc_base + UPC_GENESIS_ACCESS ;
		val = mcu_READ ( gvs_access );
	    msg_printf(VRB, ": UPC_GENESIS_ACCESS	\n" ) ;

/* Check with john */


	val |= GVS_LD_PAR ;
	mcu_WRITE( gvs_access , val);


	if (val & GVS_LD_PAR ) {					/* if param can be loaded */
	    for ( i = 0; i < size ; i++)			/* for each patrn */
		  for ( ; Nptr->name[0] != '\0' ; Nptr++) {

			DelayFor(1000);
	
			val |= GVS_AD_SEL ;
			  mcu_WRITE ( gvs_access , val );		/* set addr sel */
			  mcu_WRITE ( GENESIS_BASE, Nptr->offset - GENESIS_BASE);	/* write addr */
			val &= ~GVS_AD_SEL ;
			  mcu_WRITE ( gvs_access , val );			/* set data sel */
			  mcu_WRITE ( GENESIS_BASE, *(patrn + i));	/* write data  */

			val |= GVS_AD_SEL ;
			  mcu_WRITE ( gvs_access , val );			/* set addr sel */
			  mcu_WRITE ( GENESIS_BASE, Nptr->offset - GENESIS_BASE );		/* write addr   */

			val &= ~GVS_AD_SEL ;
			  mcu_WRITE ( gvs_access , val );			/* set data sel */
			  rcv = mcu_READ (GENESIS_BASE );			/* read data  */

			CGI1_COMPARE(Nptr->name, Nptr->offset, rcv, *(patrn + i), Nptr->mask );
	
	       }
	} else
	msg_printf(SUM, " cannot load the genesis registers \n");
	return (1);
}

int cos2_gvs_patrn0 ( void ) ;
int cos2_gvs_patrn1 ( void ) ;
int cos2_gvs_patrn  ( void ) ;

cos2_gvs_patrn0 ( void )
{
	Reset_SCALER_UPC (CHANNEL_0);
	Reset_050_016_FBC_CPC (CHANNEL_0 );
	DelayFor(10000);
	gvs_patrn (scaler_1_t, patrn8, PATRNS_8_SIZE, 
				VIDCH1_UPC_BASE, VIDCH1_GENESIS_BASE );
	gvs_patrn (scaler_1_t, walk1s0s_8, WALK_8_SIZE, 
				VIDCH1_UPC_BASE, VIDCH1_GENESIS_BASE );

	if (G_errors) {
		msg_printf(ERR, "scaler pattern test failed in video channel 1 \n");
		return (0);
		}
	else {
		msg_printf(SUM, "scaler pattern test passed in video channel 1 \n");
		return (1);
		}

}

cos2_gvs_patrn1 ( void )
{
    Reset_SCALER_UPC (CHANNEL_1);
    Reset_050_016_FBC_CPC (CHANNEL_1 );
    gvs_patrn (scaler_2_t, patrn8, PATRNS_8_SIZE,
                VIDCH2_UPC_BASE, VIDCH2_GENESIS_BASE );
    gvs_patrn (scaler_2_t, walk1s0s_8, WALK_8_SIZE,
                VIDCH2_UPC_BASE, VIDCH2_GENESIS_BASE );
	if (G_errors) {
		msg_printf(ERR, "scaler pattern test failed in video channel 2 \n");
		return (0);
		}
	else {
		msg_printf(SUM, "scaler pattern test passed in video channel 2 \n");
		return (1);
		}
}

cos2_gvs_patrn (void )
{
	cos2_gvs_patrn0 ( );
	if (G_errors) {
		msg_printf(ERR, "scaler pattern test failed in video channel 1 \n");
		return (0);
		}
	else {
		msg_printf(SUM, "scaler pattern test passed in video channel 1 \n");
		}

	cos2_gvs_patrn1 ( );

	if (G_errors) {
		msg_printf(ERR, "scaler pattern test failed in video channel 2 \n");
		return (0);
		}
	else {
		msg_printf(SUM, "scaler pattern test passed in video channel 2 \n");
		}

		return (1);

}

