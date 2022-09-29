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

#ifndef __SESMGR_H__
#define __SESMGR_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 1.12 $"

#include <sys/types.h>
#include <sys/mac_label.h>
#include <sys/mac.h>

struct bhv_desc;
struct cred;
struct mac_label;
struct soacl;
struct socket;
struct mbuf;
struct ifnet;
struct uio;
struct sockaddr_in;
struct ipsec;
struct ip;
struct vnode;
struct t6rhdb_kern_buf;

/*
 *  Session Manager Id's.  These are or-ed into the high end of the
 *  subcommand.
 */

#define SESMGR_INVALID_ID       0x0
#define SESMGR_IRIX_ID          0x1
#define SESMGR_TSIX_ID          0x2
#define SESMGR_CYLINK_ID        0x4
#define SESMGR_SATMP_ID         0x8

#define SESMGR_MAKE_CMD(a,b)    ((a & 0xffff)<<16 | (b & 0xffff))

#ifdef _KERNEL
/*
 *  Global flags and housekeeping.
 */
extern int sesmgr_enabled;

/*
 * Interposition function replacing soreceive
 */
int
sesmgr_soreceive(struct bhv_desc *, struct mbuf **, struct uio *,
		 int *, struct mbuf **);

/*
 * Session Manager Initialization
 */
void sesmgr_init(void);
void sesmgr_confignote(void);

#define _SESMGR_CONFIGNOTE()	   ((sesmgr_enabled)? \
					sesmgr_confignote(): (void)0 )

/*
 *  Routines for Security Options and Fields.
 */
extern struct mac_label * ip_sec_get_label( struct ipsec * );
extern msen_t ip_sec_get_msen( struct ipsec * );
extern mint_t ip_sec_get_mint( struct ipsec * );
extern uid_t ip_sec_get_uid_t( struct ipsec * );
extern int ip_sec_add_cache( u_char *, struct ipsec *, struct t6rhdb_kern_buf *);
extern struct mbuf * ip_sec_get_xmt( struct ipsec *, struct t6rhdb_kern_buf *);
extern int ip_sec_get_rcv( u_char *, struct ipsec * );
extern void ip_sec_init( void );

/*
 *  Routines for Tracing Security Options.
 */
int trace_sec_attr(struct ip *, struct ipsec *);
int trace_ip_options(struct mbuf *);


/*
 *  Routines for IP Security Options.
 */
int sesmgr_ip_options(struct mbuf *, struct ipsec **);
int sesmgr_add_options(struct ifnet *, struct mbuf *, struct ipsec *,
		       struct sockaddr_in *, int *);
int ip_sec_dooptions( struct mbuf *, struct ipsec *, struct t6rhdb_kern_buf *);
struct mbuf *ip_sec_insertopts( struct mbuf *, struct ipsec *, 
			struct t6rhdb_kern_buf *, int * );

#define _SESMGR_IP_OPTIONS(a,b)  ((sesmgr_enabled)? \
					sesmgr_ip_options(a,b): 0)
#define _SESMGR_ADD_OPTIONS(a,b,c,d,e)   ((sesmgr_enabled)? \
					sesmgr_add_options(a,b,c,d,e): 0)

/*
 *  Routines for Socket Security Attributes.
 */

struct ipsec *sesmgr_soattr_alloc (int);
void sesmgr_soattr_free (struct ipsec *);
int sesmgr_socket_copy_attrs(struct socket *, struct socket *, struct ipsec *);
void sesmgr_socket_init_attrs(struct socket *, struct cred *);
void sesmgr_socket_free_attrs(struct socket *);

#define _SESMGR_SOATTR_ALLOC		   ((sesmgr_enabled)? \
					sesmgr_soattr_alloc(M_DONTWAIT): NULL)
#define _SESMGR_SOATTR_FREE(a)		   ((sesmgr_enabled)? \
					sesmgr_soattr_free(a): (void)0)
#define _SESMGR_SOCKET_COPY_ATTRS(a,b,c)   ((sesmgr_enabled)? \
					sesmgr_socket_copy_attrs(a,b,c): 0)
#define _SESMGR_SOCKET_INIT_ATTRS(a,b)	   ((sesmgr_enabled)? \
					sesmgr_socket_init_attrs(a,b): (void)0)
#define _SESMGR_SOCKET_FREE_ATTRS(a)	   ((sesmgr_enabled)? \
					sesmgr_socket_free_attrs(a): (void)0)

/*
 *  SAMP routines
 */

int  sesmgr_samp_accept(struct socket *);
int  sesmgr_samp_nread(struct socket *, int *, int);
int  sesmgr_put_samp(struct socket *, struct mbuf **, struct mbuf *);
int  sesmgr_samp_init_data(struct socket *, struct mbuf *);
void sesmgr_samp_send_data(struct socket *);
void sesmgr_samp_sbcopy(struct socket *, struct socket *);
int  sesmgr_samp_kudp(struct socket *, struct mbuf **, struct mbuf *);

#define _SESMGR_PUT_SAMP(a,b,c)		((sesmgr_enabled) ? \
					 sesmgr_put_samp((a), (b), (c)) : 0)
#define _SESMGR_SAMP_INIT_DATA(so, a)	((sesmgr_enabled) ? \
					 sesmgr_samp_init_data((so), (a)) : 0)
#define _SESMGR_SAMP_SEND_DATA(so)	((sesmgr_enabled) ? \
					 sesmgr_samp_send_data(so) : (void) 0)
#define _SESMGR_SAMP_SBCOPY(so, sb)	((sesmgr_enabled) ? \
					 sesmgr_samp_sbcopy(so, sb) : (void) 0)
#define _SESMGR_SAMP_KUDP(so,m,nam)	((sesmgr_enabled) ? \
					 sesmgr_samp_kudp(so, m, nam) : 0)

/*
 * NFS routines
 */
struct ipsec *sesmgr_nfs_soattr_copy(struct ipsec *);
int sesmgr_nfs_vsetlabel(struct vnode *, struct ipsec *);
struct mbuf *sesmgr_nfs_set_ipsec(struct ipsec *, struct mbuf **);

#define _SESMGR_NFS_SOATTR_COPY(a)	((sesmgr_enabled) ? \
					 sesmgr_nfs_soattr_copy(a) : NULL)
#define _SESMGR_NFS_M_FREE(a)		((sesmgr_enabled && (a) != NULL) ? \
					 m_free(a) : (struct mbuf *) NULL)
#define _SESMGR_NFS_M_FREEM(a)		((sesmgr_enabled && (a) != NULL) ? \
					 m_freem(a) : (void) 0)
#define _SESMGR_NFS_VSETLABEL(a,b)	((sesmgr_enabled) ? \
					 sesmgr_nfs_vsetlabel(a,b) : 0)
#define _SESMGR_NFS_SET_IPSEC(a,b)	((sesmgr_enabled) ? \
					 sesmgr_nfs_set_ipsec(a,b) : *(b))

/* Remove the mbuf containing ip security options from the front of
 * the mbuf chain and stash a pointer to it in the serive structure.
 */
#define _SESMGR_NFS_GET_IP_ATTRS(a,b) {\
	if (sesmgr_enabled) { \
		(b)->xp_ipattr = (a);\
		(a) = (a)->m_next;\
		ASSERT((a)!= NULL);\
	}\
}

/*
 *  Older routines for accessing socket security attibutes.
 */

#define _SIOCGETLABEL(a,b)      ((sesmgr_enabled)? siocgetlabel(a,b): ENOSYS)
#define _SIOCSETLABEL(a,b)      ((sesmgr_enabled)? siocsetlabel(a,b): ENOSYS)

struct mac_label *sesmgr_get_label(struct socket *);
void sesmgr_set_label(struct socket *, struct mac_label *) ;
struct mac_label *sesmgr_get_sendlabel(struct socket *) ;
void sesmgr_set_sendlabel(struct socket *, struct mac_label *) ;
struct soacl *sesmgr_get_soacl(struct socket *);
void sesmgr_set_soacl(struct socket *, struct soacl *);
uid_t sesmgr_get_uid(struct socket *);
uid_t sesmgr_set_uid(struct socket *, uid_t);
uid_t sesmgr_get_rcvuid(struct socket *);
uid_t sesmgr_set_rcvuid(struct socket *, uid_t);

#endif

#ifdef __cplusplus
}
#endif

#endif	/* __SYS_SESMGR_H_ */
