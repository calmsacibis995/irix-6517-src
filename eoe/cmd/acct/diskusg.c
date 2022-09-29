/*	Copyright (c) 1984 AT&T	*/
/*	All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident	"$Revision: 1.15 $"

#include <stdio.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/sysmacros.h>
#include <sys/buf.h>
#include <sys/syssgi.h>
#include <sys/fs/efs.h>
#include <sys/fs/xfs_itable.h>
#include <pwd.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <diskinfo.h>
#include "acctdef.h"

#define		UNUSED		-1
#define		FAIL		-1
#define		MAXNAME		8
#define		SUCCEED		0
#define		TRUE		1
#define		FALSE		0

union sblock {
	struct	efs fs ;
	char	data[BBTOB(BTOBB(sizeof(struct efs)))] ;
} sblock ;

struct	efs *fs ;			/* pointer to sblock.fs */

struct	efs_dinode	*dinode;

int	VERBOSE = 0;
FILE	*ufd = 0;
extern int _pw_stayopen;
unsigned  nfiles;
ino_t	ino ;

struct dacct  {
	long long	usage;		/* daveh note 2 Jul 91: now BBs */
	int		uid;
	char		name[MAXNAME+1];
};
struct dacct *userlist;
int 	maxusers;

char	**ignlist;
int 	maxign;
int	igncnt = {0};			/* why makes it part of .data? */

char	*cmd;


void adduser(FILE *);
void bread(int, unsigned, union sblock *, int);
int  count(void);
struct efs_dinode *efs_iget(int, ino_t);
void error(void);
unsigned hash(unsigned);
void hashinit(void);
void ignore(char *);
void ilist(char *, int);
void output(void);
void setup(char *);
char *skip(char *);
int  todo(char *);
int  xfs_count(xfs_bstat_t *);
void xfs_ilist(char *file, int fd);

main(int argc, char **argv)
{
	extern	int	optind;
	extern	char	*optarg;
	register c;
	register FILE	*fd;
	register	rfd;
	struct	stat	sb;
	int	sflg = {FALSE};
	char	*pfile = {"/etc/passwd"};
	int	errfl = {FALSE};
	char 	*str;

	/* allocate memory for userlist */
	str = getenv(ACCT_MAXUSERS);
	if (str == NULL) 
		maxusers = MAXUSERS;
	else {
		maxusers = strtol(str, (char **)0, 0);
		if (errno == ERANGE || maxusers < MAXUSERS)
			maxusers = MAXUSERS;
	}
	userlist = (struct dacct *)calloc(maxusers, sizeof(struct dacct));
	if (userlist == (struct dacct *)NULL) {
		fprintf(stderr, "%s: Cannot allocate memory\n", argv[0]);
		exit(5);
	}
	/* allocate memory for ignore list */
	str = getenv(ACCT_MAXIGN);
	if (str == NULL) 
		maxign = MAXIGN;
	else {
		maxign = strtol(str, (char **)0, 0);
		if (errno == ERANGE || maxign < MAXIGN)
			maxign = MAXIGN;
	}
	ignlist = (char **)calloc(maxign, sizeof(char *));
	if (ignlist == (char **)NULL) {
		fprintf(stderr, "%s: Cannot allocate memory\n", argv[0]);
		exit(5);
	}

	cmd = argv[0];
	while((c = getopt(argc, argv, "vu:p:si:")) != EOF) switch(c) {
	case 's':
		sflg = TRUE;
		break;
	case 'v':
		VERBOSE = 1;
		break;
	case 'i':
		ignore(optarg);
		break;
	case 'u':
		ufd = fopen(optarg, "a");
		break;
	case '?':
		errfl++;
		break;
	}
	if(errfl) {
		fprintf(stderr, "Usage: %s [-sv] [-u file] [-i ignlist] [file ...]\n", cmd);
		exit(10);
	}

	hashinit();
	if(sflg == TRUE) {
		if(optind == argc){
			adduser(stdin);
		} else {
			for( ; optind < argc; optind++) {
				if( (fd = fopen(argv[optind], "r")) == NULL) {
					fprintf(stderr, "%s: Cannot open %s\n", cmd, argv[optind]);
					continue;
				}
				adduser(fd);
				fclose(fd);
			}
		}
	}
	else {
		setup(pfile);
		for( ; optind < argc; optind++) {
			if( (rfd = open(argv[optind], O_RDONLY)) < 0) {
				fprintf(stderr, "%s: Cannot open %s\n", cmd, argv[optind]);
				continue;
			}
			if(fstat(rfd, &sb) >= 0){
				if ( (sb.st_mode & S_IFMT) == S_IFCHR ||
				     (sb.st_mode & S_IFMT) == S_IFBLK ) {
					ilist(argv[optind], rfd);
				} else {
					fprintf(stderr, "%s: %s is not a special file -- ignored\n", cmd, argv[optind]);
				}
			} else {
				fprintf(stderr, "%s: Cannot stat %s\n", cmd, argv[optind]);
			}
			close(rfd);
		}
	}
	output();
	exit(0);
}

void
adduser(FILE *fd)
{
	int		usrid, index;
	long long	blcks;
	char		login[MAXNAME+10];

	while(fscanf(fd, "%d %s %lld\n", &usrid, login, &blcks) == 3) {
		if( (index = hash(usrid)) == FAIL) return;
		if(userlist[index].uid == UNUSED) {
			userlist[index].uid = usrid;
			strncpy(userlist[index].name, login, MAXNAME);
		}
		userlist[index].usage += blcks;
	}
}

void
ilist(char *file, int fd)
{
	register dev_t	dev;
	register i, j;
	char	*rawpath;
	int 	rawfd = 0;

	struct efs_dinode *efs_iget() ;

	if (fd < 0 ) {
		return; 
	}

/*	sync(); daveh 2/7/91: irrelevent due to mounted fs -> char dev hack */

	/* Read in super-block of filesystem */
	bread(fd, 1, &sblock, sizeof(sblock) );

	fs = &sblock.fs ;

/* daveh 2 7 91: a *little* sanity chacking, for God's sake... */

	if (!IS_EFS_MAGIC(fs->fs_magic)) {
		xfs_ilist(file, fd);
		return;
	}
	fs->fs_ipcg = EFS_COMPUTE_IPCG(fs);

	/* Check for filesystem names to ignore */
	if(!todo(fs->fs_fname))
		return;

	if ((rawpath = findrawpath(file)) == NULL)
		return;

	if ((rawfd = open(rawpath, O_RDONLY)) < 0)
		return;

	nfiles = (fs->fs_cgisize * EFS_INOPBB) * fs->fs_ncg ;

	/* Start at physical block 2, inode list */
	for (ino = 2; ino < nfiles; ino++ ) {
		dinode = efs_iget(rawfd, ino ) ;
		if (dinode->di_mode & S_IFMT) {
			if(count() == FAIL) {
				if(VERBOSE)
					fprintf(stderr,"BAD UID: file system = %s, inode = %u, uid = %u\n",
						file, ino, dinode->di_uid);
				if(ufd)
					fprintf(ufd, "%s %u %u\n", file, ino, dinode->di_uid);
			}
		}
	}
	return;
}

void
ignore(char *str)
{
	char	*skip();

	for( ; *str && igncnt < maxign; str = skip(str), igncnt++)
		ignlist[igncnt] = str;
	if(igncnt == maxign) {
		fprintf(stderr, "%s: ignore list overflow. Increase the size of the environment variable ACCT_MAXIGN\n", cmd);
	}
}

	/* daveh 2/7/91: now uses readb for fs > 2 gig */

struct efs_dinode *
efs_iget(int rawfd, ino_t ino)
{
	int inobb;
	static struct efs_dinode inos[EFS_INOPBB];

	if ( (ino % EFS_INOPBB) != 0 ) {
		return &inos[ino % EFS_INOPBB];
	}

	inobb = EFS_ITOBB(fs, ino);
	if (readb(rawfd, (char *) &inos[0], inobb, 1) != 1)
		error();
	return &inos[ino % EFS_INOPBB];
}

void
bread(int fd, unsigned bno, union sblock *buf, int cnt)
{
	int debug ;

	lseek(fd, BBTOB(bno), 0);
	if ((debug=read(fd, buf, cnt) ) != cnt)
	{
		fprintf(stderr, "read error %u %d %d\n", bno,debug,cnt);
		exit(1);
	}
}

int
count(void)
{
	int index;

	if ( dinode->di_nlink == 0 || dinode->di_mode == 0 )
		return(SUCCEED);
	if( (index = hash(dinode->di_uid)) == FAIL || userlist[index].uid == UNUSED )
		return (FAIL);
	userlist[index].usage += ((dinode->di_size + BBSIZE - 1) / BBSIZE);
	return (SUCCEED);
}

void
output(void)
{
	int index;

	for (index=0; index < maxusers ; index++)
		if ( userlist[index].uid != UNUSED && userlist[index].usage != 0 )
			printf("%u	%s	%lld\n",
			    userlist[index].uid,
			    userlist[index].name,
			    userlist[index].usage);
}

unsigned
hash(unsigned j)
{
	register unsigned start;
	register unsigned circle;
	circle = start = j % maxusers;
	do
	{
		if ( userlist[circle].uid == j || userlist[circle].uid == UNUSED )
			return (circle);
		circle = (circle + 1) % maxusers;
	} while ( circle != start);
	return (FAIL);
}

void
hashinit(void)
{
	int index;

	for(index=0; index < maxusers ; index++)
	{
		userlist[index].uid = UNUSED;
		userlist[index].usage = 0;
		userlist[index].name[0] = '\0';
	}
}

void
setup(char *pfile)
{
	register struct passwd	*pw;
	int index;

	_pw_stayopen = 1;
	setpwent();
	while ( (pw=getpwent()) != NULL )
	{
		if ( (index=hash(pw->pw_uid)) == FAIL )
		{
			fprintf(stderr,"diskacct: INCREASE THE VALUE OF THE ENVIRONMENT VARIABLE ACCT_MAXUSERS\n");
			return;
		}
		if ( userlist[index].uid == UNUSED )
		{
			userlist[index].uid = pw->pw_uid;
			strncpy( userlist[index].name, pw->pw_name, MAXNAME);
		}
	}
}

int
todo(char *fname)
{
	register	i;

	for(i = 0; i < igncnt; i++) {
		if(strncmp(fname, ignlist[i], 6) == 0) return(FALSE);
	}
	return(TRUE);
}

char *
skip(char *str)
{
	while(*str) {
		if(*str == ' ' ||
		    *str == ',') {
			*str = '\0';
			str++;
			break;
		}
		str++;
	}
	return(str);
}

void
error(void)
{
	fprintf(stderr, "%s: i/o error\n", cmd);
	exit(-1);
}

void
xfs_ilist(char *file, int fd)
{
	ino64_t		last;
	int		index, i, c, n = 1000;
	xfs_bstat_t	*t;

	t = malloc(n * sizeof(*t));
	last = 0;
	while (syssgi(SGI_FS_BULKSTAT, fd, &last, n, t, &c) == 0) {
		if (c == 0)
			break;
		for (i = 0; i < c; i++) {
			if (xfs_count(&t[i]) == FAIL) {
				if(VERBOSE)
					fprintf(stderr,"BAD UID: file system = %s, inode = %llu, uid = %u\n",
						file, t[i].bs_ino, t[i].bs_uid);
				if(ufd)
					fprintf(ufd, "%s %llu %u\n",
						file, t[i].bs_ino, t[i].bs_uid);
			}
		}
	}
	free(t);
}

int
xfs_count(xfs_bstat_t *tp)
{
	int index;

	if( (index = hash(tp->bs_uid)) == FAIL || userlist[index].uid == UNUSED )
		return (FAIL);
	userlist[index].usage += (tp->bs_blocks * tp->bs_blksize) / BBSIZE;
	return (SUCCEED);
}
