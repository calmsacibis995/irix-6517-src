/*
 * exop - some simple operations on files using /dev/fsctl
 *
 *	cat exno file	-concatentate exno and exno+1
 *	split exno file	-split extent 'exno'
 *	dump file	-print extents and indirect extents
 *	ne file		-print number of extents and # i-extents
 *	lock secs file	-ILOCK 'file' for 'secs' seconds
 *
 * uses lib.c:movex() for cat and split
 *
 */
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>

#include "libefs.h"

#include <sys/fsctl.h>

#include "fsr.h"

extern int errno;

char    *progname;

excat(struct efs_mount *mp, struct fscarg *fap, int exno);
exsplit(struct efs_mount *mp, struct fscarg *fap, int exno);


main(int argc, char **argv)
{
        struct stat statbuf;
	struct fscarg fa;
	int  arg = 0;
	char *devname, *filename;
	char *op;
	struct efs_mount *mp;

        progname = argv[0];

        if (argc < 3) {
                fprintf(stderr,
			"usage: %s ne|dump|split|cat|ilock [arg] file\n",
			progname);
                exit(1);
        }
	op = *++argv;
	if (argc == 4)
        	arg = atoi(*++argv);
        filename = *++argv;

	if (stat(filename, &statbuf) == -1) {
		perror("stat");
		exit(1);
	}

	efs_init(printf);

	devname = devnm(statbuf.st_dev);
	mp = efs_mount(devname, O_RDWR);

        if (fsc_init(printf, 1, 1)) {
		fprintf(stderr, "%s: fsc_init() failed e=%d\n", progname,errno);
		exit(1);
	}

	fa.fa_dev = statbuf.st_dev;
	fa.fa_ino = statbuf.st_ino;

	if (fsc_ilock(&fa) == -1) {
		if (errno == EBUSY) {
			printf("%s: ino=%d in use\n", progname, fa.fa_ino);
			exit(0);
		}
		printf("%s: ilock(0x%x, %d) failed, errno=%d\n", progname,
			fa.fa_dev, fa.fa_ino, errno);
		exit(1);
	}
	getex(mp, &fa);

	if (strcmp(op, "split") == 0)
		exsplit(mp, &fa, arg);
	else if (strcmp(op, "cat") == 0)
		excat(mp, &fa, arg);
	else if (strcmp(op, "dump") == 0)
		fprex(stdout, fa.fa_ne, fa.fa_ex, fa.fa_nie, fa.fa_ix);
	else if (strcmp(op, "ne") == 0)
		printf("%d %d %s\n", fa.fa_ne, fa.fa_nie, filename);
	else	/* op == "ilock" */
		sleep(arg);

	if (fsc_iunlock(&fa))
		exit(1);

	exit(0);
}

/*
 * split extent 'exno' in 2
 */
exsplit(struct efs_mount *mp, struct fscarg *fap, int exno)
{
	efs_daddr_t newbn;
	int gotlen;

	if (exno < 0 || exno > fap->fa_ne - 1) {
		fprintf(stderr,"%s: cannot split %d of %d extents\n",
			progname, exno, fap->fa_ne);
		exit(1);
	}
	newbn = freefs(mp, 0, 2, &gotlen);
	if (gotlen < 2) {
		fprintf(stderr, "%s: failed to find 2 free blocks\n", progname);
		exit(1);
	}
	if (movex(mp, fap, (fap->fa_ex+exno)->ex_offset, newbn, 1))
		exit(1);
}

/*
 * concatenate extent 'exno' with 'exno'+1
 * at the end of 'exno' if space, else with whole moved elsewhere
 */
excat(struct efs_mount *mp, struct fscarg *fap, int exno)
{
	efs_daddr_t newbn;
	int len, gotlen;
	extent ex;

	if (exno > fap->fa_ne - 2) {
		fprintf(stderr, "%s: cannot cat %d of %d extents\n",
			progname, exno, fap->fa_ne);
		exit(1);
	}

	len = (fap->fa_ex+exno)->ex_length + (fap->fa_ex+exno+1)->ex_length;
	if (len > EFS_MAXEXTENTLEN) {
		fprintf(stderr,"%s: exno %d + exno+1 too big\n", progname,exno);
		exit(1);
	}
	newbn = freefs(mp, EFS_BBTOCG(mp->m_fs, fap->fa_ex->ex_bn),
			len, &gotlen);
	if (gotlen < len) {
		fprintf(stderr, "%s: failed to find %d free blocks\n",
			progname, len);
		exit(1);
	}
	ex.ex_bn = newbn;
	ex.ex_length = len;
	if (vecmovex(mp, fap, 1, &ex, (fap->fa_ex+exno)->ex_offset, len) == -1){
		fprintf(stderr,"%s: newmovex() failed\n", progname);
		abort();
	}
}
