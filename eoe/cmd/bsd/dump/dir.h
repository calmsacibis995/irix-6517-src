#ident "$Revision: 1.1 $"

/*
 * Based on 3.17 of irix/kern/sys/dir.h.
 * See that file for lengthy comments on what all this means;
 * here we only need what dump/restore are using.
 */

#define MAXNAMLEN	255

struct	direct {
	u_int	d_ino;			/* inode number of entry */
	u_short	d_reclen;		/* length of this record */
	u_short	d_namlen;		/* length of string in d_name */
	char	d_name[MAXNAMLEN + 1];	/* name must be no longer than this */
};

#undef DIRSIZ
#define	__DIRALIGN	(sizeof(u_int)-1)
#define DIRSIZ(dp) \
	((sizeof (struct direct) - (MAXNAMLEN+1)) + \
	 (((dp)->d_namlen+1 + __DIRALIGN) &~ __DIRALIGN))
