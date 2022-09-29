/**************************************************************************
 *									  *
 * 		 Copyright (C) 1996, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.65 $" 

#include <sys/dpipe.h>
#include <sys/kmem.h>
#include <sys/debug.h>                  /* ASSERT */
#include <sys/ddi.h>                    /* makedevice, getemajor, geteminor */
#include <sys/errno.h>
#include <sys/systm.h>                  /* printf */
#include <ksys/sthread.h>
#include <sys/sema.h>
#include <sys/atomic_ops.h>
#include <sys/mload.h>
#include <sys/lock.h>
#include <sys/ioctl.h>
#include <sys/prio.h>
#include <ksys/fdt.h>                   /* getf() */
#include <ksys/vfile.h>
#include <sys/vnode.h>
#include <sys/cred.h>                   /* cred_t */

#define MAX_NUM_DISKS 512                   /* same as grio.h */

#define DPIPE_TAB_START_SIZE 10
#define DPIPE_TAB_CHUNK_SIZE 10
#define DPIPE_MAX_THREADS 8
#define DPIPE_NUM_THREADS 1
#define DPIPE_BUFFER_SIZE 524288 /* 32 16k pages */
#define DPIPE_TAB_ENTRY_INUSE ((long)-1)

/*
 * Per transfer data structure.  The defines for the status bits are in
 * sys/dpipe.h 
 */
typedef struct dpipe_trans_s {
    dpipe_transfer_id_t        transfer_id;
    dpipe_end_trans_ctx_hdl_t  src_cookie;
    dpipe_end_trans_ctx_hdl_t  sink_cookie;
    int                        status;
    struct dpipe_trans_s    *next;
    struct dpipe_trans_s    *prev;
} dpipe_trans_t;


/* Data structure representing a doubly linked list of 
   transfer events */
typedef struct dpipe_trans_queue_s {
    dpipe_trans_t   *head;
    dpipe_trans_t   *tail;
    mutex_t            lock;
    int                count;
} dpipe_trans_queue_t;

/*
 * Per dpipe data structure.  These are allocated per open and kept on
 * an array such that the array index is used for the minor number
 */
typedef struct dpipe_s {
    dev_t                         id;
    int                           num_threads;

    int                           src_fd;
    int                           sink_fd;

    dpipe_end_attr_t              src_attr;
    dpipe_end_attr_t              sink_attr;
    dpipe_end_control_ops_t       *src_cnt;
    dpipe_end_control_ops_t       *sink_cnt;
    dpipe_end_protocol_ops_t      *src_prot;
    dpipe_end_protocol_ops_t      *sink_prot;

    /* These only different in the buffered pipe case */
    dpipe_conn_attr_t             src_conn;
    dpipe_conn_attr_t             sink_conn;

    struct dpipe_thread_s         *threads[DPIPE_MAX_THREADS];
    dpipe_trans_queue_t           pend_tqueue;
    dpipe_trans_queue_t           done_tqueue;
    sema_t                        tlist_sema;
    sv_t                          tlist_empty;
    int                           tlist_empty_waiter;
    mutex_t                       lock;
} dpipe_t; 

typedef struct dpipe_thread_s {
    sema_t    exit_sema;
    char      *buffer;
    int       buffer_size;
    dpipe_trans_t *cur_transfer;
    dpipe_t   *pipe;
    int       exit;
} dpipe_thread_t;


mutex_t           *dpipe_tab_lock; /* Global Lock to protect the dpipe_tab */
static dpipe_t    **dpipe_tab;     /* Table of dpipe_t */
static int        dpipe_tab_size;  /* Allocated size of the table */
int               dpipedevflag = D_MT;
char              *dpipemversion = M_VERSION;

static __int64_t  dpipe_t_id_counter = 0; /* Global transfer id counter */

#if DEBUG
static int dpipe_debug = 1;
#define PRINTD if (dpipe_debug == 1) printf
#else
static int dpipe_debug = 0;
#define PRINTD if (dpipe_debug == 1) printf
#endif

extern int dpipemajor;

/* Local prototypes */

/* Standard device driver entry points */
void dpipeinit   (void);
int  dpipeopen   (dev_t *devp, int flag, int otype, struct cred *cred);
int  dpipeioctl  (dev_t dev, int cmd, __psint_t arg, int flag, 
		  struct cred *cred, int *rvalp);
int  dpipeclose  (dev_t dev, int flag, int otype, struct cred *cred);
void dpipeunload (void);

/* Helper routines for std. device driver entry points */
static dpipe_t *dpipe_dev_to_dpipe       (dev_t);
static int      dpipe_init_ends          (dpipe_t *);
static int      dpipe_get_end_attr       (dpipe_t *);
static int      dpipe_init_buf_pipe      (dpipe_t *);
static int      dpipe_transfer           (dpipe_t *, dpipe_trans_t *);
static int      dpipe_get_status         (dpipe_t *, dpipe_transfer_id_t);
static int      dpipe_get_free_tab_index (void);
static int      dpipe_flush              (dpipe_t *);
static int      dpipe_prio_alloc         (dpipe_t *, prioAllocReq_t *);
static int      dpipe_prio_get_bw        (dpipe_t *, prioAllocReq_t *);

/* Transfer queue management routines */

static int            dpipe_init_trans_queue(dpipe_trans_queue_t *); 
static int            dpipe_enqueue_trans   (dpipe_trans_queue_t *, 
					     dpipe_trans_t *);
static dpipe_trans_t *dpipe_dequeue_trans   (dpipe_trans_queue_t *);
static dpipe_trans_t *dpipe_lookup_trans_id (dpipe_trans_queue_t *, 
					     dpipe_transfer_id_t);
static int            dpipe_trans_queue_len (dpipe_trans_queue_t *);
static int            dpipe_destroy_trans_queue (dpipe_trans_queue_t *);

/* Transfer service thread management routines */

static void            dpipe_thread        (dpipe_thread_t *);
static dpipe_thread_t *dpipe_thread_alloc  (dpipe_t *, int, int *);
static void            dpipe_thread_dealloc(dpipe_thread_t *);


/* Called during the boot process, globals are inited here */
void
dpipeinit()
{
    /* Initialize globals locks etc */
    
    /* Allocate space for a table of dpipe data structs*/
    dpipe_tab_lock = mutex_alloc(MUTEX_DEFAULT, KM_SLEEP, "dpipeglob", 0);
    dpipe_tab = (dpipe_t **)kmem_zalloc(
	sizeof(dpipe_t *) * DPIPE_TAB_START_SIZE, KM_SLEEP);
    /* dpipe_tab_size refers to the size of the table not the number of
       pipes that are active */
    dpipe_tab_size= DPIPE_TAB_START_SIZE;
}

/* Deallocate global resources if driver is unloaded */
void 
dpipeunload()
{
    mutex_dealloc(dpipe_tab_lock);
    kmem_free(dpipe_tab, sizeof(dpipe_t *) * dpipe_tab_size);
    dpipe_tab_size = 0;
}

/* 
 * Looks through the dpipe_tab and returns an unused entry.  Grows
 * the table if there is not a free entry
 */
static int
dpipe_get_free_tab_index ()
{
    int i, found;
    dpipe_t **new_tab;

    found = 0;
    mutex_lock(dpipe_tab_lock, PZERO);
    for (i = 0; i < dpipe_tab_size; i++) {
	if (dpipe_tab[i] == NULL) {
	    found = 1;
	    dpipe_tab[i] = (dpipe_t *)DPIPE_TAB_ENTRY_INUSE; /* Mark in use */
	    break;
	}
    }
    /* Have to grow the table if it is full */
    if (found == 0) {
	new_tab = (dpipe_t **)kmem_zalloc(sizeof(dpipe_t *) * (dpipe_tab_size +
					  DPIPE_TAB_CHUNK_SIZE), KM_SLEEP);
	i = dpipe_tab_size;
	bcopy(dpipe_tab, new_tab, sizeof(dpipe_t *) * dpipe_tab_size);
	kmem_free(dpipe_tab, sizeof(dpipe_t *) * dpipe_tab_size);
	dpipe_tab = new_tab;
	dpipe_tab_size = dpipe_tab_size + DPIPE_TAB_CHUNK_SIZE;	
	dpipe_tab[i] = (dpipe_t *)DPIPE_TAB_ENTRY_INUSE; /* Mark in use */
    }
    mutex_unlock(dpipe_tab_lock);
    return i;
}


/* dpipeopen: We find a free index in the dpipe_tab and then we allocate a 
   dpipe_t and put it in the table.  The minor number is set to the
   index in the table.*/
/* ARGSUSED */
int 
dpipeopen(dev_t *devp, int flag, int otype, struct cred *cred)
{
    int index;
    dpipe_t *new;
    
    PRINTD("dpipeopen: entry\n");
    index = dpipe_get_free_tab_index();
    PRINTD("dpipeopen: allocated dpipe %d\n", index);
    new = (dpipe_t *)kmem_zalloc(sizeof(dpipe_t), KM_SLEEP);
    mutex_lock(dpipe_tab_lock, PZERO);
    dpipe_tab[index] = new;
    mutex_unlock(dpipe_tab_lock);
    new->id = index;
    *devp = makedevice(dpipemajor, index);
    PRINTD("\t\t\tid %d allocated\n", new->id);
    return 0;
}


/* dpipe_dev_to_dpipe : Given the dev_t from a device driver entry point 
   return the pointer the the corresponding dpipe_t. Returns NULL is
   the dev is not valid.*/
static dpipe_t *
dpipe_dev_to_dpipe(dev_t dev) 
{
    unsigned int minor;
    dpipe_t *ret;
    
    minor = geteminor(dev);
    if (getemajor(dev) != dpipemajor) {
	return NULL;
    }
    if (minor >= dpipe_tab_size) {
	return NULL;
    }
    mutex_lock(dpipe_tab_lock, PZERO);
    ret = dpipe_tab[geteminor(dev)];
    mutex_unlock(dpipe_tab_lock);

    return ret;
}

/*dpipeioctl: device driver entry point.  Dispatches each ioctl to the
  appropriate routine */
/* ARGSUSED */
int
dpipeioctl(dev_t dev, int cmd, __psint_t arg, int flag, struct cred *cred, 
	   int *rvalp)
{
    dpipe_t *pipe;
    union {
	dpipe_create_ioctl_t    create_i;
	dpipe_pretrans_ioctl_t  pretrans_i;
	dpipe_transfer_ioctl_t  transfer_i;
	prioAllocReq_t          prioreq_i;
    } ioctl_u;
    dpipe_trans_t *transfer;
    int error = 0;
   
    pipe = dpipe_dev_to_dpipe(dev);
    if (pipe == NULL)
	return ENODEV;
    switch(cmd) {
    case DPIPE_CREATE:
	PRINTD("dpipeioctl: DPIPE_CREATE entry\n");
	copyin ((char *)arg, &(ioctl_u.create_i), 
		sizeof(dpipe_create_ioctl_t));
	pipe->src_fd = ioctl_u.create_i.src_fd;
	pipe->sink_fd = ioctl_u.create_i.sink_fd;
	PRINTD("src_fd = %x\n", pipe->src_fd);
	PRINTD("sink_fd = %x\n", pipe->sink_fd);
	pipe->src_cnt = (dpipe_end_control_ops_t *)
	    ioctl_u.create_i.dpipe_src_ops;
	pipe->sink_cnt = (dpipe_end_control_ops_t *)
	    ioctl_u.create_i.dpipe_sink_ops;

	/* Assume buffered pipe.  Later we are going to need some
	   routine that determines what type of pipe to use.*/
	error = dpipe_init_ends(pipe);
	if (error) {
	    return error;
	}	
	error = dpipe_get_end_attr(pipe);
	if (error) {
	    return error;
	}
	error = dpipe_init_buf_pipe(pipe);
	if (error) {
	    return error;
	}
	break;
    case DPIPE_PRETRANS:
	PRINTD("dpipeioctl: DPIPE_PRETRANS entry\n");
	ioctl_u.pretrans_i.src_pipe_id = pipe->src_conn.dpipe_id;
	ioctl_u.pretrans_i.sink_pipe_id = pipe->sink_conn.dpipe_id;
	ioctl_u.pretrans_i.transfer_id =
	    atomicAddInt64(&dpipe_t_id_counter, 1);
	copyout(&(ioctl_u.pretrans_i), (char *)arg, 
		sizeof(dpipe_pretrans_ioctl_t));	
	error = 0;
	break;
    case DPIPE_TRANSFER:
	PRINTD("dpipeioctl: DPIPE_TRANSFER entry\n");
	copyin ((char *)arg, &(ioctl_u.transfer_i), 
		sizeof(dpipe_transfer_ioctl_t));
	transfer = (dpipe_trans_t *)kmem_zalloc(sizeof(dpipe_trans_t),
						  KM_SLEEP);
	transfer->transfer_id = ioctl_u.transfer_i.transfer_id;
	transfer->src_cookie = ioctl_u.transfer_i.src_cookie;
	transfer->sink_cookie = ioctl_u.transfer_i.sink_cookie;
	transfer->next = NULL;

	dpipe_transfer(pipe, transfer);
	error = 0;
	break;
    case DPIPE_STATUS: 
    {
	dpipe_transfer_id_t status;
	dpipe_transfer_id_t id;

	PRINTD("dpipeioctl: DPIPE_STATUS entry\n");	
	copyin ((char *)arg, &(id), sizeof(dpipe_transfer_id_t));
	status = dpipe_get_status(pipe, id);
	copyout(&(status), (char *)arg, sizeof(dpipe_transfer_id_t));
	error = 0;
	break;
    }
    case DPIPE_FLUSH:
	PRINTD("dpipeioctl: DPIPE_FLUSH  entry\n");
	error = dpipe_flush(pipe);
	PRINTD("dpipeioctl: DPIPE_FLUSH  exit\n");	
	break;
    case DPIOCSETB:
	PRINTD("dpipeioctl: DPIOCSETB entry\n");
	copyin((char *)arg, &(ioctl_u.prioreq_i), sizeof(prioAllocReq_t));
	error = dpipe_prio_alloc(pipe, &(ioctl_u.prioreq_i));
	if (error == ENOLCK)
		if (copyout((caddr_t)&(ioctl_u.prioreq_i), (caddr_t)arg,
			    sizeof(prioAllocReq_t)) != 0)
			error = EINVAL;
	PRINTD("dpipeioctl: DPIOCSETB exit with %d\n", error);
	break;
    case DPIOCGETB:
	PRINTD("dpipeioctl: DPIOCGETB entry\n");
	copyin((char *)arg, &(ioctl_u.prioreq_i), sizeof(prioAllocReq_t));
	error = dpipe_prio_get_bw(pipe, &ioctl_u.prioreq_i);
	if (!error)
	  if (copyout((caddr_t)&(ioctl_u.prioreq_i), (caddr_t)arg,
		      sizeof(prioAllocReq_t)) != 0)
	    error = EINVAL;
	PRINTD("dpipeioctl: DPIOCGETB exit with %d\n", error);
	break;
    default:
	error = EINVAL;
	break;
    }
    return error;
}

static int 
dpipe_prio_alloc(dpipe_t *pipe, prioAllocReq_t *req)
{
	int                    error = 0;
	vfile_t                 *fp;
	struct prio_alloc_info src_info, sink_info;
	int                    src_num = 0, sink_num = 0;
	int                    i, j, src_current;
	pid_t                  holder;
	bandwidth_t            *src_old_req_bw, *src_old_alloc_bw;
	bandwidth_t            *sink_old_req_bw, *sink_old_alloc_bw;
	vertex_hdl_t           *src_mem_vhdl, *sink_mem_vhdl;
	int                    s1, s2;

	/* assume the disk has the most number of vertices corresponding
	 * to a fd 
	 */
	dev_t *src_vertices, *sink_vertices;
	src_vertices = (dev_t *)kmem_alloc(sizeof(dev_t) * MAX_NUM_DISKS,
					   KM_SLEEP);
	sink_vertices = (dev_t *)kmem_alloc(sizeof(dev_t) * MAX_NUM_DISKS,
					    KM_SLEEP); 

	PRINTD("dpipe_prio_alloc entry: reqeust %d bytes/second\n",
	       req->bytesec);

	/* really only newer hardware would support bandwidth allocation */
	if ((pipe->src_cnt->dpipe_prio_vertices == NULL) ||
	    (pipe->sink_cnt->dpipe_prio_vertices == NULL) ||
	    (pipe->src_cnt->dpipe_prio_set == NULL) ||
	    (pipe->sink_cnt->dpipe_prio_set == NULL) ||
	    (pipe->src_cnt->dpipe_prio_clear == NULL) ||
	    (pipe->sink_cnt->dpipe_prio_clear == NULL))
	  return EOPNOTSUPP;

	/* 
	 * Assuming buffered pipe so that we need to do read bw allocation
	 * for source, and write bw allocation for sink. Once we get direct
	 * data pipe in place, this assumption is broken. 
	 */

	/* We should try to do everything before locking */

	error = pipe->src_cnt->dpipe_prio_vertices
	  (pipe->src_fd, &(src_vertices[0]), &src_num);
	if (error)
	  return error;
	if(src_num <= 0 || src_num > MAX_NUM_DISKS) {
		PRINTD("src_num is %d\n", src_num);
		return EINVAL;
	}
	src_old_req_bw = kmem_alloc(sizeof(bandwidth_t) * src_num, KM_NOSLEEP);
	src_old_alloc_bw = kmem_alloc(sizeof(bandwidth_t) * src_num, KM_NOSLEEP);
	src_mem_vhdl = kmem_alloc(sizeof(vertex_hdl_t) * src_num, KM_NOSLEEP);

	if ((src_old_req_bw == NULL) || (src_old_alloc_bw == NULL) ||
	    (src_mem_vhdl == NULL)) {
		PRINTD("dpipe_prio_alloc: src resource allocation failed.\n");
		if (src_old_req_bw != NULL)
		  kmem_free(src_old_req_bw, sizeof(bandwidth_t) * src_num);
		if (src_old_alloc_bw != NULL)
		  kmem_free(src_old_alloc_bw, sizeof(bandwidth_t) * src_num);
		if (src_mem_vhdl != NULL)
		  kmem_free(src_mem_vhdl, sizeof(vertex_hdl_t) * src_num);
		return ENOMEM;
	}
		
	error = pipe->sink_cnt->dpipe_prio_vertices
	  (pipe->sink_fd, &(sink_vertices[0]), &sink_num);
	if (error)
	  return error;
	if(sink_num <= 0 || sink_num > MAX_NUM_DISKS) {
		PRINTD("sink_num is %d\n", sink_num);
		return EINVAL;
	}
	sink_old_req_bw=kmem_alloc(sizeof(bandwidth_t) * sink_num, KM_NOSLEEP);
	sink_old_alloc_bw=kmem_alloc(sizeof(bandwidth_t) * sink_num, KM_NOSLEEP);
	sink_mem_vhdl=kmem_alloc(sizeof(vertex_hdl_t) * sink_num, KM_NOSLEEP);
	if ((sink_old_req_bw == NULL) || (sink_old_alloc_bw == NULL) ||
	    (sink_mem_vhdl == NULL)) {
		PRINTD("dpipe_prio_alloc: sink resource allocation failed.\n");
		if (sink_old_req_bw != NULL)
		  kmem_free(sink_old_req_bw, sizeof(bandwidth_t) * sink_num);
		if (sink_old_alloc_bw != NULL)
		  kmem_free(sink_old_alloc_bw, sizeof(bandwidth_t) * sink_num);
		if (sink_mem_vhdl != NULL)
		  kmem_free(sink_mem_vhdl, sizeof(vertex_hdl_t) * sink_num);
		kmem_free(src_old_req_bw, sizeof(bandwidth_t) * src_num);
		kmem_free(src_old_alloc_bw, sizeof(bandwidth_t) * src_num);
		kmem_free(src_mem_vhdl, sizeof(vertex_hdl_t) * src_num);
		return ENOMEM;
	}

	if ((error = getf(req->fd, &fp)) != 0)
	  return error;
	if (fp->vf_flag & FSOCKET) 
	  return EOPNOTSUPP;

#if IP30
	/* do read operation(s) on source */
	src_info.fp = fp;
	src_info.pid = req->pid;
	src_info.mode = PRIO_READ_ALLOCATE;
	src_info.req_bw = req->bytesec / src_num;
	src_info.alloc_bw = src_info.req_bw; /* same for now maybe should ask
						the pipe ends */

	/* do write operation(s) on sink */
	sink_info.fp = fp;
	sink_info.pid = req->pid;
	sink_info.mode = PRIO_WRITE_ALLOCATE;
	sink_info.req_bw = req->bytesec / sink_num;
	sink_info.alloc_bw = sink_info.req_bw;

	/* get lock and start to allocate bandwidth */
	if (prio_alloc_lock(req->pid, &holder) != PRIO_SUCCESS) {
		req->holder = holder;
		return ENOLCK;
	}

	for (i = 0; i < src_num; i++) {
		src_mem_vhdl[i] = mem_vhdl_get(src_vertices[i]);
		if (src_mem_vhdl[i] == GRAPH_VERTEX_NONE) {
			src_current = i;
			goto read_recover;
		}
		src_info.src = src_vertices[i];		
		src_info.sink = src_mem_vhdl[i];
		PRINTD("dpipe_prio_alloc: request %d bandwidth\n", 
		       src_info.req_bw);
		error = prio_alloc_set_bandwidth(&src_info, &holder, 
						 &(src_old_req_bw[i]), 
						 &(src_old_alloc_bw[i]));
		if (error != PRIO_SUCCESS) {
			src_current = i;
			goto read_recover;
		}
	}
	
	for (i = 0; i < sink_num; i++) {
		sink_info.sink = sink_vertices[i];
		sink_mem_vhdl[i] = mem_vhdl_get(sink_info.sink);
		if (sink_mem_vhdl[i] == GRAPH_VERTEX_NONE)
		  goto write_recover;
		sink_info.src = sink_mem_vhdl[i];
		if ((error = prio_alloc_set_bandwidth(&sink_info, &holder, 
						      &sink_old_req_bw[i],
						      &sink_old_alloc_bw[i]))
		    != PRIO_SUCCESS) {
			goto write_recover;
		}
	}
	goto done;
	
	/* if any allocation failed, we should release the allocated
	 * bandwidth */
      write_recover:
	for (j = 0; j < i; j++) {
		sink_info.req_bw = sink_old_req_bw[j];
		sink_info.alloc_bw = sink_old_alloc_bw[j];
		sink_info.sink = sink_vertices[j];
		sink_info.src = sink_mem_vhdl[j];
		prio_alloc_set_bandwidth(&sink_info, &holder, 
					 &sink_old_req_bw[j],
					 &sink_old_alloc_bw[j]);
	}
	
      read_recover:
	for (j = 0; j < src_current; j++) {
		src_info.req_bw = src_old_req_bw[j];
		src_info.alloc_bw = src_old_alloc_bw[j];
		src_info.sink = src_vertices[j];
		src_info.src = src_mem_vhdl[j];
		prio_alloc_set_bandwidth(&src_info, &holder, 
					 &src_old_req_bw[j],
					 &src_old_alloc_bw[j]);
	}
	
	
      done:
	prio_alloc_unlock(req->pid);

#endif /* IP30 */

	if (!error && req->bytesec) {
		pipe->src_cnt->dpipe_prio_set(pipe->src_conn.dpipe_id);
		pipe->sink_cnt->dpipe_prio_set(pipe->sink_conn.dpipe_id);
		
		fp->vf_flag |= FPRIORITY;
	} else if (!error && !req->bytesec) {
		pipe->src_cnt->dpipe_prio_clear(pipe->src_conn.dpipe_id);
		pipe->sink_cnt->dpipe_prio_clear(pipe->sink_conn.dpipe_id);
		fp->vf_flag &= ~FPRIORITY;
	}

	kmem_free(src_old_req_bw, sizeof(bandwidth_t) * src_num);
	kmem_free(src_old_alloc_bw, sizeof(bandwidth_t) * src_num);
	kmem_free(src_mem_vhdl, sizeof(vertex_hdl_t) * src_num);
	kmem_free(sink_old_req_bw, sizeof(bandwidth_t) * sink_num);
	kmem_free(sink_old_alloc_bw, sizeof(bandwidth_t) * sink_num);
	kmem_free(sink_mem_vhdl, sizeof(vertex_hdl_t) * sink_num);
	kmem_free(src_vertices, sizeof(dev_t) * MAX_NUM_DISKS);
	kmem_free(sink_vertices, sizeof(dev_t) * MAX_NUM_DISKS);

	PRINTD("dpipe_prio_alloc: exit with %d\n", error);
	return error;
}

static int 
dpipe_prio_get_bw(dpipe_t *pipe, prioAllocReq_t *req)
{
	vfile_t       *fp;
	dev_t        vertices[MAX_NUM_DISKS];
	int          num_vertices;
	vertex_hdl_t mem_vhdl;
	bandwidth_t  bytesec, sum;
	int          error = 0, i;
	
	PRINTD("dpipe_prio_get_bw: entry\n");

	if (getf(req->fd, &fp) != 0) {
		PRINTD("dpipe_prio_get_bw: getf %d fail\n", req->fd);
		return EINVAL;
	}

	if (!(fp->vf_flag & FPRIORITY)) {
		PRINTD("dpipe_prio_get_bw: no prio flag is set for the file\n");
		return EINVAL;
	}

#if IP30
	/* 
	 * assume the sum of one pipe end is equal to the sum of the other
	 * so we just do one.
	 */
	error = pipe->src_cnt->dpipe_prio_vertices
	  (pipe->src_fd, &(vertices[0]), &num_vertices);
	if (error) 
		return error;
	ASSERT(num_vertices > 0 && num_vertices <= MAX_NUM_DISKS);

	sum = 0;
	for (i = 0; i < num_vertices; i++) {
		mem_vhdl = mem_vhdl_get(vertices[i]);
		if (mem_vhdl == GRAPH_VERTEX_NONE)
		  return EIO;

		error = prio_alloc_get_bandwidth
		  (fp, vertices[0], mem_vhdl, &bytesec);
		if (error)
		  return error;
		sum += bytesec;
	}

	req->bytesec = sum;
	PRINTD("dpipe_prio_get_bw: result is 0x%d\n", sum);

#endif /* IP30 */
	return 0;
}

static int 
dpipe_active_trans(dpipe_t *pipe)
{
    int i, found;
    
    found = 0;
    for (i = 0; i < pipe->num_threads; i++)
	if (pipe->threads[i]->cur_transfer != NULL) {
	    found = 1;
	    break;
	}
    return found;
}
/*
 * Waits until all outstanding transfers on a datapipe are completed
 * Returns 0 on success.
 *         errno on failure.
 */
static int 
dpipe_flush(dpipe_t *pipe) 
{
    mutex_lock(&(pipe->lock), PZERO);
    /* We may want to check if there are pending trasnfers currently
       being serviced by a thread */
    while (dpipe_trans_queue_len(&pipe->pend_tqueue) != 0 ||
	   dpipe_active_trans(pipe)) {
	pipe->tlist_empty_waiter = 1;
	sv_wait(&(pipe->tlist_empty), PZERO, &pipe->lock, 0);
	mutex_lock(&(pipe->lock), PZERO);
    }
    mutex_unlock(&(pipe->lock));
    return 0;
}

/* Calls the dpipe_init func for both pipe ends if there is one */
static int
dpipe_init_ends(dpipe_t *pipe)
{
    dpipe_end_control_ops_t *src_cnt, *sink_cnt;
    int error;

    PRINTD("dpipe_init_ends: entry\n");
    
    src_cnt = pipe->src_cnt;
    sink_cnt = pipe->sink_cnt;
    
    if(sink_cnt->dpipe_init) {
	error = (*sink_cnt->dpipe_init)(pipe->sink_fd);
	if (error)
	    return error;
    }
    if(src_cnt->dpipe_init) {
	error = (*src_cnt->dpipe_init)(pipe->src_fd);
	if (error)
	    return error;
    }
    return 0;
}

/* Gets the attributes of each pipe end */
static int 
dpipe_get_end_attr(dpipe_t *pipe) 
{
    dpipe_end_control_ops_t *src_cnt, *sink_cnt;
    int error;

    PRINTD("dpipe_get_end_attr: entry\n");
    src_cnt = pipe->src_cnt;
    sink_cnt = pipe->sink_cnt;

    if (sink_cnt->dpipe_get_master_attr) {
	error = (*sink_cnt->dpipe_get_master_attr)(pipe->sink_fd, 
						 &(pipe->sink_attr));
	if (error)
	    return error;
    } else {
	return EINVAL;   /* Not ready to accept slaves */
    }
    if (src_cnt->dpipe_get_master_attr) {
	error = (*src_cnt->dpipe_get_master_attr)(pipe->src_fd, 
						  &(pipe->src_attr));
	if (error)
	    return error;
    } else {
	return EINVAL;   /* Not ready to accept slaves */
    }
    return 0;
}

/* Allocates the dpipe_thread_t which keeps track of a thread and launches
   an sthread.  Allocates a buffer to use in the buffered pipe */
static dpipe_thread_t *
dpipe_thread_alloc(dpipe_t *pipe, int buffer_size, int *error)
{
    dpipe_thread_t *thread;
    extern int dpipe_pri;
    
    PRINTD("dpipe_thread_alloc: entry\n");
    thread = (dpipe_thread_t *)kmem_zalloc(sizeof(dpipe_thread_t), 
					   KM_SLEEP);
    thread->pipe = pipe;  
    thread->buffer = kmem_alloc(buffer_size, KM_NOSLEEP);

    if (thread->buffer == NULL) {
	PRINTD("dpipe_thread_alloc: buffer allocation failed\n");	
	kmem_free(thread, sizeof(dpipe_thread_t));
	if (error)
	    *error = ENOMEM;
	return NULL;
    }

    initnsema(&(thread->exit_sema), 0, "dpipe exit_sema");
    sthread_create("dpipe", NULL, 4096 * 4, 0, dpipe_pri, KT_PS, 
		   (st_func_t *)dpipe_thread, (void *)thread, 
		   0, 0, 0);  
    return thread;
}

/* Attempts to destroy the thread */
static void dpipe_thread_dealloc(dpipe_thread_t *thread)
{

    PRINTD("dpipe_thread_dealloc: entry\n");
    PRINTD("dpipe_thread_dealloc: psema thread->exit_sema\n");
      
    freesema(&(thread->exit_sema));
    kern_munpin(thread->buffer, thread->buffer_size);
    kmem_free(thread->buffer, thread->buffer_size);

    kmem_free(thread, sizeof(dpipe_thread_t));
    PRINTD("dpipe_thread_dealloc: exit\n");
}

static int
dpipe_init_buf_pipe(dpipe_t *pipe)
{
    int i, error;
    dpipe_conn_attr_t *src_conn, *sink_conn;
    dpipe_end_attr_t  *src_attr, *sink_attr;


    PRINTD("dpipe_init_buf_pipe: entry\n");

    error = 0;
    src_attr = &pipe->src_attr;
    sink_attr = &pipe->sink_attr;
    src_conn = &pipe->src_conn;
    sink_conn = &pipe->sink_conn;
    
    /* This is going to be the connection attributes for all pipe 
       initially */
    if (!(src_attr->dpipe_end_role_flags & DPIPE_SOURCE_CAPABLE)) {
	PRINTD("dpipe_init_buf_pipe: src not source capable\n");
	return EINVAL;
    }
    src_conn->dpipe_conn_quantum = DPIPE_BUFFER_SIZE;
    src_conn->dpipe_conn_max_len_per_dest = DPIPE_BUFFER_SIZE;
    src_conn->dpipe_conn_max_num_of_dest = 1;
    src_conn->dpipe_conn_block_size = src_attr->dpipe_end_block_size;
    src_conn->dpipe_conn_alignment = src_attr->dpipe_end_alignment;
    src_conn->dpipe_conn_protocol_flags = DPIPE_SIMPLE_PROTOCOL;
    src_conn->dpipe_conn_status_flags = DPIPE_CONNECTED;
    src_conn->dpipe_id = pipe->id;
    src_conn->dpipe_conn_dma_flags = pipe->src_attr.dpipe_end_dma_flags;
  
    if (!(sink_attr->dpipe_end_role_flags & DPIPE_SINK_CAPABLE)) {
	PRINTD("dpipe_init_buf_pipe: src not source capable\n");
	return EINVAL;
    }
    sink_conn->dpipe_conn_quantum = DPIPE_BUFFER_SIZE;
    sink_conn->dpipe_conn_max_len_per_dest = DPIPE_BUFFER_SIZE;
    sink_conn->dpipe_conn_max_num_of_dest = 1;
    sink_conn->dpipe_conn_block_size = src_attr->dpipe_end_block_size;
    sink_conn->dpipe_conn_alignment = src_attr->dpipe_end_alignment;
    sink_conn->dpipe_conn_protocol_flags = DPIPE_SIMPLE_PROTOCOL;
    sink_conn->dpipe_conn_status_flags = DPIPE_CONNECTED;  
    sink_conn->dpipe_id = pipe->id + 50; /* hack */
    sink_conn->dpipe_conn_dma_flags = pipe->sink_attr.dpipe_end_dma_flags;


    error = (*pipe->src_cnt->dpipe_connect)(pipe->src_fd, 
					    DPIPE_SOURCE_CAPABLE, 
					    DPIPE_MASTER, src_conn, 
					    &(pipe->src_prot));
    if (error)
	return error;
    error = (*pipe->sink_cnt->dpipe_connect)(pipe->sink_fd, 
					     DPIPE_SINK_CAPABLE, 
					     DPIPE_MASTER, sink_conn, 
					     &(pipe->sink_prot));

     if (error)
	return error;
    
    dpipe_init_trans_queue(&pipe->pend_tqueue);
    dpipe_init_trans_queue(&pipe->done_tqueue);

    init_mutex(&(pipe->lock), MUTEX_DEFAULT, "dpipelock", pipe->id);
    init_sema(&(pipe->tlist_sema), 0, "tlist", pipe->id);
    init_sv(&pipe->tlist_empty, SV_FIFO, "tlist_sv", pipe->id);
    
    for (i = 0; i < DPIPE_NUM_THREADS; i++) {
	pipe->threads[i] = dpipe_thread_alloc(pipe, DPIPE_BUFFER_SIZE, &error);
	if (pipe->threads[i] == NULL)
	    break; /* Break out of for loop if there is an error */
	pipe->num_threads++;
    }
    if (pipe->num_threads == 0)
	return error;
    else
	return 0;
}

static int dpipe_init_trans_queue(dpipe_trans_queue_t *list) 
{
    list->head = NULL;
    list->tail = NULL;
    mutex_init(&list->lock, MUTEX_DEFAULT, "dpipe_q");
    list->count = 0;    
    return 0;
}

/* Queues the transfer at the end of the list */
static int
dpipe_enqueue_trans (dpipe_trans_queue_t *queue, dpipe_trans_t *item)
{
    mutex_lock(&queue->lock, PZERO);
    if (queue->head == NULL) {
	queue->head =  item;
	queue->tail = item;
	item->next = NULL;
	item->prev = NULL;
	queue->count = 1;
	mutex_unlock(&queue->lock);
	return 0;
    }
    if (queue->tail == NULL) {
	mutex_unlock(&queue->lock);
	return -1;
    } else {
	item->prev = queue->tail;
	item->next = NULL;
	queue->tail->next= item;
	queue->tail = item;
    }
    queue->count++;
    mutex_unlock(&queue->lock);
    return 0;
}

/* Returns the head of the list */
static dpipe_trans_t *
dpipe_dequeue_trans (dpipe_trans_queue_t *queue) 
{
    dpipe_trans_t *ret;

    mutex_lock(&queue->lock, PZERO);
    if ((queue == NULL) || (queue->head == NULL)) {
	mutex_unlock(&queue->lock);
	return NULL;
    }
    ret = queue->head;
    queue->head = queue->head->next;
    if (queue->head == NULL) {
	queue->tail = NULL;
    } else {
	queue->head->prev = NULL;
    }
    queue->count--;
    mutex_unlock(&queue->lock);
    return ret;
}

static int 
dpipe_trans_queue_len(dpipe_trans_queue_t *queue)
{
    int ret;

    mutex_lock(&queue->lock, PZERO);
    if (queue)
	ret = queue->count;
    else
	ret = -1;
    mutex_unlock(&queue->lock);
    return ret;
}



/* Frees the memory associated with each element on the transfer list */
static int
dpipe_destroy_trans_queue (dpipe_trans_queue_t *queue)
{
    dpipe_trans_t *elem, *list;
    int count = 0;
 
    list = queue->head;
    while (list != NULL) {
	elem = list->next;
	kmem_free(list, sizeof(dpipe_trans_t));
	count++;
	list = elem;
    }
    mutex_destroy(&queue->lock);
    return count;
}

static int
dpipe_finish_transfer(dpipe_t *pipe, dpipe_trans_t *trans, int cause) 
{
     dpipe_end_simple_protocol_ops_t *src_prot, *sink_prot;

     src_prot = &pipe->src_prot->simple;
     sink_prot = &pipe->sink_prot->simple;
     
     trans->status = cause;
     (*src_prot->dpipe_master_transfer_done)(trans->transfer_id, 
					     &pipe->src_conn, 
					     cause);
     (*sink_prot->dpipe_master_transfer_done)(trans->transfer_id, 
					      &pipe->sink_conn, 
					      cause);  
     dpipe_enqueue_trans(&pipe->done_tqueue, trans);
     return 0;
}

/* The function that the sthread executes */
static void 
dpipe_thread(dpipe_thread_t *thread) 
{
    dpipe_end_trans_ctx_hdl_t src_cookie, sink_cookie;
    int src_bytes, sink_bytes;
    __int64_t transfer_id;
    dpipe_slave_dest_t dest;
    dpipe_dest_elem_t dest_elem;
    dpipe_end_simple_protocol_ops_t *src_prot, *sink_prot;
    dpipe_conn_attr_t *src_conn, *sink_conn;
    int src_error, sink_error;

    PRINTD("Starting dpipe_thread\n");
    while (1) {
	psema(&thread->pipe->tlist_sema, PZERO);
	if (thread->exit) {
	    PRINTD("dpipe_thread: exiting\n");
	    break;
	}
	thread->cur_transfer = dpipe_dequeue_trans
	    (&(thread->pipe->pend_tqueue));
	if (thread->cur_transfer != NULL) {
	    src_cookie = thread->cur_transfer->src_cookie;
	    sink_cookie = thread->cur_transfer->sink_cookie;
	    transfer_id = thread->cur_transfer->transfer_id;
	} else {
	    goto out;
	}
	PRINTD("dpipe_thread: src_cookie: %x, sink_cookie: %x\n", src_cookie,
	       sink_cookie);
	src_prot = &thread->pipe->src_prot->simple;
	sink_prot = &thread->pipe->sink_prot->simple;
	
	src_conn = &thread->pipe->src_conn;
	sink_conn =  &thread->pipe->sink_conn;
	PRINTD("dpipe_thread: calling the prepare functions for each end\n");
	src_bytes = (*src_prot->dpipe_master_prepare)(transfer_id, src_cookie,
						      src_conn, NULL);
	PRINTD("dpipe_thread: src is prepared\n");
	sink_bytes = (*sink_prot->dpipe_master_prepare)(transfer_id, 
							sink_cookie,
							sink_conn, NULL);
	PRINTD("dpipe_thread: src bytes = 0x%x, sink_bytes = 0x%x\n", 
	       src_bytes, sink_bytes);
	if ((src_bytes != sink_bytes) || (src_bytes == DPIPE_SP_ERROR) ||
	    (src_bytes == DPIPE_SP_ERROR)) {
	    PRINTD("dpipe_thread: error at master_prepare\n");
	    dpipe_finish_transfer(thread->pipe, thread->cur_transfer, 
				  DPIPE_SP_ERROR);
	    goto out;
	} 
	
	/* Setup the destination, if the buffers are implemented as a
	   memory pipe end, this would look considerably different*/
	dest.dpipe_segflg = UIO_SYSSPACE;
	dest.dpipe_dest_count = 1;
	dest.dpipe_dest_array = &dest_elem;
	dest.dpipe_dest_array[0].dpipe_addr = (__uint64_t)thread->buffer;
	dest.dpipe_dest_array[0].dpipe_len = DPIPE_BUFFER_SIZE;
	
	while (sink_bytes > 0) {
	    PRINTD("dpipe_thread: Starting Trans. loop with 0x%x bytes left\n",
		   sink_bytes);
	    if (sink_bytes <  dest.dpipe_dest_array[0].dpipe_len) {
		dest.dpipe_dest_array[0].dpipe_len = sink_bytes;
	    }
	    src_error = (*src_prot->dpipe_master_start)(transfer_id, src_conn,
							&dest);
	    if (src_error) {
		dpipe_finish_transfer(thread->pipe, thread->cur_transfer, 
				      DPIPE_SP_ERROR);
		goto out;
	    }
	    sink_error = (*sink_prot->dpipe_master_start)(transfer_id, 
							  sink_conn, 
							  &dest);
	    if (sink_error) {
		dpipe_finish_transfer(thread->pipe, thread->cur_transfer, 
				      DPIPE_SP_ERROR);
		goto out;
	    }	    
	    sink_bytes -= dest.dpipe_dest_array[0].dpipe_len;
	    
	}
	dpipe_finish_transfer(thread->pipe, thread->cur_transfer, 0);
      out:
	mutex_lock(&(thread->pipe->lock), PZERO);
	thread->cur_transfer = NULL;

	/* If there is a waiter on the tlist_empty_waiter condition,
	   and the tlist is empty do a broadcast*/
	if ((thread->pipe->tlist_empty_waiter == 1) 
	    && (dpipe_trans_queue_len(&(thread->pipe->pend_tqueue)) == 0)) {
	    thread->pipe->tlist_empty_waiter = 0;
	    sv_broadcast(&thread->pipe->tlist_empty);
	}
	mutex_unlock(&(thread->pipe->lock));
    }
 
    PRINTD("dpipe_thread: thread exiting\n");
    vsema(&(thread->exit_sema));
    sthread_exit();
}

static int
dpipe_transfer(dpipe_t *pipe, dpipe_trans_t *transfer)
{
    PRINTD("dpipe_transfer:entry\n");
    
    dpipe_enqueue_trans(&(pipe->pend_tqueue), transfer);
    transfer->status = DPIPE_TRANS_PENDING;
    PRINTD("dpipe_transfer: vsema on pipe->tlist_sema 0x%x\n", 
	   &(pipe->tlist_sema));
    vsema(&(pipe->tlist_sema));
    PRINTD("dpipe_transfer:exit\n");
    return 0;
}

static dpipe_trans_t *
dpipe_lookup_trans_id(dpipe_trans_queue_t *queue, dpipe_transfer_id_t id)
{
    dpipe_trans_t *list;

    mutex_lock(&queue->lock, PZERO);
    list = queue->head;
    while (list != NULL) {
	if (list->transfer_id == id) {
	    mutex_unlock(&queue->lock);
	    return list; 
	} else {
	    list = list->next;
	}
    }
    mutex_unlock(&queue->lock);
    return NULL;
}


static int dpipe_get_status(dpipe_t *pipe, dpipe_transfer_id_t id)
{
    dpipe_trans_t  *scan;
    int i;
    int status = DPIPE_TRANS_UNKNOWN;

    mutex_lock(&(pipe->lock), PZERO);
    /* Look in the pend_tlist */
    scan = dpipe_lookup_trans_id(&pipe->pend_tqueue, id);
    
    /* If not there look in the done_tlist */
    if (scan == NULL)
	scan = dpipe_lookup_trans_id(&pipe->done_tqueue, id);
    
    /* If not in either list check the transfer current executing*/
    if (scan == NULL) {
	for (i = 0; i < pipe->num_threads; i++)
	    if (id == pipe->threads[i]->cur_transfer->transfer_id) {
		scan = pipe->threads[i]->cur_transfer;
		break;
	    }
    }
    if (scan)
	status = scan->status;
    PRINTD("dpipe_get_status: status = %d\n", status);
    mutex_unlock(&(pipe->lock));
    return status;
}

static void
dpipe_cleanup_threads(dpipe_t *pipe)
{
    int i;

    PRINTD("dpipe_cleanup_threads: entry\n");
    /* Can't exit threads very cleanly without doing flushing*/
    dpipe_flush(pipe);
    for (i = 0; i < pipe->num_threads; i++) {
	pipe->threads[i]->exit = 1;
    }
    for (i = 0; i < pipe->num_threads; i++) {
	vsema(&pipe->tlist_sema);
    }
    for (i = 0; i < pipe->num_threads; i++) {
	psema(&(pipe->threads[i]->exit_sema), PZERO);
    }
    for (i = 0; i < pipe->num_threads; i++) {
	dpipe_thread_dealloc(pipe->threads[i]);
    }
    PRINTD("dpipe_cleanup_threads: exit\n");   
}

/*ARGSUSED*/
int 
dpipeclose(dev_t dev, int flag, int otype, struct cred *cred) 
{
    int i, index, count;
    dpipe_t *pipe;
    dpipe_end_control_ops_t *src_cnt, *sink_cnt;

    PRINTD("dpipeclose: entry\n");
    pipe = dpipe_dev_to_dpipe(dev);
    index = pipe->id;
    if (pipe == NULL) {
	PRINTD("dpipeclose: ENODEV\n");
	return ENODEV;
    }
    src_cnt = pipe->src_cnt;
    sink_cnt = pipe->sink_cnt;

    /* First thing we should do is exit the sthread(s)*/

    if (pipe->num_threads > 0) {
	dpipe_cleanup_threads(pipe);
    }
    if (src_cnt && (src_cnt->dpipe_disconnect) 
	&& (pipe->src_conn.dpipe_conn_status_flags == DPIPE_CONNECTED)) {
	(*src_cnt->dpipe_disconnect)(pipe->src_fd, &pipe->src_conn);
	pipe->src_conn.dpipe_conn_status_flags = DPIPE_UNCONNECTED;
    }
    if (sink_cnt && (sink_cnt->dpipe_disconnect) 
	&& (pipe->sink_conn.dpipe_conn_status_flags == DPIPE_CONNECTED)) {
	(*sink_cnt->dpipe_disconnect)(pipe->sink_fd, &pipe->sink_conn);
	pipe->sink_conn.dpipe_conn_status_flags = DPIPE_UNCONNECTED;	
    }   

    mutex_destroy(&(pipe->lock));
    freesema(&(pipe->tlist_sema));
    sv_destroy(&pipe->tlist_empty);

    count = dpipe_destroy_trans_queue(&pipe->pend_tqueue);
    PRINTD("dpipeclose: pend_queue destroyed with %d left\n", count);
    count = dpipe_destroy_trans_queue(&pipe->done_tqueue);
    PRINTD("dpipeclose: done_queue destroyed with %d left\n", count);
    kmem_free(pipe, sizeof(dpipe_t));

    mutex_lock(dpipe_tab_lock, PZERO);
    dpipe_tab[index] = NULL;
    mutex_unlock(dpipe_tab_lock);
    PRINTD("\t\t\tid %d freed\n", index);
    return 0;   
}






