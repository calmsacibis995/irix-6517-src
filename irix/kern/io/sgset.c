/* Copyright 1986, Silicon Graphics Inc., Mountain View, CA. */
#ident "$Revision: 3.11 $"
#include "sys/types.h"
#include "sys/param.h"
#include "sys/buf.h"
#include "sys/debug.h"
#include "sys/sg.h"

extern int kvsgset(caddr_t, int, struct sg *, int, unsigned int *);

/*
 * Set up the scatter / gather vector for a data transfer.
 * Return the actual number of sg entries used.  If more than
 * maxvec are needed, pass back the leftover count in *_resid.
 *
 * It is up to the caller to convert the generic scatter / gather
 * vector to the appropriate device specific form.
 */
/*ARGSUSED*/
/*ARGSUSED*/
int
sgset(
	struct buf *bp,
	struct sg *vec,
	int maxvec,
	unsigned int *_resid)
{
#if EVEREST
	/* must use dma mapping on Everest */
	return -1;
#else /* !EVEREST */
	register unsigned count;
	register caddr_t vaddr;

	ASSERT(bp->b_dmaaddr != NULL);

	if (!(BP_ISMAPPED(bp)))
		return -1;

	/*
	 * Call virtual-address-space-independent kvsgset.
	 */
	count = bp->b_resid;
	vaddr = bp->b_dmaaddr + bp->b_bcount - count;

	return(kvsgset(vaddr, count, vec, maxvec, _resid));
#endif /* !EVEREST */
}


#ifdef notdef
/*
 * display the scatter / gather vector, for debugging
 */
sgprint(vec, nvec)
	struct sg *vec;
	int nvec;
{
	register struct sg *sg;
	register int i;
	register unsigned long count;

	count = 0;
	sg = vec;
	for (i = 0; i < nvec; i++)
		count += sg++ ->sg_bcount;

	printf(" {%d:", count);
	sg = vec;
	for (i = 0; i < nvec; i++) {
		printf(" 0x%x/%d", sg->sg_ioaddr, sg->sg_bcount);
		sg++;
	}
	printf("}\n");
}
#endif
