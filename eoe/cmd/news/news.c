/*	Copyright (c) 1993 UNIX System Laboratories, Inc. 	*/
/*	  All Rights Reserved                             	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF     	*/
/*	UNIX System Laboratories, Inc.   	            	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*		copyright	"%c%" 	*/

#ident	"@(#)news:news.c	1.15.1.8"
#ident "$Header: /proj/irix6.5.7m/isms/eoe/cmd/news/RCS/news.c,v 1.5 1993/07/29 23:51:23 tsai Exp $"

/*
 * news foo	prints /var/news/foo
 * news -a	prints all news items, latest first
 * news -n	lists names of new items
 * news -s	tells count of new items only
 * news		prints items changed since last news
 */

#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <setjmp.h>
#include <signal.h>
#include <dirent.h>
#include <pwd.h>
#include <time.h>
#include <locale.h>
#include <pfmt.h>
#include <errno.h>
#include <string.h>
#include <utime.h>

#define INDENT 3
#define	RD_WR_ALL	(S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH)

#define DATE_FMT	"%a %b %e %H:%M:%S %Y"
#define DATE_FMTID	":22"
/*
 *	%a	abbreviated weekday name
 *	%b	abbreviated month name
 *	%e	day of month
 *	%H	hour (24-hour clock)
 *	%M	minute
 *	%S	second
 *	%Y	year
 */

/*
 * The following items should not be printed.
 */
char	*ignore[] = {
		"core",
		NULL
};

const char nonews[] = ":338:No news.\n";

struct n_file {
	time_t	n_time;
	char	*n_name;
} *n_list;

char	NEWS[] = "/var/news";	/* directory for news items */

int	aopt = 0;	/* 1 say -a specified */
int	n_count;	/* number of items in NEWS directory */
int	number_read;	/* number of items read */
int	nopt = 0;	/* 1 say -n specified */
int	optsw;		/* for getopt */
int	opt = 0;	/* number of options specified */
int	sopt = 0;	/* 1 says -s specified */
int	exit_code = 0;	/* set to 1 if an error happend */
char	stdbuf[BUFSIZ];
char	time_buf[50];	/* holds date and time string */

jmp_buf	save_addr;

void	all_news();
void	count();
void	ck_num();
void	initialize();
void	late_news();
void	notify();
void	onintr();
void	print_item();
void	read_dir();
extern	char *strdup();

main(argc, argv)
int	argc;
char	**argv;
{
	int i;

	(void) setlocale(LC_ALL, "");
	(void) setcat("uxcore");
	(void) setlabel("UX:news");

	setbuf(stdout, stdbuf);
	initialize();
	read_dir();

	if (argc <= 1) {
		late_news(print_item, 1);
		ck_num();
	}
	else while ((optsw = getopt(argc, argv, "ans")) != EOF)
		switch(optsw) {
		case 'a':
			aopt++;
			opt++;
			break;

		case 'n':
			nopt++;
			opt++;
			break;

		case 's':
			sopt++;
			opt++;
			break;

		default:
			pfmt(stderr, MM_ERROR, 
				":339:Usage: news [-a] [-n] [-s] [items]\n");
			exit(1);
	}

        if (opt > 1) {
        	pfmt(stderr, MM_ERROR, 
        		":340:Options are mutually exclusive\n");
        	exit(1);
	}

        if (opt > 0 && argc > 2) {
        	pfmt(stderr, MM_ERROR, 
        		":341:Options are not allowed with file names\n");
        	exit(1);
	}

	if (aopt) {
		all_news();
		ck_num();
		exit(exit_code);
	}

	if (nopt) {
		late_news(notify, 0);
		ck_num();
		exit(exit_code);
	}

	if (sopt) {
		late_news(count, 0);
		exit(exit_code);
	}

	for (i = 1; i < argc; i++)
		print_item(argv[i]);

	exit(exit_code);
	/*NOTREACHED*/
}

/*
 * read_dir: get the file names and modification dates for the
 * files in /var/news into n_list; sort them in reverse by
 * modification date. We assume /var/news is the working directory.
 */

void
read_dir()
{
	int compare();
	struct dirent *readdir();
	struct dirent *nf;
	struct stat sbuf;
	char *fname;
	DIR *dirp;
	register char **p;

	/* Open the current directory */
	if ((dirp = opendir(".")) == NULL) {
		pfmt(stderr, MM_ERROR, ":92:Cannot open %s: %s\n",
			NEWS, strerror(errno));
		exit(1);
	}

	/* Read the file names into n_list */
	n_count = 0;
	while (nf = readdir(dirp)) {
		if (stat(nf->d_name, &sbuf) < 0
			|| (sbuf.st_mode & S_IFMT) != S_IFREG) {
			continue;
		}

		/* Is this filename on our list of names-to-ignore? */
		p = ignore;
		while (*p != NULL && strcmp(*p, nf->d_name) != 0)
			++p;
		if (*p != NULL)
			continue;

		/* No it's not, so save it away */
		fname = strdup(nf->d_name);
		if (n_count++ > 0)
			n_list = (struct n_file *) realloc((char *) n_list,
				(unsigned)(sizeof (struct n_file) * n_count));
		else
			n_list = (struct n_file *) malloc(
				(unsigned)(sizeof (struct n_file) * n_count));

		if (fname == NULL || n_list == NULL) {
			pfmt(stderr, MM_ERROR, ":31:Out of memory: %s\n",
				strerror(errno));
			exit(1);
		}

		n_list[n_count-1].n_time = sbuf.st_mtime;
		n_list[n_count-1].n_name = fname;
	}

	/* Sort the elements of n_list in decreasing time order */
	qsort((char *)n_list, n_count, sizeof(struct n_file), compare);

	/* Clean up */
	closedir(dirp);
}

/*
 * Comparison function for qsort: return -1, 0 or +1,
 * depending on whether a is to be considered less than,
 * equal to or greater than b.  Since we want to sort
 * in reverse order, this corresponds to a's time being
 * greater than, equal to or less than b's, respectively.
 * Awkward code, but we aren't allowed to assume that
 * time_t satisfies general arithmetic properties.
 */

int
compare(a, b)
	struct n_file *a, *b;
{
	double diff;

	diff = difftime(b->n_time, a->n_time);	/* b->n_time - a->n_time */
	return ((diff < 0)? -1 : (diff > 0));	/* sign of the result */
}

void
initialize()
{
	if (signal(SIGQUIT, SIG_IGN) != (void(*)())SIG_IGN)
		signal(SIGQUIT, _exit);
	umask(((~(S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)) & S_IAMB));
	if (chdir(NEWS) < 0) {
		pfmt(stderr, MM_ERROR,
			":342:Cannot change directory to \"%s\" directory: %s\n",
			NEWS, strerror(errno));
		exit(1);
	}
}

void
all_news()
{
	int i;

	for (i=0; i<n_count; i++)
		print_item(n_list[i].n_name);
}

void
print_item(fname)
	char *fname;
{
	FILE *fd;
	static int firstitem = 1;
	struct passwd *getpwuid();

	if (fname == NULL) {
		return;
	}
	if ((fd = fopen(fname, "r")) == NULL) {
		pfmt(stdout, MM_ERROR, ":343:Cannot open %s/%s: %s\n",
			NEWS, fname, strerror(errno));
		exit_code = 1;
	}
	else {
		register int c, ip, op;
		struct stat sbuf;
		struct passwd *pw;

		fstat(fileno(fd), &sbuf);
		if (firstitem) {
			firstitem = 0;
			putchar('\n');
		}
		if (setjmp(save_addr))
			goto finish;
		if (signal(SIGINT, SIG_IGN) != (void(*)())SIG_IGN)
			signal(SIGINT, onintr);
		printf("%s ", fname);
		pw = getpwuid(sbuf.st_uid);
		if (pw)
			printf("(%s)", pw->pw_name);
		else
			printf(".....");
		cftime(time_buf, gettxt(DATE_FMTID, DATE_FMT), &sbuf.st_mtime); 
		printf(" %s\n", time_buf);
		op = 0;
		ip = INDENT;
		while ((c = getc(fd)) != EOF) {
			switch (c) {

			case '\r':
			case '\n':
				putchar(c);
				op = 0;
				ip = INDENT;
				break;

			case ' ':
				ip++;
				break;

			case '\b':
				if (ip > INDENT)
					ip--;
				break;

			case '\t':
				ip = ((ip - INDENT + 8) & -8) + INDENT;
				break;

			default:
				while (ip < op) {
					putchar('\b');
					op--;
				}
				while ((ip & -8) > (op & -8)) {
					putchar('\t');
					op = (op + 8) & -8;
				}
				while (ip > op) {
					putchar(' ');
					op++;
				}
				putchar(c);
				ip++;
				op++;
				break;
			}
		}
		fflush(stdout);
finish:
		putchar('\n');
		fclose(fd);
		number_read++;
		if (signal(SIGINT, SIG_IGN) != (void(*)())SIG_IGN)
			signal(SIGINT, SIG_DFL);
	}
}

void
late_news(emit, update)
	int (*emit)();
	int update;
{
	time_t cutoff;
	int i;
	char fname[50], *cp;
	struct stat newstime;
	struct utimbuf utb;
	int fd;
	extern char *getenv();

	/* Determine the time when last called */
	cp = getenv("HOME");
	if (cp == NULL) {
		pfmt(stderr, MM_ERROR, ":344:Cannot find HOME variable\n");
		exit(1);
	}
	strcpy(fname, cp);
	strcat(fname, "/");
	strcat(fname, ".news_time");
	cutoff = (stat(fname, &newstime) < 0)? 0 : newstime.st_mtime;

	/* Print the recent items */
	for (i=0; i<n_count && difftime(n_list[i].n_time, cutoff) > 0.0; i++) {
		(*emit)(n_list[i].n_name);
		number_read++;
	}
	(*emit)((char *) NULL);
	fflush(stdout);

	if (update) {
		/* Re-create the file and refresh the update time */
		if (n_count > 0 && (fd = creat(fname, RD_WR_ALL)) >= 0) {
			utb.actime = utb.modtime = n_list[0].n_time;
			close(fd);
			utime(fname, &utb);
		}
	}
}

void
notify(s)
	char *s;
{
	static int first = 1;

	if (s) {
		if (first) {
			first = 0;
			pfmt(stdout, MM_INFO, NULL);
		}
		printf(" %s", s);
	} else if (!first)
		putchar('\n');
}

void
count(s)
	char *s;
{
	static int nitems = 0;

	if (s)
		nitems++;
	else {
		if (nitems) {
			if (nitems != 1) {
				pfmt(stdout, MM_INFO, ":345:%d news items.\n",
					nitems);
			} else {
				pfmt(stdout, MM_INFO, ":346:1 news item.\n");
			}
		}
		else pfmt(stdout, MM_INFO, nonews);
	}
}

void
onintr()
{
	sleep(2);
	longjmp(save_addr, 1);
}

void
ck_num()
{
	if (sopt && !number_read)
		pfmt(stdout, MM_INFO, nonews);
}
