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

#undef	R8000
#undef	TFP
#undef	R10000

#define	R4000 1

#include <sys/sbd.h>
#include <sys/EVEREST/IP19.h>

#include "sl.h"

void
R4000InitSerialPort(void)
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
