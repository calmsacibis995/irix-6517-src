/*	Copyright (c) 1984, 1986, 1987, 1988, 1989 AT&T	*/
/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

/*      Portions Copyright (c) 1988, Sun Microsystems, Inc.     */
/*      All Rights Reserved.                                    */

#ident	"$Revision: 1.19 $"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pwd.h>
#include <grp.h>
#include <sys/param.h>
#include <sys/capability.h>
#include <sys/mac.h>
#include <unistd.h>
#include <locale.h>
#include <fmtmsg.h>
#include <sgi_nl.h>
#include <msgs/uxsgicore.h>

void id_usage(void);
void puid_dashu(uid_t);
void puid(char *, uid_t);
void pgid_dashg(gid_t);
void pgid(char *, gid_t);
void pGroups(int);
void pgroups(void);
void plabel(void);
void pcap(void);

char	cmd_label[] = "UX:id";

main(argc, argv)
int argc;
char **argv;
{
	struct passwd *pwptr = (struct passwd *)NULL;
	uid_t uid, euid;
	gid_t gid, egid;
	static char stdbuf[BUFSIZ];
	int c, aflag=0;
	int Mflag = 0;			/* Set iff -M in use */
	int Pflag = 0;			/* Set iff -P in use */
	int gflag = 0, nflag = 0, rflag = 0, uflag = 0, Gflag = 0, errflg = 0;
	char *labelstate;

	/*
	 * intnl support
	 */
	(void)setlocale(LC_ALL, "");
	(void)setcat("uxsgicore");
	(void)setlabel(cmd_label);

	while ((c = getopt(argc, argv, "GMPagnru")) != EOF) {
		switch(c) {
			case 'M':
				if (sysconf(_SC_MAC) > 0)
					Mflag++;
				break;
			case 'P':
				if (sysconf(_SC_CAP) > 0)
					Pflag++;
				break;
			case 'a': 
				aflag++;
				break;
			case 'g': 
				gflag++;
				break;
			case 'n': 
				nflag++;
				break;
			case 'r': 
				rflag++;
				break;
			case 'u': 
				uflag++;
				break;
			case 'G': 
				Gflag++;
				break;
			case '?': 
				errflg++;
		}
	}
	if (errflg) {
		id_usage();
		/*NOTREACHED*/
	}


	/* If env variable is on then show security labels */

	labelstate = getenv("LABELFLAG");
	if (labelstate && strcasecmp(labelstate,"on") == 0)
		Mflag = 1;

	setbuf (stdout, stdbuf);

	if (optind < argc) {
		if ((optind + 1) != argc) {
			id_usage();
			/*NOTREACHED*/
		}
		if (aflag) {
			id_usage();
			/*NOTREACHED*/
		}
		if ((pwptr = getpwnam(argv[optind]))
				== (struct passwd *)NULL) {
			_sgi_nl_error(SGINL_NOSYSERR, cmd_label,
				gettxt(_SGI_DMMX_id_nouser, 
					"id: user: %s does not exist"), 
						argv[optind]);
			exit(1);
		}
		uid = euid = pwptr->pw_uid;
		gid = egid = pwptr->pw_gid;
	} else {
		uid = getuid();
		gid = getgid();
		euid = geteuid();
		egid = getegid();
	}

	if (uflag) {
		if (!rflag) {
			if (!nflag)
				printf("%u\n", euid);
			else
				puid_dashu(euid);
		} else {
			if (!nflag)
				printf("%u\n", uid);
			else
				puid_dashu(uid);
		}
		exit(0);
	}
	if (gflag) {
		if (!rflag) {
			if (!nflag)
				printf("%u\n", egid);
			else
				pgid_dashg(egid);
		} else {
			if (!nflag)
				printf("%u\n", gid);
			else
				pgid_dashg(gid);
		}
		exit(0);
	}
	if(Gflag) {
		pGroups (nflag);
		exit(0);
	}

	puid ("uid", uid);
	pgid (" gid", gid);

	if (uid != euid)
		puid (" euid", euid);
	if (gid != egid)
		pgid (" egid", egid);
	if (aflag)
		pgroups ();
	if (Mflag)
		plabel();
	if (Pflag)
		pcap();
	putchar ('\n');
	exit(0);
}

void
id_usage(void)
{
	_sgi_nl_usage(SGINL_USAGE, cmd_label, 
		gettxt(_SGI_DMMX_id_usage1, "id [-aMP]"));
	_sgi_nl_usage(SGINL_USAGE, cmd_label, 
		gettxt(_SGI_DMMX_id_usage2, "id user"));
	_sgi_nl_usage(SGINL_USAGE, cmd_label, 
		gettxt(_SGI_DMMX_id_usage3, "id -G[-n] [user]"));
	_sgi_nl_usage(SGINL_USAGE, cmd_label, 
		gettxt(_SGI_DMMX_id_usage4, "id -g[-nr] [user]"));
	_sgi_nl_usage(SGINL_USAGE, cmd_label, 
		gettxt(_SGI_DMMX_id_usage5, "id -u[-nr] [user]"));
	exit(1);
}

void
puid_dashu(uid_t id)
{
	struct passwd *pw;

	pw = getpwuid(id);
	if (pw)
		printf ("%s\n", pw->pw_name);
}

void
puid(char *s, uid_t id)
{
	struct passwd *pw;

	printf ("%s=%ld", s, id);
	pw = getpwuid(id);
	if (pw)
		printf ("(%s)", pw->pw_name);
}

void
pgid_dashg(gid_t id)
{
	struct group *gr;

	setgrent();
	gr = getgrgid(id);
	if (gr)
		printf ("%s\n", gr->gr_name);
}


void
pgid(char *s, gid_t id)
{
	struct group *gr;

	printf ("%s=%ld", s, id);
	setgrent();
	gr = getgrgid(id);
	if (gr)
		printf ("(%s)", gr->gr_name);
}

void
pGroups(int nflag)
{
	gid_t groupids[NGROUPS_UMAX];
	gid_t *idp;
	struct group *gr;
	int i;

	i = getgroups(NGROUPS_UMAX, groupids);
	if (i > 0) {
		for (idp = groupids; i--; idp++) {
			if (!nflag)
				printf ("%u", *idp);
			else {
				setgrent();
				gr = getgrgid(*idp);
				if (gr)
					printf ("%s", gr->gr_name);
				else
					printf ("%u", *idp);
			}
			if(i)	printf(" ");
		}
		printf("\n");
	}
}

void
pgroups(void)
{
	gid_t groupids[NGROUPS_UMAX];
	gid_t *idp;
	struct group *gr;
	int i;

	i = getgroups(NGROUPS_UMAX, groupids);
	if (i > 0) {
		printf (" groups=");
		for (idp = groupids; i--; idp++) {
			printf ("%ld", *idp);
			setgrent();
			gr = getgrgid(*idp);
			if (gr)
				printf ("(%s)", gr->gr_name);
			if (i)
				putchar (',');
		}
	}
}

void
plabel(void)
{
	mac_t mac;
	char *lsp;

	if ((mac = mac_get_proc()) == NULL) {
		perror("mac_get_proc error");
		return;
	}
	
	if ((lsp = mac_to_text(mac, NULL)) == NULL)
		printf (" label=(unrecognized)");
	else
		printf (" label=(%s)", lsp);

	free(lsp);
	mac_free(mac);
}

void
pcap(void)
{
	cap_t capset;	/* capability set */
	char *cp;

	if ((capset = cap_get_proc()) == NULL) {
		perror("Capability state fetch error");
		return;
	}
	
	if ((cp = cap_to_text(capset, NULL)) == NULL)
		printf(" capability=(unrecognized)");
	else
		printf(" capability=(%s)", cp);

	cap_free(capset);
	free(cp);
}
