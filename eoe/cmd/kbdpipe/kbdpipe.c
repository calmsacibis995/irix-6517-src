/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/* #ident	"@(#)kbdpipe:kbdpipe.c	1.3"			*/

#ident  "$Revision: 1.1 $"


/*
 * kbdpipe.c	Uses "kbd" tables in a pipeline.
 *
 *	Usage:
 *
 *	kbdpipe -t table [-f loadfile] [-F] [-o outfile] [infile]
 *
 *	The process forks, with the child handling final output and
 *	reading a pipeline.
 *	The parent pushes "kbd" into the "read" pipeline.
 *		If the table is not publicly resident, an attempt is made
 *		to load & attach the table.  Load is from trusted dir.
 *	The parent sends stdin to the pipe, the child reads it and
 *	writes the output.
 *
 *	-t table	Tries to attach "table".  If it fails, it tries
 *			to do kbdmap on file named /usr/lib/kbd/table.
 *			then attach that table.
 *
 *	-f file		If "table" can't be attached, tries to load
 *			"file" via kbdmap, then attach "table".  If the
 *			filename begins with "/", it is tried.  If it
 *			does NOT begin with "/", then "/usr/lib/kbd" is
 *			pre-pended, and that is tried.  If it succeeds in
 *			loading "file", it still tries to attach "table".
 *
 *	-F		Forces outfile to be overwritten if it exists.
 *
 *	-o outfile	Output goes to "outfile" instead of stdout.
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/stropts.h>
#include <fcntl.h>
#include <errno.h>
#include <locale.h>
#include <signal.h>
#include <sys/kbdm.h>
/* #include <sys/kbd.h> */
#include <pfmt.h>
#include <string.h>

#define assuming(x)	if (x) { while (x)
#define otherwise	} else
#define loop

extern int optind, opterr;
extern char *optarg;
char *prog;
int opttrunc;

#ifdef DEBUG
char trusted[256] = "./";
#else
char trusted[256] = "/usr/lib/kbd/";	/* THE trusted place */
#endif

#define LOADER	"kbdload"	/* name of loader program */

#define READSIDE	0
#define WRITESIDE	1

int pfd[2];	/* pipe fds */

main(argc, argv)
	int argc;
	char **argv;
{
	register char *path, *table, *output, *fname;
	register int c, chile;
	struct stat st;

	(void)setlocale(LC_ALL, "");
	(void)setlabel("UX:pipe");
	(void)setcat("uxmesg");

	table = output = path = fname = (char *) 0;
	opttrunc = opterr = 0;
	prog = *argv;
	while ((c = getopt(argc, argv, "f:t:o:F")) != EOF) {
		switch (c) {
		case 'f':	fname = optarg; break;
		case 't':	table = optarg; break;
		case 'o':	output = optarg; break;
		case 'F':	opttrunc = 1; break;
		default:
				usage(0);
		}
	}
	if (! table)
		usage(1);
	if (output) {
		if (! opttrunc) {	/* don't overwrite */
			if (stat(output, &st) == 0) {	/* file exists */
				pfmt(stderr, MM_ERROR, ":129:%s: file exists.\n",
					output);
				exit(1);
			}
		}
		if (!freopen(output, "w", stdout)) {
			pfmt(stderr, MM_ERROR, ":130:Cannot re-open stdout: %s\n",
				strerror(errno));
			exit(1);
		}
	}
	/*
	 * open pipes & try to push stuff
	 */
	dopipes(table, fname);
	switch (chile = fork()) {
		case 0:		/* child */
			close(pfd[WRITESIDE]);
			doread(optind < argc ? 512 : 1);
			exit(0);
		case -1:	/* failure */
			pfmt(stderr, MM_ERROR, ":131:fork() failed: %s\n",
				strerror(errno));
			exit(1);
		default:	/* parent */
			break;
	}
	/*
	 * Parent gets this far; child stops above.
	 */
	close(pfd[READSIDE]);	/* parent doesn't need READSIDE */
	assuming (optind < argc) loop {
		if (! freopen(argv[optind], "r", stdin)) {
			kill(chile, SIGKILL);
			pfmt(stderr, MM_ERROR, ":132:Cannot reopen stdin on %s: %s\n",
				argv[optind], strerror(errno));
			exit(1);
		}
		dowrite();
		++optind;
	}
	otherwise {
		dowrite();
	}
	close(pfd[WRITESIDE]);
	exit(0);
}

usage(complain)
int complain;
{
	if (complain)
		pfmt(stderr, MM_ERROR, ":26:Incorrect usage\n");
	pfmt(stderr, MM_ACTION,
		":133:Usage: %s -t table [-f path] [-o outfile] [-F] [infile(s)]\n", prog);
	exit(1);
}

/*
 * read stdin, write pfd[WRITESIDE]
 */

dowrite()

{
	register int c;
	char ch;	/* no register */

	while ((c = getchar()) != EOF) {
		ch = c;
		write(pfd[WRITESIDE], &ch, 1);
	}
}

/*
 * Child: read pfd[READSIDE], write stdout.  Try to do
 * reasonable chunks...  This produces the final output.
 */

char rbuf[514];

doread(n)
	int n;
{
	register int i, j;

	while ((i = read(pfd[READSIDE], rbuf, n)) > 0) {
		j = 0;
		while (i--) {
			putchar(rbuf[j]);
			++j;
		}
	}
}

char cmd[512];

/*
 * dopipes	sets up the pipe (or Stream Pipe) so we can WRITE
 *		to one and READ from the other.
 */
dopipes(t, fn)
	register unsigned char *t;	/* table name */
	register unsigned char *fn;	/* file name if can't attach */
{
	register int i;
	struct strioctl sb;
	int rval;	/* for child return */

#ifdef SVR4
	if (pipe(&pfd[0])) {
		pfmt(stderr, MM_ERROR, ":134:pipe() failed: %s\n", strerror(errno));
		exit(1);
	}
	if (ioctl(pfd[READSIDE], I_PUSH, "kbd")) {	/* push kbd */
		pfmt(stderr, MM_ERROR, ":135:Cannot push \"kbd\" module: %s\n",
			strerror(errno));
		exit(1);
	}
#else
	/*
	 * On SVR 3.x, use "/dev/spx" (the SP driver); push just after
	 * open, before link, so don't do PUSH here.
	 */
	if (spipe(&pfd[0])) {
		pfmt(stderr, MM_ERROR, ":134:pipe() failed: %s\n", strerror(errno));
		exit(1);
	}
#endif
	if (tach(t, pfd[READSIDE])) {
		/*
		 * If we can attach the table, no sweat.
		 */
		if (turnon(pfd[READSIDE]))
			return;
		pfmt(stderr, MM_ERROR, ":136:Cannot turn on \"kbd\" module.\n");
		exit(1);
	}
	/*
	 * Otherwise, try to load and then attach.  It's a pain
	 * because we need stdin/stdout to be the pipe when we call
	 * the loader.  We fork and let a child take care of it.
	 */
	switch(fork()) {
		case 0:	 break;	/* child continues */
		case -1: pfmt(stderr, MM_ERROR, ":131:fork() failed: %s\n",
				strerror(errno));
			 exit(1);
		default: if ((wait(&rval) == (-1)) && (errno != ECHILD)) {
				pfmt(stderr, MM_NOSTD, ":137:(Interrupted)");
				exit(2);
			 }
			 if ((rval != 0) && (errno != ECHILD)) {
/*				fprintf(stderr, "Unknown error %d\n", */
/*						prog, errno); */
			 	exit(1);
			 }
			 return;	/* whew! */
	}
	/*
	 * The child gets this far; the parent is waiting
	 * above, and will NEVER get this far.
	 */
	close(0); close(1);
	if ((dup(pfd[READSIDE]) == (-1)) || (dup(pfd[WRITESIDE]) == (-1))) {
		pfmt(stderr, MM_ERROR, ":138:dup() failed: %s\n", strerror(errno));
		exit(1);
	}
	fprintf(stderr, "step 1.\n");
	close(pfd[READSIDE]);
	close(pfd[WRITESIDE]);
	fprintf(stderr, "step 11.\n");
	/*
	 * Use "system" here so we don't have to fork, exec, and wait
	 * around for the return value, which simplifies things considerably
	 * at this juncture.  "exec" would probably speed it up, but
	 * would complicate things.  Use of "tach" and "turnon" directly
	 * instead of calling "kbdkey" reduces out one "system()" call;
	 * if the system is set up to cater to its user community, we'll
	 * rarely get this far...right?
	 */
	sprintf(cmd, "%s %s%s", LOADER, (fn[0] == '/' ? "" : trusted),
			(fn ? fn : t));
	fprintf(stderr, "step 2.\n");
	if (system(cmd)) {
		pfmt(stderr, MM_ERROR, ":139:Cannot load\n");
		exit(1);
	}
	fprintf(stderr, "step 3.\n");
	if (tach(t, 0))
		turnon(0);
	else {
		pfmt(stderr, MM_ERROR, ":140:Cannot attach %s\n", t);
		exit(1);
	}
	fprintf(stderr, "step 4.\n");
	exit(0);	/* child must exit */
}

#ifndef SVR4
/*
 * spipe	Open "/dev/spx" and obtain a Streams Pipe.  This function
 *		"borrowed" from UTS library source with some mods.  You
 *		only need it on SVR3.?.
 */

#define SPCLONE	"/dev/spx"

spipe(fds)
	int *fds;
{
        struct strfdinsert fdi;
	long dummy;

	if ((fds[0] = open(SPCLONE, O_RDWR)) < 0)
		return(-1);
	if (ioctl(fds[0], I_PUSH, "kbd")) {
		pfmt(stderr, MM_ERROR, ":135:Cannot push \"kbd\" module: %s\n",
			strerror(errno));
		exit(1);
	}
	if ((fds[1] = open(SPCLONE, O_RDWR)) < 0)
		return(-1);
	fdi.databuf.maxlen = fdi.databuf.len = -1;
	fdi.databuf.buf = NULL;
	fdi.ctlbuf.maxlen = fdi.ctlbuf.len = sizeof(long);
	fdi.ctlbuf.buf = (caddr_t)&dummy;
	fdi.offset = 0;
	fdi.fildes = fds[1];
	fdi.flags = 0;
	if (ioctl(fds[0], I_FDINSERT, &fdi) < 0)
		return(-1);
	return(0);
}
#endif	/* ! SVR4 */

tach(name, fd)
	char *name;
	int fd;
{
	struct strioctl sb;
	struct kbd_tach t;
	register int rval;

	sb.ic_cmd = KBD_ATTACH;
	sb.ic_timout = 15;
	sb.ic_len = sizeof(struct kbd_tach);
	sb.ic_dp = (char *) &t;
	strcpy((char *)t.t_table, name);
	t.t_type = Z_UP;
	rval = ioctl(fd, I_STR, &sb);
	if (rval)
		return(0);	/* tach failed */
	return(1);
}

turnon(fd)
	int fd;
{
	struct strioctl sb;
	struct kbd_ctl c;
	register int rval;

	sb.ic_cmd = KBD_ON;
	sb.ic_timout = 15;
	sb.ic_len = sizeof(struct kbd_ctl);
	sb.ic_dp = (char *) &c;
	c.c_type = Z_UP;
	rval = ioctl(fd, I_STR, &sb);
	if (rval) {
		pfmt(stderr, MM_ERROR, ":141:Cannot turn on mapping: %s\n",
			strerror(errno));
		return(0);
	}
	return(1);
}
