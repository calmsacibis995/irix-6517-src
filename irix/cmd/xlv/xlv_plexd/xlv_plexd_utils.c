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
#ident "$Revision: 1.4 $"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <signal.h>
#include <paths.h>
#include <assert.h>
#include <stdarg.h>
#include <sys/uuid.h>
#include <sys/flock.h>
#include <sys/file.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/debug.h>
#include <sys/wait.h>
#include <sys/dkio.h>
#include <sys/xlv_base.h>
#include <sys/xlv_tab.h>
#include <xlv_utils.h>
#include "xlv_plexd_priv.h"




/*----------------------------------------------------------------------*/
/* queue                                                                */
/*                                                                      */
/* simple, basic FIFO queue, which completely hides whether it is using */
/* a named pipe or in-memory queue.                                     */
/*                                                                      */
/* XXXsup This queue is almost a duplicate of that of xlv_shutdown,     */
/* except for the field nprocesses, and its capability to use named     */
/* pipes (among other things :) ) These two should be consolidated ... 	*/
/*----------------------------------------------------------------------*/




int
q_enqueue( 
	xlv_plexd_queue_t 	*q, 
	void 			*ent )
{

/* 
 * If PIPE_QUEUE is defined as a compile-time flag, we use named pipes    
 * to enqueue individual requests on corresponding subvolumes; the  
 * default is to have in-memory queues. Just for the heck of it :)  
 */

#ifndef PIPE_QUEUE
	xlv_plexd_queue_entry_t *qe;

	qe = malloc (sizeof (xlv_plexd_queue_entry_t));
	
	qe->value = ent;
	qe->next = NULL;

	if (q->head != NULL) {
		q->tail->next = qe;
		q->tail = qe;
	}		
	else
		q->tail = q->head = qe;
#else
	if (write (q->fifofd, ent, q->entrysz) != q->entrysz ) {
		mlog(MLOG_NORMAL,
		     "Write failed on %s : %s\n",
		     ent->subvol_name,
		     strerror (errno));
		return -1;
	}
#endif
	q->nentries++;

	return 1;
}









unsigned int
q_dequeue( 
	xlv_plexd_queue_t 	*q, 
	void 			**entpp )
{	
	u_int num;
	*entpp = NULL;

	num = q->nentries;
	
	if (! num) {
		return 0;
	}

#ifndef PIPE_QUEUE
	assert ( q->head );
	{
		xlv_plexd_queue_entry_t *qe;
		qe = q->head;
		q->head = qe->next;
		*entpp = qe->value;
		free (qe);
	}

#else
	/* we already know that there's something on the queue,
	   so this is guaranteed to not block */
	*entpp = malloc (sizeof (xlv_plexd_request_entry_t));
	if (read (q->fifofd, *entpp, q->entrysz) != q->entrysz) {
		free (*entpp);
		*entpp = NULL;
		return num;
	}
#endif
	num = --(q->nentries);

	return num;
}













xlv_plexd_queue_t *
q_construct( 
	size_t entsz)
{
	xlv_plexd_queue_t *q;
	q = (xlv_plexd_queue_t *) calloc( 1, sizeof( xlv_plexd_queue_t ) );

	assert( q );
#ifndef PIPE_QUEUE
	q->head = q->tail = NULL;
#else
	q_get_uuname( q->fifoname );
	
	if (mknod (q->fifoname, S_IFIFO | 0666, 0))
		return NULL;
	q->fifofd = open (q->fifoname, O_RDWR); /* XXX ? */

	if (q->fifofd < 0)
		return NULL;
#endif
	q->entrysz = entsz;
	q->nentries = q->nprocesses = 0;
	return q;
}





void
q_destruct( 
	   xlv_plexd_queue_t *q )
{
	assert (! q->nentries);
	assert (! q->nprocesses);
	
#ifndef PIPE_QUEUE
	assert (! q->head );
#else
	close  (q->fifofd);
	unlink (q->fifoname);
#endif	
	free (q);
}





#ifdef PIPE_QUEUE
void
q_get_uuname( 
	char *str )
{
	char *unam = NULL;
	uuid_t	uid;

	uint_t stat;

	uuid_create (&uid, &stat);
	uuid_to_string (&uid, &unam, &stat);
	assert (stat == uuid_s_ok);
	
	strcpy (str, XLV_PLEXD_DIR);
	strcat (str, ".");
	strcat (str, unam);
	strcat (str, ".PlexdQueue");
	free (unam);
}

#endif








/*----------------------------------------------------------------------*/
/* hash table                                                           */
/*                                                                      */
/* this doesn't need any locking since only the main thread accesses it.*/
/*----------------------------------------------------------------------*/

#define HTAB_KEYS_EQL(a,b)	((a) == (b))

void *
htab_getentry( 
	xlv_plexd_htab_t 	*htab, 
	htab_key_t		 key )
{
	xlv_plexd_htab_entry_t 	*ent, *i;
	void 		*val = NULL;
	
	ent = htab->table [htab_hash (key)];
	for (i = ent; i != NULL; i = i->next) {
		if (HTAB_KEYS_EQL(i->key, key)) {
			val = i->value;
			break;
		}
	}
	return val;
}

int
htab_getselect_entries( 
	xlv_plexd_htab_t 	*htab,  
	xlv_plexd_sort_t  	*qarr, 
	xlv_plexd_cb_t 		is_entry_good )
{
	int i;
	xlv_plexd_htab_entry_t 	*ent, *first;
	int num = 0;

	for (i = 0; i < XLV_PLEXD_HTAB_SZ; i++ ) {
		first = htab->table [i];
		for (ent = first; ent != NULL; ent = ent->next) {
			if ( (*is_entry_good) (ent->value) ) {
				qarr[num].queue = ent->value;
				qarr[num].key = ent->key;
				num++;
			}
		}
	}
	
	return num;
}





int
htab_putentry( 
	xlv_plexd_htab_t 	*htab, 
	htab_key_t		key, 
	void 			*val )
{
	xlv_plexd_htab_entry_t 	*first, *i, *p, *ent;
	int h;

	h = htab_hash (key);
	first = htab->table [h];
	for (p = i = first; i != NULL; i = i->next) {
		if (HTAB_KEYS_EQL(i->key, key)) {
			mlog(MLOG_DEBUG,
			     "htab - attempt to duplicate entry\n");
			return -1;
		}
		p = i;
	}
	
	ent = (xlv_plexd_htab_entry_t *)
			malloc(sizeof(xlv_plexd_htab_entry_t));

	ent->value = val;
	ent->key = key;
	ent->next = NULL;
	
	/*
	 * Add the new entry to the end of the list.
	 */
	if ( first == NULL ) {
		htab->table [h] = ent;
	} else { 
		p->next = ent;
	}
	
	htab->nentries++;

	return 1;
}









int
htab_delentry( 
	xlv_plexd_htab_t *htab, 
	htab_key_t	  key)
{
	xlv_plexd_htab_entry_t 	*first, *i, *p;
	int h;
	int found = 0;

	h = htab_hash (key);
	
	first = htab->table [h];
	for (p = i = first; i != NULL; p = i, i = i->next) {
		if (HTAB_KEYS_EQL(i->key, key)) {
			found = 1;
			break;
		}
		
	}
	
	if ( !found ) {
		mlog(MLOG_NORMAL,
		     "htab_delete_entry - entry not found\n"
		     );
		return -1;
	}

	if ( i == first ) {
		htab->table [h] = i->next;
	}
	else {
		p->next = i->next;
	}
	free (i); /* we dont actually destruct the queue here -
		     just the hash table entry */
	
	htab->nentries--;

	return 1;
}




u_int
htab_nentries( 
	xlv_plexd_htab_t *ht)
{
	return ht->nentries;
}



xlv_plexd_htab_t *
htab_construct( void )
{
	int i;
	xlv_plexd_htab_t *ht;

	ht = (xlv_plexd_htab_t *) malloc( sizeof( xlv_plexd_htab_t ) );
	ht->nentries = 0;
	
	for (i = 0; i < XLV_PLEXD_HTAB_SZ; i++ ) 
		ht->table[i] = NULL;
	return ht;
}


unsigned int
htab_hash (
	htab_key_t key)
{
	return (key % XLV_PLEXD_HTAB_SZ);
}






