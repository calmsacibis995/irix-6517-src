#ident "$Revision: 1.3 $"

/*
 * Enter the given name/inode pair into the given directory.
 */
#include "libefs.h"

void
efs_enter(EFS_MOUNT *mp, efs_ino_t dirinum, efs_ino_t inum, char *name)
{
	int namelen, wantsize;
	struct efs_dinode *di;
	struct efs_dent *dep;
#ifdef	BIG
	struct efs_big_dirblk db;
#else
	struct efs_dirblk db;
#endif
	efs_daddr_t bn;
	int offset;
	int firstused;

#ifdef	BIG
	if ((__efs_block_size != EFS_SMALL_BBSIZE) &&
	    (__efs_block_size != EFS_BIG_BBSIZE)) {
		fprintf(stderr, "efs_enter: Bad blocksize.\n");
		exit(1);
	}
#endif

#ifdef BBDEBUG
	printf("\nAdding directory: %s\n", name);
#endif

	/*
	 * Scan dirblks for a free spot.  Allocate a new dirblk if none
	 * contain enough free space.
	 */
	namelen = strlen(name);
	wantsize = efs_dentsizebynamelen(namelen);
#ifdef BBDEBUG
	printf("namelen == 0x%x, wantsize == 0x%x\n", namelen, wantsize);
	printf("dirinum == 0x%x\n", dirinum);
#endif
	di = efs_iget(mp, dirinum);
	for (offset = 0; ; offset += EFS_DIRBSIZE) {
#ifdef BBDEBUG
		printf("offset == 0x%x\n");
#endif
		bn = efs_bmap(mp, di, offset, EFS_DIRBSIZE);
#ifdef BBDEBUG
		printf("EFS_DIRBSIZE == 0x%x, bn == 0x%x\n", EFS_DIRBSIZE, bn);
#endif
		if (efs_readb(mp->m_fd, (char *)&db, bn, 1) != 1)
			error();
		if (db.magic != EFS_DIRBLK_MAGIC) {
#ifdef BBDEBUG
			printf("db.magic (0x%x) != EFS_DIRBLK_MAGIC - init\n",
				db.magic);
#endif
			/* must be a new block */
			db.magic = EFS_DIRBLK_MAGIC;
			db.slots = 0;
			db.firstused = 0;
		}
		if (EFS_DIR_FREESPACE(&db) > wantsize) {
			/* put name in free space */
#ifdef BBDEBUG
			printf("EFS_REALOFF(db.firstused) == 0x%x\n",
				EFS_REALOFF(db.firstused));
#endif
			firstused = EFS_DIR_FREEOFF(db.firstused) - wantsize;
#ifdef BBDEBUG
			printf("firstused == 0x%x, dep == 0x%x\n",
				firstused, dep);
#endif
			dep = EFS_SLOTOFF_TO_DEP(&db, firstused);
			EFS_SET_INUM(dep, inum);
			dep->d_namelen = namelen;
			bcopy(name, dep->d_name, namelen);
			db.firstused = EFS_COMPACT(firstused);
			EFS_SET_SLOT(&db, db.slots, db.firstused);
			db.slots++;
			if (efs_writeb(mp->m_fd, (char *)&db, bn, 1) != 1)
				error();
#ifdef BBDEBUG
			printf("Calling efs_iput: di == 0x%x,dirinum == 0x%x\n",
				di, dirinum);
#endif
			efs_iput(mp, di, dirinum);
			return;
		}
	}
}
