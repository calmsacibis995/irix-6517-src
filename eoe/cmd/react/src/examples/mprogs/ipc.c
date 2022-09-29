/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1994, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/


#include <stdio.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <sys/frs.h>
#include "ipc.h"

#define MAXOPEN 20

static struct{
	long 	key;
	int	qid;
} queues[MAXOPEN];

/*
 * Return queueID
 * Create if necessary
 */
static int
openqueue(long key)
{
	int	i, avail, qid;
	extern	int	errno;

	avail= -1;
	for(i=0;i<MAXOPEN;i++){
		if(queues[i].key==key)
			return(queues[i].qid);
		if(queues[i].key==0L && avail==-1)
			avail=i;
	}
	if(avail==-1){
		errno=0;
		return(-1);
	}
	if((qid=msgget(key,0666|IPC_CREAT))==-1)
		return(-1);
	queues[avail].key=key;
	queues[avail].qid=qid;
	return(qid);
}

/*
 * Return queue ID. Mark it deleted
 * if it is a current member of the
 * conversion table.
 */
static int
closequeue(long key)
{
	int i,qid;
	extern int errno;

	for(i=0;i<MAXOPEN;i++){
		if(queues[i].key==key){
			queues[i].key=0L;
			return(queues[i].qid);
		}
	}
	if(((qid=msgget(key,0666))==-1) && (errno!=ENOENT))
		return(-1);
	return(qid);
}

/*
 * Send Message
 */
int
msend(long dstkey, void* buf, int nbytes)
{
	int	qid;

	if((qid=openqueue(dstkey))==-1)
		return(-1);
	if(msgsnd(qid,buf,nbytes-sizeof(long),0)!=-1)
		return(0);
	else
		return(-1);
}


/*
 * Receive Message
 */
int
mreceive(long srckey, void* buf, int nbytes)
{
	int	qid;

	if((qid=openqueue(srckey))==-1)
		return(-1);
	if(msgrcv(qid,buf,nbytes-sizeof(long),0L,0)!=-1)
		return(0);
	else
		return(-1);
}

/*
 * Receive message of specific type
 */
int
mrecv_select(long srckey, void* buf, int nbytes, long mtype)
{
	int	qid;

	if((qid=openqueue(srckey))==-1)
		return(-1);
	if(msgrcv(qid,buf,nbytes-sizeof(long),mtype,0)!=-1)
		return(0);
	else
		return(-1);
}	

void
rmqueue(long key)
{
	int	qid;
	extern int errno;

	if((qid=closequeue(key))==-1 ||
		 ((msgctl(qid,IPC_RMID,NULL)==-1)&&(errno!=EINVAL)))
		syserr("rmqueue");
}
