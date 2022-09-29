#ifndef __PATHS_H__
#define __PATHS_H__
#ifdef __cplusplus
extern "C" {
#endif
#ident "$Revision: 1.22 $"
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
/*
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted provided
 * that: (1) source distributions retain this entire copyright notice and
 * comment, and (2) distributions including binaries display the following
 * acknowledgement:  ``This product includes software developed by the
 * University of California, Berkeley and its contributors'' in the
 * documentation or other materials provided with the distribution and in
 * all advertising materials mentioning features or use of this software.
 * Neither the name of the University nor the names of its contributors may
 * be used to endorse or promote products derived from this software without
 * specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 *
 *	@(#)paths.h	5.7 (Berkeley) 6/24/90
 */

#define	_PATH_BSHELL	"/sbin/sh"
#define	_PATH_CONFIG	"/var/config"
#define	_PATH_CONSOLE	"/dev/console"
#define	_PATH_CPP	"/usr/lib/cpp"
#define	_PATH_CSHELL	"/bin/csh"
#define	_PATH_DEV	"/dev/"
#define	_PATH_DEVNULL	"/dev/null"
#define	_PATH_DEVPTC	"/dev/ptc"
#define	_PATH_DEVPTC1	"/dev/ptc1"
#define	_PATH_DEVPTC2	"/dev/ptc2"
#define	_PATH_DEVPTC3	"/dev/ptc3"
#define	_PATH_DEVPTC4	"/dev/ptc4"
#define	_PATH_DIALUPS	"/etc/dialups"
#define	_PATH_DIALPASS	"/etc/d_passwd"
#define	_PATH_GROUP	"/etc/group"
#define	_PATH_INITTAB	"/etc/inittab"
#define	_PATH_ISSUE	"/etc/issue"
#define	_PATH_KMEM	"/dev/kmem"
#define	_PATH_LASTLOG	"/var/adm/lastlog"
#define	_PATH_LOGINLOG	"/var/adm/loginlog"
#define	_PATH_LOG	"/dev/log"
#define _PATH_LS        "/sbin/ls"
#define	_PATH_MAILDIR	"/usr/mail/"
#define	_PATH_MAN	"/usr/man"
#define	_PATH_MAGIC	"/etc/magic"
#define	_PATH_MEM	"/dev/mem"
#define	_PATH_NOLOGIN	"/etc/nologin"
#define	_PATH_PASSWD	"/etc/passwd"
#define _PATH_PROJECT	"/etc/project"
#define _PATH_PROJID	"/etc/projid"
#define	_PATH_QUOTA	"/usr/bsd/quota"
#define	_PATH_SCHEME	"/usr/lib/iaf/scheme"
#define	_PATH_SENDMAIL	"/usr/lib/sendmail"
#define	_PATH_TMP	"/tmp/"
#define	_PATH_TTY	"/dev/tty"
#define	_PATH_UNIX	"/unix"
#define	_PATH_VARTMP	"/var/tmp/"
#define	_PATH_VI	"/usr/bin/vi"
#define	_PATH_PROCFS	"/proc/"
#define	_PATH_HWGFS	"/hw/"
#define	_PATH_PROCFSPI	"/proc/pinfo/"

#define _PATH_ROOTPATH	"/usr/sbin:/usr/bsd:/sbin:/usr/bin:/etc:/usr/etc:/usr/bin/X11"
#define _PATH_USERPATH	"/usr/sbin:/usr/bsd:/sbin:/usr/bin:/usr/bin/X11:"

#ifdef __cplusplus
}
#endif
#endif /* !__PATHS_H__ */
