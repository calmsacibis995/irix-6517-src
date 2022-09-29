/* Copyright 1991 Silicon Graphics, Inc. All rights reserved. */

#if 0
static char sccsid[] = "@(#)prot_libr.c	1.5 91/06/25 NFSSRC4.1 Copyr 1990 Sun Micro";
#endif

	/*
	 * Copyright (c) 1988 by Sun Microsystems, Inc.
	 */

	/*
	 * prot_libr.c
	 * consists of routines used for initialization, mapping and debugging
	 */

#include <stdio.h>
#include <syslog.h>
#include <string.h>
#include <bstring.h>
#include <netdb.h>
#include <signal.h>
#include <sys/file.h>
#include <sys/fcntl.h>
#include <sys/param.h>
#include "prot_lock.h"
#include "prot_time.h"
#include <rpcsvc/sm_inter.h>

extern int debug;

char hostname[MAXHOSTNAMELEN+1];	/* for generating oh */
int pid;				/* id for monitor usage */
int host_len;				/* for generating oh */
remote_result res_nolock;
remote_result res_working;

#ifndef SIGLOST
#define SIGLOST	SIGUSR1
#endif

static int cookie;			/* monitonically increasing # */


init()
{
	struct hostent *hp;

	(void) gethostname(hostname, sizeof(hostname));
	/* canonicalize hostname */
	if (hp = gethostbyname(hostname)) {
		(void) strncpy(hostname, hp->h_name, sizeof(hostname) -1);
	}
	/* used to generate owner handle */
	host_len = strlen(hostname) +1;
	pid = getpid(); /* used to generate return id for status monitor */
	res_nolock.lstat = nolocks;
	res_working.lstat = blocking;
	init_fdtable();

	if (sizeof (struct priv_struct) > SM_PRIVLEN) {
		syslog(LOG_ERR,
		    "fatal error: status monitor private data size too big");
		exit(1);
	}
}

/*
 * map input (from kenel) to lock manager internal structure
 * returns -1 if cannot allocate memory;
 * returns 0 otherwise
 */
int
map_kernel_klm(a)
	reclock *a;
{
	/*
	 * common code shared between map_kernel_klm and map_klm_nlm
	 */
	if (a->lck.lox.length > MAXLEN) {
		syslog(LOG_ERR, "len(%d) greater than max len(%d)",
			a->lck.lox.length, MAXLEN);
		a->lck.lox.length = MAXLEN;
	}

	/*
	 * generate svid holder
	 */
	if (!a->lck.lox.pid)
		a->lck.lox.pid = pid;

	a->lck.lox.magic = pid;

	/*
	 * owner handle == (hostname, pid);
	 * cannot generate owner handle use obj_alloc
	 * because additioanl pid attached at the end
	 */
	a->lck.oh_len = host_len + sizeof (int);
	if ((a->lck.oh_bytes = xmalloc(a->lck.oh_len) ) == NULL)
		return (-1);
	(void) strcpy(a->lck.oh_bytes, hostname);
	bcopy((char *) &a->lck.lox.pid, &a->lck.oh_bytes[host_len],
		sizeof (int));

	/*
	 * generate cookie
	 * cookie is generated from monitonically increasing #
	 */
	cookie++;
	if (obj_alloc(&a->cookie, (char *) &cookie, sizeof (cookie))== -1)
		return (-1);

	/*
	 * generate clnt_name
	 */
	if ((a->lck.clnt_name= xmalloc(host_len)) == NULL)
		return (-1);
	(void) strcpy(a->lck.clnt_name, hostname);
	a->lck.caller_name = a->lck.clnt_name; 	/* ptr to same area */
	return (0);
}


/*
 * nlm map input from klm to lock manager internal structure
 * return -1, if cannot allocate memory!
 * returns 0, otherwise
 */
int
map_klm_nlm(a)
	reclock *a;
{
	/*
	 * generate svid holder
	 */
	if (!a->lck.lox.pid)
		a->lck.lox.pid = pid;

	a->lck.lox.magic = pid;

 	/*
	 * normal klm to nlm calls
	 */
	a->lck.clnt_name = a->lck.caller_name;
	if ((a->lck.server_name = xmalloc(host_len)) == NULL) {
		return (-1);
	}
	(void) strcpy(a->lck.server_name, hostname);
	return (0);
}

pr_oh(a)
	netobj *a;
{
	int i;
	int j;
	unsigned p = 0;

	if (a->n_len == 0) {
		printf("(Empty)");
		return(0);
	}
	if (a->n_len - sizeof (int) > 4 )
		j = 4;
	else
		j = a->n_len - sizeof (int);

	/*
	 * only print out part of oh
	 */
	for (i = 0; i< j; i++) {
		printf("%c", a->n_bytes[i]);
	}
	for (i = a->n_len - sizeof (int); i< a->n_len; i++) {
		p = (p << 8) | (((unsigned)a->n_bytes[i]) & 0xff);
	}
	printf("%u", p);
}

pr_fh(a)
	netobj *a;
{
	int i;

	for (i = 0; i< a->n_len; i++) {
		printf("%02x", (a->n_bytes[i] & 0xff));
	}
}

static int
getcookie(reclock *a)
{
	int i = 0;
	if (a->cookie.n_len == 4)
		bcopy(a->cookie.n_bytes, &i, sizeof(i));
	return i;
}

pr_lock(a)
	reclock *a;
{
	if (a != NULL) {
		printf("   req=%x cuky=%d oh=", a, getcookie(a));
		pr_oh(&a->lck.oh);
		printf(" svr=%s rel=%d lockID=%d\n",
			a->lck.server_name, a->rel, a->lck.lox.LockID);
		printf(
		   "   blk=%d excl=%d type=%d baselen=%u,%u pid=%d ipaddr=%x\n",
			a->block, a->exclusive, a->lck.lox.type,
			a->lck.lox.base, a->lck.lox.length,
			a->lck.lox.pid, a->lck.lox.ipaddr);
		printf("   fh=");
		pr_fh(&a->lck.fh);
		printf("\n");
	} else {
		printf("RECLOCK is NULL.\n");
	}
}

pr_all()
{
	msg_entry *msgp;
	extern msg_entry *msg_q;

	if (debug < 2)
		return;

	/*
	 * print msg queue
	 */
	if (msg_q != NULL) {
		printf("***** MSG QUEUE *****\n");
		msgp= msg_q;
		while (msgp != NULL) {
			printf("%x=(%x,", msgp,msgp->req);
			if (msgp->reply != NULL)
				printf(" %s) ",
					nlm_stat2name(msgp->reply->lstat));
			else
				printf(" -) ");
			msgp = msgp->nxt;
		}
		printf("\n");
	}
	else
		printf("***** MSG queue EMPTY *****\n");

	(void) fflush(stdout);
}

char *
lck2name(int stat)
{
#ifdef __STDC__
#define F(x)	case F_## x : return # x;
#else
#define F(x)	case F_/**/x : return "x";
#endif
    switch (stat) {
	F(RDLCK)
	F(WRLCK)
	F(UNLCK)
    }
    return "??";
}
#undef F

char *
klm_stat2name(int stat)
{
#ifdef __STDC__
#define F(x)	case klm_## x : return # x;
#else
#define F(x)	case klm_/**/x : return "x";
#endif
    switch (stat) {
	F(granted)
	F(denied)
	F(denied_nolocks)
	F(working)
	F(deadlck)
    }
    return "??";
}
#undef F

char *
nlm_stat2name(int stat)
{
#ifdef __STDC__
#define F(x)	case nlm_## x : return # x;
#else
#define F(x)	case nlm_/**/x : return "x";
#endif
    switch (stat) {
	F(granted)
	F(denied)
	F(denied_nolocks)
	F(blocked)
	F(denied_grace_period)
	F(deadlck)
    }
    return "??";
}
#undef F

kill_process(a)
	reclock *a;
{
	syslog(LOG_ERR, "lock not reclaimed, %d killed\n", a->lck.lox.pid);
	(void) kill(a->lck.lox.pid, SIGLOST);
}
