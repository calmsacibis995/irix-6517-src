/*
 * @OSF_COPYRIGHT@
 * COPYRIGHT NOTICE
 * Copyright (c) 1990, 1991, 1992, 1993, 1994 Open Software Foundation, Inc.
 * ALL RIGHTS RESERVED (DCE).  See the file named COPYRIGHT.DCE for
 * the full copyright text.
 */
/*
 * HISTORY
 * $Log: osi_dfserrors.h,v $
 * Revision 65.5  1999/07/21 19:01:14  gwehrman
 * Defined SGIMIPS for this development header.  Fix for bug 657352.
 *
 * Revision 65.4  1998/07/02 19:13:04  lmc
 * We are receiving an error code of 1311 from the Cray when
 * running a rename test.  This puts us into a loop continually
 * retrying because we map it to an unknown error that is a
 * dfs error.  Upping the maximum makes us treat it a kernel
 * error and we don't retry it.  I picked 2048 because
 * its an even number and it had to be at least 1600.
 *
 * Revision 65.3  1998/03/19  23:47:26  bdr
 * Misc 64 bit file changes and/or code cleanup for compiler warnings/remarks.
 *
 * Revision 65.2  1997/12/16  17:05:39  lmc
 *  1.2.2 initial merge of file directory
 *
 * Revision 65.1  1997/10/20  19:17:42  jdoak
 * *** empty log message ***
 *
 * Revision 1.1.123.1  1996/10/02  18:10:36  damon
 * 	Newest DFS from Transarc
 * 	[1996/10/01  18:44:55  damon]
 *
 * $EndLog$
 */

/*
 *      Copyright (C) 1996, 1991 Transarc Corporation
 *      All rights reserved.
 */
/* $Header: /proj/irix6.5.7m/isms/irix/kern/fs/dfs/file/osi/RCS/osi_dfserrors.h,v 65.5 1999/07/21 19:01:14 gwehrman Exp $ */

/* 
 * This file contains the symbolic names and values for DFS kernel-independent
 * errors. In addition, it also defines two MACROS that are used to perform 
 * the encoding (by the PX) and decoding (by the CM) respectively.
 *
 */

#ifndef _DFS_ERRORS_
#define _DFS_ERRORS_
#ifndef SGIMIPS
#define SGIMIPS
#endif /* !SGIMIPS */

#include <dcedfs/param.h>
#include <sys/errno.h>

#ifdef SGIMIPS
extern unsigned int err_DFSToLocal[];
extern unsigned char err_localToDFS[];
extern void osi_initDecodeTable(void);
extern void osi_initEncodeTable(void);
#else
extern unsigned char err_DFSToLocal[];
extern unsigned char err_localToDFS[];
extern void osi_initDecodeTable();
extern void osi_initEncodeTable();
#endif /* SGIMIPS */

/*
 * The maximum number of kernel-independent errors defined 
 */
#ifdef SGIMIPS
/*  If the error is within the kernel error code range, no retry is
	performed.  One of the error codes was causing a loop (1311)
	because we continually retried it.  This leaves room for some
	more error codes.  The highest on Irix is about 1600 right now. */
#define MAXKERNELERRORS		2048
#else /* SGIMIPS */
#define MAXKERNELERRORS		256
#endif /* SGIMIPS */

/* Now define a few that DFS uses generically that aren't uniformly defined.
 *
 * CAUTION -- these locally defined code are only to be used on the server
 *     side, where they will eventually get mapped to a local code.  This is
 *     because osi_errDecode(osi_errEncode(X)) != X on all platforms when X is
 *     EDQUOT or EOVERFLOW. */

#ifndef EDQUOT
#define DFS_EDQUOT_MISSING 1		/* map to ENOSPC */
#define EDQUOT (MAXKERNELERRORS-1)
#endif

#ifndef EOVERFLOW
#define DFS_EOVERFLOW_MISSING 1		/* map to EFBIG */
#define EOVERFLOW (MAXKERNELERRORS-2)
#endif

#define DFS_ESUCCESS		0	/* Successful			*/
#define DFS_EPERM		1	/* Operation not permitted	*/
#define DFS_ENOENT		2	/* No such file or directory 	*/
#define DFS_ESRCH		3	/* No such process		*/
#define DFS_EINTR		4	/* Interrupted system call 	*/
#define DFS_EIO			5	/* I/O error 			*/
#define DFS_ENXIO		6	/* No such device or address 	*/
#define DFS_E2BIG		7	/* Arg list too long 		*/
#define DFS_ENOEXEC		8	/* Exec format error 		*/
#define DFS_EBADF		9	/* Bad file number 		*/
#define DFS_ECHILD		10	/* No children 			*/
#define DFS_EDEADLK		11	/* Operation would cause deadlock */
#define DFS_ENOMEM		12	/* Not enough core		 */
#define DFS_EACCES		13	/* Permission denied 		*/
#define DFS_EFAULT		14	/* Bad address 			*/
#define DFS_ENOTBLK		15	/* Block device required 	*/
#define DFS_EBUSY		16	/* Mount device busy 		*/
#define DFS_EEXIST		17	/* File exists 			*/
#define DFS_EXDEV		18	/* Cross-device link 		*/
#define DFS_ENODEV		19	/* No such device 		*/
#define DFS_ENOTDIR		20	/* Not a directory		*/
#define DFS_EISDIR		21	/* Is a directory 		*/
#define DFS_EINVAL		22	/* Invalid argument 		*/
#define DFS_ENFILE		23	/* File table overflow 		*/
#define DFS_EMFILE		24	/* Too many open files 		*/
#define DFS_ENOTTY		25	/* Not a typewriter 		*/
#define DFS_ETXTBSY		26	/* Text file busy 		*/
#define DFS_EFBIG		27	/* File too large 		*/
#define DFS_ENOSPC		28	/* No space left on device 	*/
#define DFS_ESPIPE		29	/* Illegal seek 		*/
#define DFS_EROFS		30	/* Read-only file system 	*/
#define DFS_EMLINK		31	/* Too many links 		*/
#define DFS_EPIPE		32	/* Broken pipe 			*/
#define DFS_EDOM		33	/* Argument too large 		*/
#define DFS_ERANGE		34	/* Result too large 		*/
#define DFS_EWOULDBLOCK		35	/* Operation would block	*/

#define DFS_EINPROGRESS		36	/* Operation now in progress 	*/
#define DFS_EALREADY		37	/* Operation already in progress */
/* 
 * ipc/network software
 */
#define DFS_ENOTSOCK		38	/* Socket operation on non-socket */
#define DFS_EDESTADDRREQ	39	/* Destination address required	*/
									
#define DFS_EMSGSIZE		40	/* Message too long		*/
#define DFS_EPROTOTYPE		41	/* Protocol wrong type for socket */
#define DFS_ENOPROTOOPT		42	/* Protocol not available 	*/
#define DFS_EPROTONOSUPPORT	43	/* Protocol not supported 	*/
#define DFS_ESOCKTNOSUPPORT	44	/* Socket type not supported 	*/
#define DFS_EOPNOTSUPP		45	/* Operation not supported on socket */
#define DFS_EPFNOSUPPORT	46	/* Protocol family not supported */
#define DFS_EAFNOSUPPORT	47	/* Address family not supported	*/
#define DFS_EADDRINUSE		48	/* Address already in use 	*/
#define DFS_EADDRNOTAVAIL	49	/* Can't assign requested address */
#define DFS_ENETDOWN		50	/* Network is down 		*/
#define DFS_ENETUNREACH		51	/* Network is unreachable 	*/
#define DFS_ENETRESET		52	/* Network dropped conn on reset */
#define DFS_ECONNABORTED	53	/* Software caused connection abort */
#define DFS_ECONNRESET		54	/* Connection reset by peer 	*/
#define DFS_ENOBUFS		55	/* No buffer space available 	*/
#define DFS_EISCONN		56	/* Socket is already connected 	*/
#define DFS_ENOTCONN		57	/* Socket is not connected 	*/
#define DFS_ESHUTDOWN		58	/* Can't send after socket shutdown */
#define DFS_ETOOMANYREFS	59	/* Too many references: can't splice */
#define DFS_ETIMEDOUT		60	/* Connection timed out 	*/
#define DFS_ECONNREFUSED	61	/* Connection refused 		*/
#define DFS_ELOOP		62	/* Too many levels of symbolic links */
#define DFS_ENAMETOOLONG	63	/* File name too long 		*/
#define DFS_EHOSTDOWN		64	/* Host is down 		*/
#define DFS_EHOSTUNREACH	65	/* No route to host 		*/
#define DFS_ENOTEMPTY		66	/* Directory not empty 		*/
#define DFS_EPROCLIM		67	/* Too many processes 		*/
#define DFS_EUSERS		68	/* Too many users 		*/
#define DFS_EDQUOT		69	/* Disc quota exceeded 		*/
/*
 *  NFS errors.
 */
#define DFS_ESTALE		70	/* Stale NFS file handle 	*/
#define DFS_EREMOTE		71	/* Too many levels of remote in path */
#define DFS_EBADRPC		72	/* RPC struct is bad 		*/
#define DFS_ERPCMISMATCH	73	/* RPC version wrong 		*/
#define DFS_EPROGUNAVAIL	74	/* RPC prog. not avail 		*/
#define DFS_EPROGMISMATCH	75	/* Program version wrong 	*/
#define DFS_EPROCUNAVAIL	76	/* Bad procedure for program 	*/
/*
 * misc ..
 */
#define DFS_ENOLCK		77	/* No locks available 		*/
#define DFS_ENOSYS		78	/* Function not implemented 	*/
#define DFS_EAGAIN		79	/* Resource temporarily unavailable */
/* 
 * SYS V IPC errors 
 */
#define DFS_ENOMSG		80	/* No msg matches receive request */
#define DFS_EIDRM		81	/* Msg queue id has been removed */
/* 
 * STREAMS 
 */
#define	DFS_ENOSR		82	/* Out of STREAMS resources 	*/
#define	DFS_ETIME		83	/* System call timed out 	*/
#define	DFS_EBADMSG		84	/* Next message has wrong type 	*/
#define DFS_EPROTO		85	/* STREAMS protocol error 	*/
#define DFS_ENODATA		86	/* No message on stream head read q */
#define DFS_ENOSTR		87	/* fd not associated with a stream */
/* 
 * Not visible outside kernel 
 */
#define DFS_ECLONEME		88	/* Tells open to clone the device */
/*
 *  Filesystem 
 */
#define	DFS_EDIRTY		89	/* Mounting a dirty fs w/o force */
/*
 * Loader errors 
 */
#define	DFS_EDUPPKG		90	/* duplicate package name on install */
#define	DFS_EVERSION	 	91	/* version number mismatch 	*/
#define	DFS_ENOPKG		92	/* unresolved package name 	*/
#define	DFS_ENOSYM		93	/* unresolved symbol name 	*/
/*
 * 64/32 bit interoperability
 */
#define DFS_EOVERFLOW		94
/*
 * To be filled 
 */
#define DFS_SPARE95		95
#define DFS_SPARE96		96
#define DFS_SPARE97		97
#define DFS_SPARE98		98
#define DFS_SPARE99		99
#define DFS_SPARE100		100
#define DFS_SPARE101		101
#define DFS_SPARE102		102
#define DFS_SPARE103		103
#define DFS_SPARE104		104
#define DFS_SPARE105		105
#define DFS_SPARE106		106
#define DFS_SPARE107		107
#define DFS_SPARE108		108
#define DFS_SPARE109		109
#define DFS_SPARE110		110
#define DFS_SPARE111		111
/* 
 * security 
 */
#define DFS_ENOATTR		112	/* no attribute found 		*/
#define DFS_ESAD		113	/* security authentication denied */
#define DFS_ENOTRUST		114	/* not a trusted program 	*/
/*
 * To be filled 
 */
#define DFS_SPARE115		115
#define DFS_SPARE116		116
#define DFS_SPARE117		117
#define DFS_SPARE118		118
#define DFS_SPARE119		119
#define DFS_SPARE120		120
#define DFS_SPARE121		121
#define DFS_SPARE122		122
/* 
 * Internal Disk/Block Device error codes 
 */
#define	DFS_ESOFT		123	/* I/O completed but needs relocation*/
#define	DFS_EMEDIA		124	/* media surface error 		*/
#define	DFS_ERELOCATED		125	/* a relocation request performed ok */

#define DFS_SPARE126		126
#define DFS_SPARE127		127

/*
 * Errors not defined by local kernel.
 */
#define DFS_ENOTDEFINED 	128

/* 
 * Function that is used to perform the decoding work by the CM
 */

#define osi_errDecode(code) \
  	(((unsigned) code) > MAXKERNELERRORS ? \
  	(code) : \
  	(unsigned int) err_DFSToLocal[(code)])

/* 
 * Function that is used to perform the encoding work by the FX serverg
 */

#ifndef AFS_IRIX_ENV 
#define osi_errEncode(code) \
  	(((unsigned) code) >  MAXKERNELERRORS ? \
  	(code) : \
  	(unsigned int) err_localToDFS[(code)])
#else
extern unsigned int osi_errEncode(unsigned int);
#endif

#endif /*  _DFS_ERRORS_ */
