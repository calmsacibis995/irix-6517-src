#ifndef __CPIO_H__
#define __CPIO_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.3 $"
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



/* The following are values used by c_mode field of the cpio archive.
*/

#define C_IRUSR		0000400
#define C_IWUSR		0000200
#define C_IXUSR		0000100
#define C_IRGRP		0000040
#define C_IWGRP		0000020
#define C_IXGRP		0000010
#define C_IROTH		0000004
#define C_IWOTH		0000002
#define C_IXOTH		0000001
#define C_ISUID		0004000
#define C_ISGID		0002000
#define C_ISVTX		0001000
#define C_ISDIR		0040000
#define C_ISFIFO	0010000
#define C_ISREG		0100000
#define C_ISBLK		0060000
#define C_ISCHR		0020000
#define C_ISCTG		0110000
#define C_ISLNK		0120000
#define C_ISSOCK	0140000

#define MAGIC		"070707"

#ifdef __cplusplus
}
#endif
#endif /* !__CPIO_H__ */
