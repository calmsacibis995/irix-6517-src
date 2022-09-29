/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: compat_handleCancel.h,v $
 * Revision 65.1  1997/10/20 19:20:37  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.11.1  1996/10/02  17:54:28  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:42:11  damon]
 *
 * Revision 1.1.6.1  1994/06/09  14:13:23  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:26:31  annie]
 * 
 * Revision 1.1.4.2  1993/05/11  14:16:55  jaffe
 * 	Transarc delta: vijay-ot7220-bakserver-remove-spurious-errors 1.1
 * 	  Selected comments:
 * 
 * 	    A merge conflict caused this fix to be done again. Spurious messages in BakLog
 * 	    caused by the cancel handling macros should be removed. The macros were not
 * 	    setting their error codes to zero upon success. This caused the bakserver
 * 	    RPCs to interpret the returned error codes as error conditions.
 * 	    A one line fix to reset error value upon success.
 * 	[1993/05/10  18:15:33  jaffe]
 * 
 * Revision 1.1.2.5  1993/01/19  16:08:25  cjd
 * 	embedded copyright notice
 * 	[1993/01/19  15:11:58  cjd]
 * 
 * Revision 1.1.2.4  1993/01/13  18:17:04  shl
 * 	Transarc delta: vijay-ot6381-correct-handling-of-device-queue-lock 1.4
 * 	  Selected comments:
 * 
 * 	    The aim of this delta is to get bak restore to succeed. The problems in restore
 * 	    were some spurious thread exceptions and assertions that made bak dump core.
 * 	    Upon investigation it turned out that there were some locking problems in
 * 	    backup and butc. The delta cleans up locking in backup and butc.
 * 	    The delta is not ready for export. Much more needs to be done to get all the
 * 	    locking issues right. This is just a checkpoint.
 * 	    Second set of changes to get locking issues in order. This time the changes
 * 	    are mostly in bakserver. The changes introduced are
 * 	    1. Establish a global lock so that two RPCs do not interfere with each other.
 * 	    This was introduced because there are some global data structures in
 * 	    backup that might get affected. These global structures now have
 * 	    individual locks to serialize changes. The global lock in temporary, and
 * 	    will go away once we have serialized all accesses to all global data
 * 	    structures.
 * 	    2. Disable cancels in bakserver RPCs. Care is taken to not disable cancels in
 * 	    operations that can be cancelled. There is some more work that needs to be
 * 	    done in this area.
 * 	    3. Accesses to the database are controlled by memoryDB.lock. The accesses are
 * 	    by means of dbread and dbwrite. The hash tables too are protected by this
 * 	    mechanism.
 * 	    4. Changes to the backup database dump code to simplify the dump operation. In
 * 	    particular, the Unix pipe mechanism is used to synchronize reader and
 * 	    writer without need for condition variables.
 * 	    5. Get rid of any pthread_mutex and pthread_cond operations. Use afslk and
 * 	    afsos routines instead.
 * 	    6. Continue the work described by the previous revision of the delta in bak
 * 	    and butc. This should be it for changes in bak and butc.
 * 	    Fix compilation problems on the RIOS.
 * 	    The error returned from the cancel enable/disable macros was not zero on
 * 	    success.
 * 	    This is hopefully the final revision of this delta. The fixes here are
 * 	    1. Changes to the bakserver and butc RPC interfaces.
 * 	    The RPCs that handle variable size arrays now use conformant arrays.
 * 	    This avoids allocating lots of memory on the client thread stack which
 * 	    was the cause of the exceptions in restoreft. The server allocates
 * 	    memory which is freed by the server stub on the way out. The client thread
 * 	    frees the memory allocated by the client stub.
 * 	    2. get database dump and restore to handle status correctly.
 * 	    3. Get the locking hierarchy right in bakserver, bak and butc.
 * 	    4. Other minor nits.
 * 	    There is still a problem with scantape -dbadd that has to be addressed. With
 * 	    this delta, all backup commands should work when used in a straightforward
 * 	    manner. Some error cases are still not handled properly though. Subsequent
 * 	    deltas would fix those.
 * 	[1993/01/12  21:31:58  shl]
 * 
 * Revision 1.1.2.3  1992/09/25  18:23:45  jaffe
 * 	Transarc delta: jaffe-ot5416-cleanup-RCS-log-entries 1.1
 * 	  Selected comments:
 * 	    Cleanup extra RCS log entries.  There should only be one per file, and
 * 	    it should be closed with an EndLog comment.
 * 	[1992/09/23  19:20:29  jaffe]
 * 
 * Revision 1.1.2.2  1992/08/31  20:07:23  jaffe
 * 	Transarc delta: vijay-ot4694-dfs-servers-protect-against-cancel 1.1
 * 	[1992/08/30  03:01:35  jaffe]
 * 
 * $EndLog$
 */
/*
 * (C) Copyright 1992 Transarc Corporation.
 * All Rights Reserved.
 */
/*
 * compat_handleCancel.h -- protect DFS servers against malicious cancels
 *			    from client threads
 * 
 * $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/ncscompat/RCS/compat_handleCancel.h,v 65.1 1997/10/20 19:20:37 jdoak Exp $
 */
#ifndef TRANSARC_COMPAT_HANDLECANCEL_H
#define TRANSARC_COMPAT_HANDLECANCEL_H

/***

This header file has some macros that protect DFS servers from malicious
client cancels. Here's a piece of mail from Beth that describes the problem
and proposes a fix.

Because of the way the RPC runtime interacts with threads, it is possible for a
malicious client to force the cancellation of a thread servicing an RPC within
a DFS server (all that is necessary is for the client to be running at least 
two threads and to make an RPC with one thread and run pthread_cancel on the 
thread making the RPC from the other thread).  This will, potentially, 
deadlock many of our servers since a pending cancellation may be delivered when
a server holds any of the locks it uses internally.

Any cancellation that may be invoked on our RPC server threads can (in general)
be delivered during any call to any of the following pthread routines (these 
are referred to as "cancelation points" in the pthreads documentation):
        pthread_setasynccancel
        pthread_testcancel
        pthread_delay_np
        pthread_join
        pthread_cond_wait
        pthread_cond_timedwait

The occurrence of pthread_cond_wait in this list is particularly bad for DFS 
servers, since the implementations of the afslk_Obtain* routines use pthread_
cond_wait, and the afslk_Obtain* routines are used throughout our code base.  
This means that, whenever we use one of these routines in an RPC server thread,
that thread becomes susceptible to cancellation, no matter what DFS locks it 
might hold.

In order to protect our servers from this problem, we need to turn off cancel-
lation (saving the cancellation state when we were called) before obtaining any
of our own locks in our RPC management routines (the routines declared in .idl
files) and restore the old cancellation state before returning.  For all our 
servers (with the possible exception of the backup server since it has long-
running manager routines?), this should not be a problem.  It should also not 
be a real problem for the backup server if that server periodically tests for 
the presence of pending cancels, using the pthread_testcancel routine, and 
carefully exits (releasing any of its own locks) if a cancel is found to be 
pending.

The following code is an example of how cancellation should be handled in our 
manager routines (the code is merged from the BossvrCommonInit and 
BossvrCommonTerm routines in the bosserver/bossvr_ncs_procs.c file)

long EXAMPLE_ManagerRoutine()
{
    long        returnCode;
    int savedCancelState;

    if ((savedCancelState = pthread_setcancel(CANCEL_OFF)) != -1) {

        ** real RPC manager service code goes here; rtnCode set appropriately**
        if (pthread_setcancel(savedCancelState) == -1) {
            ** error handling appropriate to the server goes here; returnCode 
	       set appropriately **
        }
    }
    else {
        ** error handling appropriate to the server goes here; returnCode set 
           appropriately **
    }

    return returnCcode;
}

Code of this form needs to be added to each our server implementations.

***/

/*
 * Macro to disable cancels before entering server routine.
 */

#define DFS_DISABLE_CANCEL(savedCancelStateP, handleCancelError)	\
MACRO_BEGIN								\
if ((*savedCancelStateP = pthread_setcancel(CANCEL_OFF)) == -1) 	\
   handleCancelError = COMPAT_ERROR_DISABLE_CANCEL;			\
else									\
   handleCancelError = 0;						\
MACRO_END								

/* 
 * Macro to put back the cancel state to what it was before entering
 * server routine.
 */

#define DFS_ENABLE_CANCEL(savedCancelState, handleCancelError)		\
MACRO_BEGIN								\
if ((savedCancelState != CANCEL_OFF) && 				\
    (pthread_setcancel(savedCancelState) == -1))			\
   handleCancelError = COMPAT_ERROR_ENABLE_CANCEL;			\
else									\
   handleCancelError = 0;						\
MACRO_END

#endif /* TRANSARC_COMPAT_HANDLECANCEL_H */


