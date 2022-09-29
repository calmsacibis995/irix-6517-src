/*
 *  dsglue.c -- /dev/scsi driver glue layer for SGI
 *
 *	Copyright 1988, 1989, by
 *	Gene Dronek (Vulcan Laboratory) and
 *	Rich Morin  (Canta Forda Computer Laboratory).
 *	All Rights Reserved.
 *  	Modified by Dave Olson for SGI systems 3/89
 *
 *      This module mediates between the generic interface and
 * 	the host OS scsi driver.  This module shouldn't have to know
 * 	ANYTHING about stuff passed by the user.  It should get
 *      addresses that have already been validated and locked down,
 *	etc...
 */

#ident "io/dsglue.c: $Revision: 1.61 $"

/* stuff to support other include files */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysmacros.h>
#include <sys/sema.h>
#include <sys/sbd.h>
#include <sys/pda.h>
#include <sys/kthread.h>

#include <sys/buf.h>
#include <sys/errno.h>
#include <sys/kmem.h>		/* for kmem_alloc() args */
#include <sys/debug.h>

#include <sys/scsi.h>		/* host scsi include files */
#include <sys/cmn_err.h>

#include <sys/dsreq.h>		/* devscsi include file */
#include <sys/dsglue.h>		/* task to glue interface stuff */

#ifdef DEBUG
#define __DSRQ_DEBUG   (DSRQ_TRACE | DSRQ_PRINT)
#else
#define __DSRQ_DEBUG   0
#endif
uint glue_support =    /* flags supported */
  (DSRQ_SENSE | DSRQ_IOV   | DSRQ_READ   | DSRQ_WRITE | __DSRQ_DEBUG |
   DSRQ_CTRL1 | DSRQ_CTRL2 | DSRQ_SYNXFR | DSRQ_ASYNXFR);
uint glue_default = (DSRQ_DISC | DSRQ_SELATN);


/* translate host adapter (scsi.h) errrors to devscsi errors */
static u_short
hosterr_to_ds(uint srst)
{
	switch(srst) {
	case SC_TIMEOUT:
		return DSRT_TIMEOUT;
	case SC_CMDTIME:
		return DSRT_TIMEOUT;
	case SC_HARDERR:
		return DSRT_AGAIN;
	case SC_PARITY:
		return DSRT_PARITY;
	case SC_MEMERR:
		return DSRT_MEMORY;
	default:
		DBG(printf("hosterr status %d to DSRT_HOST\n"));
		return DSRT_HOST;
	}
}


/* callback function from host scsi driver */
static void
glue_callback(struct scsi_request *req)
{
	struct task_desc *tp;
	tp = (struct task_desc *) req->sr_dev;

	if(tp->td_state == TDS_WAITASYNC) {
		/* release the locked down pages now, in case no one ever waits
			for this i/o */
		dma_unlock(&tp->td_iovbuf[0], (long) tp->td_buf.b_bcount,
			((struct buf *) (req->sr_bp))->b_flags & B_READ);
	}
	tp->td_state = TDS_INTERIM;
	vsema(&tp->td_done);
}


int
glue_reserve(struct task_desc *tp, dev_t dev)
{
	int num, flags;
	struct scsi_request *req;

	atomicAddUint(&tp->td_refcount, 1);
	if (tp->td_refcount > 1) {
		atomicAddUint(&tp->td_refcount, (uint)-1);
		return EBUSY;
	}

	ASSERT(tp->td_refcount == 1);
	ASSERT(tp->td_hsreq == NULL);

	flags = (tp->td_iflags & TASK_EXCLUSIVE) ? (SCSIALLOC_EXCLUSIVE|1) : 1;

	/*
	 * Temporarily store result in hsreq in case the other
	 * driver using it is actually this driver.
	 * ds always starts out with no scsi sync negotiations
	 * enabled, because there are so many 'bad' SCSI devices
	 * out there.  This is also consistent with past releases
	 * (pre 5.x).  When DS_SYNC is seen on a cmd request, the
	 * dsreq code will do what is necessary to turn on sync,
	 * once per open.  Still no way to turn *off* sync for ds,
	 * but since we now remember everything in the low level
	 * driver across opens, this shouldn't be a problem any more.
	 */
	if((num=(SDEVI_ALLOC(tp->td_dev_vhdl))(tp->td_lun_vhdl,
					       SCSIALLOC_NOSYNC|flags, 
					       NULL)) 
	    != SCSIALLOCOK)
	{
		atomicAddUint(&tp->td_refcount, (uint)-1);
	        return num ? num : ENODEV;
	}

	/*
	 * Initialize data structures.
	 * Initialize the semaphore stuff for physio reads/writes, etc.
	 */
	tp->td_hsreq = kmem_alloc(sizeof(struct scsi_request), KM_SLEEP);
	ASSERT(tp->td_hsreq != NULL);
	bzero(tp->td_hsreq, sizeof(struct scsi_request));
	tp->td_buf.b_edev = dev;
	init_sema(&tp->td_buf.b_lock, 1, "dslock", dev);
	init_sema(&tp->td_buf.b_iodonesema, 1, "dsiodone", dev);
	init_sema(&tp->td_done, 0, "dsdone", dev);

	req = (struct scsi_request *) tp->td_hsreq;
	req->sr_ctlr = tp->td_bus;
	req->sr_target = tp->td_id;
	req->sr_lun = tp->td_lun;
	req->sr_lun_vhdl = tp->td_lun_vhdl;
	req->sr_dev_vhdl = tp->td_dev_vhdl;
	req->sr_sense = tp->td_sensebuf;
	req->sr_senselen = TDS_SENSELEN;
	req->sr_notify = glue_callback;

	DBG(printf("reserve OK.\n"));
	return 0;
}


int
glue_release(struct task_desc  *tp)
{
	if (tp->td_refcount > 0) {
		DBG(printf("glue_release: rel subchan. "));
		freesema(&tp->td_buf.b_iodonesema);
		freesema(&tp->td_buf.b_lock);
		freesema(&tp->td_done);
		kern_free(tp->td_hsreq);
		tp->td_hsreq = NULL;
		(SDEVI_FREE(tp->td_dev_vhdl))(tp->td_lun_vhdl, NULL);
		atomicAddUint(&tp->td_refcount, (uint)-1);
	}
	else
		DBG(printf("glue_release: tp->td_refcount == 0. "));
	return 0;
}


/*
 * glue: task cross in
 *
 * returns 0 if no errors; value in td_ret on ds errors,
 * and a non-zero error for 'unix' errors.
 */
int
glue_start(struct task_desc *task, int flags)
{
	register struct scsi_request *req =
		(struct scsi_request *) task->td_hsreq;
	buf_t *bp = &task->td_buf;

	DBG(printf("glue_start(%x, %x). ",task, flags));

	task->td_status = task->td_ret = 0;

	if(task->td_iflags & TASK_ABORT) {
		DBG(printf("sending abort to scsi bus %d on user request. ", task->td_bus));
		/* returns 1 on success; 0 on failure; code in sr_status */
		if((SDEVI_ABORT(task->td_dev_vhdl))(req) == 0) {
			task->td_ret = hosterr_to_ds(req->sr_status);
			return 1;
		}
		return 0;
	}

	task->td_senselen = 0;

	/* translate devscsi flags */
	if(flags & DSRQ_READ) {
		bp->b_flags = B_READ | B_BUSY;
		req->sr_flags = SRF_DIR_IN;
	}
	else {
		bp->b_flags = B_BUSY;
		req->sr_flags = 0;
	}

	if(flags & DSRQ_SYNXFR)	/* attempt to negotiate sync */
		req->sr_flags |= SRF_NEG_SYNC;
	else if(flags & DSRQ_ASYNXFR)	/* attempt to negotiate async
		* be paranoid; ignore this if SYNXFR also set */
		req->sr_flags |= SRF_NEG_ASYNC;

	/*
	 * We don't allow a user program to disable system interrupts
	 * (DSRQ_NOINTR); there should NEVER be a reason for this, as
	 * long as exclusive access to devices is implented.
	 * TRACE and PRINT aren't implemented in scsi.c either.
	 * SELAIO is ignore also, we always try to select with atn;
	 * it's up to the peripheral whether it chooses to support disconnect.
	 */
	bp->b_bcount = task->td_iovbuf[0].iov_len; /* total bytes */
	if(bp->b_bcount) {
		if(!(task->td_iflags & TASK_INTERNAL)) {
			bp->b_un.b_addr = task->td_iovbuf[0].iov_base;
			ASSERT((curthreadp != NULL) && !(KT_CUR_ISXTHREAD()));

			/* convert to kernel addresses */
			if(iomap(bp))
				return EAGAIN;
		}
		else /* generated within driver; everything taken care of */
			bp->b_dmaaddr = task->td_iovbuf[0].iov_base;
	}

	req->sr_flags |= SRF_MAP | SRF_AEN_ACK;
	req->sr_buffer = (u_char *)bp->b_dmaaddr;
	req->sr_buflen = bp->b_bcount;
	req->sr_command = task->td_cmdbuf;
	req->sr_cmdlen = task->td_cmdlen;
	req->sr_bp = bp;
	req->sr_dev = (caddr_t) task;	/* for callback routine */
	/* td_time is given in ms */
	req->sr_timeout = (999 + task->td_time * HZ) / 1000;

	task->td_state = TDS_RUN;
	(SDEVI_COMMAND(task->td_dev_vhdl))(req);
	return 0;	/* accepted OK */
}


/*
 * task cross out on completion
 *
 * Pass task back to devscsi
 *  recover task ptr
 *  pass back req ret, status, msg
 *  set state to DONE
 *  called from devscsi layer
 *	returns a value in td_ret on ds errors,
 */
void
glue_return(register struct task_desc *task, int flags)
{
	register struct scsi_request *req =
		(struct scsi_request *) task->td_hsreq;

	DBG(printf("glue_return(0x%x, flags=%x). ",task, flags));

	/*
	 * We must make sure that td_status is set in case of error,
	 * since a good td_status indicates that td_ret is good also.
	 */
	if(req->sr_status == SC_GOOD) {
		if (req->sr_sensegotten > 0) {
			if (flags & DSRQ_SENSE) {
				task->td_ret = DSRT_SENSE;
				task->td_senselen = req->sr_sensegotten;
			}
			else
				task->td_ret = DSRT_HOST;
			task->td_status = ST_CHECK;
		}
		else if (req->sr_sensegotten == -1) {
			task->td_ret = DSRT_NOSENSE;
			task->td_status = 0xff;
		}
		else {
			if (req->sr_scsi_status != ST_GOOD)
				task->td_ret = DSRT_HOST;
			task->td_status = req->sr_scsi_status;
		}
	}
	else  {
		task->td_status = 0xff;
		task->td_ret = hosterr_to_ds(req->sr_status);
	}

	/* check for short read/write */
	if(!task->td_ret && req->sr_resid && req->sr_resid < req->sr_buflen) {
		task->td_ret = DSRT_SHORT;
	}

	if(!(task->td_iflags & TASK_INTERNAL) && task->td_buf.b_bcount)
		iounmap(&task->td_buf); /* done with it */

	task->td_datalen = req->sr_buflen - req->sr_resid;

	task->td_msglen = 0; /* no msg byte at SGI */

	task->td_state = TDS_DONE;
	DBG(printf("glue_return td_ret 0x%x. ", task->td_ret));
}


/*ARGSUSED2*/
int
glue_getopt(struct task_desc *tp, int ignore)
{
	scsi_target_info_t *info;
	int flags;
	unsigned status;

	/* this will get the 'interesting flags from the scsi driver, 
	 * and return them to the user */
	info = (SDEVI_INQ(tp->td_dev_vhdl))(tp->td_lun_vhdl); 
	if(!info)
		return 0;

	status = info->si_ha_status;
	flags = 0;
	if(status & SRH_CANTSYNC)
		flags |= DSG_CANTSYNC;
	if(status & SRH_SYNCXFR)
		flags |= DSG_SYNCXFR;
	if(status & SRH_TRIEDSYNC)
		flags |= DSG_TRIEDSYNC;
	if(status & SRH_BADSYNC)
		flags |= DSG_BADSYNC;
	if(status & SRH_NOADAPSYNC)
		flags |= DSG_NOADAPSYNC;
	if(status & SRH_WIDE)
		flags |= DSG_WIDE;
	if(status & SRH_DISC)
		flags |= DSG_DISC;
	if(status & SRH_TAGQ)
		flags |= DSG_TAGQ;

	return flags;
}
