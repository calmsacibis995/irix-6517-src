
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __SO_DAC_
#define __SO_DAC_
#ident	"$Revision: 1.2 $"

#ifdef __cplusplus
extern "C" {
#endif

#include <sys/types.h>

#define	WILDACL			-1
#define	SOACL_MAX		25    /* Struct must not exceed mbuf */

struct soacl {
	short so_uidcount;     /* UID count */
	uid_t so_uidlist[SOACL_MAX];    /* List of UIDs */
};

int soacl_ok(struct soacl *);

#ifdef __cplusplus
}
#endif

#endif /* __SO_DAC_ */







