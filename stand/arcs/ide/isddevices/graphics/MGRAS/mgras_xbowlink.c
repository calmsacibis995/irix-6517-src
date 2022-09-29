#if HQ4
/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1996, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

#include <sys/types.h>
#include <sys/cpu.h>
#include "uif.h"
#include "libsc.h"
#include "sys/mgrashw.h"
#include "mgras_diag.h"
#include "mgras_hq3.h"
#include "ide_msg.h"

/*
	This function checks the default values of all the readable registers.
*/

uchar_t mgras_checkdefs (void ) 
{
     __uint32_t errors = 0;
	hq4reg *regptr;

	regptr = hq4regs_t;

	for ( regptr = hq4regs_t; regptr->name[0] != '\0'; regptr++ )
		if ((regptr->mode == _RDONLY_) || (regptr->mode == _RDWR_)) {
			HQ3_PIO_RD(regptr->address, regptr->mask, recv_32);
			if ((regptr->def	& regptr->mask) != recv_32)
				msg_printf(ERR, "Default Value of %s: Addr 0x%x mask 0x%x Exp 0x%x Rcv 0x%x \n", regptr->name, regptr->address, regptr->mask, regptr->def, recv_32) ;
	}
	return (errors);
}



__uint32_t 
_mgras_hq4_reg_test32 ( __uint32_t addr, __uint32_t mask, __uint32_t exp, 
					__uint32_t	recv, char *str)
{
    /* write the register first */
	HQ_WR ( addr, exp, mask);

    /* read the widget ide - corrupt the xtalk interconnection */
	HQ_RD ( mgbase->xt_widget_id, THIRTYTWOBIT_MASK, recv);

    /* read the register */
	HQ_RD ( addr, mask, recv);

	/* compare, exp_32 and recv_32 */
	if ((exp & mask) != ( recv & mask) ) {
		msg_printf(ERR, "  %s  Addr 0x%x mask 0x%x exp 0x%x recv 0x%x \n",
			str, addr, mask, exp, recv);
		return (1);
	}
	return (0);
}

__uint32_t 
_mgras_hq4_reg_test64 ( __uint32_t addr, __uint64_t mask, __uint64_t exp,
					__uint64_t	recv, char *str)
{
    /* write the register first */
    HQ_WR ( addr, exp, mask);

    /* read the widget ide - corrupt the xtalk interconnection */
    HQ_RD (mgbase->xt_widget_id, SIXTYFOURBIT_MASK, recv);

    /* read the register */
	HQ_RD ( addr, mask, recv);

	/* compare exp_64 and recv_64 */
    if ((exp & mask) != ( recv & mask) ) {
        msg_printf(ERR, "  %s  Addr 0x%x mask 0x%x exp 0x%x recv 0x%x \n", str, addr, mask, exp, recv);
        return (1);
    }
	return (0);

}

__uint32_t 
mgras_hq4_reg_test (void)
{

	unsigned char i;
	
	msg_printf(DBG, "Entering mgras_hq4_reg_test...\n");
 
	/* walking 1s on request timeout, Interrupt Dest Hi and LO registers */
	for (i = 0, exp_32 = 1; i < 32; i++, exp_32 <<=1)
	{
		if ( _mgras_hq4_reg_test32 ( mgbase->xt_req_timeout, TWENTYBIT_MASK, exp_32, 
						recv_32, (char *) "WALK0:WIDGET_REQ_TIMEOUT_REG" ) )
			goto _errors ;
		if ( _mgras_hq4_reg_test32 ( mgbase->xt_int_dst_addr_hi, THIRTYTWOBIT_MASK, 
						exp_32, recv_32, (char *) "WALK0:WIDGET_INTR_DST_HI_REG" ) )
			goto _errors ;
		if ( _mgras_hq4_reg_test32 ( mgbase->xt_int_dst_addr_lo, THIRTYTWOBIT_MASK, 
						exp_32, recv_32, (char *) "WALK0:WIDGET_INTR_DST_LO_REG" ) )
			goto _errors ;
		if ( _mgras_hq4_reg_test32 ( mgbase->gfx_flow_ctl_timeout, TENBIT_MASK, 
						exp_32, recv_32, (char *) "WALK0:GFX_FLOW_CNTL_TIMEOUT" ) )
			goto _errors ;
        HQ_WR(mgbase->flag_enab_clr, THIRTYTWOBIT_MASK, ~0x0);
        if ( _mgras_hq4_reg_test32 ( mgbase->flag_enab_set, THIRTYTWOBIT_MASK,
                        exp_32, recv_32, (char *) "WALK0:FLAG_ENAB_SET") )
            goto _errors ;

        HQ_WR(mgbase->flag_clr_priv, THIRTYTWOBIT_MASK, ~0x0);
        if ( _mgras_hq4_reg_test32 ( mgbase->flag_set_priv, THIRTYTWOBIT_MASK,
                        exp_32, recv_32, (char *) "WALK0:FLAG_SET_PRIV") )
            goto _errors ;

        HQ_WR(mgbase->flag_intr_enab_clr, THIRTYTWOBIT_MASK, ~0x0);
        if ( _mgras_hq4_reg_test32 ( mgbase->flag_intr_enab_set, THIRTYTWOBIT_MASK,
                        exp_32, recv_32, (char *) "WALK0:FLAG_INTR_ENAB_SET") )
            goto _errors ;

        HQ_WR(mgbase->stat_intr_enab_clr, THIRTYTWOBIT_MASK, ~0x0);
        if ( _mgras_hq4_reg_test32 ( mgbase->stat_intr_enab_set, THIRTYTWOBIT_MASK,
                        exp_32, recv_32, (char *) "WALK0:STAT_INTR_ENAB_SET") )
            goto _errors ;
	}

	/* walking 0s on  request timeout, Interrupt Dest Hi and LO registers */

	for ( i = 0, exp_32 = 0xfffffffe ; i < 32; i++, exp_32 = ((exp_32 <<=1)) ) {
		if ( _mgras_hq4_reg_test32 ( mgbase->xt_req_timeout, TWENTYBIT_MASK, exp_32,
						recv_32, (char *) "WALK1:WIDGET_REQ_TIMEOUT_REG" ))
			goto _errors ;
		if ( _mgras_hq4_reg_test32 ( mgbase->xt_int_dst_addr_hi, THIRTYTWOBIT_MASK, exp_32,
						recv_32, (char *) "WALK1:WIDGET_INTR_DST_HI_REG" ) )
			goto _errors ;
		if ( _mgras_hq4_reg_test32 ( mgbase->xt_int_dst_addr_lo, THIRTYTWOBIT_MASK, exp_32,
						recv_32, (char *) "WALK1:WIDGET_INTR_DST_LO_REG" )) 
			goto _errors ;
		if ( _mgras_hq4_reg_test32 ( mgbase->gfx_flow_ctl_timeout, TENBIT_MASK, 
						exp_32, recv_32, (char *) "WALK1:GFX_FLOW_CNTL_TIMEOUT" ) )

        HQ_WR(mgbase->flag_enab_clr, THIRTYTWOBIT_MASK, ~0x0);
        if ( _mgras_hq4_reg_test32 ( mgbase->flag_enab_set, THIRTYTWOBIT_MASK,
                        exp_32, recv_32, (char *) "WALK1:FLAG_ENAB_SET") )
            goto _errors ;

        HQ_WR(mgbase->flag_clr_priv, THIRTYTWOBIT_MASK, ~0x0);
        if ( _mgras_hq4_reg_test32 ( mgbase->flag_set_priv, THIRTYTWOBIT_MASK,
                        exp_32, recv_32, (char *) "WALK1:FLAG_SET_PRIV") )
            goto _errors ;

        HQ_WR(mgbase->flag_intr_enab_clr, THIRTYTWOBIT_MASK, ~0x0);
        if ( _mgras_hq4_reg_test32 ( mgbase->flag_intr_enab_set, THIRTYTWOBIT_MASK,
                        exp_32, recv_32, (char *) "WALK1:FLAG_INTR_ENAB_SET") )
            goto _errors ;

        HQ_WR(mgbase->stat_intr_enab_clr, THIRTYTWOBIT_MASK, ~0x0);
        if ( _mgras_hq4_reg_test32 ( mgbase->stat_intr_enab_set, THIRTYTWOBIT_MASK,
                        exp_32, recv_32, (char *) "WALK1:STAT_INTR_ENAB_SET") )
            goto _errors ;
	}
		
    REPORT_PASS_OR_FAIL((&HQ4_err[HQ4_REG_TEST]), 0);

_errors:
    REPORT_PASS_OR_FAIL((&HQ4_err[HQ4_REG_TEST]), 1);
}

__uint32_t
mgras_addruniq (void)
{
	return 0;
}

#endif /* HQ4 */
