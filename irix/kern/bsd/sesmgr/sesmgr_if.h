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

#ifndef __SESMGR_IF_H__
#define __SESMGR_IF_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 1.4 $"

struct soacl;
struct socket;
struct mbuf;
struct ifnet;
struct ifreq;

#include <sys/types.h>
#include <sys/acl.h>
#include <sys/mac.h>
#include <sys/capability.h>
#include <sys/t6attrs.h>
#include <sys/ioctl.h>

/*
 * Values for ifs_idiom.  Obsolete style of interface labeling.
 */
#define IDIOM_MONO              0       /* Monolabel (unlabelled)       */
#define IDIOM_BSO_TX            1       /* BSO required for TX, not RX  */
#define IDIOM_BSO_RX            2       /* BSO required for RX, not TX  */
#define IDIOM_BSO_REQ           3       /* BSD required for TX and RX   */
#define IDIOM_CIPSO             4       /* CIPSO, Apr 91, tag types 1, 2*/
#define IDIOM_SGIPSO            5       /* SGI's Mint-flavored CIPSO, Apr 91*/
#define IDIOM_TT1               6       /* CIPSO, Apr 91, tag type 1 only */
#define IDIOM_CIPSO2            7       /* CIPSO2, Jan 92               */
#define IDIOM_SGIPSO2           8       /* SGIPSO2, Jan 92, with UID*/
#define IDIOM_SGIPSOD           9       /* SGIPSO, Apr 91, with UID */
#define IDIOM_SGIPSO2_NO_UID    10      /* SGIPSO2, Jan 92, no UID      */
#define IDIOM_TT1_CIPSO2        11      /* CIPSO2, Jan 92, tag type 1 only */
#define IDIOM_MAX               IDIOM_TT1_CIPSO2

/*
 *  The following structure is used to pass the security attributes
 *  required to label an interface.  Note:  For pointer types below,
 *  the kernel expects the application to have pre-allocated space
 *  for the maximum size attribute.  The kernel may overwrite the
 *  pointer with null if the attribute is not present.
 *
 *  NOTE that the beginning of this structure (iq_name) must be
 *  identical to the layout of the ifreq structure defined in
 *  <net/if.h>. IFNAMSIZ is defined in <net/if.h>; include that
 *  first.
 */
typedef struct t6ifreq {
	char		ifsec_name[IFNAMSIZ];	/* if name, e.g. "en0" */
	t6mask_t	ifsec_mand_attrs;	/* Required from remote host */
	msen_t		ifsec_min_msen;
	msen_t  	ifsec_max_msen;
	mint_t		ifsec_min_mint;
	mint_t  	ifsec_max_mint;
	struct {
		t6mask_t	dflt_attrs;     /* Mask */
		msen_t		dflt_msen;
		mint_t		dflt_mint;
		uid_t		dflt_sid;
		msen_t		dflt_clearance;
		acl_t		dflt_acl;
		cap_t		dflt_privs;
		uid_t		dflt_audit_id;
		uid_t		dflt_uid;
		gid_t		dflt_gid;
		t6groups_t *    dflt_groups;
	} ifsec_dflt;
} t6ifreq_t;

#if _MIPS_SIM == _ABI64

typedef struct irix5_t6ifreq {
        char            ifsec_name[IFNAMSIZ];
        t6mask_t        ifsec_mand_attrs;
        app32_ptr_t     ifsec_min_msen;
        app32_ptr_t     ifsec_max_msen;
        app32_ptr_t     ifsec_min_mint;
        app32_ptr_t     ifsec_max_mint;
        struct {
                t6mask_t        dflt_attrs;     /* Mask */
                app32_ptr_t     dflt_msen;
                app32_ptr_t     dflt_mint;
                uid_t           dflt_sid;
                app32_ptr_t     dflt_clearance;
                app32_ptr_t     dflt_acl;
                app32_ptr_t     dflt_privs;
                uid_t           dflt_audit_id;
                uid_t           dflt_uid;
                gid_t           dflt_gid;
                app32_ptr_t    dflt_groups;
        } ifsec_dflt;
} irix5_t6ifreq_t;
#endif

#define	SIOCGIFLABEL	_IOWR('i',105, struct t6ifreq)
#define	SIOCSIFLABEL	_IOW('i', 106, struct t6ifreq)

#if _MIPS_SIM == _ABI64
#define	IRIX5_SIOCGIFLABEL	_IOWR('i',105, struct irix5_t6ifreq)
#define	IRIX5_SIOCSIFLABEL	_IOW('i', 106, struct irix5_t6ifreq)
#endif

#ifdef _KERNEL

/*
 * Session manger functions for labeling network interfaces.
 */
extern int sesmgr_enabled;
extern void set_lo_secattr(struct ifnet *);
extern int sesmgr_siocgiflabel(struct ifnet *, caddr_t);
extern int sesmgr_siocsiflabel(struct ifnet *, caddr_t);

#define _SESMGR_SIOCGIFLABEL(a,b)  ((sesmgr_enabled)? \
					sesmgr_siocgiflabel(a,b): ENOPKG);
#define _SESMGR_SIOCSIFLABEL(a,b) ((sesmgr_enabled)? \
					sesmgr_siocsiflabel(a,b): ENOPKG);
#define _SET_LO_SECATTR(a)	((sesmgr_enabled)? \
					set_lo_secattr(a): (void)0);
#endif

#ifdef __cplusplus
}
#endif

#endif	/* __SESMGR_IF_H__ */
