/**************************************************************************
 *                                                                        *
 *               Copyright (C) 1993, Silicon Graphics, Inc.               *
 *                                                                        *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *                                                                        *
 **************************************************************************/
#include <stdio.h>
#include <pwd.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <sys/capability.h>
#define MAX_UID_LINE 9

int strtouid(char *, int *);
char *uidtostr(int);

/*
 * If no uid argument is given, prints the default uid of the 
 * specified network interface.  If a username corresponding
 * with the uid is found in the passwd file, that name is printed
 * instead of the uid.
 *
 * If a uid argument is provided, and the user is root, the
 * default uid of the requested interface is set to the uid.
 * Both positive and negative numbers are accepted, as are
 * usernames found in both user.info and passwd.
 */

int
main( int argc, char * argv[] )
{
	int 		af = AF_INET;
	int		numval = 0;
	int		retval;
	int		s;
	struct ifreq 	ifr;
	uid_t		uid;
	cap_t		ocap;
	cap_value_t	capv = CAP_NETWORK_MGT;

	if (argc < 2) {
	    fprintf(stderr, "usage: ifuid interface\n");
	    fprintf(stderr, "       ifuid  interface uid | username\n");
	    exit(1);
	}
	
	s = socket(af, SOCK_DGRAM, 0);
	if (s < 0) {
	    perror("ifuid: socket");
	    exit(1);
	}
	bzero( &ifr, sizeof ifr );
	strncpy(ifr.ifr_name, argv[1], sizeof ifr.ifr_name);
	ifr.ifr_base = (void *)&uid;
	ifr.ifr_len = sizeof(uid);
	if (argc == 2) {
	    if (ioctl(s, SIOCGIFUID, (caddr_t)&ifr) < 0) {
		perror("ifuid: ioctl (SIOCGIFUID)");
		exit(1);
	    }
	    printf("%s\n", uidtostr(uid));
	    exit(0);
	}
	if (getuid(0)) {
	    fprintf(stderr, 
	    "Only the Superuser may change an interface uid.\n");
	    exit(1);
	}
	if ((retval = strtouid(argv[2], &numval)) == -1) {
	    fprintf(stderr, "bad argument %s\n", argv[2]);
	    exit(1);
	}
	uid = (uid_t)(numval);
	ocap = cap_acquire(1, &capv);
	if (ioctl(s, SIOCSIFUID, (caddr_t)&ifr) < 0) {
		cap_surrender(ocap);
		perror("ifuid: ioctl (SIOCSIFUID)");
		exit(1);
	}
	cap_surrender(ocap);
	exit(0);
}

/*
 * Converts a uid to a user name, if it's in the passwd
 * file.  Otherwise, converts it to a numeric string.
 */
char *
uidtostr(int uid)
{
    struct passwd *pwd;
    static char uidstring[MAX_UID_LINE];

    if (pwd = getpwuid(uid))
	sprintf(uidstring, "%s", pwd->pw_name);
    else
	sprintf(uidstring, "%d", uid);
    return(uidstring);
}

/*
 * Convert a user name or numeric string to a uid and pass back
 * that value in the second argument.  Positive and negative
 * numbers are accepted.  Returns -1 for non-alphanumeric 
 * characters and names not in both the passwd and user.info file.
 */
int 
strtouid(char *str, int *valp)
{
    register char *cp;
    struct passwd *pw;
    int is_name = 0;
    int is_neg = 0;

    cp = str;
    /*
     * '-' is OK as first char of numeric string only.
     */
    if (!isdigit(*cp) && !isalpha(*cp) && (*cp != '-'))	
	return(-1);            		
    if (*cp == '-') {
	if (!str[1])
	    return(-1);
	is_neg = 1;
    }
    if (isalpha(*cp))
	is_name++;
    for(cp = &str[1]; *cp; cp++) {
	if (!isdigit(*cp) && !isalpha(*cp))
	    return(-1);
	if (isalpha(*cp) && (is_name == 0)) {
	    is_name++;
	}
    }
    if (is_name) {	
	if (is_neg)
	    return(-1);
	if (pw = getpwnam(str)) {
	    *valp = pw->pw_uid;
	    return(0);
	}
	return(-1);
    }
    *valp = atoi(str);
    return(0);
}

