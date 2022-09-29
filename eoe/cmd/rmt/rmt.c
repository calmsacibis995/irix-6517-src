#ident "$Revision: 1.7 $"
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1986, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 */

#ifndef lint
char copyright[] =
"@(#) Copyright (c) 1983 Regents of the University of California.\n\
 All rights reserved.\n";
#endif /* not lint */

#ifndef lint
static char sccsid[] = "@(#)rmt.c	5.2 (Berkeley) 1/7/86";
#endif /* not lint */

/*
** This version number should sync up with the librmt.a version number,
** LIBRMT_VERSION, in lib/librmt/rmtlib.h.
** This is done so that during a remote open, we can decide if we are compatible
** enough with the library to run.
*/
#define	RMT_VERSION	2

/*
 * rmt
 */
#include <stdio.h>
#include <sgtty.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/mtio.h>
#include <errno.h>
#include <sys/tpsc.h>
#include <sys/stat.h>

int	tape = -1;

char	*record;
int	maxrecsize = -1;
char	*checkbuf();

#define	SSIZE	64
char	device[SSIZE];
char	count[SSIZE], mode[SSIZE], pos[SSIZE], op[SSIZE];

extern	errno;
extern	char	*sys_errlist[];
char	resp[BUFSIZ];
struct stat	statbuf;
int	client_version = -1;

long	lseek();

FILE	*debug;
#define	DEBUG(f)	if (debug) fprintf(debug, f)
#define	DEBUG1(f,a)	if (debug) fprintf(debug, f, a)
#define	DEBUG2(f,a1,a2)	if (debug) fprintf(debug, f, a1, a2)

main(argc, argv)
	int argc;
	char **argv;
{
	int rval;
	char c;
	int n, i, cc;

	argc--, argv++;
	if (argc > 0) {
		debug = fopen(*argv, "w");
		if (debug == 0)
			exit(1);
		(void) setbuf(debug, (char *)0);
	}
top:
	errno = 0;
	rval = 0;
	if (read(0, &c, 1) != 1)
		exit(0);
	switch (c) {

	case 'Z':	/* fstat */
		getstring(device);	/* ignored */
		rval = fstat( tape, &statbuf );
		if ( rval < 0 )
			goto ioerror;
		rval = sizeof (statbuf);
		(void) sprintf(resp, "A%d\n", rval);
		(void) write(1, resp, strlen(resp));
		(void) write(1, (char *)&statbuf, sizeof (statbuf));
		goto top;
		
	case 'O':
		if (tape >= 0)
			(void) close(tape);
		getstring(device); getstring(mode);
		DEBUG2("rmtd: O %s %s\n", device, mode);
		tape = open(device, atoi(mode));
		if (tape < 0)
			goto ioerror;
		goto respond;

	case 'V':
		getstring(device);	/* client version # */
		client_version = atoi(device);
		DEBUG1("rmtd: V%d\n", client_version);
		/* in the future, should check if client_version is less
		** than RMT_VERSION then return client_version else
		** return RMT_VERSION
		** Since they are equal for now so it doesn't matter
		*/
		rval = RMT_VERSION;
		goto respond;

	case 'C':
		DEBUG("rmtd: C\n");
		getstring(device);		/* discard */
		if (close(tape) < 0)
			goto ioerror;
		tape = -1;
		goto respond;

	case 'L':
		getstring(count); getstring(pos);
		DEBUG2("rmtd: L %s %s\n", count, pos);
		rval = lseek(tape, (long) atoi(count), atoi(pos));
		if (rval < 0)
			goto ioerror;
		goto respond;

	case 'W':
		getstring(count);
		n = atoi(count);
		DEBUG1("rmtd: W %s\n", count);
		record = checkbuf(record, n);
		for (i = 0; i < n; i += cc) {
			cc = read(0, &record[i], n - i);
			if (cc <= 0) {
				DEBUG("rmtd: premature eof\n");
				exit(2);
			}
		}
		rval = write(tape, record, n);
		if (rval < 0)
			goto ioerror;
		goto respond;

	case 'R':
		getstring(count);
		DEBUG1("rmtd: R %s\n", count);
		n = atoi(count);
		record = checkbuf(record, n);
		rval = read(tape, record, n);
		if (rval < 0)
			goto ioerror;
		(void) sprintf(resp, "A%d\n", rval);
		(void) write(1, resp, strlen(resp));
		(void) write(1, record, rval);
		goto top;

	case 'I':
		getstring(op); getstring(count);
		DEBUG2("rmtd: I %s %s\n", op, count);
		{ struct mtop mtop;
		  mtop.mt_op = atoi(op);
		  mtop.mt_count = atoi(count);
		  if (ioctl(tape, MTIOCTOP, (char *)&mtop) < 0)
			goto ioerror;
		  rval = mtop.mt_count;
		}
		goto respond;

	case 'S':		/* status */
		DEBUG("rmtd: S\n");
		{ struct mtget mtget;
		  char *mtgetp;

		  if (ioctl(tape, MTIOCGET, (char *)&mtget) < 0)
			goto ioerror;
		  /* starting from 3.2, we increase the size of
		  ** the mtget struct by 4bytes, so if client
		  ** is of the old version then we have to create an old
		  ** mtget struct and give them that instead of the
		  ** new larger struct which might make them core dump
		  */
		  if (client_version == -1) {
			struct old_mtget omtget;

			omtget.mt_type = mtget.mt_type;
			omtget.mt_dsreg = mtget.mt_dsreg;
			omtget.mt_erreg = mtget.mt_erreg;
			omtget.mt_resid = mtget.mt_resid;
			omtget.mt_fileno = mtget.mt_fileno;
			omtget.mt_blkno = mtget.mt_blkno;
		  	rval = sizeof (omtget);
			mtgetp = (char *)&omtget;
		  } else {
		  	rval = sizeof (mtget);
			mtgetp = (char *)&mtget;
		  }
		  (void) sprintf(resp, "A%d\n", rval);
		  DEBUG1("rmtd: A %d\n", rval);
		  (void) write(1, resp, strlen(resp));
		  (void) write(1, mtgetp, rval);
		  goto top;
		}

	case 'Q':
		DEBUG("rmtd: Q\n");
		{
			ct_g0inq_data_t	info;
			if (ioctl(tape, MTSCSIINQ, &info) < 0)
				goto ioerror;
			rval = sizeof (info);
			(void) sprintf(resp, "A%d\n", rval);
			(void) write(1, resp, strlen(resp));
			(void) write(1, (char *)&info, sizeof (info));
			goto top;
		}

	case 'B':
		DEBUG("rmtd: B\n");
		{
			int	blksize;

			if (ioctl(tape, MTIOCGETBLKSIZE, &blksize) < 0)
				goto ioerror;
			rval = sizeof (blksize);
			(void) sprintf(resp, "A%d\n", rval);
			(void) write(1, resp, strlen(resp));
			(void) write(1, (char *)&blksize, sizeof (blksize));
			goto top;
		}

	default:
		if ( c == '\n' )
			goto top;
		DEBUG2("rmtd: garbage command %c (0x%x)\n", c,c);
		exit(3);
	}
respond:
	DEBUG1("rmtd: A %d\n", rval);
	(void) sprintf(resp, "A%d\n", rval);
	(void) write(1, resp, strlen(resp));
	goto top;
ioerror:
	error(errno);
	goto top;
}

getstring(bp)
	char *bp;
{
	int i;
	char *cp = bp;

	for (i = 0; i < SSIZE; i++) {
		if (read(0, cp+i, 1) != 1)
			exit(0);
		if (cp[i] == '\n')
			break;
	}
	cp[i] = '\0';
}

char *
checkbuf(record, size)
	char *record;
	int size;
{
	extern char *malloc();

	if (size <= maxrecsize)
		return (record);
	if (record != 0)
		free(record);
	record = malloc(size);
	if (record == 0) {
		DEBUG("rmtd: cannot allocate buffer space\n");
		exit(4);
	}
	maxrecsize = size;
	while (size > 1024 &&
	       setsockopt(0, SOL_SOCKET, SO_RCVBUF, &size, sizeof (size)) < 0)
		size -= 1024;
	return (record);
}

error(num)
	int num;
{

	DEBUG2("rmtd: E %d (%s)\n", num, sys_errlist[num]);
	(void) sprintf(resp, "E%d\n%s\n", num, sys_errlist[num]);
	(void) write(1, resp, strlen (resp));
}
