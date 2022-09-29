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

#include <sys/immu.h>
#include "libsc.h"
#include "uif.h"
#include "ide_msg.h"
#include "mgras_diag.h"

#if HQ4

#if 0
void
resetcons(int argc, char **argv)
{
}

#endif

__uint32_t
_mgras_islinkalive (xbowreg_t   port)
{
	xbowreg_t 	widget_present; 
    	xbowreg_t   alive;
	__uint32_t	retVal = 0;
	xbow_t	   *xbow_wid = (xbow_t *)K1_MAIN_WIDGET(XBOW_ID);

	msg_printf(DBG, "Entering _mgras_islinkalive...\n");

    /* read the widget present bit in link aux status register */
#if 0
    XB_REG_RD_32((XB_LINK_AUX_STATUS(port)), XB_AUX_STAT_PRESENT, 
		widget_present);
#else

     widget_present = 
	(xbow_wid->xb_link(port).link_aux_status ) & (XB_AUX_STAT_PRESENT);
#endif
	
	widget_present >>= 5;
	msg_printf(DBG, "widget_present bit is 0x%x\n", widget_present);

    /* read the alive bit in link status register */
#if 0
	XB_REG_RD_32(XB_LINK_STATUS(port), XB_STAT_LINKALIVE, alive);

#else
	alive = (xbow_wid->xb_link(port).link_status) & (XB_STAT_LINKALIVE) ;
#endif

        alive >>= 31;
	msg_printf(DBG, "alive bit is 0x%x \n", alive);	

    /* check for widget present and alive bits */
	if (widget_present) {
	   if (!alive) {
    		msg_printf(DBG,"Widget Present in 0x%x: Link not alive\n", port);
			retVal = 0; /* error */
		}
		else {
		  msg_printf(DBG, "Widget Present in 0x%x: link is alive\n", port);
			retVal = 1; /* okay */
		}
	} else {
		msg_printf(DBG, "Widget Not Present in port 0x%x \n", port);
		retVal = 0;	/* error */
	}

	msg_printf(DBG, "Leaving _mgras_islinkalive...\n");

	return(retVal);
}

void
mgras_xbow_link_check(int argc, char **argv)
{
    __uint32_t  port = 0, widget_present, alive;

   if (argc < 2) {
     msg_printf(SUM, " Usage: mg_link_chk <port#> \n");
     return ;
   }

    atobu(argv[1], &port);

    msg_printf(SUM, "Checking link number 0x%x\n", port);

    /* read the widget present bit in link aux status register */
    XB_REG_RD_32((XB_LINK_AUX_STATUS(port)), XB_AUX_STAT_PRESENT, 
		widget_present);
	widget_present >>= 5;

    /* read the alive bit in link status register */
	XB_REG_RD_32(XB_LINK_STATUS(port), XB_STAT_LINKALIVE, alive);
        alive >>= 31;
	msg_printf(SUM, "widget present = 0x%x; alive bit is 0x%x \n", 
		widget_present, alive);	
}

/*
mgras_probe finds the port in which the gamera is plugged in.
When it finds the gamera, it checks if the link is alive.
If yes, it assigns the base address and set the port in 16bit by default.
*/

__uint32_t 
_mgras_hq4probe ( mgras_hw *mgbase , xbowreg_t port )
{
	__uint32_t	mfg, rev, part, xt_widget_id;

	msg_printf(DBG, "Entering _mgras_hq4probe...\n");

#if 0
     if (badaddr (&(mgbase->xt_widget_id), sizeof(__paddr_t)) ) {
         msg_printf(ERR, "Badaddr 0x%x \n",(__paddr_t) &(mgbase->xt_widget_id));
         return (0);
     }
#endif
	if (!(XBOW_WIDGET_IS_VALID(port)) ) {
	   msg_printf(DBG, " invalid port \n");
	   return (0);
        }

	if ( _mgras_islinkalive (port) )  { 
	    xt_widget_id = mgbase->xt_widget_id;
	    msg_printf(DBG, "xt_widget_id 0x%x\n", xt_widget_id);
	    mfg  = XWIDGET_MFG_NUM(xt_widget_id)     ;
	    rev  = XWIDGET_REV_NUM(xt_widget_id);
	    part = XWIDGET_PART_NUM(xt_widget_id);
	    msg_printf(DBG, "mfg 0x%x; rev 0x%x; part 0x%x\n", mfg, rev, part);
	
	    if ((mfg != HQ4_XT_ID_MFG_NUM_VALUE)  ||  /* (rev != HQ4_REV_NUM) || */
                (part != HQ4_XT_ID_PART_NUM_VALUE ) ) {
		msg_printf(DBG, "Incorrect HQ4 Widget ID:-\n\
		MFG_NUM 0x%x; REV_NUM 0x%x; PART_NUM 0x%x\n", mfg, rev, part);
		return (0);
	     }
	} else {
	    return (0);
	}
	if (rev <= 0x1)
	    msg_printf(SUM, "WARNING: Prototype HQ4 Revision 0x%x\n", rev );
	else 
	    msg_printf(DBG, "HQ4 Revision is 0x%x \n", rev ); 	

	msg_printf(DBG, "Leaving _mgras_hq4probe\n");

	return(1);
}
	

uchar_t mgras_regaccess_error (void)
{

    __uint32_t errors = 0;
	/*
		register access violation such as attempt to access an address mapped
		to a reserved portion of widget 0 address space,
		attempt to write to a read only  register or read from write-only
		register, an attempt to send a request packt size to widget 0 which 
		is not double word size
	*/

	XB_REG_RD_32 (WIDGET_CONTROL,  XB_WID_STAT_REG_ACC_ERR, recv_32) ;
	if ((recv_32 & XB_WID_STAT_REG_ACC_ERR) == 0)
		msg_printf(VRB, " The register access bit is set to default \n") ; 

	/* 
	check if bit is set when we write into the identification register 
	of the xbow.
	*/
	XB_REG_WR_32 (WIDGET_ID, THIRTYTWOBIT_MASK, THIRTYTWOBIT_MASK);  	
	XB_REG_RD_32 (WIDGET_CONTROL,  XB_WID_STAT_REG_ACC_ERR, recv_32);
	if ((recv_32 & XB_WID_STAT_REG_ACC_ERR) != 1) {
		msg_printf(VRB, " The register access bit is not set to one \n");
            errors++  ;
            goto _errors ;
    }
		return (0);		
	
_errors:
        return (errors);

}

__uint32_t 
_mgras_hq4linkreset (xbowreg_t port)
{
	__uint32_t *Addr;
	xbow_t	   *xbow_wid = (xbow_t *)K1_MAIN_WIDGET(XBOW_ID);

	/* 
	a write to this register will act as a "one-shot" forcing link
	reset  to be asserted for 4 core clock cycles
	After a write to this register, we call the check_defs function to
	make sure we reset correctly
	*/
	msg_printf(DBG, " Entering _mgras_hq4linkreset()...\n");

	xbow_wid->xb_link(port).link_reset = 1;

	msg_printf(DBG, " Leaving _mgras_hq4linkreset()...\n");

    return(PASSED);

}

#endif	/* HQ4 */

/*
	xbow_t	   *xbow_wid = (xbow_t *)K1_MAIN_WIDGET(XBOW_ID);
	xbow_wid->xb_link(port).link_aux_status = 1;
*/
