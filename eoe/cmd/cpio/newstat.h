
/* --------------------------------------------------- */
/* | Copyright (c) 1987, MIPS Computer Systems, Inc. | */
/* | All Rights Reserved.                            | */
/* --------------------------------------------------- */

/* Copyright 1986, Silicon Graphics Inc., Mountain View, CA. */

#ident	"$Revision: 1.4 $"

#ifdef sgi
#define HASH 8*1024	/* This number MUST be smaller than 65K
			   (i.e., 16 bits) - this is the size of 
			   the hash table that stores the inode
	 	 	   values */

struct i_node		/* dynamically allocated new inode value
			   structure */
{
    unsigned short ino;		/* lower half of new inode value */
    short          dev;		/* upper half of new inode value */
    short          old_rdev;	/* save value of special device  */
    short          old_dev;	/* save value of original device */
    ino64_t	   old_ino;	/* save original inode value     */
    struct i_node *next;	/* next in collision chain       */
};

int new_stat64(struct stat64 *buf);
#endif	/* sgi */
