/**************************************************************************
 *									  *
 * 		 Copyright (C) 1993, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* RAM "disk" defines */

#ident	"$Revision: 1.1 $"

#define	MAXRAMUNITS	4

/* master.d/ram ramflags */

#define	RAM_EFS		0x01	/* "mkfs" an EFS file system on 1st open */
#define	RAM_FREE	0x02	/* free (erase) the "device" on close */
