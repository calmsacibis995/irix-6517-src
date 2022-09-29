/*
* The following lines add source identification into this
* file which can be displayed with the RCS command "ident file".
*
*/
#if defined(SGIMIPS) && !defined(KERNEL)
static const char *_identString = "$Id: osi_os2dfserr.c,v 65.3 1998/03/23 16:26:32 gwehrman Exp $";
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
 * $Log: osi_os2dfserr.c,v $
 * Revision 65.3  1998/03/23 16:26:32  gwehrman
 * Source Identification changed.
 *  	- to eliminate compiler warnings
 *  	- to bracket with SGIMIPS && !KERNEL
 *
 * Revision 65.2  1997/11/06 19:58:22  jdoak
 * Source Identification added.
 *
 * Revision 65.1  1997/10/24  14:29:51  jdoak
 * *** empty log message ***
 *
 * Revision 1.1  1997/10/21  15:59:44  jdoak
 * Initial revision
 *
 * Revision 64.1  1997/02/14  19:46:51  dce
 * *** empty log message ***
 *
 * Revision 1.2  1996/04/06  00:17:46  bhim
 * No Message Supplied
 *
 * Revision 1.1.908.3  1994/07/13  22:05:30  devsrc
 * 	merged with bl-10 
 * 	[1994/06/28  17:34:34  devsrc]
 *
 * 	Changed #include with double quotes to #include with angle brackets.
 * 	[1994/04/28  16:00:21  mbs]
 *
 * Revision 1.1.908.2  1994/06/09  14:15:12  annie
 * 	fixed copyright in src/file
 * 	[1994/06/09  13:27:43  annie]
 * 
 * Revision 1.1.908.1  1994/02/04  20:24:12  devsrc
 * 	Merged from 1.0.3a to 1.1
 * 	[1994/02/04  15:15:30  devsrc]
 * 
 * Revision 1.1.906.1  1993/12/07  17:29:36  jaffe
 * 	1.0.3a update from Transarc
 * 	[1993/12/03  15:59:20  jaffe]
 * 
 * Revision 1.1.2.4  1993/01/21  14:50:05  cjd
 * 	embedded copyright notice
 * 	[1993/01/20  14:51:55  cjd]
 * 
 * Revision 1.1.2.3  1992/09/25  18:32:07  jaffe
 * 	Remove duplicate HEADER and LOG entries
 * 	[1992/09/25  12:27:36  jaffe]
 * 
 * Revision 1.1.2.2  1992/08/31  20:23:29  jaffe
 * 	Transarc delta: jaffe-ot4719-cleanup-gcc-Wall-in-osi 1.3
 * 	  Selected comments:
 * 	    Fixed many compiler warnings in the osi directory.
 * 	    Reworked ALL of the header files.  All files in the osi directory now
 * 	    have NO machine specific ifdefs.  All machine specific code is in the
 * 	    machine specific subdirectories.  To make this work, additional flags
 * 	    were added to the afs/param.h file so that we can tell if a particular
 * 	    platform has any additional changes for a given osi header file.
 * 	    make osi_initEncodeTable return void.
 * 	    Corrected errors that appeared while trying to build everything on AIX3.2
 * 	    cleanup for OSF1 compilation
 * 	[1992/08/30  03:12:08  jaffe]
 * 
 * Revision 1.1.1.2  1992/08/30  03:12:08  jaffe
 * 	Transarc delta: jaffe-ot4719-cleanup-gcc-Wall-in-osi 1.3
 * 	  Selected comments:
 * 	    Fixed many compiler warnings in the osi directory.
 * 	    Reworked ALL of the header files.  All files in the osi directory now
 * 	    have NO machine specific ifdefs.  All machine specific code is in the
 * 	    machine specific subdirectories.  To make this work, additional flags
 * 	    were added to the afs/param.h file so that we can tell if a particular
 * 	    platform has any additional changes for a given osi header file.
 * 	    make osi_initEncodeTable return void.
 * 	    Corrected errors that appeared while trying to build everything on AIX3.2
 * 	    cleanup for OSF1 compilation
 * 
 * 	$TALog: osi_os2dfserr.c,v $
 * 	Revision 1.2  1993/10/29  18:01:06  bwl
 * 	Add missing error code encodings.
 * 
 * 	Add missing encodings.
 * 	[from r1.1 by delta bwl-o-ot9293-hp-errno-mapping, r1.1]
 * 
 * Revision 1.1  1993/07/30  18:17:24  bwl
 * 	Port DFS 1.0.3 to HP/UX, adapting HP's changes (which were made to the
 * 	1.0.2a code base) to our own code base.
 * 
 * 	Moved to HPUX from HP800 subdirectory, while adapting changes from HP
 * 	(and renaming, hpux => os).
 * 	[added by delta bwl-o-db3961-port-103-to-HP, r1.1]
 * 
 * Revision 1.3  1993/01/29  15:00:28  jaffe
 * 	Pick up files from the OSF up to the 2.4 submission.
 * 	[from r1.2 by delta osf-revdrop-01-27-93, r1.1]
 * 
 * 	Revision 1.2  1992/07/29  21:35:45  jaffe
 * 	Fixed many compiler warnings in the osi directory.
 * 	Reworked ALL of the header files.  All files in the osi directory now
 * 	have NO machine specific ifdefs.  All machine specific code is in the
 * 	machine specific subdirectories.  To make this work, additional flags
 * 	were added to the afs/param.h file so that we can tell if a particular
 * 	platform has any additional changes for a given osi header file.
 * 
 * 	make osi_initEncodeTable return void.
 * 	[from revision 1.1 by delta jaffe-ot4719-cleanup-gcc-Wall-in-osi, revision 1.1]
 * 
 * Revision 1.1  1992/06/06  23:38:45  mason
 * 	Really a bit misnamed - this delta adds code necessary for HP-UX; it isn't
 * 	sufficient for a complete port.
 * 
 * 	HP-UX error conversion routines.
 * 	[added by delta mason-add-hp800-osi-routines, revision 1.1]
 * 
 * $EndLog$
 */

/*
 *      Copyright (C) 1992 Transarc Corporation
 *      All rights reserved.
 */


#include <osi_dfserrors.h>
#include <sys/errno.h>

unsigned char err_localToDFS[MAXKERNELERRORS+1];

/*
 * The routine is used to initialize the Error Mapping Table used by the DFS 
 * server to map a native (SGIMIPS) KERNEL error to a DFS kernel-independent
 * error. This routine is called during the DFS server initialization process. 
 *
 * Note that those kernel errors that are either very SGIMIPS specific or
 * should never occur to (or be visible to ) remote callers will be treated
 * as DFS_ENOTDEFINED during the conversion and will be ignored by the CM.
 */

void
osi_initEncodeTable()
{
    int i;

    for (i= 0; i <= MAXKERNELERRORS; i++)
	err_localToDFS[i] = DFS_ENOTDEFINED; /* not defined */

    err_localToDFS[0] 			= 0;
    err_localToDFS[EPERM]		= DFS_EPERM;
    err_localToDFS[ENOENT]		= DFS_ENOENT;
    err_localToDFS[ESRCH]		= DFS_ESRCH;
    err_localToDFS[EINTR]		= DFS_EINTR;
    err_localToDFS[EIO]			= DFS_EIO;
    err_localToDFS[ENXIO]		= DFS_ENXIO;
    err_localToDFS[E2BIG]		= DFS_E2BIG;
    err_localToDFS[ENOEXEC]		= DFS_ENOEXEC;
    err_localToDFS[EBADF]		= DFS_EBADF;
    err_localToDFS[ECHILD]		= DFS_ECHILD;
    err_localToDFS[EAGAIN]		= DFS_EAGAIN;
    err_localToDFS[ENOMEM]		= DFS_ENOMEM;
    err_localToDFS[EACCES]		= DFS_EACCES;
    err_localToDFS[EFAULT]		= DFS_EFAULT;
    err_localToDFS[ENOTBLK]		= DFS_ENOTBLK;
    err_localToDFS[EBUSY]		= DFS_EBUSY;
    err_localToDFS[EEXIST]		= DFS_EEXIST;
    err_localToDFS[EXDEV]		= DFS_EXDEV;
    err_localToDFS[ENODEV]		= DFS_ENODEV;
    err_localToDFS[ENOTDIR]		= DFS_ENOTDIR;
    err_localToDFS[EISDIR]		= DFS_EISDIR;
    err_localToDFS[EINVAL]		= DFS_EINVAL;
    err_localToDFS[ENFILE]		= DFS_ENFILE;
    err_localToDFS[EMFILE]		= DFS_EMFILE;
    err_localToDFS[ENOTTY]		= DFS_ENOTTY;
    err_localToDFS[ETXTBSY]		= DFS_ETXTBSY;
    err_localToDFS[EFBIG]		= DFS_EFBIG;
    err_localToDFS[ENOSPC]		= DFS_ENOSPC;
    err_localToDFS[ESPIPE]		= DFS_ESPIPE;
    err_localToDFS[EROFS]		= DFS_EROFS;
    err_localToDFS[EMLINK]		= DFS_EMLINK;
    err_localToDFS[EPIPE]		= DFS_EPIPE;
    err_localToDFS[EDOM]		= DFS_EDOM;
    err_localToDFS[ERANGE]		= DFS_ERANGE;
    err_localToDFS[ENOMSG]		= DFS_ENOMSG;	/* 80 */
    err_localToDFS[EIDRM]		= DFS_EIDRM;	/* 81 */
    /* 37-44 */
    err_localToDFS[EDEADLK]		= DFS_EDEADLK;	/* 11 */
    err_localToDFS[ENOLCK]		= DFS_ENOLCK;	/* 77 */
    /* 47-59 */
    err_localToDFS[ENOSTR]              = DFS_ENOSTR;	/* 87 */
    err_localToDFS[ENODATA]             = DFS_ENODATA;	/* 86 */
    err_localToDFS[ETIME]               = DFS_ETIME;	/* 83 */
    err_localToDFS[ENOSR]               = DFS_ENOSR;	/* 82 */
    /* 64 */
    err_localToDFS[ENOPKG]              = DFS_ENOPKG;	/* 92 */
    err_localToDFS[EREMOTE]		= DFS_EREMOTE;	/* 71 */
    /* 67-70 */
    err_localToDFS[EPROTO]              = DFS_EPROTO;	/* 85 */
    /* 72-76 */
    err_localToDFS[EBADMSG]             = DFS_EBADMSG;	/* 84 */
    err_localToDFS[ENAMETOOLONG]	= DFS_ENAMETOOLONG; /* 63 */
    /* 79-88 */
    err_localToDFS[ENOSYS]		= DFS_ENOSYS;	/* 78 */
    err_localToDFS[ELOOP]		= DFS_ELOOP;	/* 62 */
    /* 91-92 */
    err_localToDFS[ENOTEMPTY]		= DFS_ENOTEMPTY;/* 66 */
    err_localToDFS[EUSERS]		= DFS_EUSERS;	/* 68 */
    err_localToDFS[ENOTSOCK]		= DFS_ENOTSOCK; /* 38 */
    err_localToDFS[EDESTADDRREQ]	= DFS_EDESTADDRREQ; /* 39 */
    err_localToDFS[EMSGSIZE]		= DFS_EMSGSIZE;	/* 40 */
    err_localToDFS[EPROTOTYPE]		= DFS_EPROTOTYPE; /* 41 */
    err_localToDFS[ENOPROTOOPT]		= DFS_ENOPROTOOPT;	/* 42 */
    /* 100-119 */
    err_localToDFS[EPROTONOSUPPORT]	= DFS_EPROTONOSUPPORT;	/* 43 */
    err_localToDFS[ESOCKTNOSUPPORT]	= DFS_ESOCKTNOSUPPORT; /* 44 */
    err_localToDFS[EOPNOTSUPP]		= DFS_EOPNOTSUPP;	/* 45 */
    err_localToDFS[EPFNOSUPPORT]	= DFS_EPFNOSUPPORT;	/* 46 */
    err_localToDFS[EAFNOSUPPORT]	= DFS_EAFNOSUPPORT;	/* 47 */
    err_localToDFS[EADDRINUSE]		= DFS_EADDRINUSE;	/* 48 */
    err_localToDFS[EADDRNOTAVAIL]	= DFS_EADDRNOTAVAIL;	/* 49 */
    err_localToDFS[ENETDOWN]		= DFS_ENETDOWN;		/* 50 */
    err_localToDFS[ENETUNREACH]		= DFS_ENETUNREACH;	/* 51 */
    err_localToDFS[ENETRESET]		= DFS_ENETRESET;	/* 52 */
    err_localToDFS[ECONNABORTED]	= DFS_ECONNABORTED;	/* 53 */
    err_localToDFS[ECONNRESET]		= DFS_ECONNRESET;	/* 54 */
    err_localToDFS[ENOBUFS]		= DFS_ENOBUFS;	/* 55 */
    err_localToDFS[EISCONN]		= DFS_EISCONN;	/* 56 */
    err_localToDFS[ENOTCONN]		= DFS_ENOTCONN;	/* 57 */
    /* 135-142 */
    err_localToDFS[ESHUTDOWN]		= DFS_ESHUTDOWN;	/* 58 */
    err_localToDFS[ETOOMANYREFS]	= DFS_ETOOMANYREFS;	/* 59 */
    err_localToDFS[ETIMEDOUT]		= DFS_ETIMEDOUT;	/* 60 */
    err_localToDFS[ECONNREFUSED]	= DFS_ECONNREFUSED;	/* 61 */
    err_localToDFS[EHOSTDOWN]		= DFS_EHOSTDOWN;	/* 64 */
    err_localToDFS[EHOSTUNREACH]	= DFS_EHOSTUNREACH;	/* 65 */
    err_localToDFS[EALREADY]		= DFS_EALREADY;	/* 37 */
    err_localToDFS[EINPROGRESS]		= DFS_EINPROGRESS;	/* 36 */
    err_localToDFS[ESTALE]		= DFS_ESTALE;	/* 70 */
}

/* This is an SGIMIPS-specific Encode function */
unsigned int
osi_errEncode(unsigned int code)
{
	switch(code) {
		case EPROCLIM: return DFS_EPROCLIM;
		case ENOATTR: return DFS_ENOATTR;
		case EDQUOT: return DFS_EDQUOT;
		default: {
			   if(code > MAXKERNELERRORS)
				return code;
			   else 
                                return (unsigned int) err_DFSToLocal[code];
			}
	}
}
