#ifndef __FCNTL_H__
#define __FCNTL_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.24 $"
/*
*
* Copyright 1992, Silicon Graphics, Inc.
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
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/


#include <sys/types.h>
#include <sys/fcntl.h>

#ifndef S_IRWXU
#define S_IRWXU      00700   /* read, write, execute: owner */
#define S_IRUSR      00400   /* read permission: owner */
#define S_IWUSR      00200   /* write permission: owner */
#define S_IXUSR      00100   /* execute permission: owner */
#define S_IRWXG      00070   /* read, write, execute: group */
#define S_IRGRP      00040   /* read permission: group */
#define S_IWGRP      00020   /* write permission: group */
#define S_IXGRP      00010   /* execute permission: group */
#define S_IRWXO      00007   /* read, write, execute: other */
#define S_IROTH      00004   /* read permission: other */
#define S_IWOTH      00002   /* write permission: other */
#define S_IXOTH      00001   /* execute permission: other */
#define S_ISUID      0x800   /* set user id on execution */
#define S_ISGID      0x400   /* set group id on execution */
#endif /* S_IRWXU */

#ifndef SEEK_SET
#   define SEEK_SET     0       /* Set file pointer to "offset" */
#   define SEEK_CUR     1       /* Set file pointer to current plus "offset" */
#   define SEEK_END     2       /* Set file pointer to EOF plus "offset" */
#endif

extern int fcntl(int, int, ...);
extern int open(const char *, int, ...);
extern int creat(const char *, mode_t);

#if _LFAPI
extern int open64(const char *, int, ...);
extern int creat64(const char *, mode_t);
#endif /* _LFAPI */

#ifdef __cplusplus
}
#endif
#endif /* !__FCNTL_H__ */
