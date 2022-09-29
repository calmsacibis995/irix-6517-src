#include <sys/param.h>
#include <sys/buf.h>
#include <sys/iobuf.h>
#include <sys/errno.h>
#include <sys/sbd.h>
#include <saio.h>	/* for the dummy u struct */
#include <libsc.h>
#include <libsk.h>

#ident "$Revision: 1.17 $"

struct pfdat    *pfdat;         /* Page frame database. */

/* trimmed version of kernel stdname */
char *
stdname(char *name, char *nmbuf, int ctrl, int unit, int part)
{
	sprintf(nmbuf, "%s%dd%ds%d:", name, ctrl, unit, part);
	return nmbuf;
}

/* For the standalone environment, copyout() and copyin() are bcopy() */
int
copyin(void *f, void *t, int c)
{
	bcopy(f, t, c);
	return 0;
}

int
copyout(void *f, void *t, int c)
{
	bcopy(f, t, c);
	return 0;
}

/*	very trimmed down disksort; there is never more than one
	request at a time in standalone.
	actl isn't used.
*/
void
disksort(register struct iobuf *dp, register struct buf *bp)
{
	register struct buf *ap;

	/*	If nothing on the activity queue, then we become the only thing.
		This always be the case for standalone */
	ap = dp->io_head;
	if(ap == NULL) {
		dp->io_head = bp;
		bp->av_forw = NULL;
	}
	else {	/* just always tack on at end for now. shouldn't happen */
		register struct buf *tmp;
		while(tmp=ap->av_forw) {
			ap = tmp;
		}
		bp->av_forw = NULL;
		ap->av_forw = bp;
	}
}

void
iodone(buf_t *bp)
{
	geterror(bp);
	bp->b_flags |= B_DONE;
}

void *kern_malloc(size_t n)
{
	return (malloc(n));
}
void kern_free(void *s)
{
	free(s);
}
