/*
 * Handle metrics for ipc/msg (30)
 */

#ident "$Id: msg.c,v 1.14 1997/11/21 06:34:25 kenmcd Exp $"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmp.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <syslog.h>
#include "pmapi.h"
#include "impl.h"
#include "./cluster.h"

#if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4)
#include "./kmemread.h"
#else
#include <sys/sysget.h>
#endif

static pmMeta		meta[] = {
/* irix.ipc.msg.cbytes */
  { 0, { PMID(1,30,1), PM_TYPE_U32, PM_INDOM_MSG, PM_SEM_INSTANT, {1,0,0,PM_SPACE_BYTE,0,0} } },
/* irix.ipc.msg.qnum */
  { 0, { PMID(1,30,2), PM_TYPE_U32, PM_INDOM_MSG, PM_SEM_INSTANT, {0,0,1,0,0,PM_COUNT_ONE} } },
/* irix.ipc.msg.qbytes */
  { 0, { PMID(1,30,3), PM_TYPE_U32, PM_INDOM_MSG, PM_SEM_INSTANT, {1,0,0,PM_SPACE_BYTE,0,0} } },
};

static int		nmeta = (sizeof(meta)/sizeof(meta[0]));
static int		fetched = 0;
static int		msg_inst_n;
static int		msg_inst_max=8;
static struct msqid_ds	*msg_inst;
static int		*msgid_list;

#if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4)
static off_t		msginfo_addr;
static off_t		msgds_addr = (off_t)0;
#endif

/* exported to indom.c */
struct msqid_ds         **msgds;
struct msqid_ds         *msgds_instance;
struct msginfo          msginfo;

int
reload_msg(void)
{
    int i, count;
    struct msqid_ds *sp;
    static int first_time=1;

#if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4)
    int sts;
#else
    sgt_cookie_t ck;
#endif

    if (first_time) {

#if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4)

	if ((msginfo_addr = sysmp(MP_KERNADDR, MPKA_MSGINFO)) == -1 || !VALID_KMEM_ADDR(msginfo_addr)) {
	    __pmNotifyErr(LOG_ERR, "reload_msg: failed to get address of msginfo: %s\n", pmErrStr(-errno));
	    goto FAIL;
	}

	if ((sts = kmemread(msginfo_addr, &msginfo, (int)sizeof(msginfo))) != sizeof(msginfo)) {
	    __pmNotifyErr(LOG_ERR, "reload_msg: failed to read msginfo: %s\n", pmErrStr(-errno));
	    goto FAIL;
	}
#else /* IRIX6_5 */

	SGT_COOKIE_INIT(&ck);
	SGT_COOKIE_SET_KSYM(&ck, KSYM_MSGINFO);
	if (sysget(SGT_KSYM, (char *)&msginfo, (int)sizeof(msginfo), SGT_READ, &ck) < 0) {
	    __pmNotifyErr(LOG_ERR, "reload_msg: failed to read msginfo: %s\n", pmErrStr(-errno));
	    return -errno;
	}

#endif

	if ((msg_inst = (struct msqid_ds *)malloc(msg_inst_max * sizeof(struct msqid_ds))) == (struct msqid_ds *)NULL) {
	    __pmNotifyErr(LOG_ERR, "reload_msg: failed to malloc %d msqid_ds entries : %s\n", msg_inst_max, pmErrStr(-errno));
	    goto FAIL;
	}

	if ((msgid_list = (int *)malloc(msg_inst_max * sizeof(int))) == (int *)NULL) {
	    __pmNotifyErr(LOG_ERR, "reload_msg: failed to malloc %d ints for msgid list : %s\n", msg_inst_max, pmErrStr(-errno));
	    goto FAIL;
	}

	/*
	 * Allocate an array of pointers (actually kernel off_t's) for the 
	 * msg seg table. The msgid's are hashed into this table but only 
	 * the key_t is stored. So to get the id's we have to use 
	 * id = msgget(key, 0, 0).
	 */

#if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4)

	if (msgds_addr == (off_t)0 && (msgds_addr = sysmp(MP_KERNADDR, MPKA_MSG)) == -1 || !VALID_KMEM_ADDR(msgds_addr)) {
	    __pmNotifyErr(LOG_ERR, "reload_msg: failed to get address of msg: %s\n", pmErrStr(-errno));
	    return -errno;
	}

	if ((msgds = (struct msqid_ds **)malloc(msginfo.msgmni * sizeof(struct msqid_ds *))) == (struct msqid_ds**)0) {
	    __pmNotifyErr(LOG_ERR, "reload_msg: failed to malloc %d msg pointers: %s\n", msginfo.msgmni, pmErrStr(-errno));
	    goto FAIL;
	}

	if ((msgds_instance = (struct msqid_ds *)malloc(msginfo.msgmni * sizeof(struct msqid_ds))) == (struct msqid_ds *)0) {
	    __pmNotifyErr(LOG_ERR, "reload_msg: failed to malloc %d msg instances: %s\n", msginfo.msgmni, pmErrStr(-errno));
	    goto FAIL;
	}

#else /* IRIX6_5 */

	if ((msgds_instance = (struct msqid_ds *)calloc(msginfo.msgmni, sizeof(struct msqid_ds))) == (struct msqid_ds *)0) {
	    __pmNotifyErr(LOG_ERR, "reload_msg: failed to malloc %d msg instances: %s\n", msginfo.msgmni, pmErrStr(-errno));
	    goto FAIL;
	}

#endif

	first_time=0;
    }

#if defined(IRIX6_2) || defined(IRIX6_3) || defined(IRIX6_4)

    if (kmemread(msgds_addr, msgds, msginfo.msgmni * (int)sizeof(struct msqid_ds *)) < 0) {
	__pmNotifyErr(LOG_ERR, "reload_msg: failed to read msg seg table: %s\n", pmErrStr(-errno));
	return -errno;
    }

    for (count=i=0; i < msginfo.msgmni; i++) {
        if (msgds[i] != (struct msqid_ds *)0 && VALID_KMEM_ADDR((off_t)(msgds[i]))) {
            sp = &msgds_instance[i];
            sts = kmemread((off_t)msgds[i], sp, (int)sizeof(struct msqid_ds));
            if (sts < 0 || !(sp->msg_perm.mode & IPC_ALLOC)) {
                /*
                 * Invalid entry for one reason or another
                 * This flags that msgds_instance[i] is not a valid entry.
                 */
                msgds[i] = (struct msqid_ds *)0;
            }
            else {
                count++;
            }
        } else
            msgds[i] = (struct msqid_ds *)0;
    }

#else /* IRIX6_5 */

    SGT_COOKIE_INIT(&ck);
    if (sysget(SGT_MSGQUEUE, (char *)msgds_instance, msginfo.msgmni * (int)sizeof(struct msqid_ds), SGT_READ, &ck) < 0) {
	__pmNotifyErr(LOG_ERR, "reload_msg: failed to read msg seg table: %s\n", pmErrStr(-errno));
	return -errno;
    }

    for (count=i=0; i < msginfo.msgmni; i++) {
            sp = &msgds_instance[i];
            if (!(sp->msg_perm.mode & IPC_ALLOC)) {
                /*
                 * Invalid entry for one reason or another
                 * This flags that msgds_instance[i] is not a valid entry.
                 */
		continue;
            }
            else {
                count++;
            }
    }

#endif

    return count;

FAIL:
    return 0;
}


void msg_init(int reset)
{
    int	i;
    int	indomtag;		/* Constant from descr in form */

    if (reset)
	return;

    for (i = 0; i < nmeta; i++) {
	indomtag = meta[i].m_desc.indom;
	if (indomtag == PM_INDOM_NULL)
	    continue;
	if (indomtag < 0 || indomtag >= PM_INDOM_NEXT) {
	    __pmNotifyErr(LOG_ERR, "msg_init: bad instance domain (%d) for metric %s\n",
			 indomtag, pmIDStr(meta[i].m_desc.pmid));
	    continue;
	}
	/* Replace tag with it's indom */
	meta[i].m_desc.indom = indomtab[indomtag].i_indom;
    }

    return;
}

void msg_fetch_setup(void)
{
    indomtab[PM_INDOM_MSG].i_numinst = reload_msg();

    fetched = 0;
    msg_inst_n = 0;
}

int msg_desc(pmID pmid, pmDesc *desc)
{
    int		i;

    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {
	    *desc = meta[i].m_desc;	/* struct assignment */
	    return 0;
	}
    }
    return PM_ERR_PMID;
}

int msg_fetch(pmID pmid, __pmProfile *profp, pmVPCB *vpcb)
{
    int			i;
    int			j;
    int			k;
    int			e;
    int			id;
    int			sts;
    int			nval;
    int			profile_found;
    pmAtomValue		av;

    for (i = 0; i < nmeta; i++) {
	if (pmid == meta[i].m_desc.pmid) {
	    if (fetched == 0) {
		/*
		 * Extract the instance list (i.e. IPC keys) from the
		 * supplied profile.
		 * The profile MUST be an explicit inclusion list
		 * (i.e. exclude all, include some)
		 */

		for (profile_found=0, j=0; j < profp->profile_len; j++) {

		    if (profp->profile[j].indom != meta[i].m_desc.indom) {
			continue;
		    }

		    if (profp->profile[j].state != PM_PROFILE_EXCLUDE ||
			profp->profile[j].instances_len == 0)
			break;

		    profile_found++;
		    for (k=0; k < profp->profile[j].instances_len; k++) {
			/*
			 * Extract the requested msqid_ds instances.
			 * Ignore non-existent instances.
			 */
			id = profp->profile[j].instances[k];
			if ((sts = msgctl(id, IPC_STAT, &msg_inst[msg_inst_n])) < 0) {
			    __pmNotifyErr(LOG_ERR, "msg_fetch: msgctl(id=%d) : %s\n", id, pmErrStr(-errno));
			    continue;
			}

                        if (msg_inst_n >= msg_inst_max-1) {
                            /* get some more space! */
                            msg_inst_max += 8;
                            if ((msg_inst = (struct msqid_ds *)realloc(msg_inst,
                                msg_inst_max * sizeof(struct msqid_ds))) == (struct msqid_ds *)NULL) {
                                e = -errno;
                                __pmNotifyErr(LOG_ERR, "msg_fetch: failed to realloc %d msqid_ds entries : %s\n", msg_inst_max, pmErrStr(-errno));
                                return e;
                            }

                            if ((msgid_list = (int *)realloc(msgid_list,
                                msg_inst_max * sizeof(int))) == (int *)NULL) {
                                e = -errno;
                                __pmNotifyErr(LOG_ERR, "msg_fetch: failed to realloc %d ints for msgid list : %s\n", msg_inst_max, pmErrStr(-errno));
                                return e;
                            }
                        }

			msgid_list[msg_inst_n++] = id;
		    }
		}

		if (profile_found == 0) {
		    if (indomtab[PM_INDOM_MSG].i_numinst == 0)
			vpcb->p_nval = PM_ERR_VALUE;
		    else
			vpcb->p_nval = PM_ERR_PROFILE;
		    return 0;
		}
		fetched = 1;
	    }

	    vpcb->p_nval = msg_inst_n;
	    sizepool(vpcb);

	    nval = 0;
	    for (j=0; j < msg_inst_n; j++) {
		switch (pmid) {
		    case PMID(1,30,1):		/* irix.ipc.msg.cbytes */
			av.ul = (__uint32_t)msg_inst[j].msg_cbytes;
			break;
		    case PMID(1,30,2):		/* irix.ipc.msg.qnum */
			av.ul = (__uint32_t)msg_inst[j].msg_qnum;
			break;
		    case PMID(1,30,3):		/* irix.ipc.msg.qbytes */
			av.ul = (__uint32_t)msg_inst[j].msg_qbytes;
			break;
		    default:
			__pmNotifyErr(LOG_WARNING, "msg_fetch: no case for PMID %s\n", pmIDStr(pmid));
			vpcb->p_nval = 0;
			return PM_ERR_PMID;
		}

                if ((sts = __pmStuffValue(&av, 0, &vpcb->p_vp[nval], meta[i].m_desc.type)) < 0)
                    return sts;
                vpcb->p_vp[nval].inst = msgid_list[j];
                if (nval == 0)
                    vpcb->p_valfmt = sts;
                nval++;
	    }
	    return 0;
	}
    }
    return PM_ERR_PMID;
}
