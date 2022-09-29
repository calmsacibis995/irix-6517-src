#ifndef lint
static char sccsid[] = "@(#)opt_software.c	1.10 88/03/08 Copyr 1987 Sun Micro";
#endif

/*
 * @(#)opt_software.c	1.1 88/06/08 4.0NFSSRC; from 1.10 88/03/08 D/NFS
 * Copyright (c) 1987 by Sun Microsystems, Inc.
 */

/*
 * Create the extractlist for given architecture
 *
 * Usage: opt_software arch tapedev tapehost
 */

#include "mktp.h"
#include <errno.h>
#include <string.h>
extern int errno;

#undef ALWAYS_GET_REQUIRED
#define EXTFILE "/tmp/EXTRACTLIST."
#define STRSZ 256
char *Prog;

main(argc,argv)
        int argc;
        char *argv[];
{
	char *arch, *tapedev, *tapehost;
	char buf[STRSZ], ans[STRSZ];
	char *swtype;
        FILE *ip;
        toc_entry *eptr;
        Information *infoptr;
        tapeaddress *tapeptr;
        int nentries, foundone;
        u_int software_size;
	int status;
	char *oneword();

	Prog = argv[0];
	if (argc != 3 && argc != 4)
		usage();

	arch = argv[1];
	tapedev = argv[2];
	if (argc == 3)
		tapehost = NULL;
	else {
		tapehost = argv[3];
		if (*tapehost == '\0')
			tapehost = NULL;
		if (tapehost && strncmp("/dev/", tapedev, 5)) {
			fprintf(stderr, "\
%s: tapedev must start with \"/dev/\" if tapehost if given.\n", Prog);
			usage();
		}
	}

	/*
         * Set up toc entry table
         */
	printf("Reading the table of contents for %s architecture...\n", arch);
	do {
tryagain:
		if (!strncmp("/dev/", tapedev, 5)) {
			for (;;) {
				printf("\n\
Mount a %s release tape%s%s and hit ENTER, or type \"exit\" : ",
					arch, tapehost? " on " : "",
					tapehost? tapehost : "");
				if (gets(ans) == NULL || !strcmp(ans, "exit")) {
					printf("\n%s: terminating\n", Prog);
					exit(1);
				}
				if (tapehost) {
					sprintf(buf, "\
rsh %s -n \"mt -f %s rew && mt -f %s fsf 1\" 2>&1", tapehost, tapedev, tapedev);
					ip = popen(buf, "r");
					if (ip == 0)
						fatal("run", buf);
					status = 0;
					while (fgets(ans, sizeof ans, ip)) {
						fputs(ans, stdout);
						status++;
					}
					pclose(ip);
				} else {
					sprintf(buf, "\
mt -t %s rew && mt -t %s fsf 1", tapedev, tapedev);
					status = system(buf);
				}
				if (status == 0)
					break;
				printf("\
Unable to position tape; command sent:\n    %s\n", buf);
			}
		}
        	if (tapehost) {
                	sprintf(buf, "rsh %s -n dd if=%s 2>/dev/null",
                        	tapehost, tapedev);
			ip = popen(buf, "r");
                	if (ip == NULL)
				fatal("run", buf);
        	} else {
			ip = fopen(tapedev, "r");
			if (ip == NULL)
				fatal("open", tapedev);
        	}
		nentries = read_xdr_toc(ip,&dinfo,&vinfo,entries,NENTRIES);
		if (tapehost)
			pclose(ip);
		else
			fclose(ip);
	} while (nentries == 0);
	/*
       	 * Start select software for the specified architecture
       	 */
	sprintf(buf, "%s%s", EXTFILE, arch);
	unlink(buf);
	ip = fopen(buf, "w");
	if (ip == NULL)
		fatal("create", buf);
	fprintf(ip, "IDENT %s %s\n", oneword(dinfo.Title), dinfo.Version);
	foundone = 0;
	for (eptr = entries; --nentries >= 0; eptr++) {
		infoptr = &eptr->Info;
		tapeptr = &eptr->Where.Address_u.tape;
		/*
		 *  Skip over files which are not installable or are
		 *  of the wrong architecture.
		 */
		if (infoptr->kind != INSTALLABLE
		    || !cmp_strlist(&infoptr->Information_u.Install.arch_for,
				    arch)) {
			continue;
                }
		/*
		 *  If this is the root entry, just write it out to
		 *  the extractlist.
		 */
		if (!strcmp(eptr->Name, "root")) {
			fprintf(ip,
				"ENTRY root %d %d / required not_movable %d\n",
				tapeptr->volno,
				tapeptr->file_number,
				tapeptr->size);
			continue;
		}
		if (foundone++ == 0) {
			printf("\n\
Select optional software for the %s architecture:\n", arch);
		}
		software_size = infoptr->Information_u.Install.size;
		if (infoptr->Information_u.Install.required)
			swtype = "required";
		else if (infoptr->Information_u.Install.desirable)
			swtype = "desirable";
		else if (infoptr->Information_u.Install.common)
			swtype = "common";
		else
			swtype = "optional";
		for (;;) {
			printf("Install \"%s\" [%d bytes; %s] ? [y/n]: ",
				eptr->LongName, software_size, swtype);
#ifdef ALWAYS_GET_REQUIRED
			if (infoptr->Information_u.Install.required) {
				printf("y\n");
				strcpy(ans, "y\n");
				break;
			}
#endif ALWAYS_GET_REQUIRED
			gets(ans);
			if (*ans == 'y' || *ans == 'n')
				break;
			printf("  Please answer \"yes\" or \"no\"\n");
		}
		if (*ans == 'y') {
			fprintf(ip, "ENTRY %s %d %d %s %s %s %d\n",
				eptr->Name,
				tapeptr->volno,
				tapeptr->file_number,
				infoptr->Information_u.Install.loadpoint,
				swtype,
				infoptr->Information_u.Install.moveable == TRUE?
					"movable" : "not_movable",
				tapeptr->size);
		}
	}
	fclose(ip);
	if (!foundone) {
		printf("\nRelease tape is not for %s architecture.\n", arch);
		if (strncmp("/dev/", tapedev, 5))
			exit(1);
		goto tryagain;
	}
	exit(0);
}

char *
oneword(s)
	char *s;
{
	char *p;

	p = s;
	while ((p = strpbrk(p, " \t")) != NULL)
		*p++ = '_';
	return s;
}

/*
 *  search the string_list looking for a match of s.  Returns
 *  1 if match, 0 if no match.
 */
cmp_strlist(list, s)
	string_list *list;
	char *s;
{
	int cnt;
	char **p;

	if (list == NULL)
		return 0;

	cnt = list->string_list_len;
	for (p = list->string_list_val; --cnt >= 0; p++) {
		if (!strcmp(*p, s))
			return 1;		/* found a match */
	}
	return 0;
}

usage()
{
	fprintf(stderr, "Usage: %s arch tapedev [tapehost]\n", Prog);
	exit(1);
}

fatal(verb, object)
	char *verb, *object;
{
	int error;

	error = errno;
	fprintf(stderr, "%s: Cannot %s ", Prog, verb);
	errno = error;
	perror(object);
	exit(1);
}
