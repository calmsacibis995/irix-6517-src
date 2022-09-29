/*	Copyright (c) 1990, 1991 UNIX System Laboratories, Inc.	*/
/*	Copyright (c) 1988 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.                     	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"@(#)libc-port:gen/lfmt_log.c	1.4"

/* lfmt_log() - log info */
#include "synonyms.h"
#include <pfmt.h>
#include <stdio.h>
#include <sys/types.h>
#include <stropts.h>
#include <sys/strlog.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/syslog.h>
#include "pfmt_data.h"

#define MAXMSG	1024
#define LOGNAME	"/dev/conslog"

/* move out of function scope so we get a global symbol for use with data cording */
static int fd = -1;

int
__lfmt_log(const char *text, const char *sev,
	 va_list args, long flag, int ret)
{
	struct log_ctl hdr;
	struct strbuf dat, ctl;
	int msg_offset;
	int len;
	char msgbuf[MAXMSG];

	len = ret + (int)sizeof(long) + 3;

	/* message can't be longer than 256 bytes */

	if (len > 256){
		setoserror(ERANGE);
		return -2;
	}

	*(long *)msgbuf = flag;
	msg_offset = sizeof(long);

	if (*__pfmt_label)
		msg_offset += sprintf(msgbuf + msg_offset, __pfmt_label);
	msgbuf[msg_offset++] = '\0';

	if (sev)
		msg_offset += sprintf(msgbuf + msg_offset, sev, flag & 0xff);
	msgbuf[msg_offset++] = '\0';

	msg_offset += 1 + vsprintf(msgbuf + msg_offset, text, args);

	if (fd == -1 &&
		((fd = open(LOGNAME, O_WRONLY)) == -1 || fcntl(fd, F_SETFD, 1) == -1))
		return -2;

	hdr.pri = LOG_LFMT|LOG_ERR;
	hdr.flags = SL_CONSOLE;
	hdr.level = 0;
	ctl.maxlen = ctl.len = sizeof hdr;
	ctl.buf = (caddr_t)&hdr;
	dat.maxlen = MAXMSG;
	dat.len = msg_offset;
	dat.buf = msgbuf;

	if (putmsg(fd, &ctl, &dat, 0) == -1)
		return -2;

	return ret;
}
