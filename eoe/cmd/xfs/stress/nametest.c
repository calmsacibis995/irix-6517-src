#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <math.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/errno.h>
#include <sys/types.h>
#include <sys/param.h>

/*
 * nametest.c
 *
 * Run a fully automatic, random test of the directory routines.
 */

#define	DOT_COUNT	100	/* print a '.' every X operations */

struct info {
	ino64_t	inumber;
	char	*name;
	short	namelen;
	short	exists;
} *table;

char *table_data;	/* char string storage for info table */

int good_adds, good_rms, good_looks, good_tot;	/* ops that suceeded */
int bad_adds, bad_rms, bad_looks, bad_tot;	/* ops that failed */

int verbose;

int	auto_lookup(struct info *);
int	auto_create(struct info *);
int	auto_remove(struct info *);

void	usage(void);

void
usage(void)
{
	printf("usage: nametest [-l srcfile] [-i iterations] [-s seed] [-z] [-v]\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	char *sourcefile, *c;
	int totalnames, iterations, zeroout;
	int zone, op, pct_remove, pct_create, ch, i, retval, fd;
	struct stat64 statb;
	struct info *ip;
	int seed, linedots;

	linedots = zeroout = verbose = 0;
	seed = (int)time(NULL) % 1000;
	iterations = 100000;
	sourcefile = "input";
	while ((ch = getopt(argc, argv, "l:i:s:zv")) != EOF) {
		switch (ch) {
		case 'l':	sourcefile = optarg;		break;
		case 's':	seed = atoi(optarg);		break;
		case 'i':	iterations = atoi(optarg);	break;
		case 'z':	zeroout++;			break;
		case 'v':	verbose++;			break;
		default:	usage();			break;
		}
	}

	/*
	 * Read in the source file.
	 */
	if (stat64(sourcefile, &statb) < 0) {
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
	for (i = 0; i < totalnames;  ) {
		if (*c++ == 0) {
			ip++;
			ip->name = c;
			i++;
		} else {
			ip->namelen++;
		}
	}
	for (ip = table, i = 0; i < totalnames; ip++, i++) {
		if (strncmp(ip->name, "touch ", strlen("touch ")) == 0) {
			ip->name += strlen("touch ");
			ip->namelen -= strlen("touch ");
		} else if (strncmp(ip->name, "rm ", strlen("rm ")) == 0) {
			printf("bad input file, \"rm\" cmds not allowed\n");
			return 1;
		} else if (strncmp(ip->name, "ls ", strlen("ls ")) == 0) {
			printf("bad input file, \"ls\" cmds not allowed\n");
			return 1;
		}
	}

	/*
	 * Run random transactions against the directory.
	 */
	zone = -1;
	printf("Seed = %d (use \"-s %d\" to re-execute this test)\n", seed, seed);
	srandom(seed);
	for (i = 0; i < iterations; i++) {
		/*
		 * The distribution of transaction types changes over time.
		 * At first we have an equal distribution which gives us
		 * a steady state directory of 50% total size.
		 * Later, we have an unequal distribution which gives us
		 * more creates than removes, growing the directory.
		 * Later still, we have an unequal distribution which gives
		 * us more removes than creates, shrinking the directory.
		 */
		if ((i % totalnames) == 0) {
			zone++;
			switch(zone % 3) {
			case 0: pct_remove = 20; pct_create = 60; break;
			case 1: pct_remove = 33; pct_create = 33; break;
			case 2: pct_remove = 60; pct_create = 20; break;
			}
		}

		/*
		 * Choose an operation based on the current distribution.
		 */
		ip = &table[ random() % totalnames ];
		op = random() % 100;
		if (op > (pct_remove + pct_create)) {
			retval = auto_lookup(ip);
		} else if (op > pct_remove) {
			retval = auto_create(ip);
		} else {
			retval = auto_remove(ip);
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

	printf("creates: %6d OK, %6d EEXIST  (%6d total, %2d%% EEXIST)\n",
			 good_adds, bad_adds, good_adds + bad_adds,
			 (good_adds+bad_adds)
				 ? (bad_adds*100) / (good_adds+bad_adds)
				 : 0);
	printf("removes: %6d OK, %6d ENOENT  (%6d total, %2d%% ENOENT)\n",
			 good_rms, bad_rms, good_rms + bad_rms,
			 (good_rms+bad_rms)
				 ? (bad_rms*100) / (good_rms+bad_rms)
				 : 0);
	printf("lookups: %6d OK, %6d ENOENT  (%6d total, %2d%% ENOENT)\n",
			 good_looks, bad_looks, good_looks + bad_looks,
			 (good_looks+bad_looks)
				 ? (bad_looks*100) / (good_looks+bad_looks)
				 : 0);
	good_tot = good_looks + good_adds + good_rms;
	bad_tot = bad_looks + bad_adds + bad_rms;
	printf("total  : %6d OK, %6d w/error (%6d total, %2d%% w/error)\n",
			 good_tot, bad_tot, good_tot + bad_tot,
			 (good_tot + bad_tot)
				 ? (bad_tot*100) / (good_tot+bad_tot)
				 : 0);

	/*
	 * If asked to clear the directory out after the run,
	 * reomve everything that is left.
	 */
	if (zeroout) {
		good_rms = 0;
		for (ip = table, i = 0; i < totalnames; ip++, i++) {
			if (!ip->exists)
				continue;
			good_rms++;
			retval = unlink(ip->name);
			if (retval < 0) {
				if (errno == ENOENT) {
					printf("\"%s\"(%lld) not removed, should have existed\n",
						      ip->name, ip->inumber);
				} else {
					printf("\"%s\"(%lld) on remove: ",
						      ip->name, ip->inumber);
					perror("unlink");
				}
			}

			if ((good_rms % DOT_COUNT) == 0) {
				write(1, ".", 1);
				fflush(stdout);
			}
		}
		printf("\ncleanup: %6d removes\n", good_rms);
	}
	return 0;
}

int
auto_lookup(struct info *ip)
{
	struct stat64 statb;
	int retval;

	retval = stat64(ip->name, &statb);
	if (retval >= 0) {
		good_looks++;
		retval = 0;
		if (ip->exists == 0) {
			printf("\"%s\"(%lld) lookup, should not exist\n",
				      ip->name, statb.st_ino);
			retval = 1;
		} else if (ip->inumber != statb.st_ino) {
			printf("\"%s\"(%lld) lookup, should be inumber %lld\n",
				      ip->name, statb.st_ino, ip->inumber);
			retval = 1;
		} else if (verbose) {
			printf("\"%s\"(%lld) lookup ok\n",
				ip->name, statb.st_ino);
		}
	} else if (errno == ENOENT) {
		bad_looks++;
		retval = 0;
		if (ip->exists == 1) {
			printf("\"%s\"(%lld) lookup, should exist\n",
				      ip->name, ip->inumber);
			retval = 1;
		} else if (verbose) {
			printf("\"%s\"(%lld) lookup ENOENT ok\n",
				ip->name, ip->inumber);
		}
	} else {
		retval = errno;
		printf("\"%s\"(%lld) on lookup: ", ip->name, ip->inumber);
		perror("stat64");
	}
	return(retval);
}

int
auto_create(struct info *ip)
{
	struct stat64 statb;
	int retval;

	retval = open(ip->name, O_RDWR|O_EXCL|O_CREAT, 0666);
	if (retval >= 0) {
		close(retval);
		good_adds++;
		retval = 0;
		if (stat64(ip->name, &statb) < 0) {
			perror("stat64");
			exit(1);
		}
		if (ip->exists == 1) {
			printf("\"%s\"(%lld) created, but already existed as inumber %lld\n",
				      ip->name, statb.st_ino, ip->inumber);
			retval = 1;
		} else if (verbose) {
			printf("\"%s\"(%lld) create new ok\n",
				ip->name, statb.st_ino);
		}
		ip->exists = 1;
		ip->inumber = statb.st_ino;
	} else if (errno == EEXIST) {
		bad_adds++;
		retval = 0;
		if (ip->exists == 0) {
			if (stat64(ip->name, &statb) < 0) {
				perror("stat64");
				exit(1);
			}
			printf("\"%s\"(%lld) not created, should not exist\n",
				      ip->name, statb.st_ino);
			retval = 1;
		} else if (verbose) {
			printf("\"%s\"(%lld) not created ok\n",
				ip->name, ip->inumber);
		}
		ip->exists = 1;
	} else {
		retval = errno;
		printf("\"%s\"(%lld) on create: ", ip->name, ip->inumber);
		perror("creat");
	}
	return(retval);
}

int
auto_remove(struct info *ip)
{
	int retval;

	retval = unlink(ip->name);
	if (retval >= 0) {
		good_rms++;
		retval = 0;
		if (ip->exists == 0) {
			printf("\"%s\"(%lld) removed, should not have existed\n",
				      ip->name, ip->inumber);
			retval = 1;
		} else if (verbose) {
			printf("\"%s\"(%lld) remove ok\n",
				ip->name, ip->inumber);
		}
		ip->exists = 0;
		ip->inumber = 0;
	} else if (errno == ENOENT) {
		bad_rms++;
		retval = 0;
		if (ip->exists == 1) {
			printf("\"%s\"(%lld) not removed, should have existed\n",
				      ip->name, ip->inumber);
			retval = 1;
		} else if (verbose) {
			printf("\"%s\"(%lld) not removed ok\n",
				ip->name, ip->inumber);
		}
		ip->exists = 0;
	} else {
		retval = errno;
		printf("\"%s\"(%lld) on remove: ", ip->name, ip->inumber);
		perror("unlink");
	}
	return(retval);
}
