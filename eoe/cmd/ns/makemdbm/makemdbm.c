/* 
 * Copyright (c) 1985 by Sun Microsystems, Inc.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/file.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <ctype.h>
#include <mdbm.h>

#include "ypdefs.h"
USE_YP_MASTER_NAME
USE_YP_LAST_MODIFIED
USE_YP_INPUT_FILE
USE_YP_OUTPUT_NAME
USE_YP_DOMAIN_NAME
USE_YP_SECURE
USE_YP_INTERDOMAIN

static MDBM *db;

#define MAXLINE 4096		/* max length of input line */

void
usage(void)
{
	fprintf(stderr,
"usage: makemdbm -u file\n       makemdbm [-b][-l][-s][-i YP_INPUT_FILE][-o YP_OUTPUT_FILE][-d YP_DOMAIN_NAME][-m YP_MASTER_NAME] infile outfile\n");
	exit(1);
}


/* 
 * scans cp, looking for a match with any character
 * in match.  Returns pointer to place in cp that matched
 * (or NULL if no match)
 */
static char *
any(char *cp, char *match)
{
	register char *mp, c;

	while (c = *cp) {
		for (mp = match; *mp; mp++)
			if (*mp == c)
				return (cp);
		cp++;
	}
	return ((char *)0);
}

static char *
get_date(char *name)
{
	struct stat filestat;
	static char ans[MAX_ASCII_ORDER_NUMBER_LENGTH];/* ASCII numeric string*/

	if (strcmp(name, "-") == 0)
		sprintf(ans, "%010d", time(0));
	else {
		if (stat(name, &filestat) < 0) {
			fprintf(stderr, "makemdbm: can't stat %s\n", name);
			exit(1);
		}
		sprintf(ans, "%010d", filestat.st_mtime);
	}
	return ans;
}

void
addpair(char *str1, char *str2)
{
	datum key;
	datum content;
	
	key.dptr = str1;
	key.dsize = strlen(str1);
	content.dptr  = str2;
	content.dsize = strlen(str2);
	if (mdbm_store(db, key, content, MDBM_REPLACE) != 0){
		printf("makemdbm: problem storing %.*s %.*s\n",
		    key.dsize, key.dptr, content.dsize, content.dptr);
		exit(1);
	}
}

void
unmake(char *file)
{
	kvpair kv;
	int pagesize;
	char *key, *val;

	if (file == NULL)
		usage();
	
	db = mdbm_open(file, O_RDONLY, 0, 0);
	if (! db) {
		fprintf(stderr, "makemdbm: couldn't init %s\n", file);
		exit(1);
	}

	pagesize = MDBM_PAGE_SIZE(db);
	key = malloc(pagesize);
	val = malloc(pagesize);
	if (!key || !val) {
		fprintf(stderr, "makemdbm: malloc failed\n");
		exit(1);
	}

	for (kv.key.dptr=key, kv.key.dsize=pagesize, 
		     kv.val.dptr=val, kv.val.dsize=pagesize,
		     kv = mdbm_first(db,kv); 
	     kv.key.dptr != NULL; 
	     kv.key.dptr=key, kv.key.dsize=pagesize, 
		     kv.val.dptr=val, kv.val.dsize=pagesize,
		     kv = mdbm_next(db,kv)) {
		printf("%.*s %.*s\n", kv.key.dsize, kv.key.dptr,
		    kv.val.dsize, kv.val.dptr);
	}
	free(key);
	free(val);
}

main(int argc, char **argv)
{
	FILE *infp;
	kvpair kv;
	datum content;
	char buf[MAXLINE];
	char outbuf[MAXPATHLEN];
	char tmpoutbuf[MAXPATHLEN];
	char *p,ic;
	char *infile, *outfile;
	char *infilename, *outfilename, *mastername, *domainname,
	    *interdomain_bind, *security, *lower_case_keys;
	char local_host[MAX_MASTER_NAME];
	int cnt,i;
	int pagesize;
	char *val;

	infile = outfile = NULL; /* where to get files */
	/* name to imbed in database */
	infilename = outfilename = mastername = domainname = interdomain_bind =
	    security = lower_case_keys = NULL; 
	argv++;
	argc--;
	while (argc > 0) {
		if (argv[0][0] == '-' && argv[0][1]) {
			switch(argv[0][1]) {
				case 'i':
					infilename = argv[1];
					argv++;
					argc--;
					break;
				case 'o':
					outfilename = argv[1];
					argv++;
					argc--;
					break;
				case 'm':
					mastername = argv[1];
					argv++;
					argc--;
					break;
				case 'd':
					domainname = argv[1];
					argv++;
					argc--;
					break;
				case 'b':
					interdomain_bind = argv[0];
					break;
				case 'l':
					lower_case_keys = argv[0];
					break;
				case 's':
					security = argv[0];
					break;
				case 'u':
					unmake(argv[1]);
					argv++;
					argc--;
					exit(0);
				default:
					usage();
			}
		}
		else if (infile == NULL)
			infile = argv[0];
		else if (outfile == NULL)
			outfile = argv[0];
		else
			usage();
		argv++;
		argc--;
	}
	if (infile == NULL || outfile == NULL)
		usage();
	if (strcmp(infile, "-") != 0)
		infp = fopen(infile, "r");
	else
		infp = stdin;
	if (infp == NULL) {
		fprintf(stderr, "makemdbm: can't open %s\n", infile);
		exit(1);
	}
	strcpy(tmpoutbuf, outfile);
	strcat(tmpoutbuf, ".tmp");
	db = mdbm_open(tmpoutbuf, O_CREAT | O_TRUNC | O_RDWR, 0666, 0);
	if (! db) {
	    	fprintf(stderr, "makemdbm: can't create %s\n", tmpoutbuf);
		exit(1);
	}
	pagesize=MDBM_PAGE_SIZE(db);
	val=malloc(pagesize);
	if (!val) {
		fprintf(stderr, "makemdbm: malloc failed\n");
		exit(1);
	}

	strcpy(outbuf, outfile);
	while (fgets(buf, sizeof(buf), infp) != NULL) {
		/*
		** Check for line with nothing but whitespace
		*/
		p = buf;
		while(isspace(*p)) {
			p++;
		}
		if (p != buf && *p == NULL) {
			continue;
		}

		p = buf;

		cnt = strlen(buf) - 1; /* erase trailing newline */
		while (p[cnt-1] == '\\') {
			p+=cnt-1;
			if (fgets(p, sizeof(buf)-(p-buf), infp) == NULL)
				goto breakout;
			cnt = strlen(p) - 1;
		}
		p = any(buf, " \t\n");
		kv.key.dptr = buf;
		kv.key.dsize = p - buf;
		for (;;) {
			if (p == NULL || *p == NULL) {
				fprintf(stderr, "makemdbm: source file is garbage!\n");
				unlink(tmpoutbuf);   /* Remove the tmp files */
				exit(1);
			}
			if (*p != ' ' && *p != '\t')
				break;
			p++;
		}
		content.dptr = p;
		content.dsize = strlen(p) - 1; /* erase trailing newline */
		if (lower_case_keys) 
			for (i=0; i<kv.key.dsize; i++) {
				ic = *((char *)kv.key.dptr+i);
				if (isascii(ic) && isupper(ic)) 
					*((char *)kv.key.dptr+i) = tolower(ic);
			} 
		kv.val.dptr=val;
		kv.val.dsize=pagesize;
		kv.val = mdbm_fetch(db, kv);
		if (kv.val.dptr == NULL) {
			if (mdbm_store(db, kv.key, content, MDBM_REPLACE) != 0) {
				printf("problem storing %.*s %.*s\n",
				    kv.key.dsize, kv.key.dptr,
				    content.dsize, content.dptr);
				unlink(tmpoutbuf);   /* Remove the tmp files */
				exit(1);
			}
		}
#ifdef DEBUG
		else {
			printf("duplicate: %.*s %.*s\n",
			    kv.key.dsize, kv.key.dptr,
			    content.dsize, content.dptr);
		}
#endif
	}
   breakout:
	addpair(yp_last_modified, get_date(infile));
	if (infilename)
		addpair(yp_input_file, infilename);
	if (outfilename)
		addpair(yp_output_file, outfilename);
	if (domainname)
		addpair(yp_domain_name, domainname);
	if (security)
		addpair(yp_secure, "");
	if (interdomain_bind)
		addpair(yp_interdomain, "");
	if (!mastername) {
		gethostname(local_host, sizeof (local_host) - 1);
		mastername = local_host;
	}
	addpair(yp_master_name, mastername);

	if (rename(tmpoutbuf, outbuf) < 0) {
		perror("makemdbm: rename");
		unlink(tmpoutbuf);		/* Remove the tmp files */
		exit(1);
	}
	exit(0);
}
