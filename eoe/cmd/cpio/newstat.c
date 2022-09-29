
/* --------------------------------------------------- */
/* | Copyright (c) 1987, MIPS Computer Systems, Inc. | */
/* | All Rights Reserved.                            | */
/* --------------------------------------------------- */

/* Copyright 1986, Silicon Graphics Inc., Mountain View, CA. */

#ident	"$Revision: 1.6 $"

#ifdef sgi
#include <malloc.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mkdev.h>
#include "newstat.h"
#include "cpio.h"
#define  FATAL  -1

/*  new_stat.c
 *	This function was added to cpio when the inode was resized
 *	from 16 bits to 32 bits.  All system and library calls 
 *	in cpio.c to obtain status information about a file were
 * 	changed to call this routine.  THE PROBLEM:  65K files can
 *	now have the same low 16-bits (i.e. - SAME inode value).
 *	If the link count is greater than 1, then cpio doesn't
 *	know which of those 65K files to which the link actually
 *	belongs, and, accordingly, links all files with the same
 *	inode/dev pair together.  THE SOLUTION HERE:  the low
 *	16 bits of the inode along with the device number are
 *	hashed into a small table.  A new inode/dev pair is
 *	generated for each file (i.e., each 48-bit - and possibly
 *	future 64-bit dev/inode pair shrinks into a 32-bit dev/
 *	inode pair), which makes cpio happy.  If two files have
 *	the same original device/inode numbers, then the files
 *	are links and are given the same new inode/dev number.
 *	These new numbers are absolutely bogus, and I feel sorry 
 *	for the poor sucker looking at the headers for meaningful 
 *	information, but cpio only uses this information to tell 
 *	which files are linked.  This disgusting code guarantees 
 *	REAL links.  The initial new inode pair starts at 3, and 
 *	continues up to	2^32-1, so if you're looking for something
 *	redeeming, maybe you can use that little gem of information.
 *
 */
/*
 * NOTE:
 * This code has the unwanted side-effect of not returning the same
 * ino/dev for multiple calls on the same file, if that file has
 * 'nlinks' < 2, since we only create hash table entries for files
 * which have hard links. This does not (currently) cause any problems,
 * (some problems were resolved by use of the 'Tstatb' buffer) but
 * it is something to be aware of.
 */

static struct i_node *HASHTBL[HASH];    /* assumes initial NULLs   */
static ino64_t next_ino = 3LL;  	/* initial inode value -
					   inode values will increase
					   from this value on      */

int
new_stat64(struct stat64 *buf)
{

    unsigned long hashval;	/* index into hashtable		*/
    struct i_node *iptr;	/* info struct for linked files */
    struct i_node *oldptr;


#ifdef DEBUG
fprintf(stderr,"new_stat: inode = %lld, dev = %d, rdev = %d, nlinks = %d\n",buf->st_ino,buf->st_dev,buf->st_rdev,buf->st_nlink);
#endif

    /* directories have nlink > 1 */
    if (buf->st_nlink < 2 || (buf->st_mode & S_IFMT) == S_IFDIR)  {

#ifdef DEBUG
fprintf(stderr,"new_stat: old_ino = %lld, old_dev = %d, ",buf->st_ino,buf->st_dev);
#endif

	buf->st_ino = next_ino & 0xffffLL;
	buf->st_dev = ((dev_t)(next_ino >> 16)) & 0xffff;

#ifdef DEBUG
fprintf(stderr,"new_ino = %lld, new_dev = %d\n",buf->st_ino,buf->st_dev);
#endif

	next_ino++;
	return 0;
    }
    else {		/*  possible linked files	*/

	hashval = (unsigned long)((buf->st_ino&0xffLL << 8) + (buf->st_ino&0xff00LL >> 8)
		  + buf->st_dev) % (HASH - 1);

/* 
    Need to turn the dev/inode pair into two shorts for cpio's
    data structures - cpio compares the dev/inode pair to see if
    the files are linked - the two shorts generated here are
    bogus, but serve the purpose of identifying files that really
    are linked for cpio's use
 */


#ifdef DEBUG
fprintf(stderr,"new_stat: old_ino = %lld, old_dev = %d, ",buf->st_ino,buf->st_dev);
#endif
	oldptr = NULL;
	iptr = HASHTBL[hashval];


	while (iptr != NULL) {
#ifdef DEBUG
	fprintf(stderr,"new_stat: ** Link entries **\n");
	fprintf(stderr,"new_stat: hashval: %d\n",hashval);
	fprintf(stderr,"new_stat: ino: %lld %lld\n",iptr->old_ino,buf->st_ino);
	fprintf(stderr,"new_stat: iptr dev: %d %d\n",cpioMAJOR(iptr->old_dev),cpioMINOR(iptr->old_dev));
	fprintf(stderr,"new_stat: buf dev: %d %d\n",cpioMAJOR(DEV32TO16(buf->st_dev)),cpioMINOR(buf->st_dev));
	fprintf(stderr,"new_stat: rdev: %d %d\n",iptr->old_rdev,buf->st_rdev);
#endif
	/*
	 * The check of buf->st_dev ... cpioMINOR only looks at the last
	 * 8 bits (ie. 0xff).  While cpioMAJOR expects that buf->dev to
	 * be only 16 bits instead of the 32 bits.  Thus buf->dev need to
	 * reduced and then cpioMAJOR will bit shift correctly for the correct
	 * major number.
	 */
		if ((iptr->old_ino == buf->st_ino) &&  
		    (cpioMAJOR(iptr->old_dev) == cpioMAJOR(DEV32TO16(buf->st_dev))) &&
		    (cpioMINOR(iptr->old_dev) == cpioMINOR(buf->st_dev)) ) {
			buf->st_ino = iptr->ino;
			buf->st_dev = iptr->dev;
#ifdef DEBUG
			fprintf(stderr,"new_stat: ** Linked file entries **\n");
			fprintf(stderr,"new_stat: ino: %d\n",iptr->ino);
			fprintf(stderr,"new_stat: dev: %d\n",iptr->dev);
			fprintf(stderr,"new_stat: old_ino: %lld\n",iptr->old_ino);
			fprintf(stderr,"new_stat: old_dev: %d\n",iptr->old_dev);
			fprintf(stderr,"new_stat: old_rdev: %d\n",iptr->old_rdev);
#endif
			return 0;
		}
		oldptr = iptr;
		iptr = iptr->next;
	}

	/* didn't find a matching linked file, so must create a new
	 * entry into the hash table */

	if ((iptr = (struct i_node *)calloc(1,sizeof(struct i_node)))
			== NULL) {
		return FATAL;
	}

	if (oldptr)
		oldptr->next = iptr;
	else
		HASHTBL[hashval] = iptr;
	iptr->next = NULL;
	iptr->ino = next_ino & 0xffffLL;
	iptr->dev = (next_ino >> 16) & 0xffffLL;
	iptr->old_ino = buf->st_ino;
	iptr->old_dev = (short)DEV32TO16(buf->st_dev);
	iptr->old_rdev = (short)buf->st_rdev;
	buf->st_ino = iptr->ino;
	buf->st_dev = iptr->dev;
	next_ino++;

#ifdef DEBUG
	fprintf(stderr,"new_stat: *** New table entry ***\n");
	fprintf(stderr,"new_stat: ino = %d\n",iptr->ino);
	fprintf(stderr,"new_stat: dev = %d\n",iptr->dev);
	fprintf(stderr,"new_stat: old_ino = %lld\n",iptr->old_ino);
	fprintf(stderr,"new_stat: old_dev = %d\n",iptr->old_dev);
	fprintf(stderr,"new_stat: old_rdev = %d\n",iptr->old_rdev);
#endif

	return 0;
    }
}
#endif	/* sgi */
