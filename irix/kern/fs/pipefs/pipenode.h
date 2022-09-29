/*
 * Definitions for "pipe" filesystem.
 *
 */

#ident  "$Revision: 1.7 $"

#include <sys/types.h>
#include <sys/poll.h>
#include <sys/sema.h>

/*
 * Each pipes object is identified by a struct pipenode/vnode pair.
 */
typedef struct pipenode {
	ushort		pi_flags;	/* state flags */
	ushort          pi_ino;         /* node id for pipes */
	short		pi_frptr;	/* read pointer */
	short		pi_fwptr;	/* write pointer */
	caddr_t		pi_bp;		/* buffer holding data */
	time_t  	pi_atime;	/* creation times for pipe */
	time_t  	pi_mtime;	
	time_t  	pi_ctime;	
	sv_t		pi_empty;	/* counting sema for waiting empty */
	sv_t		pi_full;	/* counting sema for waiting full */
	mutex_t		pi_lock;	/* read/write lock for pipe */
	sema_t		pi_fwcnt;	/* number of writers count */
	sema_t		pi_frcnt;	/* number of readers count */
	long		pi_size;	/* pipe size */
	struct		pollhead pi_rpq; /* poll/select process queues */
	struct		pollhead pi_wpq;
	bhv_desc_t	pi_bhv_desc;	/* pipefs behavior descriptor */
}pipenode_t;

#define	BHVTOP(bdp) 	((struct pipenode *)(BHV_PDATA(bdp)))
#define	PTOBHV(pp) 	(&((pp)->pi_bhv_desc))	
#define PTOV(pp) 	BHV_TO_VNODE(PTOBHV(pp))

/*
 * Pipe flags.
 */
#define	PIPE_WCLOSED	0x0001	/* last writer has closed */

#ifdef _KERNEL
extern int oldpipe(void *uap, rval_t *rvp);
#endif
