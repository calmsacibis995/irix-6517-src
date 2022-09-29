#ident "$Revision: 1.14 $"

#include <sys/param.h>
#include <sys/attributes.h>
#include <sys/stat.h>
#include <sys/syssgi.h>
#include <sys/time.h>
#include <sys/uuid.h>
#include <sys/wait.h>
#include <sys/fs/xfs_fsops.h>
#include <sys/fs/xfs_itable.h>
#include <sys/fs/xfs_error.h>
#include <bstring.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

typedef enum {
	OP_ALLOCSP,
	OP_ATTR_REMOVE,
	OP_ATTR_SET,
	OP_BULKSTAT,
	OP_BULKSTAT1,
	OP_CHOWN,
	OP_CREAT,
	OP_DREAD,
	OP_DWRITE,
	OP_FDATASYNC,
	OP_FREESP,
	OP_FSYNC,
	OP_GETDENTS,
	OP_LINK,
	OP_MKDIR,
	OP_MKNOD,
	OP_READ,
	OP_READLINK,
	OP_RENAME,
	OP_RESVSP,
	OP_RMDIR,
	OP_STAT,
	OP_SYMLINK,
	OP_SYNC,
	OP_TRUNCATE,
	OP_UNLINK,
	OP_UNRESVSP,
	OP_WRITE,
	OP_LAST
} opty_t;

typedef void (*opfnc_t)(int, long);

typedef struct opdesc {
	opty_t	op;
	char	*name;
	opfnc_t	func;
	int	freq;
	int	iswrite;
} opdesc_t;

typedef struct fent {
	int	id;
	int	parent;
} fent_t;

typedef struct flist {
	int	nfiles;
	int	nslots;
	int	tag;
	fent_t	*fents;
} flist_t;

typedef struct pathname {
	int	len;
	char	*path;
} pathname_t;

#define	FT_DIR	0
#define	FT_DIRm	(1 << FT_DIR)
#define	FT_REG	1
#define	FT_REGm	(1 << FT_REG)
#define	FT_SYM	2
#define	FT_SYMm	(1 << FT_SYM)
#define	FT_DEV	3
#define	FT_DEVm	(1 << FT_DEV)
#define	FT_RTF	4
#define	FT_RTFm	(1 << FT_RTF)
#define	FT_nft	5
#define	FT_ANYm	((1 << FT_nft) - 1)
#define	FT_REGFILE	(FT_REGm | FT_RTFm)
#define	FT_NOTDIR	(FT_ANYm & ~FT_DIRm)

#define	FLIST_SLOT_INCR	16
#define	NDCACHE	64

#define	MAXFSIZE	((1ULL << 63) - 1ULL)
#define	MAXFSIZE32	((1ULL << 40) - 1ULL)

void	allocsp_f(int, long);
void	attr_remove_f(int, long);
void	attr_set_f(int, long);
void	bulkstat_f(int, long);
void	bulkstat1_f(int, long);
void	chown_f(int, long);
void	creat_f(int, long);
void	dread_f(int, long);
void	dwrite_f(int, long);
void	fdatasync_f(int, long);
void	freesp_f(int, long);
void	fsync_f(int, long);
void	getdents_f(int, long);
void	link_f(int, long);
void	mkdir_f(int, long);
void	mknod_f(int, long);
void	read_f(int, long);
void	readlink_f(int, long);
void	rename_f(int, long);
void	resvsp_f(int, long);
void	rmdir_f(int, long);
void	stat_f(int, long);
void	symlink_f(int, long);
void	sync_f(int, long);
void	truncate_f(int, long);
void	unlink_f(int, long);
void	unresvsp_f(int, long);
void	write_f(int, long);

opdesc_t	ops[] = {
	{ OP_ALLOCSP, "allocsp", allocsp_f, 1, 1 },
	{ OP_ATTR_REMOVE, "attr_remove", attr_remove_f, 1, 1 },
	{ OP_ATTR_SET, "attr_set", attr_set_f, 2, 1 },
	{ OP_BULKSTAT, "bulkstat", bulkstat_f, 1, 0 },
	{ OP_BULKSTAT1, "bulkstat1", bulkstat1_f, 1, 0 },
	{ OP_CHOWN, "chown", chown_f, 3, 1 },
	{ OP_CREAT, "creat", creat_f, 4, 1 },
	{ OP_DREAD, "dread", dread_f, 4, 0 },
	{ OP_DWRITE, "dwrite", dwrite_f, 4, 1 },
	{ OP_FDATASYNC, "fdatasync", fdatasync_f, 1, 1 },
	{ OP_FREESP, "freesp", freesp_f, 1, 1 },
	{ OP_FSYNC, "fsync", fsync_f, 1, 1 },
	{ OP_GETDENTS, "getdents", getdents_f, 1, 0 },
	{ OP_LINK, "link", link_f, 1, 1 },
	{ OP_MKDIR, "mkdir", mkdir_f, 2, 1 },
	{ OP_MKNOD, "mknod", mknod_f, 2, 1 },
	{ OP_READ, "read", read_f, 1, 0 },
	{ OP_READLINK, "readlink", readlink_f, 1, 0 },
	{ OP_RENAME, "rename", rename_f, 2, 1 },
	{ OP_RESVSP, "resvsp", resvsp_f, 1, 1 },
	{ OP_RMDIR, "rmdir", rmdir_f, 1, 1 },
	{ OP_STAT, "stat", stat_f, 1, 0 },
	{ OP_SYMLINK, "symlink", symlink_f, 2, 1 },
	{ OP_SYNC, "sync", sync_f, 1, 0 },
	{ OP_TRUNCATE, "truncate", truncate_f, 2, 1 },
	{ OP_UNLINK, "unlink", unlink_f, 1, 1 },
	{ OP_UNRESVSP, "unresvsp", unresvsp_f, 1, 1 },
	{ OP_WRITE, "write", write_f, 4, 1 },
}, *ops_end;

flist_t	flist[FT_nft] = {
	{ 0, 0, 'd', NULL },
	{ 0, 0, 'f', NULL },
	{ 0, 0, 'l', NULL },
	{ 0, 0, 'c', NULL },
	{ 0, 0, 'r', NULL },
};

int		dcache[NDCACHE];
int		errrange;
int		errtag;
opty_t		*freq_table;
int		freq_table_size;
xfs_fsop_geom_t	geom;
char		*homedir;
int		*ilist;
int		ilistlen;
off64_t		maxfsize;
char		*myprog;
int		namerand;
int		nameseq;
int		nops;
int		nproc = 1;
int		operations = 1;
int		procid;
unsigned long	rstate[256 / sizeof(unsigned long)];
int		rtpct;
unsigned long	seed = 0;
ino_t		top_ino;
int		verbose = 0;

void	add_to_flist(int, int, int);
void	append_pathname(pathname_t *, char *);
int	attr_list_path(pathname_t *, char *, const int, int,
		       attrlist_cursor_t *);
int	attr_remove_path(pathname_t *, const char *, int);
int	attr_set_path(pathname_t *, const char *, const char *, const int, int);
void	check_cwd(void);
int	creat_path(pathname_t *, mode_t);
void	dcache_enter(int, int);
void	dcache_init(void);
fent_t	*dcache_lookup(int);
void	dcache_purge(int);
void	del_from_flist(int, int);
int	dirid_to_name(char *, int);
void	doproc(void);
void	fent_to_name(pathname_t *, flist_t *, fent_t *);
void	fix_parent(int, int);
void	free_pathname(pathname_t *);
int	generate_fname(fent_t *, int, pathname_t *, int *, int *);
int	get_fname(int, long, pathname_t *, flist_t **, fent_t **, int *);
void	init_pathname(pathname_t *);
int	lchown_path(pathname_t *, uid_t, gid_t);
int	link_path(pathname_t *, pathname_t *);
int	lstat64_path(pathname_t *, struct stat64 *);
void	make_freq_table(void);
int	mkdir_path(pathname_t *, mode_t);
int	mknod_path(pathname_t *, mode_t, dev_t);
void	namerandpad(int, char *, int);
int	open_path(pathname_t *, int);
DIR	*opendir_path(pathname_t *);
void	process_freq(char *);
int	readlink_path(pathname_t *, char *, size_t);
int	rename_path(pathname_t *, pathname_t *);
int	rmdir_path(pathname_t *);
void	separate_pathname(pathname_t *, char *, pathname_t *);
void	show_ops(int, char *);
int	stat64_path(pathname_t *, struct stat64 *);
int	symlink_path(const char *, pathname_t *);
int	truncate64_path(pathname_t *, off64_t);
int	unlink_path(pathname_t *);
void	usage(void);
void	write_freq(void);
void	zero_freq(void);

main(int argc, char **argv)
{
	char		buf[10];
	int		c;
	char		*dirname = ".";
	int		fd;
	int		i;
	int		j;
	char		*p;
	int		stat;
	struct timeval	t;
	ptrdiff_t	srval;

	errrange = errtag = 0;
	umask(0);
	nops = sizeof(ops) / sizeof(ops[0]);
	ops_end = &ops[nops];
	myprog = argv[0];
	while ((c = getopt(argc, argv, "d:e:f:i:n:p:rs:vwzHS")) != -1) {
		switch (c) {
		case 'd':
			dirname = optarg;
			break;
		case 'e':
			sscanf(optarg, "%d", &errtag);
			if (errtag < 0) {
				errtag = -errtag;
				errrange = 1;
			} else if (errtag == 0)
				errtag = -1;
			if (errtag >= XFS_ERRTAG_MAX) {
				fprintf(stderr,
					"error tag %d too large (max %d)\n",
					errtag, XFS_ERRTAG_MAX - 1);
				exit(1);
			}
			break;
		case 'f':
			process_freq(optarg);
			break;
		case 'i':
			ilist = realloc(ilist, ++ilistlen * sizeof(*ilist));
			ilist[ilistlen - 1] = strtol(optarg, &p, 16);
			break;
		case 'n':
			operations = atoi(optarg);
			break;
		case 'p':
			nproc = atoi(optarg);
			break;
		case 'r':
			namerand = 1;
			break;
		case 's':
			seed = strtoul(optarg, NULL, 0);
			break;
		case 'v':
			verbose = 1;
			break;
		case 'w':
			write_freq();
			break;
		case 'z':
			zero_freq();
			break;
		case 'S':
			show_ops(0, NULL);
			printf("\n");
			break;
		case '?':
			fprintf(stderr, "%s - invalid parameters\n",
				myprog);
			/* fall through */
		case 'H':
			usage();
			exit(1);
		}
	}
	(void)mkdir(dirname, 0777);
	if (chdir(dirname) < 0) {
		perror(dirname);
		exit(1);
	}
	sprintf(buf, "fss%x", getpid());
	fd = creat(buf, 0666);
	if (lseek64(fd, (off64_t)(MAXFSIZE32 + 1), SEEK_SET) < 0)
		maxfsize = (off64_t)MAXFSIZE32;
	else
		maxfsize = (off64_t)MAXFSIZE;
	make_freq_table();
	dcache_init();
	setlinebuf(stdout);
	if (!seed) {
		gettimeofday(&t, (void *)NULL);
		seed = (int)t.tv_sec ^ (int)t.tv_usec;
		printf("seed = %d\n", seed);
	}
	i = syssgi(SGI_XFS_FSOPERATIONS, fd, XFS_FS_GEOMETRY, NULL, &geom);
	if (i >= 0 && geom.rtblocks)
		rtpct = MIN(MAX(geom.rtblocks * 100 /
				(geom.rtblocks + geom.datablocks), 1), 99);
	else
		rtpct = 0;
	if (errtag != 0) {
		if (errrange == 0) {
			if (errtag <= 0) {
				initstate((unsigned)seed, (char *)rstate,
					sizeof(rstate));
				j = random() % 100;

				for (i = 0; i < j; i++)
					(void) random();

				errtag = (random() % (XFS_ERRTAG_MAX-1)) + 1;
			}
		} else {
			initstate((unsigned)seed, (char *)rstate,
				sizeof(rstate));
			j = random() % 100;

			for (i = 0; i < j; i++)
				(void) random();

			errtag += (random() % (XFS_ERRTAG_MAX - errtag));
		}
		printf("Injecting failure on tag #%d\n", errtag);
		srval = syssgi(SGI_XFS_INJECT_ERROR, errtag, fd);
		if (srval == -1) {
			perror("fsstress - syssgi error injection call");
			close(fd);
			unlink(buf);
			exit(1);
		}
	} else
		close(fd);
	unlink(buf);
	for (i = 0; i < nproc; i++) {
		if (fork() == 0) {
			procid = i;
			doproc();
			return 0;
		}
	}
	while (wait(&stat) > 0)
		continue;
	if (errtag != 0) {
		if ((srval = syssgi(SGI_XFS_CLEARALL_ERROR, fd)) == -1) {
			perror("fsstress - syssgi clear error injection call");
			close(fd);
			exit(1);
		}
		close(fd);
	}

	return 0;
}

void
add_to_flist(int ft, int id, int parent)
{
	fent_t	*fep;
	flist_t	*ftp;

	ftp = &flist[ft];
	if (ftp->nfiles == ftp->nslots) {
		ftp->nslots += FLIST_SLOT_INCR;
		ftp->fents = realloc(ftp->fents, ftp->nslots * sizeof(fent_t));
	}
	fep = &ftp->fents[ftp->nfiles++];
	fep->id = id;
	fep->parent = parent;
}

void
append_pathname(pathname_t *name, char *str)
{
	int	len;

	len = strlen(str);
#ifdef DEBUG
	if (len && *str == '/' && name->len == 0) {
		fprintf(stderr, "fsstress: append_pathname failure\n");
		chdir(homedir);
		abort();
		/* NOTREACHED */
	}
#endif
	name->path = realloc(name->path, name->len + 1 + len);
	strcpy(&name->path[name->len], str);
	name->len += len;
}

int
attr_list_path(pathname_t *name, char *buffer, const int buffersize, int flags,
	       attrlist_cursor_t *cursor)
{
	char		buf[MAXNAMELEN];
	pathname_t	newname;
	int		rval;

	rval = attr_list(name->path, buffer, buffersize, flags, cursor);
	if (rval >= 0 || errno != ENAMETOOLONG)
		return rval;
	separate_pathname(name, buf, &newname);
	if (chdir(buf) == 0) {
		rval = attr_list_path(&newname, buffer, buffersize, flags,
			cursor);
		chdir("..");
	}
	free_pathname(&newname);
	return rval;
}

int
attr_remove_path(pathname_t *name, const char *attrname, int flags)
{
	char		buf[MAXNAMELEN];
	pathname_t	newname;
	int		rval;

	rval = attr_remove(name->path, attrname, flags);
	if (rval >= 0 || errno != ENAMETOOLONG)
		return rval;
	separate_pathname(name, buf, &newname);
	if (chdir(buf) == 0) {
		rval = attr_remove_path(&newname, attrname, flags);
		chdir("..");
	}
	free_pathname(&newname);
	return rval;
}

int
attr_set_path(pathname_t *name, const char *attrname, const char *attrvalue,
	      const int valuelength, int flags)
{
	char		buf[MAXNAMELEN];
	pathname_t	newname;
	int		rval;

	rval = attr_set(name->path, attrname, attrvalue, valuelength, flags);
	if (rval >= 0 || errno != ENAMETOOLONG)
		return rval;
	separate_pathname(name, buf, &newname);
	if (chdir(buf) == 0) {
		rval = attr_set_path(&newname, attrname, attrvalue, valuelength,
			flags);
		chdir("..");
	}
	free_pathname(&newname);
	return rval;
}

void
check_cwd(void)
{
#ifdef DEBUG
	struct stat64	statbuf;

	if (stat64(".", &statbuf) == 0 && statbuf.st_ino == top_ino)
		return;
	chdir(homedir);
	fprintf(stderr, "fsstress: check_cwd failure\n");
	abort();
	/* NOTREACHED */
#endif
}

int
creat_path(pathname_t *name, mode_t mode)
{
	char		buf[MAXNAMELEN];
	pathname_t	newname;
	int		rval;

	rval = creat(name->path, mode);
	if (rval >= 0 || errno != ENAMETOOLONG)
		return rval;
	separate_pathname(name, buf, &newname);
	if (chdir(buf) == 0) {
		rval = creat_path(&newname, mode);
		chdir("..");
	}
	free_pathname(&newname);
	return rval;
}

void
dcache_enter(int dirid, int slot)
{
	dcache[dirid % NDCACHE] = slot;
}

void
dcache_init(void)
{
	int	i;

	for (i = 0; i < NDCACHE; i++)
		dcache[i] = -1;
}

fent_t *
dcache_lookup(int dirid)
{
	fent_t	*fep;
	int	i;

	i = dcache[dirid % NDCACHE];
	if (i >= 0 && (fep = &flist[FT_DIR].fents[i])->id == dirid)
		return fep;
	return NULL;
}

void
dcache_purge(int dirid)
{
	int	*dcp;

	dcp = &dcache[dirid % NDCACHE];
	if (*dcp >= 0 && flist[FT_DIR].fents[*dcp].id == dirid)
		*dcp = -1;
}

void
del_from_flist(int ft, int slot)
{
	flist_t	*ftp;

	ftp = &flist[ft];
	if (ft == FT_DIR)
		dcache_purge(ftp->fents[slot].id);
	if (slot != ftp->nfiles - 1) {
		if (ft == FT_DIR)
			dcache_purge(ftp->fents[ftp->nfiles - 1].id);
		ftp->fents[slot] = ftp->fents[--ftp->nfiles];
	} else
		ftp->nfiles--;
}

fent_t *
dirid_to_fent(int dirid)
{
	fent_t	*efep;
	fent_t	*fep;
	flist_t	*flp;

	if (fep = dcache_lookup(dirid))
		return fep;
	flp = &flist[FT_DIR];
	for (fep = flp->fents, efep = &fep[flp->nfiles]; fep < efep; fep++) {
		if (fep->id == dirid) {
			dcache_enter(dirid, fep - flp->fents);
			return fep;
		}
	}
	return NULL;
}

void
doproc(void)
{
	struct stat64	statbuf;
	char		buf[10];
	int		opno;
	int		rval;
	opdesc_t	*p;

	sprintf(buf, "p%x", procid);
	(void)mkdir(buf, 0777);
	if (chdir(buf) < 0 || stat64(".", &statbuf) < 0) {
		perror(buf);
		_exit(1);
	}
	top_ino = statbuf.st_ino;
	homedir = getcwd(NULL, -1);
	seed += procid;
	initstate((unsigned)seed, (char *)rstate, sizeof(rstate));
	if (namerand)
		namerand = random();
	for (opno = 0; opno < operations; opno++) {
		p = &ops[freq_table[random() % freq_table_size]];
		p->func(opno, random());
		/*
		 * test for forced shutdown by stat'ing the test
		 * directory.  If this stat returns EIO, assume
		 * the forced shutdown happened.
		 */
		if (errtag != 0 && opno % 100 == 0)  {
			rval = stat64(".", &statbuf);
			if (rval == EIO)  {
				fprintf(stderr, "Detected EIO\n");
				return;
			}
		}
	}
}

void
fent_to_name(pathname_t *name, flist_t *flp, fent_t *fep)
{
	char	buf[MAXNAMELEN];
	int	i;
	fent_t	*pfep;

	if (fep == NULL)
		return;
	if (fep->parent != -1) {
		pfep = dirid_to_fent(fep->parent);
		fent_to_name(name, &flist[FT_DIR], pfep);
		append_pathname(name, "/");
	}
	i = sprintf(buf, "%c%x", flp->tag, fep->id);
	namerandpad(fep->id, buf, i);
	append_pathname(name, buf);
}

void
fix_parent(int oldid, int newid)
{
	fent_t	*fep;
	flist_t	*flp;
	int	i;
	int	j;

	for (i = 0, flp = flist; i < FT_nft; i++, flp++) {
		for (j = 0, fep = flp->fents; j < flp->nfiles; j++, fep++) {
			if (fep->parent == oldid)
				fep->parent = newid;
		}
	}
}

void
free_pathname(pathname_t *name)
{
	if (name->path) {
		free(name->path);
		name->path = NULL;
		name->len = 0;
	}
}

int
generate_fname(fent_t *fep, int ft, pathname_t *name, int *idp, int *v)
{
	char	buf[MAXNAMELEN];
	flist_t	*flp;
	int	id;
	int	j;
	int	len;

	flp = &flist[ft];
	len = sprintf(buf, "%c%x", flp->tag, id = nameseq++);
	namerandpad(id, buf, len);
	if (fep) {
		fent_to_name(name, &flist[FT_DIR], fep);
		append_pathname(name, "/");
	}
	append_pathname(name, buf);
	*idp = id;
	*v = verbose;
	for (j = 0; !*v && j < ilistlen; j++) {
		if (ilist[j] == id) {
			*v = 1;
			break;
		}
	}
	return 1;
}

int
get_fname(int which, long r, pathname_t *name, flist_t **flpp, fent_t **fepp,
	  int *v)
{
	int	c;
	fent_t	*fep;
	flist_t	*flp;
	int	i;
	int	j;
	int	x;

	for (i = 0, c = 0, flp = flist; i < FT_nft; i++, flp++) {
		if (which & (1 << i))
			c += flp->nfiles;
	}
	if (c == 0) {
		if (flpp)
			*flpp = NULL;
		if (fepp)
			*fepp = NULL;
		*v = verbose;
		return 0;
	}
	x = (int)(r % c);
	for (i = 0, c = 0, flp = flist; i < FT_nft; i++, flp++) {
		if (which & (1 << i)) {
			if (x < c + flp->nfiles) {
				fep = &flp->fents[x - c];
				if (name)
					fent_to_name(name, flp, fep);
				if (flpp)
					*flpp = flp;
				if (fepp)
					*fepp = fep;
				*v = verbose;
				for (j = 0; !*v && j < ilistlen; j++) {
					if (ilist[j] == fep->id) {
						*v = 1;
						break;
					}
				}
				return 1;
			}
			c += flp->nfiles;
		}
	}
#ifdef DEBUG
	fprintf(stderr, "fsstress: get_fname failure\n");
	abort();
#endif
	/* NOTREACHED */
}

void
init_pathname(pathname_t *name)
{
	name->len = 0;
	name->path = NULL;
}

int
lchown_path(pathname_t *name, uid_t owner, gid_t group)
{
	char		buf[MAXNAMELEN];
	pathname_t	newname;
	int		rval;

	rval = lchown(name->path, owner, group);
	if (rval >= 0 || errno != ENAMETOOLONG)
		return rval;
	separate_pathname(name, buf, &newname);
	if (chdir(buf) == 0) {
		rval = lchown_path(&newname, owner, group);
		chdir("..");
	}
	free_pathname(&newname);
	return rval;
}

int
link_path(pathname_t *name1, pathname_t *name2)
{
	char		buf1[MAXNAMELEN];
	char		buf2[MAXNAMELEN];
	int		down1;
	pathname_t	newname1;
	pathname_t	newname2;
	int		rval;

	rval = link(name1->path, name2->path);
	if (rval >= 0 || errno != ENAMETOOLONG)
		return rval;
	separate_pathname(name1, buf1, &newname1);
	separate_pathname(name2, buf2, &newname2);
	if (strcmp(buf1, buf2) == 0) {
		if (chdir(buf1) == 0) {
			rval = link_path(&newname1, &newname2);
			chdir("..");
		}
	} else {
		if (strcmp(buf1, "..") == 0)
			down1 = 0;
		else if (strcmp(buf2, "..") == 0)
			down1 = 1;
		else if (strlen(buf1) == 0)
			down1 = 0;
		else if (strlen(buf2) == 0)
			down1 = 1;
		else
			down1 = MAX(newname1.len, 3 + name2->len) <=
				MAX(3 + name1->len, newname2.len);
		if (down1) {
			free_pathname(&newname2);
			append_pathname(&newname2, "../");
			append_pathname(&newname2, name2->path);
			if (chdir(buf1) == 0) {
				rval = link_path(&newname1, &newname2);
				chdir("..");
			}
		} else {
			free_pathname(&newname1);
			append_pathname(&newname1, "../");
			append_pathname(&newname1, name1->path);
			if (chdir(buf2) == 0) {
				rval = link_path(&newname1, &newname2);
				chdir("..");
			}
		}
	}
	free_pathname(&newname1);
	free_pathname(&newname2);
	return rval;
}

int
lstat64_path(pathname_t *name, struct stat64 *sbuf)
{
	char		buf[MAXNAMELEN];
	pathname_t	newname;
	int		rval;

	rval = lstat64(name->path, sbuf);
	if (rval >= 0 || errno != ENAMETOOLONG)
		return rval;
	separate_pathname(name, buf, &newname);
	if (chdir(buf) == 0) {
		rval = lstat64_path(&newname, sbuf);
		chdir("..");
	}
	free_pathname(&newname);
	return rval;
}

void
make_freq_table(void)
{
	int		f;
	int		i;
	opdesc_t	*p;

	for (p = ops, f = 0; p < ops_end; p++)
		f += p->freq;
	freq_table = malloc(f * sizeof(*freq_table));
	freq_table_size = f;
	for (p = ops, i = 0; p < ops_end; p++) {
		for (f = 0; f < p->freq; f++, i++)
			freq_table[i] = p->op;
	}
}

int
mkdir_path(pathname_t *name, mode_t mode)
{
	char		buf[MAXNAMELEN];
	pathname_t	newname;
	int		rval;

	rval = mkdir(name->path, mode);
	if (rval >= 0 || errno != ENAMETOOLONG)
		return rval;
	separate_pathname(name, buf, &newname);
	if (chdir(buf) == 0) {
		rval = mkdir_path(&newname, mode);
		chdir("..");
	}
	free_pathname(&newname);
	return rval;
}

int
mknod_path(pathname_t *name, mode_t mode, dev_t dev)
{
	char		buf[MAXNAMELEN];
	pathname_t	newname;
	int		rval;

	rval = mknod(name->path, mode, dev);
	if (rval >= 0 || errno != ENAMETOOLONG)
		return rval;
	separate_pathname(name, buf, &newname);
	if (chdir(buf) == 0) {
		rval = mknod_path(&newname, mode, dev);
		chdir("..");
	}
	free_pathname(&newname);
	return rval;
}

void
namerandpad(int id, char *buf, int i)
{
	int		bucket;
	static int	buckets[] =
				{ 2, 4, 8, 16, 32, 64, 128, MAXNAMELEN - 1 };
	int		padlen;
	int		padmod;

	if (namerand == 0)
		return;
	bucket = (id ^ namerand) % (sizeof(buckets) / sizeof(buckets[0]));
	padmod = buckets[bucket] + 1 - i;
	if (padmod <= 0)
		return;
	padlen = (id ^ namerand) % padmod;
	if (padlen) {
		memset(&buf[i], 'X', padlen);
		buf[i + padlen] = '\0';
	}
}

int
open_path(pathname_t *name, int oflag)
{
	char		buf[MAXNAMELEN];
	pathname_t	newname;
	int		rval;

	rval = open(name->path, oflag);
	if (rval >= 0 || errno != ENAMETOOLONG)
		return rval;
	separate_pathname(name, buf, &newname);
	if (chdir(buf) == 0) {
		rval = open_path(&newname, oflag);
		chdir("..");
	}
	free_pathname(&newname);
	return rval;
}

DIR *
opendir_path(pathname_t *name)
{
	char		buf[MAXNAMELEN];
	pathname_t	newname;
	DIR		*rval;

	rval = opendir(name->path);
	if (rval || errno != ENAMETOOLONG)
		return rval;
	separate_pathname(name, buf, &newname);
	if (chdir(buf) == 0) {
		rval = opendir_path(&newname);
		chdir("..");
	}
	free_pathname(&newname);
	return rval;
}

void
process_freq(char *arg)
{
	opdesc_t	*p;
	char		*s;

	s = strchr(arg, '=');
	if (s == NULL) {
		fprintf(stderr, "bad argument '%s'\n", arg);
		exit(1);
	}
	*s++ = '\0';
	for (p = ops; p < ops_end; p++) {
		if (strcmp(arg, p->name) == 0) {
			p->freq = atoi(s);
			return;
		}
	}
	fprintf(stderr, "can't find op type %s for -f\n", arg);
	exit(1);
}

int
readlink_path(pathname_t *name, char *lbuf, size_t lbufsiz)
{
	char		buf[MAXNAMELEN];
	pathname_t	newname;
	int		rval;

	rval = readlink(name->path, lbuf, lbufsiz);
	if (rval >= 0 || errno != ENAMETOOLONG)
		return rval;
	separate_pathname(name, buf, &newname);
	if (chdir(buf) == 0) {
		rval = readlink_path(&newname, lbuf, lbufsiz);
		chdir("..");
	}
	free_pathname(&newname);
	return rval;
}

int
rename_path(pathname_t *name1, pathname_t *name2)
{
	char		buf1[MAXNAMELEN];
	char		buf2[MAXNAMELEN];
	int		down1;
	pathname_t	newname1;
	pathname_t	newname2;
	int		rval;

	rval = rename(name1->path, name2->path);
	if (rval >= 0 || errno != ENAMETOOLONG)
		return rval;
	separate_pathname(name1, buf1, &newname1);
	separate_pathname(name2, buf2, &newname2);
	if (strcmp(buf1, buf2) == 0) {
		if (chdir(buf1) == 0) {
			rval = rename_path(&newname1, &newname2);
			chdir("..");
		}
	} else {
		if (strcmp(buf1, "..") == 0)
			down1 = 0;
		else if (strcmp(buf2, "..") == 0)
			down1 = 1;
		else if (strlen(buf1) == 0)
			down1 = 0;
		else if (strlen(buf2) == 0)
			down1 = 1;
		else
			down1 = MAX(newname1.len, 3 + name2->len) <=
				MAX(3 + name1->len, newname2.len);
		if (down1) {
			free_pathname(&newname2);
			append_pathname(&newname2, "../");
			append_pathname(&newname2, name2->path);
			if (chdir(buf1) == 0) {
				rval = rename_path(&newname1, &newname2);
				chdir("..");
			}
		} else {
			free_pathname(&newname1);
			append_pathname(&newname1, "../");
			append_pathname(&newname1, name1->path);
			if (chdir(buf2) == 0) {
				rval = rename_path(&newname1, &newname2);
				chdir("..");
			}
		}
	}
	free_pathname(&newname1);
	free_pathname(&newname2);
	return rval;
}

int
rmdir_path(pathname_t *name)
{
	char		buf[MAXNAMELEN];
	pathname_t	newname;
	int		rval;

	rval = rmdir(name->path);
	if (rval >= 0 || errno != ENAMETOOLONG)
		return rval;
	separate_pathname(name, buf, &newname);
	if (chdir(buf) == 0) {
		rval = rmdir_path(&newname);
		chdir("..");
	}
	free_pathname(&newname);
	return rval;
}

void
separate_pathname(pathname_t *name, char *buf, pathname_t *newname)
{
	char	*slash;

	init_pathname(newname);
	slash = strchr(name->path, '/');
	if (slash == NULL) {
		buf[0] = '\0';
		return;
	}
	*slash = '\0';
	strcpy(buf, name->path);
	*slash = '/';
	append_pathname(newname, slash + 1);
}

void
show_ops(int flag, char *lead_str)
{
	opdesc_t	*p;
	int		f;

	for (f = 0, p = ops; p < ops_end; p++)
		f += p->freq;

	if (f == 0)
		flag = 1;

	for (p = ops; p < ops_end; p++) {
		if (flag != 0 || p->freq > 0) {
			if (lead_str != NULL)
				printf("%s", lead_str);
			printf("%20s %d/%d %s\n",
			p->name, p->freq, f,
			(p->iswrite == 0) ? " " : "write op");
		}
	}
}

int
stat64_path(pathname_t *name, struct stat64 *sbuf)
{
	char		buf[MAXNAMELEN];
	pathname_t	newname;
	int		rval;

	rval = stat64(name->path, sbuf);
	if (rval >= 0 || errno != ENAMETOOLONG)
		return rval;
	separate_pathname(name, buf, &newname);
	if (chdir(buf) == 0) {
		rval = stat64_path(&newname, sbuf);
		chdir("..");
	}
	free_pathname(&newname);
	return rval;
}

int
symlink_path(const char *name1, pathname_t *name)
{
	char		buf[MAXNAMELEN];
	pathname_t	newname;
	int		rval;

	rval = symlink(name1, name->path);
	if (rval >= 0 || errno != ENAMETOOLONG)
		return rval;
	separate_pathname(name, buf, &newname);
	if (chdir(buf) == 0) {
		rval = symlink_path(name1, &newname);
		chdir("..");
	}
	free_pathname(&newname);
	return rval;
}

int
truncate64_path(pathname_t *name, off64_t length)
{
	char		buf[MAXNAMELEN];
	pathname_t	newname;
	int		rval;

	rval = truncate64(name->path, length);
	if (rval >= 0 || errno != ENAMETOOLONG)
		return rval;
	separate_pathname(name, buf, &newname);
	if (chdir(buf) == 0) {
		rval = truncate64_path(&newname, length);
		chdir("..");
	}
	free_pathname(&newname);
	return rval;
}

int
unlink_path(pathname_t *name)
{
	char		buf[MAXNAMELEN];
	pathname_t	newname;
	int		rval;

	rval = unlink(name->path);
	if (rval >= 0 || errno != ENAMETOOLONG)
		return rval;
	separate_pathname(name, buf, &newname);
	if (chdir(buf) == 0) {
		rval = unlink_path(&newname);
		chdir("..");
	}
	free_pathname(&newname);
	return rval;
}

void
usage(void)
{
	printf("Usage: %s -H   or\n", myprog);
	printf("       %s [-d dir][-e errtg][-f op_name=freq][-n nops]\n",
		myprog);
	printf("          [-p nproc][-r len][-s seed][-v][-w][-z][-S]\n");
	printf("where\n");
	printf("   -d dir           specifies the base directory for operations\n");
	printf("   -e errtg         specifies error injection stuff\n");
	printf("   -f op_name=freq  changes the frequency of option name to freq\n");
	printf("                    the valid operation names are:\n");
	show_ops(1, "        ");
	printf("   -n nops          specifies the no. of operations per process\n");
	printf("                    (default is 1)\n");
	printf("   -p nproc         specifies the no. of processes performing operations\n");
	printf("                    (default is 1)\n");
	printf("   -r               specifies random name padding\n");
	printf("   -s seed          specifies the seed for the random generator\n");
	printf("                    (default is random)\n");
	printf("   -v               specifies verbose mode\n");
	printf("   -w               zeros frequencies of non-write operations\n");
	printf("   -z               zeros frequencies of all operations\n");
	printf("   -S               prints the table of operations (omitting\n");
	printf("                    entries with a zero frequency, unless all zero)\n");
	printf("   -H               prints usage and exits\n");
}

void
write_freq(void)
{
	opdesc_t	*p;

	for (p = ops; p < ops_end; p++) {
		if (!p->iswrite)
			p->freq = 0;
	}
}

void
zero_freq(void)
{
	opdesc_t	*p;

	for (p = ops; p < ops_end; p++)
		p->freq = 0;
}

void
allocsp_f(int opno, long r)
{
	int		e;
	pathname_t	f;
	int		fd;
	struct flock64	fl;
	__int64_t	lr;
	off64_t		off;
	struct stat64	stb;
	int		v;

	init_pathname(&f);
	if (!get_fname(FT_REGFILE, r, &f, NULL, NULL, &v)) {
		if (v)
			printf("%d/%d: allocsp - no filename\n", procid, opno);
		free_pathname(&f);
		return;
	}
	fd = open_path(&f, O_RDWR);
	e = fd < 0 ? errno : 0;
	check_cwd();
	if (fd < 0) {
		if (v)
			printf("%d/%d: allocsp - open %s failed %d\n",
				procid, opno, f.path, e);
		free_pathname(&f);
		return;
	}
	if (fstat64(fd, &stb) < 0) {
		if (v)
			printf("%d/%d: allocsp - fstat64 %s failed %d\n",
				procid, opno, f.path, errno);
		free_pathname(&f);
		close(fd);
		return;
	}
	lr = ((__int64_t)random() << 32) + random();
	off = (off64_t)(lr % MIN(stb.st_size + (1024 * 1024), MAXFSIZE));
	off %= maxfsize;
	fl.l_whence = SEEK_SET;
	fl.l_start = off;
	fl.l_len = 0;
	e = fcntl(fd, F_ALLOCSP64, &fl) < 0 ? errno : 0;
	if (v)
		printf("%d/%d: fcntl(F_ALLOCSP64) %s %lld 0 %d\n",
			procid, opno, f.path, off, e);
	free_pathname(&f);
	close(fd);
}

void
attr_remove_f(int opno, long r)
{
	attrlist_ent_t		*aep;
	attrlist_t		*alist;
	char			*aname;
	char			buf[4096];
	attrlist_cursor_t	cursor;
	int			e;
	int			ent;
	pathname_t		f;
	int			total;
	int			v;
	int			which;

	init_pathname(&f);
	if (!get_fname(FT_ANYm, r, &f, NULL, NULL, &v))
		append_pathname(&f, ".");
	total = 0;
	bzero(&cursor, sizeof(cursor));
	do {
		e = attr_list_path(&f, buf, sizeof(buf), ATTR_DONTFOLLOW,
			&cursor);
		check_cwd();
		if (e)
			break;
		alist = (attrlist_t *)buf;
		total += alist->al_count;
	} while (alist->al_more);
	if (total == 0) {
		if (v)
			printf("%d/%d: attr_remove - no attrs for %s\n",
				procid, opno, f.path);
		free_pathname(&f);
		return;
	}
	which = (int)(random() % total);
	bzero(&cursor, sizeof(cursor));
	ent = 0;
	aname = NULL;
	do {
		e = attr_list_path(&f, buf, sizeof(buf), ATTR_DONTFOLLOW,
			&cursor);
		check_cwd();
		if (e)
			break;
		alist = (attrlist_t *)buf;
		if (which < ent + alist->al_count) {
			aep = (attrlist_ent_t *)
				&buf[alist->al_offset[which - ent]];
			aname = aep->a_name;
			break;
		}
		ent += alist->al_count;
	} while (alist->al_more);
	if (aname == NULL) {
		if (v)
			printf(
			"%d/%d: attr_remove - name %d not found at %s\n",	
				procid, opno, which, f.path);
		free_pathname(&f);
		return;
	}
	e = attr_remove_path(&f, aname, ATTR_DONTFOLLOW) < 0 ? errno : 0;
	check_cwd();
	if (v)
		printf("%d/%d: attr_remove %s %s %d\n",
			procid, opno, f.path, aname, e);
	free_pathname(&f);
}

void
attr_set_f(int opno, long r)
{
	char		aname[10];
	char		*aval;
	int		e;
	pathname_t	f;
	int		len;
	static int	lengths[] = { 10, 100, 1000, 10000 };
	int		li;
	int		v;

	init_pathname(&f);
	if (!get_fname(FT_ANYm, r, &f, NULL, NULL, &v))
		append_pathname(&f, ".");
	sprintf(aname, "a%x", nameseq++);
	li = (int)(random() % (sizeof(lengths) / sizeof(lengths[0])));
	len = (int)(random() % lengths[li]);
	if (len == 0)
		len = 1;
	aval = malloc(len);
	memset(aval, nameseq & 0xff, len);
	e = attr_set_path(&f, aname, aval, len, ATTR_DONTFOLLOW) < 0 ?
		errno : 0;
	check_cwd();
	free(aval);
	if (v)
		printf("%d/%d: attr_set %s %s %d\n", procid, opno, f.path,
			aname, e);
	free_pathname(&f);
}

void
bulkstat_f(int opno, long r)
{
	int		count;
	int		fd;
	ino64_t		last;
	int		nent;
	xfs_bstat_t	*t;
	__int64_t	total;

	last = 0;
	nent = (r % 999) + 2;
	t = malloc(nent * sizeof(*t));
	fd = open(".", O_RDONLY);
	total = 0;
	while (syssgi(SGI_FS_BULKSTAT, fd, &last, nent, t, &count) == 0 &&
	       count > 0)
		total += count;
	free(t);
	if (verbose)
		printf("%d/%d: bulkstat nent %d total %lld\n",
			procid, opno, nent, total);
	close(fd);
}

void
bulkstat1_f(int opno, long r)
{
	int		e;
	pathname_t	f;
	int		fd;
	int		good;
	ino64_t		ino;
	struct stat64	s;
	xfs_bstat_t	t;
	int		v;

	good = random() & 1;
	if (good) {
		init_pathname(&f);
		if (!get_fname(FT_ANYm, r, &f, NULL, NULL, &v))
			append_pathname(&f, ".");
		ino = stat64_path(&f, &s) < 0 ? (ino64_t)r : s.st_ino;
		check_cwd();
		free_pathname(&f);
	} else {
		ino = (ino64_t)r;
		v = verbose;
	}
	fd = open(".", O_RDONLY);
	e = syssgi(SGI_FS_BULKSTAT_SINGLE, fd, &ino, &t) < 0 ? errno : 0;
	if (v)
		printf("%d/%d: bulkstat1 ino %lld %d\n", procid, opno, ino, e);
	close(fd);
}

void
chown_f(int opno, long r)
{
	int		e;
	pathname_t	f;
	int		nbits;
	uid_t		u;
	int		v;

	init_pathname(&f);
	if (!get_fname(FT_ANYm, r, &f, NULL, NULL, &v))
		append_pathname(&f, ".");
	u = (uid_t)random();
	nbits = (int)(random() % 32);
	u &= (1 << nbits) - 1;
	e = lchown_path(&f, u, -1) < 0 ? errno : 0;
	check_cwd();
	if (v)
		printf("%d/%d: chown %s %d %d\n", procid, opno, f.path, u, e);
	free_pathname(&f);
}

void
creat_f(int opno, long r)
{
	struct fsxattr	a;
	int		e;
	int		e1;
	int		extsize;
	pathname_t	f;
	int		fd;
	fent_t		*fep;
	int		id;
	int		parid;
	int		type;
	int		v;
	int		v1;

	if (!get_fname(FT_DIRm, r, NULL, NULL, &fep, &v1))
		parid = -1;
	else
		parid = fep->id;
	init_pathname(&f);
	type = rtpct ? ((random() % 100) > rtpct ? FT_REG : FT_RTF) : FT_REG;
	if (type == FT_RTF)
		extsize = (random() % 10) + 1;
	else
		extsize = 0;
	e = generate_fname(fep, type, &f, &id, &v);
	v |= v1;
	if (!e) {
		if (v) {
			fent_to_name(&f, &flist[FT_DIR], fep);
			printf("%d/%d: creat - no filename from %s\n",
				procid, opno, f.path);
		}
		free_pathname(&f);
		return;
	}
	fd = creat_path(&f, 0666);
	e = fd < 0 ? errno : 0;
	e1 = 0;
	check_cwd();
	if (fd >= 0) {
		if (extsize && fcntl(fd, F_FSGETXATTR, &a) >= 0) {
			a.fsx_xflags |= XFS_XFLAG_REALTIME;
			a.fsx_extsize =
				geom.rtextsize * geom.blocksize * extsize;
			if (fcntl(fd, F_FSSETXATTR, &a) < 0)
				e1 = errno;
		}
		add_to_flist(type, id, parid);
		close(fd);
	}
	if (v)
		printf("%d/%d: creat %s x:%d %d %d\n", procid, opno, f.path,
			extsize ? a.fsx_extsize : 0, e, e1);
	free_pathname(&f);
}

void
dread_f(int opno, long r)
{
	__int64_t	align;
	char		*buf;
	struct dioattr	diob;
	int		e;
	pathname_t	f;
	int		fd;
	size_t		len;
	__int64_t	lr;
	off64_t		off;
	struct stat64	stb;
	int		v;

	init_pathname(&f);
	if (!get_fname(FT_REGFILE, r, &f, NULL, NULL, &v)) {
		if (v)
			printf("%d/%d: dread - no filename\n", procid, opno);
		free_pathname(&f);
		return;
	}
	fd = open_path(&f, O_RDONLY|O_DIRECT);
	e = fd < 0 ? errno : 0;
	check_cwd();
	if (fd < 0) {
		if (v)
			printf("%d/%d: dread - open %s failed %d\n",
				procid, opno, f.path, e);
		free_pathname(&f);
		return;
	}
	if (fstat64(fd, &stb) < 0) {
		if (v)
			printf("%d/%d: dread - fstat64 %s failed %d\n",
				procid, opno, f.path, errno);
		free_pathname(&f);
		close(fd);
		return;
	}
	if (stb.st_size == 0) {
		if (v)
			printf("%d/%d: dread - %s zero size\n", procid, opno,
				f.path);
		free_pathname(&f);
		close(fd);
		return;
	}
	if (fcntl(fd, F_DIOINFO, &diob) < 0) {
		if (v)
			printf(
			"%d/%d: dread - fcntl(fd, F_DIOINFO) %s failed %d\n",
				procid, opno, f.path, errno);
		free_pathname(&f);
		close(fd);
		return;
	}
	align = (__int64_t)diob.d_miniosz;
	lr = ((__int64_t)random() << 32) + random();
	off = (off64_t)(lr % stb.st_size);
	off -= (off % align);
	lseek64(fd, off, SEEK_SET);
	len = (random() % (getpagesize() * 32)) + 1;
	len -= (len % align);
	if (len <= 0)
		len = align;
	else if (len > diob.d_maxiosz) 
		len = diob.d_maxiosz;
	buf = memalign(diob.d_mem, len);
	e = read(fd, buf, len) < 0 ? errno : 0;
	free(buf);
	if (v)
		printf("%d/%d: dread %s [%lld,%d] %d\n",
			procid, opno, f.path, off, len, e);
	free_pathname(&f);
	close(fd);
}

void
dwrite_f(int opno, long r)
{
	__int64_t	align;
	char		*buf;
	struct dioattr	diob;
	int		e;
	pathname_t	f;
	int		fd;
	size_t		len;
	__int64_t	lr;
	off64_t		off;
	struct stat64	stb;
	int		v;

	init_pathname(&f);
	if (!get_fname(FT_REGFILE, r, &f, NULL, NULL, &v)) {
		if (v)
			printf("%d/%d: dwrite - no filename\n", procid, opno);
		free_pathname(&f);
		return;
	}
	fd = open_path(&f, O_WRONLY|O_DIRECT);
	e = fd < 0 ? errno : 0;
	check_cwd();
	if (fd < 0) {
		if (v)
			printf("%d/%d: dwrite - open %s failed %d\n",
				procid, opno, f.path, e);
		free_pathname(&f);
		return;
	}
	if (fstat64(fd, &stb) < 0) {
		if (v)
			printf("%d/%d: dwrite - fstat64 %s failed %d\n",
				procid, opno, f.path, errno);
		free_pathname(&f);
		close(fd);
		return;
	}
	if (fcntl(fd, F_DIOINFO, &diob) < 0) {
		if (v)
			printf(
			"%d/%d: dwrite - fcntl(fd, F_DIOINFO) %s failed %d\n",
				procid, opno, f.path, errno);
		free_pathname(&f);
		close(fd);
		return;
	}
	align = (__int64_t)diob.d_miniosz;
	lr = ((__int64_t)random() << 32) + random();
	off = (off64_t)(lr % MIN(stb.st_size + (1024 * 1024), MAXFSIZE));
	off -= (off % align);
	lseek64(fd, off, SEEK_SET);
	len = (random() % (getpagesize() * 32)) + 1;
	len -= (len % align);
	if (len <= 0)
		len = align;
	else if (len > diob.d_maxiosz) 
		len = diob.d_maxiosz;
	buf = memalign(diob.d_mem, len);
	off %= maxfsize;
	lseek64(fd, off, SEEK_SET);
	memset(buf, nameseq & 0xff, len);
	e = write(fd, buf, len) < 0 ? errno : 0;
	free(buf);
	if (v)
		printf("%d/%d: dwrite %s [%lld,%d] %d\n",
			procid, opno, f.path, off, len, e);
	free_pathname(&f);
	close(fd);
}

void
fdatasync_f(int opno, long r)
{
	int		e;
	pathname_t	f;
	int		fd;
	int		v;

	init_pathname(&f);
	if (!get_fname(FT_REGFILE, r, &f, NULL, NULL, &v)) {
		if (v)
			printf("%d/%d: fdatasync - no filename\n",
				procid, opno);
		free_pathname(&f);
		return;
	}
	fd = open_path(&f, O_WRONLY);
	e = fd < 0 ? errno : 0;
	check_cwd();
	if (fd < 0) {
		if (v)
			printf("%d/%d: fdatasync - open %s failed %d\n",
				procid, opno, f.path, e);
		free_pathname(&f);
		return;
	}
	e = fdatasync(fd) < 0 ? errno : 0;
	if (v)
		printf("%d/%d: fdatasync %s %d\n", procid, opno, f.path, e);
	free_pathname(&f);
	close(fd);
}

void
freesp_f(int opno, long r)
{
	int		e;
	pathname_t	f;
	int		fd;
	struct flock64	fl;
	__int64_t	lr;
	off64_t		off;
	struct stat64	stb;
	int		v;

	init_pathname(&f);
	if (!get_fname(FT_REGFILE, r, &f, NULL, NULL, &v)) {
		if (v)
			printf("%d/%d: freesp - no filename\n", procid, opno);
		free_pathname(&f);
		return;
	}
	fd = open_path(&f, O_RDWR);
	e = fd < 0 ? errno : 0;
	check_cwd();
	if (fd < 0) {
		if (v)
			printf("%d/%d: freesp - open %s failed %d\n",
				procid, opno, f.path, e);
		free_pathname(&f);
		return;
	}
	if (fstat64(fd, &stb) < 0) {
		if (v)
			printf("%d/%d: freesp - fstat64 %s failed %d\n",
				procid, opno, f.path, errno);
		free_pathname(&f);
		close(fd);
		return;
	}
	lr = ((__int64_t)random() << 32) + random();
	off = (off64_t)(lr % MIN(stb.st_size + (1024 * 1024), MAXFSIZE));
	off %= maxfsize;
	fl.l_whence = SEEK_SET;
	fl.l_start = off;
	fl.l_len = 0;
	e = fcntl(fd, F_FREESP64, &fl) < 0 ? errno : 0;
	if (v)
		printf("%d/%d: fcntl(F_FREESP64) %s %lld 0 %d\n",
			procid, opno, f.path, off, e);
	free_pathname(&f);
	close(fd);
}

void
fsync_f(int opno, long r)
{
	int		e;
	pathname_t	f;
	int		fd;
	int		v;

	init_pathname(&f);
	if (!get_fname(FT_REGFILE, r, &f, NULL, NULL, &v)) {
		if (v)
			printf("%d/%d: fsync - no filename\n", procid, opno);
		free_pathname(&f);
		return;
	}
	fd = open_path(&f, O_WRONLY);
	e = fd < 0 ? errno : 0;
	check_cwd();
	if (fd < 0) {
		if (v)
			printf("%d/%d: fsync - open %s failed %d\n",
				procid, opno, f.path, e);
		free_pathname(&f);
		return;
	}
	e = fsync(fd) < 0 ? errno : 0;
	if (v)
		printf("%d/%d: fsync %s %d\n", procid, opno, f.path, e);
	free_pathname(&f);
	close(fd);
}

void
getdents_f(int opno, long r)
{
	DIR		*dir;
	pathname_t	f;
	int		v;

	init_pathname(&f);
	if (!get_fname(FT_DIRm, r, &f, NULL, NULL, &v))
		append_pathname(&f, ".");
	dir = opendir_path(&f);
	check_cwd();
	if (dir == NULL) {
		if (v)
			printf("%d/%d: getdents - can't open %s\n",
				procid, opno, f.path);
		free_pathname(&f);
		return;
	}
	while (readdir64(dir) != NULL)
		continue;
	if (v)
		printf("%d/%d: getdents %s 0\n", procid, opno, f.path);
	free_pathname(&f);
	closedir(dir);
}

void
link_f(int opno, long r)
{
	int		e;
	pathname_t	f;
	fent_t		*fep;
	flist_t		*flp;
	int		id;
	pathname_t	l;
	int		parid;
	int		v;
	int		v1;

	init_pathname(&f);
	if (!get_fname(FT_NOTDIR, r, &f, &flp, NULL, &v1)) {
		if (v1)
			printf("%d/%d: link - no file\n", procid, opno);
		free_pathname(&f);
		return;
	}
	if (!get_fname(FT_DIRm, random(), NULL, NULL, &fep, &v))
		parid = -1;
	else
		parid = fep->id;
	v |= v1;
	init_pathname(&l);
	e = generate_fname(fep, flp - flist, &l, &id, &v1);
	v |= v1;
	if (!e) {
		if (v) {
			fent_to_name(&l, &flist[FT_DIR], fep);
			printf("%d/%d: link - no filename from %s\n",
				procid, opno, l.path);
		}
		free_pathname(&l);
		free_pathname(&f);
		return;
	}
	e = link_path(&f, &l) < 0 ? errno : 0;
	check_cwd();
	if (e == 0)
		add_to_flist(flp - flist, id, parid);
	if (v)
		printf("%d/%d: link %s %s %d\n", procid, opno, f.path, l.path,
			e);
	free_pathname(&l);
	free_pathname(&f);
}

void
mkdir_f(int opno, long r)
{
	int		e;
	pathname_t	f;
	fent_t		*fep;
	int		id;
	int		parid;
	int		v;
	int		v1;

	if (!get_fname(FT_DIRm, r, NULL, NULL, &fep, &v))
		parid = -1;
	else
		parid = fep->id;
	init_pathname(&f);
	e = generate_fname(fep, FT_DIR, &f, &id, &v1);
	v |= v1;
	if (!e) {
		if (v) {
			fent_to_name(&f, &flist[FT_DIR], fep);
			printf("%d/%d: mkdir - no filename from %s\n",
				procid, opno, f.path);
		}
		free_pathname(&f);
		return;
	}
	e = mkdir_path(&f, 0777) < 0 ? errno : 0;
	check_cwd();
	if (e == 0)
		add_to_flist(FT_DIR, id, parid);
	if (v)
		printf("%d/%d: mkdir %s %d\n", procid, opno, f.path, e);
	free_pathname(&f);
}

void
mknod_f(int opno, long r)
{
	int		e;
	pathname_t	f;
	fent_t		*fep;
	int		id;
	int		parid;
	int		v;
	int		v1;

	if (!get_fname(FT_DIRm, r, NULL, NULL, &fep, &v))
		parid = -1;
	else
		parid = fep->id;
	init_pathname(&f);
	e = generate_fname(fep, FT_DEV, &f, &id, &v1);
	v |= v1;
	if (!e) {
		if (v) {
			fent_to_name(&f, &flist[FT_DIR], fep);
			printf("%d/%d: mknod - no filename from %s\n",
				procid, opno, f.path);
		}
		free_pathname(&f);
		return;
	}
	e = mknod_path(&f, S_IFCHR|0444, 0) < 0 ? errno : 0;
	check_cwd();
	if (e == 0)
		add_to_flist(FT_DEV, id, parid);
	if (v)
		printf("%d/%d: mknod %s %d\n", procid, opno, f.path, e);
	free_pathname(&f);
}

void
read_f(int opno, long r)
{
	char		*buf;
	int		e;
	pathname_t	f;
	int		fd;
	size_t		len;
	__int64_t	lr;
	off64_t		off;
	struct stat64	stb;
	int		v;

	init_pathname(&f);
	if (!get_fname(FT_REGFILE, r, &f, NULL, NULL, &v)) {
		if (v)
			printf("%d/%d: read - no filename\n", procid, opno);
		free_pathname(&f);
		return;
	}
	fd = open_path(&f, O_RDONLY);
	e = fd < 0 ? errno : 0;
	check_cwd();
	if (fd < 0) {
		if (v)
			printf("%d/%d: read - open %s failed %d\n",
				procid, opno, f.path, e);
		free_pathname(&f);
		return;
	}
	if (fstat64(fd, &stb) < 0) {
		if (v)
			printf("%d/%d: read - fstat64 %s failed %d\n",
				procid, opno, f.path, errno);
		free_pathname(&f);
		close(fd);
		return;
	}
	if (stb.st_size == 0) {
		if (v)
			printf("%d/%d: read - %s zero size\n", procid, opno,
				f.path);
		free_pathname(&f);
		close(fd);
		return;
	}
	lr = ((__int64_t)random() << 32) + random();
	off = (off64_t)(lr % stb.st_size);
	lseek64(fd, off, SEEK_SET);
	len = (random() % (getpagesize() * 32)) + 1;
	buf = malloc(len);
	e = read(fd, buf, len) < 0 ? errno : 0;
	free(buf);
	if (v)
		printf("%d/%d: read %s [%lld,%d] %d\n",
			procid, opno, f.path, off, len, e);
	free_pathname(&f);
	close(fd);
}

void
readlink_f(int opno, long r)
{
	char		buf[MAXPATHLEN];
	int		e;
	pathname_t	f;
	int		v;

	init_pathname(&f);
	if (!get_fname(FT_SYMm, r, &f, NULL, NULL, &v)) {
		if (v)
			printf("%d/%d: readlink - no filename\n", procid, opno);
		free_pathname(&f);
		return;
	}
	e = readlink_path(&f, buf, MAXPATHLEN) < 0 ? errno : 0;
	check_cwd();
	if (v)
		printf("%d/%d: readlink %s %d\n", procid, opno, f.path, e);
	free_pathname(&f);
}

void
rename_f(int opno, long r)
{
	fent_t		*dfep;
	int		e;
	pathname_t	f;
	fent_t		*fep;
	flist_t		*flp;
	int		id;
	pathname_t	newf;
	int		oldid;
	int		parid;
	int		v;
	int		v1;

	init_pathname(&f);
	if (!get_fname(FT_ANYm, r, &f, &flp, &fep, &v1)) {
		if (v1)
			printf("%d/%d: rename - no filename\n", procid, opno);
		free_pathname(&f);
		return;
	}
	if (!get_fname(FT_DIRm, random(), NULL, NULL, &dfep, &v))
		parid = -1;
	else
		parid = dfep->id;
	v |= v1;
	init_pathname(&newf);
	e = generate_fname(dfep, flp - flist, &newf, &id, &v1);
	v |= v1;
	if (!e) {
		if (v) {
			fent_to_name(&f, &flist[FT_DIR], dfep);
			printf("%d/%d: rename - no filename from %s\n",
				procid, opno, f.path);
		}
		free_pathname(&newf);
		free_pathname(&f);
		return;
	}
	e = rename_path(&f, &newf) < 0 ? errno : 0;
	check_cwd();
	if (e == 0) {
		if (flp - flist == FT_DIR) {
			oldid = fep->id;
			fix_parent(oldid, id);
		}
		del_from_flist(flp - flist, fep - flp->fents);
		add_to_flist(flp - flist, id, parid);
	}
	if (v)
		printf("%d/%d: rename %s to %s %d\n", procid, opno, f.path,
			newf.path, e);
	free_pathname(&newf);
	free_pathname(&f);
}

void
resvsp_f(int opno, long r)
{
	int		e;
	pathname_t	f;
	int		fd;
	struct flock64	fl;
	__int64_t	lr;
	off64_t		off;
	struct stat64	stb;
	int		v;

	init_pathname(&f);
	if (!get_fname(FT_REGFILE, r, &f, NULL, NULL, &v)) {
		if (v)
			printf("%d/%d: resvsp - no filename\n", procid, opno);
		free_pathname(&f);
		return;
	}
	fd = open_path(&f, O_RDWR);
	e = fd < 0 ? errno : 0;
	check_cwd();
	if (fd < 0) {
		if (v)
			printf("%d/%d: resvsp - open %s failed %d\n",
				procid, opno, f.path, e);
		free_pathname(&f);
		return;
	}
	if (fstat64(fd, &stb) < 0) {
		if (v)
			printf("%d/%d: resvsp - fstat64 %s failed %d\n",
				procid, opno, f.path, errno);
		free_pathname(&f);
		close(fd);
		return;
	}
	lr = ((__int64_t)random() << 32) + random();
	off = (off64_t)(lr % MIN(stb.st_size + (1024 * 1024), MAXFSIZE));
	off %= maxfsize;
	fl.l_whence = SEEK_SET;
	fl.l_start = off;
	fl.l_len = (off64_t)(random() % (1024 * 1024));
	e = fcntl(fd, F_RESVSP64, &fl) < 0 ? errno : 0;
	if (v)
		printf("%d/%d: fcntl(F_RESVSP64) %s %lld %lld %d\n",
			procid, opno, f.path, off, fl.l_len, e);
	free_pathname(&f);
	close(fd);
}

void
rmdir_f(int opno, long r)
{
	int		e;
	pathname_t	f;
	fent_t		*fep;
	int		v;

	init_pathname(&f);
	if (!get_fname(FT_DIRm, r, &f, NULL, &fep, &v)) {
		if (v)
			printf("%d/%d: rmdir - no directory\n", procid, opno);
		free_pathname(&f);
		return;
	}
	e = rmdir_path(&f) < 0 ? errno : 0;
	check_cwd();
	if (e == 0)
		del_from_flist(FT_DIR, fep - flist[FT_DIR].fents);
	if (v)
		printf("%d/%d: rmdir %s %d\n", procid, opno, f.path, e);
	free_pathname(&f);
}

void
stat_f(int opno, long r)
{
	int		e;
	pathname_t	f;
	struct stat64	stb;
	int		v;

	init_pathname(&f);
	if (!get_fname(FT_ANYm, r, &f, NULL, NULL, &v))
		append_pathname(&f, ".");
	e = lstat64_path(&f, &stb) < 0 ? errno : 0;
	check_cwd();
	if (v)
		printf("%d/%d: stat %s %d\n", procid, opno, f.path, e);
	free_pathname(&f);
}

void
symlink_f(int opno, long r)
{
	int		e;
	pathname_t	f;
	fent_t		*fep;
	int		i;
	int		id;
	int		len;
	int		parid;
	int		v;
	int		v1;
	char		*val;

	if (!get_fname(FT_DIRm, r, NULL, NULL, &fep, &v))
		parid = -1;
	else
		parid = fep->id;
	init_pathname(&f);
	e = generate_fname(fep, FT_SYM, &f, &id, &v1);
	v |= v1;
	if (!e) {
		if (v) {
			fent_to_name(&f, &flist[FT_DIR], fep);
			printf("%d/%d: symlink - no filename from %s\n",
				procid, opno, f.path);
		}
		free_pathname(&f);
		return;
	}
	len = (int)(random() % MAXPATHLEN);
	val = malloc(len + 1);
	if (len)
		memset(val, 'x', len);
	val[len] = '\0';
	for (i = 10; i < len - 1; i += 10)
		val[i] = '/';
	e = symlink_path(val, &f) < 0 ? errno : 0;
	check_cwd();
	if (e == 0)
		add_to_flist(FT_SYM, id, parid);
	free(val);
	if (v)
		printf("%d/%d: symlink %s %d\n", procid, opno, f.path, e);
	free_pathname(&f);
}

/* ARGSUSED */
void
sync_f(int opno, long r)
{
	sync();
	if (verbose)
		printf("%d/%d: sync\n", procid, opno);
}

void
truncate_f(int opno, long r)
{
	int		e;
	pathname_t	f;
	__int64_t	lr;
	off64_t		off;
	struct stat64	stb;
	int		v;

	init_pathname(&f);
	if (!get_fname(FT_REGFILE, r, &f, NULL, NULL, &v)) {
		if (v)
			printf("%d/%d: truncate - no filename\n", procid, opno);
		free_pathname(&f);
		return;
	}
	e = stat64_path(&f, &stb) < 0 ? errno : 0;
	check_cwd();
	if (e > 0) {
		if (v)
			printf("%d/%d: truncate - stat64 %s failed %d\n",
				procid, opno, f.path, e);
		free_pathname(&f);
		return;
	}
	lr = ((__int64_t)random() << 32) + random();
	off = (off64_t)(lr % MIN(stb.st_size + (1024 * 1024), MAXFSIZE));
	off %= maxfsize;
	e = truncate64_path(&f, off) < 0 ? errno : 0;
	check_cwd();
	if (v)
		printf("%d/%d: truncate %s %lld %d\n", procid, opno, f.path,
			off, e);
	free_pathname(&f);
}

void
unlink_f(int opno, long r)
{
	int		e;
	pathname_t	f;
	fent_t		*fep;
	flist_t		*flp;
	int		v;

	init_pathname(&f);
	if (!get_fname(FT_NOTDIR, r, &f, &flp, &fep, &v)) {
		if (v)
			printf("%d/%d: unlink - no file\n", procid, opno);
		free_pathname(&f);
		return;
	}
	e = unlink_path(&f) < 0 ? errno : 0;
	check_cwd();
	if (e == 0)
		del_from_flist(flp - flist, fep - flp->fents);
	if (v)
		printf("%d/%d: unlink %s %d\n", procid, opno, f.path, e);
	free_pathname(&f);
}

void
unresvsp_f(int opno, long r)
{
	int		e;
	pathname_t	f;
	int		fd;
	struct flock64	fl;
	__int64_t	lr;
	off64_t		off;
	struct stat64	stb;
	int		v;

	init_pathname(&f);
	if (!get_fname(FT_REGFILE, r, &f, NULL, NULL, &v)) {
		if (v)
			printf("%d/%d: unresvsp - no filename\n", procid, opno);
		free_pathname(&f);
		return;
	}
	fd = open_path(&f, O_RDWR);
	e = fd < 0 ? errno : 0;
	check_cwd();
	if (fd < 0) {
		if (v)
			printf("%d/%d: unresvsp - open %s failed %d\n",
				procid, opno, f.path, e);
		free_pathname(&f);
		return;
	}
	if (fstat64(fd, &stb) < 0) {
		if (v)
			printf("%d/%d: unresvsp - fstat64 %s failed %d\n",
				procid, opno, f.path, errno);
		free_pathname(&f);
		close(fd);
		return;
	}
	lr = ((__int64_t)random() << 32) + random();
	off = (off64_t)(lr % MIN(stb.st_size + (1024 * 1024), MAXFSIZE));
	off %= maxfsize;
	fl.l_whence = SEEK_SET;
	fl.l_start = off;
	fl.l_len = (off64_t)(random() % (1 << 20));
	e = fcntl(fd, F_UNRESVSP64, &fl) < 0 ? errno : 0;
	if (v)
		printf("%d/%d: fcntl(F_UNRESVSP64) %s %lld %lld %d\n",
			procid, opno, f.path, off, fl.l_len, e);
	free_pathname(&f);
	close(fd);
}

void
write_f(int opno, long r)
{
	char		*buf;
	int		e;
	pathname_t	f;
	int		fd;
	size_t		len;
	__int64_t	lr;
	off64_t		off;
	struct stat64	stb;
	int		v;

	init_pathname(&f);
	if (!get_fname(FT_REGm, r, &f, NULL, NULL, &v)) {
		if (v)
			printf("%d/%d: write - no filename\n", procid, opno);
		free_pathname(&f);
		return;
	}
	fd = open_path(&f, O_WRONLY);
	e = fd < 0 ? errno : 0;
	check_cwd();
	if (fd < 0) {
		if (v)
			printf("%d/%d: write - open %s failed %d\n",
				procid, opno, f.path, e);
		free_pathname(&f);
		return;
	}
	if (fstat64(fd, &stb) < 0) {
		if (v)
			printf("%d/%d: write - fstat64 %s failed %d\n",
				procid, opno, f.path, errno);
		free_pathname(&f);
		close(fd);
		return;
	}
	lr = ((__int64_t)random() << 32) + random();
	off = (off64_t)(lr % MIN(stb.st_size + (1024 * 1024), MAXFSIZE));
	off %= maxfsize;
	lseek64(fd, off, SEEK_SET);
	len = (random() % (getpagesize() * 32)) + 1;
	buf = malloc(len);
	memset(buf, nameseq & 0xff, len);
	e = write(fd, buf, len) < 0 ? errno : 0;
	free(buf);
	if (v)
		printf("%d/%d: write %s [%lld,%d] %d\n",
			procid, opno, f.path, off, len, e);
	free_pathname(&f);
	close(fd);
}
