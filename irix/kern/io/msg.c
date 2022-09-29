/* Copyright 1986, Silicon Graphics Inc., Mountain View, CA */
/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 3.65 $"
/*
**	Inter-Process Communication Message Facility.
*/

#include <limits.h>
#include <sys/types.h>
#include <sys/buf.h>
#include <sys/cred.h>
#include <sys/debug.h>
#include <sys/errno.h>
#include <sys/kipc.h>
#include <sys/ipc.h>
#include <sys/map.h>
#include <sys/param.h>
#include <ksys/vproc.h>
#include <sys/proc.h>
#include <sys/sema.h>
#include <sys/sysinfo.h>
#include <sys/sysmp.h>
#include <sys/sysmacros.h>
#include <sys/systm.h>
#include <sys/xlate.h>
#include <sys/kmem.h>
#include <sys/mac_label.h>
#include <sys/capability.h>
#include <sys/sat.h>
#include <sys/sysget.h>
#include <sys/msg.h>

struct msginfo	msginfo;	/* message parameters */
extern int msgmax;
extern int msgmnb;
extern int msgmni;
extern int msgssz;
extern int msgtql;
extern int msgseg;
extern time_t		time;		/* system idea of date */

static sv_t		msgflw;		/* waiting for a free msg structure */
struct msqid_ds		**msgque;	/* msg queue headers */
struct msgsem		**msgsem;	/* mutexes/sv_t for queue headers */
static struct msg	*volatile msgfp;/* ptr to head of free header list */
					/* bug 613859 warrants volatile */
static mutex_t		msgfl;		/* mutex for msgfp */
static mutex_t		msgmalloc_lock;	/* mutex for msgmalloc_cnt*/
static int		msgmalloc_cnt;  /* count of allocated msg text buffer */
static int		msgbsize;  	/* max msg txt size, > msgmalloc_cnt */
static int		msgh_cnt;	/* count of # of msg header allocated */
static sv_t		msgmalloc_wait;	/* wait for free text space */
static mutex_t		msgalloc_mutex;	/* new header allocate lock */
static struct zone 	*msg_zone;	/* zone for allocating msg structure */

static void msgfree(int, struct msqid_ds *, struct msg *, struct msg *);
static int msgsema(int, struct ipc_perm **, mutex_t **);

#if _MIPS_SIM == _ABI64
static int msqid_to_irix5(void *, int, xlate_info_t *);
static int irix5_to_msqid(enum xlate_mode, void *, int, xlate_info_t *);
#endif

/* Convert bytes to msg segments. */
#define	btoq(X)	((X + msginfo.msgssz - 1) / msginfo.msgssz)

/* macros to lock/unlock msgmalloc_lock */
#define MSGMALLOC_INIT()    mutex_init(&msgmalloc_lock, MUTEX_DEFAULT, \
				"msgmalloc_lock")
#define MSGMALLOC_LOCK(s)   mutex_lock(&msgmalloc_lock, PZERO)
#define MSGMALLOC_UNLOCK(s) mutex_unlock(&msgmalloc_lock)

/* macros to lock/unlock free header list */
#define MSGFL_INIT()		mutex_init(&msgfl, MUTEX_DEFAULT, "msgfl")
#define MSGFL_LOCK(ospl)	mutex_lock(&msgfl, PZERO)
#define MSGFL_UNLOCK(ospl)	mutex_unlock(&msgfl)

/* number of element to allocate when runs out of space */
#define MSGALLOC_CNT	5

#define COPYIN(syscall, from, to, size) \
		(syscall ? copyin(from, to, size) : (bcopy(from, to, size), 0))
#define COPYOUT(syscall, from, to, size) \
		(syscall ? copyout(from, to, size) : (bcopy(from, to, size), 0))

/*
**	msgconv - Convert a user supplied message queue id into a ptr to a
**		msqid_ds structure.
*/

int
msgconv(register int id, struct msqid_ds **mpp)
{
	register struct msqid_ds	*qp;	/* ptr to associated q slot */
	register mutex_t		*lockp;	/* ptr to lock.		*/
	register int			indx = id % msginfo.msgmni;

	if (id < 0)
		return(EINVAL);
	qp = msgque[indx];
	/*
	 * this identifier is not allocated yet, 
	 * so the id must be bogus
	 */
	if (!qp)
		return(EINVAL);
	if (_MAC_MSG_ACCESS(id % msginfo.msgmni, get_current_cred()))
		return(EACCES);
	lockp = &MSGADDR(indx)->msg_lock;
	mutex_lock(lockp, PZERO);
	if ((qp->msg_perm.mode & IPC_ALLOC) == 0 ||
		id / msginfo.msgmni != qp->msg_perm.seq) {
		mutex_unlock(lockp);
		return(EINVAL);
	}
	*mpp = qp;
	return(0);
}

/*
 * free up a message header
 */
static void
msgfp_free(struct msg *mp)
{
	/*REFERENCED*/
	mutex_t ospl;

	MSGFL_LOCK(ospl);
	mp->msg_next = msgfp;
	msgfp = mp;
	sv_signal(&msgflw);
	MSGFL_UNLOCK(ospl);
}


/* ARGSUSED */
int
msgctl(struct msgctla *uap, rval_t *rvp, int syscall)
{
	struct msqid_ds	ds;		/* queue work area */
	auto struct msqid_ds *qp;	/* ptr to associated q */
	register mutex_t *lockp;	/* ptr to assoc. mutex */
	register int indx = uap->msgid % msginfo.msgmni;
	struct ipc_perm *sat_save = NULL;
	int error;
	cred_t *crp = get_current_cred();

	_SAT_SET_SUBSYSNUM(MSGCTL | (uap->cmd << 2));

	if (error = msgconv(uap->msgid, &qp))
		return error;
	lockp = &MSGADDR(indx)->msg_lock;
	switch (uap->cmd) {
	case IPC_RMID:
		if (crp->cr_uid != qp->msg_perm.uid
			&& crp->cr_uid != qp->msg_perm.cuid
			&& !_CAP_ABLE(CAP_FOWNER)) {
			error = EPERM;
			break;
		}
		while (qp->msg_first)
			msgfree(indx, qp, NULL, qp->msg_first);
		qp->msg_cbytes = 0;
		/*
		 * For now, do not permit seq #'s > 32K - this makes
		 * things compatible with SVR3/IRIX4. It also
		 * means that we don't have to worry about SVR3/4 interopability
		 * and returning ERANGE/EOVERFLOW.
		 * The seq # manipulation will return error back from
		 * any further msgconv (msgrcv/msgsnd) on the old id.
		 */
		if (uap->msgid + msginfo.msgmni > SHRT_MAX)
			qp->msg_perm.seq = 0;
		else
			qp->msg_perm.seq++;
		sv_broadcast(&MSGADDR(indx)->msg_rwait);
		sv_broadcast(&MSGADDR(indx)->msg_wwait);
		qp->msg_perm.mode = 0;
		break;
	case IPC_SET:
		if (crp->cr_uid != qp->msg_perm.uid
			&& crp->cr_uid != qp->msg_perm.cuid
			 && !_CAP_ABLE(CAP_FOWNER)) {
			error = EPERM;
			break;
		}
		if (syscall) {
			error = COPYIN_XLATE(uap->buf, &ds, sizeof(ds),
				 irix5_to_msqid, get_current_abi(), 1);
		} else {
			error = COPYIN(0, qp, uap->buf, sizeof(*qp));
		}
		if (error) {
			error = EFAULT;
			break;
		}
		sat_save = _SAT_SVIPC_SAVE(&qp->msg_perm);
		if (ds.msg_qbytes > qp->msg_qbytes && !_CAP_ABLE(CAP_SVIPC_MGT)) {
			error = EPERM;
			break;
		}
		qp->msg_perm.uid = ds.msg_perm.uid;
		qp->msg_perm.gid = ds.msg_perm.gid;
		qp->msg_perm.mode = (qp->msg_perm.mode & ~0777) |
			(ds.msg_perm.mode & 0777);
		qp->msg_qbytes = ds.msg_qbytes;
		qp->msg_ctime = time;
		break;
	case IPC_STAT:
		if (error = ipcaccess(&qp->msg_perm, crp, MSG_R))
			break;
		if (syscall) {
			error = XLATE_COPYOUT(qp, uap->buf, sizeof(*qp),
				  msqid_to_irix5, get_current_abi(), 1);
		} else {
			error = COPYOUT(0, qp, uap->buf, sizeof(*qp));
		}
		if (error) {
			error = EFAULT;
		}
		break;
	default:
		error = EINVAL;
		break;
	}

	mutex_unlock(lockp);

	_SAT_SVIPC_CTL(uap->cmd, uap->msgid, &qp->msg_perm, sat_save, error);

	return error;
}

/*
**	msgfree - Free up space and message header, relink pointers on q,
**	and wakeup anyone waiting for resources.
*/

static void
msgfree(
register int			indx,	/* indx to msgque */
register struct msqid_ds	*qp,	/* ptr to q of mesg being freed */
register struct msg		*pmp,	/* ptr to mp's predecessor */
register struct msg		*mp)	/* ptr to msg being freed */
{
	/* Unlink message from the q. */
	if (pmp == NULL)
		qp->msg_first = mp->msg_next;
	else
		pmp->msg_next = mp->msg_next;
	if (mp->msg_next == NULL)
		qp->msg_last = pmp;
	qp->msg_qnum--;
	sv_broadcast(&MSGADDR(indx)->msg_wwait);

	/* Free up message text. */
	if (mp->msg_ts) {
		kern_free(mp->msg_spot);
		/* update cnt of allocated msg buffers */
		MSGMALLOC_LOCK(ospl);
		msgmalloc_cnt -= mp->msg_ts;
		mp->msg_ts = 0;
		/* wakeup those sleeping when the msg txt size was exceeded */
		sv_broadcast(&msgmalloc_wait);
		MSGMALLOC_UNLOCK(ospl);
	}

	/* Free up header */
	msgfp_free(mp);
}


int
msgget(struct msggeta *uap, rval_t *rvp)
{
	register struct msqid_ds	*qp;	/* ptr to associated q */
	int				s;	/* ipcget status return */
	int				indx;
	int error;
	cred_t *crp = get_current_cred();

	if (error = ipcget(uap->key, uap->msgflg, msgsema, &s, crp, &indx))
		return error;

	qp = msgque[indx];
	if (s) {
		/* This is a new queue.  Finish initialization. */
		ASSERT(sv_waitq(&MSGADDR(indx)->msg_rwait) == 0);
		ASSERT(sv_waitq(&MSGADDR(indx)->msg_wwait) == 0);
		qp->msg_first = qp->msg_last = NULL;
		qp->msg_qnum = 0;
		qp->msg_qbytes = msginfo.msgmnb;
		qp->msg_lspid = qp->msg_lrpid = 0;
		qp->msg_stime = qp->msg_rtime = 0;
		qp->msg_ctime = time;
		_MAC_MSG_SETLABEL(indx, crp->cr_mac);
	} else if (_MAC_MSG_ACCESS(indx, crp)) {
		error = EACCES;
		goto get_out;
	}

	rvp->r_val1 = qp->msg_perm.seq * msginfo.msgmni + indx;
get_out:
	mutex_unlock(&MSGADDR(indx)->msg_lock);
	if (s)
		_SAT_SVIPC_CREATE( uap->key, rvp->r_val1, uap->msgflg, error );
	return error;
}


/*
**	msginit - Called by main(main.c) to initialize message queues.
*/
void
msginit(void)
{
	int 			msgtotal;
	caddr_t 		kaddr;
	int 			sysget_msgq(int, char *, int *, int,
				 sgt_cookie_t *, sgt_info_t *);

	msginfo.msgmax = msgmax;
	msginfo.msgmnb = msgmnb;
	msginfo.msgmni = msgmni;
	msginfo.msgssz = msgssz;
	msginfo.msgtql = msgtql;
	msginfo.msgseg = (msgseg & 0x0000ffff);
	msginfo.msgseg_h= (msgseg & 0xffff0000);

	/* Allocate physical memory for message buffer. */
	msgtotal = (sizeof(int *)  * msginfo.msgmni) +
		(sizeof(int *) * msginfo.msgmni);
	kaddr = kern_calloc(1,msgtotal);

	/* allocate space for msgque table */
	msgque = (struct msqid_ds **) kaddr;
	kaddr += sizeof(int *) * msginfo.msgmni;
	/* allocate space for msgsem table */
	msgsem = (struct msgsem **) kaddr;
	/* max msg text buffer size */
	msgbsize = (msgseg * msginfo.msgssz);

	MSGFL_INIT();
	sv_init(&msgflw, SV_DEFAULT, "msgflw");
	sysmp_mpka[MPKA_MSG - 1] = (caddr_t)msgque;
	sysmp_mpka[MPKA_MSGINFO - 1] = (caddr_t)&msginfo;
	sysget_attr[SGT_MSGQUEUE - 1].func = sysget_msgq;
	sysget_attr[SGT_MSGQUEUE - 1].size = sizeof(struct msqid_ds);
	ksym_add(KSYM_MSGINFO, &msginfo, sizeof(msginfo), 1, SA_GLOBAL);
	/* lock to maintain msgmalloc_cnt */
	MSGMALLOC_INIT();
	/* sv_t to sleep when over limit of msg text size */
	sv_init(&msgmalloc_wait, SV_DEFAULT, "msgmal_sem");
	msg_zone = kmem_zone_init(sizeof(struct msg), "msg");
	_MAC_MSG_INIT(msginfo.msgmni);
	/* mutex to protect header allocations */
	mutex_init(&msgalloc_mutex, MUTEX_DEFAULT, "msgalloc");
}



int
msgrcv(register struct msgrcva *uap, rval_t *rvp, int syscall)
{
	register struct msg		*mp,	/* ptr to msg on q */
					*pmp,	/* ptr to mp's predecessor */
					*smp,	/* ptr to best msg on q */
					*spmp;	/* ptr to smp's predecessor */
	auto struct msqid_ds		*qp;	/* ptr to associated q */
	register mutex_t		*lockp;	/* q mutex ptr */
	auto struct msqid_ds		*qp1;
	int				sz;	/* transfer byte count */
	int				again;	/* in case of reacquire */
	int				indx = uap->msqid % msginfo.msgmni;
	int				error;

	SYSINFO.msg++;		/* bump message send/rcv count */
	if (error = msgconv(uap->msqid, &qp))
		return error;
	lockp = &MSGADDR(indx)->msg_lock;
	if (error = ipcaccess(&qp->msg_perm, get_current_cred(), MSG_R))
		goto msgrcv_out;
	if (uap->msgsz < 0) {
		error = EINVAL;
		goto msgrcv_out;
	}
	again = 0;
findmsg:
	if (again) {
		if (msgconv(uap->msqid, &qp1))
			return EIDRM;
		ASSERT(qp1 == qp);
	}
	again = 1;
	smp = spmp = NULL;
	pmp = NULL;
	if (uap->msgtyp == 0)
		smp = qp->msg_first;
	else if (uap->msgtyp > 0)
		for (mp = qp->msg_first;mp;pmp = mp, mp = mp->msg_next) {
			if (uap->msgtyp != mp->msg_type)
				continue;
			smp = mp;
			spmp = pmp;
			break;
		}
	else
		for (mp = qp->msg_first;mp;pmp = mp, mp = mp->msg_next) {
			if (mp->msg_type <= -uap->msgtyp) {
				if (smp && smp->msg_type <= mp->msg_type)
					continue;
				smp = mp;
				spmp = pmp;
			}
		}
	if (smp) {
		if (uap->msgsz < smp->msg_ts)
			if (!(uap->msgflg & MSG_NOERROR)) {
				error = E2BIG;
				goto msgrcv_out;
			} else
				sz = uap->msgsz;
		else
			sz = smp->msg_ts;
#if _MIPS_SIM == _ABI64
		if ((!ABI_IS_IRIX5_64(get_current_abi())) && (syscall)) {
			app32_long_t i5_type = smp->msg_type;
			if (copyout(&i5_type, uap->msgp, sizeof(i5_type))) {
				error = EFAULT;
				goto msgrcv_out;
			}
			if (sz && copyout(smp->msg_spot, 
					  (caddr_t)uap->msgp + sizeof(i5_type),
					  sz)) {
				error = EFAULT;
				goto msgrcv_out;
			}
		} else
#endif
		{
			if (COPYOUT(syscall, &smp->msg_type, uap->msgp,
				    sizeof(smp->msg_type))) {
				error = EFAULT;
				goto msgrcv_out;
			}
			if (sz && COPYOUT(syscall, smp->msg_spot, 
					(caddr_t)uap->msgp +
					sizeof(smp->msg_type),
					sz)) {
				error = EFAULT;
				goto msgrcv_out;
			}
		}

		rvp->r_val1 = sz;
		qp->msg_cbytes -= smp->msg_ts;
		qp->msg_lrpid = current_pid();
		qp->msg_rtime = time;
		msgfree(indx, qp, spmp, smp);
		goto msgrcv_out;
	}
	if (uap->msgflg & IPC_NOWAIT) {
		error = ENOMSG;
		goto msgrcv_out;
	}
	if (sv_wait_sig(&MSGADDR(indx)->msg_rwait, PMSG, lockp, 0) == -1)
		return EINTR;
	/* either have mutex or got a cancelled signal */
	goto findmsg;

msgrcv_out:
	mutex_unlock(lockp);
	return(error);
}


/* ARGSUSED */
int
msgsnd(struct msgsnda *uap, rval_t *rvp, int syscall)
{
	auto	 struct msqid_ds	*qp;	/* ptr to associated q */
	register mutex_t		*lockp;	/* q mutex ptrs */
	register struct msg		*mp;	/* ptr to allocated msg hdr */
	register int			cnt;	/* byte count */
	register int			ospl = 0;/* spl save area */
	auto struct msqid_ds		*qp1;
	long				type;	/* msg type */
	int				again;	/* in case of reacquire */
	caddr_t				spot;	/* kern addr of msg text */
	int				indx = uap->msqid % msginfo.msgmni;
	int				error;
#if _MIPS_SIM == _ABI64
	app32_long_t i5_type;
	int sztype = (ABI_IS_IRIX5_64(get_current_abi()) || (!syscall)) ?
				sizeof(type) : sizeof(i5_type);
#else
	int sztype = sizeof(type);
#endif

	SYSINFO.msg++;		/* bump message send/rcv count */
	if (error = msgconv(uap->msqid, &qp))
		return (error);
	lockp = &MSGADDR(indx)->msg_lock;
	if (error = ipcaccess(&qp->msg_perm, get_current_cred(), MSG_W))
		goto msgsnd_out;
	if ((cnt = uap->msgsz) < 0 || cnt > msginfo.msgmax) {
		error = EINVAL;
		goto msgsnd_out;
	}
#if _MIPS_SIM == _ABI64
	if ((!ABI_IS_IRIX5_64(get_current_abi())) && (syscall)) {
		if (copyin(uap->msgp, &i5_type, sizeof(i5_type))) {
			error = EFAULT;
			goto msgsnd_out;
		}
		type = i5_type;
	} else
#endif
	if (COPYIN(syscall, uap->msgp, &type, sizeof(type))) {
		error = EFAULT;
		goto msgsnd_out;
	}
	if (type < 1) {
		error = EINVAL;
		goto msgsnd_out;
	}
	again = 0;
getres:
	if (again) {
		/* Be sure that q has not been removed. */
		if (msgconv(uap->msqid, &qp1))
			return EIDRM;
		ASSERT(qp1 == qp);
	}
	again = 1;

	/* Allocate space on q, message header, & buffer space. */
	if (cnt > (qp->msg_qbytes - qp->msg_cbytes)) {
		if (uap->msgflg & IPC_NOWAIT) {
			error = EAGAIN;
			goto msgsnd_out;
		}
		if (sv_wait_sig(&MSGADDR(indx)->msg_wwait, PMSG, lockp, 0)
				== -1)
			return EINTR;
		goto getres;
	}
	MSGFL_LOCK(ospl);
	if (msgfp == NULL) {
		/* 
		** if not already at the limit on # of msg header then
		** allocate some more
		*/
		if (msgh_cnt < msginfo.msgtql) {
			MSGFL_UNLOCK(ospl);
			mp = (struct msg *)kmem_zone_zalloc(msg_zone, KM_SLEEP);
			MSGFL_LOCK(ospl);
			/*
			 * seems inefficient, but right now the emphasis
			 * is on space saving	
			 */
			if (msgfp != NULL)   
				kmem_zone_free(msg_zone, mp);
			else {
				msgfp = mp;
				msgh_cnt++;
			}
		} else {	
			if (uap->msgflg & IPC_NOWAIT) {
				MSGFL_UNLOCK(ospl);
				error = EAGAIN;
				goto msgsnd_out;
			}
			mutex_unlock(lockp);
			error = sv_wait_sig(&msgflw, PMSG, &msgfl, ospl);
			if (error == -1)
				return EINTR; /* got a cancelled signal */
			goto getres;
		}
	}
	mp = msgfp;
	msgfp = mp->msg_next;
	MSGFL_UNLOCK(ospl);

	if (cnt) {
		/* check to make sure not over the limit on msg text size */
		MSGMALLOC_LOCK(ospl);
		if (cnt > (msgbsize - msgmalloc_cnt)) {
			MSGMALLOC_UNLOCK(ospl);
			/*
			 * Not enough room for message text - release message
			 * header and wait
			 */
			msgfp_free(mp);

			if (uap->msgflg & IPC_NOWAIT) {
				error = EAGAIN;
				goto msgsnd_out;
			}
			mutex_unlock(lockp);
			/* sleep until someone frees up some msg text */
			MSGMALLOC_LOCK(ospl);
			if (mp_sv_wait_sig(&msgmalloc_wait, PMSG,
					&msgmalloc_lock, ospl) == -1)
				return EINTR;
			goto getres;
		} else {
			msgmalloc_cnt += cnt;
			MSGMALLOC_UNLOCK(ospl);
			spot = kern_calloc(1,cnt);
		}


		/* Everything is available, copy in text and put msg on q. */
		if (COPYIN(syscall, (caddr_t)uap->msgp + sztype, spot, cnt)) {
			error = EFAULT;
			/*
			 * sigh - release msg text and message header
			 */
			MSGMALLOC_LOCK(ospl);
			msgmalloc_cnt -= cnt;
			/*
			 * wakeup those sleeping when the msg txt size
			 * was exceeded
			 */
			sv_broadcast(&msgmalloc_wait);
			MSGMALLOC_UNLOCK(ospl);
			kern_free(spot);

			msgfp_free(mp);
			goto msgsnd_out;
		}
		qp->msg_cbytes += cnt;
		mp->msg_spot = spot;
	} else {
		mp->msg_spot = (caddr_t)-1L;
	}
	qp->msg_qnum++;
	qp->msg_lspid = current_pid();
	qp->msg_stime = time;
	mp->msg_next = NULL;
	mp->msg_type = type;
	mp->msg_ts = cnt;
	if (qp->msg_last == NULL)
		qp->msg_first = qp->msg_last = mp;
	else {
		qp->msg_last->msg_next = mp;
		qp->msg_last = mp;
	}
	sv_broadcast(&MSGADDR(indx)->msg_rwait);

msgsnd_out:
	mutex_unlock(lockp);
	return(error);
}

/*
 * routine to return info to ipcget
 */
static int
msgsema(int i, struct ipc_perm **ip, mutex_t **mp)
{
	if (i < 0 || i >= msginfo.msgmni) {
		return(0);
	}
	/* this entry hasn't been allocated and is needed now */
	if (!msgque[i]) {	
		register int nalloc, j, fooi;
		register caddr_t kaddr;
		register struct msgsem *msp;

		mutex_lock(&msgalloc_mutex, PZERO);
		if (msgque[i]) {
			mutex_unlock(&msgalloc_mutex);
			goto done;
		}

		nalloc = MIN(msginfo.msgmni-i, MSGALLOC_CNT);
		kaddr = kern_calloc(nalloc, sizeof(struct msgsem));
		for (fooi = i, j = nalloc; j--; fooi++) {
			msp = msgsem[fooi] = (struct msgsem *)kaddr;
			kaddr += sizeof(struct msgsem);
			init_mutex(&msp->msg_lock, MUTEX_DEFAULT,
				"msglock", fooi);
			init_sv(&msp->msg_rwait, SV_DEFAULT, "msgrwt", fooi);
			init_sv(&msp->msg_wwait, SV_DEFAULT, "msgwwt", fooi);
		}

		/* alloc msgque now that msgsem is initialized */
		kaddr = kern_calloc(nalloc, sizeof(struct msqid_ds));
		for (fooi = i, j = nalloc; j--; fooi++) {
			msgque[fooi] = (struct msqid_ds *)kaddr;
			kaddr += sizeof(struct msqid_ds);
		}
		mutex_unlock(&msgalloc_mutex);
	}
		
done:
	*ip = &(msgque[i]->msg_perm);
	*mp = &(msgsem[i]->msg_lock);
	return(1);
}

/*
 * msgsys - System entry point for msgctl, msgget, msgrcv, and msgsnd
 * system calls. This is also the entry point for kernel level msging
 * primitives. r_val1 is checked to determine if the call is from the
 * regular syscall path or from kernel msging path (used in 3rd party
 * UniCenter software).
 */

int
msgsys(struct msgsysa *uap, rval_t *rvp)
{
	register int error;
	int syscall = (rvp->r_val1 == 0);

	_SAT_SET_SUBSYSNUM(uap->opcode);

	switch (uap->opcode) {
	case MSGGET:
		error = msgget((struct msggeta *)uap, rvp);
		break;
	case MSGCTL:
		error = msgctl((struct msgctla *)uap, rvp, syscall);
		break;
	case MSGRCV:
		error = msgrcv((struct msgrcva *)uap, rvp, syscall);
		break;
	case MSGSND:
		error = msgsnd((struct msgsnda *)uap, rvp, syscall);
		break;
	default:
		error = EINVAL;
		break;
	}

	return error;
}

#if _MIPS_SIM == _ABI64
/*ARGSUSED*/
static int
msqid_to_irix5(
	void *from,
	int count,
	register xlate_info_t *info)
{
	register struct irix5_msqid_ds *i5_ds;
	register struct msqid_ds *ds = from;

	ASSERT(count == 1);
	ASSERT(info->smallbuf != NULL);

	if (sizeof(struct irix5_msqid_ds) <= info->inbufsize)
		info->copybuf = info->smallbuf;
	else
		info->copybuf = kern_malloc(sizeof(struct irix5_msqid_ds));
	info->copysize = sizeof(struct irix5_msqid_ds);

	i5_ds = info->copybuf;

	ipc_perm_to_irix5(&ds->msg_perm, &i5_ds->msg_perm);
	i5_ds->msg_first = (app32_ptr_t)(__psint_t)ds->msg_first;
	i5_ds->msg_last = (app32_ptr_t)(__psint_t)ds->msg_last;
	i5_ds->msg_cbytes = ds->msg_cbytes;
	i5_ds->msg_qnum = ds->msg_qnum;
	i5_ds->msg_qbytes = ds->msg_qbytes;
	i5_ds->msg_lspid = ds->msg_lspid;
	i5_ds->msg_lrpid = ds->msg_lrpid;
	i5_ds->msg_stime = ds->msg_stime;
	i5_ds->msg_rtime = ds->msg_rtime;
	i5_ds->msg_ctime = ds->msg_ctime;

	return 0;
}

/*ARGSUSED*/
static int
irix5_to_msqid(
	enum xlate_mode mode,
	void *to,
	int count,
	xlate_info_t *info)
{
	register struct msqid_ds *ds;
	register struct irix5_msqid_ds *i5_ds;

	ASSERT(info->smallbuf != NULL);
	ASSERT(mode == SETUP_BUFFER || mode == DO_XLATE);

	if (mode == SETUP_BUFFER) {
		ASSERT(info->copybuf == NULL);
		ASSERT(info->copysize == 0);
		if (sizeof(struct irix5_msqid_ds) <= info->inbufsize)
			info->copybuf = info->smallbuf;
		else
			info->copybuf = kern_malloc(
					    sizeof(struct irix5_msqid_ds));
		info->copysize = sizeof(struct irix5_msqid_ds);
		return 0;
	}

	ASSERT(info->copysize == sizeof(struct irix5_msqid_ds));
	ASSERT(info->copybuf != NULL);

	ds = to;
	i5_ds = info->copybuf;

	irix5_to_ipc_perm(&i5_ds->msg_perm, &ds->msg_perm);
	ds->msg_first = (void *)(__psint_t)i5_ds->msg_first;
	ds->msg_last = (void *)(__psint_t)i5_ds->msg_last;
	ds->msg_cbytes = i5_ds->msg_last;
	ds->msg_qnum = i5_ds->msg_qnum;
	ds->msg_qbytes = i5_ds->msg_qbytes;
	ds->msg_lspid = i5_ds->msg_lspid;
	ds->msg_lrpid = i5_ds->msg_lrpid;
	ds->msg_stime = i5_ds->msg_stime;
	ds->msg_rtime = i5_ds->msg_rtime;
	ds->msg_ctime = i5_ds->msg_ctime;

	return 0;
}
#endif

/* sysget routine to return the msgque list.  This list is assumed to be global
 * to the system so the entire list can be returned from any cell without
 * iterating thru VHOST.
 */
int
sysget_msgq(	int name,
		char *buf,
		int *buflen_p,
		int flags,
		sgt_cookie_t *cookie_p,
		sgt_info_t *info_p)
{
	int error = 0;
	int start = 0;
	sgt_attr_t *ap = &sysget_attr[name - 1];
	caddr_t ubuf;
	int i, ulen, copied;
	sgt_cookie_genop_t *ckop = SGT_COOKIE_GENOP(cookie_p);

	/* process request for this cell */

	if (cookie_p->sc_status == SC_CONTINUE) {

		/* Cookie tells us where they left off */

		start = ckop->index + 1;
		if (start < 0 || start > msginfo.msgmni) {
			return(EINVAL);
		}
	}

	switch (flags & (SGT_READ | SGT_WRITE | SGT_INFO | SGT_STAT)) {
	case SGT_INFO:

		/* accumulate size info into the info_p struct */

		info_p->si_size = ap->size;
		info_p->si_num = msginfo.msgmni;
		info_p->si_hiwater = info_p->si_num;
		ulen = sizeof(sgt_info_t);
		break;
	case SGT_READ:

		copied = 0;
		ubuf = buf;
		ulen = MIN(*buflen_p, ap->size);

		mutex_lock(&msgalloc_mutex, PZERO);
		for (i = start; i < msginfo.msgmni; i++) {
			if (!ulen) {
				/* No buffer left */
				break;
			}
			if (!msgque[i]) {
				continue;
			}

			/* Copy out the msgque entry. */

			if (copyout(msgque[i], ubuf, ulen)) {
				error = EFAULT;
				break;
			}
			copied += ulen;
			ubuf = &buf[copied];
			ulen = MIN((*buflen_p - copied), ap->size);
		}
		mutex_unlock(&msgalloc_mutex);
		break;
	default:
		error = EINVAL;
	}

	if (copied && i < msginfo.msgmni) {

		/* User's buffer wasn't big enough so return a
		 * cookie that will allow the call to re-start
		 * where it left off.
		 */

		cookie_p->sc_status = SC_CONTINUE;
		ckop->curcell = cellid();
		ckop->index = i;
	}
	else {
		cookie_p->sc_status = SC_DONE;
		ckop->index = -1;
	}

	*buflen_p = copied;
	return(error);
}
