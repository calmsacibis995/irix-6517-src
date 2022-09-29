/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993 Silicon Graphics, Inc.	          *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#undef	R4000
#undef	R10000

#define	TFP	1

#include <sys/tfp.h>
#include <sys/EVEREST/IP21.h>

void
R8000InitSerialPort(void)
/*
 * Function: 
 * Purpose:
 * Parameters:
 * Returns:
 */
{
    extern	ccuart_init(unsigned long);

    ccuart_init(EV_UART_BASE);
}
