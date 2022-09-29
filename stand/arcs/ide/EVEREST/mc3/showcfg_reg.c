/*
 * mc3/displayconfig_reg.c
 *
 *
 * Copyright 1991, 1992 Silicon Graphics, Inc.
 * All Rights Reserved.
 *
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or
 * duplicated in any form, in whole or in part, without the prior written
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished -
 * rights reserved under the Copyright Laws of the United States.
 */

#ident "$Revision: 1.8 $"

/*
 * connect -- 	
 *		This is a memory sockets connection test. It writes patterns
 *		to the first 16 bytes of each SIMM then
 *		read it back. If doesn't match, the socket has connection
 *		problem.
 *
 *
 * NOTICE: Reserved 1M memory for diagnostics.
 */

#include "pattern.h"
#include <ide_msg.h>
#include <everr_hints.h>
#include <prototypes.h>


showcfg_reg()
{

    msg_printf(SUM, "Read memory configuration registers\n");

	/* probably get rid of this eventually and use the scripting */
	/* language to call readconfig_reg */

	readconfig_reg(RDCONFIG_DEBUG,1);
	return (0);
}
