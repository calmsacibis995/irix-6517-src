/**************************************************************************
 *									  *
 * 		 Copyright (C) 1990, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Revision: 1.3 $"

/* RealTime Test Program */

/* Functional test of the MP isolation features */
/*
** - has to have more than 1 cpu
** - create sproc and set must run on 2nd cpu
** - main process isolates the 2nd CPU, should fail
** - kill the sproc
** - main process isolates the 2nd CPU, should work
** - create  sproc on 2nd cpu, should fail
** - fork a process, set must run on 2nd cpu
*/
/* Huy Nguyen: July  1990 */

#include "stdio.h"
#include "sys/types.h"
#include "sys/param.h"
#include <sys/sysmp.h>
#include <malloc.h>
#include <sys/lock.h>

struct common {
	int	pid;
	int	cpid;
} comarea;
int maxcpu;

#define WARN	0
#define FATAL	1
#define MAXCPID	10
int cpid[MAXCPID];

main()
{
	extern void isolate_sproc();

	/* see if there are at least 2 cpus */
	if ( (maxcpu = sysmp(MP_NPROCS)) < 2) {
		err("Need at least 2 cpus for this test" , WARN);	
	}

	/* isolate a cpu and check status */	
	isolate_cpu();

	/* check that can not isolate a cpu that already had an sproc on it */ 
	comarea.pid = getpid();
	if ((comarea.cpid = sproc(isolate_sproc, PR_SALL)) == -1) {
		err("Can't create sproc", WARN);
	}
	blockproc(comarea.pid);
	comarea.cpid = -1;

}

/*
** 
*/
void
isolate_sproc()
{
	if (sysmp(MP_MUSTRUN, maxcpu-1) == -1) {
		err("isolate_sproc can't set must run", WARN);
	}
	if (sysmp(MP_ISOLATE, maxcpu-1) != -1) {
		err("Can isolate a processor that has a running sproc", FATAL);
	}
	unblockproc(comarea.pid);
	exit(0);
}

isolate_cpu()
{
	extern void dummythread();
	int count;

	if (sysmp(MP_MUSTRUN, maxcpu-1) == -1) {
		err("Can't set must run", WARN);
	}
	printf("Creating childs on cpu %d\n",maxcpu-1);
	forkproc();
	printf("Allow time for sched to swap childs out\n");
	count = 10;
	while (count--) {
		printf("Time left:%d\r",count);
		sleep(1);
	}
	printf("Isolate cpu %d to swap childs in\n",maxcpu-1);
	if (sysmp(MP_ISOLATE, maxcpu-1) == -1) {
		err("Can't isolate a processor", FATAL);
	}
	killproc();
	if (sproc(dummythread, PR_SALL) != -1) {
		err("Can sproc on isolated processor", FATAL);
	}
	if (sysmp(MP_UNISOLATE, maxcpu-1) == -1) {
		err("Can't un-isolate a processor", FATAL);
	}
	if (sysmp(MP_RUNANYWHERE) == -1) {
		err("Can't set run-anywhere", WARN);
	}
	printf("Done isolation check\n");
}

void
dummythread()
{}

forkproc()
{
	int i;

	for (i=0; i<MAXCPID; i++) {
		if (cpid[i] = fork())
			continue;
		else {
			childproc();
		}
	}
}

killproc()
{
	int i;

	for (i=0; i<MAXCPID; i++) {
		kill(cpid[i], SIGKILL);
	}
}

childproc()
{
	char *buf;

	if (plock(PROCLOCK)) {
		printf("Failed plock\n");
		exit();
	}
	buf = malloc(4*1024*1024);
	if (buf == (char *)0)
		return;
	*buf = 0;
	*(buf+10*1023) = 0;
	while (1) {}
}
	

err(msg, code)
char *msg;
int code;
{
	fprintf(stderr,"%s\n",msg);
	if (code == WARN) {
		fprintf(stderr,"WARNING\n");
		exit(0);
	}
	else if (code == FATAL) {
		fprintf(stderr,"FATAL ERROR\n");
		while(1) {}
	}
	
}	
