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

#ifndef __SESMGR_SAMP_H__
#define __SESMGR_SAMP_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 1.2 $"

struct ifnet;
struct ifreq;
struct inpcb;
struct ip_attrs;
struct mac_label;
struct mbuf;
struct msghdr;
struct recvluinfo;
struct recvlumsga;
struct socket;

#include <sys/types.h>
#include <netinet/in.h>
#include <sys/sema.h>
#include <sys/acl.h>
#include <sys/mac.h>
#include <sys/capability.h>
#include <sys/t6attrs.h>
#include <sys/t6samp.h>
#include <sys/hashing.h>

typedef struct token_entry {
	struct hashbucket tk_hash;
	struct in_addr tk_addr;
	sv_t tk_wait;
	int tk_token;
	int tk_gen;
	unsigned int tk_mid;		/* mapping id */
	int tk_attr_id;
	int tk_attr_len;
	caddr_t tk_attr;
	unsigned int tk_refcnt;
	lock_t tk_lock;			/* protect object contents */
} token_entry_t;

typedef struct _host_entry {
	struct hashbucket he_hash;
	struct in_addr he_host;		/* inet address of host */
	u_char he_gen[3];		/* generation of this hosts token
					   server */
	unsigned int he_refcnt;
	lock_t he_lock;			/* protect object contents */
} host_entry_t;

/*
 *  There is a hash table for the tokens
 *  for each supported attribute.  The hash
 *  function depends on the hash table size
 *  being a power of 2.
 */
#define SAMP_TOKEN_TABLE_SIZE	256
#define SAMP_TOKEN_NTABLES	6
#define SAMP_HOST_TABLE_SIZE	256

/*
 *  Library Interfaces.
 */
#ifndef _KERNEL
extern int recvumsg(int, struct msghdr *, int, uid_t *);
#endif

/*
 *  Session manager system call and kernel structures.
 */
#ifdef _KERNEL
struct _get_attr_reply;
struct _get_lrtok_reply;
void sesmgr_samp_init(void);
void samp_update_attr(struct _get_attr_reply *);
void samp_update_token(struct _get_lrtok_reply *);
int  samp_update_host (u_int, u_int);

struct mbuf *samp_create_header(struct socket *, struct in_addr, int, int);
int samp_get_attrs(struct socket *, struct in_addr, t6samp_header_t *);
int sesmgr_samp_check(t6samp_header_t *);
struct mbuf *samp_create_priv_hdr(int ulen);

#endif  /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif	/* __SESMGR_SAMP_H__ */
