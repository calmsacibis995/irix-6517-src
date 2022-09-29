#ident	"$Revision: 1.4 $"

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/buf.h>
#include "xfs_types.h"
#include "xfs_alist.h"
#include "sim.h"

int alist_nallocs;
struct alloc *alist_free, *alist_used;

void
add_alloced(xfs_fsblock_t bno, xfs_extlen_t len, int rt)
{
	struct alloc *ap;
	struct alloc *end;
	struct alloc *new;

	if (!alist_free) {
		new = malloc(ALLOC_INCR * sizeof(*new));
		alist_free = new;
		end = &new[ALLOC_INCR - 1];
		for (ap = new; ap < end; ap++)
			ap->next = ap + 1;
		end->next = 0;
	}
	ap = alist_free;
	alist_free = ap->next;
	ap->len = len;
	ap->bno = bno;
	ap->rt = rt;
	if (alist_used) {
		end = alist_used->prev;
		ap->next = alist_used;
		ap->prev = end;
		end->next = ap;
		alist_used->prev = ap;
	} else
		ap->next = ap->prev = ap;
	alist_used = ap;
	alist_nallocs++;
}

void
get_alloced(int c, xfs_fsblock_t *bno, xfs_extlen_t *len, int *rt, int remove)
{
	struct alloc *ap = alist_used;
	int i;

	if (c < alist_nallocs / 2)
		for (i = 0; i < c; i++)
			ap = ap->next;
	else
		for (i = alist_nallocs; i > c; i--)
			ap = ap->prev;
	*bno = ap->bno;
	*len = ap->len;
	*rt = ap->rt;
	if (remove) {
		alist_nallocs--;
		ap->prev->next = ap->next;
		ap->next->prev = ap->prev;
		if (ap == alist_used)
			alist_used = ap->next;
		ap->next = alist_free;
		alist_free = ap;
	}
}

void
print_alist()
{
	struct alloc *ap;

	for (ap = alist_used; ap; ap = ap->next)
		printf("%lld:%lld:%d ", ap->bno, ap->bno + ap->len - 1, ap->rt);
	printf("\n");
}
