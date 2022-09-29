#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <bstring.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/attributes.h>

/*
 * attrtest.c
 *
 * Run a fully automatic, random test of the attribute routines.
 */

#define	DOT_COUNT	100	/* print a '.' every X operations */

struct info {
	char	*name;
	char	*value;
	int	valuelen;
	short	namelen;
	short	exists;
} *table;

char *table_data;	/* char string storage for info table */

struct status {
	int	size;
	int	exists;
} *attrstat;

int good_gets, good_sets, good_creates, good_replaces;	/* good ops */
int good_lists, good_rms, good_tot;			/* good ops */

int bad_gets, bad_sets, bad_creates, bad_replaces;	/* bad ops */
int bad_lists, bad_rms, bad_tot;			/* bad ops */

int failures;		/* count of unexpected events */

int verbose;		/* print messages as we go along */
int rootflag;		/* all ops done in "root" namespace */

int testfd;		/* FD of test file */
int totalnames;		/* total number of attribute names in test */

char *list_buffer;	/* buffer to use for attr_list() operations */
int   list_bufsize;	/* size of buffer to use for attr_list() operations */

int	auto_list(void);
void	findattr_setup(void);
void	findattr_check(void);
int	findattr(char *, int);
int	auto_get(struct info *);
int	auto_set(struct info *);
int	auto_create(struct info *);
int	auto_replace(struct info *);
int	auto_remove(struct info *);

void	usage(void);

void
usage(void)
{
	printf("usage:\tattrtest [-S] [-R] [-l srcfile] [-i iterations]\n");
	printf("\t\t [-s seed] [-b buffersize] [-z] [-v] testfile\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *sourcefile, *testfile, *c;
	int iterations, zeroout, stopflag;
	int zone, op, pct_remove, pct_create, ch, i, retval, fd;
	struct stat statb;
	struct info *ip;
	int seed, linedots;

	linedots = zeroout = verbose = stopflag = 0;
	seed = (int)time(NULL) % 1000;
	iterations = 100000;
	list_bufsize = 512;
	sourcefile = "input";
	while ((ch = getopt(argc, argv, "l:i:s:b:zvSR")) != EOF) {
		switch (ch) {
		case 'l':	sourcefile = optarg;		break;
		case 's':	seed = atoi(optarg);		break;
		case 'i':	iterations = atoi(optarg);	break;
		case 'b':	list_bufsize = atoi(optarg);	break;
		case 'z':	zeroout++;			break;
		case 'v':	verbose++;			break;
		case 'S':	stopflag++;			break;
		case 'R':	rootflag++;			break;
		default:	usage();			break;
		}
	}
	if (optind != (argc-1)) {
		usage();
	}
	testfile = argv[optind];
	if ((list_buffer = malloc(list_bufsize)) == NULL) {
		perror("malloc");
		return 1;
	}

	/*
	 * Read in the source file.
	 */
	if (stat(sourcefile, &statb) < 0) {
		perror(sourcefile);
		usage();
		return 1;
	}
	if ((table_data = malloc(statb.st_size)) == NULL) {
		perror("calloc");
		return 1;
	}
	if ((fd = open(sourcefile, O_RDONLY)) < 0) {
		perror(sourcefile);
		return 1;
	}
	if (read(fd, table_data, statb.st_size) < 0) {
		perror(sourcefile);
		return 1;
	}
	close(fd);

	/*
	 * Allocate space for the info table and fill it in.
	 */
	totalnames = 0;
	for (c = table_data, i = 0; i < statb.st_size; c++, i++) {
		if (*c == '\n') {
			*c = 0;
			totalnames++;
		}
	}
	table = (struct info *)calloc(totalnames+1, sizeof(struct info));
	if (table == NULL) {
		perror("calloc");
		return 1;
	}
	ip = table;
	ip->name = c = table_data;
	for (i = 0; i < totalnames; c++) {
		if (*c == 0) {
			ip++;
			ip->name = ++c;
			i++;
		} else if (*c == '\t') {
			*c++ = 0;
			ip->value = c;
			ip->valuelen = 1;
		} else if (ip->value == 0) {
			ip->namelen++;
		} else {
			ip->valuelen++;
		}
	}

	if ((testfd = open(testfile, O_RDONLY)) < 0) {
		perror(testfile);
		return 1;
	}

	/*
	 * Run random transactions against the attribute list.
	 */
	failures = 0;
	zone = -1;
	printf("Seed = %d (use \"-s %d\" to re-execute this test)\n",
		     seed, seed);
	srandom(seed);
	for (i = 0; i < iterations; i++) {
		/*
		 * The distribution of transaction types changes over time.
		 * At first we have an equal distribution which gives us
		 * a steady state attribute list of 50% total size.
		 * Later, we have an unequal distribution which gives us
		 * more creates than removes, growing the attribute list.
		 * Later still, we have an unequal distribution which gives
		 * us more removes than creates, shrinking the attribute list.
		 */
		if ((i % totalnames) == 0) {
			zone++;
			switch(zone % 3) {
			case 0: pct_remove = 15; pct_create = 60; break;
			case 1: pct_remove = 25; pct_create = 33; break;
			case 2: pct_remove = 60; pct_create = 20; break;
			}
		}

		/*
		 * Choose an operation based on the current distribution.
		 */
		ip = &table[ random() % totalnames ];
#undef GROT
#ifdef GROT
{
    static struct info *ptr11;
    char getbuffer[64];
    int getlen, fail;

    if (strcmp(ip->name, "0bpqueue") == 0) {
	ptr11 = ip;
    }
    fail = 0;
    getlen = sizeof(getbuffer);
    if (attr_getf(testfd, "0bpqueue", getbuffer, &getlen,
			  rootflag ? ATTR_ROOT : 0) < 0) {
	if (ptr11) {
	    perror("attr_getf");
	    if (ptr11->exists) {
		printf("it went away\n");
		fail++;
	    }
	}
    } else if (ptr11) {
	if (ptr11->valuelen != getlen) {
	    printf("it changed value lengths: from %d to %d\n",
		       ptr11->valuelen, getlen);
	    fail++;
	}
	if (bcmp(ptr11->value, getbuffer, getlen) != 0) {
	    printf("it changed values: from %s to %s (%d valid bytes)\n",
		       ptr11->value, getbuffer, getlen);
	    fail++;
	}
	if (!ptr11->exists) {
	    printf("it magically appeared\n");
	    fail++;
	}
    } else {
	printf("it appeared before our first operation\n");
	fail++;
    }
    if (fail && stopflag) {
	printf("stop here\n");
	return 32;
    }
}
#endif /* GROT */
		op = random() % 100;
		if (op > (pct_remove + pct_create)) {
			op = random() % 500;
			if (op == 0) {
				retval = auto_list();
			} else {
				retval = auto_get(ip);
			}
		} else if (op > pct_remove) {
			op = random() % 100;
			if (op < 20) {
				retval = auto_create(ip);
			} else if (op > 80) {
				retval = auto_replace(ip);
			} else {
				retval = auto_set(ip);
			}
		} else {
			retval = auto_remove(ip);
		}
		if (retval && stopflag) {
			return 1;
		}

		if ((i % DOT_COUNT) == 0) {
			if (linedots++ == 72) {
				linedots = 0;
				write(1, "\n", 1);
			}
			write(1, ".", 1);
			fflush(stdout);
		}
	}
	printf("\n");

	printf("get ops    : %6d OK, %6d ENOATTR (%6d total, %2d%% ENOATTR)\n",
		    good_gets, bad_gets, good_gets + bad_gets,
		    (good_gets+bad_gets)
		    ? (bad_gets*100) / (good_gets+bad_gets)
		    : 0);
	printf("list ops   : %6d OK, %6d w/error (%6d total, %2d%% w/error)\n",
		     good_lists, bad_lists, good_lists + bad_lists,
		     (good_lists+bad_lists)
		     ? (bad_lists*100) / (good_lists+bad_lists)
		     : 0);
	printf("set ops    : %6d OK, %6d w/error (%6d total, %2d%% w/error)\n",
		    good_sets, bad_sets, good_sets + bad_sets,
		    (good_sets+bad_sets)
		    ? (bad_sets*100) / (good_sets+bad_sets)
		    : 0);
	printf("create ops : %6d OK, %6d EEXIST  (%6d total, %2d%% EEXIST)\n",
		       good_creates, bad_creates, good_creates + bad_creates,
		       (good_creates+bad_creates)
		       ? (bad_creates*100)/(good_creates+bad_creates)
		       : 0);
	printf("replace ops: %6d OK, %6d ENOATTR (%6d total, %2d%% ENOATTR)\n",
			good_replaces, bad_replaces, good_replaces+bad_replaces,
			(good_replaces+bad_replaces)
			? (bad_replaces*100) / (good_replaces+bad_replaces)
			: 0);
	printf("remove ops : %6d OK, %6d ENOATTR (%6d total, %2d%% ENOATTR)\n",
		       good_rms, bad_rms, good_rms + bad_rms,
		       (good_rms+bad_rms)
		       ? (bad_rms*100) / (good_rms+bad_rms)
		       : 0);
	good_tot = good_gets + good_lists + good_sets + good_creates +
			good_replaces + good_rms;
	bad_tot = bad_gets + bad_lists + bad_sets + bad_creates +
			bad_replaces + bad_rms;
	printf("total ops  : %6d OK, %6d w/error (%6d total, %2d%% w/error)\n",
		      good_tot, bad_tot, good_tot + bad_tot,
		      (good_tot + bad_tot)
		      ? (bad_tot*100) / (good_tot+bad_tot)
		      : 0);

	/*
	 * If asked to clear the directory out after the run,
	 * remove everything that is left.
	 */
	if (zeroout) {
		good_rms = bad_rms = 0;
		for (ip = table, i = 0; i < totalnames; ip++, i++) {
			if (!ip->exists)
				continue;
			retval = auto_remove(ip);
			if (retval < 0) {
				bad_rms++;
				if (errno == ENOATTR) {
					printf("Remove, should have existed: \"%s\" => \"%s\"(%ld)\n",
							ip->name, ip->value,
							ip->valuelen);
					failures++;
				} else {
					perror("attr_removef");
					printf("Remove, other error: \"%s\" => \"%s\"(%ld)\n",
							ip->name, ip->value,
							ip->valuelen);
					failures++;
				}
			} else {
				good_rms++;
			}

			if (((good_rms + bad_rms) % DOT_COUNT) == 0) {
				write(1, ".", 1);
				fflush(stdout);
			}
		}
		printf("\ncleanup    : %6d good removes and %6d bad removes\n",
				   good_rms, bad_rms);
	}
	printf("failures   : %6d bug occurrences\n", failures);
	return 0;
}

int
auto_list(void)
{
	attrlist_t *alist;
	attrlist_ent_t *aep;
	attrlist_cursor_t cursor;
	int retval, i;

	findattr_setup();
	bzero((char *)&cursor, sizeof(cursor));
	do {
		retval = attr_listf(testfd, list_buffer, list_bufsize,
					    rootflag ? ATTR_ROOT : 0, &cursor);
		if (retval < 0) {
			bad_lists++;
			retval = errno;
			perror("attr_listf");
			return(retval);
		}
		good_lists++;
		alist = (attrlist_t *)list_buffer;
		for (i = 0; i < alist->al_count; i++) {
			aep = ATTR_ENTRY(list_buffer, i);
			if (findattr(aep->a_name, aep->a_valuelen)) {
				printf("at index %d\n", i);
			}
		}
	} while (alist->al_more);
	findattr_check();

	return(0);
}

void
findattr_setup(void)
{
	if (attrstat == NULL) {
		attrstat = malloc(totalnames * sizeof(*attrstat));
		if (attrstat == NULL) {
			perror("malloc");
			exit(1);
		}
	}
	bzero(attrstat, totalnames * sizeof(*attrstat));
}

void
findattr_check(void)
{
	struct status *st;
	struct info *ip;
	int i;

	for (ip = table, st = attrstat, i = 0; i < totalnames; ip++, st++, i++){
		if (st->exists) { 
			if (ip->exists == 0) {
				printf("List: should not exist: \"%s\"\n",
					      ip->name);
				failures++;
			}
			if (ip->valuelen != st->size) {
				printf("List: length wrong: \"%s\" %d, should be %d\n",
					      ip->name, st->size, ip->valuelen);
				failures++;
			}
		} else if (ip->exists == 1) {
			printf("List: should exist: \"%s\"\n", ip->name);
			failures++;
		}
	}
}

int
findattr(char *name, int size)
{
	struct status *st;
	struct info *ip;
	int min, max, probe, i;

	min = 0;
	max = totalnames;
	while (max >= min) {
		probe = (max + min) / 2;
		ip = &table[probe];
		i = strcmp(name, ip->name);
		if (i < 0) {
			max = probe - 1;
		} else if (i > 0) {
			min = probe + 1;
		} else {
			st = &attrstat[probe];
			if (st->exists) {
				printf("Attribute already seen during a list: %s\n", name);
				if (st->size != size)
					printf("Attribute size doesn't match: %d bytes\n", size);
				failures++;
				return(1);
			}
			st->exists = 1;
			st->size = size;
			return(0);
		}
	}
	printf("Attribute name not found in internal table: %s (%d byte value)\n",
			  name, size);
	failures++;
	return(1);
}

int
auto_get(struct info *ip)
{
	static char *newvalue = NULL;
	int newvaluelen, retval;

#ifdef GROT
printf("GET    : %s\n", ip->name);
#endif /* GROT */
	if (newvalue == NULL) {
		newvalue = malloc(ATTR_MAX_VALUELEN);
		if (newvalue == NULL) {
			perror("malloc");
			exit(1);
		}
	}
	newvaluelen = ATTR_MAX_VALUELEN;
	bzero(newvalue, newvaluelen);
	retval = attr_getf(testfd, ip->name, newvalue, &newvaluelen,
				   rootflag ? ATTR_ROOT : 0);
	if (retval == 0) {
		good_gets++;
		retval = 0;
		if (!ip->exists) {
			printf("Get: should not exist: \"%s\" => \"%s\"(%d)\n",
				     ip->name, newvalue, newvaluelen);
			retval = 1;
			failures++;
		} else if (ip->valuelen != newvaluelen) {
			printf("Get: length wrong: \"%s\" => \"%s\"(%d), should be \"%s\"(%d)\n",
				     ip->name, newvalue, newvaluelen,
				     ip->value, ip->valuelen);
			retval = 1;
			failures++;
		} else if (strcmp(ip->value, newvalue) != 0) {
			printf("Get: value wrong: \"%s\" => \"%s\"(%d), should be \"%s\"(%d)\n",
				     ip->name, newvalue, newvaluelen,
				     ip->value, ip->valuelen);
			retval = 1;
			failures++;
		}
	} else if (errno == ENOATTR) {
		bad_gets++;
		retval = 0;
		if (ip->exists == 1) {
			printf("Get: should exist: \"%s\" => \"%s\"(%d)\n",
				     ip->name, ip->value, ip->valuelen);
			retval = 1;
			failures++;
		}
	} else {
		retval = errno;
		perror("attr_getf");
		printf("Get: other error: \"%s\" => \"%s\"(%ld)\n",
			    ip->name, ip->value, ip->valuelen);
		failures++;
	}
	return(retval);
}

int
auto_create(struct info *ip)
{
	int retval;

#ifdef GROT
printf("CREATE : %s\n", ip->name);
#endif /* GROT */
	retval = attr_setf(testfd, ip->name, ip->value, ip->valuelen,
				   ATTR_CREATE | (rootflag ? ATTR_ROOT : 0));
	if (retval == 0) {
		good_creates++;
		retval = 0;
		if (ip->exists == 1) {
			printf("Create: should have failed: \"%s\" => \"%s\"(%ld)\n",
					ip->name, ip->value, ip->valuelen);
			retval = 1;
			failures++;
		}
		ip->exists = 1;
	} else if (errno == EEXIST) {
		bad_creates++;
		retval = 0;
		if (ip->exists == 0) {
			printf("Create: should not have failed: \"%s\" => \"%s\"(%ld)\n",
					ip->name, ip->value, ip->valuelen);
			retval = 1;
			failures++;
		}
		ip->exists = 1;
	} else {
		retval = errno;
		perror("attr_setf");
		printf("Create: other error: \"%s\" => \"%s\"(%ld)\n",
				ip->name, ip->value, ip->valuelen);
		failures++;
	}
	return(retval);
}

int
auto_replace(struct info *ip)
{
	int retval;

#ifdef GROT
printf("REPLACE: %s\n", ip->name);
#endif /* GROT */
	retval = attr_setf(testfd, ip->name, ip->value, ip->valuelen,
				   ATTR_REPLACE | (rootflag ? ATTR_ROOT : 0));
	if (retval == 0) {
		good_replaces++;
		retval = 0;
		if (ip->exists == 0) {
			printf("Replace: should have failed: \"%s\" => \"%s\"(%ld)\n",
					 ip->name, ip->value, ip->valuelen);
			retval = 1;
			failures++;
		}
		ip->exists = 1;
	} else if (errno == ENOATTR) {
		bad_replaces++;
		retval = 0;
		if (ip->exists == 1) {
			printf("Replace: should not have failed: \"%s\" => \"%s\"(%ld)\n",
					 ip->name, ip->value, ip->valuelen);
			retval = 1;
			failures++;
		}
		ip->exists = 0;
	} else {
		retval = errno;
		perror("attr_setf");
		printf("Replace: other error: \"%s\" => \"%s\"(%ld)\n",
				 ip->name, ip->value, ip->valuelen);
		failures++;
	}
	return(retval);
}

int
auto_set(struct info *ip)
{
	int retval;

#ifdef GROT
printf("SET    : %s\n", ip->name);
#endif /* GROT */
	retval = attr_setf(testfd, ip->name, ip->value, ip->valuelen,
				   rootflag ? ATTR_ROOT : 0);
	if (retval == 0) {
		good_sets++;
		ip->exists = 1;
	} else if (errno != 0) {
		bad_sets++;
		retval = errno;
		perror("attr_setf");
		printf("Set: should not have failed: \"%s\" => \"%s\"(%ld)\n",
			     ip->name, ip->value, ip->valuelen);
		ip->exists = 0;
		failures++;
	} else {
		retval = errno;
		perror("attr_setf");
		printf("Set: other error: \"%s\" => \"%s\"(%ld)\n",
			     ip->name, ip->value, ip->valuelen);
		failures++;
	}
	return(retval);
}

int
auto_remove(struct info *ip)
{
	int retval;

#ifdef GROT
printf("REMOVE : %s\n", ip->name);
#endif /* GROT */
	retval = attr_removef(testfd, ip->name, rootflag ? ATTR_ROOT : 0);
	if (retval >= 0) {
		good_rms++;
		retval = 0;
		if (ip->exists == 0) {
			printf("Remove: should not have existed: \"%s\" => \"%s\"(%ld)\n",
					ip->name, ip->value, ip->valuelen);
			retval = 1;
			failures++;
		}
		ip->exists = 0;
	} else if (errno == ENOATTR) {
		bad_rms++;
		retval = 0;
		if (ip->exists == 1) {
			printf("Remove: should have existed: \"%s\" => \"%s\"(%ld)\n",
					ip->name, ip->value, ip->valuelen);
			retval = 1;
			failures++;
		}
		ip->exists = 0;
	} else {
		retval = errno;
		perror("attr_removef");
		printf("Remove: other error: \"%s\" => \"%s\"(%ld)\n",
				ip->name, ip->value, ip->valuelen);
		failures++;
	}
	return(retval);
}
