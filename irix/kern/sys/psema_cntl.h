/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990-1991 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.5 $"

#ifndef __SYS_PSEMA_CNTL_H__
#define __SYS_PSEMA_CNTL_H__

#ifdef __cplusplus
extern "C" {
#endif

/*
 * psema_cntl commands.
 */

#define PSEMA_OPEN		1
#define PSEMA_CLOSE		2
#define PSEMA_UNLINK		3
#define PSEMA_POST		4
#define PSEMA_WAIT		5
#define PSEMA_TRYWAIT		6
#define PSEMA_GETVALUE		7
#define PSEMA_BROADCAST		8
#define PSEMA_WAIT_USERPRI	9

#ifdef _KERNEL
int psema_indirect_close(vnode_t *vp);
int psema_indirect_unlink(vnode_t *vp);
#else
extern int psema_cntl(int cmd, ...);
#endif

#ifdef __cplusplus
}
#endif
#endif	/* __SYS_PSEMA_CNTL_H__ */
