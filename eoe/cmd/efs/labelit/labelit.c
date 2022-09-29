#ident "$Revision: 1.9 $"

/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*
 * EFS version of labelit.
 */
#include "efs.h"
struct efs *fs;		/* XXX library globals, feh */
#include <sys/stat.h>
#include <signal.h>
#include <ustat.h>

#define DEV 1
#define FSNAME 2
#define VOLUME 3
 /* write fsname, volume # on disk superblock */
struct {
	char filler[EFS_SUPERBOFF];
	union {
		char data[BBTOB(BTOBB(sizeof(struct efs)))];
		struct efs fs;
	} su;
} super;

#define IFTAPE(s) (equal(s,"/dev/rmt",8)||equal(s,"rmt",3)||equal(s,"/dev/tape",8))
#define TAPENAMES "'/dev/rmt' or '/dev/tape'"

struct {
	char	t_magic[8];
	char	t_volume[6];
	char	t_reels,
		t_reel;
	long	t_time,
		t_length,
		t_dens;
	char	t_fill[484];
} Tape_hdr;
char *progname;

static int equal(char *s1, char *s2, int ct);
static int ismounted(char *name);

static void 
sigalrm(void)
{
	signal(SIGALRM, sigalrm);
}

int
main(int argc, char ** argv) 
{
	int fsi, fso;
	static char *usage = "Usage: %s /dev/r??? [fsname volume [-n]]\n";

	progname = argv[0];
	signal(SIGALRM, sigalrm);

	if(argc!=4 && argc!=2 && argc!=5)  {
		fprintf(stderr, usage, progname);
		exit(2);
	}

	if ((argc != 2) && ismounted(argv[DEV]))
	{
		fprintf(stderr,"%s: %s contains a mounted filesystem.\n",
			progname, argv[DEV]);
		exit(1);
	}

	if(argc==5) {
		if(strcmp(argv[4], "-n")!=0) {
			fprintf(stderr, usage, progname);
			exit(2);
		}
		if(!IFTAPE(argv[DEV])) {
			fprintf(stderr, "%s: `-n' option for tape only\n",
					progname);
			fprintf(stderr, "\t'%s' is not a valid tape name\n", argv[DEV]);
			fprintf(stderr, "\tValid tape names begin with %s\n", TAPENAMES);
			fprintf(stderr, usage, progname);
			exit(2);
		}
		printf("Skipping label check!\n");
		goto do_it;
	}

	if((fsi = open(argv[DEV],0)) < 0) {
		fprintf(stderr, "%s: cannot open %s: %s\n", progname,argv[DEV],
			strerror(errno));
		exit(2);
	}

	if(IFTAPE(argv[DEV])) {
		alarm(5);
		read(fsi, &Tape_hdr, sizeof(Tape_hdr));
		alarm(0);
		if(!(equal(Tape_hdr.t_magic, "Volcopy", 7)||
		    equal(Tape_hdr.t_magic,"Finc",4))) {
			fprintf(stderr, "%s: tape not labelled!\n", progname);
			exit(2);
		}
		printf("%s tape volume: %s, reel %d of %d reels\n",
			Tape_hdr.t_magic, Tape_hdr.t_volume, Tape_hdr.t_reel, Tape_hdr.t_reels);
		printf("Written: %s", ctime(&Tape_hdr.t_time));
		if(argc==2 && Tape_hdr.t_reel>1)
			exit(0);
	}
	if(read(fsi, &super, sizeof(super)) != sizeof(super))  {
		fprintf(stderr, "%s: cannot read superblock\n", progname);
		exit(2);
	}
	if (!IS_EFS_MAGIC(super.su.fs.fs_magic))
	{
		fprintf(stderr, "%s: %s is not an extent filesystem\n",
				progname, argv[DEV]);
		exit(2);
	}

	printf("Current fsname: %.6s, Current volname: %.6s,",
		super.su.fs.fs_fname, super.su.fs.fs_fpack);
	printf(" Blocks: %ld, Inodes: %d\nFS Units: 1Kb, ",
		super.su.fs.fs_cgfsize * super.su.fs.fs_ncg,
		super.su.fs.fs_cgisize * super.su.fs.fs_ncg * EFS_INOPBB - 2);
	printf("Date last modified: %s", ctime((time_t *)&super.su.fs.fs_time));
	if(argc==2)
		exit(0);
do_it:
	printf("NEW fsname = %.6s, NEW volname = %.6s \n",
		argv[FSNAME], argv[VOLUME]);
	sprintf(super.su.fs.fs_fname, "%.6s", argv[FSNAME]);
	sprintf(super.su.fs.fs_fpack, "%.6s", argv[VOLUME]);
	fs = &super.su.fs;
	efs_checksum();

	close(fsi);
	if((fso = open(argv[DEV],1)) < 0) {
		fprintf(stderr, "%s: cannot open %s for writing: %s\n", progname,
			argv[DEV], strerror(errno));
		exit(2);
	}
	if(IFTAPE(argv[DEV])) {
		strcpy(Tape_hdr.t_magic, "Volcopy");
		sprintf(Tape_hdr.t_volume, "%.6s", argv[VOLUME]);
		if(write(fso, &Tape_hdr, sizeof(Tape_hdr)) < 0)
			goto cannot;
	}
	if(write(fso, &super, sizeof(super)) < 0) {
cannot:
		fprintf(stderr, "labelit cannot write label\n");
		exit(2);
	}
	exit(0);
	/* NOTREACHED */
}

static int
equal(char *s1, char *s2, int ct)
{
	int i;

	for(i=0; i<ct; ++i) {
		if(*s1 == *s2) {;
			if(*s1 == '\0') return(1);
			s1++; s2++;
			continue;
		} else return(0);
	}
	return(1);
}

static int
ismounted(char *name)
{
	struct ustat ustatarea;
	struct stat sb;

	if (stat(name, &sb) < 0) return (0);

	if (((sb.st_mode & S_IFMT) != S_IFCHR) &&
	    ((sb.st_mode & S_IFMT) != S_IFBLK))  return (0);

	if (ustat(sb.st_rdev,&ustatarea) >= 0) return (1);
	else return (0);
}
