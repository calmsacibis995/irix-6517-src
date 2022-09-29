#ident "$Revision: 1.1 $"

#include <sys/types.h>
#include <stddef.h>
#include <stdlib.h>
#include <sys/fs/xfs_types.h>
#include "bmap.h"

/*
 * Block mapping code taken from xfs_db.
 */

/*
 * Append an extent to the block entry.
 */
void
blkent_append(
	blkent_t	**entp,
	xfs_dfsbno_t	b,
	xfs_dfilblks_t	c)
{
	blkent_t	*ent;
	int		i;

	ent = *entp;
	*entp = ent = realloc(ent, BLKENT_SIZE(c + ent->nblks));
	for (i = 0; i < c; i++)
		ent->blks[ent->nblks + i] = b + i;
	ent->nblks += c;
}

/*
 * Make a new block entry.
 */
blkent_t *
blkent_new(
	xfs_dfiloff_t	o,
	xfs_dfsbno_t	b,
	xfs_dfilblks_t	c)
{
	blkent_t	*ent;
	int		i;

	ent = malloc(BLKENT_SIZE(c));
	ent->nblks = c;
	ent->startoff = o;
	for (i = 0; i < c; i++)
		ent->blks[i] = b + i;
	return ent;
}

/*
 * Prepend an extent to the block entry.
 */
void
blkent_prepend(
	blkent_t	**entp,
	xfs_dfsbno_t	b,
	xfs_dfilblks_t	c)
{
	int		i;
	blkent_t	*newent;
	blkent_t	*oldent;

	oldent = *entp;
	newent = malloc(BLKENT_SIZE(oldent->nblks + c));
	newent->nblks = oldent->nblks + c;
	newent->startoff = oldent->startoff - c;
	for (i = 0; i < c; i++)
		newent->blks[i] = b + c;
	for (; i < oldent->nblks + c; i++)
		newent->blks[i] = oldent->blks[i - c];
	free(oldent);
	*entp = newent;
}

/*
 * Allocate a block map.
 */
blkmap_t *
blkmap_alloc(
	xfs_extnum_t	nex)
{
	blkmap_t	*blkmap;

	if (nex < 1)
		nex = 1;
	blkmap = malloc(BLKMAP_SIZE(nex));
	blkmap->naents = nex;
	blkmap->nents = 0;
	return blkmap;
}

/*
 * Free a block map.
 */
void
blkmap_free(
	blkmap_t	*blkmap)
{
	blkent_t	**entp;
	xfs_extnum_t	i;

	if (blkmap == NULL)
		return;
	for (i = 0, entp = blkmap->ents; i < blkmap->nents; i++, entp++)
		free(*entp);
	free(blkmap);
}

/*
 * Get one entry from a block map.
 */
xfs_dfsbno_t
blkmap_get(
	blkmap_t	*blkmap,
	xfs_dfiloff_t	o)
{
	blkent_t	*ent;
	blkent_t	**entp;
	int		i;

	for (i = 0, entp = blkmap->ents; i < blkmap->nents; i++, entp++) {
		ent = *entp;
		if (o >= ent->startoff && o < ent->startoff + ent->nblks)
			return ent->blks[o - ent->startoff];
	}
	return NULLDFSBNO;
}

/*
 * Get a chunk of entries from a block map.
 */
int
blkmap_getn(
	blkmap_t	*blkmap,
	xfs_dfiloff_t	o,
	xfs_dfilblks_t	nb,
	bmap_ext_t	**bmpp)
{
	bmap_ext_t	*bmp;
	blkent_t	*ent;
	xfs_dfiloff_t	ento;
	blkent_t	**entp;
	int		i;
	int		nex;

	for (i = nex = 0, bmp = NULL, entp = blkmap->ents;
	     i < blkmap->nents;
	     i++, entp++) {
		ent = *entp;
		if (ent->startoff >= o + nb)
			break;
		if (ent->startoff + ent->nblks <= o)
			continue;
		for (ento = ent->startoff;
		     ento < ent->startoff + ent->nblks && ento < o + nb;
		     ento++) {
			if (ento < o)
				continue;
			if (bmp &&
			    bmp[nex - 1].startoff + bmp[nex - 1].blockcount ==
				    ento &&
			    bmp[nex - 1].startblock + bmp[nex - 1].blockcount ==
				    ent->blks[ento - ent->startoff])
				bmp[nex - 1].blockcount++;
			else {
				bmp = realloc(bmp, ++nex * sizeof(*bmp));
				bmp[nex - 1].startoff = ento;
				bmp[nex - 1].startblock =
					ent->blks[ento - ent->startoff];
				bmp[nex - 1].blockcount = 1;
				bmp[nex - 1].flag = 0;
			}
		}
	}
	*bmpp = bmp;
	return nex;
}

/*
 * Make a block map larger.
 */
void
blkmap_grow(
	blkmap_t	**blkmapp,
	blkent_t	**entp,
	blkent_t	*newent)
{
	blkmap_t	*blkmap;
	int		i;
	int		idx;

	blkmap = *blkmapp;
	idx = (int)(entp - blkmap->ents);
	if (blkmap->naents == blkmap->nents) {
		blkmap = realloc(blkmap, BLKMAP_SIZE(blkmap->nents + 1));
		*blkmapp = blkmap;
		blkmap->naents++;
	}
	for (i = blkmap->nents; i > idx; i--)
		blkmap->ents[i] = blkmap->ents[i - 1];
	blkmap->ents[idx] = newent;
	blkmap->nents++;
}

/*
 * Return the last offset in a block map.
 */
xfs_dfiloff_t
blkmap_last_off(
	blkmap_t	*blkmap)
{
	blkent_t	*ent;

	if (!blkmap->nents)
		return NULLDFILOFF;
	ent = blkmap->ents[blkmap->nents - 1];
	return ent->startoff + ent->nblks;
}

/*
 * Return the next offset in a block map.
 */
xfs_dfiloff_t
blkmap_next_off(
	blkmap_t	*blkmap,
	xfs_dfiloff_t	o,
	int		*t)
{
	blkent_t	*ent;
	blkent_t	**entp;

	if (!blkmap->nents)
		return NULLDFILOFF;
	if (o == NULLDFILOFF) {
		*t = 0;
		ent = blkmap->ents[0];
		return ent->startoff;
	}
	entp = &blkmap->ents[*t];
	ent = *entp;
	if (o < ent->startoff + ent->nblks - 1)
		return o + 1;
	entp++;
	if (entp >= &blkmap->ents[blkmap->nents])
		return NULLDFILOFF;
	(*t)++;
	ent = *entp;
	return ent->startoff;
}

/*
 * Set a block value in a block map.
 */
void
blkmap_set_blk(
	blkmap_t	**blkmapp,
	xfs_dfiloff_t	o,
	xfs_dfsbno_t	b)
{
	blkmap_t	*blkmap;
	blkent_t	*ent;
	blkent_t	**entp;
	blkent_t	*nextent;

	blkmap = *blkmapp;
	for (entp = blkmap->ents; entp < &blkmap->ents[blkmap->nents]; entp++) {
		ent = *entp;
		if (o < ent->startoff - 1) {
			ent = blkent_new(o, b, 1);
			blkmap_grow(blkmapp, entp, ent);
			return;
		}
		if (o == ent->startoff - 1) {
			blkent_prepend(entp, b, 1);
			return;
		}
		if (o >= ent->startoff && o < ent->startoff + ent->nblks) {
			ent->blks[o - ent->startoff] = b;
			return;
		}
		if (o > ent->startoff + ent->nblks)
			continue;
		blkent_append(entp, b, 1);
		if (entp == &blkmap->ents[blkmap->nents - 1])
			return;
		ent = *entp;
		nextent = entp[1];
		if (ent->startoff + ent->nblks < nextent->startoff)
			return;
		blkent_append(entp, nextent->blks[0], nextent->nblks);
		blkmap_shrink(blkmap, &entp[1]);
		return;
	}
	ent = blkent_new(o, b, 1);
	blkmap_grow(blkmapp, entp, ent);
}

/*
 * Set an extent into a block map.
 */
void
blkmap_set_ext(
	blkmap_t	**blkmapp,
	xfs_dfiloff_t	o,
	xfs_dfsbno_t	b,
	xfs_dfilblks_t	c)
{
	blkmap_t	*blkmap;
	blkent_t	*ent;
	blkent_t	**entp;
	xfs_extnum_t	i;

	blkmap = *blkmapp;
	if (!blkmap->nents) {
		blkmap->ents[0] = blkent_new(o, b, c);
		blkmap->nents = 1;
		return;
	}
	entp = &blkmap->ents[blkmap->nents - 1];
	ent = *entp;
	if (ent->startoff + ent->nblks == o) {
		blkent_append(entp, b, c);
		return;
	}
	if (ent->startoff + ent->nblks < o) {
		ent = blkent_new(o, b, c);
		blkmap_grow(blkmapp, &blkmap->ents[blkmap->nents], ent);
		return;
	}
	for (i = 0; i < c; i++)
		blkmap_set_blk(blkmapp, o + i, b + i);
}

/*
 * Make a block map smaller.
 */
void
blkmap_shrink(
	blkmap_t	*blkmap,
	blkent_t	**entp)
{
	int		i;
	int		idx;

	free(*entp);
	idx = (int)(entp - blkmap->ents);
	for (i = idx + 1; i < blkmap->nents; i++)
		blkmap->ents[i] = blkmap->ents[i - 1];
	blkmap->nents--;
}
