#ident "$Revision: 1.4 $"

/**************************************************************************
 *									  *
 *            Copyright (C) 1993-1994, Silicon Graphics, Inc.		  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/

#ifndef _XLV_MEM_H
#define _XLV_MEM_H

extern __uint32_t	xlv_top_mem_bufs, xlv_low_mem_bufs;

/*
 * General purpose reserved memory allocator:
 * This allocator sets aside reserved memory to be used in low memory
 * situations when sleeping for memory is not an option.  It is mainly
 * used so that xlv volumes can be used as swap devices.
 */

/* types */
#ifndef __SYS_XLV_LOCK_H__
typedef struct {
    buf_t		 *head;
    buf_t		 *tail;
    unsigned		  count;
    unsigned short	  n_queued;
    unsigned short	  max_queued;
} buf_queue_t;
#endif /* __SYS_XLV_LOCK_H__ */

typedef struct {
    lock_t	  lock;
    __uint32_t	  size, slots, active, quiesce, waiters;
    __uint32_t	  totalhits, totalmisses, hits, misses, waits;
    __uint32_t	  scale, threshold, resized, resizefailed;
    boolean_t	  memresize;
    sema_t	  io_wait, quiesce_wait;
    void	 *res_mem_start, *res_mem, *res_mem_last;
    char	 *name;
    buf_queue_t	  memory_wait_queue;
} xlv_res_mem_t;


/* macros */
#define BUF_QUEUE_HEAD(q) (q).head
#define BUF_QUEUE_TAIL(q) (q).tail
#define BUF_QUEUE_ISEMPTY(q) (BUF_QUEUE_HEAD(q) == NULL)
#define BUF_QUEUE_NEXT(b) (b)->av_forw

#ifdef XLV_QUEUE_STATS
#define BUF_QUEUE_INIT(q) BUF_QUEUE_HEAD(q) = NULL; (q).count = (q).n_queued = (q).max_queued = 0;

#define BUF_ENQUEUE(q, e) {						\
	if (BUF_QUEUE_HEAD(q)) BUF_QUEUE_NEXT(BUF_QUEUE_TAIL(q)) = (e); \
	else BUF_QUEUE_HEAD(q) = (e);					\
	BUF_QUEUE_TAIL(q) = (e);					\
	BUF_QUEUE_NEXT(e) = NULL;					\
	(q).count++;							\
	(q).n_queued++;							\
	if ((q).n_queued > (q).max_queued) (q).max_queued++;		\
}

#define BUF_DEQUEUE(q, e) {				\
	(e) = BUF_QUEUE_HEAD(q);			\
	if (e) BUF_QUEUE_HEAD(q) = BUF_QUEUE_NEXT(e);	\
	(q).n_queued--;					\
}

#define BUF_QUEUE_EMPTY(q, e) {		\
	(e) = BUF_QUEUE_HEAD(q);	\
	BUF_QUEUE_HEAD(q) = NULL;	\
	(q).n_queued = 0;		\
}
#else /* XLV_QUEUE_STATS */
#define BUF_QUEUE_INIT(q) BUF_QUEUE_HEAD(q) = NULL

#define BUF_ENQUEUE(q, e) {						\
	if (BUF_QUEUE_HEAD(q)) BUF_QUEUE_NEXT(BUF_QUEUE_TAIL(q)) = (e); \
	else BUF_QUEUE_HEAD(q) = (e);					\
	BUF_QUEUE_TAIL(q) = (e);					\
	BUF_QUEUE_NEXT(e) = NULL;					\
}

#define BUF_DEQUEUE(q, e) {				\
	(e) = BUF_QUEUE_HEAD(q);			\
	if (e) BUF_QUEUE_HEAD(q) = BUF_QUEUE_NEXT(e);	\
}

#define BUF_QUEUE_EMPTY(q, e) {		\
	(e) = BUF_QUEUE_HEAD(q);	\
	BUF_QUEUE_HEAD(q) = NULL;	\
}
#endif /* XLV_QUEUE_STATS */


/* Global variables */

extern xlv_res_mem_t	xlv_ioctx_mem, xlv_data_buf, xlv_mmap_bufs, xlv_top_mem,
			xlv_low_mem;

/* forward declaration */
struct xlv_attr_memory;

/* Prototypes */

void xlv_res_mem_create(xlv_res_mem_t *rm, char *name);
int xlv_res_mem_resize(xlv_res_mem_t *rm, __uint32_t size, __uint32_t slots,
			boolean_t growpool_slots);
int xlv_res_mem_adjust(struct xlv_attr_memory *params, xlv_res_mem_t *rm);
void xlv_res_mem_destroy(xlv_res_mem_t *rm);
void *xlv_res_mem_alloc(xlv_res_mem_t *rm, buf_t *bp, __uint32_t zerosize);
void xlv_res_mem_free(xlv_res_mem_t *rm, void *ptr);


#define IS_MASTERBUF(bp) ((emajor((bp)->b_edev) == xlvmajor) || \
                          (emajor((bp)->b_edev) == XLV_LOWER_MAJOR))

#endif /* _XLV_MEM_H */
