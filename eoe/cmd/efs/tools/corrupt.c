/*
 * Filesystem corrupter for testing fsck.
 *
 * Written by: Kipp Hickman
 *
 * $Source: /proj/irix6.5.7m/isms/eoe/cmd/efs/tools/RCS/corrupt.c,v $
 * $Revision: 1.2 $
 * $Date: 1997/06/03 20:58:03 $
 */
#include "efs.h"

char	*progname;
int	fs_fd;
struct	efs *fs;
char	*bitmap;
union {
	struct	efs fs;
	char	data[BBTOB(BTOBB(sizeof(struct efs)))];
} sblock;

efs_daddr_t	dirbn;
struct	efs_dirblk dirblk;
efs_ino_t	inum;

int	badDirSize();
int	badDirMagic();
int	badDirSlotCount();
int	badDirSlotOffset();
int	badDirSlotOverlap();
int	badDirName();
int	badDirDot();
int	badDirDotDot();

struct	hows {
	char	*test;
	char	*file;
	int	type;
	int	(*func)();
} hows[] = {
	{ "directory size", "test-dir-1", IFDIR, badDirSize },
	{ "directory magic", "test-dir-2", IFDIR, badDirMagic, },
	{ "directory slot count", "test-dir-3", IFDIR, badDirSlotCount, },
	{ "directory slot offset", "test-dir-4", IFDIR, badDirSlotOffset, },
	{ "directory slot overlap", "test-dir-5", IFDIR, badDirSlotOverlap, },
	{ "directory name", "test-dir-6", IFDIR, badDirName, },
	{ "directory dot", "test-dir-7", IFDIR, badDirDot, },
	{ "directory dot dot", "test-dir-8", IFDIR, badDirDotDot, },
};
#define	NHOWS	(sizeof(hows) / sizeof(struct hows))
struct	hows *current;

void	verify();
struct	efs_dinode *namei();

main(argc, argv)
	int argc;
	char *argv[];
{
	int i;
	struct efs_dinode *di;

	progname = argv[0];
	if (argc != 2) {
		fprintf(stderr, "usage: %s device\n", progname);
		exit(-1);
	}
	if ((fs_fd = open(argv[1], O_RDWR)) < 0) {
		fprintf(stderr, "%s: can't open %s\n", progname, argv[1]);
		exit(-1);
	}
	fs = &sblock.fs;
	if (efs_mount()) {
		fprintf(stderr, "%s: %s is not an extent filesystem\n",
				progname, argv[1]);
		exit(-1);
	}

	current = &hows[0];
	for (i = 0; i < NHOWS; i++, current++) {
		di = namei(current->file);
		printf("%s: test \"corrupt %s\" is messing up inode %u\n",
			    progname, current->test, inum);
		verify(current->type, di);
		(*current->func)(di);
	}
	exit(0);
}

/*
 * Make sure the given inode is of the correct type
 */
void
verify(format, di)
	int format;
	struct efs_dinode *di;
{
	if ((di->di_mode & IFMT) != format) {
		fprintf(stderr, "%s: file %s should have format %x\n",
				progname, current->file, format);
		exit(-1);
	}
}

/*
 * Read in a directory block
 */
void
getDirBlock(di, offset)
	struct efs_dinode *di;
	off_t offset;
{
	dirbn = efs_bmap(di, offset, EFS_DIRBSIZE);
	lseek(fs_fd, BBTOB(dirbn), 0);
	if (read(fs_fd, (char *) &dirblk, sizeof(dirblk)) != sizeof(dirblk))
		error();
}

void
putDirBlock()
{
	lseek(fs_fd, BBTOB(dirbn), 0);
	if (write(fs_fd, &dirblk, sizeof(dirblk)) != sizeof(dirblk))
		error();
}

/*
 * Translate a name into an inode pointer.  This only searches "/" in the
 * filesystem.
 */
struct efs_dinode *
namei(name)
	char *name;
{
	struct efs_dinode *root;
	off_t offset;
	struct efs_dirblk *db;
	struct efs_dent *dep;
	int slot;
	int namelen;

	root = efs_iget(EFS_ROOTINO);
	verify(IFDIR, root);
	namelen = strlen(name);

	/*
	 * Search root directory
	 */
	db = &dirblk;
	for (offset = 0; offset < root->di_size; offset += EFS_DIRBSIZE) {
		getDirBlock(root, offset);
		for (slot = 0; slot < db->slots; slot++) {
			if (EFS_SLOTAT(db, slot) == 0)
				continue;
			dep = EFS_SLOTOFF_TO_DEP(db, EFS_SLOTAT(db, slot));
			if ((dep->d_namelen == namelen) &&
			    (strncmp(dep->d_name, name, namelen) == 0)) {
				inum = EFS_GET_INUM(dep);
				return efs_iget(inum);
			}
		}
	}
	fprintf(stderr, "%s: file \"%s\" not found in filesystem\n",
			progname, name);
	exit(-1);
}

/*
 * Mangle a directories size
 */
badDirSize(di)
	struct efs_dinode *di;
{
	di->di_size = di->di_size | 1;
	efs_iput(di, inum);
}

/*
 * Mess up a directories magic number in the first directory block.
 */
badDirMagic(di)
	struct efs_dinode *di;
{
	getDirBlock(di, 0);
	dirblk.magic = 0;
	putDirBlock();
}

/*
 * Mess up count of directory slots
 */
badDirSlotCount(di)
	struct efs_dinode *di;
{
	getDirBlock(di, 0);
	dirblk.slots = EFS_MAXENTS + 1;
	putDirBlock();
}

/*
 * Mess up a valid slot offset - push into free space/slot array
 */
badDirSlotOffset(di)
	struct efs_dinode *di;
{
	int slot;
	int slotoff;

	getDirBlock(di, 0);
	for (slot = 0; slot < dirblk.slots; slot++) {
		slotoff = EFS_SLOTAT(&dirblk, slot);
		if (slotoff == 0)
			continue;
		EFS_SET_SLOT(&dirblk, slot, 1);
		putDirBlock();
		return;
	}
	fprintf(stderr, "%s: no valid names in directory block for file %s\n",
			progname, current->file);
	exit(-1);
}

/*
 * Make two slots overlap
 */
badDirSlotOverlap(di)
	struct efs_dinode *di;
{
	int slot, slot1, slot2;
	int slotoff;

	getDirBlock(di, 0);
	slot1 = slot2 = 0;
	for (slot = 0; slot < dirblk.slots; slot++) {
		slotoff = EFS_SLOTAT(&dirblk, slot);
		if (slotoff == 0)
			continue;
		if (slot1) {
			slot2 = slot;
			break;
		}
		slot1 = slot;
	}
	if ((slot1 == 0) || (slot2 == 0)) {
		fprintf(stderr,
			"%s: no valid names in directory block for file %s\n",
			progname, current->file);
		exit(-1);
	}

	EFS_SET_SLOT(&dirblk, slot1,
			      EFS_COMPACT(EFS_SLOTAT(&dirblk, slot2) + 2));
	putDirBlock();
}

/*
 * Put some slashes and null's into a directory name
 */
badDirName(di)
	struct efs_dinode *di;
{
	int slot;
	int slotoff;
	struct efs_dent *dep;

	getDirBlock(di, 0);
	for (slot = 0; slot < dirblk.slots; slot++) {
		slotoff = EFS_SLOTAT(&dirblk, slot);
		if (slotoff == 0)
			continue;
		dep = EFS_SLOTOFF_TO_DEP(&dirblk, slotoff);
		if (dep->d_namelen < 2)
			continue;
		dep->d_name[0] = '/';
		dep->d_name[1] = 0;
		putDirBlock();
		return;
	}
	fprintf(stderr, "%s: no valid names in directory block for file %s\n",
			progname, current->file);
	exit(-1);
}

/*
 * Mess up the directory name "."
 */
badDirDot(di)
	struct efs_dinode *di;
{
	int slot;
	int slotoff;
	struct efs_dent *dep;

	getDirBlock(di, 0);
	for (slot = 0; slot < dirblk.slots; slot++) {
		slotoff = EFS_SLOTAT(&dirblk, slot);
		if (slotoff == 0)
			continue;
		dep = EFS_SLOTOFF_TO_DEP(&dirblk, slotoff);
		if ((dep->d_namelen != 1) ||
		    (dep->d_name[0] != '.'))
			continue;
		dep->d_name[0] = 'Z';
		putDirBlock();
		return;
	}
	fprintf(stderr, "%s: no valid \".\" in directory block for file %s\n",
			progname, current->file);
	exit(-1);
}

/*
 * Mess up the directory name "."
 */
badDirDotDot(di)
	struct efs_dinode *di;
{
	int slot;
	int slotoff;
	struct efs_dent *dep;

	getDirBlock(di, 0);
	for (slot = 0; slot < dirblk.slots; slot++) {
		slotoff = EFS_SLOTAT(&dirblk, slot);
		if (slotoff == 0)
			continue;
		dep = EFS_SLOTOFF_TO_DEP(&dirblk, slotoff);
		if ((dep->d_namelen != 2) ||
		    (dep->d_name[0] != '.') ||
		    (dep->d_name[1] != '.'))
			continue;
		dep->d_name[0] = 'Z';
		dep->d_name[1] = 'Z';
		putDirBlock();
		return;
	}
	fprintf(stderr, "%s: no valid \"..\" in directory block for file %s\n",
			progname, current->file);
	exit(-1);
}

error()
{
	int old_errno;
	extern int errno;

	old_errno = errno;
	fprintf(stderr, "%s: i/o error\n", progname);
	errno = old_errno;
	perror(current->test);
	exit(-1);
}
