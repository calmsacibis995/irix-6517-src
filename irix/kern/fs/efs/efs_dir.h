#ifndef __efs_dir__
#define	__efs_dir__
/*
 * Long-name directory structure for the efs.
 *
 * $Revision: 3.11 $
 * $Date: 1996/09/30 19:39:45 $
 */

/*
 * Directory block size corresponds to basic block (sector) size,
 * for atomic writes.
 */
#define	EFS_DIRBSHIFT	BBSHIFT
#define EFS_DIRBSIZE	BBSIZE
#define	EFS_DIRBMASK	BBMASK

/*
 * A dirblk is composed of 3 major components: a header, entry offsets and
 * entries.  Initially, a dirblock is all zeros, except for the magic number
 * and the freeoffset.  A entries are allocated, a byte is reserved at
 * the beginning of the "space" array for holding the offset to the entry.
 * At the end of the "space" array the actual entry is stored.  The directory
 * is considered full, if a name is going to be added and (1) there is not
 * enough room for the dent plus halfword alignment padding plus the the
 * byte offset.
 *
 * The directory management procedures that return an "offset" actually
 * return a magic cookie with the following format:
 *	directory-block-number<23:0>|index-into-offsets<7:0>
 */

/* entry structure */
struct	efs_dent {
	union {
		__uint32_t 	l;
		ushort		s[2];
	} ud_inum;			/* inumber */
	unchar	d_namelen;		/* length of string in d_name */
	char	d_name[3];		/* name flex array */
};
/*
 * Minimum size of a dent is the whole structure, less the 3 bytes for
 * the name storage, plus one byte of a one character minimum name.
 */
#define	EFS_DENTSIZE	(sizeof(struct efs_dent) - 3 + 1)
#define	EFS_MAXNAMELEN	((1<<(sizeof(unsigned char)*BITSPERBYTE))-1)

/* dirblk structure */
#define	EFS_DIRBLK_HEADERSIZE	4	/* size of header of dirblk */
struct	efs_dirblk {
	/* begin header */
	ushort	magic;			/* magic number */
	unchar	firstused;		/* offset to first used dent byte */
	unchar	slots;			/* # of offset slots in dirblk */
	/* end header */

	/* rest is space for efs_dent's */
	unchar	space[EFS_DIRBSIZE - EFS_DIRBLK_HEADERSIZE];
};
#define	EFS_DIRBLK_MAGIC	0xBEEF	/* moo */

/* maximum number of entries that can fit in a dirblk */
#define	EFS_MAXENTS \
	((EFS_DIRBSIZE - EFS_DIRBLK_HEADERSIZE) / \
	 (EFS_DENTSIZE + sizeof(unchar)))

/*
 * Given a cookie or a byte offset into a file, convert it into an offset
 * to the beginning of the dirblk containing that offset/cookie.
 */
#define	EFS_DBOFF(cookie) \
	((cookie) & (~EFS_DIRBMASK & 0x7FFFFFFF))

/* construct a new cookie */
#define	EFS_MKCOOKIE(offset, slot) \
	(EFS_DBOFF(offset) | (slot))

/* given a cookie, return the slot # */
#define	EFS_SLOT(cookie) \
	((cookie) & EFS_DIRBMASK)

/* given a real dirblk byte offset, compact it */
#define	EFS_COMPACT(off)		((off) >> 1)

/* given a slot, return the offset for it */
#define	EFS_SLOTAT(db, slot) \
	EFS_REALOFF( (db)->space[slot] )

/* given a slotoffset, return an efs_dent for it */
#define	EFS_SLOTOFF_TO_DEP(db, slotoffset) \
	((struct efs_dent *) ((char *)(db) + slotoffset))

#define	EFS_SET_SLOT(db, slot, compactoffset) \
	((db)->space[slot] = (compactoffset))

/* tag for identifying free spots in the directory */
#define	EFS_FREESLOT		EFS_DIRBMASK

/* given a compacted offset, turn it into a real dirblk byte offset */
#define	EFS_REALOFF(coff) \
	((coff) << 1)

/*
 * Given the "firstused" value, determine offset to first used byte.  A value
 * of zero in "firstused" means no bytes are used, and that the offset to
 * the first used byte is outside the directories space.
 */
#define	EFS_DIR_FREEOFF(firstused) \
	(((firstused) == 0) ? EFS_DIRBSIZE : EFS_REALOFF(firstused))

/*
 * Given a dirblk, compute the size of the free space.  The size of the
 * is the amount of space prior to the first used byte, minus the
 * amount consumed by the dirblk header and the offsets for the dents.
 */
#define	EFS_DIR_FREESPACE(db) \
	(EFS_DIR_FREEOFF((db)->firstused) - \
	   (EFS_DIRBLK_HEADERSIZE + sizeof(unchar) * (db)->slots))

/*
 * The efs_dentsize macro gives the minimum record length which will hold the
 * directory entry pointed at by dep.  efs_dentsizebynamelen calculates the
 * size of a struct efs_dent excluding the name array, and adds the name
 * length.  Given a null-terminated string, efs_dentsizebyname computes the
 * size of an efs_dent which has that string as its name.  The dentsize
 * includes the possible padding to enforce halfword alignment.  It does
 * not include the offset byte.
 */
#define	efs_dentsizebynamelen(namelen) \
	(EFS_DENTSIZE + (namelen) - 1 + (((namelen) ^ 1) & 1) )
#define	efs_dentsize(dep) \
	efs_dentsizebynamelen((dep)->d_namelen)
#define	efs_dentsizebyname(name) \
	efs_dentsizebynamelen(strlen(name))

/*
 * Because the efs_dent structure is only 6 bytes long, and some machines
 * require ints to be on int boundaries, the following macros are used
 * to get at the inumber.
 */
#if defined(mips) || defined(PM2)
#define	EFS_INUM_ISZERO(dep) \
	(((dep)->ud_inum.s[0] == 0) && ((dep)->ud_inum.s[1] == 0))
#if _MIPSEB
#define	EFS_SET_INUM(dep, inum) { \
	(dep)->ud_inum.s[0] = (inum) >> 16; \
	(dep)->ud_inum.s[1] = (inum); \
}
#define	EFS_GET_INUM(dep) \
	(((dep)->ud_inum.s[0] << 16) + (dep)->ud_inum.s[1])
#endif /* _MIPSEB */
#if _MIPSEL
#define	EFS_SET_INUM(dep, inum) { \
	(dep)->ud_inum.s[1] = (inum) >> 16; \
	(dep)->ud_inum.s[0] = (inum); \
}
#define	EFS_GET_INUM(dep) \
	(((dep)->ud_inum.s[1] << 16) + (dep)->ud_inum.s[0])
#endif /* _MIPSEL */
#endif

/*
 * Minimum and maximum directory entry sizes.
 */
#define	EFS_MINDENTSIZE	efs_dentsizebynamelen(1)
#define	EFS_MAXDENTSIZE	efs_dentsizebynamelen(EFS_MAXNAMELEN)

#ifdef _KERNEL
#include <sys/dnlc.h>

/*
 * An abstract directory entry suitable for passing parameters efficiently
 * between efs vnode ops and efs directory ops.
 */
struct entry {
	struct ncfastdata e_fastdata;	/* name cache lookup results */
	efs_ino_t	e_inum;		/* inumber of sought entry */
	struct inode	*e_ip;		/* inode when not just looking up */
};
#define	e_name		e_fastdata.name
#define	e_namlen	e_fastdata.namlen
#define	e_flags		e_fastdata.flags
#define e_offset	e_fastdata.offset

/* efs_dirlookup how-to flags */
#define DLF_IGET	0x01	/* get entry inode if name lookup succeeds */
#define	DLF_MUSTHAVE	0x02	/* if entry does not exist return ENOENT */
#define DLF_ENTER	0x08	/* lookup in order to make an entry */
#define DLF_REMOVE	0x10	/* lookup in order to remove an entry */
#define DLF_EXCL	0x20	/* lookup to make an exclusive entry */

/* efs_dirisempty result flags */
#define DIR_HASDOT	0x1	/* "." is present and valid */
#define DIR_HASDOTDOT	0x2	/* ".." is present and valid */

/*
 * Directory operations.
 *
 * efs_dirlookup(dp, name, flags, &entry, cred) fills in entry, setting e_inum
 * to the found entry's i-number iff !(flags & DLF_IGET), otherwise getting an
 * inode for the entry's i-number or using dp in the case of ".", and in any
 * case returning 0 in e_inum and e_ip if the named entry is unlinked.
 *
 * efs_direnter(dp, ip, &entry, cred) uses the entry filled in by dirlookup to
 * make a new entry in dp; efs_dirremove(dp, &entry, cred) removes the looked-
 * up entry from dp.  Mkdir and rename use efs_dirinit(dp, parentino, cred) to
 * (re-)write the "." and ".." entries given a directory inode and its parent.
 * Directory emptiness is tested by calling efs_dirisempty(dp, &flags, cred)
 * and looking in flags for DIR_HASDOT and DIR_HASDOTDOT.  Rename over top of
 * an existing target name uses efs_dirrewrite(dp, ip, &entry, cred) instead
 * of efs_direnter.
 */
struct cred;
struct inode;
struct pathname;

extern int efs_dirlookup(struct inode *, char *, struct pathname *, int,
			 struct entry *, struct cred *);
extern int efs_direnter(struct inode *, struct inode *, struct entry *,
			struct cred *);
extern int efs_dirrewrite(struct inode *, struct inode *, struct entry *,
			  struct cred *);
extern int efs_dirremove(struct inode *, struct entry *, struct cred *);
extern int efs_dirinit(struct inode *, efs_ino_t, struct cred *);
extern int efs_dirisempty(struct inode *, int *, struct cred *);
extern int efs_notancestor(struct inode *, struct inode *, struct cred *);

/*
 * EFS readdir vnode op.
 */
struct uio;
struct vnode;

extern int efs_readdir(bhv_desc_t *, struct uio *, struct cred *, int *);

#endif	/* _KERNEL */
#endif	/* __efs_dir_ */
