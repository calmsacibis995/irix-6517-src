/**************************************************************************
 *									  *
 *		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ifndef	_HOST_DHOST_H_
#define	_HOST_DHOST_H_
#ident "$Id: dhost.h,v 1.6 1997/09/08 13:55:23 henseler Exp $"

#include <ksys/behavior.h>
#include <ksys/cell/handle.h>
#include <ksys/cell/object.h>
#include <ksys/cell/tkm.h>
#include <ksys/cell/wp.h>
#include <ksys/vhost.h>

#include "../vhost_private.h"
#include "../phost_private.h"

/*
 * Id registered with White Pages as the service of the host server.
 * Looking up this id yields the service id for the host server.
 */
#define HOST_SERVICE_WPBASE	0
#define HOST_SERVICE_WPRANGE	1

/*
 * Token class for distributed host state synchronization.
 */
#define	VHOST_TOKEN_MEMBER	0
#define	VHOST_TOKEN_REBOOT	1
#define	VHOST_TOKEN_KILLALL	2
#define	VHOST_TOKEN_SYNC	3
#define	VHOST_TOKEN_HOSTID	4
#define	VHOST_TOKEN_HOSTNAME	5
#define	VHOST_TOKEN_DOMAINNAME	6
#define	VHOST_TOKEN_CRED	7
#define	VHOST_TOKEN_SET_TIME	8
#define	VHOST_NTOKENS		9

/*
 * The MEMBER token is held by every cell is the system.
 */
#define	VHOST_MEMBER_TOKEN					\
	TK_MAKE_SINGLETON(VHOST_TOKEN_MEMBER, TK_READ)
#define	VHOST_MEMBER_TOKENSET					\
	TK_MAKE(VHOST_TOKEN_MEMBER, TK_READ)

/*
 * The REBOOT token is held by every cell is the system.
 * Except for UNIKERNEL in which only the server holds it.
 */
#define	VHOST_REBOOT_TOKEN					\
	TK_MAKE_SINGLETON(VHOST_TOKEN_REBOOT, TK_READ)
#define	VHOST_REBOOT_TOKENSET					\
	TK_MAKE(VHOST_TOKEN_REBOOT, TK_READ)

/*
 * The KILLALL token is held by each cell hosting process managemant.
 * It is used to visit all cells to kill all processes before rebooting.
 */
#define	VHOST_KILLALL_TOKEN					\
	TK_MAKE_SINGLETON(VHOST_TOKEN_KILLALL, TK_READ)
#define	VHOST_KILLALL_TOKENSET					\
	TK_MAKE(VHOST_TOKEN_KILLALL, TK_READ)

/*
 * The SYNC token is held by each cell hosting a filesystem.
 */
#define	VHOST_SYNC_TOKEN					\
	TK_MAKE_SINGLETON(VHOST_TOKEN_SYNC, TK_READ)
#define	VHOST_SYNC_TOKENSET					\
	TK_MAKE(VHOST_TOKEN_SYNC, TK_READ)

/*
 * The SET_TIME token is held by each cell.
 */
#define	VHOST_SET_TIME_TOKEN					\
	TK_MAKE_SINGLETON(VHOST_TOKEN_SET_TIME, TK_READ)
#define	VHOST_SET_TIME_TOKENSET					\
	TK_MAKE(VHOST_TOKEN_SET_TIME, TK_READ)

/*
 * The HOSTID token synchronizes access to the BSD hostid.
 */
#define	VHOST_HOSTID_READ_TOKEN					\
	TK_MAKE_SINGLETON(VHOST_TOKEN_HOSTID, TK_READ)
#define	VHOST_HOSTID_READ_TOKENSET				\
	TK_MAKE(VHOST_TOKEN_HOSTID, TK_READ)
#define	VHOST_HOSTID_WRITE_TOKEN				\
	TK_MAKE_SINGLETON(VHOST_TOKEN_HOSTID, TK_WRITE)
#define	VHOST_HOSTID_WRITE_TOKENSET				\
	TK_MAKE(VHOST_TOKEN_HOSTID, TK_WRITE)

/*
 * The HOSTNAME token...
 */
#define	VHOST_HOSTNAME_READ_TOKEN				\
	TK_MAKE_SINGLETON(VHOST_TOKEN_HOSTNAME, TK_READ)
#define	VHOST_HOSTNAME_READ_TOKENSET				\
	TK_MAKE(VHOST_TOKEN_HOSTNAME, TK_READ)
#define	VHOST_HOSTNAME_WRITE_TOKEN				\
	TK_MAKE_SINGLETON(VHOST_TOKEN_HOSTNAME, TK_WRITE)
#define	VHOST_HOSTNAME_WRITE_TOKENSET				\
	TK_MAKE(VHOST_TOKEN_HOSTNAME, TK_WRITE)

/*
 * The DOMAINNAME token...
 */
#define	VHOST_DOMAINNAME_READ_TOKEN				\
	TK_MAKE_SINGLETON(VHOST_TOKEN_DOMAINNAME, TK_READ)
#define	VHOST_DOMAINNAME_READ_TOKENSET				\
	TK_MAKE(VHOST_TOKEN_DOMAINNAME, TK_READ)
#define	VHOST_DOMAINNAME_WRITE_TOKEN				\
	TK_MAKE_SINGLETON(VHOST_TOKEN_DOMAINNAME, TK_WRITE)
#define	VHOST_DOMAINNAME_WRITE_TOKENSET				\
	TK_MAKE(VHOST_TOKEN_DOMAINNAME, TK_WRITE)


/*
 * The CRED token is held by every cell in the system
 */
#define	VHOST_CRED_TOKEN					\
	TK_MAKE_SINGLETON(VHOST_TOKEN_CRED, TK_READ)
#define	VHOST_CRED_TOKENSET					\
	TK_MAKE(VHOST_TOKEN_CRED, TK_READ)

/*
 * The distributed behavior private data structures:
 */
typedef struct dchost {
	TKC_DECL(dch_tclient, VHOST_NTOKENS);
	obj_handle_t	dch_handle;
	bhv_desc_t	dch_bhv;
} dchost_t;

typedef struct dshost {
	TKC_DECL(dsh_tclient, VHOST_NTOKENS);
	TKS_DECL(dsh_tserver, VHOST_NTOKENS);
	bhv_desc_t	dsh_bhv;
} dshost_t;


/*
 * Macros:
 */
#define BHV_TO_DCHOST(bdp) \
	(ASSERT(BHV_OPS(bdp) == &dchost_ops), \
	(dchost_t *)(BHV_PDATA(bdp)))
#define BHV_TO_DSHOST(bdp) \
	(ASSERT(BHV_OPS(bdp) == &dshost_ops), \
	(dshost_t *)(BHV_PDATA(bdp)))

#define	DCHOST_TO_SERVICE(dcp)	HANDLE_TO_SERVICE((dcp)->dch_handle)
#define	DCHOST_TO_OBJID(dcp)	HANDLE_TO_OBJID((dcp)->dch_handle)

/*
 * Subsystem internal symbols:
 */
extern vhost_ops_t	dshost_ops;
extern vhost_ops_t	dchost_ops;

extern service_t	vhost_service(void);
extern void		dchost_create(vhost_t *vhost);
extern void		vhost_interpose(vhost_t *vhost);

/*
 * Define table mapping between host service masks and tokens.
 */
typedef struct {
	int		svc;
	tk_set_t	tk;
} vh_svc_tk_map_t;
#define VHOST_SVC_TK_MAP {					\
		{VH_SVC_REBOOT,	 VHOST_REBOOT_TOKENSET},	\
		{VH_SVC_KILLALL, VHOST_KILLALL_TOKENSET},	\
		{VH_SVC_SYNC,    VHOST_SYNC_TOKENSET},		\
		{VH_SVC_SET_TIME,    VHOST_SET_TIME_TOKENSET},		\
		{VH_SVC_CRED,    VHOST_CRED_TOKENSET}		\
	}
#define VHOST_SVC_ALL_TOKENSET					\
	(VHOST_REBOOT_TOKENSET|VHOST_KILLALL_TOKENSET|VHOST_SYNC_TOKENSET|VHOST_CRED_TOKENSET|VHOST_SET_TIME_TOKENSET)
extern vh_svc_tk_map_t	vh_svc_tk_map[];
extern tk_set_t		vh_svc_to_tokenset(int);	

extern void		idbg_dshost_print(struct dshost *);
extern void		idbg_dchost_print(struct dchost *);
extern void		idbg_phost_print(struct phost *);
extern void		dshost_recovery(cell_t);

#endif	/* _HOST_DHOST_H_ */
