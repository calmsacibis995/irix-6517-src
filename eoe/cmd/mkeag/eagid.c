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
 */

#include <getopt.h>
#include <pwd.h>
#include <grp.h>
#include <mls.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/param.h>
#include <sys/mac_label.h>
#include <sys/inf_label.h>
#include <sys/capability.h>
#include <sys/eag.h>

char *program;

static void
fmt_cap(void *in)
{
	cap_proc_t *cap = (cap_proc_t *)in;
	int i;
	int j;

	if (!in) {
		printf("capability=N/A");
		return;
	}
	for (i = 0, j = 0; j < 8; j++)
		i += cap->cap_inheritable.cap_set[j];

	printf("capability=%s",
	    i ? sgi_cap_captostr(&cap->cap_inheritable) : "None");

}

static void
fmt_inf(void *in)
{
	inf_label *ip = (inf_label *)in;
	int i;

	if (!in) {
		printf("inflabel=N/A");
		return;
	}
	printf("inflabel=%d", ip->il_level);
	for (i = 0; i < ip->il_catcount; i++)
		printf(",%d", ip->il_list[i]);
}

static void
fmt_mac(void *in)
{
	mac_label *lp = (mac_label *)in;
	char *cp;

	if (!in) {
		printf("maclabel=N/A");
		return;
	}
	cp = mac_labeltostr(lp, NAME_OR_COMP);
	printf("maclabel=%s", cp ? cp : "<INVALID MAC LABEL>");
	if (cp)
		free(cp);

}

struct todo {
	char	*name;			/* attribute name */
	void	(*func)(void *);	/* function to call */
	mode_t	filetypes;		/* types for which it's appropriate */
} todo[] = {
	EAG_CAP_PROCESS,	fmt_cap,	S_IFMT,
	EAG_INF_PROC,		fmt_inf,	S_IFMT,
	EAG_MAC_PROC,		fmt_mac,	S_IFMT,
};

#define TODO_SIZE (sizeof(todo) / sizeof(struct todo))

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
	uid_t euid;
	uid_t ruid;
	gid_t egid;
	gid_t rgid;
	gid_t gids[NGROUPS_UMAX];
	struct todo *tp;
	struct passwd *pwd;
	struct group *grp;
	char buffer[1024];
	char *cp;

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
			for (i = 0, tp = todo; i < TODO_SIZE; i++, tp++)
				if ((size = strlen(tp->name)) > vflag)
					vflag = size;
			break;
		}
	}

#ifdef	SLOPPY
	if (pfp = popen("id", "r")) {
		if (fgets(buffer, 1024, pfp)) {
			if (cp = strchr(buffer, '\n'))
				*cp = '\0';
			printf("%s ", buffer);
		}
		pclose(pfp);
	}
#else	/* SLOPPY */
	euid = geteuid();
	ruid = getuid();
	egid = getegid();
	rgid = getgid();
	i = getgroups(NGROUPS_UMAX, gids);

	if (pwd = getpwuid(ruid))
		printf("uid=%s ", pwd->pw_name);
	else
		printf("uid=%d ", ruid);

	if (euid != ruid) {
		if (pwd = getpwuid(euid))
			printf("euid=%s ", pwd->pw_name);
		else
			printf("euid=%d ", euid);
	}

	if (grp = getgrgid(rgid))
		printf("gid=%s ", grp->gr_name);
	else
		printf("gid=%d ", rgid);

	if (egid != rgid) {
		if (grp = getgrgid(egid))
			printf("egid=%s ", grp->gr_name);
		else
			printf("egid=%d ", egid);
	}

	for (c = 0; c < i; c++) {
		if (gids[c] == egid || gids[c] == rgid)
			continue;
		if (grp = getgrgid(gids[c]))
			printf("group=%s ", grp->gr_name);
		else
			printf("group=%d ", gids[c]);
	}

#endif	/* SLOPPY */

	for (c = 0, tp = todo; c < TODO_SIZE; c++, tp++) {
		if (sgi_eag_getprocattr(tp->name, buffer) < 0)
			(tp->func)(NULL);
		else
			(tp->func)(buffer);
		printf(" ");
	}
	printf("\n");
	exit(0);
}
