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

#ifndef __T6RHDB_H__
#define __T6RHDB_H__

#ifdef __cplusplus
extern "C" {
#endif

#ident	"$Revision: 1.3 $"

#include <limits.h>		/* For NGROUPS_MAX */
#include <netinet/in.h>
#include <sys/capability.h>

/*
 * Session Manager IDs.  Identifies the protocol used
 * to communicate with a host.
 */
typedef enum t6rhdb_smm_id {
	T6RHDB_SMM_INVALID=0,
	T6RHDB_SMM_SINGLE_LEVEL,
	T6RHDB_SMM_MSIX_1_0,
	T6RHDB_SMM_MSIX_2_0,
	T6RHDB_SMM_TSIX_1_0,
	T6RHDB_SMM_TSIX_1_1
} t6rhdb_smm_id_t;

/*
 *  IP Security Options.  NLM is a historical
 *  reference to the TSIX spec.
 */
typedef enum t6rhdb_nlm_id {
	T6RHDB_NLM_INVALID=0,	/* IP Security Packet is wrong    */
	T6RHDB_NLM_UNLABELED,	/* Mono label                     */
	T6RHDB_NLM_RIPSO_BSO,	/* Basic Security Options RFC1108 */
	T6RHDB_NLM_RIPSO_BSOT,	/* BSO required for TX, not RX    */
	T6RHDB_NLM_RIPSO_BSOR,	/* BSO required for RX, not TX    */
	T6RHDB_NLM_RIPSO_ESO,	/* Extended Security Options R1108*/
	T6RHDB_NLM_CIPSO,	/* CIPSO tag type 1 or 2          */
	T6RHDB_NLM_CIPSO_TT1,	/* CIPSO tag type 1 only          */
	T6RHDB_NLM_CIPSO_TT2,	/* CIPSO tag type 2 only          */
	T6RHDB_NLM_SGIPSO,	/* SIPSO tag type 1 or 2 with uid */
	T6RHDB_NLM_SGIPSONOUID,	/* SIPSO tag type 1 or 2          */
	T6RHDB_NLM_SGIPSO_SPCL,	/* SIPSO tag type Spcial          */
	T6RHDB_NLM_SGIPSO_LOOP,	/* SIPSO internal loop the stack  */
	T6RHDB_NLM_MAX		/* Maximum number of NLM entries  */
} t6rhdb_nlm_id_t;

/* 
 * Macros to evaluate CIPSO and SGIPSO Network Level Modules (NLM) characteristics.
 */
#define CIPSO(n)        ( (n==T6RHDB_NLM_CIPSO)			|| \
			  (n==T6RHDB_NLM_CIPSO_TT1) 		|| \
			  (n==T6RHDB_NLM_CIPSO_TT2) )

#define SGIPSO(n)	( (n==T6RHDB_NLM_SGIPSO)		|| \
                          (n==T6RHDB_NLM_SGIPSO_SPCL)           || \
			  (n==T6RHDB_NLM_SGIPSONOUID) )
/*
 * The following information is for historical from idiom to nlm and is no 
 * longer used.
 *
 * Values for ifs_idiom.  Obsolete style of interface labeling.
 *
 * IDIOM_MONO 		Monolabel (unlabelled)		T6RHDB_NLM_UNLABELED
 * IDIOM_BSO_TX		BSO required for TX, not RX	T6RHDB_NLM_RIPSO_BSOR
 * IDIOM_BSO_RX		BSO required for RX, not TX	T6RHDB_NLM_RIPSO_BSOT
 * IDIOM_BSO_REQ	BSD required for TX and RX	T6RHDB_NLM_RIPSO_BSO,
 * IDIOM_CIPSO		CIPSO, Apr 91, tag types 1, 2	Pre-CIPSO, code removed
 * IDIOM_SGIPSO		SGI's Mint-flavored CIPSO,Apr91	Pre-CIPSO, code removed
 * IDIOM_TT1		CIPSO, Apr 91, tag type 1 only	Pre-CIPSO, code removed
 * IDIOM_SGIPSOD	SGIPSO, Apr 91, with UID	Pre-CIPSO, code removed
 * IDIOM_CIPSO2		CIPSO2, Jan 92			T6RHDB_NLM_CIPSO
 * IDIOM_TT1_CIPSO2	CIPSO2, Jan 92, tag type 1 only	T6RHDB_NLM_CIPSO_TT1
 * New, no IDIOM	CIPSO2, Jan 92, tag type 2 only	T6RHDB_NLM_CIPSO_TT2
 * IDIOM_SGIPSO2	SGIPSO2, Jan 92, with UID	T6RHDB_NLM_SGIPSO
 * IDIOM_SGIPSO2_NO_UID	SGIPSO2, Jan 92, no UID		T6RHDB_NLM_SGIPSONOUID
 */

/*
 *  Flags.  Indicates which attributes are mandatory
 *  on packets received from a host.
 */
typedef enum t6rhdb_flag_id {
	T6RHDB_FLG_INVALID=0,
	T6RHDB_FLG_IMPORT,
	T6RHDB_FLG_EXPORT,
	T6RHDB_FLG_DENY_ACCESS,
	T6RHDB_FLG_MAND_SL,
	T6RHDB_FLG_MAND_INTEG,
	T6RHDB_FLG_MAND_ILB,
	T6RHDB_FLG_MAND_PRIVS,
	T6RHDB_FLG_MAND_LUID,
	T6RHDB_FLG_MAND_IDS,
	T6RHDB_FLG_MAND_SID,
	T6RHDB_FLG_MAND_PID,
	T6RHDB_FLG_MAND_CLEARANCE,
	T6RHDB_FLG_TRC_RCVPKT,
	T6RHDB_FLG_TRC_XMTPKT,
	T6RHDB_FLG_TRC_RCVATT,
	T6RHDB_FLG_TRC_XMTATT
} t6rhdb_flag_id_t;

#define T6RHDB_MASK(value)          ((unsigned int) (1<<(value)))

/*
 *  Vendor ID for tweaking the protocol to
 *  account for different interpretations of 
 *  the TSIX spec.
 */
typedef enum t6rhdb_vendor_id {
	T6RHDB_VENDOR_INVALID=0,
	T6RHDB_VENDOR_UNKNOWN,
	T6RHDB_VENDOR_SUN,
	T6RHDB_VENDOR_HP,
	T6RHDB_VENDOR_IBM,
	T6RHDB_VENDOR_CRAY,
	T6RHDB_VENDOR_DG,
	T6RHDB_VENDOR_HARRIS
} t6rhdb_vendor_id_t;

/*
 *  Buffer for returning statistics about the
 *  remote host data base.
 */
typedef struct t6rhdb_rstat {
	int host_cnt;
	int host_size;
	int profile_cnt;
	int profile_size;
} t6rhdb_rstat_t;

#if defined(_KERNEL)
#include <sys/hashing.h>

/*
 * This structure is the in-kernel representation
 * of a t6rhdb_host_buf_t.
 */
typedef struct t6rhdb_kern_buf {
	int		hp_smm_type;
	int		hp_nlm_type;
	int		hp_auth_type;
	int		hp_encrypt_type;
	int 		hp_attributes;
	int		hp_flags;
	int		hp_cache_size;
	int		hp_host_cnt;
	cap_set_t	hp_def_priv;
	cap_set_t	hp_max_priv;
	mac_b_label *	hp_def_sl;
	mac_b_label *	hp_min_sl;
	mac_b_label *	hp_max_sl;
	mac_b_label *	hp_def_integ;
	mac_b_label *	hp_min_integ;
	mac_b_label *	hp_max_integ;
	mac_b_label *	hp_def_ilb;
	mac_b_label *	hp_def_clearance;
	uid_t		hp_def_sid;		/* Session ID */
	uid_t		hp_def_uid;
	uid_t		hp_def_luid;		/* Login or Audit ID */
	gid_t		hp_def_gid;
	int		hp_def_grp_cnt;
	gid_t		hp_def_groups[NGROUPS_MAX];
} t6rhdb_kern_buf_t;

/*
 *  For fast lookup, host addresses are stored in a small hash
 *  table with a linked list to resolve collisions.
 */
#define T6RHDB_TABLE_SIZE	256
#define T6RHDB_REFCNT_MASK	0x7fffffff
#define T6RHDB_REFCNT_BAD	0x80000000

typedef struct t6rhdb_host_hash {
	struct hashbucket		hh_hash;
	t6rhdb_kern_buf_t		hh_profile;
	struct in_addr			hh_addr;
	int				hh_flags;
	unsigned int			hh_refcnt;
	lock_t				hh_lock;
} t6rhdb_host_hash_t;
#endif

/*
 *  This structure is used to pass a host security profile
 *  into the kernel.  The structure is followed by a list
 *  of the applicable IP addresses, the sensitivity ranges and the
 *  integrity label ranges.
 */
typedef struct t6rhdb_host_buf {
	int		hp_smm_type;
	int		hp_nlm_type;
	int		hp_auth_type;
	int		hp_encrypt_type;
	int 		hp_attributes;
	int		hp_flags;
	int		hp_cache_size;	
	int		hp_host_cnt;
	cap_set_t	hp_max_priv;
	mac_b_label	hp_def_sl;
	mac_b_label	hp_min_sl;
	mac_b_label	hp_max_sl;
	mac_b_label	hp_def_integ;
	mac_b_label	hp_min_integ;
	mac_b_label	hp_max_integ;
	mac_b_label	hp_def_ilb;
	mac_b_label	hp_def_clearance;
	uid_t		hp_def_sid;		/* Session ID */
	uid_t		hp_def_uid;
	uid_t		hp_def_luid;		/* Login or Audit ID */
	gid_t		hp_def_gid;
	int		hp_def_grp_cnt;
	gid_t		hp_def_groups[NGROUPS_MAX];
	cap_set_t	hp_def_priv;
} t6rhdb_host_buf_t;

/*
 *  Library Interfaces.
 */

#if defined(_KERNEL)
#include <sys/systm.h>
extern int t6rhdb_put_host(size_t, caddr_t, rval_t *);
extern int t6rhdb_get_host(struct in_addr *, size_t, caddr_t, rval_t *);
extern int t6rhdb_stat(caddr_t data);
extern int t6rhdb_flush(struct in_addr *, rval_t *);
extern void t6rhdb_init(void);
extern int t6findhost(struct in_addr *, int, t6rhdb_kern_buf_t *);
#else
int t6rhdb_put_host(size_t, caddr_t);
int t6rhdb_get_host(struct in_addr *, size_t, caddr_t);
int t6rhdb_stat(caddr_t);
int t6rhdb_flush(const struct in_addr *);
#endif

#ifdef __cplusplus
}
#endif

#endif	/* __T6RHDB_H__ */
