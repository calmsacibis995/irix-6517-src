#include <stdio.h>
#include <string.h>
#include <pwd.h>

#define Strsize		256
#define	DefSource	"/dev/tape"

char defsource[Strsize];


/* parsename (name, who, host, fname) -- parse a name into components
 *
 * The incoming name may be a promstyle name, e.g. tpsc(0,7,0) or
 * bootp()host:/fname, or an IPstyle name, e.g. guest@host:/fname, or
 * just a filename.
 *
 * Translate the name into the user, hostname, and filename components,
 * will zero-length strings for missing parts.
 */

parsename (name, who, host, fname)
	char		*name;
	char		*who;
	char		*host;
	char		*fname;
{
	static char	usrname [20];
	struct passwd	*pw;
	char		*p, *s;
	int		n1, n2, n3;

	*who = *host = '\0';
	p = strpbrk (name, "(:");
	if (p == NULL) {
		strcpy (fname, name);
		return;
	}
	if (*p == '(') {
		n1 = n2 = n3 = 0;
		for (s = p + 1; *s >= '0' && *s <= '9'; ++s)
			n1 = n1 * 10 + *s - '0';
		if (*s == ',') {
			for (s = s + 1; *s >= '0' && *s <= '9'; ++s)
				n2 = n2 * 10 + *s - '0';
			if (*s == ',') {
				for (s = s + 1; *s >= '0' && *s <= '9'; ++s)
					n3 = n3 * 10 + *s - '0';
			}
		}
		if (*s == ')') ++s;
		if (strncmp ("tpqic", name, p - name) == 0) {
			sprintf (fname, "/dev/mt/ts%dd%d", n1, n2);
		} else if (strncmp ("tpsc", name, p - name) == 0) {
			sprintf (fname, "/dev/mt/tps%dd%d", n1, n2);
		} else if (strncmp ("dksc", name, p - name) == 0) {
			sprintf (fname, "/dev/dsk/dks%dd%ds%d", n1, n2, n3);
		} else if (strncmp ("dkip", name, p - name) == 0) {
			sprintf (fname, "/dev/dsk/ips%dd%ds%d", n1, n2, n3);
		}
		if (strncmp ("bootp", name, p - name) != 0)
			return;
		name = s;
	}
	if ((p = strchr (name, ':')) != NULL) {
		if ((s = strchr (name, '@')) != NULL && s < p) {
			strncpy (who, name, s - name);
			who [s - name] = '\0';
			strncpy (host, s + 1, p - (s + 1));
			host [p - (s + 1)] = '\0';
		} else {
			if (*usrname == '\0') {
				if ((pw = getpwuid (geteuid ())) != NULL) {
					strncpy (usrname, pw->pw_name,
						sizeof (usrname));
					usrname [sizeof (usrname) - 1] = '\0';
				} else {
					strcpy (usrname, "guest");
				}
			}
			strcpy (who, usrname);
			strncpy (host, name, p - name);
			host [p - name] = '\0';
		}
		name = p + 1;
	}
	if ((p = strchr (name, '(')) != NULL) {
		strncpy (fname, name, p - name);
		fname [p - name] = '\0';
	} else {
		strcpy (fname, name);
	}
}

/* main () -- get default source name
 *
 * Fetch the kernel option "tapename" if available, and reconstitute the
 * appropriate source name from it.  Otherwise, use the last resort.
 */

main ()
{
	char		host [Strsize], who [Strsize], fname [Strsize];
	char		buff [Strsize];

	*defsource = '\0';
	if (sgikopt ("tapedevice", buff, sizeof (buff)) == 0) {
		parsename (buff, who, host, fname);
		if (*host != '\0') {
			sprintf (defsource, "%s %s", host, DefSource);
		} else {
			sprintf (defsource, "NULL %s", DefSource);
		}
	} 
	if (*defsource == '\0') {
		sprintf (defsource, "NULL %s", DefSource);
	}
	printf("%s", defsource);
}
