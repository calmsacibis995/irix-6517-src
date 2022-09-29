/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1990, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/

/*
 * #ident "$Revision: 1.2 $"
 *
 * Display Plan G data.
 *
 *
 */

#include <getopt.h>
#include <pwd.h>
#include <grp.h>
#include <mls.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/mac_label.h>
#include <sys/inf_label.h>
#include <sys/acl.h>
#include <sys/capability.h>
#include <sys/eag.h>

char *program;

static void
fmt_acl(void *in)
{
	acl_eag_t *ap = (acl_eag_t *)in;
	char as[1024];
	int count;

	if (ap->acl_cnt < 1) {
		printf("NoACL");
		return;
	}
	if ((count = sgi_acl_acltostr(ap, as)) > 0) {
		printf("%s", as);
		return;
	}
	printf("BadACL");
}

static void
fmt_cap(void *in)
{
	cap_eag_t *cep = (cap_eag_t *)in;
	char *allowed;
	char *forced;

	allowed = sgi_cap_captostr(&cep->cap_allowed);
	forced = sgi_cap_captostr(&cep->cap_forced);
	printf("%s%s/%s", (cep->cap_flags & CAP_SET_EFFECTIVE) ? "+" : "-",
	    allowed ? allowed : "None", forced ? forced : "None");
	if (allowed)
		free(allowed);
	if (forced)
		free(forced);
}

static void
fmt_inf(void *in)
{
	inf_label *ip = (inf_label *)in;
	int i;

	printf("%d", ip->il_level);
	for (i = 0; i < ip->il_catcount; i++)
		printf(",%d", ip->il_list[i]);
}

static void
fmt_mac(void *in)
{
	mac_label label;
	char *cp;

	bcopy(in, &label, sizeof(mac_label));

	cp = mac_labeltostr(&label, NAME_OR_COMP);
	printf("%s", cp ? cp : "<INVALID MAC LABEL>");
	if (cp)
		free(cp);

}

struct todo {
	char	*name;			/* attribute name */
	void	(*func)(void *);	/* function to call */
	mode_t	filetypes;		/* types for which it's appropriate */
} todo[] = {
	EAG_ACL,		fmt_acl,	S_IFMT,
	EAG_DEFAULT_ACL,	fmt_acl,	S_IFDIR,
	EAG_CAP_FILE,		fmt_cap,	S_IFMT,
	EAG_INF,		fmt_inf,	S_IFMT,
	EAG_MAC,		fmt_mac,	S_IFMT,
};

#define TODO_SIZE (sizeof(todo) / sizeof(struct todo))

static void
fmt_all(char *path, int vflag, mode_t filetype)
{
	struct todo *tp;
	char buffer[1024];
	int c;
	int i;

	for (c = 0, tp = todo; c < TODO_SIZE; c++, tp++) {
		if (vflag && !(tp->filetypes & filetype))
			continue;
		if (vflag) {
			printf("\n\t%s:\t", tp->name);
			if (strlen(tp->name) < 8)
				printf("\t");
		}
		else
			printf(" ");

		if (sgi_eag_getattr(path, tp->name, buffer) < 0)
			printf("%s", vflag ? "N/A" : "*");
		else
			(tp->func)(buffer);
	}
}

static char *
pread_oneline(char *cmd)
{
	static char result[256];
	char *cp = result;
	FILE *fp;

	result[0] = '\0';

	if (fp = popen(cmd, "r")) {
		fgets(result, 255, fp);
		pclose(fp);
		if (cp = strchr(result, '\n'))
			*cp = '\0';
		for (cp = result; *cp == ' '; cp++)
			;
	}

	return (cp);
}

static void
print_one(char *path, int lflag, int vflag)
{
	struct stat sbuf;
	int i;

	printf("%s", path);
	i = lstat(path, &sbuf);
	if (lflag) {
#ifdef	SLOPPY
		char cmd[256];
		if (i)
			printf("%s", vflag ? "\n\tNot Available" :
			    " ********** * * * * * * *:*");
		else {
			sprintf(cmd,
			    "ls -ldi %s | sed -e 's/\\(.*\\)  *[^ ]*$/\\1/'%s",
			    path, vflag ? "" : " | sed -e 's/  */ /g'");
			printf(vflag ? "\n\t%s" : " %s", pread_oneline(cmd));
		}
#else	/* SLOPPY */
		if (i >= 0) {
			struct passwd *pwd = getpwuid(sbuf.st_uid);
			struct group *grp = getgrgid(sbuf.st_gid);
			char *when;
			char *tp;

			when = strdup(ctime(&sbuf.st_mtime));
			when[24] = '\0';
			for (tp = when; *tp == ' '; tp++)
				;

			if (vflag) {
				printf("\n\tinode:\t\t%d", sbuf.st_ino);
				printf("\n\tmode:\t\t%05o", 07777&sbuf.st_mode);
				printf("\n\tlinks:\t\t%d", sbuf.st_nlink);
				printf("\n\tuid:\t\t");
				if (pwd)
					printf("%s", pwd->pw_name);
				else
					printf("%d", sbuf.st_uid);
				printf("\n\tgid:\t\t");
				if (grp)
					printf("%s", grp->gr_name);
				else
					printf("%d", sbuf.st_gid);
				printf("\n\tsize:\t\t%d", sbuf.st_size);
				printf("\n\tmodified:\t%s", tp);
			}
			else {
				printf(" %d", sbuf.st_ino);
				printf(" %03o", 07777&sbuf.st_mode);
				printf(" %d", sbuf.st_nlink);
				if (pwd)
					printf(" %s", pwd->pw_name);
				else
					printf(" %d", sbuf.st_uid);
				if (grp)
					printf(" %s", grp->gr_name);
				else
					printf(" %d", sbuf.st_gid);
				printf(" %d", sbuf.st_size);
				printf(" %s", tp);
			}
			free(when);
		}
#endif	/* SLOPPY */
	}
	fmt_all(path, vflag, sbuf.st_mode);
	printf("\n");
}

static void
print_link(char *path, int lflag, int vflag)
{
	char *cp;
	char *arrow;
	char *space;
	char cmd[256];
	int c;
	int i;

	printf("%s", path);
	if (lflag) {
		sprintf(cmd,
"ls -li %s | sed -e 's/\\(.*\\) [^ ]* \\(-> [^ ]*\\)/\\1 \\2/'%s",
		    path, vflag ? "" : " | sed -e 's/  */ /g'");
		cp = pread_oneline(cmd);
		if (arrow = strstr(cp, " -> ")) {
			*arrow = '\0';
			arrow += 4;
		}
		if (vflag) {
			if (arrow)
				printf(" -> %s", arrow);
			printf("\n\t%s", cp);
		}
		else {
			if (arrow) {
				printf("->%s %s", arrow, cp);
			}
			else
				printf(" %s", cp);
		}
	}
	fmt_all(path, vflag, S_IFLNK);
	printf("\n");
}

static void
print_dir(char *path, int lflag, int vflag, int aflag)
{
	struct stat sbuf;
	char *prefix = (strcmp(path, "/")) ? path : "";
	char *cp;
	char cmd[256];
	char full[256];
	FILE *fp;


	sprintf(cmd, "/bin/ls %s%s", aflag ? "-a ": "", path);
	if (fp = popen(cmd, "r")) {
		*cmd = '\0';
		while (fgets(cmd, 255, fp)) {
			if (cp = strchr(cmd, '\n'))
				*cp = '\0';
			for (cp = cmd; *cp == ' '; cp++)
				;
			if (strlen(path)) {
				sprintf(full, "%s/%s", prefix, cp);
				cp = full;
			}
			if (lstat(cp, &sbuf) < 0)
				print_one(cp, lflag, vflag);
			else if (S_ISLNK(sbuf.st_mode))
				print_link(cp, lflag, vflag);
			else
				print_one(cp, lflag, vflag);
		}
		pclose(fp);
	}
}

main(int argc, char *argv[])
{
	extern char *optarg;
	extern int optind;
	int c;
	int i;
	int size;
	int aflag = 0;
	int dflag = 0;
	int lflag = 0;
	int vflag = 0;
	struct stat sbuf;

	program = argv[0];

	while ((c = getopt(argc, argv, "adlv")) != -1) {
		switch (c) {
		case 'a':
			aflag = 1;
			break;
		case 'd':
			dflag = 1;
			break;
		case 'l':
			lflag = 1;
			break;
		case 'v':
			vflag = 1;
			break;
		}
	}

	if (optind == argc) {
		print_dir("", lflag, vflag, aflag);
		exit(0);
	}
	
	for (; optind < argc; optind++) {
		if (lstat(argv[optind], &sbuf) < 0)
			print_one(argv[optind], lflag, vflag);
		else if (!dflag && S_ISDIR(sbuf.st_mode))
			print_dir(argv[optind], lflag, vflag, aflag);
		else if (S_ISLNK(sbuf.st_mode))
			print_link(argv[optind], lflag, vflag);
		else
			print_one(argv[optind], lflag, vflag);
	}
	exit(0);
}
