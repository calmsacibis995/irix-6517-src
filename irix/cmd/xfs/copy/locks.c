#define _SGI_MP_SOURCE
#include <sgidefs.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/param.h>
#include <assert.h>
#include <unistd.h>
#include <ulocks.h>
#include <string.h>
#include "locks.h"

extern usptr_t	*arena;

#define RCC_READ_DEBUG
#define RCC_WRITE_DEBUG

/* ARGSUSED */
thread_control *
thread_control_init(thread_control *mask, int num_threads)
{
	if ((mask->mutex = usnewsema(arena, 1)) == NULL)
		return(NULL);

	mask->num_working = 0;

	return(mask);
}

wbuf *
wbuf_init(wbuf *buf, int data_size, int data_alignment, int min_io_size, int id)
{
	buf->id = id;

	if ((buf->data = memalign(data_alignment, data_size)) == NULL)
		return(NULL);

	assert(min_io_size % BBSIZE == 0);

	buf->min_io_size = min_io_size;
	buf->size = MAX(data_size, 2*min_io_size);

	return(buf);
}

void
buf_read_start(void)
{
}

void
buf_read_end(thread_control *tmask, usema_t *mainwait)
{
	uspsema(tmask->mutex);

	tmask->num_working--;

	if (tmask->num_working == 0)  {
		usvsema(mainwait);
	}

	usvsema(tmask->mutex);
}

/*
 * me should be set to (1 << (thread_id%32)),
 * the tmask bit is already set to INACTIVE (1)
 */

void
buf_read_error(thread_control *tmask, usema_t *mainwait, thread_id id)
{
	uspsema(tmask->mutex);

	tmask->num_working--;
	target_states[id] = INACTIVE;

	if (tmask->num_working == 0)  {
		usvsema(mainwait);
	}

	usvsema(tmask->mutex);

}

void
buf_write_start(void)
{
}

void
buf_write_end(thread_control *tmask, usema_t *mainwait)
{
	uspsema(tmask->mutex);

	tmask->num_working--;

	if (tmask->num_working == 0)  {
		usvsema(mainwait);
	}

	usvsema(tmask->mutex);
}

