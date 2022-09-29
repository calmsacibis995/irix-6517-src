
/**************************************************************************
 *									  *
 * 		 Copyright (C) 1992, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef __CIPSO_
#define __CIPSO_

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 1.8 $"
#include <sys/types.h>
#include <sys/mac_label.h>
#ifdef _KERNEL
#include <sys/systm.h>	/* for rval_t */
#endif /* _KERNEL */
#include <sys/so_dac.h>

/* SVR4 Network Loopback */
#include <sys/ticlts.h>
#include <sys/ticots.h>
#include <sys/ticotsord.h>

#if defined(__sgi) && !defined(_KERNEL)
/* cipso */
extern int recvl(int, void *, int, int, struct mac_label *);
extern int recvlfrom(int, void *, int, int, void *, int *, struct mac_label *);
extern int recvlmsg(int, struct msghdr *, int, struct mac_label *);
extern int recvlu(int, void *, int, int, struct recvluinfo *);
extern int recvlufrom(int, void *, int, int, void *, int *, struct
		      recvluinfo *);
extern int recvlumsg(int, struct msghdr *, int, struct recvluinfo *);
extern int recvu(int, void *, int, int, uid_t *);
extern int recvufrom(int, void *, int, int, void *, int *, uid_t *);
extern int recvumsg(int, struct msghdr *, int, uid_t *);
extern int getpsoacl(struct soacl *);
extern int setpsoacl(struct soacl *);
#endif

#ifdef _KERNEL
struct ifnet;
struct ifreq;
struct mac_label;
struct mbuf;
struct socket;
struct inpcb;
struct msghdr;
struct recvlumsga;
extern int getpsoacl(struct soacl *, rval_t *);
extern int setpsoacl(struct soacl *);
extern int recvlumsg(struct recvlumsga *,rval_t *);
extern int siocgetlabel(struct socket *, caddr_t);
extern int siocsetlabel(struct socket *, caddr_t);
extern int siocgetacl(struct socket *, caddr_t, rval_t *);
extern int siocsetacl(struct socket *, struct soacl *);
extern int siocgetuid(struct socket *, uid_t *, rval_t *);
extern int siocsetuid(struct socket *, uid_t *);
extern int siocgetrcvuid(struct socket *, uid_t *, rval_t *);
extern int siocgiflabel(struct ifnet *, struct ifreq *);
extern int siocsiflabel(struct ifnet *, struct ifreq *, 
			struct socket *so);
extern int siocgifuid(struct ifnet *, struct ifreq *);
extern int siocsifuid(struct ifnet *, struct ifreq *);
extern int mac_inrange(struct mac_label *, struct mac_label *, 
		       struct mac_label *);
extern int dac_allowed(uid_t, struct soacl *);
extern struct mac_label *ip_recvlabel(struct mbuf *, 
				      struct ifnet *, uid_t *);
extern struct mbuf * ip_xmitlabel( struct ifnet *, struct mbuf *, 
			mac_label *, uid_t, int *);
extern void set_lo_secattr(struct ifnet *);
extern void cipso_confignote(void);

extern void cipso_proc_init(struct proc *, struct proc *);
extern void cipso_proc_exit(struct proc *);
extern void cipso_socket_init(struct proc *, struct socket *);
extern void svr4_tcl_endpt_init(tcl_endpt_t * );
extern void svr4_tco_endpt_init(tco_endpt_t * );
extern void svr4_tcoo_endpt_init(tcoo_endpt_t * );


/* Define macros choosing stub functions or real functions here */
extern int cipso_enabled;
#define _GETPSOACL(a,b)		((cipso_enabled)? getpsoacl(a,b): ENOPKG)
#define _SETPSOACL(a)		((cipso_enabled)? setpsoacl(a): ENOPKG)
#define _RECVLUMSG(a,b)	 	((cipso_enabled)? recvlumsg(a,b): ENOPKG)
#define _SIOCGETLABEL(a,b) 	((cipso_enabled)? siocgetlabel(a,b): ENOPKG)
#define _SIOCSETLABEL(a,b) 	((cipso_enabled)? siocsetlabel(a,b): ENOPKG)
#define _SIOCGETACL(a,b,c) 	((cipso_enabled)? siocgetacl(a,b,c): ENOPKG)
#define _SIOCSETACL(a,b) 	((cipso_enabled)? siocsetacl(a,b): ENOPKG)
#define _SIOCGETUID(a,b,c) 	((cipso_enabled)? siocgetuid(a,b,c): ENOPKG)
#define _SIOCSETUID(a,b) 	((cipso_enabled)? siocsetuid(a,b): ENOPKG)
#define _SIOCGETRCVUID(a,b,c) 	((cipso_enabled)? siocgetrcvuid(a,b,c): ENOPKG)
#define _SIOCGIFLABEL(a,b) 	((cipso_enabled)? siocgiflabel(a,b): ENOPKG)
#define _SIOCSIFLABEL(a,b,c) 	((cipso_enabled)? siocsiflabel(a,b,c):ENOPKG)
#define _SIOCGIFUID(a,b) 	((cipso_enabled)? siocgifuid(a,b): ENOPKG)
#define _SIOCSIFUID(a,b) 	((cipso_enabled)? siocsifuid(a,b): ENOPKG)
#define _DAC_ALLOWED(a,b)   	((cipso_enabled)? dac_allowed(a,b): 1)
#define _SOACL_OK(a)	   	((cipso_enabled)? soacl_ok(a): 1)
#define _MAC_INRANGE(a,b,c)   	((cipso_enabled)? mac_inrange(a,b,c): 1)
#define _IP_RECVLABEL(a,b,c)	((cipso_enabled)? ip_recvlabel(a,b,c): 0)
#define _IP_XMITLABEL(a,b,c,d,e) ((cipso_enabled)? ip_xmitlabel(a,b,c,d,e):1)
#define _SET_LO_SECATTR(a) 	((cipso_enabled)? set_lo_secattr(a):(void)0)
#define _CIPSO_CONFIGNOTE() 	((cipso_enabled)? cipso_confignote(): (void)0)

#define _CIPSO_PROC_INIT(a,b)	((cipso_enabled)? cipso_proc_init(a,b): (void)0)
#define _CIPSO_PROC_EXIT(a)	((cipso_enabled)? cipso_proc_exit(a): (void)0)
#define _CIPSO_SOCKET_INIT(a,b) \
		((cipso_enabled)? cipso_socket_init(a,b): (void)0)
#define _SVR4_TCL_ENDPT_INIT(a) \
		((cipso_enabled)? svr4_tcl_endpt_init(a): (void)0)
#define _SVR4_TCO_ENDPT_INIT(a) \
		((cipso_enabled)? svr4_tco_endpt_init(a): (void)0)
#define _SVR4_TCOO_ENDPT_INIT(a) \
		((cipso_enabled)? svr4_tcoo_endpt_init(a): (void)0)

/*
 *  cipso_proc is a structure parallel to the proc table that holds
 *  per process information for the cipso subsystem.
 */

typedef struct cipso_proc {
	struct soacl * soacl;
} cipso_proc_t;

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif

#endif /* __CIPSO_ */
