/*
 * Copyright (c) 1991 by Sun Microsystems, Inc.
 */
#ident	"$Header: /proj/irix6.5.7m/isms/eoe/cmd/sun/snoop/RCS/snoop_mount.c,v 1.2 1996/07/06 17:50:54 nn Exp $"

#include <sys/types.h>
#include <sys/errno.h>
#include <setjmp.h>
#include <sys/tiuser.h>

#include <rpc/types.h>
#include <rpc/xdr.h>
#include <rpc/auth.h>
#include <rpc/clnt.h>
#include <rpc/rpc_msg.h>
#include <sys/fs/nfs.h>
#ifdef sgi
#include <sys/fs/nfs.h>
#else
#include <nfs/nfs.h>
#endif
#include <rpcsvc/mount.h>
#include <string.h>
#include "snoop.h"

#ifdef sgi
#define	AUTH_KERB	4
#endif

#ifndef MIN
#define	MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

extern char *dlc_header;
extern jmp_buf xdr_err;

void sum_mountstat();
void sum_mountstat3();
char *sum_mountfh();
char *sum_mountfh3();
char *sum_exports();
char *sum_mounts();

int detail_mountstat();
void detail_mountstat3();
void detail_mountfh();
void detail_mountfh3();
void detail_exports();
void detail_mounts();
void mountcall(int proc, int vers);
void mountreply(int proc, int vers);

static char *procnames_short[] = {
	"Null",			/*  0 */
	"Mount",		/*  1 */
	"Get mount list",	/*  2 */
	"Unmount",		/*  3 */
	"Unmountall",		/*  4 */
	"Get export list",	/*  5 */
	"Get export list",	/*  6 */
	"PATHCONF",		/*  7 */
};

static char *procnames_long[] = {
	"Null procedure",		/*  0 */
	"Add mount entry",		/*  1 */
	"Return mount entries",		/*  2 */
	"Remove mount entry",		/*  3 */
	"Remove all mount entries",	/*  4 */
	"Return export list",		/*  5 */
	"Return export list",		/*  6 */
	"Get POSIX Pathconf info",	/*  7 */
};

#define	MAXPROC	7

void
interpret_mount(flags, type, xid, vers, proc, data, len)
	int flags, type, xid, vers, proc;
	char *data;
	int len;
{
	char *line;
	char buff[1024];

	if (proc < 0 || proc > MAXPROC)
		return;

	if (flags & F_SUM) {
		if (setjmp(xdr_err)) {
			return;
		}

		line = get_sum_line();

		if (type == CALL) {
			(void) sprintf(line, "MOUNT%d C %s",
				vers, procnames_short[proc]);
			line += strlen(line);
			switch (proc) {
			case MOUNTPROC_MNT:
			case MOUNTPROC_UMNT:
				(void) sprintf(line, " %s",
					getxdr_string(buff, 1024));
				break;
			case MOUNTPROC_DUMP:
			case MOUNTPROC_UMNTALL:
			case MOUNTPROC_EXPORT:
			case MOUNTPROC_EXPORTALL:
#ifdef MOUNTPROC_PATHCONF
			case MOUNTPROC_PATHCONF:
				if (vers != 3)
					(void) sprintf(line, " %s",
						getxdr_string(buff, 1024));
#endif
				break;
			default:
				break;
			}

			check_retransmit(line, xid);
		} else {
			(void) sprintf(line, "MOUNT%d R %s ",
				vers, procnames_short[proc]);
			line += strlen(line);
			switch (proc) {
			case MOUNTPROC_MNT:
				if (vers == 3)
					sum_mountstat3(line);
				else
					sum_mountstat(line);
				break;
			case MOUNTPROC_DUMP:
				(void) sprintf(line, sum_mounts());
				break;
			case MOUNTPROC_UMNT:
			case MOUNTPROC_UMNTALL:
				(void) sprintf(line, "reply");
				break;
			case MOUNTPROC_EXPORTALL:
				if (vers == 3)
					break;
			case MOUNTPROC_EXPORT:
				(void) sprintf(line, sum_exports());
				break;
#ifdef MOUNTPROC_PATHCONF
			case MOUNTPROC_PATHCONF:
				if (vers != 2)
					break;
#ifdef notyet
				(void) sprintf(line, sum_ppathcnf());
#endif
				break;
#endif
			default:
				break;
			}
		}
	}

	if (flags & F_DTAIL) {
		show_header("MOUNT:", "NFS MOUNT", len);
		show_space();
		if (setjmp(xdr_err)) {
			return;
		}
		(void) sprintf(get_line(0, 0),
			"Proc = %d (%s)",
			proc, procnames_long[proc]);
		if (type == CALL)
			mountcall(proc, vers);
		else
			mountreply(proc, vers);
		show_trailer();
	}
}

/*
 *  Interpret call packets in detail
 */
void
mountcall(proc, vers)
	int proc, vers;
{

	switch (proc) {
	case MOUNTPROC_MNT:
	case MOUNTPROC_UMNT:
		(void) showxdr_string(1024, "Directory = %s");
		break;
	case MOUNTPROC_DUMP:
		break;
	case MOUNTPROC_UMNTALL:
		break;
	case MOUNTPROC_EXPORTALL:
		if (vers == 3)
			break;
		break;
	case MOUNTPROC_EXPORT:
		break;
#ifdef MOUNTPROC_PATHCONF
	case MOUNTPROC_PATHCONF:
		if (vers != 2)
			break;
		(void) showxdr_string(1024, "File = %s");
		break;
#endif
	default:
		break;
	}
}

/*
 *  Interpret reply packets in detail
 */
void
mountreply(proc, vers)
	int proc, vers;
{

	switch (proc) {
	case MOUNTPROC_MNT:
		if (vers == 3) {
			detail_mountstat3();
		} else {
			if (detail_mountstat() == 0) {
				detail_mountfh();
			}
		}
		break;
	case MOUNTPROC_DUMP:
		detail_mounts();
		break;
	case MOUNTPROC_UMNT:
	case MOUNTPROC_UMNTALL:
		(void) detail_mountstat();
		break;
	case MOUNTPROC_EXPORTALL:
		if (vers == 3)
			break;
	case MOUNTPROC_EXPORT:
		detail_exports();
		break;
#ifdef MOUNTPROC_PATHCONF
	case MOUNTPROC_PATHCONF:
#ifdef notyet
		(void) detail_ppathcnf();
#endif
		break;
#endif
	default:
		break;
	}
}

void
sum_mountstat(line)
	char *line;
{
	u_long status;
	char *str;

	status = getxdr_u_long();
	if (status == 0)
		str = "OK";
	else if ((str = strerror(status)) == (char *) NULL)
		str = "";
	(void) strcpy(line, str);
	if (status == 0) {
		(void) strcat(line, sum_mountfh());
	}
}

int
detail_mountstat()
{
	u_long status;
	char *str;

	status = getxdr_u_long();
	if (status == 0)
		str = "OK";
	else if ((str = strerror(status)) == (char *) NULL)
		str = "";

	(void) sprintf(get_line(0, 0), "Status = %d (%s)", status, str);

	return ((int) status);
}

char *
sum_mountfh()
{
	int i, l;
	int fh = 0;
	static char buff[8];

	for (i = 0; i < NFS_FHSIZE; i += 4) {
		l = getxdr_long();
		fh ^= (l >> 16) ^ l;
	}
	(void) sprintf(buff, " FH=%04X", fh & 0xFFFF);
	return (buff);
}

void
detail_mountfh()
{

	(void) showxdr_hex(NFS_FHSIZE / 2, "File handle = %s");
	(void) showxdr_hex(NFS_FHSIZE / 2, "              %s");
}

char *
print_auth()
{
	int i, auth, flavors;
	char *p;
	static char buff[32];

	buff[0] = '\0';
	flavors = getxdr_long();
	for (i = 0; i < flavors; i++) {
		if (i > 0)
			(void) strcat(buff, ",");
		switch (auth = getxdr_u_long()) {
		case AUTH_NONE:
			(void) strcat(buff, "none");
			break;
		case AUTH_UNIX:
			(void) strcat(buff, "unix");
			break;
		case AUTH_SHORT:
			(void) strcat(buff, "short");
			break;
		case AUTH_DES:
			(void) strcat(buff, "des");
			break;
		case AUTH_KERB:
			(void) strcat(buff, "kerb");
			break;
		default:
			p = buff + strlen(buff);
			(void) sprintf(p, "%d", auth);
			break;
		}
	}
	return (buff);
}

void
sum_mountstat3(line)
	char *line;
{
	u_long status;
	char *str;

	status = getxdr_u_long();
	if (status == 0)
		str = "OK";
	else if ((str = strerror(status)) == (char *) NULL)
		str = "";
	(void) strcpy(line, str);
	if (status == 0) {
		(void) strcat(line, sum_mountfh3());
		(void) strcat(line, " Auth=");
		(void) strcat(line, print_auth());
	}
}

void
detail_mountstat3()
{
	u_long status;
	char *str;

	status = getxdr_u_long();
	if (status == 0)
		str = "OK";
	else if ((str = strerror(status)) == (char *) NULL)
		str = "";

	(void) sprintf(get_line(0, 0), "Status = %d (%s)", status, str);
	if (status == 0) {
		detail_mountfh3();
		(void) sprintf(get_line(0, 0),
			"Authentication flavor = %s",
			print_auth());
	}
}

char *
sum_mountfh3()
{
	int i, l, len;
	int fh = 0;
	static char buff[8];

	len = getxdr_long();
	for (i = 0; i < len; i += 4) {
		l = getxdr_long();
		fh ^= (l >> 16) ^ l;
	}
	(void) sprintf(buff, " FH=%04X", fh & 0xFFFF);
	return (buff);
}

void
detail_mountfh3()
{
	int i, l, len;

	len = getxdr_long();
	l = MIN(len, 16);
	(void) showxdr_hex(l, "File handle = %s");
	i = l;
	while (i < len) {
		l = MIN(len - i, 16);
		(void) showxdr_hex(l, "              %s");
		i += l;
	}
}

char *
sum_exports()
{
	static char buff[1024];
	int entries = 0;

	if (setjmp(xdr_err)) {
		(void) sprintf(buff, "%d+ entries", entries);
		return (buff);
	}

	while (getxdr_long()) {
		(void) getxdr_string(buff, 1024);
		while (getxdr_long()) {
			(void) getxdr_string(buff, 255);
		}
		entries++;
	}

	(void) sprintf(buff, "%d entries", entries);
	return (buff);
}

void
detail_exports()
{
	int entries = 0;
	char *dirpath, *grpname;
	char buff[1024];

	if (setjmp(xdr_err)) {
		(void) sprintf(get_line(0, 0),
			" %d+ entries. (Frame is incomplete)",
			entries);
		return;
	}

	while (getxdr_long()) {
		dirpath = (char *) getxdr_string(buff, 1024);
		(void) sprintf(get_line(0, 0), "Directory = %s", dirpath);
		entries++;
		while (getxdr_long()) {
			grpname = (char *) getxdr_string(buff, 255);
			(void) sprintf(get_line(0, 0), " Group = %s", grpname);
		}
	}
}

char *
sum_mounts()
{
	int entries = 0;
	char buff[1024];

	if (setjmp(xdr_err)) {
		(void) sprintf(buff, "%d+ entries", entries);
		return (buff);
	}

	while (getxdr_long()) {
		(void) getxdr_string(buff, 255);
		(void) getxdr_string(buff, 1024);
		entries++;
	}

	(void) sprintf(buff, "%d entries", entries);
	return (buff);
}

void
detail_mounts()
{
	int entries = 0;
	char *hostname, *directory;
	char buff1[1024], buff2[1024];

	if (setjmp(xdr_err)) {
		(void) sprintf(get_line(0, 0),
			" %d+ entries. (Frame is incomplete)",
			entries);
		return;
	}

	(void) sprintf(get_line(0, 0), "Mount list");

	while (getxdr_long()) {
		hostname  = (char *) getxdr_string(buff1, 255);
		directory = (char *) getxdr_string(buff2, 1024);
		(void) sprintf(get_line(0, 0), "   %s:%s", hostname, directory);
		entries++;
	}
}
