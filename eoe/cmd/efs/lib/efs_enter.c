#ident "$Revision: 1.5 $"

/*
 * Enter the given name/inode pair into the given directory.
 */
#include "efs.h"

#ifdef	OEFS
void
efs_enter(efs_ino_t dirino, efs_ino_t ino, char *name)
{
	struct efs_direct dir;

	bzero(&dir, sizeof(dir));
	dir.d_ino = ino;
	strcpy(dir.d_name, name);
	efs_write(dirino, &dir, sizeof(dir));
}
#endif

#ifdef	EFS
void
efs_enter(efs_ino_t dirinum, efs_ino_t inum, char *name)
{
	int namelen, wantsize;
	struct efs_dinode *di;
	struct efs_dent *dep;
	struct efs_dirblk db;
	efs_daddr_t bn;
	int offset;
	int firstused;

	/*
	 * Scan dirblks for a free spot.  Allocate a new dirblk if none
	 * contain enough free space.
	 */
	namelen = strlen(name);
	wantsize = efs_dentsizebynamelen(namelen);
	di = efs_iget(dirinum);
	for (offset = 0; ; offset += EFS_DIRBSIZE) {
		bn = efs_bmap(di, offset, EFS_DIRBSIZE);
		lseek(fs_fd, BBTOB(bn), SEEK_SET);
		if (read(fs_fd, &db, sizeof(db)) != sizeof(db))
			error();
		if (db.magic != EFS_DIRBLK_MAGIC) {
			/* must be a new block */
			db.magic = EFS_DIRBLK_MAGIC;
			db.slots = 0;
			db.firstused = 0;
		}
		if (EFS_DIR_FREESPACE(&db) > wantsize) {
			/* put name in free space */
			firstused = EFS_DIR_FREEOFF(db.firstused) - wantsize;
			dep = EFS_SLOTOFF_TO_DEP(&db, firstused);
			EFS_SET_INUM(dep, inum);
			dep->d_namelen = namelen;
			bcopy(name, dep->d_name, namelen);
			db.firstused = EFS_COMPACT(firstused);
			EFS_SET_SLOT(&db, db.slots, db.firstused);
			db.slots++;
			lseek(fs_fd, BBTOB(bn), SEEK_SET);
			if (write(fs_fd, &db, sizeof(db)) != sizeof(db))
				error();
			efs_iput(di, dirinum);
			return;
		}
	}
}
#endif
