/*
 * COPYRIGHT NOTICE
 * Copyright 1990, Silicon Graphics, Inc. All Rights Reserved.
 * This is UNPUBLISHED PROPRIETARY SOURCE CODE of Silicon Graphics, Inc.;
 * the contents of this file may not be disclosed to third parties, copied or 
 * duplicated in any form, in whole or in part, without the prior written 
 * permission of Silicon Graphics, Inc.
 *
 * RESTRICTED RIGHTS LEGEND:
 * Use, duplication or disclosure by the Government is subject to restrictions 
 * as set forth in subdivision (c)(1)(ii) of the Rights in Technical Data
 * and Computer Software clause at DFARS 252.227-7013, and/or in similar or 
 * successor clauses in the FAR, DOD or NASA FAR Supplement. Unpublished - 
 * rights reserved under the Copyright Laws of the United States.
 *
 */

/*
 * userck - verify correctness of user.info
 */

#ident	"$Revision: 1.5 $"

#include <stdio.h>
#include <strings.h>
#include <mls.h>
#include <sys/stat.h>

mac_label access_label =	{MSEN_HIGH_LABEL,	MINT_LOW_LABEL, };
mac_label userinfo_label =	{MSEN_ADMIN_LABEL,	MINT_HIGH_LABEL, };
mac_label passwd_label =	{MSEN_LOW_LABEL,	MINT_HIGH_LABEL, };

char *userinfo_path = 		"/secadm/auth/user.info";
char *passwd_path = 		"/secadm/etc/passwd";
char *passwd_link = 		"/etc/passwd";

char *root_name =		"root";
char *dbadmin_name =		"dbadmin";


main(int argc, char *argv[], char **envp)
{
	extern char *optarg;	/* For use by getopt(3) */
	extern int optind;	/* For use by getopt(3) */
	extern int optind;	/* For use by getopt(3) */
	int c;			/* For use by getopt(3) */
	int vflag = 0;		/* verbose mode flag */
	int result = 0;		/* exit code */
	char line[256];		/* line buffer */
	char *cp;		/* char manipulation */
	struct stat sbuf;	/* stat(2) result buffer */
	mac_label label;	/* The desired label value */
	mac_label *low_lp;	/* user's low label */
	mac_label *high_lp;	/* user's high label */
	userinfo_t *urp;	/* to look up user name */
	userinfo_t *wurp;	/* to look up user name */
	userinfo_t *root_ur;	/* user information for root */
	userinfo_t *dbadmin_ur;	/* user information for dbadmin */
	FILE *fp;		/* input from user.info */


	while ((c = getopt(argc, argv, "v")) != -1) {
		switch (c) {
		case 'v':
			vflag++;
			break;
		case '?':
		default:
			fprintf(stderr, "userck: unknown option '%c'\n", c);
			exit(1);
		}
	}

	if (getuid() != 0)
		fprintf(stderr, "userck: Without root privilege your results may not be accurate\n");
	else
		_sgi_setplabel(&access_label);
	
	/*
	 * Get entrys for expected user id's.
	 */
	if ((root_ur = getuserinfonam(root_name)) == NULL) {
		printf("userck: Expected entry for %s not found\n",
		    root_name);
		result = 1;
	}
	if ((dbadmin_ur = getuserinfonam(dbadmin_name)) == NULL) {
		printf("userck: Expected entry for %s not found\n",
		    dbadmin_name);
		result = 1;
	}

	/*
	 * Check the access control information of all three files.
	 *
	 * Do user.info first.
	 */
	if (lstat(userinfo_path, &sbuf) < 0) {
		printf("userck: failure stating %s\n", userinfo_path);
		result = 1;
	}
	else {
		if (!((root_ur && (sbuf.st_uid == root_ur->ur_uid)) ||
		    (dbadmin_ur && (sbuf.st_uid == dbadmin_ur->ur_uid)))) {
			if ((urp = getuserinfouid(sbuf.st_uid)) != NULL) {
				printf(
				"userck: Owner of %s is incorrectly %s.\n",
				userinfo_path, urp->ur_name);
				free(urp);
			}
			else
				printf(
				"userck: Owner of %s is incorrectly %d.\n",
				userinfo_path, sbuf.st_uid);
			result = 1;
		}
	}

	if (_sgi_getlabel(userinfo_path, &label) < 0) {
		printf("userck: failure getting label for %s\n",
		    userinfo_path);
		result = 1;
	}
	/*
	 * Do passwd file next
	 */
	if (lstat(passwd_path, &sbuf) < 0) {
		printf("userck: failure stating %s\n", passwd_path);
		result = 1;
	}
	else {
		if (!((root_ur && (sbuf.st_uid == root_ur->ur_uid)) ||
		    (dbadmin_ur && (sbuf.st_uid == dbadmin_ur->ur_uid)))) {
			if ((urp = getuserinfouid(sbuf.st_uid)) != NULL) {
				printf(
				"userck: Owner of %s is incorrectly %s.\n",
				passwd_path, urp->ur_name);
				free(urp);
			}
			else
				printf(
				"userck: Owner of %s is incorrectly %d.\n",
				passwd_path, sbuf.st_uid);
			result = 1;
		}
	}

	if (_sgi_getlabel(passwd_path, &label) < 0) {
		printf("userck: failure getting label for %s\n",
		    passwd_path);
		result = 1;
	}
	/*
	 * Do passwd link last
	 */
	if (lstat(passwd_link, &sbuf) < 0) {
		printf("userck: failure stating %s\n", passwd_link);
		result = 1;
	}
	else {
		if (!((root_ur && (sbuf.st_uid == root_ur->ur_uid)) ||
		    (dbadmin_ur && (sbuf.st_uid == dbadmin_ur->ur_uid)))) {
			if ((urp = getuserinfouid(sbuf.st_uid)) != NULL) {
				printf(
				"userck: Owner of %s is incorrectly %s.\n",
				passwd_link, urp->ur_name);
				free(urp);
			}
			else
				printf(
				"userck: Owner of %s is incorrectly %d.\n",
				passwd_link, sbuf.st_uid);
			result = 1;
		}
	}

	if (_sgi_getlabel(passwd_link, &label) < 0) {
		printf("userck: failure getting label for %s\n",
		    passwd_link);
		result = 1;
	}

	/*
	 * For each entry in user.info verify a valid return from
	 * each of getuserinfonam() and getuserinfoid().
	 */
	if ((fp = fopen(userinfo_path, "r")) == NULL) {
		printf("userck: cannot open %s\n", userinfo_path);
		result = 1;
	}
	for (c = 1; fp && fgets(line, 255, fp) != NULL; c++) {
		if (index(line, '\n') == NULL) {
			printf(
			    "userck: user.info line %d too long\n", c);
			result = 1;
		}
		if (line[0] == '#')
			continue;
		if ((cp = index(line, ':')) == NULL) {
			printf(
			    "userck: user.info line %d has no colons\n", c);
			result = 1;
			continue;
		}
		*cp++ = '\0';
		if ((cp = index(cp, ':')) != NULL)
			cp++;

		if ((urp = getuserinfonam(line)) == NULL) {
			printf(
    "userck: user %s has no passwd file entry (user.info line %d).\n",
			    line, c);
			result = 1;
		}
		else
			free(urp);

		if ((urp = getuserinfouid(atoi(cp))) == NULL) {
			printf(
    "userck: uid %d has no passwd file entry (user.info line %d).\n",
			    atoi(cp), c);
			result = 1;
		}
		else
			free(urp);
	}
	if (fp != NULL)
		fclose(fp);

	/*
	 * For each entry in passwd verify a valid return from
	 * each of getuserinfonam() and getuserinfoid().
	 */
	if ((fp = fopen(passwd_path, "r")) == NULL) {
		printf("userck: cannot open %s\n", passwd_path);
		result = 1;
	}
	for (c = 1; fp && fgets(line, 255, fp) != NULL; c++) {
		if (index(line, '\n') == NULL) {
			printf(
			    "userck: passwd line %d too long\n", c);
			result = 1;
		}
		if ((cp = index(line, ':')) == NULL) {
			printf(
			    "userck: passwd line %d has no colons\n", c);
			result = 1;
			continue;
		}
		*cp++ = '\0';
		if ((cp = index(cp, ':')) != NULL)
			cp++;

		if ((urp = getuserinfonam(line)) == NULL) {
			printf(
	"userck: user %s has no user.info file entry (passwd line %d).\n",
			    line, c);
			result = 1;
		}
		else {
			free(urp);

			if ((urp = getuserinfouid(atoi(cp))) == NULL) {
				printf(
	"userck: uid %d has no user.info file entry (passwd line %d).\n",
				    atoi(cp), c);
				result = 1;
			}
			else
				free(urp);
		}
	}
	if (fp != NULL)
		fclose(fp);

	/*
	 * For each entry in user.info verify
	 *	1. There is a password
	 *	2. The home directory exists and
	 *		it's at the user's lowest label, or
	 *		it's at least within the user's range
	 *	3. The user's shell is always accessable
	 *	4. The clearance range makes sense.
	 */
	if ((fp = fopen(userinfo_path, "r")) == NULL) {
		printf("userck: cannot open %s\n", userinfo_path);
		result = 1;
	}
	for (c = 1; fp && fgets(line, 255, fp) != NULL; c++) {
		if (index(line, '\n') == NULL)
			continue;
		if (line[0] == '#')
			continue;
		if ((cp = index(line, ':')) == NULL)
			continue;
		*cp++ = '\0';
		if ((urp = getuserinfonam(line)) == NULL)
			continue;

		if (strlen(urp->ur_passwd) == 0) {
			printf(
			    "userck: user %s has no password.\n",
			    urp->ur_name);
			result = 1;
		}
		/*
		 * Check the user's home directory.
		 */
		if (stat(urp->ur_dir, &sbuf) < 0) {
			printf(
			    "userck: user %s has no home \"%s\".\n",
			    urp->ur_name, urp->ur_dir);
			result = 1;
		}
		else {
			if ((sbuf.st_mode & S_IFDIR) == 0) {
				printf(
		"userck: user %s's home \"%s\" is not a directory.\n",
				    urp->ur_name, urp->ur_dir);
				result = 1;
			}
			if (sbuf.st_uid != urp->ur_uid) {
				if ((wurp=getuserinfouid(sbuf.st_uid)) == NULL){
					printf(
			"userck: user %s's home \"%s\" owned by %d.\n",
					    urp->ur_name, urp->ur_dir,
					    sbuf.st_uid);
				result = 1;
				}
				else {
					printf(
			"userck: user %s's home \"%s\" owned by %s.\n",
					    urp->ur_name, urp->ur_dir,
					    wurp->ur_name);
					free(wurp);
				result = 1;
				}
			}
		}
		if (_sgi_getlabel(urp->ur_dir, &label) >= 0) {
			switch (c = mac_clearedlbl(urp, &label)) {
			case MAC_CLEARED:
				break;
			case MAC_LBL_TOO_LOW:
				printf(
	"userck: user %s's home %s is below user's clearance.\n",
				    urp->ur_name, urp->ur_dir);
				break;
			case MAC_INCOMPARABLE:
				printf(
	"userck: user %s's home %s is not comparable to user's clearance.\n",
				    urp->ur_name, urp->ur_dir);
				break;
			case MAC_LBL_TOO_HIGH:
				printf(
	"userck: user %s's home %s is above user's clearance.\n",
				    urp->ur_name, urp->ur_dir);
				break;
			case MAC_MSEN_EQUAL:
				printf(
	"userck: user %s's home %s is at equal sensitivity.\n",
				    urp->ur_name, urp->ur_dir);
				break;
			case MAC_MINT_EQUAL:
				printf(
	"userck: user %s's home %s is at equal integrity.\n",
				    urp->ur_name, urp->ur_dir);
				break;
			case MAC_NULL_CLEARANCE:
			case MAC_BAD_USER_INFO:
				printf(
	"userck: clearance error while checking home directory of %s : \n\t\t%s\n",urp->ur_name,mac_clearance_error(c)); 
				break;
			default:
				printf(
	"userck: Program error (mac_clearedlbl returned bad value %d)\n",c);
				break;
			}
		}
		/*
		 * Check the user's shell.
		 */
		if (*urp->ur_shell == '\0') {
			printf("userck: user %s has no shell.\n", urp->ur_name);
			result = 1;
		}
		else if (stat(urp->ur_shell, &sbuf) < 0) {
			printf(
			    "userck: user %s's shell \"%s\" does not exist.\n",
			    urp->ur_name, urp->ur_shell);
			result = 1;
		}
		else {
			if ((sbuf.st_mode & S_IFREG) == 0) {
				printf(
		"userck: user %s's shell \"%s\" is not a file.\n",
				    urp->ur_name, urp->ur_shell);
				result = 1;
			}
			if ((sbuf.st_mode & 0111) == 0) {
				printf(
		"userck: user %s's shell \"%s\" is not executable.\n",
				    urp->ur_name, urp->ur_shell);
				result = 1;
			}
		}
		if (_sgi_getlabel(urp->ur_shell, &label) >= 0) {
			switch (c = mac_clearedlbl(urp, &label)) {
			case MAC_CLEARED:
			case MAC_LBL_TOO_LOW:
				break;
			case MAC_INCOMPARABLE:
				printf(
	"userck: user %s's shell %s is not comparable to user's clearance.\n",
				    urp->ur_name, urp->ur_shell);
				break;
			case MAC_LBL_TOO_HIGH:
				printf(
	"userck: user %s's shell %s is above user's clearance.\n",
				    urp->ur_name, urp->ur_shell);
				break;
			case MAC_MSEN_EQUAL:
				printf(
	"userck: user %s's shell %s is at equal sensitivity.\n",
				    urp->ur_name, urp->ur_shell);
				break;
			case MAC_MINT_EQUAL:
				printf(
	"userck: user %s's shell %s is at equal integrity.\n",
				    urp->ur_name, urp->ur_shell);
				break;
			case MAC_NULL_CLEARANCE:
			case MAC_BAD_USER_INFO:
				printf(
	"userck: clearance error while checking shell of %s : \n\t\t%s\n",urp->ur_name,mac_clearance_error(c)); 
				break;
			default:
				printf(
	"userck: Program error (mac_clearedlbl returned bad value %d)\n",c);
				break;
			}
		}
		/*
		 * To check to make sure the default login label is within
		 * the clearance range(s).
		 */

		if ((c = mac_cleared(urp, urp->ur_deflabel)) != MAC_CLEARED ) {
			switch (c) {
                        case MAC_BAD_REQD_LBL:
				if (strlen(urp->ur_deflabel) > 0)
				    printf(
	"userck: user %s's default label \"%s\" is bad.\n",
                                    urp->ur_name,urp->ur_deflabel);
                                break;
                        case MAC_LBL_TOO_LOW:
                                printf(
        "userck: user %s's default label \"%s\" is below their clearance.\n",
                                    urp->ur_name, urp->ur_deflabel);
                                break;
                        case MAC_INCOMPARABLE:
                                printf(
        "userck: user %s's default label \"%s\" is not comparable to user's clearance.\n",
                                    urp->ur_name, urp->ur_deflabel);
                                break;
                        case MAC_LBL_TOO_HIGH:
                                printf(
        "userck: user %s's default label \"%s\" is above their clearance.\n",
                                    urp->ur_name, urp->ur_deflabel);
                                break;
                        case MAC_MSEN_EQUAL:
                                printf(
        "userck: user %s's default label \"%s\" is at equal sensitivity.\n",
                                    urp->ur_name, urp->ur_deflabel);
                                break;
                        case MAC_MINT_EQUAL:
                                printf(
        "userck: user %s's default label \"%s\" is at equal integrity.\n",
                                    urp->ur_name, urp->ur_deflabel);
                                break;
                        case MAC_NULL_CLEARANCE:
                        case MAC_BAD_USER_INFO:
                                printf(
        "userck: clearance error while checking default label of %s : \n\t\t%s\n",urp->ur_name,mac_clearance_error(c)); 
                                break;
                        default:
                                printf(
        "userck: Program error (mac_clearedlbl returned bad value %d)\n",c);
                                break;
			}
		}
		free(urp);

	}
	if (fp != NULL)
		fclose(fp);

	/*
	 * Uniqueness checks in user.info file.
	 */
	if ((fp = fopen(userinfo_path, "r")) == NULL) {
		printf("userck: cannot open %s\n", userinfo_path);
		result = 1;
	}
	for (c = 1; fp && fgets(line, 255, fp) != NULL; c++) {
		FILE *nfp;
		char name[256];
		int uid;
		int cc;

		if (index(line, '\n') == NULL)
			continue;
		if (line[0] == '#')
			continue;
		if ((cp = index(line, ':')) == NULL)
			continue;
		*cp++ = '\0';

		if ((nfp = fopen(userinfo_path, "r")) == NULL) {
			printf("userck: cannot open %s\n", userinfo_path);
			result = 1;
			continue;
		}
		for (cc = 1; nfp && fgets(name, 255, nfp) != NULL; cc++) {
			if (index(name, '\n') == NULL)
				continue;
			if (name[0] == '#')
				continue;
			if ((cp = index(name, ':')) == NULL)
				continue;
			*cp++ = '\0';
			
			if (strcmp(name, line) == 0 && c != cc)
				printf(
		"userck: user.info has duplicate name %s at lines %d and %d.\n",
				    name, c, cc);

		}
		fclose(nfp);

	}
	if (fp != NULL)
		fclose(fp);

	/*
	 * Uniqueness checks in password file.
	 */
	if ((fp = fopen(passwd_path, "r")) == NULL) {
		printf("userck: cannot open %s\n", passwd_path);
		result = 1;
	}
	for (c = 1; fp && fgets(line, 255, fp) != NULL; c++) {
		FILE *nfp;
		char name[256];
		int uid;
		int cc;

		if (index(line, '\n') == NULL)
			continue;
		if (line[0] == '#')
			continue;
		if ((cp = index(line, ':')) == NULL)
			continue;
		*cp++ = '\0';
		if ((cp = index(cp, ':')) != NULL)
			cp++;

		uid = atoi(cp);

		if ((nfp = fopen(passwd_path, "r")) == NULL) {
			printf("userck: cannot open %s\n", passwd_path);
			result = 1;
			continue;
		}
		for (cc = 1; nfp && fgets(name, 255, nfp) != NULL; cc++) {
			if (index(name, '\n') == NULL)
				continue;
			if (name[0] == '#')
				continue;
			if ((cp = index(name, ':')) == NULL)
				continue;
			*cp++ = '\0';
			if ((cp = index(cp, ':')) != NULL)
				cp++;
			
			if (strcmp(name, line) == 0 && c != cc)
				printf(
		"userck: passwd has duplicate name %s at lines %d and %d.\n",
				    name, c, cc);
			if (uid == atoi(cp) && c != cc)
				printf(
		"userck: passwd has duplicate uid %d at lines %d and %d.\n",
				    uid, c, cc);
		}
		fclose(nfp);

	}
	if (fp != NULL)
		fclose(fp);
	exit(result);
}	

