#ident "$Headers"

/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996 Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

/* File system datapipe end code */

#include <sys/types.h>
#include <sys/systm.h>                    /* printf */
#include <sys/kmem.h>                     
#include <sys/ksynch.h>                   /* mutex_alloc */
#include <sys/ddi.h>
#include <sys/cred.h>                     /* cred_t */
#include <sys/errno.h>                    /* errno */
#include <sys/debug.h>                    /* ASSERT */
#include <ksys/vfile.h>                     /* vfile_t */
#include <sys/vnode.h>                    /* vops */
#include <ksys/fdt.h>                     /* getf() */
#include <sys/dpipe.h>
#include <sys/uuid.h>                     /* keep grio.h happy */
#include <sys/resource.h>
#include <sys/uthread.h>
#include <sys/kthread.h>
#include <sys/grio.h>                     /* MAX_NUM_DISKS */
#include <sys/statvfs.h>                  /* statvfs_t */
#include <sys/fsid.h>                     /* FSID_XFS */

/* debugging stuff */
#if DEBUG
static int fspe_debug = 1;
#else
static int fspe_debug = 0;
#endif
#define PRINTD if(fspe_debug) printf

extern void statvfs_socket(statvfs_t *);
extern int  xfs_file_to_disks(vfile_t *, dev_t *, int *);
extern int  xfs_fspe_dioinfo(struct vnode *, struct dioattr *);
extern int  efs_fspe_dioinfo(struct vnode *, struct dioattr *);

/* When considering the data structure for transfers, we have these
 * assumptions:
 * 1. each pipe id is unique in a system, but the pipe ends has no
 *    knowledge about how they are generated. So linked list seems
 *    to be the reasonable way to put them together.
 * 2. each transfer id is unique in a pipe. The pipe end does not
 *    make assumptions that the transfer ids are unique within a system,
 *    nor does it know how it is generated. 
 * 3. each transfer has a scatter gather list that specifies the source
 *    or sink segment that the data is to be transferred.
 *
 * When the above does not hold, we may be able to change the data
 * structure to make it more efficient. 
 */

/*
 * transfer information
 */
typedef struct fspe_transfer_info_s {
	int                         transfer_id;  /* unique within each pipe */
        int                         iovcount;     /* number of entries in sg_list */
	struct dpipe_fspe_bind_list *sg_list;     /* scatter gather list */
	int                         byte_count;   /* averall byte count */
	int                         ioindex;      /* pointer to the entry
						     in the list */
	off_t                       offset;       /* offset within the sg_list
						     entry */
	struct fspe_transfer_info_s *next;        /* next transfer in the pipe */
} fspe_transfer_info_t;

/*
 * fspe_t: the data structure for each pipeid 
 */
typedef struct fspe_s {
	int                  dpipe_id;         /* identifier in the table */
	int                  fd;               /* specify the pipe end */
	int                  flag;             /* passed to vop_read/write */
	short                role;             /* source or sink */
	vnode_t              *vp;              /* the corresponding vnode */
        mutex_t              *ti_lock;         /* lock for the transfer table */
	struct dioattr       da;             /* direct io attributes */
	fspe_transfer_info_t *transfer_list;   /* transfer table */
	
	struct fspe_s        *previous;        /* previous entry in the fspe
						  table */
	struct fspe_s        *next;            /* next entry in the fspe table */
} fspe_t;

fspe_t *dpipe_table;                           /* fspe table */

/* data pipe entry points */
int fspe_connect             (int, int, int, dpipe_conn_attr_t *, 
			      dpipe_end_protocol_ops_t **);
int fspe_disconnect          (int, dpipe_conn_attr_t *);
int fspe_get_master_attr     (int, dpipe_end_attr_t *);
int fspe_prio_vertex         (int, dev_t *, int *);
int fspe_prio_set            (int);
int fspe_prio_clear          (int);
int fspe_master_prepare      (dpipe_transfer_id_t, 
			      dpipe_end_trans_ctx_hdl_t,
			      dpipe_conn_attr_t *,
			      dpipe_pad_info_t *);
int fspe_master_start        (dpipe_transfer_id_t, 
			      dpipe_conn_attr_t *,
			      dpipe_slave_dest_t *);
int fspe_master_transfer_done(dpipe_transfer_id_t, dpipe_conn_attr_t *, int);

/* auxilliary routines */
static fspe_t *pipeid_to_fspe(int);
static void    fspe_dump(void);

/* fspe function table */
dpipe_end_control_ops_t 
fspe_control_ops = {
	NULL,                       /* dpipe_init */
	NULL,                       /* dpipe_get_slave_attr */
	fspe_get_master_attr,       /* dpipe_get_master_attr */
	NULL,                       /* dpipe_free */
	fspe_connect,               /* dpipe_connect */
	fspe_disconnect,            /* dpipe_disconnect */
	fspe_prio_vertex,           /* dpipe_prio_vertex */
	fspe_prio_set,              /* dpipe_prio_set */
	fspe_prio_clear             /* dpipe_prio_clear */
};

dpipe_end_simple_protocol_ops_t 
fspe_simple_ops = {
	NULL,                       /* dpipe_slave_prepare */        
	fspe_master_prepare,        /* dpipe_master_prepare */
	NULL,                       /* dpipe_slave_get_dest */
	NULL,                       /* dpipe_slave_done */
	fspe_master_start,          /* dpipe_master_start */
	NULL,                       /* dpipe_slave_transfer_done */
	fspe_master_transfer_done   /* dpipe_master_transfer_done */
};

mutex_t *fspe_global_lock;          /* lock for fspe table */

/* 
 * This routine is called from xfs_init because right now only 
 * xfs supports priority io and data pipe.
 */
void 
fspeinit() 
{
	fspe_global_lock = mutex_alloc(MUTEX_DEFAULT, KM_SLEEP, "fspe global", 0);
	dpipe_table = NULL;
}

/* 
 * This routine does two things: 
 * 1. allocate resource for the new connection, remember the 
 *    corresponding file descriptor;
 * 2. return the address of the operation table based on protocol.
 */
int
fspe_connect(int fd, int role, int bus_type, 
	     dpipe_conn_attr_t *connection,
	     dpipe_end_protocol_ops_t **protocol_sw) 
{
	fspe_t     *pe;
	vfile_t     *fp;
	int error;
	rval_t     rvp;
	statvfs_t  ds;

	PRINTD("fspe_connect: fd %d, role %d, bus_type %d, pipe id %d\n",
	       fd, role, bus_type, connection->dpipe_id);

	if (!(role & (DPIPE_SOURCE_CAPABLE | DPIPE_SINK_CAPABLE)))
		return EINVAL;

	/* 
	 * fspe can only be a master bus type
	 * connection should be a valid pointer 
	 */
	if ((bus_type != DPIPE_MASTER) || (connection == NULL))
		return EINVAL;

	if (fd < 0) 
		return EBADFD;

	/* look to see if this pipe id is already being used */
	pe = pipeid_to_fspe(connection->dpipe_id);
	if (pe != NULL) {
		PRINTD("This pipe id is already connected.\n");
		return EINVAL;
	}

	/* allocate a new entry for fspe_t and insert it 
	 * at the beginning of the table */

	pe = (fspe_t *)kmem_alloc(sizeof(fspe_t), KM_SLEEP);
	ASSERT(pe != NULL);

	mutex_lock(fspe_global_lock, -1);
	if (dpipe_table == NULL) {
		dpipe_table = pe;
		dpipe_table->next = NULL;
		dpipe_table->previous = NULL;
	} else {
		pe->next = dpipe_table;
		pe->previous = NULL;
		dpipe_table->previous = pe;
		dpipe_table = pe;
	}
	mutex_unlock(fspe_global_lock);

	pe->fd        = fd;
	pe->flag      = 0;
	pe->role      = role;
	pe->dpipe_id  = connection->dpipe_id;
	pe->ti_lock   = mutex_alloc(MUTEX_DEFAULT, KM_SLEEP, "transfer info", 0);
	pe->transfer_list = NULL;
	if (role & DPIPE_SINK_CAPABLE)
	  pe->flag |= IO_SYNC;

	/* get the vnode pointer and open the file */
	if (error = getf(fd, &fp)) {
		PRINTD("fspe_connect: getf fail\n");
		return error;
	}
	if (!VF_IS_VNODE(fp)) {
		PRINTD("fspe_connect: not a vnode\n");
		return EINVAL;
	}
	
	/* get direct io info from fs */
	VFS_STATVFS(pe->vp->v_vfsp, &ds, pe->vp, error);
	if (error) {
		PRINTD("fspe_connect: VFS_STATVFS fail\n");
		return error;
	}

	if (!strcmp(ds.f_basetype, FSID_XFS)) 
		xfs_fspe_dioinfo(pe->vp, &(pe->da));
	else if (!strcmp(ds.f_basetype, FSID_EFS))
		efs_fspe_dioinfo(pe->vp, &(pe->da));
	     
	PRINTD("fspe_connect: dio info: mem %d min %d max %d\n",
	       pe->da.d_mem, pe->da.d_miniosz, pe->da.d_maxiosz);

	switch(connection->dpipe_conn_protocol_flags) {

		/* There is only one protocol for now */

	      case DPIPE_SIMPLE_PROTOCOL:
		*protocol_sw = 
		  (dpipe_end_protocol_ops_t *)&fspe_simple_ops;
		break;
	}

/*	if (fspe_debug)
	  fspe_dump();*/
	return 0;
}

/*ARGSUSED*/
int
fspe_disconnect(int fd, dpipe_conn_attr_t *connection) 
{
	fspe_t *pe;
	fspe_transfer_info_t *nextone;
	int error = 0;

	PRINTD("fspe_disconnect pipe id: %d\n", connection->dpipe_id);

	/* look through the dpipe_table to look for this connection */
	if ((pe = pipeid_to_fspe(connection->dpipe_id)) == NULL) {
		PRINTD("fspe_disconnect: no connection is available.\n");
		return EINVAL;
	}

	mutex_lock(fspe_global_lock, -1);
	if (pe->previous != NULL) {
		pe->previous->next = pe->next;
		if (pe->next != NULL)
		  pe->next->previous = pe->previous;
	} else {
		/* already the beginning of the table */
		if (pe->next != NULL) 
		  pe->next->previous = NULL;
		else
		  dpipe_table = NULL;
	}
	mutex_unlock(fspe_global_lock);

	/* free the resources related to this pipe */

	mutex_lock(pe->ti_lock, -1);
	nextone = pe->transfer_list;
	while(nextone != NULL) {
		nextone = nextone->next;

		/* sg_list cannot be null because the transfer is allocated
		 * when sg_list is set */
		ASSERT(pe->transfer_list->sg_list != NULL);
		kmem_free(pe->transfer_list->sg_list, 
			  pe->transfer_list->iovcount * 
			  sizeof(struct dpipe_fspe_bind_list));

		kmem_free(pe->transfer_list, sizeof(fspe_transfer_info_t));
		pe->transfer_list = nextone;
	}
	mutex_unlock(pe->ti_lock);

	mutex_dealloc(pe->ti_lock);

	VOP_CLOSE(pe->vp, FREAD|FWRITE, L_TRUE, get_current_cred(), error);
	if (error) {
		PRINTD("fspe_connect: VOP_CLOSE fail\n");
	}
	kmem_free(pe, sizeof(fspe_t));
/*	if (fspe_debug)
	  fspe_dump();*/
	return error;
}

int 
fspe_get_master_attr(int fd, dpipe_end_attr_t *connection) 
{
	vfile_t *fp;
	struct vattr va;
	vnode_t *vp;
	int error = 0;
	cred_t *crp = get_current_cred();
	int index;
	
	if (fd < 0)
		return EBADFD;

	if (connection == NULL) {
		PRINTD("connection is NULL\n");
		return EINVAL;
	}
	connection->dpipe_end_bus_type_flags = DPIPE_MASTER;
	connection->dpipe_end_role_flags = DPIPE_SINK_CAPABLE | DPIPE_SOURCE_CAPABLE;
	connection->dpipe_end_dma_flags |= DPIPE_PARTIAL_BLOCK; 
	connection->dpipe_end_protocol_flags |= DPIPE_SIMPLE_PROTOCOL;

	error = getf(fd, &fp);
	if (error) {
		PRINTD("fspe_get_master_attr: getf failed\n");
		return error;
	}
	if (!VF_IS_VNODE(fp)) {
		PRINTD("fspe_get_master_attr: not a vnode\n");
		return EINVAL;
	}

	vp = VF_TO_VNODE(fp);
	VOP_OPEN(vp, &vp, FREAD|FWRITE, get_current_cred(), error);
	if (error) {
		PRINTD("fspe_get_master_attr: VOP_OPEN failed\n");
		return error;
	}

 	va.va_mask = AT_BLKSIZE;
	VOP_GETATTR(vp, &va, 0, crp, error);
	if (error) {
		PRINTD("fspe VOP_GETATTR fail\n");
		return error;
	}

	connection->dpipe_end_block_size = va.va_blksize;
	connection->dpipe_end_max_len_per_dest = 0;
	connection->dpipe_end_max_num_of_dest = 0;
	return 0;
}

/* 
 * Return DPIPE_SR_ERROR if anything is wrong.
 */
int fspe_master_prepare(dpipe_transfer_id_t transfer_id, 
			dpipe_end_trans_ctx_hdl_t cookie_handle,
			dpipe_conn_attr_t *connection,
			dpipe_pad_info_t *pad)
{
	fspe_t               *pe;
	fspe_transfer_info_t *ti;

	if (connection == NULL)
	  return DPIPE_SP_ERROR;

	if ((pe = pipeid_to_fspe(connection->dpipe_id)) == NULL) {
		PRINTD("fspe_master_prepare: pipe id %d\n", 
		       connection->dpipe_id);
		return DPIPE_SP_ERROR;
	}

	/* look for transfer info handle */
	   
	mutex_lock(pe->ti_lock, -1);
	ti = pe->transfer_list;
	while(ti != NULL) {
		if (ti->transfer_id == transfer_id)
		  break;
		ti = ti->next;
	}
	mutex_unlock(pe->ti_lock);
	if (ti == NULL) {
		PRINTD("fspe_master_prepare: no ti correspond to %d\n",
		       transfer_id);
		return DPIPE_SP_ERROR;
	} 
	
	return ti->byte_count;
}

/* 
 * This routine actually starts the transfer of data to the other
 * pipe end
 */
int fspe_master_start(dpipe_transfer_id_t transfer_id, 
		      dpipe_conn_attr_t *connection,
		      dpipe_slave_dest_t *dest)
{
	fspe_t               *pe;
	uio_t                uiop;
	iovec_t              iovec;	/* Assume only one iovec involved*/
	struct cred          *crp = get_current_cred();
	fspe_transfer_info_t *ti;
	int                  error = 0;
	size_t               finished = 0, current_len;
	char                 *current_base;
	size_t               residual, mem_left, sg_left;
	int                  vop_flag, new_mem = 0;

	ASSERT(dest != NULL && dest->dpipe_dest_array != NULL);

	PRINTD("fspe_master_start: entry\n");

	if ((pe = pipeid_to_fspe(connection->dpipe_id)) == NULL) {
		PRINTD("fspe_master_start: no entry for pipeid %d\n",
		       connection->dpipe_id);
		return DPIPE_SP_ERROR ;
	}
	/* look for transfer info handle */
	mutex_lock(pe->ti_lock, -1);
	ti = pe->transfer_list;
	while (ti != NULL) {
		if (ti->transfer_id == transfer_id) 
		  break;
		ti = ti->next;
	}
	mutex_unlock(pe->ti_lock);

	if (ti == NULL) {
		PRINTD("fspe_master_start: no ti corresponding to %d\n", 
		       transfer_id);
		return EINVAL;
	}

	/* Enforce the assumption that dest has one element in buffered
	 * pipe case. 10/07/96
	 */
	ASSERT(dest->dpipe_dest_count == 1);

	while ((ti->ioindex < ti->iovcount) && 
	       (dest->dpipe_dest_array[0].dpipe_len > finished)) {

		PRINTD("mem len is: %d finished: %d\n",
		       dest->dpipe_dest_array[0].dpipe_len, finished);
		mem_left = dest->dpipe_dest_array[0].dpipe_len - finished;
		sg_left = ti->sg_list[ti->ioindex].size - ti->offset;
		vop_flag = pe->flag;
		uiop.uio_offset = ti->sg_list[ti->ioindex].offset + ti->offset;

		iovec.iov_base = (void *)
		  (dest->dpipe_dest_array[0].dpipe_addr + finished);

		if ((pe->flag & IO_DIRECT) || 
		    (mem_left < pe->da.d_miniosz) ||
		    (sg_left < pe->da.d_miniosz)) {
			if (sg_left <= mem_left) {
				iovec.iov_len = sg_left;
				ti->ioindex++;
				ti->offset = 0;
			} else {
				iovec.iov_len = mem_left;
				ti->offset += iovec.iov_len;
			}
		}else {
			residual = (ti->sg_list[ti->ioindex].offset + 
				    ti->offset) % pe->da.d_miniosz;
		        if (residual == 0) {
				/* A direct IO should be done. If the memory
				 * given by the slave doesn't meet d_mem
				 * requirement, we need to allocate memory
				 * alligned as required, and then copy it
				 * over to the memory given. 
				 */
				PRINTD("fspe_master_start: direct io.\n");
				vop_flag |= IO_DIRECT;
				iovec.iov_len = (((sg_left > mem_left) ?
						mem_left: sg_left) /
						 pe->da.d_miniosz) *
						   pe->da.d_miniosz;
				if ((unsigned long long)iovec.iov_base % pe->da.d_mem) {
					iovec.iov_base = kmem_alloc(iovec.iov_len,
								    KM_SLEEP);
					new_mem = 1;
				}
			} else {
				/* has to deal with indirect io */
				iovec.iov_len = pe->da.d_miniosz - residual;
			}
			
			ti->offset += iovec.iov_len; 
			if (ti->offset == ti->sg_list[ti->ioindex].size) {
				ti->ioindex++;
				ti->offset = 0;
			}
		}
		
		uiop.uio_iov = &iovec;
		
		uiop.uio_resid = iovec.iov_len;
		uiop.uio_iovcnt = dest->dpipe_dest_count;
		uiop.uio_segflg = dest->dpipe_segflg;
		uiop.uio_fmode = FREAD|FWRITE;
		uiop.uio_pio = 0;
		uiop.uio_readiolog = 0;
		uiop.uio_writeiolog = 0;
		uiop.uio_pmp = NULL;
		uiop.uio_sigpipe = 0;
		uiop.uio_limit = RLIM_INFINITY;
		current_len = iovec.iov_len;
		current_base = iovec.iov_base;
		
		if (pe->role & DPIPE_SINK_CAPABLE) {
			/* we are sink, write to file system */
			PRINTD("fspe write %d bytes\n", iovec.iov_len);
			if (new_mem)
			  bcopy((void *)(dest->dpipe_dest_array[0].dpipe_addr 
				+ finished), iovec.iov_base, iovec.iov_len);
			VOP_WRITE(pe->vp, &uiop, vop_flag, crp, 
				  &curuthread->ut_flid, error);
			ASSERT(uiop.uio_sigpipe == 0);
			if (new_mem) {
				kmem_free(current_base, current_len);
				new_mem = 0;
			}
		} else if (pe->role & DPIPE_SOURCE_CAPABLE) {
			/* we are source, read from file system */
			PRINTD("fspe read %d bytes\n", iovec.iov_len);
			VOP_READ(pe->vp, &uiop, vop_flag, crp, 
				 &curuthread->ut_flid, error);
			if (new_mem) {
				bcopy(current_base,
				      (void *)(dest->dpipe_dest_array[0].dpipe_addr + finished),
				      current_len);
				kmem_free(current_base, current_len);
				new_mem = 0;
			}
		} else {
			PRINTD("Invalid role flag.\n");
			error = EINVAL;
		}
		
		if (error) {
			PRINTD("fspe transfer fail\n");
			break;
		} 

		vop_flag &= ~IO_DIRECT;
		finished += current_len;			

	} /* while */
	return error;
}

/*
 * clean up the data structure associated with a transfer
 */
/* ARGSUSED */
int 
fspe_master_transfer_done (dpipe_transfer_id_t transfer_id,
			   dpipe_conn_attr_t *connection,
			   int status)
{
	fspe_t *pe;
	struct fspe_cookie *fc;
	fspe_transfer_info_t *ti, *ti_pre;
	int index;
	int i;
	int error = 0;

	PRINTD("fspe_master_transfer_done: pipe %d, transfer %d\n",
	       connection->dpipe_id, transfer_id);
	pe = pipeid_to_fspe(connection->dpipe_id);
	if (pe == NULL)  {
		PRINTD("fspe_master_transfer_done: no pipe for pipeid %d\n",
		       connection->dpipe_id);
		return EINVAL;
	}
	/* Free this transfer_id from the transfer list */
	mutex_lock(pe->ti_lock, -1);
	ti_pre = NULL;
	ti = pe->transfer_list;
	while (ti != NULL) {
		if (ti->transfer_id == transfer_id) {
			if (ti_pre == NULL)
			  /* first element in the list */
			  pe->transfer_list = ti->next;
			else 
			  ti_pre->next = ti->next;
			break;
		}
		ti_pre = ti;
		ti = ti->next;
	}
	mutex_unlock(pe->ti_lock);
	
	if (ti != NULL) {

		ASSERT((ti->sg_list != NULL) && (ti->iovcount > 0));
		kmem_free(ti->sg_list, ti->iovcount * 
			  sizeof(struct dpipe_fspe_bind_list));
		kmem_free(ti, sizeof(fspe_transfer_info_t));
	}
	
	return error;
}

/* only xfs file system supports priority bandwidth allocation.
 * The caller is responsible to allocate enough memory for disks
 */
int 
fspe_prio_vertex(int fd, dev_t *disks, int *disk_count)
{
	statvfs_t ds;
	vfile_t    *fp;
	vnode_t   *vp;
	int       error;
	vfs_t     *vfsp;

	PRINTD("fspe_prio_vertex: entry with fd %d disks 0x%x"
	       " disk count 0x%x\n", fd, disks, disk_count);
	if (disks == NULL || disk_count == NULL)
	  return EINVAL;

	if (error = getf(fd, &fp))
	  return error;

	if (fp->vf_flag & FSOCKET) {
		statvfs_socket(&ds);
	} else {
		vp = VF_TO_VNODE(fp);
		vfsp = vp->v_vfsp;
		VFS_STATVFS(vfsp, &ds, vp, error);
		if (error)
		  return error;
	}

	/* only xfs supports prio */
	if (strcmp(ds.f_basetype, FSID_XFS))
		return EOPNOTSUPP;

	error = xfs_file_to_disks(fp, disks, disk_count);
	if (*disk_count <= 0) {
		PRINTD("fspe_prio_vertex: disk_count wrong.\n");
	} 
	PRINTD("fspe_prio_vertex: exit with %d\n", error);
	return error;
}

int 
fspe_prio_set(int dpipe_id) 
{
	fspe_t *pe;

	PRINTD("fspe_prio_set: entry for pipe %d\n", dpipe_id);
	pe = pipeid_to_fspe(dpipe_id);
	if (pe == NULL) {
		PRINTD("fspe_prio_set: fail no pipe\n");
		return EINVAL;
	}

	pe->flag |= IO_PRIORITY;
	return 0;
}

int
fspe_prio_clear(int dpipe_id)
{
	fspe_t *pe;

	PRINTD("fspe_prio_clear: entry for pipe %d\n", dpipe_id);
	pe = pipeid_to_fspe(dpipe_id);
	if (pe == NULL) {
		PRINTD("fspe_prio_clear: fail no pipe\n");
		return EINVAL;
	}

	pe->flag &= ~IO_PRIORITY;
	return 0;
}

/* 
 * Called from fcntl F_GETOPS
 */
int fspe_get_ops(void *arg)
{
	__uint64_t addr = (__uint64_t)&fspe_control_ops;
	copyout(&addr, arg, sizeof (__uint64_t));
	return 0;
}

/* 
 * Called from syssgi command SGI_DPIPE_FSPE_BIND
 */
int fspe_bind(struct sgi_dpipe_fspe_bind *arg)
{
	
	fspe_transfer_info_t *ti, *tmp;
	fspe_t *pe;
	int error, i;
	struct dpipe_fspe_bind_list *newlist;
	struct sgi_dpipe_fspe_bind input;
	vfile_t *fp;

	copyin(arg, &input, sizeof(struct sgi_dpipe_fspe_bind));
	/* The library should have checked the validity of iovcnt
	 * and sglist, so we just do an ASSERT here.
	 */
	ASSERT((input.iovcnt > 0) && (input.sglist != NULL));
	
	PRINTD("fspe_bind: pipeid 0x%lx, transfer_id 0x%lx, role %d, iovcount %d, sg_list 0x%lx\n",
	       input.pipe_id, input.transfer_id, input.role, input.iovcnt, 
	       input.sglist);
	pe = pipeid_to_fspe(input.pipe_id);
	if (pe == NULL) {
		PRINTD("fspe_bind: pipe id %d invalid\n", input.pipe_id);
		return EINVAL;
	}

	/* check direct io flag and set accordingly */
	if (error = getf(pe->fd, &fp)) {
		PRINTD("fspe_bind: getf fail with %d\n", error);
		return DPIPE_SP_ERROR;
	}
	if (fp->vf_flag & FDIRECT) 
	  pe->flag |= IO_DIRECT;
	else 
	  pe->flag &= ~IO_DIRECT;

	/* Check to see if the transfer id is already used.
	 * If this happens, that means the DPIPE_PRETRANS command
	 * didn't work correctly. If we decide that the system
	 * works pretty well, we can delete this checking to
	 * save some time. For now, leave it here.
	 */
	tmp = pe->transfer_list;
	while(tmp != NULL) {
		if (tmp->transfer_id == input.transfer_id) {
			PRINTD("fspe_bind: transfer_id %d used\n",
			       input.transfer_id);
			return EINVAL;
		}
		tmp = tmp->next;
	}

	/* 
	 * add a new transfer to the list
	 */
	
	ti = (fspe_transfer_info_t *) kmem_alloc 
	  (sizeof(fspe_transfer_info_t), KM_SLEEP);
	ASSERT(ti != NULL);
	
	ti->transfer_id = input.transfer_id;
	ti->ioindex = 0;
	ti->offset = 0;
	ti->iovcount = input.iovcnt;
	
	newlist = (struct dpipe_fspe_bind_list *)kmem_alloc
	  (input.iovcnt * sizeof(struct dpipe_fspe_bind_list), KM_SLEEP);
	ASSERT(newlist != NULL);

	copyin((void *)input.sglist, newlist, input.iovcnt * sizeof(struct dpipe_fspe_bind_list));
	ti->sg_list = newlist;

	ti->byte_count = 0;
	for (i = 0; i < input.iovcnt; i++) {
		if ((newlist[i].offset < 0) || newlist[i].size <= 0) {
			kmem_free(newlist, input.iovcnt * 
				  sizeof(struct dpipe_fspe_bind_list));
			kmem_free(ti, sizeof(fspe_transfer_info_t));
			return EINVAL;
		}
		ti->byte_count += newlist[i].size;
	}

	mutex_lock(pe->ti_lock, -1);
	if (pe->transfer_list == NULL) {
		pe->transfer_list = ti;
		ti->next = NULL;
	} else {
		ti->next = pe->transfer_list;
		pe->transfer_list = ti;
	}
	mutex_unlock(pe->ti_lock);

	return 0;
}


/*
 * pipeid_to_fspe: takes the pipe id as the argument, return the
 * corresponding fspe data structure.
 */
static fspe_t *pipeid_to_fspe(int id)
{
	fspe_t *pe;

	mutex_lock(fspe_global_lock, -1);
	pe = dpipe_table;
	while(pe != NULL) {
		if (pe->dpipe_id == id)
		  break;
		pe = pe->next;
	}
	mutex_unlock(fspe_global_lock);

	return pe;
}

static void fspe_dump()
{
	fspe_t *pe;
	fspe_transfer_info_t *ti;

	mutex_lock(fspe_global_lock, -1);
	pe = dpipe_table;
	printf("fspe table: \n");
	while(pe != NULL) {
		printf("fd %d, role %d, pipe ID %d.\n", pe->fd, pe->role,
		       pe->dpipe_id);
		printf("transfer_list: ");
		ti = pe->transfer_list;
		while(ti != NULL) {
			printf("transfer id %d", ti->transfer_id);
			ti = ti->next;
		}
		pe = pe->next;
		printf("\n");
	}
	mutex_unlock(fspe_global_lock);
}

