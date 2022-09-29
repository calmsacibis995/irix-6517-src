/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995-1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ident "$Id: vhost.h,v 1.7 1997/09/08 13:50:59 henseler Exp $"

#ifndef _KSYS_VHOST_H_
#define _KSYS_VHOST_H_

#include <ksys/behavior.h>
#include <ksys/cell.h>
#include <sys/sysget.h>
#if CELL_IRIX
#include <fs/cfs/cfs.h>
#else
typedef void cfs_handle_t;
#endif

/*
 * Global notification services for which a cell can register.
 */
#define	VH_SVC_REBOOT	0x01
#define	VH_SVC_KILLALL	0x02
#define	VH_SVC_SYNC	0x04
#define	VH_SVC_CRED	0x08
#define	VH_SVC_SET_TIME	0x10
#define VH_SVC_ALL	VH_SVC_REBOOT|VH_SVC_KILLALL|VH_SVC_SYNC|VH_SVC_CRED|VH_SVC_SET_TIME

/*
 * Trivial virtual host:
 */
typedef struct vhost {
	bhv_head_t	vh_bhvh;	/* Behavior head */
} vhost_t;

extern void	vhost_init(void);

typedef struct vhost_ops {
	bhv_position_t	vh_position;	/* position within behavior chain */
	void (*vhop_register)		/* register cell for services */
		(bhv_desc_t *b,		/*   IN: behavior */
		 int);			/*   IN: mask of services */
	void (*vhop_deregister)		/* deregister cell */
		(bhv_desc_t *b,		/*   IN: behavior */
		 int);			/*   IN: mask of services */
	void (*vhop_sethostid)		/* set BSD hostid */
		(bhv_desc_t *b,		/*   IN: behavior */
		 int);			/*   IN: hostid to set */
	void (*vhop_gethostid)		/* get BSD hostid */
		(bhv_desc_t *b,		/*   IN: behavior */
		 int *);		/*   OUT: hostid returned */
	void (*vhop_sethostname)	/* set hostname */
		(bhv_desc_t *b,		/*   IN: behavior */
		 char *,		/*   IN: hostname to set */
		 size_t *);		/*   INOUT: name length */
	void (*vhop_gethostname)	/* get hostname */
		(bhv_desc_t *b,		/*   IN: behavior */
		 char *,		/*   IN: hostname buffer */
		 size_t *);		/*   INOUT: name length */
	void (*vhop_setdomainname)	/* set domainname */
		(bhv_desc_t *b,		/*   IN: behavior */
		 char *,		/*   IN: hostname to set */
		 size_t);		/*   IN: name length */
	void (*vhop_getdomainname)	/* get domainname */
		(bhv_desc_t *b,		/*   IN: behavior */
		 char *,		/*   IN: hostname buffer */
		 size_t *);		/*   INOUT: name length */
	void (*vhop_killall)		/* signal all processes */
		(bhv_desc_t *b,		/*   IN: behavior */
		 int,			/*   IN: signo */
		 pid_t,			/*   IN: pgid */
		 pid_t,			/*   IN: caller's pid */
		 pid_t);		/*   IN: caller's session id */
	void (*vhop_reboot)		/* system reboot */
		(bhv_desc_t *b,		/*   IN: behavior */
		 int,			/*   IN: fcn */
		 char *);		/*   IN: mdep - XXX unused */
	void (*vhop_sync)		/* sync filesystems */
		(bhv_desc_t *b,		/*   IN: behavior */
		 int);			/*   IN: flags */
	int (*vhop_sysget)		/* get system data */
		(bhv_desc_t *b,		/*   IN: behavior */
		 int,			/*   IN: name */
		 char *,		/*   IN: buffer */
		 int *,			/*   INOUT: buflen */
		 int,			/*   IN: flags */
		 sgt_cookie_t *, 	/*   INOUT: cookie */
		 sgt_info_t *); 	/*   INOUT: info */
	int (*vhop_systune)		/* set system tuneables */
		(bhv_desc_t *b,		/*   IN: behavior */
		 int,			/*   IN: group index */
		 int,			/*   IN: tune index */
		 uint64_t);		/*   IN: value */
	void (*vhop_credflush)		/* credid cache flush */
		(bhv_desc_t *b,		/*   IN: behavior */
		 cell_t);		/*   IN: server cell */
	void (*vhop_credflush_wait)	/* credid cache flush wait */
		(bhv_desc_t *b,		/*   IN: behavior */
		 cell_t);		/*   IN: flush handle */
	void (*vhop_sysacct)		/* accounting start/stop */
		(bhv_desc_t *b,		/*   IN: behavior */
		 int,			/*   IN: enable flag */
		 cfs_handle_t *);	/*   IN: accounting file handle */
	void (*vhop_set_time)		/* set global time */
		(bhv_desc_t *b,		/*   IN: behavior */
		 cell_t);		/*   IN: source cell */
	void (*vhop_adj_time)		/* adjust global time */
		(bhv_desc_t *b,		/*   IN: behavior */
		 long,			/*   IN: time adjustment */
		 long *);		/*   OUT: old time adjustment */
} vhost_ops_t;

extern	vhost_t *vhost_local(void);	/* Function returning pointer to vhost */

#define	vh_fbhv		vh_bhvh.bh_first	/* 1st behavior */
#define	vh_ops		vh_fbhv->bd_ops		/* ops for 1st behavior */
#define _VHOP_(op, v)	(*((vhost_ops_t *)(v)->vh_ops)->op)

#define VHOST_REGISTER(_vh_svc) \
	{ \
	vhost_t	*vhp = vhost_local(); \
	BHV_READ_LOCK(&(vhp)->vh_bhvh); \
	_VHOP_(vhop_register, vhp) ((vhp)->vh_fbhv, _vh_svc); \
	BHV_READ_UNLOCK(&(vhp)->vh_bhvh); \
	}

#define VHOST_DEREGISTER(_vh_svc) \
	{ \
	vhost_t	*vhp = vhost_local(); \
	BHV_READ_LOCK(&(vhp)->vh_bhvh); \
	_VHOP_(vhop_deregister, vhp) ((vhp)->vh_fbhv, _vh_svc); \
	BHV_READ_UNLOCK(&(vhp)->vh_bhvh); \
	}

#define VHOST_SETHOSTID(_hostid) \
	{ \
	vhost_t	*vhp = vhost_local(); \
	BHV_READ_LOCK(&(vhp)->vh_bhvh); \
	_VHOP_(vhop_sethostid, vhp) ((vhp)->vh_fbhv, _hostid); \
	BHV_READ_UNLOCK(&(vhp)->vh_bhvh); \
	}

#define VHOST_GETHOSTID(_hostid) \
	{ \
	vhost_t	*vhp = vhost_local(); \
	BHV_READ_LOCK(&(vhp)->vh_bhvh); \
	_VHOP_(vhop_gethostid, vhp) ((vhp)->vh_fbhv, _hostid); \
	BHV_READ_UNLOCK(&(vhp)->vh_bhvh); \
	}

#define VHOST_SETHOSTNAME(_hostname, _len) \
	{ \
	vhost_t	*vhp = vhost_local(); \
	BHV_READ_LOCK(&(vhp)->vh_bhvh); \
	_VHOP_(vhop_sethostname, vhp) ((vhp)->vh_fbhv, _hostname, _len); \
	BHV_READ_UNLOCK(&(vhp)->vh_bhvh); \
	}

#define VHOST_GETHOSTNAME(_hostname, _len) \
	{ \
	vhost_t	*vhp = vhost_local(); \
	BHV_READ_LOCK(&(vhp)->vh_bhvh); \
	_VHOP_(vhop_gethostname, vhp) ((vhp)->vh_fbhv, _hostname, _len); \
	BHV_READ_UNLOCK(&(vhp)->vh_bhvh); \
	}

#define VHOST_SETDOMAINNAME(_domainname, _len) \
	{ \
	vhost_t	*vhp = vhost_local(); \
	BHV_READ_LOCK(&(vhp)->vh_bhvh); \
	_VHOP_(vhop_setdomainname, vhp) ((vhp)->vh_fbhv, _domainname, _len); \
	BHV_READ_UNLOCK(&(vhp)->vh_bhvh); \
	}

#define VHOST_GETDOMAINNAME(_domainname, _len) \
	{ \
	vhost_t	*vhp = vhost_local(); \
	BHV_READ_LOCK(&(vhp)->vh_bhvh); \
	_VHOP_(vhop_getdomainname, vhp) ((vhp)->vh_fbhv, _domainname, _len); \
	BHV_READ_UNLOCK(&(vhp)->vh_bhvh); \
	}

#define VHOST_KILLALL(_signo, _pgid, _pid, _sid) \
	{ \
	vhost_t	*vhp = vhost_local(); \
	BHV_READ_LOCK(&(vhp)->vh_bhvh); \
	_VHOP_(vhop_killall, vhp) ((vhp)->vh_fbhv, _signo, _pgid, _pid, _sid); \
	BHV_READ_UNLOCK(&(vhp)->vh_bhvh); \
	}

#define VHOST_REBOOT(_fcn, _mdep) \
	{ \
	vhost_t	*vhp = vhost_local(); \
	BHV_READ_LOCK(&(vhp)->vh_bhvh); \
	_VHOP_(vhop_reboot, vhp) ((vhp)->vh_fbhv, _fcn, _mdep); \
	BHV_READ_UNLOCK(&(vhp)->vh_bhvh); \
	}

#define VHOST_SYNC(_flags) \
	{ \
	vhost_t	*vhp = vhost_local(); \
	BHV_READ_LOCK(&(vhp)->vh_bhvh); \
	_VHOP_(vhop_sync, vhp) ((vhp)->vh_fbhv, _flags); \
	BHV_READ_UNLOCK(&(vhp)->vh_bhvh); \
	}

#define VHOST_SYSGET(_name, _buf, _buflen, _flags, _cookie, _info, _rv) \
	{ \
	vhost_t	*vhp = vhost_local(); \
	BHV_READ_LOCK(&(vhp)->vh_bhvh); \
	_rv = _VHOP_(vhop_sysget, vhp) ((vhp)->vh_fbhv, _name, _buf, _buflen, \
		_flags, _cookie, _info); \
	BHV_READ_UNLOCK(&(vhp)->vh_bhvh); \
	}

#define VHOST_SYSTUNE(_group_index, _tune_index, _value, _rv) \
	{ \
	vhost_t	*vhp = vhost_local(); 					\
	BHV_READ_LOCK(&(vhp)->vh_bhvh); 				\
									\
	/* Do our cell first */ 					\
									\
	_rv = phost_systune((vhp)->vh_fbhv, _group_index,  _tune_index, \
		 _value); 						\
	if (!_rv && (((vhost_ops_t *)(vhp)->vh_ops)->vhop_systune !=	\
				 phost_systune)) { 			\
									\
		/* Call server */					\
									\
		_rv = _VHOP_(vhop_systune, vhp) ((vhp)->vh_fbhv, _group_index, \
			_tune_index, _value); 				\
	}								\
	BHV_READ_UNLOCK(&(vhp)->vh_bhvh); 				\
	}

#define VHOST_CREDFLUSH(_server) \
	{ \
	vhost_t	*vhp = vhost_local(); \
	BHV_READ_LOCK(&(vhp)->vh_bhvh); \
	_VHOP_(vhop_credflush, vhp) ((vhp)->vh_fbhv, _server); \
	BHV_READ_UNLOCK(&(vhp)->vh_bhvh); \
	}

#define VHOST_CREDFLUSH_WAIT(_server) \
	{ \
	vhost_t	*vhp = vhost_local(); \
	BHV_READ_LOCK(&(vhp)->vh_bhvh); \
	_VHOP_(vhop_credflush_wait, vhp) ((vhp)->vh_fbhv, _server); \
	BHV_READ_UNLOCK(&(vhp)->vh_bhvh); \
	}

#if CELL_IRIX
#define VHOST_SYSACCT(_enable)					\
	{ 							\
	vhost_t	*vhp = vhost_local(); 				\
	cfs_handle_t handle;					\
	if (_enable) {						\
		cfs_vnexport(acctvp, &handle);			\
	}							\
	BHV_READ_LOCK(&(vhp)->vh_bhvh); 			\
	_VHOP_(vhop_sysacct, vhp) ((vhp)->vh_fbhv, _enable, &handle); \
	BHV_READ_UNLOCK(&(vhp)->vh_bhvh); 			\
	}
#else
#define VHOST_SYSACCT(_enable)
#endif

#define VHOST_SET_TIME(_cellid) \
	{ \
	vhost_t	*vhp = vhost_local(); \
	BHV_READ_LOCK(&(vhp)->vh_bhvh); \
	_VHOP_(vhop_set_time, vhp) ((vhp)->vh_fbhv, _cellid); \
	BHV_READ_UNLOCK(&(vhp)->vh_bhvh); \
	}

#define VHOST_ADJ_TIME(_adjustment, _odelta) \
	{ \
	vhost_t	*vhp = vhost_local(); \
	BHV_READ_LOCK(&(vhp)->vh_bhvh); \
	_VHOP_(vhop_adj_time, vhp) ((vhp)->vh_fbhv, _adjustment, _odelta); \
	BHV_READ_UNLOCK(&(vhp)->vh_bhvh); \
	}


/*
 * Declarations for physical hosts operations.
 * The VHOST subsystem is unusual in having its physical behavior ops
 * throughout kernel subdirectories rather than being private to the
 * os/host directory.
 */
extern void	 phost_sethostid(bhv_desc_t *, int);
extern void	 phost_gethostid(bhv_desc_t *, int *);
extern void	 phost_sethostname(bhv_desc_t *, char *, size_t *);
extern void	 phost_gethostname(bhv_desc_t *, char *, size_t *);
extern void	 phost_setdomainname(bhv_desc_t *, char *, size_t);
extern void	 phost_getdomainname(bhv_desc_t *, char *, size_t *);
extern int 	 phost_sysget(bhv_desc_t *, int, char *, int *, int,
			sgt_cookie_t *, sgt_info_t *);
extern int	 phost_systune(bhv_desc_t *, int, int, uint64_t);
extern void	 phost_credflush(bhv_desc_t *, cell_t);
extern void	 phost_sysacct(bhv_desc_t *, int, cfs_handle_t *);
extern void	 phost_set_time(bhv_desc_t *, cell_t);
extern void	 phost_adj_time(bhv_desc_t *, long, long *);

struct host_celldata;
extern void	dump_host_cellda(struct host_celldata *);
#endif	/* _KSYS_VHOST_H_ */
