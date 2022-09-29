/* aio.h - asynchronous I/O header file */

/*
modification history
--------------------
01b,26jan94,kdl  removed prototype for aio_fsync(); minor cleanup.
01a,04apr93,elh  written.
*/

#ifndef __INCaioh
#define __INCaioh

#ifdef __cplusplus
extern "C" {
#endif

/* includes */

#include "sys/types.h"
#include "signal.h"
#include "fcntl.h"
#include "private/aiopxlibp.h"

/* defines */
						
/* aio_cancel() return values */

#define	AIO_CANCELED			1	/* operations canceled */
#define	AIO_NOTCANCELED			2	/* operations are in progress */
#define	AIO_ALLDONE			3	/* operations complete */

/* lio_listio()  modes */

#define	LIO_WAIT			4	/* wait for completion */
#define	LIO_NOWAIT			5	/* don't wait for completion */

/* lio_listio() operations */
					 	/* lio op codes */
#define	LIO_READ			0	/* read operation */
#define	LIO_WRITE			1	/* write operation */
#define	LIO_NOP				2	/* no transfer operation*/


/* Asynchronous I/O control block */

struct aiocb
    {
    int			aio_fildes;		/* file descriptor */
    off_t		aio_offset;		/* file offset */
    volatile void *	aio_buf;		/* location of buffer */
    size_t		aio_nbytes;		/* length of transfer */ 
    int			aio_reqprio;		/* request priority offset */
    struct sigevent	aio_sigevent;		/* signal number and value */
    int			aio_lio_opcode;		/* operation to be performed */

    /* WRS addition */
    AIO_SYS		aio_sys;    		/* implementation-specific */
    };

/* forward declarations */

#if defined(__STDC__) || defined(__cplusplus)

extern int   	aio_read (struct aiocb *);
extern int 	aio_write (struct aiocb *);
extern int 	lio_listio (int, struct aiocb * [], int, 
		    	    struct sigevent *);
extern int 	aio_error (const struct aiocb *);
extern size_t	aio_return (struct aiocb *);
extern int 	aio_cancel (int, struct aiocb *);
extern int 	aio_suspend (const struct aiocb * [], int , 
		             const struct timespec *);

#else	/* __STDC__ */

extern int   	aio_read ();
extern int 	aio_write ();
extern int 	lio_listio ()
extern int 	aio_error ();
extern size_t	aio_return ();
extern int 	aio_cancel ();
extern int 	aio_suspend ();

#endif	/* __STDC__ */

#ifdef __cplusplus
}
#endif

#endif /* __INCaioh */
