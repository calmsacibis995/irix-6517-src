#ident "$Revision: 1.9 $"

/*
 * Common include file for filesystem tools.
 */

#include <sys/param.h>
#include <sys/sysmacros.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>
#include <values.h>
#include <sys/fs/efs.h>

/*
 * Operators for manipulating the bitmap
 */
#define	bset(bp, b)	(*((bp) + ((b) >> 3)) |= 1 << ((b) & 7))
#define	bclr(bp, b)	(*((bp) + ((b) >> 3)) &= ~(1 << ((b) & 7)))
#define	btst(bp, b)	(*((bp) + ((b) >> 3)) & (1 << ((b) & 7)))

extern int fs_fd;		/* descriptor filesystem is open on */
extern struct efs *fs;		/* pointer to superblock */
extern void error(void);	/* user provided error printer */
extern char *progname;		/* program running */
extern char *bitmap;		/* pointer to bitmap memory */

/* library exports */
extern struct efs_dinode *efs_iget(efs_ino_t);
extern void efs_iput(struct efs_dinode *, efs_ino_t);
extern void efs_mknod(efs_ino_t, ushort, ushort, ushort);
extern void efs_checksum(void);
extern void efs_extend(struct efs_dinode *, int);
extern void efs_write(efs_ino_t, char *, int);
extern void efs_update(void);
extern efs_ino_t efs_allocino(void);
extern efs_ino_t efs_lastallocino(void);
extern void efs_bget(void);
extern void efs_bput(void);
extern efs_daddr_t efs_bmap(struct efs_dinode *, off_t, off_t);
extern extent *efs_getextents(struct efs_dinode *);
extern void efs_enter(efs_ino_t, efs_ino_t, char *);
extern void efs_mklostandfound(efs_ino_t);
extern int efs_mount(void);
