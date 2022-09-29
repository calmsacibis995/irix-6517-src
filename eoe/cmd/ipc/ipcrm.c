/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)ipc:/ipcrm.c	1.7" */
#ident "$Revision: 1.6 $"
/*
**	ipcrm - IPC remove
**	Remove specified message queues, semaphore sets and shared memory ids.
*/

#include	"sys/types.h"
#include	"sys/ipc.h"
#include	"sys/msg.h"
#include	"sys/sem.h"
#ifndef	pdp11
#include	"sys/shm.h"
#endif
#include	"errno.h"
#include	"getopt.h"
#include	"stdio.h"

char opts[] = "q:m:s:Q:M:S:";	/* allowable options for getopt */

main(argc, argv)
int	argc;	/* arg count */
char	**argv;	/* arg vector */
{
	register int	o;	/* option flag */
	register int	err;	/* error count */
	register int	ipc_id;	/* id to remove */
	register key_t	ipc_key;/* key to remove */
	union semun a;

	/* Go through the options */
	err = 0;
	while ((o = getopt(argc, argv, opts)) != EOF)
	{
		switch(o) {

		case 'q':	/* message queue */
			ipc_id = atoi(optarg);
			if (msgctl(ipc_id, IPC_RMID, 0) == -1)
			{
				oops("msqid", (long)ipc_id);
				err++;
			}
			break;

		case 'm':	/* shared memory */
#ifdef	pdp11
			fprintf(stderr,"shared memory not implemented on pdp11\n");
#else
			ipc_id = atoi(optarg);
			if (shmctl(ipc_id, IPC_RMID, 0) == -1)
			{
				oops("shmid", (long)ipc_id);
				err++;
			}
#endif
			break;

		case 's':	/* semaphores */
			ipc_id = atoi(optarg);
			if (semctl(ipc_id, 0, IPC_RMID, a) == -1)
			{
				oops("semid", (long)ipc_id);
				err++;
			}
			break;

		case 'Q':	/* message queue (by key) */
			if((ipc_key = (key_t)getkey(optarg)) == 0)
			{
				err++;
				break;
			}
			if ((ipc_id=msgget(ipc_key, 0)) == -1
				|| msgctl(ipc_id, IPC_RMID, 0) == -1)
			{
				oops("msgkey", ipc_key);
				err++;
			}
			break;

		case 'M':	/* shared memory (by key) */
#ifdef	pdp11
			fprintf(stderr,"shared memory not implemented on pdp11\n");
#else
			if((ipc_key = (key_t)getkey(optarg)) == 0)
			{
				err++;
				break;
			}
			if ((ipc_id=shmget(ipc_key, 0, 0)) == -1
				|| shmctl(ipc_id, IPC_RMID, 0) == -1)
			{
				oops("shmkey", ipc_key);
				err++;
			}
#endif
			break;

		case 'S':	/* semaphores (by key) */
			if((ipc_key = (key_t)getkey(optarg)) == 0)
			{
				err++;
				break;
			}
			if ((ipc_id=semget(ipc_key, 0, 0)) == -1
				|| semctl(ipc_id, 0, IPC_RMID, a) == -1)
			{
				oops("semkey", ipc_key);
				err++;
			}
			break;

		default:
		case '?':	/* anything else */
			err++;
			break;
		}
	}
	if (err || (optind < argc))
	{
		fprintf(stderr,
		   "usage: ipcrm [ [-q msqid] [-m shmid] [-s semid]\n%s\n",
		   "	[-Q msgkey] [-M shmkey] [-S semkey] ... ]");
		err++;
	}
	exit(err);
}

oops(char *s, int i)
{
	char *e;

	switch (errno) {

	case	ENOENT:	/* key not found */
	case	EINVAL:	/* id not found */
		e = "not found";
		break;

	case	EPERM:	/* permission denied */
		e = "permission denied";
		break;
	default:
		e = "unknown error";
	}

	/* since id's must be > 0 but key's can be anything - print
	 * as an unsigned
	 */
	fprintf(stderr, "ipcrm: %s(%u): %s\n", s, i, e);
}

/*
 * alas there doesn't seem to be any restriction on keys being positive.
 */
key_t
getkey(kp)
register char *kp;
{
	key_t k;

	if((k = (key_t)strtoul(kp, NULL, 0)) == IPC_PRIVATE)
		fprintf(stderr, "illegal key: %s\n", kp);
	return(k);
}

