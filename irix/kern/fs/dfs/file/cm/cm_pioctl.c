/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: cm_pioctl.c,v 65.9 1999/07/22 20:42:47 gwehrman Exp $";
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
 *      Copyright (C) 1996, 1990 Transarc Corporation
 *      All rights reserved.
 */
#include <dcedfs/param.h>		/* Should be always first */
#include <dcedfs/stds.h>
#ifdef SGIMIPS
#include <dcedfs/common_data.h>
#endif /* SGIMIPS */
#include <dcedfs/osi.h>
#include <dcedfs/osi_cred.h>
#include <dcedfs/osi_user.h>
#include <dcedfs/afs4int.h>
#include <dcedfs/sysincludes.h>		/* for ioctl stuff */
#include <dcedfs/ioctl.h>
#include <dcedfs/dacl.h>
#include <dcedfs/syscall.h>
#include <cm.h>				/* Cm-based standard headers */
#include <cm_conn.h>
#include <cm_volume.h>
#include <cm_cell.h>
#include <cm_vcache.h>
#include <cm_dcache.h>
#include <cm_bkg.h>
#include <cm_server.h>
#include <cm_serverpref.h>
#include <cm_site.h>
#include <cm_vnodeops.h>
#include <cm_dnamehash.h>

RCSID("$Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/cm/RCS/cm_pioctl.c,v 65.9 1999/07/22 20:42:47 gwehrman Exp $")

#define PIOCTL_DECLARATION _TAKES((struct cm_scache *scp, \
				   long function, \
				   struct cm_rrequest *rreqp, \
				   char *inDatap, \
				   char *outDatap, \
				   long inSize, \
				   long *outSizep))

/*
 * Local functions for supported Pioctl ops
 */
static long cm_PBogus			PIOCTL_DECLARATION;
static long cm_PGetVolumeStatus		PIOCTL_DECLARATION;
static long cm_PSetVolumeStatus		PIOCTL_DECLARATION;
static long cm_PFlush			PIOCTL_DECLARATION;
static long cm_PCheckServers		PIOCTL_DECLARATION;
static long cm_PCheckVolNames		PIOCTL_DECLARATION;
static long cm_PFindVolume		PIOCTL_DECLARATION;
static long cm_PAccess			PIOCTL_DECLARATION;
static long cm_PGetFID			PIOCTL_DECLARATION;
static long cm_PGetCacheSize		PIOCTL_DECLARATION;
static long cm_PSetCacheSize		PIOCTL_DECLARATION;
static long cm_PListCells		PIOCTL_DECLARATION;
static long cm_PRemoveMount		PIOCTL_DECLARATION;
static long cm_PNewStatMount		PIOCTL_DECLARATION;
static long cm_PGetFileCell		PIOCTL_DECLARATION;
static long cm_PFlushVolumeData		PIOCTL_DECLARATION;
static long cm_PSetSysName		PIOCTL_DECLARATION;
static long cm_PResetStores		PIOCTL_DECLARATION;
static long cm_PListStores		PIOCTL_DECLARATION;
static long cm_PClockControl		PIOCTL_DECLARATION;
static long cm_PSetServPrefs		PIOCTL_DECLARATION;
static long cm_PGetServPrefs		PIOCTL_DECLARATION;
static long cm_PGetProtBounds		PIOCTL_DECLARATION;
static long cm_PSetProtBounds		PIOCTL_DECLARATION;
static long cm_PCreateMount		PIOCTL_DECLARATION;

/*
 * These functions do not go throught the table, and hence can have a
 * normal signature.
 */
static long cm_Prefetch		_TAKES((char *pathp,
					struct afs_ioctl *adata,
					int afollow,
					osi_cred_t *credp));

static long (*(pioctlSw[])) PIOCTL_DECLARATION = {
    cm_PBogus,			/* 0 */
    cm_PGetVolumeStatus,	/* 1 */
    cm_PSetVolumeStatus,	/* 2 */
    cm_PFlush,			/* 3 */
    cm_PCheckServers,		/* 4 */
    cm_PCheckVolNames,		/* 5 */
    cm_PFindVolume,		/* 6 */
    cm_PBogus,			/* 7 -- prefetch is now special-cased! */
    cm_PAccess,			/* 8 -- access using the PRS_FS bits */
    cm_PGetFID,			/* 9 -- get file ID */
    cm_PGetCacheSize,		/* 10 -- get cache size and usage */
    cm_PSetCacheSize,		/* 11 -- set cache size */
    cm_PListCells,		/* 12 -- get cell info */
    cm_PRemoveMount,		/* 13 -- delete mount point */
    cm_PNewStatMount,		/* 14 -- new style mount point stat */
    cm_PGetFileCell,		/* 15 -- get cell name for input file */
    cm_PFlushVolumeData,	/* 16 -- flush all data from a volume */
    cm_PSetSysName,		/* 17 -- Set system name */
    cm_PResetStores,		/* 18 -- reset background stores */
    cm_PListStores,		/* 19 -- list background stores */
    cm_PClockControl,		/* 20 -- read & set clock-setting attributes */
    cm_PSetServPrefs,		/* 21 -- Set Server Preferences */
    cm_PGetServPrefs,		/* 22 -- Get Server Preferences */
    cm_PGetProtBounds,		/* 23 -- Get Protection Bounds */
    cm_PSetProtBounds,		/* 24 -- Set Protection Bounds */
    cm_PCreateMount,		/* 25 -- delete mount point */
};

static long HandlePioctl(
  struct cm_scache *,
  long,
  struct afs_ioctl *,
  osi_cred_t *);

/*
 * Main entry for the pioctl(2) system call
 */
int afscall_cm_pioctl(
  long pathArg,
  long com,
  long ioctlArg,
  long follow,
  long ignoredParm5,
  int  *retvalP)
{
    char *pathp = (char *)pathArg;
    struct afs_ioctl data;
    struct vnode *vp = NULL;
    int code;

    if (!cm_globalVFS && !osi_suser(osi_getucred())) {	/* Afsd NOT running! */
	return EINVAL;
    }

    *retvalP = 0;			/* clear any return value */
#ifdef SGIMIPS64
#define AFS_IOCTL_32_SIZE       0xc

    if(ABI_IS_IRIX5(get_current_abi()) || ABI_IS_IRIX5_N32(get_current_abi())) {
        int com_ioctl_size = (int)((com >> 16) & 0xff);

        if (com_ioctl_size == AFS_IOCTL_32_SIZE)
                com = (com & 0xff00ffff) | 0x00180000;
    }
#endif /* SGIMIPS64 */
    if (!_VALIDAFSIOCTL(com))
	return EINVAL;
#ifdef SGIMIPS64
    if(ABI_IS_IRIX5(get_current_abi()) || ABI_IS_IRIX5_N32(get_current_abi())) {
        struct afs_ioctl_32 {
                unsigned32 in, out;
                short in_size, out_size;
        } local_afs_ioctl;

        code = osi_copyin((caddr_t)ioctlArg, (caddr_t) &local_afs_ioctl, 
			   sizeof(struct afs_ioctl_32));
        if(!code) {
                data.in = (char *)(__psint_t) local_afs_ioctl.in;
                data.out = (char *)(__psint_t) local_afs_ioctl.out;
                data.in_size = local_afs_ioctl.in_size;
                data.out_size = local_afs_ioctl.out_size;
        }
    }
    else
        code = osi_copyin((caddr_t)ioctlArg, (caddr_t) &data, sizeof (data));
#else  /* SGIMIPS64 */
    code = osi_copyin((caddr_t)ioctlArg, (caddr_t)&data, sizeof (data));
#endif /* SGIMIPS64 */

    if (code != 0)
	return code;

    if ((com & 0xff) == 7) {
	/*
	 * Special case prefetch so entire pathname eval occurs in helper
	 * process. otherwise, the pioctl call is essentially useless
	 */
#ifdef SGIMIPS
	return (int) cm_Prefetch(pathp, &data, (int)follow, osi_getucred());
#else  /* SGIMIPS */
	return cm_Prefetch(pathp, &data, follow, osi_getucred());
#endif /* SGIMIPS */
    }

    if (pathp != NULL) {
#ifdef SGIMIPS
	osi_RestorePreemption(0);
#endif /* SGIMIPS */
	code = osi_lookupname(pathp, OSI_UIOUSER,
			      (follow ? FOLLOW_LINK : NO_FOLLOW),
			      (struct vnode **) 0, &vp);
#ifdef SGIMIPS
	osi_PreemptionOff();
#endif /* SGIMIPS */
	if (code != 0)
	    return code;
    }

    /*
     * Now make the call if we were passed no file, or were passed an AFS file
     */
    if (vp == NULL || osi_IsAfsVnode(vp)) {
#ifdef SGIMIPS
	struct cm_scache *scp;
	
	if (vp) VTOSC(vp, scp);
	code = (int) HandlePioctl( vp ? scp: 0, com, &data, osi_getucred());
#else  /* SGIMIPS */
	code = HandlePioctl(vp ? VTOSC(vp) : 0, com, &data, osi_getucred());
#endif /* SGIMIPS */
    } else
	code = EINVAL;		/* not in /afs */

    if (vp != NULL)
	OSI_VN_RELE(vp);		/* put vnode back */

    return code;
}


/*
 * Execute pioctl: Get incoming parameters, call the appropriate operation
 * and pass the output parameters to the out data buffer
 */
static long
HandlePioctl(
  struct cm_scache *scp,
  long acom,
  struct afs_ioctl *opaquep,
  osi_cred_t *credp)
{
    struct cm_rrequest rreq;
    long code, function, inSize, outSize;
    char *inDatap, *outDatap;

    icl_Trace2(cm_iclSetp, CM_TRACE_PIOCTL, ICL_TYPE_POINTER, scp,
	       ICL_TYPE_LONG, acom & 0xff);
    cm_InitReq(&rreq, credp);
    function = acom & 0xff;
    if (function >= (sizeof(pioctlSw) / sizeof(pioctlSw[0])))
	return EINVAL;			/* out of range */
    inSize = opaquep->in_size;
    if (inSize >= osi_BUFFERSIZE)
	return E2BIG;
    inDatap = osi_AllocBufferSpace();
    if (inSize > 0) {
#ifdef SGIMIPS
	code = osi_copyin(opaquep->in, inDatap, (int)inSize);
#else  /* SGIMIPS */
	code = osi_copyin(opaquep->in, inDatap, inSize);
#endif /* SGIMIPS */
	if (code) {
	    osi_FreeBufferSpace((struct osi_buffer *)inDatap);
	    return code;
	}
    }
    outDatap = osi_AllocBufferSpace();
    outSize = 0;

    code = (*pioctlSw[function])(scp, function, &rreq, inDatap, outDatap,
				 inSize, &outSize);
    osi_FreeBufferSpace((struct osi_buffer *)inDatap);
    if (code == 0 && opaquep->out_size > 0) {
	if (outSize > opaquep->out_size)
	    outSize = opaquep->out_size;
	if (outSize >= osi_BUFFERSIZE)
	    code = E2BIG;
	else if	(outSize)
#ifdef SGIMIPS
	    code = osi_copyout(outDatap, opaquep->out, (int)outSize);
#else  /* SGIMIPS */
	    code = osi_copyout(outDatap, opaquep->out, outSize);
#endif /* SGIMIPS */
    }
    osi_FreeBufferSpace((struct osi_buffer *)outDatap);
    return cm_CheckError(code, &rreq);
}


/*
 * Get Volume status call
 */
/* ARGSUSED */
static long
cm_PGetVolumeStatus(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    long temp;
    struct cm_volume *volp;

    if (!scp)
	return EINVAL;

    if (volp = scp->volp)
	temp = (volp->states & VL_SAVEBITS);
    else
	temp = 0;	/* we're in the global name space */
#ifdef SGIMIPS64
    if(ABI_IS_IRIX5(get_current_abi()) || ABI_IS_IRIX5_N32(get_current_abi())) {
        signed32 temp_32 = (signed32)temp;

        bcopy((char *) &temp_32, outDatap, sizeof(temp_32));
        *outSizep = sizeof(temp_32);
    }
    else {
        bcopy((char *) &temp, outDatap, sizeof(temp));
        *outSizep = sizeof(temp);
    }
#else  /* SGIMIPS64 */
    bcopy((char *) &temp, outDatap, sizeof(temp));
    *outSizep = sizeof(temp);
#endif /* SGIMIPS64 */
    return 0;
}


/*
 * Set volume status call
 */
/* ARGSUSED */
static long
cm_PSetVolumeStatus(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    long temp;
    register struct cm_volume *volp;

    if (!scp)
	return EINVAL;

    if (!osi_suser(osi_getucred()))
	return EACCES;

#ifdef SGIMIPS64
    if(ABI_IS_IRIX5(get_current_abi()) || ABI_IS_IRIX5_N32(get_current_abi())) {
        signed32 temp_32;

        bcopy(inDatap, (char *) &temp_32, sizeof(temp_32));
        temp = temp_32;
    }
    else
        bcopy(inDatap, (char *) &temp, sizeof(temp));
#else  /* SGIMIPS64 */
    bcopy(inDatap, (char *) &temp, sizeof(temp));
#endif /* SGIMIPS64 */
    volp = scp->volp;

    if (!volp) return EINVAL;	/* can't do it in DFS global name space */

    /* now store in the bits */
    volp->states &= ~VL_SAVEBITS;
    volp->states |= (temp & VL_SAVEBITS);
    return 0;
}


/*
 * Flush file call
 */
/* ARGSUSED */
static long
cm_PFlush(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    int i;
    long code;

    if (!scp)
	return EINVAL;

    lock_ObtainWrite(&scp->llock);
    for (i = 0; i < 100; i++) {
	if (scp->modChunks == 0 && !cm_NeedStatusStore(scp))
	    break;
	lock_ReleaseWrite(&scp->llock);
	code = cm_SyncDCache(scp, 0, (osi_cred_t *) 0);
	if (code) return code;
	code = cm_SyncSCache(scp);
	if (code) return code;
	lock_ObtainWrite(&scp->llock);
    }
    /* if looped too long, give up */
    if (i >= 100) {
	lock_ReleaseWrite(&scp->llock);
	return EBUSY;
    }

    /* if we make it here, it is fine to zap the status info.
     * TryToSmush won't discard dirty data.
     */
    cm_ForceReturnToken(scp, TKN_UPDATE, 0);
    lock_ReleaseWrite(&scp->llock);
    cm_TryToSmush(scp, /* sync */ 1);
    return 0;
}


/*
 * Check up (or down) servers call
 */
/* ARGSUSED */
static long
cm_PCheckServers(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    register char *cp = outDatap;
    struct cm_server *serverp;
    struct cm_cell *cellp = NULL;
    unsigned long maxTimeInterval = (u_long)-1;
    int i, len = 0;
    long opcode = 0, returnCode = 0;

    /*
     * op code = 0 : check all cells;  ie., cellp == NULL
     * op code = 1 : fast check, check all cells;  ie., cellp == NULL
     * op code = 2 : check local cell only
     * op code = 3 : fast, local cell check!
     */
#ifdef SGIMIPS64
    if(ABI_IS_IRIX5(get_current_abi()) || ABI_IS_IRIX5_N32(get_current_abi())) {
        signed32 opcode_32;

        bcopy(inDatap, (char *) &opcode_32, sizeof(signed32));
        inDatap += sizeof(signed32); inSize -= sizeof(signed32);
        opcode = opcode_32;
    }
    else {
        bcopy(inDatap, (char *) &opcode, sizeof(long));
        inDatap += sizeof(long); inSize -= sizeof(long);
    }
#else  /* SGIMIPS64 */
    bcopy(inDatap, (char *) &opcode, sizeof(long));
    inDatap += sizeof(long); inSize -= sizeof(long);
#endif /* SGIMIPS64 */
    /*
     * Then, check whether the user provids a specified cell,
     * If not, check to see if it is only for the local cell,
     * If not, must be for all cells, ie., cellp == NULL;
     */
    if (inSize > 0) {
#ifdef SGIMIPS
	i = (int)strlen(inDatap);
	if (i > inSize) i = (int)inSize;
#else  /* SGIMIPS */
	i = strlen(inDatap);
	if (i > inSize) i = inSize;
#endif /* SGIMIPS */
    } else
	i = 0;
    if (i > 0) { /* length of the specified cell name */
	cellp = cm_GetCellByName(inDatap);
	if (cellp == NULL) { /* no such a cell */
	    returnCode = -1;
#ifdef SGIMIPS64
        if(ABI_IS_IRIX5(get_current_abi()) || ABI_IS_IRIX5_N32(get_current_abi())) {
                signed32 returnCode_32 = (signed32)returnCode;

                bcopy((char *) &returnCode_32, cp, sizeof(signed32));
                cp += sizeof(signed32);
        }
        else {
            bcopy((char *) &returnCode, cp, sizeof(long));
            cp += sizeof(long);
        }
#else /* SGIMIPS64 */
	    bcopy((char *) &returnCode, cp, sizeof(long));
	    cp += sizeof(long);
#endif /* SGIMIPS64 */
	    *outSizep = cp - outDatap;
	    return 0;
	}
    } else if (opcode & 0x2) {   /* Only check local cell */
	cellp = cm_GetCellByName(cm_localCellName);
    }

    if ((opcode & 0x1) == 0 ) {		/* Not fast check */
	(void) cm_CheckDownServers(cellp, maxTimeInterval);
	(void) cm_PingServers(cellp, maxTimeInterval);
    }

    /*
     * Now send back the returnCode and the current down server list
     */

#ifdef SGIMIPS64
    if(ABI_IS_IRIX5(get_current_abi()) || ABI_IS_IRIX5_N32(get_current_abi())) {
        signed32 returnCode_32 = (signed32)returnCode;

        bcopy((char *) &returnCode_32, cp, sizeof(signed32));
        cp += sizeof(signed32);
    }
    else {
        bcopy((char *) &returnCode, cp, sizeof(long));
        cp += sizeof(long);
    }
#else  /* SGIMIPS64 */
    bcopy((char *) &returnCode, cp, sizeof(long));
    cp += sizeof(long);
#endif /* SGIMIPS64 */

    lock_ObtainRead(&cm_serverlock);
    for (i = 0; i < SR_NSERVERS; i++) {
	for (serverp = cm_servers[i]; serverp; serverp = serverp->next) {
	    if (cellp && cellp != serverp->cellp) {
		continue;
	    }
	    /* Go ahead, we don't GC servers */
	    lock_ReleaseRead(&cm_serverlock);
	    lock_ObtainRead(&serverp->lock);
	    lock_ObtainRead(&cm_siteLock);
	    if ((serverp->states & SR_DOWN) && (len < osi_BUFFERSIZE)) {
		bcopy((char *) &serverp->sitep->addrCurp->addr, cp,
		      sizeof(struct sockaddr_in));
		osi_UserSockaddr((struct sockaddr *)cp);
		len += sizeof(struct sockaddr_in);
		cp += sizeof(struct sockaddr_in);
	    }
	    lock_ReleaseRead(&cm_siteLock);
	    lock_ReleaseRead(&serverp->lock);
	    lock_ObtainRead(&cm_serverlock);
	}
    }
    lock_ReleaseRead(&cm_serverlock);

    *outSizep = cp - outDatap;
    return 0;
}


/*
 * Check backup/readonly volumes call
 */
/* ARGSUSED */
static long
cm_PCheckVolNames(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    cm_CheckVolumeNames(1);
    cm_ResetAllVDirs();
    return 0;
}


/*
 * Return addresses of servers associated with the volume call
 */
/* ARGSUSED */
static long
cm_PFindVolume(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    struct cm_volume *volp;
    long i;
    long maxi;
    char *cp = outDatap;
    struct cm_server	*servers[CM_VL_MAX_HOSTS];

    if (!scp)
	return EINVAL;
    if (volp = cm_GetVolume(&scp->fid, rreqp)) {
	/* Prefix the volume's host address(es) with a count of them. */
	lock_ObtainRead(&volp->lock);
	for (i = 0; i < CM_VL_MAX_HOSTS; i++) {
	    if (!volp->serverHost[i])
		break;
	    servers[i] = volp->serverHost[i];
	}
	lock_ReleaseRead(&volp->lock);

	maxi = i;	/* from 0 to CM_VL_MAX_HOSTS, inclusive. */
#ifdef SGIMIPS64
        if(ABI_IS_IRIX5(get_current_abi()) || ABI_IS_IRIX5_N32(get_current_abi())) {
                signed32 maxi_32 = (signed32)maxi;

                bcopy((char *) &maxi_32, cp, sizeof(signed32));
                cp += sizeof(signed32);
        }
        else {
                bcopy((char *) &maxi, cp, sizeof(long));
                cp += sizeof(long);
        }
#else  /* SGIMIPS64 */
	bcopy((char *) &maxi, cp, sizeof(long));
	cp += sizeof(long);
#endif /* SGIMIPS64 */

	lock_ObtainRead(&cm_siteLock);
	for (i = 0; i < maxi; i++) {
	    bcopy((char *) &servers[i]->sitep->addrCurp->addr, cp,
		  sizeof(struct sockaddr_in));
	    osi_UserSockaddr((struct sockaddr *)cp);
	    cp += sizeof(struct sockaddr_in);
	}
	lock_ReleaseRead(&cm_siteLock);

	/* Return the volume name also, if we know it. */
	lock_ObtainRead(&volp->lock);
	if (volp->volnamep) {
#ifdef SGIMIPS
	    maxi = (int) strlen(volp->volnamep) + 1;
#else  /* SGIMIPS */
	    maxi = strlen(volp->volnamep) + 1;
#endif /* SGIMIPS */
	    bcopy(volp->volnamep, cp, maxi);
	    cp += maxi;
	} else {
	    *cp++ = '\0';	/* null-terminate the name */
	}
	lock_ReleaseRead(&volp->lock);
	*outSizep = cp - outDatap;
	cm_PutVolume(volp);
	return 0;
    }
    return ENODEV;
}


/*
 * Prefetch file if background daemon(s) available
 */
static long
cm_Prefetch(
  char *pathp,
  struct afs_ioctl *adata,
  int afollow,
  osi_cred_t *credp)
{
    register char *tp;
    long code = 0;
    size_t bufferSize;

    if (!pathp)
	return EINVAL;
    tp = osi_AllocBufferSpace();
    code = osi_copyinstr(pathp, tp, 1024, &bufferSize);
    if (code) {
	osi_FreeBufferSpace((struct osi_buffer *)tp);
	return code;
    }

    /* try to queue request, reporting failure if need be */
    if (cm_bkgQueue(BK_PATH_OP, (struct cm_scache *)0, 0, credp,
		    (long)tp, 0 /* chunk number */, afollow, 0, 0) == 0) {
	/* request failed, free up buffer and return error */
	osi_FreeBufferSpace((struct osi_buffer *) tp);
	return EWOULDBLOCK;
    }
    /* if we make it here, tp will be freed by daemon */
    return 0;
}


/*
 * Check if we have access to a particular file
 */
/* ARGSUSED */
static long
cm_PAccess(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    long temp, code = 0;

    if (!scp)
	return EINVAL;
#ifdef SGIMIPS64
    if(ABI_IS_IRIX5(get_current_abi()) || ABI_IS_IRIX5_N32(get_current_abi())) {
        signed32 temp_32;

        bcopy(inDatap, (char *)&temp_32, sizeof(signed32));
        temp = temp_32;
    }
    else
        bcopy(inDatap, (char *)&temp, sizeof(long));
#else  /* SGIMIPS64 */
    bcopy(inDatap, (char *)&temp, sizeof(long));
#endif /* SGIMIPS64 */
    /* check access, returns EACCES if fails, error code if fails
     * to get answer, and 0 for success.
     */
    code = cm_AccessError(scp, temp, rreqp);
    return code;
}

/*
 * Get the file handle associated with a file
 */
/* ARGSUSED */
static long
cm_PGetFID(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    if (!scp)
	return EINVAL;
    bcopy((char *) &scp->fid, outDatap, sizeof(afsFid));
    *outSizep = sizeof(afsFid);
    return 0;
}


/*
 * Set a new cache size call
 */
/* ARGSUSED */
static long
cm_PSetCacheSize(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    long newValue;

    if (!osi_suser(osi_getucred()))
	return EACCES;

    if (cm_diskless)
	return EINVAL;

#ifdef SGIMIPS64
    if(ABI_IS_IRIX5(get_current_abi()) || ABI_IS_IRIX5_N32(get_current_abi())) {
        signed32 newValue_32;

        bcopy(inDatap, (char *) &newValue_32, sizeof(signed32));
        newValue = newValue_32;
    }
    else
        bcopy(inDatap, (char *) &newValue, sizeof(long));
#else  /* SGIMIPS64 */
    bcopy(inDatap, (char *) &newValue, sizeof(long));
#endif /* SGIMIPS64 */
    lock_ObtainWrite(&cm_dcachelock);
    if (newValue == 0)
	newValue = cm_origCacheBlocks;
    /*
     * Make sure that there is at least two free chunk blocks worth of
     * space left in the cache.
     */
    newValue = cm_GetSafeCacheSize(newValue);

    /* Change the cache size after assuring that we'll have enough space. */
    cm_cacheBlocks = newValue;
    lock_ReleaseWrite(&cm_dcachelock);
    cm_CheckSize(0);

   return 0;
}

/*
 * Lists information about a particular cell
 */
/* ARGSUSED */
static long cm_PListCells(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    long whichCell, maxi;
    register long i;
    register struct cm_cell *cellp;
    register char *cp;

#ifdef SGIMIPS64
    if(ABI_IS_IRIX5(get_current_abi()) || ABI_IS_IRIX5_N32(get_current_abi())) {
        unsigned32 whichCell_32;

        bcopy(inDatap, (char *) &whichCell_32, sizeof(unsigned32));
        whichCell = whichCell_32;
    }
    else
       bcopy(inDatap, (char *) &whichCell, sizeof(long));
#else  /* SGIMIPS64 */
    bcopy(inDatap, (char *) &whichCell, sizeof(long));
#endif /* SGIMIPS64 */
    lock_ObtainRead(&cm_celllock);
    for (cellp = cm_cells; cellp; cellp = cellp->next) {
	if (whichCell == 0)
	    break;
	whichCell--;
    }
    if (cellp) {
	lock_ObtainRead(&cellp->lock);
	lock_ObtainRead(&cm_siteLock);

	cp = outDatap;

	for (i = 0; i < MAXVLHOSTSPERCELL; i++) {
	    if (cellp->cellHosts[i] == 0)
		break;
	}
	maxi = i;	/* from 0 to MAXVLHOSTSPERCELL */

#ifdef SGIMIPS64
        if(ABI_IS_IRIX5(get_current_abi()) || ABI_IS_IRIX5_N32(get_current_abi())) {
                signed32 maxi_32 = (signed32)maxi;

                bcopy((char *) &maxi_32, cp, sizeof(signed32));
                cp += sizeof(signed32);
        }
        else {
                bcopy((char *) &maxi, cp, sizeof(long));
                cp += sizeof(long);
        }
#else  /* SGIMIPS64 */
	bcopy((char *) &maxi, cp, sizeof(long));
	cp += sizeof(long);
#endif /* SGIMIPS64 */

	for (i = 0; i < maxi; i++) {
	    bcopy((char *) &cellp->cellHosts[i]->sitep->addrCurp->addr, cp,
		  sizeof(struct sockaddr_in));
	    osi_UserSockaddr((struct sockaddr *)cp);
	    cp += sizeof(struct sockaddr_in);
	}

	strcpy(cp, cellp->cellNamep);
	cp += strlen(cellp->cellNamep)+1;
	*outSizep = cp - outDatap;

	lock_ReleaseRead(&cm_siteLock);
	lock_ReleaseRead(&cellp->lock);
    }
    lock_ReleaseRead(&cm_celllock);
    if (cellp)
	return 0;
    else
	return EDOM;
}

/*
 * Create an AFS mount point call
 */
/* ARGSUSED */
static long
cm_PCreateMount(
  struct cm_scache *dscp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
#ifdef SGIMIPS
    long code;
#else  /* SGIMIPS */
    long code, alen, srvix;
#endif /* SGIMIPS */
    char *srcp, *tgtp;
    struct cm_CreateMountPoint *mountp = (struct cm_CreateMountPoint*) inDatap;

    icl_Trace1(cm_iclSetp, CM_TRACE_CRMOUNT, ICL_TYPE_POINTER, dscp);
    if (!dscp)
	return EINVAL;
    if (inSize < sizeof(struct cm_CreateMountPoint)
	|| mountp->nameOffset >= inSize
	|| mountp->pathOffset >= inSize
	|| (mountp->nameOffset + mountp->nameLen) > inSize
	|| (mountp->pathOffset + mountp->pathLen) > inSize)
	return E2BIG;

    srcp = &inDatap[mountp->nameOffset];

    tgtp = &inDatap[mountp->pathOffset];
    if (*tgtp != '#' && *tgtp != '%' && *tgtp != '!') {
	return EINVAL;
    }
    code = cm_SymlinkOrMount(dscp, srcp, mountp->nameTag, mountp->nameLen,
			     tgtp, mountp->pathTag, mountp->pathLen, 1, rreqp);
    icl_Trace1(cm_iclSetp, CM_TRACE_CRMOUNT_END, ICL_TYPE_LONG, code);
    return code;
}


/*
 * Remove an AFS mount point call
 */
/* ARGSUSED */
static long
cm_PRemoveMount(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    long code = 0;
    long srvix;
    register struct cm_conn *connp;
    struct cm_scache *mntscp;
    struct lclHeap {
	afsFetchStatus OutFileStatus;
	afsFetchStatus OutDirStatus;
	afsFidTaggedName inName;
	afsVolSync tsync;
    };
#ifdef AFS_SMALLSTACK_ENV
    struct lclHeap *lhp = NULL;
#else /* AFS_SMALLSTACK_ENV */
    struct lclHeap lh;
    struct lclHeap *lhp = &lh;
#endif /* AFS_SMALLSTACK_ENV */
    afsFid removeFid;
    afsVolSync *tsyncp;
    long irevcnt;
    u_long startTime;
    struct cm_volume *volp = 0;
    afs_hyper_t openID;
    error_status_t st;

    /* Need a parent directory that's not a virtual directory. */
    if (!scp)
	return EINVAL;
    if (cm_vType(scp) != VDIR) {
	return ENOTDIR;
    }
    if (scp->states & SC_VDIR)
	return EROFS;
    if (inSize >= sizeof(lhp->inName.name.tn_chars))
	return E2BIG;

#ifdef AFS_SMALLSTACK_ENV
    /* Keep stack frame small by doing heap-allocation */
    lhp = (struct lclHeap *)osi_AllocBufferSpace();
    /* this is essentially a compile-time check, which will compile into
     * a noop or a panic.
     */
#ifdef SGIMIPS
#pragma set woff 1171
#endif
    /* CONSTCOND */
    osi_assert(sizeof(struct lclHeap) <= osi_BUFFERSIZE);
#ifdef SGIMIPS
#pragma reset woff 1171
#endif
#endif /* AFS_SMALLSTACK_ENV */

    bzero((char *) &lhp->inName, sizeof(lhp->inName));
    strncpy((char *)&lhp->inName.name.tn_chars[0], inDatap, inSize);
    /*
     * Make sure that the file pointed by inName is indeed a fileset mnt point.
     */
    code = nh_dolookup(scp, (char *)lhp->inName.name.tn_chars, &removeFid, &mntscp, rreqp);
    if (code == 0) {
	if (!mntscp)
	    (void) cm_GetSCache(&removeFid, &mntscp, rreqp);
	if (mntscp) {
	    lock_ObtainWrite(&mntscp->llock);
	    if ((mntscp->states & SC_MOUNTPOINT) == 0) {
		 /* the given file is not really a fileset mnt. */
		code = EINVAL;
	    } else {
#ifdef AFS_SUNOS5_ENV
		if (CM_RC(mntscp) == 1 && mntscp->opens != 0) {
		    /* now re-lock and check for real */
		    lock_ReleaseWrite(&mntscp->llock);
		    lock_ObtainWrite(&cm_scachelock);
		    cm_CheckOpens(mntscp, 1);
		    lock_ReleaseWrite(&cm_scachelock);
		    lock_ObtainWrite(&mntscp->llock);
		}
#endif /* AFS_SUNOS5_ENV */
		/* return the open token, if not held, to the server */
		if (mntscp->opens == 0) {
		    AFS_hzero(openID);
		    cm_ReturnSingleOpenToken(mntscp, &openID);
		}
	    }
	    lock_ReleaseWrite(&mntscp->llock);
	    cm_PutSCache(mntscp);
	} else {
	    code = ENOENT;
	}
    }
    if (code)
	goto out;

    lock_ObtainRead(&scp->llock);
    /* need data token before looking at dirRevokes */
    code = cm_GetTokens(scp, TKN_READ, rreqp, READ_LOCK);
    irevcnt = scp->dirRevokes;
    lock_ReleaseRead(&scp->llock);
    if (code)
	goto out;

    do {
	if (connp = cm_Conn(&scp->fid,   MAIN_SERVICE(SRT_FX),
			    rreqp, &volp, &srvix)) {
	    icl_Trace0(cm_iclSetp, CM_TRACE_REMOVEMOUNTSTART);
	    startTime = osi_Time();
	    osi_RestorePreemption(0);
	    st = AFS_RemoveFile(connp->connp, &scp->fid, &lhp->inName,
				&openID, &osi_hZero,  0, &lhp->OutDirStatus,
				&lhp->OutFileStatus, &removeFid, &lhp->tsync);
	    osi_PreemptionOff();
	    code = osi_errDecode(st);
	    icl_Trace1(cm_iclSetp, CM_TRACE_REMOVEMOUNTEND, ICL_TYPE_LONG, code);
	} else
	    code = -1;
    } while (cm_Analyze(connp, code, &scp->fid, rreqp, volp, srvix, startTime));
    if (!code) {
	code = cm_UpdateStatus(scp, &lhp->OutDirStatus, inDatap, (char *)0,
			       (afsFid *)0, rreqp, irevcnt);
    }
out:
    if (volp)
	cm_EndVolumeRPC(volp);
#ifdef AFS_SMALLSTACK_ENV
    osi_FreeBufferSpace((struct osi_buffer *)lhp);
#endif /* AFS_SMALLSTACK_ENV */
    return code;
}


/*
 * Get information from a mount object
 */
/* ARGSUSED */
static long cm_PNewStatMount(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    struct cm_scache *tscp;
    afsFid tfid;
    long code;

    if (!scp)
	return EINVAL;
    if (cm_vType(scp) != VDIR)
	return ENOTDIR;
    code = nh_dolookup(scp, inDatap, &tfid, &tscp, rreqp);
    if (code)
	return code;
    if (!tscp) {
	tfid.Cell = scp->fid.Cell;
	tfid.Volume = scp->fid.Volume;
	if (cm_GetSCache(&tfid, &tscp, rreqp))
	    return ENOENT;
    }
    /* Ensure that it's a link and marked as a mountpoint. */
    if (cm_vType(tscp) != VLNK || !(tscp->states & SC_MOUNTPOINT)) {
	cm_PutSCache(tscp);
	return EINVAL;
    }
    lock_ObtainWrite(&tscp->llock);
    if (!(code = cm_HandleLink(tscp, rreqp))) {
	if (tscp->linkDatap) {
	    strcpy(outDatap, tscp->linkDatap);	/* we have the data */
#ifdef SGIMIPS
	    *outSizep = (long)strlen(tscp->linkDatap)+1;
#else  /* SGIMIPS */
	    *outSizep = strlen(tscp->linkDatap)+1;
#endif /* SGIMIPS */
	} else
	    code = EIO;
    }
    lock_ReleaseWrite(&tscp->llock);
    cm_PutSCache(tscp);
    return code;
}


/*
 * Get cell name associated with a particular file
 */
/* ARGSUSED */
static long
cm_PGetFileCell(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    register struct cm_cell *cellp;

    if (!scp)
	return EINVAL;
    if (!(cellp = cm_GetCell(&scp->fid.Cell)))
	return ESRCH;
    strcpy(outDatap, cellp->cellNamep);
#ifdef SGIMIPS
    *outSizep = (long)strlen(outDatap) + 1;
#else  /* SGIMIPS */
    *outSizep = strlen(outDatap) + 1;
#endif /* SGIMIPS */
    return 0;
}

/*
 * Flush all cached data associated with a particular volume
 */
/* ARGSUSED */
static long
cm_PFlushVolumeData(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    register struct cm_dcache *dcp;
    afs_hyper_t volume, cell;
    long code;
    long i;

    if (!scp)
	return EINVAL;

    volume = scp->fid.Volume;	/* who to zap */
    cell = scp->fid.Cell;
    lock_ObtainWrite(&cm_dcachelock);	/* needed flushing any stuff! */
    /* look at each cache file, and see if it is part of the volume
     * we want to nuke.
     */
    for (i = 0; i < cm_cacheFiles; i++) {
	dcp = cm_GetDSlot(i, (struct cm_dcache *) 0);
	if (dcp->refCount <= 1) {
	    if (AFS_hsame(dcp->f.fid.Volume, volume) &&
		AFS_hsame(dcp->f.fid.Cell, cell)) {

		/* now, we're going to look at the vnode corresponding
		 * to this chunk, and remove any pages first, since
		 * we need to maintain the invariant that dirty pages
		 * are always backed by real chunks.
		 * After we do this, someone could create a page, so we'll
		 * check again below to avoid flushing a chunk that has
		 * dirty pages.  We just skip those chunks, since it is
		 * an acceptable failure mode for this call to miss a
		 * chunk once in a great while.
		 */
		lock_ReleaseWrite(&cm_dcachelock);
		lock_ObtainWrite(&cm_scachelock);
		scp = cm_FindSCache(&dcp->f.fid);
		lock_ReleaseWrite(&cm_scachelock);
		if (scp) {
		    /* lock vnode so we can block out getpages */
		    lock_ObtainWrite(&scp->llock);
		    scp->activeRevokes++;
		    lock_ReleaseWrite(&scp->llock);

		    code = cm_VMDiscardChunk(scp, dcp, 0);
		    /*
		     * This chunk is now clean; track it so FlushDCache
		     * will get called.
		     */
		    lock_ObtainWrite(&cm_dcachelock);
		    if (code == 0)
			cm_indexFlags[i] &= ~(DC_IFANYPAGES | DC_IFDIRTYPAGES);
		    lock_ReleaseWrite(&cm_dcachelock);
		    lock_ObtainWrite(&scp->llock);
		    cm_PutActiveRevokes(scp);
		    lock_ReleaseWrite(&scp->llock);
		    cm_PutSCache(scp);
		}
		lock_ObtainWrite(&cm_dcachelock);
		if (!(cm_indexFlags[i] & (DC_IFFREE | DC_IFDATAMOD |
					DC_IFANYPAGES | DC_IFFLUSHING))
		    && (dcp->refCount == 1)) {
		    cm_FlushDCache(dcp);
		}
	    }	/* if (same volume and cell) */
	}	/* if last reference to chunk */
	dcp->refCount--;		/* bumped by getdslot */
    }	/* for loop over all chunks */
    lock_ReleaseWrite(&cm_dcachelock);
    return 0;
}


/* ARGSUSED */
static long
cm_PSetSysName(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    char *cp, inname[CM_MAXSYSNAME], outname[CM_MAXSYSNAME];
    int setsysname;
    long foundname = 0;

    bzero(inname, sizeof(inname));
#ifdef SGIMIPS
    bcopy(inDatap, (char *) &setsysname, sizeof(int));
    inDatap += sizeof(int); inSize -= sizeof(int);
#else  /* SGIMIPS */
    bcopy(inDatap, (char *) &setsysname, sizeof(long));
    inDatap += sizeof(long); inSize -= sizeof(long);
#endif /* SGIMIPS */
    if (setsysname && inSize > 0) {
	if (inSize >= sizeof(inname))
	    return EINVAL;
	strncpy(inname, inDatap, inSize);
    }
    if (!cm_sysname)
	return EINVAL;
	/* panic("PSetSysName: !cm_sysname\n"); */
    if (!setsysname) {
	strcpy(outname, cm_sysname);
	foundname = 1;
    } else {
	/*
	 * Local guy; only root can change sysname
	 */
	if (!osi_suser(osi_getucred()))
	    return EACCES;
	strcpy(cm_sysname, inname);
    }
    if (!setsysname) {
	cp = outDatap;
#ifdef SGIMIPS64
     if(ABI_IS_IRIX5(get_current_abi()) || ABI_IS_IRIX5_N32(get_current_abi())) {
        unsigned32 foundname_32 = (unsigned32)foundname;

        bcopy((char *) &foundname_32, cp, sizeof(unsigned32));
        cp += sizeof(unsigned32);
     }
     else {
        bcopy((char *) &foundname, cp, sizeof(long));
        cp += sizeof(long);
     }
#else  /* SGIMIPS64 */
	bcopy((char *) &foundname, cp, sizeof(long));
	cp += sizeof(long);
#endif /* SGIMIPS64 */
	if (foundname) {
	    strcpy(cp, outname);
	    cp += strlen(outname)+1;
	}
	*outSizep = cp - outDatap;
    }
    return 0;
}


/*
 * Get the cached status information (i.e. cache size, cache used)
 */
#define MAXGCSTATS	16
/* ARGSUSED */
static long
cm_PGetCacheSize(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    long results[MAXGCSTATS];

    bzero((char *) results, sizeof(results));
    lock_ObtainRead(&cm_dcachelock);
    results[0] = cm_cacheBlocks;
    results[1] = cm_blocksUsed;
    results[2] = (cm_diskless ? 1 : 0);
    lock_ReleaseRead(&cm_dcachelock);
#ifdef SGIMIPS64
    if(ABI_IS_IRIX5(get_current_abi()) || ABI_IS_IRIX5_N32(get_current_abi())) {
        signed32 results_32[MAXGCSTATS];
        int i;

        for(i = 0; i < MAXGCSTATS; i++)
                results_32[i] = (signed32)results[i];

        bcopy((char *) results_32, outDatap, sizeof(results_32));
        *outSizep = sizeof(results_32);
    }
    else {
        bcopy((char *) results, outDatap, sizeof(results));
        *outSizep = sizeof(results);
    }
#else  /* SGIMIPS64 */
    bcopy((char *) results, outDatap, sizeof(results));
    *outSizep = sizeof(results);
#endif /* SGIMIPS64 */
    return 0;
}


/*
 * Track down all files that are being background stored due to errors
 * while storing previously, and reset these files so that we give up on
 * them.
 */
/* ARGSUSED */
static long
cm_PResetStores(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    register struct cm_scache *tscp;
    register int i;
#ifdef SGIMIPS
    int s;
#endif /* SGIMIPS */

    lock_ObtainWrite(&cm_scachelock);
    for (i = 0; i < SC_HASHSIZE; i++) {
	for (tscp = cm_shashTable[i]; tscp; tscp = tscp->next) {
#ifdef SGIMIPS
            s = VN_LOCK(SCTOV(tscp));
            if ( SCTOV(tscp)->v_flag & VINACT ) {
                osi_assert(CM_RC(tscp) == 0);
                VN_UNLOCK(SCTOV(tscp), s);
                continue;
            }
            CM_RAISERC(tscp);
            VN_UNLOCK(SCTOV(tscp), s);
#else  /* SGIMIPS */
	    CM_HOLD(tscp);
#endif /* SGIMIPS */
	    lock_ReleaseWrite(&cm_scachelock);
	    lock_ObtainWrite(&tscp->llock);
	    /* now check to see if this is a file we're going to abort */
	    if (tscp->states & SC_STOREFAILED) {
		/* yes, nuke this file */
		cm_MarkBadSCache(tscp, ESTALE);
	    }
	    lock_ReleaseWrite(&tscp->llock);
	    lock_ObtainWrite(&cm_scachelock);
	    CM_RELE(tscp);
	}
    }
    lock_ReleaseWrite(&cm_scachelock);
    return 0;
}

/*
 * Return status about stores that are being retried in the background.
 * Returns structure that contains:
 *  a long count of # of files stored
 *  a long count of # of fileset IDs returned below
 *  an array of up to 8 hypers giving up to 8 fileset IDs naming
 *   those filesets containing files whose stores are being retried.
 */
#define CM_MAXSTOREIDS	8
/* ARGSUSED */
static long
cm_PListStores(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    afs_hyper_t volIDs[CM_MAXSTOREIDS];
#ifdef SGIMIPS
    unsigned32 allStores;
#else /* SGIMIPS */
    long allStores;
#endif /* SGIMIPS */
    register int i;
    int j;
    int foundIt;
    register struct cm_scache *tscp;
    char *tp;
#ifdef SGIMIPS
    unsigned32 hyperCount;
    int s;
#else /* SGIMIPS */
    long hyperCount;
#endif /* SGIMIPS */

    /* initialize counters */
    hyperCount = 0;
    allStores = 0;

    lock_ObtainWrite(&cm_scachelock);
    for (i = 0; i < SC_HASHSIZE; i++) {
	for (tscp = cm_shashTable[i]; tscp; tscp = tscp->next) {
	    if (tscp->states & SC_STOREFAILED) {
#ifdef SGIMIPS
                s = VN_LOCK(SCTOV(tscp));
                if ( SCTOV(tscp)->v_flag & VINACT ) {
                        osi_assert(CM_RC(tscp) == 0);
                        VN_UNLOCK(SCTOV(tscp), s);
                        continue;
                }
                CM_RAISERC(tscp);
                VN_UNLOCK(SCTOV(tscp), s);
#else /* SGIMIPS */
		CM_HOLD(tscp);
#endif /* SGIMIPS */
		lock_ReleaseWrite(&cm_scachelock);
		lock_ObtainRead(&tscp->llock);
		if (tscp->states & SC_STOREFAILED) {
		    allStores++;
		    foundIt=0;
		    for (j = 0; j < hyperCount; j++) {
			if (AFS_hcmp(volIDs[j], tscp->fid.Volume) == 0) {
			    foundIt = 1;
			    break;
			}
		    }
		    if (!foundIt && hyperCount < CM_MAXSTOREIDS) {
			volIDs[hyperCount] = tscp->fid.Volume;
			hyperCount++;
		    }
		}
		lock_ReleaseRead(&tscp->llock);
		lock_ObtainWrite(&cm_scachelock);
		CM_RELE(tscp);
	    }
	}
    }
    lock_ReleaseWrite(&cm_scachelock);

    /* now copy out the gathered data */
    tp = outDatap;
#ifdef SGIMIPS
    bcopy((char *) &allStores, tp, sizeof(unsigned32));
    tp += sizeof(unsigned32);
    bcopy((char *) &hyperCount, tp, sizeof(unsigned32));
    tp += sizeof(unsigned32);
#else /* SGIMIPS */
    bcopy((char *) &allStores, tp, sizeof(long));
    tp += sizeof(long);
    bcopy((char *) &hyperCount, tp, sizeof(long));
    tp += sizeof(long);
#endif /* SGIMIPS */
    bcopy((char *) volIDs, tp, ((int) hyperCount * sizeof(afs_hyper_t)));
#ifdef SGIMIPS
    *outSizep = (2 * sizeof(unsigned32)) + (hyperCount * sizeof(afs_hyper_t));
#else  /* SGIMIPS */
    *outSizep = (2 * sizeof(long)) + (hyperCount * sizeof(afs_hyper_t));
#endif /* SGIMIPS */
    return 0;
}


/*
 * Read or set the clock management.
 */
/* ARGSUSED */
static long cm_PClockControl(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    extern struct afsNetAddr cm_setTimeHostAddr;
    extern struct afsTimeval cm_timeWhenSet;
    extern unsigned long cm_timeResidue;
    struct afsTimeval now;
    long temp;
    register char *cp;

#ifdef SGIMIPS64
    if(ABI_IS_IRIX5(get_current_abi()) || ABI_IS_IRIX5_N32(get_current_abi())) {
        unsigned32 temp;
        unsigned32 afscall_timeSynchDistance_32;
        unsigned32 afscall_timeSynchDispersion_32;
        unsigned32 cm_timeResidue_32;

      if (inSize < 2*sizeof(signed32))
                return EINVAL;
      cp = inDatap;
      bcopy(cp, (char *) &temp, sizeof(unsigned32));
      cp += sizeof(unsigned32); inSize -= sizeof(unsigned32);
    /* check style--must be zero */
    if (temp != 0)
        return EINVAL;
    /* check whether read or write */
    if (temp == 0) {
        /* Read the values */
        cp = outDatap;
        bcopy((char *) &temp, cp, sizeof(unsigned32)); /* style == 0 */
        cp += sizeof(unsigned32);
        bcopy((char *) &temp, cp, sizeof(unsigned32)); /* RW == 0 */
        cp += sizeof(unsigned32);
        temp = (cm_setTime ? 1 : 0);
        bcopy((char *) &temp, cp, sizeof(unsigned32)); /* whether setting time v
ia FS */
        cp += sizeof(unsigned32);
#ifdef SGIMIPS
        osi_afsGetTime(&now);
#else /* SGIMIPS */
        osi_GetTime(((struct timeval *) &now));
#endif /* SGIMIPS */
        bcopy((char *) &now, cp, sizeof(struct afsTimeval)); /* current time */
        cp += sizeof(struct afsTimeval);
        afscall_timeSynchDistance_32 = afscall_timeSynchDistance;
        bcopy((char *) &afscall_timeSynchDistance_32, cp, sizeof(unsigned32)); /* synchDist */
        cp += sizeof(unsigned32);
        afscall_timeSynchDispersion_32 = afscall_timeSynchDispersion;
        bcopy((char *) &afscall_timeSynchDispersion_32, cp, sizeof(unsigned32));
 /* synchDisp */
        cp += sizeof(unsigned32);
        bcopy((char *) &cm_setTimeHostAddr, cp, sizeof(cm_setTimeHostAddr));
/* source FS */
        cp += sizeof(struct afsTimeval);
        bcopy((char *) &cm_timeWhenSet, cp, sizeof(struct afsTimeval)); /* time
when set */
        cp += sizeof(struct afsTimeval);
        cm_timeResidue_32 = (unsigned32)cm_timeResidue;
        bcopy((char *) &cm_timeResidue_32, cp, sizeof(unsigned32));     /* left
to be set */
        cp += sizeof(unsigned32);
        *outSizep = cp - outDatap;
    } else {
        if (!osi_suser(osi_getucred()))
            return EACCES;
        if (inSize < sizeof(unsigned32))
            return EINVAL;
        /* Whether the CM sets the clock via FS */
        bcopy(cp, (char *) &temp, sizeof(unsigned32));
        cp += sizeof(unsigned32); inSize -= sizeof(unsigned32);
        if (temp)
            cm_setTime = 1;
        else
            cm_setTime = 0;
        if (inSize >= (sizeof(struct afsTimeval) + 2*sizeof(unsigned32))) {
            /* Skip the current time--set it via other syscalls. */
            cp += sizeof(struct afsTimeval); /* inSize -= sizeof(struct afsTimev
al); */
            /* Copy in the current synch distance and dispersion. */
            bcopy(cp, (char *) &afscall_timeSynchDistance_32,
                  sizeof(unsigned32));
            cp += sizeof(unsigned32);
            bcopy(cp, (char *) &afscall_timeSynchDispersion_32,
                  sizeof(unsigned32));

            afscall_timeSynchDistance = afscall_timeSynchDistance_32;
            afscall_timeSynchDispersion = afscall_timeSynchDispersion_32;
        }
        /* Skip the remainder. */
    }
      return 0;
    }
#endif /* SGIMIPS64 */

    if (inSize < 2*sizeof(unsigned long))
	return EINVAL;
    cp = inDatap;
    bcopy(cp, (char *) &temp, sizeof(unsigned long));
    cp += sizeof(unsigned long); inSize -= sizeof(unsigned long);
    /* check style--must be zero */
    if (temp != 0)
	return EINVAL;
    bcopy(cp, (char *) &temp, sizeof(unsigned long));
    cp += sizeof(unsigned long); inSize -= sizeof(unsigned long);
    /* check whether read or write */
    if (temp == 0) {
	/* Read the values */
	cp = outDatap;
	bcopy((char *) &temp, cp, sizeof(unsigned long)); /* style == 0 */
	cp += sizeof(unsigned long);
	bcopy((char *) &temp, cp, sizeof(unsigned long)); /* RW == 0 */
	cp += sizeof(unsigned long);
	temp = (cm_setTime ? 1 : 0);
	bcopy((char *) &temp, cp, sizeof(unsigned long)); /* whether setting time via FS */
	cp += sizeof(unsigned long);
#ifdef SGIMIPS
	osi_afsGetTime( &now);
#else /* SGIMIPS */
	osi_GetTime(((struct timeval *) &now));
#endif /* SGIMIPS */
	bcopy((char *) &now, cp, sizeof(struct afsTimeval)); /* current time */
	cp += sizeof(struct afsTimeval);
	bcopy((char *) &afscall_timeSynchDistance, cp, sizeof(unsigned long)); /* synchDist */
	cp += sizeof(unsigned long);
	bcopy((char *) &afscall_timeSynchDispersion, cp, sizeof(unsigned long)); /* synchDisp */
	cp += sizeof(unsigned long);
	bcopy((char *) &cm_setTimeHostAddr, cp, sizeof(cm_setTimeHostAddr));	/* source FS */
	cp += sizeof(struct afsTimeval);
	bcopy((char *) &cm_timeWhenSet, cp, sizeof(struct afsTimeval));	/* time when set */
	cp += sizeof(struct afsTimeval);
	bcopy((char *) &cm_timeResidue, cp, sizeof(unsigned long));	/* left to be set */
	cp += sizeof(unsigned long);
	*outSizep = cp - outDatap;
    } else {
	if (!osi_suser(osi_getucred()))
	    return EACCES;
	if (inSize < sizeof(unsigned long))
	    return EINVAL;
	/* Whether the CM sets the clock via FS */
	bcopy(cp, (char *) &temp, sizeof(unsigned long));
	cp += sizeof(unsigned long); inSize -= sizeof(unsigned long);
	if (temp)
	    cm_setTime = 1;
	else
	    cm_setTime = 0;
	if (inSize >= (sizeof(struct afsTimeval) + 2*sizeof(unsigned long))) {
	    /* Skip the current time--set it via other syscalls. */
	    cp += sizeof(struct afsTimeval); /* inSize -= sizeof(struct afsTimeval); */
	    /* Copy in the current synch distance and dispersion. */
	    bcopy(cp, (char *) &afscall_timeSynchDistance,
		  sizeof(unsigned long));
	    cp += sizeof(unsigned long);
	    bcopy(cp, (char *) &afscall_timeSynchDispersion,
		  sizeof(unsigned long));
	}
	/* Skip the remainder. */
    }
    return 0;
}

/***********************************************************************
 *
 * cm_PGetServPrefs(scp, function, rreqp, inDatap, outDatap,
 *		    inSize, outSizeP):		Get server preference
 *							values.
 *
 ***********************************************************************/

/* ARGSUSED */
static long
cm_PGetServPrefs(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    register struct sprefrequest *spin;	/* input */
    register struct sprefinfo *spout;	/* output */
    register struct spref *srvout;	/* one output component */
    register unsigned curoffset = 0;	/* counter for hash table traversal */
    struct cm_server *srvrp = NULL;	/* one of CM's server structs */
    cm_addrRank_t *srp = NULL;          /* one of CM's rank requests */
    struct cm_siteAddr *siteaddrp;      /* one of server's addresses */

    static struct loop_control {
	u_short		svc;
	u_short		flag;
    } prefloop_control[] = {
      {SRT_FL,  CM_PREF_FLDB},
      {SRT_FX,  CM_PREF_FX}
#if defined(CM_DEBUG_PREFS)
	  ,
      {SRT_REP, CM_PREF_REP}
#endif
    };

    int j, pref_bound = sizeof(prefloop_control)/sizeof(struct loop_control);

    if (inSize < sizeof (struct sprefrequest)) {
	return EINVAL;
    } else {
	spin = ((struct sprefrequest *) inDatap);
    }

    /*
     * struct sprefinfo includes 1 server struct...  that size gets added
     * in during the loop that follows.
     * Initialize outgoing results.
     */
    *outSizep = sizeof(struct sprefinfo) - sizeof (struct spref);
    spout = (struct sprefinfo *) outDatap;
    spout->next_offset = spin->offset;
    spout->num_servers = 0;
    srvout = spout->servers;

    /* Check that at least one result expected */
    if (spin->num_servers == 0) {
	return (0);
    }

    /* Loop over types of servers from which to extract addrs/ranks. */
    lock_ObtainRead(&cm_siteLock);

    for (j = 0; j < pref_bound; j++) {
	/* Walk server hash table.  Step up to requested offset, find servers
	 * of type specified in prefloop_control[i].svc.
	 */
	srvrp = NULL;
	for (;;) {
	    srvrp = cm_FindNextServer(srvrp, prefloop_control[j].svc);

	    if (srvrp == NULL) {
		/* done with this server type */
		break;
	    }

	    if (spin->offset > curoffset++) {
		/* catch up to where we left off */
		continue;
	    }

	    /* Get this server's data and adjust output info. */
	    for (siteaddrp = srvrp->sitep->addrListp;
		 siteaddrp;
		 siteaddrp = siteaddrp->next_sa) {
		/* output server's (addr, svc, rank) values */
		srvout->host  = siteaddrp->addr;
		srvout->rank  = siteaddrp->rank;
	        srvout->flags = prefloop_control[j].flag;

		/* adjust output buffer size/pointer and address count */
		*outSizep += sizeof(struct spref);
		srvout++;

		spout->num_servers++;  /* really server-addr count */

		/* Watch out for trying to return too much at once */
		if ((*outSizep > osi_BUFFERSIZE - sizeof(struct spref)) ||
		    (spout->num_servers >= spin->num_servers)) {
		    lock_ReleaseRead(&cm_siteLock);
		    return 0;
		}
	    }

	    /* Increment offset to that of next server */
	    spout->next_offset++;
	}
    }

    lock_ReleaseRead(&cm_siteLock);

    /* Iterate through all the requests in the cm_addrRankReqs list. */

    lock_ObtainRead(&cm_addrRankReqsLock);

    for (srp = cm_addrRankReqs; srp; srp = srp->next) {
	if (spin->offset > curoffset++) {
	    /* catch up to where we left off */
	    continue;
	}

	/* Get address preference info. */
	srvout->host.sin_addr = srp->addr;
	srvout->rank = srp->rank;
	srvout->flags = CM_PREF_QUEUED + srp->svc;

	/* Adjust output buffer size/pointer and address count */
	*outSizep += sizeof(struct spref);
	srvout++;

	spout->num_servers++;  /* really server-addr count */

	/* Increment offset to that of next server-preference entry */
	spout->next_offset++;

	/* Watch out for trying to return too much at once */
	if ((*outSizep > osi_BUFFERSIZE - sizeof(struct spref)) ||
	    (spout->num_servers >= spin->num_servers)) {
	    break;
	}
    }
    lock_ReleaseRead(&cm_addrRankReqsLock);

    if (srp == NULL) {
	/* Got everything we have to offer! Start all over the next time */
	spout->next_offset = 0;
    }

    return 0;
}



/***********************************************************************
 *
 * cm_PSetServPrefs(scp, function, rreqp, inDatap, outDatap,
 *		    inSize, outSizeP):		Set server preference
 *						values.
 *
 ***********************************************************************/

/* ARGSUSED */
static long
cm_PSetServPrefs(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    register struct spref	*sp;
    u_short			svc;

    if (!osi_suser(osi_getucred())) {
	return EACCES;
    }

    for (sp = (struct spref *) inDatap; inSize >= sizeof (struct spref);
	 sp++, inSize -= sizeof (struct spref)) {
	/*
	 * Older clients (cm) may not set flag; not a problem since
	 * default is to do the file server not the fldb server
	 */
	if ((sp->flags & CM_PREF_MASK) == CM_PREF_FLDB) {
	    svc = SRT_FL;
	} else {
	    svc = SRT_FX;
	}

	cm_SiteAddrSetRankAll(&sp->host, svc, sp->rank);
    }
    return(0);
}



/*
 * Get the current protection bounds for local and non-local cells
 */
/* ARGSUSED */
static long
cm_PGetProtBounds(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    struct cm_ProtBounds *resp = (struct cm_ProtBounds *)outDatap;

    /* Start this message with a format tag value of 1 */
    resp->formatTag = 1;
    resp->local = cm_localSec;
    resp->nonLocal = cm_nonLocalSec;
    *outSizep = sizeof(*resp);
    return 0;
}

/* Set the protection bounds for local and non-local cells */
/* ARGSUSED */
static long
cm_PSetProtBounds(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    struct cm_ProtBounds *newp = (struct cm_ProtBounds *)inDatap;
    int result;

    if (!osi_suser(osi_getucred()))
	return EACCES;

    if (inSize < (sizeof(struct cm_ProtBounds)))
	return EINVAL;
    if (newp->formatTag != 1)
	return EINVAL;

    /* reset cached authn bounds for all file-servers */
    cm_ResetAuthnForServers();

    /* set CM authn bounds as specified */
    result = cm_SetSecBounds(&newp->local, &newp->nonLocal, 1);

    return result;
}


/* ARGSUSED */
static long
cm_PBogus(
  struct cm_scache *scp,
  long function,
  struct cm_rrequest *rreqp,
  char *inDatap,
  char *outDatap,
  long inSize,
  long *outSizep)
{
    return EINVAL;
}
