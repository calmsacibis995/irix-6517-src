/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: krpc_misc.c,v 65.5 1998/04/01 14:16:01 gwehrman Exp $";
static void _identFunc(const char *x) {if(!x)_identFunc(_identString);}
#endif


/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: krpc_misc.c,v $
 * Revision 65.5  1998/04/01 14:16:01  gwehrman
 * 	Cleanup of errors turned on by -diag_error flags in kernel cflags.
 * 	Added includes for krpc_misc.h and uuidutils.h.  Changed sockPtr
 * 	to a sockaddr_p_t in krpc_InvokeServer for compatibility with its
 * 	use.  Changed definition of protSeq from unsigned char array to
 * 	char array.  This is consistent with existing extern declarations.
 *
 * Revision 65.4  1998/03/23 16:36:14  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.3  1997/12/16 17:05:36  lmc
 *  1.2.2 initial merge of file directory
 *
 * Revision 65.2  1997/11/06  20:00:31  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/20  19:20:11  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.72.1  1996/10/02  17:53:14  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:41:31  damon]
 *
 * $EndLog$
 */
/*
 *      Copyright (C) 1991, 1996 Transarc Corporation
 *      All rights reserved.
 */

/*
 * This file contains the common functions and data structures that 
 * are used to invoke the kernel servers, ie, PX or TKM services.
 */
#include <dcedfs/param.h>                  /* Should be always first */
#include <dcedfs/sysincludes.h>
#include <dcedfs/queue.h>
#include <dcedfs/afs4int.h>
#include <dcedfs/tkn4int.h>
#include <dcedfs/icl.h>
#include <dcedfs/osi.h>
#include <dcedfs/osi_net.h>
#include <dce/ker/pthread.h>
#ifdef SGIMIPS
#include <krpc_misc.h>
#include <uuidutils.h>
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/kutils/RCS/krpc_misc.c,v 65.5 1998/04/01 14:16:01 gwehrman Exp $")

/*
 * Define the network protcol sequence DFS currently uses.
 */
#ifdef SGIMIPS
/*
 * For type compatibilty, it would make for sense for protSeq to be defined
 * as a unsigned char type.  However, external uses of protSeq expect it to
 * be a char type.
 */
char protSeq[] = "ncadg_ip_udp";
#else
unsigned char protSeq[] = "ncadg_ip_udp";
#endif
/* 
 * To get the DCE's principal name. Note, it may become a MACRO someday !
 * Its form should be   "/.../<cellname>/<hostname>/dfs-server "
 */
long krpc_GetPrincName(princName, cellName, hostName)
     char *princName;
     char *cellName;
     char *hostName;
{
    strcpy(princName, "/.../");
    strcat(princName, cellName);
    strcat(princName, "/");
    strcat(princName, hostName);
    strcat(princName, "/dfs-server");


#ifdef SGIMIPS
    return 0; /* hush up the mongoose 7.0 compiler */
#else
    return;
#endif /* SGIMIPS */ 

}

/* 
 * Invoke a particular kenrel DFS server (ie., serverName) by the user (caller)
 * The fucntion returns -1, if it fails and prints the appropriate message.
 * If it succeeds, this function returns to the caller the rpc string binding 
 * pertaining to the rpc binding that just is registered with the NCS runtime.
 */

long krpc_InvokeServer (serverName, caller, ifSpec, mgrEpv, uuidPtr, 
			Flags, maxCalls, rpcString, stringSize, saddrPtr)
     char *serverName;		/* server to be invoked */
     char *caller;		/* who is the caller */
     rpc_if_handle_t ifSpec;	/* server interface spec */
     rpc_mgr_epv_t mgrEpv;	/* manager vector for that IF service */
     uuid_t *uuidPtr;		/* object type uuid pointer */
     long Flags;		/* when verbose is set */
     int  maxCalls;		/* Max number threads */
     char *rpcString;		/* OUTput the rpc string binding */
     int  *stringSize;		/* Size of the rpc string binding */ 
     struct sockaddr_in *saddrPtr;	/* Ptr to the IP addr for now */
{
    boolean32 valid_protSeq;
    unsigned_char_t *stringBinding, *protseq_string;
    rpc_binding_vector_p_t bvec;
    int i, Found, Index, fromPX = 0;
#ifdef SGIMIPS
    sockaddr_p_t    sockPtr;
#else
    struct sockaddr *sockPtr;
#endif
    unsigned32 st; 

    /*
     * Verify whether the specified protocol sequence is supported by
     * both the RPC runtime and the underlying operating system.
     */
#ifdef SGIMIPS
    valid_protSeq = rpc_network_is_protseq_valid((unsigned char *)protSeq, &st);
#else
    valid_protSeq = rpc_network_is_protseq_valid(protSeq, &st);
#endif
    if (st != error_status_ok) {
	uprintf("%s: can't validate protocol sequence, %s (code %d)\n", 
		caller, protSeq, st);
	return -1;
    }
    if (!valid_protSeq) {
	uprintf("%s: the protocol sequence, %s, is not valid\n", 
		caller, protSeq);
	return -1;
    }
    /*
     * Tell the RPC runtime to listen for the remote procedure calls using 
     * the specified protocol sequence.
     */
#ifdef SGIMIPS
    rpc_server_use_protseq((unsigned char *)protSeq, maxCalls, &st);
#else
    rpc_server_use_protseq(protSeq, maxCalls, &st);
#endif
    if (st != error_status_ok)  {
        uprintf("%s: can't register protocol sequence, %s (code %d)\n", 
		caller, protSeq, st);
        return -1;
    }
    /*
     * Register the Server Interface with the RPC runtime.
     */
    rpc_server_register_if(ifSpec,
			   uuidPtr,
			   mgrEpv,
			   &st);
    if (st != error_status_ok)  {
	uprintf("%s: can't register %s server (code %d)\n", 
		caller, serverName, st);
	return -1;
    }
    if (strcmp(serverName, "FX") == 0)
        fromPX = 1;

    /*
     * Inquire all protocol sequences supported by both the RPC runtime and
     * the underlying operating system. We really want to return to the user
     * the desired rpc binding that uses "ncadg_ip_udp" protocol sequence.
     */
    rpc_server_inq_bindings(&bvec, &st);
    if (st != error_status_ok)  {
	uprintf("%s: can't inquire bindings (code %d)\n", caller, st);
	return -1;
    }
    if (Flags) /* If -verbose or -debug is turned on */
	uprintf("\nBindings:\n");
    Found = 0;  /* Not Found */
    for (i = 0; i < bvec->count && Found == 0; i++)  {
        rpc_binding_to_string_binding(bvec->binding_h[i], &stringBinding, &st);
        if (st != error_status_ok)  {
	    uprintf("%s: can't get string binding (code %d)\n", caller, st);
	    rpc_binding_vector_free(&bvec, &st);
	    return -1;
	}
	rpc_string_binding_parse(stringBinding, NULL, &protseq_string,
				 NULL, NULL, NULL, &st);
	if (st != error_status_ok) {
	    uprintf("%s: can't parse string binding (code %d)\n",caller, st);
	    rpc_binding_vector_free(&bvec, &st);
	    rpc_string_free(&stringBinding, &st);
	    return -1;
	}
	if (strcmp((char *)protseq_string, (char *)protSeq) == 0 ) {
	    /* Found FIRST desired binding that matches "ncadg_ip_udp"  */
	    if (fromPX) {
		if (rpcString) 
		    strcpy(rpcString, (char *)stringBinding);
		if (stringSize)
		    *stringSize = strlen(rpcString) + 1;
	    }
	    if (Flags) /* If -verbose or -debug is turned on */
		uprintf("%s\n", stringBinding);
	    Index = i;
	    Found = 1;
	}
	rpc_string_free(&protseq_string, &st);
	rpc_string_free(&stringBinding, &st);
    }   /* for loop */

    if (saddrPtr != NULL) {   /* for UDP/IP only */
	rpc__binding_inq_sockaddr(bvec->binding_h[Index], &sockPtr, &st);
	*saddrPtr = *((struct sockaddr_in *) sockPtr);
    }
    rpc_binding_vector_free(&bvec, &st);
    return 0;
}

/*
 * Undo a previous call to krpc_InvokeServer, for error recovery
 */
void krpc_UnInvokeServer (serverName, caller, ifSpec, mgrEpv, uuidPtr, Flags)
    char *serverName;		/* server invoked */
    char *caller;		/* who is the caller */
    rpc_if_handle_t ifSpec;	/* server interface spec */
    rpc_mgr_epv_t mgrEpv;	/* manager vector for that IF service */
    uuid_t *uuidPtr;		/* object type uuid pointer */
    long Flags;			/* when verbose is set */
{
    unsigned32 st;

    rpc_server_unregister_if(ifSpec,
			     uuidPtr,
			     &st);
}

/*
 * Generate an rpc binding from the sockaddr 
 */
int krpc_BindingFromSockaddr(caller, protseq, saddrPtr, rpcBinding)
     char *caller;			/* who is the caller */
     char *protseq;			/* protocol sequence */
     afsNetAddr  *saddrPtr;		/* ptr to the sockaddr */
     handle_t  *rpcBinding;		/* rpc binding */
{
    unsigned char *newString;
    char netaddr_string[60];
    char endpoint_string[12];
    unsigned32 ip_addr;
    unsigned32 ip_port;
    struct sockaddr_in *ipaddrPtr;
    handle_t binding;
    unsigned32 st;
    int code = 0;

    /*
     * NOTE: Assuming the protocol used here is UDP/IP. Otherwise, the
     *       code should be rewritten. 
     */

    ipaddrPtr = (struct sockaddr_in *) saddrPtr;
    ip_addr = ntohl(ipaddrPtr->sin_addr.s_addr);
    ip_port = ntohs(ipaddrPtr->sin_port);

    osi_cv2string(ip_addr >> 24, netaddr_string);
    strcat(netaddr_string, ".");
    osi_cv2string((ip_addr >> 16) & 0xff,
	&netaddr_string[strlen(netaddr_string)]);
    strcat(netaddr_string, ".");
    osi_cv2string((ip_addr >> 8) & 0xff,
	&netaddr_string[strlen(netaddr_string)]);
    strcat(netaddr_string, ".");
    osi_cv2string(ip_addr & 0xff,
	&netaddr_string[strlen(netaddr_string)]);
    osi_cv2string(ip_port, endpoint_string);

    osi_RestorePreemption(0);
#ifdef SGIMIPS
    rpc_string_binding_compose(NULL,            /* uuid */
			       (unsigned char *)protSeq, /* protocol sequence */
			       (u_char *)netaddr_string,  /* netaddr */
			       (u_char *)endpoint_string, /* Endpoint */
			       NULL,            /* NULL options */
			       &newString,
			       &st);
#else
    rpc_string_binding_compose(NULL,            /* uuid */
			       protSeq,         /* protocol sequence */
			       (u_char *)netaddr_string,  /* netaddr */
			       (u_char *)endpoint_string, /* Endpoint */
			       NULL,            /* NULL options */
			       &newString,
			       &st);
#endif
    if (st != error_status_ok) {
	(void)osi_PreemptionOff();
	code = st;
	uprintf("%s: can't compose string binding (code %d)\n", caller, st);
	return code;
    }
    rpc_binding_from_string_binding(newString, &binding, &st);
    if (st != error_status_ok) {
	(void)osi_PreemptionOff();
	uprintf("%s: can't get rpc binding from strings %s (code %d)\n",
		caller, newString, st);
	code = st;
	goto out;
    }
    (void)osi_PreemptionOff();
    *rpcBinding = binding;

out:    
    rpc_string_free(&newString, &st);
    return code;

}

/*
 * Generate a fresh caller binding from the callee binding.
 */
int krpc_BindingFromInBinding(srcName, callee, callerp, inSockAddrp, outSockAddrp)
    char *srcName;
    handle_t callee;
    handle_t *callerp;
    struct sockaddr_in *inSockAddrp;	/* incoming sockaddr */
    struct sockaddr_in *outSockAddrp;	/* breakdown of callerp */
{
    unsigned_char_t *stringBinding = NULL,
	*protseqString = NULL,
	*netaddrString = NULL;
    char oldEndpoint[12];
    handle_t newHdl;
    sockaddr_p_t sap;
    struct sockaddr_in newAddr;
    unsigned32 ip_port;
    unsigned32 st;
    int code = 0;

    /*
     * NOTE: Assuming the protocol used here is UDP/IP. Otherwise, the
     *       code should be rewritten. 
     */

    bzero((char *)outSockAddrp, sizeof(*outSockAddrp));
    *callerp = NULL;

    osi_RestorePreemption(0);
    rpc_binding_to_string_binding(callee, &stringBinding, &st);
    if (st != error_status_ok) {
	uprintf("%s: can't get string binding: code %d\n",
		srcName, st);
	code = st;
	goto out;
    }
    rpc_string_binding_parse(stringBinding, NULL, &protseqString,
			     &netaddrString, NULL, NULL, &st);
    if (st != error_status_ok) {
	uprintf("%s: can't parse rpc string binding %s: code %d\n",
		srcName, stringBinding, st);
	code = st;
	goto out;
    }
    if (strcmp((char*)protseqString, (char*)protSeq)) {
	uprintf("%s: incoming binding uses %s, not %s\n",
		srcName, protseqString, protSeq);
    }
    /* Use the given endpoint, not the one from the client call handle */
    ip_port = ntohs(inSockAddrp->sin_port);
    osi_cv2string(ip_port, &oldEndpoint[0]);

    rpc_string_free(&stringBinding, &st);
#ifdef SGIMIPS
    rpc_string_binding_compose(NULL,            /* uuid */
			       (unsigned char *)protSeq, /* protocol sequence */
			       netaddrString,  /* netaddr */
			       (u_char *)oldEndpoint, /* Endpoint */
			       NULL,            /* NULL options */
			       &stringBinding,
			       &st);
#else
    rpc_string_binding_compose(NULL,            /* uuid */
			       protSeq,         /* protocol sequence */
			       netaddrString,  /* netaddr */
			       (u_char *)oldEndpoint, /* Endpoint */
			       NULL,            /* NULL options */
			       &stringBinding,
			       &st);
#endif
    if (st != error_status_ok) {
	uprintf("%s: can't compose string binding %s/%s: code %d\n",
		srcName, netaddrString, oldEndpoint, st);
	code = st;
	goto out;
    }
    rpc_binding_from_string_binding(stringBinding, &newHdl, &st);
    if (st != error_status_ok) {
	uprintf("%s: can't get rpc binding from string %s: code %d\n",
		srcName, stringBinding, st);
	code = st;
	goto out;
    }
    rpc__binding_inq_sockaddr(newHdl, &sap, &st);
    newAddr = *((struct sockaddr_in *) sap);
    if (newAddr.sin_addr.s_addr == inSockAddrp->sin_addr.s_addr) {
	/* This binding is no different from the existing one */
	rpc_binding_free(&newHdl, &st);
	code = -1;
	goto out;
    }
    
    /* Finally, something we wanted.  Set the OUT parameters. */
    *callerp = newHdl;
    *outSockAddrp = newAddr;

out:    
    (void)osi_PreemptionOff();
    if (stringBinding != NULL)
	rpc_string_free(&stringBinding, &st);
    if (protseqString != NULL)
	rpc_string_free(&protseqString, &st);
    if (netaddrString != NULL)
	rpc_string_free(&netaddrString, &st);
    return code;
}

/*
 * Generate a copy of a binding that has a different endpoint (port number).
 */
int krpc_CopyBindingWithNewPort(srcName, oldb, newbp, newPort)
    char *srcName;
    handle_t oldb;
    handle_t *newbp;
    int newPort;	/* in HOST order */
{
    unsigned_char_t *stringBinding = NULL,
	*protseqString = NULL,
	*netaddrString = NULL;
    char endpoint[12];
    handle_t newHdl;
    unsigned32 st;
    int code = 0;

    *newbp = NULL;

    osi_RestorePreemption(0);
    rpc_binding_to_string_binding(oldb, &stringBinding, &st);
    if (st != error_status_ok) {
	uprintf("%s: can't get string binding: code %d\n",
		srcName, st);
	code = st;
	goto out;
    }
    rpc_string_binding_parse(stringBinding, NULL, &protseqString,
			     &netaddrString, NULL, NULL, &st);
    if (st != error_status_ok) {
	uprintf("%s: can't parse rpc string binding %s: code %d\n",
		srcName, stringBinding, st);
	code = st;
	goto out;
    }
    if (strcmp((char*)protseqString, (char*)protSeq)) {
	uprintf("%s: incoming binding uses %s, not %s\n",
		srcName, protseqString, protSeq);
    }
    /* Use the given endpoint, not the one from the client call handle */
    osi_cv2string(newPort, &endpoint[0]);

    rpc_string_free(&stringBinding, &st);
#ifdef SGIMIPS
    rpc_string_binding_compose(NULL,            /* uuid */
			       (unsigned char *)protSeq, /* protocol sequence */
			       netaddrString,  /* netaddr */
			       (u_char *)endpoint, /* Endpoint */
			       NULL,            /* NULL options */
			       &stringBinding,
			       &st);
#else
    rpc_string_binding_compose(NULL,            /* uuid */
			       protSeq,         /* protocol sequence */
			       netaddrString,  /* netaddr */
			       (u_char *)endpoint, /* Endpoint */
			       NULL,            /* NULL options */
			       &stringBinding,
			       &st);
#endif
    if (st != error_status_ok) {
	uprintf("%s: can't compose string binding %s/%s: code %d\n",
		srcName, netaddrString, endpoint, st);
	code = st;
	goto out;
    }
    rpc_binding_from_string_binding(stringBinding, &newHdl, &st);
    if (st != error_status_ok) {
	uprintf("%s: can't get rpc binding from string %s: code %d\n",
		srcName, stringBinding, st);
	code = st;
	goto out;
    }
    /* What we wanted!  Set the OUT parameter. */
    *newbp = newHdl;

out:    
    (void)osi_PreemptionOff();
    if (stringBinding != NULL)
	rpc_string_free(&stringBinding, &st);
    if (protseqString != NULL)
	rpc_string_free(&protseqString, &st);
    if (netaddrString != NULL)
	rpc_string_free(&netaddrString, &st);
    return code;

}

/* 
 * The following functions and data structures are to manage an additional 
 * threads pool for the secondary service of each exported Interface.
 * NOTE: Since some of functions and data structures will be referenced by 
 * Knck, their implementations are very Knck specific. The current assumption 
 * is there is only one secondary service per exported interface. 
 */ 
typedef struct poolTableEntry {
   rpc_if_id_t  ifspecID;                /* the exported interface ID */
   rpc_thread_pool_handle_t secPool_h;   /* reserved threads for the service */
   int flags;
} poolTableEntry;

/* 
 * DFS exports only one IFspec 
 */
#define POOLSIZE 2
/* poolTableEntry flags */
#define POOL_CHECK_SEC_OBJ_UUID	1

static poolTableEntry  poolTable[POOLSIZE]; /* one Interface per entry */
static pthread_mutex_t pool_mutex;          /* mutex lock for pool Table */
static unsigned32  ThreadPoolInited = 0;    /* init flag */

/*
 * Locate a reserved thread-pool if the rpc call is for the secondary service.
 * NOTE: This function will be called by the kernel RPC runtime.
 */
static void krpc_LookupPool(objUUID, ifspecID, opnum, poolHandle, code)
     uuid_t  *objUUID;
     rpc_if_id_t *ifspecID;
     unsigned32 opnum;
     rpc_thread_pool_handle_t *poolHandle;
     unsigned32  *code;
{
    unsigned32 st, i;

    *code = 0;
    *poolHandle = NULL;
    if (uuid_is_nil(objUUID, &st))
        return;

   /* 
    * Check whether the call is from an appropriate ifspec,
    * Check whether the call is with the appropriate object UUID (ie., a 
    * secondary service id)
    */
    pthread_mutex_lock(&pool_mutex);
    for (i=0; (i < POOLSIZE); i++)  {
	if (uuid_equal(&ifspecID->uuid, &poolTable[i].ifspecID.uuid, &st)) {
	    if (!(poolTable[i].flags & POOL_CHECK_SEC_OBJ_UUID)) {
		*poolHandle = poolTable[i].secPool_h;
	    } else {
		if (dfsuuid_GetParity(objUUID) == 1) {
		    /* secondary interface */
		    *poolHandle = poolTable[i].secPool_h;
		}
	    }
	    break;
	}
    } 
    pthread_mutex_unlock(&pool_mutex);
    return;
}

/*
 * Create a secondary threads pool for handling particular service.
 * Register the provided threads-pool lookup function with KRPC runtime.
 * This function is only called during the PX initialization. 
 */
rpc_thread_pool_handle_t 
krpc_SetupThreadsPool(caller, ifSpec, serverName, numThreads, check_objuuid)
     char *caller;
     rpc_if_handle_t ifSpec;
     char *serverName;
     int numThreads;
     int check_objuuid;
{     
    rpc_thread_pool_handle_t poolPtr;
    rpc_if_id_t ifspec_id;
    boolean32 wait_flag = 1;
    unsigned32 i, st;
    static lookup_fn_set = 0;

    pthread_mutex_init (&pool_mutex, pthread_mutexattr_default);

    if (ThreadPoolInited == 0) {
       bzero((caddr_t)poolTable, sizeof poolTable);
       ThreadPoolInited = 1;
    }

    /* Limit addtional threads pools to FX and TKN servers */
    if ((strcmp(serverName, "FX") == 0) ||
	(strcmp(serverName, "TKN") == 0)) {
	rpc_if_inq_id(ifSpec, &ifspec_id, &st);
    } else {
	return NULL;
    }

    rpc_server_create_thread_pool(numThreads, &poolPtr, &st);
    if (st != error_status_ok) {
	uprintf("%s: can't create the thread pool (code %d)\n", caller, st);
	return NULL;
    }    
    /*
     * Add this entry to the threads pool table. Note, the secondary service
     * Object UUID is determined/added during caller's context setup.
     */
    for (i = 0; (i < POOLSIZE); i++) {
	if (poolTable[i].secPool_h == NULL) {
	    poolTable[i].ifspecID = ifspec_id;
	    poolTable[i].secPool_h = poolPtr;
	    if (check_objuuid)
		poolTable[i].flags |= POOL_CHECK_SEC_OBJ_UUID;
	    break;
	}
    }
    if (i >= POOLSIZE) {
	uprintf("%s: can't add a new Ifspec; Threads Pool overflow\n", caller);
	goto bad;
    }
    if (!lookup_fn_set) {
	rpc_server_set_thread_pool_fn(krpc_LookupPool, &st);
	if (st != error_status_ok) {
	    uprintf("%s: can't set the thread-pool function (code %d)\n", 
		    caller, st);
	    goto bad;
	}
	lookup_fn_set = 1;
    }
    return poolPtr;
bad:
    rpc_server_free_thread_pool(&poolPtr, wait_flag, &st);
    return NULL;
}

static struct icl_log *krpcLogp = 0;
static struct icl_set *krpcSetp = 0;

#if 0
/* This is just a sketch of the current AIX implementation. */
#define KRPCLOG_MAXSTRING 256
int krpc_dfs_icl_log_function(urgent, logmsg)
    int urgent;
    char *logmsg;
{
    char tstring[KRPCLOG_MAXSTRING + 1];
    int lockCode;

    if (!ICL_SETACTIVE(krpcSetp) && !urgent)
	return 0;		/* nothing to do */

    /* Only take a max of KRPCLOG_MAXSTRING chars */
    strncpy(tstring, logmsg, KRPCLOG_MAXSTRING);
    tstring[KRPCLOG_MAXSTRING] = '\0';
    /*
     * Note: special osi preemption macroes are used which allow
     * recursive locking.  This is because there may still be
     * some places in DFS which call the RPC with preemption
     * disabled.  This is better than having a machine crash on
     * a trace call.
     */
    lockCode = osi_PreemptionOffRecursionOK();
    icl_Trace1(krpcSetp, DFS_TRACE_GEN_MSG, ICL_TYPE_STRING, tstring);
    if (urgent) icl_SysLogError(tstring);
    osi_RestorePreemptionRecursionOK(lockCode);
    return 0;
}
#endif /* 0 */

#ifdef AFS_SUNOS5_ENV
/* Could be up to four extra parameters.  The typecodes say which are good. */
static void krpc_icltraceAny(unsigned32 id, unsigned32 typecodes,
			   long p1, long p2, long p3, long p4)
{
    if (ICL_SETACTIVE(krpcSetp))
	icl_Event4(krpcSetp, id, typecodes, p1, p2, p3, p4);
}

int krpc_setup_dfs_icl_log()
{
    long code;

    code = icl_CreateLog("cmfx", 0, &krpcLogp);
    if (code == 0) {
	code = icl_CreateSet("krpc", krpcLogp, (struct icl_log *) 0, &krpcSetp);
	krpc__register_trace_function(krpc_icltraceAny);
    }
    if (code)
	uprintf("dfs: warning: could not setup krpc log: %d\n", code);
    return code;
}

void once_krpc_setup_dfs_icl_log()
{
    long code;
    static pthread_once_t onceBlock = pthread_once_init;

    pthread_once(&onceBlock, (pthread_initroutine_t)krpc_setup_dfs_icl_log);
}
#endif /* AFS_SUNOS5_ENV */
