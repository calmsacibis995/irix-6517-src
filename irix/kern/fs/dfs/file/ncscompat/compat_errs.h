/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 * HISTORY
 * $Log: compat_errs.h,v $
 * Revision 65.2  1999/02/04 19:19:42  mek
 * Provide C style include file for IRIX kernel integration.
 *
 * Revision 65.1  1997/10/20 19:20:37  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.12.1  1996/10/02  17:54:27  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:42:10  damon]
 *
 * Revision 1.1.7.1  1994/06/09  14:13:21  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:26:30  annie]
 * 
 * Revision 1.1.5.7  1993/01/19  16:08:22  cjd
 * 	embedded copyright notice
 * 	[1993/01/19  15:11:53  cjd]
 * 
 * Revision 1.1.5.6  1992/12/09  20:21:21  jaffe
 * 	Transarc delta: vijay-ot5953-butc-bring-it-upto-date 1.3
 * 	  Selected comments:
 * 
 * 	    RPCs to butc from bak fail with rpc_s_comm_failure. Upon reviewing the butc
 * 	    code, it turns out that the server is calling dfsauth_server_InitAuthentication
 * 	    that requires authenticated incoming calls. But, bak is not authenticating
 * 	    the bindings with which it makes the RPCs. This could be one reason why the RPC
 * 	    fails. This delta updates the bak and butc code to reflect changes in
 * 	    cds and security since 1.0
 * 	    No changes made yet, closing to fix another problem.
 * 	    A single set of routines was constructed in ncscompat to register a DFS
 * 	    interface and to locate it. Butc had its own routines in ncscompat that
 * 	    register and locate butc servers. The difference between these routines
 * 	    and the original rpc_register_dfs_server and rpc_locate_dfs_server routines
 * 	    is that butc passes an object UUID and a type UUID as part of the registration
 * 	    process and passes a object UUID to locate butc processes. The general
 * 	    routines now support these UUIDs in their interface. This makes the
 * 	    general routines support servers like butc. Also, there would one set of
 * 	    routines to maintain. The general registration and location routines are
 * 	    used in many dfs packages and changes were necessary on all of them to
 * 	    support the new, more general function signature.
 * 	    some new compat errors
 * 	    Correct some compilation errors and warnings. uuid_create_nil needed some
 * 	    casting to avoid warnings.
 * 	[1992/12/04  16:50:39  jaffe]
 * 
 * Revision 1.1.5.5  1992/10/28  21:45:23  jaffe
 * 	Fix missing RCS id.
 * 	[1992/10/28  21:17:36  jaffe]
 * 
 * Revision 1.1.5.4  1992/10/27  21:15:39  jaffe
 * 	Transarc delta: bab-ot5476-dfsauth-force-noauth-notify 1.3
 * 	  Selected comments:
 * 	    When it finds no default creds, the dfsauth package will now
 * 	    complete initialization in unauthenticated mode.  This delta also
 * 	    includes code to allow the bos command to take advantage of this
 * 	    feature.
 * 	    ot 5476
 * 	    Added error code to indicate dfsauth package has forced noauth mode.
 * 	    The first implementation wasn't really clean.  The place for the forced
 * 	    noauth mode is not really in dfsauth package, but in the client of that
 * 	    package.  The dfsauth package was, essentially restored; one routine was
 * 	    added.  And the burden of retrying in noauth mode if no credentials were
 * 	    found is moved to the end client.
 * 	    Renamed the compat error code to be used when a noauth retry should
 * 	    be attempted.
 * 	    Need to set noauth for the name space, even in some cases in which
 * 	    noauth is not specified to rpc_locate_dfs_server.
 * 	Transarc delta: bab-ot5477-ncscompat-force-noauth-notify 1.2
 * 	  Selected comments:
 * 	    When the rpc_locate_dfs_server routine in the ncscompat package forces
 * 	    a client into noauth mode due to the use of the short version of the
 * 	    machine name, it now sets a special status code, so the client can print
 * 	    a warning message, if desired.  Also added code for all the clients of
 * 	    rpc_locate_dfs_server.  All except the bos command will ignore the new error
 * 	    code (since they had previously been written as if the forced noauth mode
 * 	    in this case is all right).  The bos command now prints a warning message
 * 	    when this occurs.
 * 	    ot 5477
 * 	    Add error message to use when noauth mode forced by the use of the short
 * 	    version of the machine name.
 * 	    Fixed masking of error by name space authentication resetting.
 * 	[1992/10/27  14:38:10  jaffe]
 * 
 * Revision 1.1.5.3  1992/09/25  18:23:42  jaffe
 * 	Transarc delta: vijay-ot4780-ubik-handling-multiple-interfaces 1.6
 * 	  Selected comments:
 * 
 * 	    Ubik servers should be able to run on multi-homed machines. i.e., machines
 * 	    with multiple addresses. This translates to multiple host bindings for each
 * 	    host. This delta adds changes to ubik to handle this scenario. For details
 * 	    on the design and implementation, refer to the document "Ubik on machines
 * 	    with multi-network interface"
 * 	    Not ready yet, close to upgrade.
 * 	    Some bug fixes, also fix left out components.
 * 	    Add some new compat error codes to be returned
 * 	    Many fixes to the previous delta to get things to work. Many fixes to ubik
 * 	    that I didn't notice until now. Also, some optimizations in selecting
 * 	    binding handles. Everything seems to work on single and multiple servers,
 * 	    so ready for export.
 * 	    missed turning off debug messages, so here goes.
 * 	    Missed a file in bak which was using the old style ubik_client structure
 * 	    added a missing cast
 * 	[1992/09/23  19:19:31  jaffe]
 * 
 * Revision 1.1.5.2  1992/08/31  20:07:17  jaffe
 * 	*** empty log message ***
 * 	[1992/08/31  15:19:36  jaffe]
 * 
 * 	Transarc delta: vijay-ot3973-ncsubik-improved-error-handling 1.4
 * 	  Selected comments:
 * 	    Error handling in ncsubik was erroneous in most parts. Ubik errors are
 * 	    generated for different error conditions and these are propagated to the
 * 	    callers. Also, upon hitting an error condition, control returns with an error
 * 	    instead of proceeding. Also resource cleanup is handled in the right way. Some
 * 	    miscellaneous bugs found in the code review process were also fixed.
 * 	    This delta is a checkpoint to upgrade, not ready for release yet.
 * 	    code cleanup, see above
 * 	    fix a bug in handling rpc_s_no_more_members
 * 	    Add a help feature to utst_client
 * 	    Fix some compilation problems in backup
 * 	Transarc delta: vijay-ot4694-dfs-servers-protect-against-cancel 1.1
 * 	  Selected comments:
 * 
 * 	    This delta protects DFS servers from malicious cancels from client threads.
 * 	    Here's a piece of mail from Beth Bottos explaining the problem.
 * 	    Because of the way the RPC runtime interacts with threads, it is possible for a
 * 	    malicious client to force the cancellation of a thread servicing an RPC within
 * 	    a DFS server (all that is necessary is for the client to be running at least
 * 	    two threads and to make an RPC with one thread and run pthread_cancel on the
 * 	    thread making the RPC from the other thread).  This will, potentially,
 * 	    deadlock many of our servers since a pending cancellation may be delivered when
 * 	    a server holds any of the locks it uses internally.
 * 	    Any cancellation that may be invoked on our RPC server threads can (in general)
 * 	    be delivered during any call to any of the following pthread routines (these
 * 	    are referred to as "cancelation points" in the pthreads documentation):
 * 	    pthread_setasynccancel
 * 	    pthread_testcancel
 * 	    pthread_delay_np
 * 	    pthread_join
 * 	    pthread_cond_wait
 * 	    pthread_cond_timedwait
 * 	    The occurrence of pthread_cond_wait in this list is particularly bad for DFS
 * 	    servers, since the implementations of the afslk_Obtain* routines use pthread_
 * 	    cond_wait, and the afslk_Obtain* routines are used throughout our code base.
 * 	    This means that, whenever we use one of these routines in an RPC server thread,
 * 	    that thread becomes susceptible to cancellation, no matter what DFS locks it
 * 	    might hold.
 * 	    In order to protect our servers from this problem, we need to turn off cancel-
 * 	    lation (saving the cancellation state when we were called) before obtaining any
 * 	    of our own locks in our RPC management routines (the routines declared in .idl
 * 	    files) and restore the old cancellation state before returning.  For all our
 * 	    servers (with the possible exception of the backup server since it has long-
 * 	    running manager routines?), this should not be a problem.  It should also not
 * 	    be a real problem for the backup server if that server periodically tests for
 * 	    the presence of pending cancels, using the pthread_testcancel routine, and
 * 	    carefully exits (releasing any of its own locks) if a cancel is found to be
 * 	    pending.
 * 	    The following code is an example of how cancellation should be handled in our
 * 	    manager routines (the code is merged from the BossvrCommonInit and
 * 	    BossvrCommonTerm routines in the bosserver/bossvr_ncs_procs.c file)
 * 	    long EXAMPLE_ManagerRoutine()
 * 	    {
 * 	    long        returnCode;
 * 	    int savedCancelState;
 * 	    if ((savedCancelState = pthread_setcancel(CANCEL_OFF)) != -1) {
 * 	    /* real RPC manager service code goes here; rtnCode set appropriately*/
 * 	    if (pthread_setcancel(savedCancelState) == -1) {
 * 	    /* error handling appropriate to the server goes here; returnCode
 * 	    set appropriately */
 * 	    }
 * 	    }
 * 	    else {
 * 	    /* error handling appropriate to the server goes here; returnCode set
 * 	    appropriately */
 * 	    return returnCcode;
 * 	    }
 * 	    Code of this form needs to be added to each our server implementations.
 * 	    Add some new error codes used in compat_handleCancel.h.
 * 	[1992/08/30  03:00:47  jaffe]
 * 
 * Revision 1.1.2.2  1992/04/01  23:10:19  mason
 * 	My understanding is that the first error in an .et file needs to be a dummy
 * 	error code.  The files file/ftserver/ftserver.et and
 * 	file/ncscompat/compat_errs.et used to have such a dummy error code, but
 * 	those codes must have appeared to be malformed and they were eliminated.
 * 	[1992/04/01  23:08:39  mason]
 * 
 * Revision 1.1  1992/01/19  19:42:03  devrcs
 * 	Initial revision
 * 
 * $EndLog$
 */

/*
 * compat_errs.h:
 * This file is automatically generated; please do not edit it.
 */
#define COMPAT_ERROR_RCSID                       (552005632L)
#define COMPAT_ERROR_PARAMETER_ERROR             (552005633L)
#define COMPAT_ERROR_BUFFER_TOO_SMALL            (552005634L)
#define COMPAT_ERROR_MALFORMED_HOSTNAME          (552005635L)
#define COMPAT_ERROR_DISABLE_CANCEL              (552005636L)
#define COMPAT_ERROR_ENABLE_CANCEL               (552005637L)
#define COMPAT_ERROR_NOMEM                       (552005638L)
#define COMPAT_ERROR_NOBINDINGS                  (552005639L)
#define COMPAT_ERROR_TOO_MANY_OBJECTS            (552005640L)
#define COMPAT_ERROR_NO_CREDENTIALS              (552005641L)
#define COMPAT_ERROR_SHORTNAME_NOAUTH            (552005642L)
#define COMPAT_ERROR_BAD_UUID                    (552005643L)
#define initialize_cmp_error_table()
#define ERROR_TABLE_BASE_cmp (552005632L)

/* for compatibility with older versions... */
#define init_cmp_err_tbl initialize_cmp_error_table
#define cmp_err_base ERROR_TABLE_BASE_cmp
