/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: osi_dfs2oserr.c,v 65.3 1998/03/23 16:26:36 gwehrman Exp $";
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
 * $Log: osi_dfs2oserr.c,v $
 * Revision 65.3  1998/03/23 16:26:36  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 19:58:23  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/24  14:29:50  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:44  jdoak
 * Initial revision
 *
 * Revision 64.1  1997/02/14  19:46:50  dce
 * *** empty log message ***
 *
 * Revision 1.2  1996/04/06  00:17:33  bhim
 * No Message Supplied
 *
 * Revision 1.1.102.2  1994/06/09  14:15:03  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:27:37  annie]
 *
 * Revision 1.1.102.1  1994/02/04  20:23:56  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:15:25  devsrc]
 * 
 * Revision 1.1.100.1  1993/12/07  17:29:28  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  15:57:42  jaffe]
 * 
 * Revision 1.1.2.4  1993/01/21  14:50:03  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  14:51:50  cjd]
 * 
 * Revision 1.1.2.3  1992/09/25  18:32:05  jaffe
 * 	Cleanup Minor header differences
 * 	[1992/09/24  15:43:20  jaffe]
 * 
 * Revision 1.1.2.2  1992/08/31  20:23:20  jaffe
 * 	Transarc delta: jaffe-ot4719-cleanup-gcc-Wall-in-osi 1.3
 * 	  Selected comments:
 * 	    Fixed many compiler warnings in the osi directory.
 * 	    Reworked ALL of the header files.  All files in the osi directory now
 * 	    have NO machine specific ifdefs.  All machine specific code is in the
 * 	    machine specific subdirectories.  To make this work, additional flags
 * 	    were added to the afs/param.h file so that we can tell if a particular
 * 	    platform has any additional changes for a given osi header file.
 * 	    make osi_initDecodeTable return void.
 * 	    Corrected errors that appeared while trying to build everything on AIX3.2
 * 	    cleanup for OSF1 compilation
 * 	[1992/08/30  03:11:50  jaffe]
 * 
 * $EndLog$
 */

/*
 * (C) Copyright 1992 Transarc Corporation
 * All Rights Reserved
 */

#include <osi_dfserrors.h>
#include <sys/errno.h>

unsigned int err_DFSToLocal[MAXKERNELERRORS+1];

/*
 * This routine is used to initialize the Error Mapping Table which is used by
 * the CM to perform the decoding work. This routine is invoked when when the
 * CM is initialized.
 */
void
osi_initDecodeTable()
{
    int i;
    for (i= 0; i <= MAXKERNELERRORS; i++)
	err_DFSToLocal[i] = ENOEXIST; /* not defined */

    err_DFSToLocal[DFS_ESUCCESS] 	= 0;
    err_DFSToLocal[DFS_EPERM]		= EPERM;
    err_DFSToLocal[DFS_ENOENT]		= ENOENT;
    err_DFSToLocal[DFS_ESRCH]		= ESRCH;
    err_DFSToLocal[DFS_EINTR]		= EINTR;
    err_DFSToLocal[DFS_EIO]		= EIO;
    err_DFSToLocal[DFS_ENXIO]		= ENXIO;
    err_DFSToLocal[DFS_E2BIG]		= E2BIG;
    err_DFSToLocal[DFS_ENOEXEC]		= ENOEXEC;
    err_DFSToLocal[DFS_EBADF]		= EBADF;
    err_DFSToLocal[DFS_ECHILD]		= ECHILD;
    err_DFSToLocal[DFS_EDEADLK]		= EDEADLK;	/* 45 */
    err_DFSToLocal[DFS_ENOMEM]		= ENOMEM;
    err_DFSToLocal[DFS_EACCES]		= EACCES;
    err_DFSToLocal[DFS_EFAULT]		= EFAULT;
    err_DFSToLocal[DFS_ENOTBLK]		= ENOTBLK;
    err_DFSToLocal[DFS_EBUSY]		= EBUSY;
    err_DFSToLocal[DFS_EEXIST]		= EEXIST;
    err_DFSToLocal[DFS_EXDEV]		= EXDEV;
    err_DFSToLocal[DFS_ENODEV]		= ENODEV;
    err_DFSToLocal[DFS_ENOTDIR]		= ENOTDIR;
    err_DFSToLocal[DFS_EISDIR]		= EISDIR;
    err_DFSToLocal[DFS_EINVAL]		= EINVAL;
    err_DFSToLocal[DFS_ENFILE]		= ENFILE;
    err_DFSToLocal[DFS_EMFILE]		= EMFILE;
    err_DFSToLocal[DFS_ENOTTY]		= ENOTTY;
    err_DFSToLocal[DFS_ETXTBSY]		= ETXTBSY;    
    err_DFSToLocal[DFS_EFBIG]		= EFBIG;
    err_DFSToLocal[DFS_ENOSPC]		= ENOSPC;	
    err_DFSToLocal[DFS_ESPIPE]		= ESPIPE;	
    err_DFSToLocal[DFS_EROFS]		= EROFS;	
    err_DFSToLocal[DFS_EMLINK]		= EMLINK;	
    err_DFSToLocal[DFS_EPIPE]		= EPIPE;
    err_DFSToLocal[DFS_EDOM]		= EDOM;	
    err_DFSToLocal[DFS_ERANGE]		= ERANGE;	

    /* EWOULDBLOCK is __KBASE+101 (1101) if KERNEL, and 11 otherwise */
    err_DFSToLocal[DFS_EWOULDBLOCK]	= EWOULDBLOCK;

    err_DFSToLocal[DFS_EINPROGRESS]	= EINPROGRESS;
    err_DFSToLocal[DFS_EALREADY]	= EALREADY;
/* 
 * ipc/network software
 */
    err_DFSToLocal[DFS_ENOTSOCK]	= ENOTSOCK;	/* 95 */
    err_DFSToLocal[DFS_EDESTADDRREQ]	= EDESTADDRREQ;	/* 96 */
    err_DFSToLocal[DFS_EMSGSIZE]	= EMSGSIZE;	/* 97 */
    err_DFSToLocal[DFS_EPROTOTYPE]	= EPROTOTYPE;	/* 98 */
    err_DFSToLocal[DFS_ENOPROTOOPT]	= ENOPROTOOPT;	/* 99 */
    err_DFSToLocal[DFS_EPROTONOSUPPORT]	= EPROTONOSUPPORT;	/* 120 */
    err_DFSToLocal[DFS_ESOCKTNOSUPPORT]	= ESOCKTNOSUPPORT;	/* 121 */
    err_DFSToLocal[DFS_EOPNOTSUPP]	= EOPNOTSUPP;	/* 122 */
    err_DFSToLocal[DFS_EPFNOSUPPORT]	= EPFNOSUPPORT;	/* 123 */
    err_DFSToLocal[DFS_EAFNOSUPPORT]	= EAFNOSUPPORT;	/* 124 */
    err_DFSToLocal[DFS_EADDRINUSE]	= EADDRINUSE;	/* 125 */
    err_DFSToLocal[DFS_EADDRNOTAVAIL]	= EADDRNOTAVAIL;	/* 126 */
    err_DFSToLocal[DFS_ENETDOWN]	= ENETDOWN;	/* 127 */
    err_DFSToLocal[DFS_ENETUNREACH]	= ENETUNREACH;  /* 128 */	
    err_DFSToLocal[DFS_ENETRESET]	= ENETRESET;	/* 129 */
    err_DFSToLocal[DFS_ECONNABORTED]	= ECONNABORTED;	/* 130 */
    err_DFSToLocal[DFS_ECONNRESET]	= ECONNRESET;	/* 131 */
    err_DFSToLocal[DFS_ENOBUFS]		= ENOBUFS;	/* 132 */
    err_DFSToLocal[DFS_EISCONN]		= EISCONN;	/* 133 */
    err_DFSToLocal[DFS_ENOTCONN]	= ENOTCONN;	/* 134 */
    err_DFSToLocal[DFS_ESHUTDOWN]	= ESHUTDOWN;	/* 143 */

    err_DFSToLocal[DFS_ETOOMANYREFS]    = ETOOMANYREFS;	/* 144 */

    err_DFSToLocal[DFS_ETIMEDOUT]	= ETIMEDOUT;	/* 145 */
    err_DFSToLocal[DFS_ECONNREFUSED]	= ECONNREFUSED;	/* 146 */
    err_DFSToLocal[DFS_ELOOP]		= ELOOP;	/* 90 */
    err_DFSToLocal[DFS_ENAMETOOLONG]	= ENAMETOOLONG;	/* 78 */
    err_DFSToLocal[DFS_EHOSTDOWN]	= EHOSTDOWN;	/* 147 */
    err_DFSToLocal[DFS_EHOSTUNREACH]	= EHOSTUNREACH;	/* 148 */
    err_DFSToLocal[DFS_ENOTEMPTY]	= ENOTEMPTY;	/* 93 */

    err_DFSToLocal[DFS_EPROCLIM]	= EPROCLIM;	/* 1002 */
/* 
 * disk quotas and limits
 */
    err_DFSToLocal[DFS_EUSERS]		= EUSERS;	/* 94 */
    err_DFSToLocal[DFS_EDQUOT]		= EDQUOT;	/* 1133 */
/*
 *  NFS errors.
 */
    err_DFSToLocal[DFS_ESTALE]		= ESTALE;	/* 151 */
    err_DFSToLocal[DFS_EREMOTE]		= EREMOTE;	/* 66 */

/*
 * misc ..
 */
    err_DFSToLocal[DFS_ENOLCK]		= ENOLCK;	/* 46 */
    err_DFSToLocal[DFS_ENOSYS]		= ENOSYS;	/* 89 */
    err_DFSToLocal[DFS_EAGAIN]		= EAGAIN;	/* 11 */
/* 
 * SYS V IPC errors 
 */
    err_DFSToLocal[DFS_ENOMSG]		= ENOMSG;	/* 35 */
    err_DFSToLocal[DFS_EIDRM]		= EIDRM;	/* 36 */

/*
 * Streams
 */
    err_DFSToLocal[DFS_ENOSR]		= ENOSR;	/* 63 */
    err_DFSToLocal[DFS_ETIME]		= ETIME;	/* 62 */
    err_DFSToLocal[DFS_EBADMSG]		= EBADMSG;	/* 77 */
    err_DFSToLocal[DFS_EPROTO]		= EPROTO;	/* 71 */
    err_DFSToLocal[DFS_ENODATA]		= ENODATA;	/* 61 */
    err_DFSToLocal[DFS_ENOSTR]		= ENOSTR;	/* 60 */

/* Loader errors
 *
 */
    err_DFSToLocal[DFS_ENOPKG]		= ENOPKG;	/* 65 */

/* 
 * security
 */
    err_DFSToLocal[DFS_ENOATTR]		= ENOATTR;	/* 1009 */

}

