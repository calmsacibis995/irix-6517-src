#ident "$Revision: 1.18 $"

/*
 * Directory management.
 */
#include "fsck.h"

# define dirblkLIMIT	((char *)dirblk + EFS_DIRBSIZE)
# define NEXTDENT(dep)	NEXTDENTWITHSIZE(dep, efs_dentsize(dep))
# define NEXTDENTWITHSIZE(dep,size) \
	 ((DIRECT *)((char *)(dep) + (size)))

# define DotCheck(name,len) \
	 ((name[0] == '.') && ((len == 1) || ((name[1] == '.') && (len == 2))))

/* Chkblk(): function used to check directory blocks.
 *
 * It is sort of a dual-purpose function. It's called during pass 1 on every
 * directory block seen: at that stage, attempts are made to fix anything
 * wrong.
 *
 * It is also called in pass 2 before trying to scan a dir block's entries:
 * in this mode it just returns a go/no-go answer. (Except: the one thing
 * that is checked & possibly fixed in pass 2 is the .. inum, since we don't
 * know that in pass 1).
 *
 * This double call is rather inelegant, but unfortunately in interactive 
 * mode, the user may elect NOT to fix problems seen in pass 1, so we MUST 
 * have a safety net: (there's no easy way to store the "not fixed" status of 
 * a block between passes). 
 *
 * The following things are checked for:
 *	1. directory block is within the filesystem
 *	2. directory block has the correct magic number
 *	3. number of slots is <= possible maximum entries
 *	4. freespace pointer does not overlap slot array
 *	5. slot offsets are within allocated space
 *	6. slot offsets do not collide
 *	7. directory entries are within allocated space
 *	8. directory entry inum is within filesystem range
 *	9. directory entries do not overlap
 *	10. directory entries name fields do not contain nulls, slashes
 *
 *	And, if checkdot, indicating it's the first block of the dir:
 *
 *	11. entry for "." exists and is correct.
 *	12. entry for ".." exists. And, in pass 2, is correct.
 *
 * Everything possible is done in pass 1 to make sure that the block ends up 
 * at least minimally legal; in the worst case the block is coerced to
 * be an empty dir block. This may lose the inum of "..", but it will be
 * reinstated if necessary (& possible) in pass 2: in pass 1, we do not have
 * knowledge of the parent inum. Missing entries for "." and ".." are 
 * synthesized if missing (space permitting); since we don't have the parent 
 * inum in pass 1, ".." is temporarily set to EFS_ROOTINO; it'll be fixed in 
 * pass 2.
 *
 * However, if a user in interactive mode elects NOT to fix a problem, we
 * quit checking: caveat "no" answerer!!!
 *
 * Information is accumulated in the globals dstateflag & dentrycount; 
 * once all the blocks of a directory inode have been checked, higher
 * level logic in phase1.c calls direvaluate (below) to decide whether 
 * the inode should be kept or wiped.
 *
 * In pass 1, returns a value indicating whether the block was successfully 
 * read from disk, to tell the cacheing system whether to scarf it in.
 *
 * OR, after pass 1, returns a value telling dirscan whether to try to
 * scan the entries.
 */

#define BADINUM ((efs_ino_t)-1)

int
chkblk(efs_daddr_t blk, int checkdot,
	efs_ino_t pinum,   /* parent inum: passed only in pass 2 */
	efs_ino_t curinum) /* current inum, passed only in pass 2. Global inum
			    * is valid in pass 1. */
{
	DIRECT *dirp;
	char *ptr;
	unchar slot;
	struct efs_dirblk *db;
	char *why;
	int freeoff, slotoff, other, otherslotoff;
	struct efs_dent *e1, *e2;
	int fixit;
	int nameScrewed;
	int i;
	efs_ino_t entryinum;
	int badmagic = 0;
	efs_ino_t dot = BADINUM;
	efs_ino_t ddot = BADINUM;
	int fixfreep = 0;
	int entrycnt = 0;

	if (!is_data_block(blk))
	{
		dstateflag |= DBADBLOCK;
		return NO; 
	}
	if (getblk(&fileblk, blk) == NULL)
	{
		dstateflag |= DBADBLOCK;
		return NO;
	}
	if (pass == 1 && !checkdot)
		ddot = get_ddot(inum);

	/*
	 * Make sure directory block has the correct magic number.
	 * If not, quietly fix it. 
	 * However, note the bad magic number: if any further errors
	 * are seen it's likely that a complete wrong block write has happened
	 * and we're no longer looking at a directory block: in that case,
	 * we'll take the conservative view and wipe it.
	 */
	db = (struct efs_dirblk *) dirblk;
	if (db->magic != EFS_DIRBLK_MAGIC) 
	{
		if (pass > 1)
			return NO;
		badmagic = 1;
		dstateflag |= DFIXED;
		db->magic = EFS_DIRBLK_MAGIC;
		fbdirty();
	}

	/* Check db->slots & db->firstused are at least plausible */

	freeoff = EFS_DIR_FREEOFF(db->firstused);

	if (db->slots == 0)
	{
	/* No slots:  db->firstused should be 0. */

		if (db->firstused != 0)
		{
			if (pass > 1)
				return NO;
			if (badmagic)  /* whole block's probably garbage */
			{
	    			why = "DIRECTORY HAS BAD FREESPACE POINTER";
				goto bad_dirblk;
			}
			fixit = 0;
			if (!qflag)
				idprintf("BAD FREESPACE POINTER IN DIR BLK %d",blk);
			else
				fixit = 1;
			if (gflag)
				gerrexit();
			if (!fixit &&  (reply("FIX") == YES))
				fixit = 1;
			if (fixit)
			{
				db->firstused = 0;
				dstateflag |= DFIXED;
				fbdirty();
			}
			else
			{
				dstateflag |= DCORRUPT;
				return YES;
			}
		}
	}
	else
	{
	/* compute the minumum value freeoff can reasonably have: given
	 * the value of db->slots, coercing that to a reasonable value
	 * if bad. If freespace pointer is unreasonable, we will try to
	 * reconstruct it later while scanning slots.
	 */
		if ((slot = db->slots) > EFS_MAXENTS)
			slot = EFS_MAXENTS;
		i = EFS_DIRBLK_HEADERSIZE + slot; 
		if (freeoff < i)
		{
			if (pass > 1)
				return NO;
			if (badmagic)  /* whole block's probably garbage */
			{
	    			why = "DIRECTORY HAS BAD FREESPACE POINTER";
				goto bad_dirblk;
			}
			if (!qflag)
				idprintf("BAD FREESPACE POINTER IN DIR BLK %d",blk);
			else
				fixfreep = 1;
			if (gflag)
				gerrexit();
			if (!fixfreep &&  (reply("FIX") == YES))
				fixfreep = 1;
			if (fixfreep)
				freeoff = EFS_DIRBSIZE;
			else
			{
				dstateflag |= DCORRUPT;
				return YES;
			}
		}
	}

	if (db->slots > EFS_MAXENTS) 
	{
		if (pass > 1)
			return NO;
		if (badmagic || fixfreep)  /* whole block's probably garbage */
		{
	    		why = "DIRECTORY HAS TOO MANY SLOT OFFSETS";
			goto bad_dirblk;
		}
		fixit = 0;
		if (!qflag)
			idprintf("TOO MANY SLOT OFFSETS IN DIR BLK %d",blk);
		else
			fixit = 1;
		if (gflag)
			gerrexit();
		if (!fixit &&  (reply("FIX") == YES))
			fixit = 1;
		if (fixit)
		{
		
			/* Using freeoff, calculate the maximum number
			 * of slots there could be before the first dent.
			 */
			slot = freeoff - EFS_DIRBLK_HEADERSIZE;
			if (slot > EFS_MAXENTS)
				slot = EFS_MAXENTS;

		/* Now search backwards from last possible slot, skipping any 
		 * whose slotoff is zero. (Note "slot" here is # of slots,
		 * index is 1 less since arrays start at 0).
		 */
			while (EFS_SLOTAT(db, (slot - 1)) == 0 && --slot > 0)
			       ;
			db->slots = slot;
			dstateflag |= DFIXED;
			fbdirty();
		}
		else
		{
			dstateflag |= DCORRUPT;
			return YES;
		}
	}

	/* At this point, by checking db->slots & db->firstused against
	 * each other, we have coerced slots to at least a plausible value,
	 * and know whether freeoff is reliable or must be recovered.
	 * Hopefully, the checks which now follow will eliminate any
	 * spurious entries:
	 *
	 * Check slot offsets and insure that they are all within the
	 * allocated space of the directory block.  At the same time, check
	 * that the entries do not overlap. Also check that each slot's
	 * entry has a reasonable inum. If any slot points to a bad
	 * entry, clear the slot. This may leave unused space in the
	 * block; that's wasteful but harmless. The alternative, trying
	 * to compact the block on the basis of a known-to-be-dubious
	 * dir entry, is likely to destroy good entries!
	 */
slotchk:
	for (slot = 0; (unchar)slot < db->slots; slot++) {
	    slotoff = EFS_SLOTAT(db, slot);
	    if (slotoff == 0) {
		/* unused slot */
		continue;
	    }

	/* Check for an offset too near end of block to hold a valid entry. */

	    if (slotoff > (EFS_DIRBSIZE - EFS_DENTSIZE))
	    {
		if (pass > 1)
			return NO;
		if (badmagic)  /* whole block's probably garbage */
		{
			why = "SLOT OFFSET TOO BIG";
			goto bad_dirblk;
		}
		fixit = 0;
		if (!qflag)
			idprintf("SLOT OFFSET TOO BIG IN DIR BLK %d",blk);
		else
			fixit = 1;
		if (gflag)
			gerrexit();
		if (!fixit &&  (reply("CLEAR") == YES))
			fixit = 1;
		if (fixit)
		{
			EFS_SET_SLOT(db, slot, 0);	/* zot offset slot */
			dstateflag |= DFIXED;
			fbdirty();
			if (slot == (db->slots - 1))
			{
				db->slots--;
				break;
			}
			else
				continue;
		}
		else
		{
			dstateflag |= DCORRUPT;
			return YES;
		}
	    }

	    if (!fixfreep && (slotoff < freeoff)) {
		if (pass > 1)
			return NO;
		if (badmagic)  /* whole block's probably garbage */
		{
			why = "SLOT OFFSET POINTS TO UNUSED SPACE";
			goto bad_dirblk;
		}
		fixit = 0;
		if (!qflag)
			idprintf("SLOT OFFSET POINTS TO UNUSED SPACE IN DIR BLK %d",blk);
		else
			fixit = 1;
		if (gflag)
			gerrexit();
		if (!fixit &&  (reply("CLEAR") == YES))
			fixit = 1;
		if (fixit)
		{
			EFS_SET_SLOT(db, slot, 0);	/* zot offset slot */
			dstateflag |= DFIXED;
			fbdirty();
			if (slot == (db->slots - 1))
			{
				db->slots--;
				break;
			}
			else
				continue;
		}
		else
		{
			dstateflag |= DCORRUPT;
			return YES;
		}
	    }

	    /* Check for entry that falls off end of block */

	    e1 = EFS_SLOTOFF_TO_DEP(db, slotoff);

	    if ((slotoff + efs_dentsize(e1)) > EFS_DIRBSIZE)
	    {
		if (pass > 1)
			return NO;
		if (badmagic || fixfreep)  /* whole block's probably garbage */
		{
			why = "DIRECTORY ENTRY OVERRUNS BLOCK";
			goto bad_dirblk;
		}
		fixit = 0;
		if (!qflag)
			idprintf("DIRECTORY ENTRY OVERRUNS BLOCK IN BLK %d",blk);
		else
			fixit = 1;
		if (gflag)
			gerrexit();
		if (!fixit &&  (reply("CLEAR") == YES))
			fixit = 1;
		if (fixit)
		{
			EFS_SET_SLOT(db, slot, 0);	/* zot offset slot */
			dstateflag |= DFIXED;
			fbdirty();
			if (slot == (db->slots - 1))
			{
				db->slots--;
				break;
			}
			else
				continue;
		}
		else
		{
			dstateflag |= DCORRUPT;
			return YES;
		}
	    }

	    /* Check for entry with outrange inum */

	    entryinum = EFS_GET_INUM(e1);
	    if (ioutrange(entryinum))
	    {
		if (pass > 1)
		    return NO;
		if (badmagic || fixfreep)
		{
		    	why = "OUTRANGE INODE NUMBER";
			goto bad_dirblk;
		}
		fixit = 0;
		if (!qflag)
			idprintf("ENTRY INUM %u OUTRANGE IN DIR BLK %d",entryinum, blk);
		else
			fixit = 1;
		if (gflag)
			gerrexit();
		if (!fixit &&  (reply("CLEAR") == YES))
			fixit = 1;
		if (fixit)
		{
			EFS_SET_SLOT(db, slot, 0);	/* zot offset slot */
			dstateflag |= DFIXED;
			fbdirty();
			if (slot == (db->slots - 1))
			{
				db->slots--;
				break;
			}
			else
				continue;
		}
		else
		{
			dstateflag |= DCORRUPT;
			return YES;
		}
	    }

	    /* Check for slot collisions & entry overlap */
	    
	    for (other = slot + 1; (unchar)other < db->slots; other++) {
		otherslotoff = EFS_SLOTAT(db, other);
		if (otherslotoff == 0)
		    continue;
		if (otherslotoff == slotoff) 
		{
			if (pass > 1)
				return NO;
			if (badmagic)  /* whole block's probably garbage */
			{
				why = "DIRECTORY SLOTS OVERLAP";
				goto bad_dirblk;
			}
			fixit = 0;
			if (!qflag)
				idprintf("OVERLAPPING DIRECTORY SLOTS IN DIR BLK %d",blk);

			else
				fixit = 1;
			if (gflag)
				gerrexit();
			if (!fixit &&  (reply("CLEAR") == YES))
				fixit = 1;
			if (fixit)
			{
			/* Duplicate entries: zap the later slot since dirs are
			 * searched from beginning. */

				EFS_SET_SLOT(db, other, 0);
				dstateflag |= DFIXED;
				fbdirty();
				continue;
			}
			else
			{
				dstateflag |= DCORRUPT;
				return YES;
			}
		}

		e2 = EFS_SLOTOFF_TO_DEP(db, otherslotoff);
		if (((slotoff + efs_dentsize(e1)) <= otherslotoff) ||
		    ((otherslotoff + efs_dentsize(e2)) <= slotoff))
		    continue;
		else
		{
			if (pass > 1)
				return NO;
			if (badmagic || fixfreep)
			{
				why = "DIRECTORY ENTRIES OVERLAP";
				goto bad_dirblk;
			}
			fixit = 0;
			if (!qflag)
				idprintf("OVERLAPPING DIRECTORY ENTRIES IN DIR BLK %d",blk);
			else
				fixit = 1;
			if (gflag)
				gerrexit();
			if (!fixit &&  (reply("CLEAR") == YES))
				fixit = 1;
			if (fixit)
			{
				EFS_SET_SLOT(db, slot, 0);
				EFS_SET_SLOT(db, other, 0);
				dstateflag |= DFIXED;
				fbdirty();
				if (slot == (db->slots - 1))
					db->slots--;
				goto slotchk;
			}
			else
			{
				dstateflag |= DCORRUPT;
				return YES;
			}
		}
	    }
	    if (fixfreep && (slotoff < freeoff))
		freeoff = slotoff;
	}

	if (fixfreep)
	{
		dstateflag |= DFIXED;
		db->firstused = EFS_COMPACT(freeoff);
		fbdirty();
	}

	/*
	 * Make sure the directory has legitimate names.  If checkdot,
	 * look for "." and ".." as we go; also check for bogus
	 * current and parent inode entries: these must be expunged since
	 * they can lead to directory loops!
	 */
	for (slot = 0; (unchar)slot < db->slots; slot++) 
	{
		if (EFS_SLOTAT(db, slot) == 0)
			continue;
		dirp = EFS_SLOTOFF_TO_DEP(db, EFS_SLOTAT(db, slot));
		nameScrewed = 0;
		ptr = dirp->d_name;
		while (ptr < dirp->d_name + dirp->d_namelen) {
			if ((*ptr == '/') || (*ptr == '\0'))
				nameScrewed = 1;
			ptr++;
		}
		if (nameScrewed) {
			if (pass > 1)
				return NO;
			if (badmagic)
			{
				why = "ENTRY CONTAINS ILLEGAL CHARACTERS";
				goto bad_dirblk;
			}
			idprintf("DIRECTORY ENTRY CONTAINS ILLEGAL CHARACTERS IN DIR I= %u\n",
					inum);
			idprintf("BLK %ld ",blk);
			pinode();
			newline();
			fixit = 0;
			if (qflag || gflag) {
				printf(" (PATCHED)\n");
				fixit = 1;
			} else
			if (reply("PATCH") == YES)
				fixit = 1;
			if (fixit) {
				ptr = dirp->d_name;
				while (ptr < dirp->d_name + dirp->d_namelen) {
					if ((*ptr == '/') || (*ptr == '\0'))
						*ptr = '#';
					ptr++;
				}
				dstateflag |= DFIXED;
				fbdirty();
			}
			else
			{
				dstateflag |= DCORRUPT;
				return YES;
			}
			nameScrewed = 0;
		}
		else if (!DotCheck(dirp->d_name, dirp->d_namelen))
			dentrycount++;		/* don't count . & .. */

		/* Now check for duplicate entries pointing to current (in
		 * pass 1) or parent (in pass 2) inum: there should be only
		 * "." and ".." that do this; remove any impostors! Only
		 * exception: in root inode . and .. are both EFS_ROOTINO.
		 */

	    	entryinum = EFS_GET_INUM(dirp);

		if (entryinum != 0)
			entrycnt++;

		if (entryinum == ((pass == 1) ? inum : dot))
		{
			if ((dirp->d_namelen != 1) || (dirp->d_name[0] != '.'))
			{
				if ((inum != EFS_ROOTINO) ||
				    ((dirp->d_namelen != 2) ||
				     (dirp->d_name[0] != '.') ||
				     (dirp->d_name[1] != '.')))
				{
#ifdef DHDEBUG
			/*DHXX*/ printf("Bogus '.' impersonator!\n");
#endif
					dentrycount--;
					EFS_SET_SLOT(db, slot, 0);
					dstateflag |= DFIXED;
					fbdirty();
					if (slot == (db->slots - 1))
					{
						db->slots--;
						break;
					}
				}
			}
		}

		if (entryinum == ((pass == 2) ? pinum : ddot))
		{
			if ((dirp->d_namelen != 2) || 
			    (dirp->d_name[0] != '.') ||
			    (dirp->d_name[1] != '.'))
			{
				if ((curinum != EFS_ROOTINO) ||
				    ((dirp->d_namelen != 1) ||
				     (dirp->d_name[0] != '.')))
				{
#ifdef DHDEBUG
			/*DHXX*/ printf("Bogus '..' impersonator!\n");
#endif
					EFS_SET_SLOT(db, slot, 0);
					fbdirty();
					if (slot == (db->slots - 1))
					{
						db->slots--;
						break;
					}
				}
			}
		}

		/* Next, if checkdot set: look for real "." and ".." entries */

		if (checkdot && 
		    (dirp->d_namelen == 1) && (dirp->d_name[0] == '.'))
		{
			dot = entryinum;
			if ((pass == 1) && (entryinum != inum))
			{
				fixdone = 1;
				printf("directory inum %d had bad '.' entry: corrected.\n", inum); 
				EFS_SET_INUM(dirp, inum);
				dstateflag |= DFIXED;
				fbdirty();		
			}
		}
		if (checkdot && 
		    (dirp->d_namelen == 2) && 
		    (dirp->d_name[0] == '.') &&
		    (dirp->d_name[1] == '.'))
		{
			ddot = entryinum;

			/* if we're sure this is dotdot, and there are
			 * no others matching, cache the dotdot value */
			if (pass == 1 && entrycnt == 2
			    && dot != BADINUM && dot != ddot)
				cache_ddot(inum, ddot);

			if ((pass == 2) && (entryinum != pinum))
			{
				fixdone = 1;
				printf("directory inum %d had bad '..' entry: %d corrected to %d.\n", curinum, entryinum, pinum); 
				EFS_SET_INUM(dirp, pinum);
				dstateflag |= DFIXED;
				fbdirty();  
			}
		}
	}

	if (checkdot && (dot == BADINUM || ddot == BADINUM))
	{
		if (pass > 1)
			return NO;
		goto fixdots;
	}


	return YES;

bad_dirblk:
	idprintf("%s\n", why);
	idprintf("DIR BLK %ld", blk);
	pinode();
	newline();
	fixit = 0;
	if (gflag)
		gerrexit();
	if (qflag) {
		printf(" (FIXED)\n\n");
		fixit = 1;
	} else
	if (reply("FIX") == YES)
		fixit = 1;
	if (fixit) {
		fixdone = 1;
		bzero((char *) db, EFS_DIRBSIZE);
		db->magic = EFS_DIRBLK_MAGIC;
		dot = BADINUM;
		ddot = BADINUM;
		dstateflag |= DFIXED;
		fbdirty();
	}
	else
	{
		dstateflag |= DCORRUPT;
		return YES;
	}
	
	if (!checkdot)
		return YES;

fixdots:

	/* Here, we are missing entries for ".", "..", or both. Try to
	 * add them. */

	if (dot == BADINUM)
	{
		if (addentry(db, ".", inum) == NO)
		{
			dstateflag |= DNODOT;
			return YES;
		}
	}
	if (ddot == BADINUM)
	{
		if (addentry(db, "..", pinum ? pinum : EFS_ROOTINO) == NO)
			dstateflag |= DNODOTDOT;
	}

	return YES;
}

/* ARGSUSED */

int
pass2(DIRECT *dirp, int size, efs_ino_t curinum)
{
	char *p;
	int n;
	DINODE *dp;

	if ((inum = EFS_GET_INUM(dirp)) == 0)
		return(KEEPON);
	thisname = pathp;

	if ((&pathname[MAXPATH] - pathp) < (int)dirp->d_namelen) {
		idprintf("DIR pathname too deep\n");
		idprintf("Increase MAXPATH and recompile.\n");
		idprintf("DIR pathname is <%s>\n", pathname);
		ckfini();
		exit(4);
	}
	for (p = dirp->d_name; p < dirp->d_name + dirp->d_namelen; )
		if ((*pathp++ = *p++) == 0) {
			--pathp;
			break;
		}
	*pathp = 0;
	n = NO;
	if (ioutrange(inum))
		n = direrr("I OUT OF RANGE");
	else {
	again:
		switch (getstate()) {
			case USTATE:
				n = direrr("UNALLOCATED");
				break;
			case CLEAR:
				if ((n = direrr("DUP/BAD")) == YES)
					break;
				if ((dp = ginode()) == NULL)
					break;
#ifdef AFS
				setstate(DIR ? DSTATE : VICEINODE ? VSTATE : FSTATE);
#else
				setstate(DIR ? DSTATE : FSTATE);
#endif
				goto again;
#ifdef AFS
			case VSTATE:
				idprintf("VICE INODE REFERENCED BY DIRECTORY ");
				pinode();
				if ((n = reply("CONVERT TO REGULAR FILE")) != YES)
					break;
				if ((dp = ginode()) == NULL)
					break;
				CLEAR_DVICEMAGIC(dp);
				inodirty();
				setstate(FSTATE);
				/* FALLTHROUGH */
#endif
			case FSTATE:
				if ((pass == 2) || (pass == 3))
					declncnt();
				break;
			case DSTATE:
				if ((pass == 2) || (pass == 3))
					declncnt();
				descend(curinum);
		}
	}
	pathp = thisname;
	if (n == NO)
		return(KEEPON);

	/*
	 * Zero out the directory entries inumber.  The caller, dirscan,
	 * knows that we aren't quite doing the right thing here and
	 * will fix the directory by compacting the free space at the end
	 * of the directory block.
	 */
	fixdone = 1;
	EFS_SET_INUM(dirp, 0);
	return(KEEPON|ALTERD);
}

/*
 * chkeblk() --
 * check a directory block for being "empty" .
 */
int
chkeblk(efs_daddr_t bn)
{
	DIRECT *dirp;
	struct efs_dirblk *db;
	int slot;

	if (outrange(bn)) {
		printf("chkempt: blk %d out of range\n",bn);
		return SKIP;
	}
	if (getblk(&fileblk, bn) == NULL) {
		printf("chkempt: Can't find blk %d\n", bn);
		return SKIP;
	}
	dir_size -= EFS_DIRBSIZE;

	db = (struct efs_dirblk *) dirblk;
	for (slot = 0; (unchar)slot < db->slots; slot++) {
		if (EFS_SLOTAT(db, slot) == 0)
			continue;
		dirp = EFS_SLOTOFF_TO_DEP(db, EFS_SLOTAT(db, slot));
		if (DotCheck(dirp->d_name, dirp->d_namelen))
			continue;
		return STOP;
	}
	return (dir_size > 0 ? KEEPON : STOP);
}

/*
 * Compact a directory, removing the entry at "slot".
 * Ruggedized to behave sensibly if entry appears to be below freespace,
 * and to never clear outside the block.
 */
void
dircompact(struct efs_dirblk *db, struct efs_dent *dep, int slot)
{
	int size;
	int slotoff, firstusedoff;

	size = efs_dentsize(dep);

	slotoff = EFS_SLOTAT(db, slot);
	firstusedoff = EFS_REALOFF(db->firstused);
	if ((size + slotoff) > EFS_DIRBSIZE)
		size = EFS_DIRBSIZE - slotoff;	   /* sanity check */
	if ((size + firstusedoff) > EFS_DIRBSIZE)
		size = EFS_DIRBSIZE - firstusedoff;	   /* sanity check */

	/*
	 * Remove the entry.  If the entry is adjacent to the free space in
	 * the directory then all we need do is zero its contents and increase
	 * the size of the free space.  Otherwise, we have to to compact the
	 * directory and then adjust the free space.
	 */
	if (firstusedoff < slotoff) {
		unchar i;
		int movedslotoff;

		/*
		 * For each entry that is before the current entry,
		 * adjust its offset to account for its movement when
		 * the current entry is erased.
		 */
		for (i = 0; i < db->slots; i++) {
			movedslotoff = EFS_SLOTAT(db, i);
			if ((movedslotoff == 0) || (movedslotoff >= slotoff)) {
				/* empty entry/entry is not moving */
				continue;
			}
			EFS_SET_SLOT(db, i, EFS_COMPACT(movedslotoff + size));
		}
		bcopy((caddr_t) EFS_SLOTOFF_TO_DEP(db, firstusedoff),
			(caddr_t) EFS_SLOTOFF_TO_DEP(db, firstusedoff + size),
			slotoff - firstusedoff);
		dep = EFS_SLOTOFF_TO_DEP(db, firstusedoff);
	}

	/* zero bytes no longer being used, sanity checking first */

	bzero(dep, size);
	if (firstusedoff < slotoff) 
		firstusedoff += size;
	if (firstusedoff == EFS_DIRBSIZE)
	{
		db->slots = 0;
		db->firstused = 0;
	}
	else
		db->firstused = EFS_COMPACT(firstusedoff);
	if (slot == db->slots - 1)
		db->slots--;

	EFS_SET_SLOT(db, slot, 0);		/* zot offset slot */
}

int
dirscan(efs_daddr_t blk, int firstblk, efs_ino_t pinum, efs_ino_t curinum)
{
	DIRECT *dirp;
	int n, size;
	union {
		char maxspace[EFS_DIRBSIZE];
		DIRECT direntry;
	} du;
	struct efs_dirblk *db;
	int slots, slot;
	int isfree;

	if (outrange(blk)) {
		filsize -= BBSIZE;
		return(SKIP);
	}

	filsize -= EFS_DIRBSIZE;
	if (getblk(&fileblk, blk) == NULL) {
		return (SKIP);
	}
	if (pinum == get_ddot(curinum)) {
		n = YES;
	} else if ((n = chkblk(blk, firstblk, pinum, curinum)) != YES) {
		/* directory is corrupted.  don't touch it */
		return (STOP);
	}
	db = (struct efs_dirblk *) dirblk;
	slots = db->slots;			/* save */

	/*
	 * Scan all entries, including the free space, if its big enough
	 * for a new entry.  When slot == slots, we scan the free space.
	 */
	isfree = 0;
	for (slot = 0; slot <= slots; slot++) {
		if (getblk(&fileblk, blk) == NULL) {
			/* error */
			filsize -= BBSIZE;
			return (SKIP);
		}
		db = (struct efs_dirblk *) dirblk;
		if (slot < slots) {
			if (EFS_SLOTAT(db, slot) == 0)
				continue;

			dirp = EFS_SLOTOFF_TO_DEP(db, EFS_SLOTAT(db, slot));
			size = efs_dentsize(dirp);
			copy((char *) dirp, (char *) &du, size);
		} else {
			/*
			 * Scan free space.  Make up a dummy inode that looks
			 * free, whose size contains most of, if not all of,
			 * the free space in the dirblk.  The function called
			 * by bellow via pfunc is responsible for seeing
			 * if the size is large enough for a new entry.
			 */
			EFS_SET_INUM(&du.direntry, 0);
			size = EFS_DIR_FREESPACE(db);
			isfree = 1;
		}

		if ((n = (*pfunc)(&du.direntry, size, curinum)) & ALTERD) {
			if (getblk(&fileblk,blk) != NULL) {
				db = (struct efs_dirblk *) dirblk;
				if (isfree) {
					/* EMPTY */
					/* don't do anything to free
					 * entries */
					;
				} else
				if (EFS_INUM_ISZERO(&du.direntry)) {
					/*
					 * Entry was destroyed.  Fix up
					 * directory by compacting the new
					 * free space.  Fix up dirp so that
					 * loop will complete if there are
					 * more entries left.
					 */
					dircompact(db, dirp, slot);
				} else {
					copy((char *) &du, (char *) dirp, size);
				}
				fbdirty();
			} else
				n &= ~ALTERD;
		}
		if (n & STOP)
			return(n);
	}
	return(filsize > 0 ? KEEPON : STOP);
}

int
findino(DIRECT *dirp)
{
	efs_ino_t	fino;

	if ((dirp->d_namelen == strlen(srchname)) &&
	    (strncmp(dirp->d_name, srchname, dirp->d_namelen) == 0)) {
		fino = EFS_GET_INUM(dirp);
		if (fino >= EFS_ROOTINO && fino <= max_inodes)
			foundino = fino;
		return (STOP);
	}
	return(KEEPON);
}

/*
 * Make a directory entry for a reconnected file in lost+found.
 */
int
mkentry(DIRECT *dirp, int size)
{
	DIRECT *newdirp;
	struct efs_dirblk *db;
	off_t dirblkoff;
	int dentsize, namelen;
	unchar i;
	char name[20];

	/* make sure this is a free space inode */
	if (!EFS_INUM_ISZERO(dirp))
		return (KEEPON);

	/* construct inode name */
	sprintf(name, "%06ld", orphan);
	namelen = strlen(name);
	dentsize = efs_dentsizebyname(name);
	if (dentsize >= size) {
		/* this dirblk is too small. sorry */
		return (KEEPON);
	}

	/*
	 * We have to allocate a new directory entry.  Get a pointer to
	 * the new dirent, and then set it up.
	 */
	db = (struct efs_dirblk *) dirblk;
	dirblkoff = EFS_DIR_FREEOFF(db->firstused) - dentsize;
	db->firstused = EFS_COMPACT(dirblkoff);
	newdirp = EFS_SLOTOFF_TO_DEP(db, dirblkoff);
	EFS_SET_INUM(newdirp, orphan);
	newdirp->d_namelen = (unchar)namelen;
	bcopy(name, newdirp->d_name, namelen);

	/*
	 * Now scan the slots, looking for an empty one to use.
	 * If none are empty, add a new slot.
	 */
	for (i = 0; i < db->slots; i++) {
		if (EFS_SLOTAT(db, i) == 0)
			break;
	}
	EFS_SET_SLOT(db, i, db->firstused);
	if (i == db->slots)
		db->slots++;
	fbdirty();
	return(ALTERD|STOP);
}

/* Addentry: variant of the above used to make "." or ".." entries if these
 * are missing in the pass 1 chkblk(). Only scans one block, returns YES
 * if successfully added, NO if not enough space. */

int
addentry(struct efs_dirblk *db, char *name, efs_ino_t num)
{
	DIRECT *newdirp;
	off_t dirblkoff;
	int dentsize, namelen;
	unchar i;

	namelen = strlen(name);
	dentsize = efs_dentsizebyname(name);
	if (dentsize >= (EFS_DIR_FREESPACE(db) + 1))  /* enough space ? */
		return NO;
	/*
	 * We have to allocate a new directory entry.  Get a pointer to
	 * the new dirent, and then set it up.
	 */

	dirblkoff = EFS_DIR_FREEOFF(db->firstused) - dentsize;
	db->firstused = EFS_COMPACT(dirblkoff);
	newdirp = EFS_SLOTOFF_TO_DEP(db, dirblkoff);
	EFS_SET_INUM(newdirp, num);
	newdirp->d_namelen = (unchar)namelen;
	bcopy(name, newdirp->d_name, namelen);

	/*
	 * Now scan the slots, looking for an empty one to use.
	 * If none are empty, add a new slot.
	 */
	for (i = 0; i < db->slots; i++) {
		if (EFS_SLOTAT(db, i) == 0)
			break;
	}
	EFS_SET_SLOT(db, i, db->firstused);
	if (i == db->slots)
		db->slots++;
	fbdirty();
	return YES;
}

/* change dotdot value in a directory */
int
chgdd(DIRECT *dirp)
{
	if (dirp->d_namelen == 2 && dirp->d_name[0] == '.'
	    && dirp->d_name[1] == '.') {
		EFS_SET_INUM(dirp, lfdir);
		return(ALTERD|STOP);
	}
	return(KEEPON);
}

/* Directory blocks are now scanned for sanity in pass 1 (and cached, for 
 * later use). This allows us to catch & kill bad directories earlier, and
 * ensures that even unreferenced directories are fully checked: eliminating
 * multi-pass requirements. This function is called to make an overall 
 * decision on the directory after all its component blocks have been scanned. 
 * It uses the accumulated info in dstateflag and dentrycount. 
 *
 * Returns a value indicating whether the directory has been retained: this is 
 * used to decide whether the block containing its inode is a candidate for 
 * cacheing.
 */

int
direvaluate(void)
{
	int fixit;

	if ((dstateflag == 0) || (dstateflag == DFIXED)) /* OK or fixed */
		return YES;

	/* If there are no entries other than . and .., but dir is bad,
	 * there's nothing that can be salvaged, quietly clear it. */

	if ((dentrycount == 0) && dstateflag )
	{
		if (!nflag)
			clri("", REM);
		return NO;
	}

	/* If we get here, there are entries but also something seriously
	 * wrong. */

	idprintf("DIR I= %u ", inum);
	if (dstateflag & DCORRUPT)
		printf("CORRUPTED");
	else if (dstateflag & DBADBLOCK)
		printf("BLOCK OUT OF RANGE");
	else if (dstateflag & DNODOT)
		printf("MISSING '.' ENTRY");
	else if (dstateflag & DNODOTDOT)
		printf("MISSING '..' ENTRY");
	pinode();
	newline();
	fixit = 0;
	if (qflag) {
		printf(" (CLEARED)\n");
		fixit = 1;
	} else
	if (reply("CLEAR") == YES)
		fixit = 1;
	if (fixit) 
	{
		clri("", REM);
		return NO;
	}
	else
		return YES;
}

int
is_data_block(efs_daddr_t blk)
{
	int block_in_cg;

	if (outrange(blk))
		return 0;
	blk -= fmin;
	block_in_cg = blk % superblk.fs_cgfsize;
	if (block_in_cg < superblk.fs_cgisize)  /* in inode area */
		return 0;
	else
		return 1;
}

#define NDD_CACHE 128
#define NDD_SHIFT 7
#define DD_HASH(x) ((x + (x>>7)) & (NDD_CACHE - 1))

struct dd_cache {
	efs_ino_t in;
	efs_ino_t to;
	struct dd_cache *next;
};

struct dd_cache *dd_cache_head[NDD_CACHE];

void
cache_ddot(efs_ino_t in, efs_ino_t to)
{
	struct dd_cache *ddp = malloc(sizeof(*ddp));
	int dch;

	BCACHE_LOCK();
	if (!ddp)
		return;

	dch = DD_HASH(in);
	ddp->in = in;
	ddp->to = to;
	ddp->next = dd_cache_head[dch];
	dd_cache_head[dch] = ddp;
	BCACHE_UNLOCK();
}

void
clear_ddot(void)
{
	int i;

	for (i = 0; i < NDD_CACHE; i++) {
		struct dd_cache *ddp = dd_cache_head[i];
		dd_cache_head[i] = 0;

		while (ddp) {
			struct dd_cache *ddpnext = ddp->next;
			free(ddp);
			ddp = ddpnext;
		}
	}
}

efs_ino_t
get_ddot(efs_ino_t in)
{
	struct dd_cache *ddp = dd_cache_head[DD_HASH(in)];

	while (ddp && ddp->in != in)
		ddp = ddp->next;
	return ddp ? ddp->to : BADINUM;
}

void
rm_ddot(efs_ino_t in)
{
	struct dd_cache *ddp = dd_cache_head[DD_HASH(in)];
	struct dd_cache *ddprev = 0;

	while (ddp && ddp->in != in) {
		ddprev = ddp;
		ddp = ddp->next;
	}
	if (ddp) {
		if (ddprev)
			ddprev->next = ddp->next;
		else
			dd_cache_head[DD_HASH(in)] = ddp->next;
		free(ddp);
	}
}
