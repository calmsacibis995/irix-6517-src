/* @(#)dir.h	9.11 */

/*
 *	NOTE:  This file is derived from a public domain version
 *	of the BSD compatible directory access library by Doug
 *	Gwyn at BRL.  It is not subject to any distribution
 *	restrictions and Doug's contribution to the Unix community
 *	is gratefully acknowledged.  This file is provided for
 *	convenience in using the bru source code on non-BSD systems.
 */

#ifdef amiga

#define MAXNAMLEN  31		/* Is actually 30, but be safe... */

#else

#define DIRBLKSIZ  512		/* size of directory block */
#define MAXNAMLEN  15		/* maximum filename length */

/* NOTE:  MAXNAMLEN must be one less than a multiple of 4 */

struct direct {			/* data from readdir() */
    long d_ino;			/* inode number of entry */
    unsigned short d_reclen;	/* length of this record */
    unsigned short d_namlen;	/* length of string in d_name */
    char d_name[MAXNAMLEN + 1];	/* name of file */
};

typedef struct {
    int dd_fd;			/* file descriptor */
    int dd_loc;			/* offset in block */
    int dd_size;		/* amount of valid data */
    char dd_buf[DIRBLKSIZ];	/* directory block */
} DIR;				/* stream data from opendir() */

extern DIR *opendir ();
extern struct direct *readdir ();
extern long telldir ();
extern VOID seekdir ();
extern VOID closedir ();

#define rewinddir(dirp)  seekdir (dirp, 0L)

#endif	/* amiga */
