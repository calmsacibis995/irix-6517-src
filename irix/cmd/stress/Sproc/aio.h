#ifndef __AIO_H__
#define __AIO_H__

#include "sys/types.h"
#define _POSIX_ASYNCHRONOUS_IO	1
#define _POSIX_PRIORITIZED_IO	1

/*
 * aio - POSIX 1003.4 Draft 10
 */
#if _IRIX4
/* these belong in signal.h */
typedef long signal_t;
typedef struct sigevent {
	void	*sevt_value;
	signal_t sevt_signo;
} sigevent_t;
#endif

/* these belong in limits.h */
#define AIO_PRIO_MIN	1
#define AIO_PRIO_MAX	100

typedef  struct aiocb *aiohandle_t;
typedef struct aiocb {
	int	aio_whence;
	off_t	aio_offset;
	volatile char *aio_buf;
	size_t	aio_nbytes;
	int	aio_reqprio;
	sigevent_t aio_event;
	int	aio_flag;
	aiohandle_t aio_handle;
	size_t	aio_nobytes;	/* return bytes */
	int	aio_errno;
} aiocb_t;

#define AIO_EVENT	0x0001	/* notify via signal */
#define AIO_CANCELED	1	/* cancelled operation */
#define AIO_NOTCANCELED 2	/* some ops not cancelled */
#define AIO_ALLDONE	3	/* all aio has completed */

/* XXX should be in errno.h */
#define EINPROG		1001

int aio_read(int, aiocb_t *);
int aio_write(int, aiocb_t *);
int aio_cancel(int, aiocb_t *);
#define aio_error(h)	(h)->aio_errno
#define aio_return(h)	(h)->aio_nobytes
int aio_suspend(int, const aiocb_t *[]);
void _aio_init(int);

#endif
