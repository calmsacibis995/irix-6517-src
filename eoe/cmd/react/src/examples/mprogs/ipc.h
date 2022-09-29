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

/*
 * Message Buffer
 */

typedef struct {
	long  mtype;
	long  replykey;
	int   pid;
	int   minor;
	int   disc;
	int   errorcode;
	frs_t frs;
} msgbuf_t;

/*
 * Message types
 */
#define MESSAGE_FRS       1
#define MESSAGE_FRSREPLY  2
#define MESSAGE_READY     3
#define MESSAGE_GO        4
#define MESSAGE_ENQUEUE   5
#define MESSAGE_RERROR    6
#define MESSAGE_ROK       7
#define MESSAGE_SEQBASE   100

/*
 * IPC channels
 */
#define REQKEY   0xfeedfeed     /* Request channel */
#define BASEKEY  0xf00df00d     /* Reply channel base */
#define SEQKEY   0x01234567     /* Sequencing channel */ 

int msend(long dstkey, void* buf, int nbytes);
int mreceive(long srckey, void* buf, int nbytes);
int mrecv_select(long srckey, void* buf, int nbytes, long mtype);
void rmqueue(long key);
