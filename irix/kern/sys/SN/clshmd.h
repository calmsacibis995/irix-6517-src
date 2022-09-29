#ifndef __CLSHM_D_H__
#define __CLSHM_D_H__
/*
 * File: clshmd.h
 *
 *	Header file to be included by clshmd and the clshm driver
 * 	in order to communicate from driver to/from clshmd daemon
 *	(which is the daemon for shared-memory communication between
 *	partitions over CrayLink)
 *
 * Copyright 1998, Silicon Graphics, Inc.
 * ALL RIGHTS RESERVED
 *
 * UNPUBLISHED -- Rights reserved under the copyright laws of the United
 * States.   Use of a copyright notice is precautionary only and does not
 * imply publication or disclosure.
 *
 * U.S. GOVERNMENT RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions
 * as set forth in FAR 52.227.19(c)(2) or subparagraph (c)(1)(ii) of the Rights
 * in Technical Data and Computer Software clause at DFARS 252.227-7013 and/or
 * in similar or successor clauses in the FAR, or the DOD or NASA FAR
 * Supplement.  Contractor/manufacturer is Silicon Graphics, Inc.,
 * 2011 N. Shoreline Blvd. Mountain View, CA 94039-7311.
 *
 * THE CONTENT OF THIS WORK CONTAINS CONFIDENTIAL AND PROPRIETARY
 * INFORMATION OF SILICON GRAPHICS, INC. ANY DUPLICATION, MODIFICATION,
 * DISTRIBUTION, OR DISCLOSURE IN ANY FORM, IN WHOLE, OR IN PART, IS STRICTLY
 * PROHIBITED WITHOUT THE PRIOR EXPRESS WRITTEN PERMISSION OF SILICON
 * GRAPHICS, INC.
 */



#if defined(_LANGUAGE_C) || defined(_LANGUAGE_C_PLUS_PLUS)

/* version defines */
#define CLSHM_DAEM_MAJOR_VERS	0x01
#define CLSHM_DAEM_MINOR_VERS	0x01

/* general defines */
#define	MAX_MSG_LEN		256	/* in # bytes */
#define	MAX_OUTSTAND_MSGS	128	/* across all partitions */
#define CLSHMD_SOCKET_NAME	"/tmp/.clshmd_socket"
#define CLSHMD_SOCKET_MAX	32

/* types sent in the body of msg */
typedef __uint64_t	len_in_msg;
typedef __uint64_t	off_in_msg;
typedef	__uint64_t	kpaddr_in_msg;

typedef	__uint32_t	pid_in_msg;
typedef __uint32_t	key_in_msg;
typedef __uint32_t	shmid_in_msg;
typedef __uint32_t	msgnum_in_msg;
typedef __uint32_t	err_in_msg;
typedef __uint32_t 	vers_in_msg;
typedef __uint32_t	pgsz_in_msg;
typedef __uint32_t	pmoh_in_msg;
typedef __uint32_t	pmot_in_msg;


/* define the message types for ctl messages: all the types that the 
 * daemon is allowed to send 
 */

typedef	char	clshmd_msg_type_t;


/* enumerated message types */
#define	DAEM_TO_DAEM_HDR		'0'	/* "hdr" for real msg */
#define USR_TO_DAEM_HDR			'0'	
#define DAEM_TO_USR_HDR			'0'


/* passed: NOTHING */
#define	DRV_TO_DAEM_PART_DEAD		'a'	/* backing part gone */
#define	DAEM_TO_DAEM_PART_DEAD		'b'
#define	DAEM_TO_DRV_PART_DEAD		'c'
#define DAEM_TO_USR_PART_DEAD		'd'

#define	DRV_TO_DAEM_CLSHM_DIE_NOW	'e'	/* tell daem to exit */

/* passed: NOTHING */
#define	DRV_TO_DAEM_NEW_PART 		'f'	/* part coming up */
/* passed: version major #, version minor #, page_size */
#define	DAEM_TO_DAEM_NEW_PART 		'g'	/* part coming up */
/* passed: NOTHING, or version major #, verstion major #, page_size if 
 * partition is our own partition */
#define	DAEM_TO_DRV_NEW_PART 		'h'	/* part coming up */

/* passed: user version major, version minor */
#define USR_TO_DAEM_NEW_USER		'i'	/* get daemon version */
/* passed: daemon version major, version minor, page_size */
#define DAEM_TO_USR_NEW_USER		'j'



/* specific shmem interface messages */

/* passed: NOTHING */
#define	USR_TO_DAEM_NEED_HOSTNAME	'n'	/* need host/domain */
/* passed: NOTHING */
#define	DAEM_TO_DAEM_NEED_HOSTNAME	'o'	/* propagetes */
/* passed: hostname -- 0 length == error */
#define	DAEM_TO_DAEM_TAKE_HOSTNAME	'q'	/* propagetes back */
/* passed: hostname -- 0 length == error */
#define	DAEM_TO_USR_TAKE_HOSTNAME	'r'	/* to user */


/* KEY defines */
#define	KEY_TO_PARTITION(k)	(((k) & 0xff000000) >> 24)
#define CLSHMD_ABSENT_KEY	0xffffffff

/* passed: pid */ 
#define	USR_TO_DAEM_NEED_KEY		's'	/* need key */
/* passed: pid */
#define	DAEM_TO_DAEM_NEED_KEY		't'	/* propagetes */
/* passed: pid, key */
#define	DAEM_TO_DAEM_TAKE_KEY		'u'	/* propagetes back */
/* passed: pid, key, errno */
#define	DAEM_TO_USR_TAKE_KEY		'v'	/* to user */


/* passed: key, pid, pmo_handle, pmo_type */
#define USR_TO_DAEM_TAKE_PMO		'w'	/* set pmo info */
/* passed: errno */
#define DAEM_TO_USR_TAKE_PMO		'x'	/* valid or not */



/* SHMID defines */
#define	SHMID_TO_PARTITION(s)		(((s) & 0xff000000) >> 24)
#define	SHMID_TO_OFFSET(s)		((s) & 0x00ffffff)
#define MAKE_SHMID(part, off)		(((part) << 24) | (off))
#define CLSHMD_ABSENT_SHMID		0xffffffff
#define CLSHMD_ABSENT_PMO_PID		0
#define CLSHMD_ABSENT_PMO_HANDLE	-1
#define CLSHMD_ABSENT_PMO_TYPE		((pmo_type_t) -1)



/* passed: key, len, len == -1 if coming from shmat */
#define USR_TO_DAEM_NEED_SHMID		'y'	/* need shmid */
/* passed: key, len, len == -1 if coming from shmat */
#define DAEM_TO_DAEM_NEED_SHMID		'z'	/* propagates */
/* passed: key, len, shmid, pid, pmo_handle, pmo_type */
#define DAEM_TO_DRV_NEED_SHMID		'A'

/* passed: key, len, shmid, msg # (0 unless it is the second message needed
   to send kaddrs), kaddrs (# determined by size) */
/* passed: key, len, shmid, errno, in error case and and shmid is
   CLHSMD_ABSEND_SHMID */
#define DAEM_TO_DRV_TAKE_SHMID		'B'	/* take shmid */
/* passed: same as DAEM_TO_DRV_TAKE_SHMID */
#define DRV_TO_DAEM_TAKE_SHMID		'C'
/* passed: same as DAEM_TO_DRV_TAKE_SHMID */
#define DAEM_TO_DAEM_TAKE_SHMID		'D'	/* propagates back */
/* passed: key, len, shmid, errno */
#define DAEM_TO_USR_TAKE_SHMID		'E'	/* to user */


/* passed: shmid */
#define USR_TO_DAEM_NEED_PATH		'F'	/* ask for nodepath */
/* passed: shmid */
#define DAEM_TO_DRV_NEED_PATH		'G'	/* propagates */
/* passed: shmid, path -- 0 length if not avail, shmid == ABSENT if error */
#define DRV_TO_DAEM_TAKE_PATH		'H'	/* propagates back */
/* passed; shmid, path -- 0 length if not avail, shmid == ABSENT if error */
#define DAEM_TO_USR_TAKE_PATH		'I'	/* to user */


/* all passed: shmid */
#define USR_TO_DAEM_RMID		'J'	/* remove shmid */
#define DAEM_TO_DAEM_RMID		'K'	/* propagates */
#define DAEM_TO_DRV_RMID		'L'	/* propagates */
#define DRV_TO_DAEM_RMID		'M'	/* bad event cleanup */
/* passed: errno */
#define DAEM_TO_USR_RMID		'N'	/* answer */

/* undefined behavior if have AUTORMID set and try to attach after
 * the last detach -- one of three things will happen:
 * 1) the attach will fail
 * 2) the attach will succeed and things will continue as if the
 * 	last detach was never seen
 * 3) the attach will succeed and then the program will get killed when
 * 	the last detach was noticed and destroys the segment
 */


/* passed: shmid */
#define USR_TO_DAEM_AUTORMID		'O'	/* tell autocleanup */
/* passed: shmid, errno */
#define DAEM_TO_DAEM_AUTORMID		'P'	/* propagates */
/* passed: errno */
#define DAEM_TO_USR_AUTORMID		'Q'	/* answer */

/* passed: shmid */
#define DRV_TO_DAEM_ATTACH		'R'	/* first attach */
#define DAEM_TO_DAEM_ATTACH		'S'	/* first attach */
#define DRV_TO_DAEM_DETACH		'T'	/* last detach */
#define DAEM_TO_DAEM_DETACH		'U'	/* last detach */


/* passed: nothing !!! */
#define USR_TO_DAEM_DUMP_DAEM_STATE		'.'
#define USR_TO_DAEM_DUMP_DRV_STATE		'?'
#define DAEM_TO_DRV_DUMP_DRV_STATE		'!'



#define OHEAD_UINTS     4  /* head, tail, max, align-up */

/* number of kpaddrs that can fit into a single message */
#define ADDRS_PER_MSG	((MAX_MSG_LEN - sizeof(key_in_msg) - 		\
			  sizeof(len_in_msg) - sizeof(shmid_in_msg) - 	\
			  sizeof(msgnum_in_msg)) / sizeof(kpaddr_in_msg))


/* clshm_msg_hdr_t
 * ---------------
 * Header is sent before a message so we can tell how long the msg will
 * be when it comes.
 */
typedef	struct	clshm_msg_hdr_s {
        clshmd_msg_type_t	type;	/* can only be "HDR" type */
        int                     len;    /* # bytes to expect in msg */
	} clshm_msg_hdr_t;


/* clshm_msg_t
 * -----------
 * actually message that is sent between all the partitions from
 * daems and to the driver as well as users of the daemon.
 */
typedef	struct	clshm_msg_s {
        clshmd_msg_type_t	type;		/* one of enum types */
	partid_t		src_part;	/* message is from */
	partid_t		dst_part;	/* message is to */
	char			dummy;		/* to align up */
	__uint32_t		len;		/* # bytes in msg */
        char			body[MAX_MSG_LEN];
	} clshm_msg_t;

#endif /* C || C++ */
#endif /* ! __CLSHMD_H__ */


