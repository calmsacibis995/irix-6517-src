/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __T6API_PRIVATE_H__
#define __T6API_PRIVATE_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 1.4 $"

/*
 *  Internal definitions for the TSIX API.  This file should be used
 *  only by the library routines implementing the API, and the
 *  system call code.
 */

#include <limits.h>            /* For NGROUPS_MAX */
#include <sys/uio.h>		/* For struct iovec */
#include <sys/types.h>
#include <sys/t6attrs.h>
#include <sys/capability.h>

struct mac_label;
struct soacl;
struct socket;
struct mbuf;
struct ifnet;
struct t6parms;

#define T6_MAX_ATTRS	16
#define T6MAX_TEXT_BUF	256

/*
 *  Library interface to the sesmgr system call.
 */
#ifndef _KERNEL
#include <stddef.h>
extern ptrdiff_t sgi_sesmgr(int, ...);
#endif /* !_KERNEL */

#ifdef _KERNEL

/*
 *  kernel side of the sesmgr system call.
 */
struct syssesmgra {
        sysarg_t cmd;
        sysarg_t arg1, arg2, arg3, arg4, arg5, arg6, arg7;
};

extern int sgi_sesmgr(struct syssesmgra *, rval_t *);
int sesmgr_t6_syscall (struct syssesmgra *, rval_t *);
int sesmgr_satmp_syscall (struct syssesmgra *, rval_t *);

#ifndef __SATMP_ESI_T_DEFINED__
#define __SATMP_ESI_T_DEFINED__
typedef __uint64_t satmp_esi_t;
#endif /* !__SATMP_ESI_T_DEFINED__ */

/*
 *  Token mapping daemon request structure.
 */
typedef struct _init_request_queue {
	sv_t  irq_wait;
	lock_t irq_lock;
	u_int irq_hostid;
	u_int irq_generation;
	satmp_esi_t irq_seqno;
	u_int irq_refcnt;
	int   irq_flag;
	struct _init_request_queue *irq_next;
} init_request_queue;

#include <sys/kuio.h>
typedef struct irix5_t6ctl {
	t6mask_t		t6_alloc_mask;
	t6mask_t		t6_valid_mask;
	struct irix5_iovec	t6_iov[T6_MAX_ATTRS];
} irix5_t6ctl_t;

typedef struct irix5_t6parms {
	app32_ptr_t attrp;
	app32_ptr_t maskp;
} irix5_t6parms_t;

extern int sesmgr_enabled;
extern void sesmgr_t6_init(void);
extern int sesmgr_t6ext_attr(int, t6cmd_t, rval_t *);
extern int sesmgr_t6new_attr(int, t6cmd_t, rval_t *);
extern int sesmgr_t6get_endpt_mask(int, t6mask_t *, rval_t *);
extern int sesmgr_t6set_endpt_mask(int, t6mask_t, rval_t *);
extern int sesmgr_t6get_endpt_default(int, t6mask_t *, t6attr_t, rval_t *); 
extern int sesmgr_t6set_endpt_default(int, t6mask_t, t6attr_t, rval_t *);
extern int sesmgr_t6peek_attr(int, t6attr_t, t6mask_t *, rval_t *);
extern int sesmgr_t6last_attr(int, t6attr_t, t6mask_t *, rval_t *);
extern int sesmgr_t6sendto(int, char *, int, int,
			struct sockaddr *, int,
			t6attr_t, rval_t *);
extern int sesmgr_t6recvfrom(int, char *, int, int,
			struct sockaddr *, int *,
			struct t6parms *, rval_t *);

/* Define macros choosing stub functions or real functions here */
#define _SESMGR_T6_NEW_ATTR(e)  ((sesmgr_enabled && SESMGR_TSIX_ID)? \
					sesmgr_t6_new_attr(): ENOSYS)

#endif

#ifdef __cplusplus
}
#endif

/*
 * sesmgr() system call commands.
 */

/* TSIX API */
#define T6EXT_INVALID_CMD		0
#define T6EXT_ATTR_CMD			1
#define T6NEW_ATTR_CMD			2
#define T6GET_ENDPT_MASK_CMD		3
#define T6SET_ENDPT_MASK_CMD		4
#define T6GET_ENDPT_DEFAULT_CMD		5
#define T6SET_ENDPT_DEFAULT_CMD		6
#define T6PEEK_ATTR_CMD			7
#define T6LAST_ATTR_CMD			8
#define T6SENDTO_CMD			9
#define T6RECVFROM_CMD			10
#define T6MLS_SOCKET_CMD		11

#define T6RHDB_PUT_HOST_CMD		20
#define T6RHDB_GET_HOST_CMD		21
#define T6RHDB_STAT_CMD			22
#define T6RHDB_FLUSH_CMD		23

#define T6SATMP_INIT_CMD		30
#define T6SATMP_DONE_CMD		31
#define T6SATMP_GET_ATTR_REPLY_CMD	32
#define T6SATMP_GET_LRTOK_REPLY_CMD	33
#define T6SATMP_INIT_REPLY_CMD		34

typedef struct t6ctl {
	t6mask_t		t6_alloc_mask;
	t6mask_t		t6_valid_mask;
	struct iovec		t6_iov[T6_MAX_ATTRS];
} t6ctl_t;

/*
 *  This structure is used to pass an attributes between the library
 *  interfaces and the kernel.  It consists of a mask to indicate
 *  which attributes are present, followed by the small fixed length
 *  attributes.  the remaining variable length attributes are tagged
 *  on the end.  No space is allocated if there are not beiing passed.
 */

typedef struct t6attr_buf {
	t6mask_t	t6_mask;
	uid_t           t6_sid;             /* Session ID */
	uid_t           t6_uid;
	uid_t           t6_luid;            /* Login or Audit ID */
	gid_t           t6_gid;
	int             t6_grp_cnt;
	gid_t           t6_groups[NGROUPS_MAX];
	cap_set_t       t6_priv;
	/*mac_b_label   t6_msen_lbl; */
	/*mac_b_label   t6_mint_lbl; */
	/*mac_b_label   t6_info_lbl; */
	/*mac_b_label   t6_clearance; */

} t6attr_buf_t;

/* This structure is used for the t6recvfrom system
 * call to work around the limit 8 arguments per
 * system call.
 */
typedef struct t6parms {
	t6ctl_t  *attrp;
	t6mask_t *maskp;
} t6parms_t;

#endif	/* __T6API_PRIVATE_H__ */
