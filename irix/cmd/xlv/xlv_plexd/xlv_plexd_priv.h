#ifndef __XLV_PLEXD_PRIV_H_
#define __XLV_PLEXD_PRIV_H_

/**************************************************************************
 *                                                                        *
 *              Copyright (C) 1994, Silicon Graphics, Inc.                *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#ident "$Revision: 1.6 $"

/* This is not xlv_plexd's public interface. it *is* private */

#include <xlv_plexd.h>

/*
 * Plex daemon waits on a well-known queue for an array of revive-requests
 * pertaining to (possibly) different subvolumes. Whenever it receives one,
 * it queues the individual plex-requests according to their subvolumes in 
 * separate queues. It then spawns processes (upto a maximum) and assigns
 * them a request each (ie. the main process dequeues the sub-request ).
 * These processes exec copies of itself, but this time to do the syscall 
 * to revive the plex copy.
 */

#define XLV_PLEXD_HTAB_SZ 		10

#define MLOG_NORMAL	0
#define MLOG_VERBOSE	1
#define MLOG_DEBUG	2
#define MLOG_JOURNAL	3
/* 
 * If PIPE_QUEUE is defined as a compile-time flag, we use named pipes    
 * to enqueue individual requests on corresponding subvolumes; the  
 * default is to have in-memory queues. 
 */

#ifndef PIPE_QUEUE
typedef struct qentry {
	void 	*value;
	struct 	qentry *next;
} xlv_plexd_queue_entry_t;
#endif

typedef struct {
#ifndef PIPE_QUEUE
	xlv_plexd_queue_entry_t *head;
	xlv_plexd_queue_entry_t *tail;
#else
	int 	fifofd;
	char 	fifoname[128];
#endif
	size_t  entrysz;
	u_int	nentries;
	u_int	nprocesses;
} xlv_plexd_queue_t;

typedef unsigned long	htab_key_t;

typedef struct xlv_plexd_htab_entry {
	htab_key_t			key;
	void				*value;
	struct xlv_plexd_htab_entry 	*next;
} xlv_plexd_htab_entry_t;

typedef struct {
	u_int 			nentries;
	int			lock;
	xlv_plexd_htab_entry_t 	*table[ XLV_PLEXD_HTAB_SZ ];
} xlv_plexd_htab_t;

	
typedef struct {
	long			pid;
	dev_t			svdevice;
	xlv_plexd_queue_t	*queue;
} xlv_plexd_pidarr_entry_t;


typedef struct {
	u_int 			 cur_nprocesses;
	u_int			 max_nprocesses;
	u_int			 block_size;
	u_int			 sleep_intvl_msec;
	xlv_plexd_pidarr_entry_t *pidarr;
	xlv_plexd_htab_t	 *subvol_tab;
} xlv_plexd_master_t;

typedef struct {
	xlv_plexd_queue_t	*queue;
	htab_key_t		 key;
} xlv_plexd_sort_t;

typedef u_int (* xlv_plexd_cb_t) ( void * );



/*----------------------------------------------------------------------*/
/* function prototypes                                                  */
/*                                                                      */
/*                                                                      */
/*----------------------------------------------------------------------*/


/*-------  Main Xlv Plex Daemon functions ------*/
int	 	plexd__wait_for_requests ( void );
void 		plexd__process_request (xlv_plexd_request_t *r);
dev_t		plexd__select_queue ( xlv_plexd_queue_t  **q );
int 		plexd__service_rqsts ( void );
int 		plexd__queue_up_rqst ( xlv_plexd_request_entry_t *r );
int 		plexd__free_process( long pid );
long 		plexd__spawn_request ( xlv_plexd_request_entry_t  *req );
void		plexd__start_daemon (u_int max, u_int sleepintvl,
				     u_int blksz);
void		plexd__serve_revive_request(char *device, 
					    dev_t     dev,
					    __int64_t start_blkno, 
					    __int64_t end_blkno, 
					    unsigned  block_size,
					    unsigned  sleep_intvl_msec);
u_int 		plexd__is_entry_good( void *q );

/*------- Some utilities, callbacks ------------*/
void 		sighandler(int);
void		mlog (int level, char *fmt, ...);
void 		usage (void);
void 		background(void);
int 		nentries_cmp( const void *q1, const void *q2 );

/*------- Queue --------------------------------*/
int			q_enqueue( xlv_plexd_queue_t *q, void *ent );
u_int			q_dequeue( xlv_plexd_queue_t *q, void **r);
xlv_plexd_queue_t * 	q_construct( size_t entsz );
void			q_destruct( xlv_plexd_queue_t *q );
#ifdef PIPE_QUEUE
void 			q_get_uuname( char *str );
#endif

/*------- Hash Table ---------------------------*/
void *		htab_getentry( xlv_plexd_htab_t *htab, htab_key_t key );
int		htab_putentry( xlv_plexd_htab_t *htab, htab_key_t key, 
			       void *val );
int		htab_getselect_entries( xlv_plexd_htab_t *htab,  
				        xlv_plexd_sort_t  *qarr, 
				        xlv_plexd_cb_t is_entry_good );
int		htab_delentry( xlv_plexd_htab_t *htab, htab_key_t key );
xlv_plexd_htab_t *htab_construct( void );
unsigned	htab_hash (htab_key_t key);
u_int		htab_nentries( xlv_plexd_htab_t *ht );



#endif
