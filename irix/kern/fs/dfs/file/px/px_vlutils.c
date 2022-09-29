/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: px_vlutils.c,v 65.5 1998/04/01 14:16:11 gwehrman Exp $";
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
 *      Copyright (C) 1996, 1991 Transarc Corporation
 *      All rights reserved.
 */
#include <dcedfs/param.h>
#include <dcedfs/sysincludes.h>
#include <dcedfs/afs4int.h>
#include <dcedfs/basicincludes.h>
#include <dcedfs/osi.h>
#include <dcedfs/osi_net.h>
#include <dcedfs/lock.h>
#include <dcedfs/afsvl_data.h>
#include <dcedfs/flserver.h>
#include <dcedfs/fshs.h>
#include <px.h>
#include <dcedfs/nubik.h>
#ifdef SGIMIPS
#include <dcedfs/fldb_proc.h>
#endif

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/px/RCS/px_vlutils.c,v 65.5 1998/04/01 14:16:11 gwehrman Exp $")

/*
 * This module interacts with the AFS FileSet Location Servers
 */

#define	PXFL_DOWN	1		/* FL server is down */

typedef struct flconn {
    struct sockaddr_in serverAddr;
    handle_t connp; 
    short states;
} flconn;

static struct flconn 
    px_FLServerConns[MAXVLHOSTSPERCELL];	/* FLservers to contact */
static long px_FLServers = 0;			/* Number of vlservers */
static long px_FLServersDown = 0;		/* Are all vlservers down? */
static uuid_t FLObjectID;

/* 
 * Create a rpc binding to the FL Server 
 */
#ifdef SGIMIPS
static px_NewFLBinding(
    struct flconn *connp,
    uuid_t *ObjectID)
#else
static px_NewFLBinding(connp, ObjectID)
    struct flconn *connp;
    uuid_t *ObjectID;
#endif
{
    unsigned32 st;
    /* char princName[1024]; */

    if (connp->connp)
        rpc_binding_free(&connp->connp, &st);

#ifdef SGIMIPS
    st = krpc_BindingFromSockaddr("fx", protSeq, (afsNetAddr *)&connp->serverAddr, 
				  &connp->connp);
#else
    st = krpc_BindingFromSockaddr("fx", protSeq, &connp->serverAddr, 
				  &connp->connp);
#endif
    if (st != error_status_ok) {
        uprintf("fx: can't create a FL binding (code %d)\n", st);
	goto bad;
    }
    rpc_binding_set_object(connp->connp, ObjectID, &st);
    if (st != error_status_ok) {
        uprintf("fx: can't set FL ObjectID (code %d)\n", st);
	rpc_binding_free(&connp->connp, &st);
	goto bad;
    }    

#ifdef notdef /* May Be Necessary in the future! */	
    /*
     * Note: It takes a user-helper to do the binding authentication in 
     * kernel. For now, use none-authn binding to talk to flserver.
     * (Since, both VL_Probe and VL_GetEntryByID are open for any users.)
     */
    krpc_GetPrincName(princName, tcellp->cellName, HOSTNAME ????);

    rpc_binding_set_auth_info(connp->connp,
			      princName,
			      rpc_c_authn_level_pkt,
			      rpc_c_authn_none,
			      NULL,
			      rpc_c_authz_dce,
			      &st);
    if (st != error_status_ok) {
        uprintf("fx: can't set the authn info with princ %s (code %d)\n",
		princName, st);
	/* do nothing */
    }
    if (st != error_status_ok) {
        uprintf("fx: can't setup the timeout value (code %d)\n", st);
	/* do nothing */
    }
#endif /* ToBeDone, If necessary */	

    return st;

bad:
    connp->states = PXFL_DOWN;  /* Let's treat this flserver DOWN */
    connp->connp = NULL;
    return st;
}

/*
 * Check whether the FL servers are back 
 */
#ifdef SGIMIPS
int px_CheckFLServers(void)
#else
int px_CheckFLServers()
#endif
{
    flconn *connp;
    long i, code, anyUp;
    unsigned32 st;

    icl_Trace1(px_iclSetp, PX_TRACE_CHECKFLS, ICL_TYPE_LONG, px_FLServers);
    anyUp = 0;
    for (i = 0; i < px_FLServers; i++) {
	if (px_FLServerConns[i].states & PXFL_DOWN) {
	    connp = &px_FLServerConns[i];
	    st = px_NewFLBinding(connp, &FLObjectID);
	    if (st != rpc_s_ok)
		continue;
	    rpc_mgmt_set_com_timeout(connp->connp, rpc_c_binding_min_timeout+2,
				     &st);
	    icl_Trace0(px_iclSetp, PX_TRACE_CHECKFLSBEGIN);
	    code = VL_Probe(connp->connp);
	    icl_Trace1(px_iclSetp, PX_TRACE_CHECKFLSEND, ICL_TYPE_LONG, code);
	    if (code == 0) {
		px_FLServerConns[i].states &= ~PXFL_DOWN;
		px_FLServersDown = 0;
		osi_printIPaddr("fx: fileset location server ", 
				px_FLServerConns[i].serverAddr.sin_addr.s_addr,
				" back up\n");
		anyUp = 1;
	    }
	    rpc_mgmt_set_com_timeout(connp->connp,
				     rpc_c_binding_default_timeout, &st);
	} else
	    anyUp = 1;
    }
    if (anyUp == 0) {
	px_FLServersDown = 1;
    }
    return 120;
}

#ifdef SGIMIPS
px_initFLServers(
    struct fsop_cell *tcellp)
#else
px_initFLServers(tcellp)
    struct fsop_cell *tcellp;
#endif
{
    long i, temp;
    handle_t h;
    unsigned32 st;

    icl_Trace0(px_iclSetp, PX_TRACE_INITFLS);
    bzero((caddr_t)&px_FLServerConns[0], sizeof px_FLServerConns);
    /*
     * Assume the FLserver Hosts are coming in randomized.
     */
    FLObjectID = tcellp->fsServerGp;

    for (i = 0; i < MAXVLHOSTSPERCELL; ++i) {
	if (tcellp->hosts[i].type == 0)
	    break;
	temp = ((struct sockaddr_in *) &tcellp->hosts[i])->sin_addr.s_addr;
	if (temp == 0)
	    break;
	osi_KernelSockaddr(&tcellp->hosts[i]);
	*((struct afsNetAddr *) &px_FLServerConns[px_FLServers].serverAddr) =
	    tcellp->hosts[i];
	st = px_NewFLBinding(&px_FLServerConns[px_FLServers], &FLObjectID);
	if (st != rpc_s_ok) 
	    continue;
	++px_FLServers;
    }  /* for */
    icl_Trace1(px_iclSetp, PX_TRACE_INITFLSEND, ICL_TYPE_LONG, px_FLServers);
    return 0;
}

#ifdef notdef
px_FLGetEntryByID(volIdP, volType, volentP)
     afs_hyper_t *volIdP;
     unsigned long volType;
     struct vldbentry *volentP;
{
    register long count=0, pass=0, i, code;
    unsigned long st;
    handle_t connp;
    char *msg;

    code = -1;	/* default if nothing found */
    icl_Trace1(px_iclSetp, PX_TRACE_GETENTRY, ICL_TYPE_HYPER, volIdP);
    while(1) {
	if (!(connp = px_FLServerConns[count].connp)) {
	    if (pass == 0) {
		pass = 1;	/* in pass 1, we look at down hosts, too */
		count = 0;
		continue;
	    } else
		break;		/* nothing left to try */
	}
	if (pass == 0 && (px_FLServerConns[count].states & PXFL_DOWN)) {
	    count++;
	    icl_Trace1(px_iclSetp, PX_TRACE_GETENTRYNOCONN,
		       ICL_TYPE_POINTER, connp);
	    continue;	/* this guy's down, try someone else first */
	}
	icl_Trace1(px_iclSetp, PX_TRACE_GETENTRYSTART, ICL_TYPE_POINTER, connp);
	code = VL_GetEntryByID(connp, volIdP, volType, volentP);
	icl_Trace1(px_iclSetp, PX_TRACE_GETENTRYEND, ICL_TYPE_LONG, code);
	if (code < 0 || (code >= rpc_s_mod && code <= (rpc_s_mod + 4096))
	    || code == UNOTSYNC || code == UNOQUORUM) {
	    if (code == UNOTSYNC)
		msg = "fx: not sync site from fileset location server ";
	    else if (code == UNOQUORUM)
		msg = "fx: no quorum from fileset location server ";
	    else
		msg = "fx: rpc error from fileset location server ";
	    osi_printIPaddr(msg,
			    px_FLServerConns[count].serverAddr.sin_addr.s_addr,
			    "\n");
	    px_FLServerConns[count].states |= PXFL_DOWN;
	    if (code >= rpc_s_mod && code <= (rpc_s_mod + 4096))
		rpc_binding_reset(connp, &st);
	    count++;
	    continue;
	} else if (code == 0 || code == VL_NOENT) {
	    if (px_FLServerConns[count].states & PXFL_DOWN) {
		osi_printIPaddr("fx: fileset location server ",
				px_FLServerConns[count].serverAddr.sin_addr.s_addr,
				" back up\n");
		px_FLServerConns[count].states &= ~PXFL_DOWN;
		px_FLServersDown = 0;
	    }
	    return code;
	}
    } /* while(1) */

    for (i = 0; i < px_FLServers; i++) {
	if (!(px_FLServerConns[i].states & PXFL_DOWN))
	    break;
    }
    if (i >= px_FLServers)
	px_FLServersDown = 1;		/* XXX */
    return code;
}
#endif /* notdef */
