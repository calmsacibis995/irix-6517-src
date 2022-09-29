#include <bstring.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/kmem.h>
#include <sys/stat.h>
#include <sys/attributes.h>

/*
 * Exercise the attribute storage mechanism in the kernel.
 */

#define	SETOP		1		/* do a SET operation */
#define	GETOP		2		/* do a GET operation */
#define	REMOVEOP	3		/* do a REMOVE operation */
#define	LISTOP		4		/* do a LIST operation */

#define	BUFSIZE		(64*1024)	/* buffer size for LIST operations */

void
usage(char *progname)
{
	fprintf(stderr, "usage:\t%s [-LRq] -s attrname [-V attrvalue] pathname\t# set value\n",
			progname);
	fprintf(stderr, "\t%s [-LRq] -g attrname pathname\t\t\t# get value\n",
			progname);
	fprintf(stderr, "\t%s [-LRq] -r attrname pathname\t\t\t# remove attr\n",
			progname);
	fprintf(stderr, "\t%s [-LRq] -l pathname\t\t\t\t# list attrs \n",
			progname);
	fprintf(stderr, "\t-s accepts a value from stdin and -g writes a value to stdout\n");
	exit(1);
}


int
main(int argc, char **argv)
{
	char *attrname, *attrvalue, *filename, *buffer;
	int attrlength;
	int opflag, ch, error, follow, verbose, rootflag, i;
	attrlist_t *alist;
	attrlist_ent_t *aep;
	attrlist_cursor_t cursor;

	/*
	 * Pick up and validate the arguments.
	 */
	verbose = 1;
	follow = opflag = rootflag = 0;
	attrname = attrvalue = NULL;
	while ((ch = getopt(argc, argv, "s:V:g:r:lqLR")) != EOF) {
		switch (ch) {
		case 's':
			if ((opflag != 0) && (opflag != SETOP)) {
				fprintf(stderr,
				    "Only one of -s, -g, -r, or -l allowed\n");
				usage(argv[0]);
			}
			opflag = SETOP;
			attrname = optarg;
			break;
		case 'V':
			if ((opflag != 0) && (opflag != SETOP)) {
				fprintf(stderr,
				    "-V only allowed with -s\n");
				usage(argv[0]);
			}
			opflag = SETOP;
			attrvalue = optarg;
			break;
		case 'g':
			if (opflag) {
				fprintf(stderr,
				    "Only one of -s, -g, -r, or -l allowed\n");
				usage(argv[0]);
			}
			opflag = GETOP;
			attrname = optarg;
			break;
		case 'r':
			if (opflag) {
				fprintf(stderr,
				    "Only one of -s, -g, -r, or -l allowed\n");
				usage(argv[0]);
			}
			opflag = REMOVEOP;
			attrname = optarg;
			break;
		case 'l':
			if (opflag) {
				fprintf(stderr,
				    "Only one of -s, -g, -r, or -l allowed\n");
				usage(argv[0]);
			}
			opflag = LISTOP;
			break;
		case 'L':
			follow++;
			break;
		case 'R':
			rootflag++;
			break;
		case 'q':
			verbose = 0;
			break;
		default:
			fprintf(stderr, "unrecognized option: %c\n", (char)ch);
			usage(argv[0]);
			break;
		}
	}
	if (optind != argc-1) {
		fprintf(stderr, "a filename to operate on is required\n");
		usage(argv[0]);
	}
	filename = argv[optind];

	/*
	 * Break out into option-specific processing.
	 */
	switch (opflag) {
	case SETOP:
		if (attrvalue == NULL) {
			attrvalue = malloc(ATTR_MAX_VALUELEN);
			if (attrvalue == NULL) {
				perror("malloc");
				exit(1);
			}
			attrlength =
				fread(attrvalue, 1, ATTR_MAX_VALUELEN, stdin);
		} else {
			attrlength = strlen(attrvalue);
		}
		error = attr_set(filename, attrname, attrvalue,
					   attrlength,
					   (!follow ? ATTR_DONTFOLLOW : 0) |
					   (rootflag ? ATTR_ROOT : 0));
		if (error) {
			perror("attr_set");
			fprintf(stderr, "Could not set \"%s\" for %s\n",
					attrname, filename);
			exit(1);
		}
		if (verbose) {
			printf("Attribute \"%s\" set to a %d byte value "
			       "for %s:\n",
			       attrname, attrlength, filename);
			fwrite(attrvalue, 1, attrlength, stdout);
			printf("\n");
		}
		break;

	case GETOP:
		attrvalue = malloc(ATTR_MAX_VALUELEN);
		if (attrvalue == NULL) {
			perror("malloc");
			exit(1);
		}
		attrlength = ATTR_MAX_VALUELEN;
		error = attr_get(filename, attrname, attrvalue,
					   &attrlength,
					   (!follow ? ATTR_DONTFOLLOW : 0) |
					   (rootflag ? ATTR_ROOT : 0));
		if (error) {
			perror("attr_get");
			fprintf(stderr, "Could not get \"%s\" for %s\n",
					attrname, filename);
			exit(1);
		}
		if (verbose) {
			printf("Attribute \"%s\" had a %d byte value for %s:\n",
				attrname, attrlength, filename);
		}
		fwrite(attrvalue, 1, attrlength, stdout);
		if (verbose) {
			printf("\n");
		}
		break;

	case REMOVEOP:
		error = attr_remove(filename, attrname,
					      (!follow ? ATTR_DONTFOLLOW : 0) |
					      (rootflag ? ATTR_ROOT : 0));
		if (error) {
			perror("attr_remove");
			fprintf(stderr, "Could not remove \"%s\" for %s\n",
					attrname, filename);
			exit(1);
		}
		break;

	case LISTOP:
		if ((buffer = malloc(BUFSIZE)) == NULL) {
			perror("malloc");
			exit(1);
		}
		bzero((char *)&cursor, sizeof(cursor));
		do {
			error = attr_list(filename, buffer, BUFSIZE,
					    (!follow ? ATTR_DONTFOLLOW : 0) |
					    (rootflag ? ATTR_ROOT : 0),
					    &cursor);
			if (error) {
				perror("attr_list");
				fprintf(stderr, "Could not list \"%s\" for %s\n",
						attrname, filename);
				exit(1);
			}

			alist = (attrlist_t *)buffer;
			for (i = 0; i < alist->al_count; i++) {
				aep = (attrlist_ent_t *)&buffer[ alist->al_offset[i] ];
				if (verbose) {
					printf("Attribute \"%s\" has a %d byte value for %s\n",
							    aep->a_name,
							    aep->a_valuelen,
							    filename);
				} else { 
					printf("%s\n", aep->a_name);
				}
			}
		} while (alist->al_more);
		break;

	default:
		fprintf(stderr,
			"At least one of -s, -g, -r, or -l is required\n");
		usage(argv[0]);
		break;
	}

	return(0);
}
