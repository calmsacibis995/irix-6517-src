#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "stdlib.h"
#include "getopt.h"
#include "errno.h"

#define PTRUE	0x0001	/* print true branches */
#define PFALSE	0x0002
#define PCASE	0x0004
#define PFUNC	0x0008

int toprint = PTRUE|PFALSE|PCASE|PFUNC;
int pmissing;
int onlyzero;
int covered;

#define BIF	0
#define BFUNC	1
#define BCASE	2
#define BWHILE	3
#define BFOR	4
#define BDOWHILE 5
#define BDEFAULT 6
#define BQUEST	7

/* in same sequence as above */
char *types[] = {
	"if",
	"func",
	"case",
	"while",
	"for",
	"do-while",
	"default",
	"quest",
	0
};

struct map {
	long bno;	/* branch number */
	char *file;	/* file name */
	char *bfile;	/* basename of file name */
	long lno;	/* line number in file */
	char type;	/* type of operation */
	short ord;	/* ordinal per line */
	char *func;	/* function name for func operation */
};
struct map *map;
int nmap;		/* number of read in map lines */
int amap;		/* size of map array currently */
#define CHUNK	100

struct log {
	long bno;
	long a1;
	long a2;
};
struct log *log;
int nlog;
int alog;

/* string cache for files and functions */
struct strcache {
	char *name;
	char *bname;	/* basename */
	char sl;	/* does name have a slash in it?? */
	struct strcache *next;
};
struct strcache *strcache;

struct strcache *fnames;	/* function names to report on */
struct strcache *Fnames;	/* filename to report on */

struct strcache nullfunc = {
	"(null)",
	0,
	NULL
};

extern void readmap(FILE *f);
extern void readlog(FILE *f);
extern char *lookup(struct strcache **, char *, char **);
extern int mapcompare(const void *, const void *);
extern int logcompare(const void *, const void *);
extern void Usage(void);

int
main(int argc, char **argv)
{
	int i, c;
	char *mapfile = "btool_map";
	char *logfile;
	FILE *mf, *lf;
	struct map *mp;

	while ((c = getopt(argc, argv, "m:czwf:F:")) != EOF) {
		switch (c) {
		case 'm':
			mapfile = optarg;
			break;
		case 'c':
			covered = 1;
			if (onlyzero)
				Usage();
			break;
		case 'z':
			onlyzero = 1;
			if (covered)
				Usage();
			break;
		case 'w':
			pmissing = 1;
			break;
		case 'F':
			(void) lookup(&Fnames, optarg, NULL);
			break;
		case 'f':
			(void) lookup(&fnames, optarg, NULL);
			break;
		default:
			Usage();
			break;
		}
	}
	if (optind >= argc)
		Usage();

	if ((mf = fopen(mapfile, "r")) == NULL) {
		fprintf(stderr, "Cannot open %s\n", mapfile);
		exit(1);
	}

	/* read in and sort by name/line number the map file */
	readmap(mf);
	qsort(map, nmap, sizeof(*map), mapcompare);

	/* read in and sort by sequence number the log file(s) */
	for (; optind < argc; optind++) {
		logfile = argv[optind];
		if ((lf = fopen(logfile, "r")) == NULL) {
			fprintf(stderr, "Cannot open %s\n", logfile);
			exit(1);
		}
		readlog(lf);
	}
	qsort(log, nlog, sizeof(*log), logcompare);

	/*
	 * go through each map line looking for matches and find the
	 * correpsonding line in the log file
	 */
	for (i = 0, mp = map; i < nmap; i++, mp++) {
		struct log l, *lm;
		/* call matchfunc first! */
		if (!matchfunc(mp) || !matchfile(mp))
			continue;

		/* find log entry if there is one */
		l.bno = mp->bno;

		if ((lm = bsearch(&l, log, nlog, sizeof(*log), logcompare)) ==
						NULL) {
			if (pmissing)
				fprintf(stderr, "Missing branch number %d from logfile\n",
					l.bno);
			continue;
		}
		switch(mp->type) {
		case BFUNC:
			if (onlyzero && lm->a1) break;
			if (covered && !lm->a1) break;
			printf("\"%s\", line %d: %s was called %d times.\n",
				mp->file, mp->lno, mp->func, lm->a1);
			break;
		case BCASE:
		case BDEFAULT:
			if (onlyzero && lm->a1) break;
			if (covered && !lm->a1) break;
			if (mp->ord > 1)
				printf("\"%s\", line %d: %s(%d) was taken %d times.\n",
					mp->file, mp->lno,
					types[mp->type],
					mp->ord,
					lm->a1);
			else
				printf("\"%s\", line %d: %s was taken %d times.\n",
					mp->file, mp->lno,
					types[mp->type],
					lm->a1);
			break;
		case BIF:
		case BWHILE:
		case BFOR:
		case BDOWHILE:
		case BQUEST:
			if (onlyzero && (lm->a1 || lm->a2)) break;
			if (covered && !lm->a1 && !lm->a2) break;
			if (mp->ord > 1)
				printf("\"%s\", line %d: %s(%d) was taken TRUE %d, FALSE %d times.\n",
					mp->file, mp->lno,
					types[mp->type],
					mp->ord,
					lm->a1,
					lm->a2);
			else
				printf("\"%s\", line %d: %s was taken TRUE %d, FALSE %d times.\n",
					mp->file, mp->lno,
					types[mp->type],
					lm->a1,
					lm->a2);
		}
	}
	exit(0);
}

char *
lookup(struct strcache **c, char *name, char **sl)
{
	struct strcache *s;
	char *bn;

	/*
	 * most of the mapfile consists of (null) for func -
	 * special case that here
	 */
	if (name[0] == '(')
		return nullfunc.name;
	for (s = *c; s; s = s->next) {
		if (*name == s->name[0] && (strcmp(name, s->name) == 0)) {
			if (sl)
				*sl = s->bname;
			return s->name;
		}
	}
	s = malloc(sizeof(*s));
	s->name = strdup(name);
	s->next = *c;
	s->sl = 0;
	if ((s->bname = strrchr(s->name, '/')) != NULL) {
		s->sl = 1;
		s->bname++;
	} else
		s->bname = s->name;
	*c = s;
	if (sl)
		*sl = s->bname;
	return s->name;
}

void
readmap(FILE *f)
{
	int nf;
	auto long bno, lno, occur;
	char file[255], func[255];
	char type[20];
	struct map *mp;
	char **tt;

	amap = nmap = 0;
	while ((nf = fscanf(f, "%ld %s %ld %s %hd %s\n", &bno, file,
			    &lno, &type, &occur, func)) != EOF) {
		if (nf != 6) {
			fprintf(stderr, "Syntax error in mapfile\n");
			exit(1);
		}

		if (nmap >= amap) {
			map = realloc(map, (amap + CHUNK) * sizeof(*map));
			amap += CHUNK;
		}
		mp = &map[nmap++];
		mp->bno = bno;
		mp->lno = lno;
		mp->ord = occur;
		mp->file = lookup(&strcache, file, &mp->bfile);
		mp->func = lookup(&strcache, func, NULL);
		for (tt = &types[0]; *tt; tt++) {
			if (strcmp(type, *tt) == 0)
				mp->type = tt - types;
		}
		if (tt == NULL) {
			fprintf(stderr, "Unknown type <%s>\n", type);
			exit(1);
		}
	}
}

/*
 * sort on file then line number
 */
int
mapcompare(const void *a1, const void *b1)
{
	struct map *a = (struct map *)a1;
	struct map *b = (struct map *)b1;
	
	/* since we share string tables we can just compare pointers */
	if (a->file == b->file)
		if (a->lno == b->lno)
			return (a->type == BFUNC) ? -1 : 1;
		else
			return a->lno - b->lno;
	else
		return strcmp(a->file, b->file);
}

void
readlog(FILE *f)
{
	int nf;
	auto long bno, a1, a2;
	struct log *lp;

	alog = nlog = 0;
	while ((nf = fscanf(f, "%ld %ld %ld\n", &bno, &a1, &a2)) != EOF) {
		if (nf != 3) {
			fprintf(stderr, "Syntax error in logfile\n");
			exit(1);
		}

		if (nlog >= alog) {
			log = realloc(log, (alog + CHUNK) * sizeof(*log));
			alog += CHUNK;
		}
		lp = &log[nlog++];
		lp->bno = bno;
		lp->a1 = a1;
		lp->a2 = a2;
	}
}

/*
 * sort on branch number
 */
int
logcompare(const void *a1, const void *b1)
{
	struct log *a = (struct log *)a1;
	struct log *b = (struct log *)b1;
	
	return a->bno - b->bno;
}

void
Usage(void)
{
	fprintf(stderr, "Usage:btell [-czw][-f func][-F file][-m mapfile] logfile...\n");
	fprintf(stderr, "\t-z	print only branches/functions never taken\n");
	fprintf(stderr, "\t-c	print only branches/functions taken\n");
	fprintf(stderr, "\t-w	print warnings for missing log entries\n");
	fprintf(stderr, "\t-f func limit output to branches within \"func\"\n");
	fprintf(stderr, "\t-F file limit output to branches within \"file\"\n");
	fprintf(stderr, "\t\tmultiple -f and -F options may be given\n");
	exit(1);
}

int
matchfile(struct map *mp)
{
	struct strcache *s;

	if (Fnames == NULL)
		return 1;

	/* XXX special handling for '/'s */
	for (s = Fnames; s; s = s->next) {
		/*
		 * if they provide a '/' in the name then match
		 * exactly only.
		 * Otherwise, match basename
		 */
		if (s->sl) {
			if (*(s->name) == *(mp->file) && strcmp(s->name, mp->file) == 0)
				return 1;
		} else if (*(s->name) == *(mp->bfile) && strcmp(s->name, mp->bfile) == 0) {
			return 1;
		}
	}
	return 0;
}

/*
 * match a function name - somewhat hueristic
 * XXX assumes that a func is always first in a file ..
 */
int
matchfunc(struct map *mp)
{
	static char *lastfunc = NULL;

	struct strcache *s;

	if (fnames == NULL)
		return 1;
	if (mp->type == BFUNC) {
		for (s = fnames; s; s = s->next) {
			if (strcmp(s->name, mp->func) == 0) {
				lastfunc = mp->func;
				return 1;
			}
		}
		lastfunc = NULL;
	} else if (lastfunc)
		return 1;

	return 0;
}
