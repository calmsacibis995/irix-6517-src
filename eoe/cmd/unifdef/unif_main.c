/*
 * Unifdef - strip or reduce ifdefs in C code.
 *
 * TODO:
 *	#define/#undef tracking/ignoring?
 */
#include <errno.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "unifdef.h"

char	*progname;		/* name invoked as */
int	status;			/* exit status counts errors */
FILE	*outfile = stdout;	/* output file pointer */
char	*outfilename;		/* output file name or null if stdout */
int	zflag;			/* whether to strip #if 0...#endif */

extern void	initkeywords(void);
extern void	process(FILE *, char *);
static void	openoutput(char *, int, char **);
static void	closeoutput(char *);

void
main(int argc, char **argv)
{
	int opt;
	char *oname, *value;
	FILE *input;
#ifdef METERING
	int mflag = 0;
#endif

	progname = argv[0];
	oname = 0;
	while ((opt = getopt(argc, argv, "D:U:d:u:o:mz")) != EOF)
		switch (opt) {
		  case 'D':
		  case 'd':
			value = strchr(optarg, '=');
			if (value)
				*value++ = '\0';
			install(optarg, SDEFINED, 1, value);
			break;
		  case 'U':
		  case 'u':
			install(optarg, SUNDEFINED, 0, 0);
			break;
		  case 'o':
			oname = optarg;
			break;
		  case 'm':
#ifdef METERING
			mflag = 1;
			break;
#endif
		  case 'z':
			zflag = 1;
			break;
		  default:
			fprintf(stderr,
	    "usage: %s [-D name] [-U name] [-o output] [-z] [input ...]\n",
				progname);
			exit(-1);
		}

	argc -= optind;
	argv += optind;
	if (oname)
		openoutput(oname, argc, argv);
	initkeywords();
	if (argc == 0)
		process(stdin, 0);
	else
		for (; --argc >= 0; argv++) {
			input = fopen(*argv, "r");
			if (input == 0) {
				Perror(*argv);
				continue;
			}
			process(input, *argv);
			if (fclose(input) == EOF)
				Perror(*argv);
		}
	if (oname)
		closeoutput(oname);
#ifdef METERING
	if (mflag)
		dumphashmeter();
#endif
	exit(status);
}

static int	overwriting;
static char	tmplate[] = "unifdef.XXXXXX";
static char	*tmpname = tmplate;

#define	EQ(sb1, sb2) \
	((sb1).st_dev == (sb2).st_dev && (sb1).st_ino == (sb2).st_ino)

static void
openoutput(char *name, int argc, char **argv)
{
	int ofd;
	struct stat64 osb, isb;
	char *cp, *tp, dirname[PATH_MAX];

	ofd = -1;
	outfilename = name;
	if (stat64(name, &osb) == 0) {
		if (argc == 0) {
			if (fstat64(fileno(stdin), &isb) == 0 && EQ(isb, osb))
				overwriting = 1;
		} else {
			for (; --argc >= 0; argv++) {
				if (stat64(*argv, &isb) < 0) {
					Perror(*argv);
					exit(status);
				}
				if (EQ(isb, osb)) {
					overwriting = 1;
					break;
				}
			}
		}
		if (overwriting) {
			if (name[0] == '/') {
				cp = strcpy(dirname, name) + strlen(name) - 1;
				while (cp > dirname && *cp == '/')
					--cp;
				while (*cp != '/')
					--cp;
				for (tp = tmplate; (*++cp = *tp) != '\0'; tp++)
					;
				tmpname = strdup(dirname);
			}
			ofd = mkstemp(tmpname);
			if (ofd < 0) {
				Perror(tmpname);
				exit(status);
			}
			outfilename = tmpname;
		}
	}
	if (ofd < 0)
		outfile = fopen(name, "w");
	else
		outfile = fdopen(ofd, "w");
	if (outfile == 0) {
		Perror(name);
		terminate();
	}
}

static void
closeoutput(char *name)
{
	if (fclose(outfile) == EOF) {
		Perror(outfilename);
		terminate();
	}
	if (overwriting && rename(tmpname, name) < 0) {
		Perror(name);
		terminate();
	}
}

void
terminate()
{
	if (overwriting)
		(void) unlink(tmpname);
	exit(status);
}

void
Perror(char *message)
{
	int error = errno;

	fprintf(stderr, "%s: ", progname);
	if (message)
		fprintf(stderr, "%s: ", message);
	fprintf(stderr, "%s.\n", strerror(error));
	status++;
}

void *
_new(int size)
{
	void *p;

	p = malloc(size);
	if (p == 0) {
		Perror("malloc");
		terminate();
	}
	return p;
}

void
delete(void *p)
{
	if (p)
		free(p);
}

char *
strdup(const char *s)
{
	return strcpy(newvec(strlen(s)+1, char), s);
}
