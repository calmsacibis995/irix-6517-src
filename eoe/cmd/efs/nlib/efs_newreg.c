#ident "$Revision: 1.3 $"

/*
 * [Lifted from mkfs]
 * Now a new function efs_newregfile(). This creates a new regular file
 * of the given size; this file may have indirect extents.
 * This is done to avoid rehacking the
 * whole of libefs to know about indirect extents!
 * Basically to get round the size limitations when a mkfs proto specifies
 * a humongo file! Rather than successive extends, we allocate the space
 * all at once (since we're working from a regular file we already
 * know the size).
 *
 * Assumptions: 
 *
 *	efs_mknod has been called to allocate & initialize the dinode.
 *
 *	The bitmap is in core and is correctly representative of blocks
 *	used so far.
 */
#include "libefs.h"

static extent *findex(EFS_MOUNT *mp, efs_daddr_t block, int nblocks);
static void busyout(EFS_MOUNT *mp, efs_daddr_t bn, int len);

void
efs_newregfile(EFS_MOUNT *mp, efs_ino_t ino, char *name)
{
	struct stat sb;
	int len;
	efs_daddr_t blocks, allocblocks;
	int fd;
	struct efs_dinode *di;
	struct extent *exbase = NULL;
	struct extent *ex, *foundex;
	int extents, exbufsize;
	int copysize, copyoffset, copied;
	char *copybuf = NULL;
	static efs_daddr_t curblock = 2;   /* starting place to search for 
					* free blocks */
	int largestextent;
	int i;
	int numindirs = 0;
	int indirblocks;

	if ((fd = open(name, O_RDONLY)) < 0) {
		fprintf(stderr,"%s: can't open %s: %s\n", mp->m_progname, name,
			strerror(errno));
		exit(1);
	}
	if (fstat(fd, &sb) < 0) {
		fprintf(stderr,"%s: cannot stat %s: %s\n", mp->m_progname, name,
			strerror(errno));
		exit(1);
	}
	if ((sb.st_mode & S_IFMT) != S_IFREG) {
		fprintf(stderr,"%s: %s is not a regular file\n",
				mp->m_progname, name);
		exit(1);
	}
	len = sb.st_size; 
	blocks = (len + (EFS_BBSIZE - 1)) / EFS_BBSIZE;
	di = efs_iget(mp, ino);

	/* Guess at number of extents & allocate space for them. We will
	 * realloc later if it turns out we need more; however since it's
	 * assumed we're creating in a virgin fs that is unlikely.
	 */

	exbufsize = blocks / 64; 
	exbase = (struct extent *)
			malloc(EFS_BBSIZE + exbufsize * sizeof (struct extent));

	/* now allocate extent space from the bitmap until we've got enough
	 * extents to hold the file.
	 */

	allocblocks = 0;
	extents = 0;
	largestextent = 0;

	while (allocblocks < blocks) {
		if ((foundex = findex(mp, curblock, (blocks - allocblocks)))
								== NULL) {
			fprintf(stderr,
				"%s: cannot allocate space for file: %s\n",
				mp->m_progname, name);
			exit(1);
		}
		if (foundex->ex_length > largestextent)
			largestextent = foundex->ex_length; 
		curblock = foundex->ex_bn + foundex->ex_length;
		ex = (exbase + extents);
		ex->ex_magic = 0;
		ex->ex_bn = foundex->ex_bn;
		ex->ex_length = foundex->ex_length;
		ex->ex_offset = allocblocks;
		allocblocks += foundex->ex_length;
		if (++extents == exbufsize) {
			exbufsize += 10;
			if ((exbase = (struct extent *)realloc((char *)exbase, (EFS_BBSIZE + exbufsize * sizeof (struct extent)))) == NULL) {
				fprintf(stderr,
				    "%s: cannot allocate space for file: %s\n",
				    mp->m_progname, name);
				exit(1);
			}
		}
	}

	if (extents > EFS_DIRECTEXTENTS) {
		indirblocks = ((EFS_BBSIZE - 1 + (extents * sizeof(struct extent))) 				/ EFS_BBSIZE);
		allocblocks = 0;
		while (allocblocks < indirblocks) {
			foundex = findex(mp, curblock,
						(indirblocks - allocblocks));
			if (foundex == NULL) {
				fprintf(stderr,
				    "%s: cannot allocate space for file: %s\n",
				    mp->m_progname, name);
				exit(1);
			}
			curblock = foundex->ex_bn + foundex->ex_length;
			ex = &di->di_u.di_extents[numindirs];
			ex->ex_magic = 0;
			ex->ex_bn = foundex->ex_bn;
			ex->ex_length = foundex->ex_length;
			allocblocks += foundex->ex_length;
			if (++numindirs == EFS_DIRECTEXTENTS) {
				fprintf(stderr,
				    "%s: cannot allocate space for file: %s\n",
				    mp->m_progname, name);
				exit(1);
			}
		}
		di->di_u.di_extents[0].ex_offset = numindirs;
	}

	/* Hokay. Now we've allocated all the extents needed to hold the new 
	 * file's data (including indirect ones if any). Copy the data to the 
	 * appropriate places.
	 */

	if ((copybuf = (char *)malloc(largestextent * EFS_BBSIZE)) == NULL) {
		fprintf(stderr,"%s: can't get buffer memory for file copy\n",
			mp->m_progname);
		exit(1);
	}

	for (i = 0, ex = exbase, copied = 0; i < extents; i++) {
		copysize = ex->ex_length * EFS_BBSIZE;
		copyoffset = ex->ex_bn * EFS_BBSIZE;
		bzero(copybuf, copysize);
		if ((len - copied) < copysize) /* partial last block */
			copysize = len - copied;
		if (read(fd, copybuf, copysize) != copysize) {
			fprintf(stderr, "%s: error reading %s: %s\n", 
				mp->m_progname, name, strerror(errno));
			exit(1);
		}
		/* set copysize back to EFS_BBSIZE multiple: we may be working 
		 * on a raw device!
		 */
		copysize = ex->ex_length * EFS_BBSIZE;
		lseek(mp->m_fd, copyoffset, SEEK_SET);
		if (write(mp->m_fd, copybuf, copysize) != copysize) {
			fprintf(stderr, "%s: error writing %s: %s\n", 
				mp->m_progname, name, strerror(errno));
			exit(1);
		}
		copied += copysize;
		ex++;
	}
	free (copybuf);
	copybuf = NULL;

	/*
	 * Data copied. Now the extents: if < EFS_DIRECTEXTENTS, they go
	 * in the dinode. If greater, they must be written to the indirect
	 * extents allocated for them; pointers to these are already in 
	 * the dinode in that case.
	 */
	if (extents <= EFS_DIRECTEXTENTS) {
		for (i = 0, foundex = exbase, ex = di->di_u.di_extents;
			i < extents;
			i++, ex++, foundex++) {
			ex->ex_bn = foundex->ex_bn;
			ex->ex_length = foundex->ex_length;
			ex->ex_offset = foundex->ex_offset;
			ex->ex_magic = 0;
		}
	} else {
		copybuf = (char *)exbase;
		ex = di->di_u.di_extents;
		for (i = 0; i < numindirs; i++, ex++) {
			if (efs_writeb(mp->m_fd, copybuf, ex->ex_bn,
					ex->ex_length) != ex->ex_length) {
				fprintf(stderr, "%s: error writing %s: %s\n", 
					mp->m_progname, name, strerror(errno));
				exit(1);
			}
			copybuf += ex->ex_length * EFS_BBSIZE;
		}
		copybuf = NULL;
	}

	
	/* Busy out the appropriate parts of the bitmap. */

	for (i = 0, ex = exbase; i < extents; i++, ex++)
		busyout(mp, ex->ex_bn, ex->ex_length);

	if (numindirs)
		for (i = 0, ex = di->di_u.di_extents; i < numindirs; i++, ex++)
			busyout(mp, ex->ex_bn, ex->ex_length);

	di->di_size = len;
	di->di_numextents = extents;
	efs_iput(mp, di, ino);
	close (fd);
	if (exbase)
		free ((char *)exbase);
	if (copybuf)
		free ((char *)copybuf);
	return;
}

static struct extent *
findex(EFS_MOUNT *mp, efs_daddr_t block, int nblocks)
{
	static struct extent ex;
	efs_daddr_t nextblock = block;
	int foundblocks = 0;

	if (nblocks > EFS_MAXEXTENTLEN)
		nblocks = EFS_MAXEXTENTLEN;

	/* first skip any nonfree blocks */

	while (!btst(mp->m_bitmap, nextblock)) {
		nextblock++;	/* side effect warning: btst is a macro! */
		if (nextblock == (mp->m_fs->fs_size - 1))
			return (NULL);
	}
	block = nextblock;
		
	while ((foundblocks < nblocks)  && 
		(nextblock < mp->m_fs->fs_size) &&
		btst(mp->m_bitmap, nextblock)) {
		foundblocks++;
		nextblock++;
	}

	if (!foundblocks) 
		return (NULL);

	ex.ex_bn = block;
	ex.ex_length = foundblocks;

	return (&ex);
}


static void
busyout(EFS_MOUNT *mp, efs_daddr_t bn, int len)
{
	while (len--) {
		bclr(mp->m_bitmap, bn);
		bn++;		/* can't do it inside bclr: side effects!! */
		mp->m_fs->fs_tfree--;
	}
}
