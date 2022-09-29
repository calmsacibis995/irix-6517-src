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

#ifndef	__SYS_UNCINTF_H
#define	__SYS_UNCINTF_H
#ifdef __cplusplus
extern "C" {
#endif

#ifdef _KERNEL

#include        <sys/types.h>
#include        <sys/systm.h>
#include        <sys/msg.h>
#include        <sys/cmn_err.h>
#include        <sys/syscall.h>
#include        <sys/driver.h>
#include        <sys/conf.h>
#include        <sys/vnode.h>
#include        <sys/ddi.h>
#include        <sys/cred.h>
#include        <sys/errno.h>
#include        <sys/mtio.h>
#include        <sys/tpsc.h>
#include        <sys/scsi.h>
#include        <sys/proc.h>
#include	<sys/file.h>
#include	<sys/prctl.h>

#define         NOZOMB          0	/* pidstats parm - don't do zombs   */
#define         DOZOMB          1	/* pidstats parm - return zomb info */

#define		PARENTPID	0x00000001	/* returned always      */
#define		CONTROLDEV	0x00000002
#define		NICEVAL		0x00000004	/* returned always      */
#define		PGRPID		0x00000008	/* returned always      */
#define		USERID		0x00000010	/* real/eff/saved uids  */
#define		SLPCHAN		0x00000020	/* returned always      */
#define		STARTTIME	0x00000040
#define		PRIORITY	0x00000080	/* same as ps info      */
#define		LASTCPU		0x00000100	/* returned always      */
#define		PRSS		0x00000200
#define		PNAME		0x00000400	/* comm and args        */
#define		PSIZE		0x00001000	/* stack and total size */
#define		USTIME		0x00002000	/* user and sys time    */
#define		CID		0x00002000	/* cell id    		*/

typedef struct pidparm_s {
	pid_t   	parentpid;
	gid_t		pgrpid;
	uid_t		effuid;
	uid_t		saveuid;
	uid_t		realuid;
	dev_t		controldev;
	caddr_t		slpchan;
	char		niceval;
	long		priority;
	cpuid_t		lastcpu;
	pgno_t		prss;
	char		procname[PSCOMSIZ];
	char		procargs[PSARGSZ];
	size_t		stacksz;
	pgno_t		totsz;
	struct timeval	starttime;
	struct timeval	utime;
	struct timeval	stime;
	cell_t		cellid;
	void		*pad;
} pidparm_t;

extern int	tpscstat(vnode_t *, void *, int);
extern int	isa_scsi_tape(vnode_t *, int *);
extern void	*syscall_intercept(int sysnum, sy_call_t intercept_function);
extern void	syscall_restore(int sysnum, void *);
extern int	syscall_continue(void *, sysarg_t *, void *, uint);
extern dev_t	getcttydev(pid_t tpid);
extern int	fdt_getnum(void);
extern pid_t	getparent(pid_t cpid);
extern int 	kmsgget(key_t key, int msgflg, int *errno);
extern int 	kmsgctl(int msqid, int cmd, struct msqid_ds *buf, int *errno);
extern int 	kmsgsnd(int msqid, void *msgp, size_t msgsz, int msgflg, 
						int *errno);
extern int 	kmsgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp, 
						int msgflg, int *errno);
extern int	pidstats(pid_t tpid, pidparm_t *buf, uint_t mask, int dozombie);

#endif /* _KERNEL */

#ifdef __cplusplus
}
#endif
#endif	/* !__SYS_UNCINTF_H */
