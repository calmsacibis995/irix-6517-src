#include "fsdump.h"

static heap_set *prevmh;
static ino64_t prevbaseino = 0;
static inm2_t* previnm2;


hpindex_t *pnm1 (heap_set *mh, ino64_t inobase)
{
	return (mh->hp_inum + HASHFUNC(inobase));
}

inm2_t *PNM2 (heap_set *mh, ino64_t inobase)
{
	inm2_t *pnm2;

	if (prevmh == mh && prevbaseino == inobase)
		return previnm2;

	pnm2 = mh->hp_inm2 + *PNM1 (*mh, inobase);

	while (pnm2->startino != inobase && pnm2->nextptr != 0) {
		pnm2 = mh->hp_inm2 + pnm2->nextptr;
	}

	if (pnm2->startino == inobase) {
		prevmh = mh;
		prevbaseino = inobase;
		previnm2 = pnm2;
	} else {
		pnm2 = mh->hp_inm2;
	}

	return pnm2;
}

inod_t *PINO (heap_set *mh, ino64_t ino)
{
	int offsetino = (int)(ino & 63);

	if ((prevmh == mh) && ((ino & ~63) == prevbaseino))
		return mh->hp_inod + previnm2->inoptr[offsetino];

	return (mh->hp_inod + PNM2(mh, ino & ~63)->inoptr[offsetino]); 
}

void PADD(heap_set *mh, hpindex_t inod, ino64_t ino)
{
	hpindex_t offsetino = (hpindex_t)(ino & 63);
	ino64_t baseino = ino & ~63;
	hpindex_t *inm1;
	inm2_t *inm2;

	if (mh == prevmh && prevbaseino == baseino) {
		previnm2->inoptr[offsetino] = inod;
	}
	
	inm1 = PNM1 (*mh, baseino);
	if (*inm1 == 0) {	
		*inm1 = heapalloc ((void **)(&mh->hp_inm2), 1);
		inm2 = mh->hp_inm2 + *inm1;
		inm2->startino = baseino;
	} else {
		inm2 = mh->hp_inm2 + *inm1;
		while(inm2->startino != baseino) {
			if (inm2->nextptr == 0) {
				hpindex_t inm2x = heapalloc ((void **)(&mh->hp_inm2), 1);
				inm2->nextptr = inm2x; 	
				inm2 = mh->hp_inm2 + inm2x; 
				inm2->startino = baseino;
			} else {
				inm2 = mh->hp_inm2 + inm2->nextptr;
			}
		}
	}
	inm2->inoptr[offsetino] = inod;
	prevbaseino = baseino;
	previnm2 = inm2;
	prevmh = mh;
}	
