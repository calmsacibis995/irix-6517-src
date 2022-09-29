#ifndef lint
static char sccsid[] =  "@(#)bootparam_lib.c	1.1 88/03/07 4.0NFSSRC; from 1.2 87/10/14 SMI";
#endif

/*
 * Copyright (c) 1988 by Sun Microsystems, Inc.
 */

/*
 * Library routines of the bootparam server.
 */

#include <stdio.h>
#include <strings.h>
#include <rpcsvc/ypclnt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <syslog.h>

extern int debug;

#define iseol(c)	(c == '\0' || c == '\n' || c == '#')
#define issep(c)	(index(sep,c) != NULL)
#define isignore(c) (index(ignore,c) != NULL)

#define BOOTPARAMS	"/etc/bootparams"
#define	LINESIZE	512
#define NAMESIZE	256
#define DOMAINSIZE	256

static char domain[DOMAINSIZE];			/* NIS domain name */

/*
 * getline()
 * Read a line from a file.
 * What's returned is a cookie to be passed to getname
 */
char **
getline(line,maxlinelen,f)
	char *line;
	int maxlinelen;
	FILE *f;
{
	char *p;
	static char *lp;
	do {
		if (! fgets(line, maxlinelen, f)) {
			return(NULL);
		}
	} while (iseol(line[0]));
	p = line;
	for (;;) {	
		while (*p) {	
			p++;
		}
		if (*--p == '\n' && *--p == '\\') {
			if (! fgets(p, maxlinelen, f)) {
				break;	
			}
		} else {
			break;	
		}
	}
	lp = line;
	return(&lp);
}

/*
 * getname()
 * Get the next entry from the line.
 * You tell getname() which characters to ignore before storing them 
 * into name, and which characters separate entries in a line.
 * The cookie is updated appropriately.
 * return:
 *	   1: one entry parsed
 *	   0: partial entry parsed, ran out of space in name
 *    -1: no more entries in line
 */
int
getname(name, namelen, ignore, sep, linep)
	char *name;
	int namelen;
	char *ignore;	
	char *sep;
	char **linep;
{
	char c;
	char *lp;
	char *np;
	int maxnamelen;

	lp = *linep;
	do {
		c = *lp++;
	} while (isignore(c) && !iseol(c));
	if (iseol(c)) {
		*linep = lp - 1;
		return(-1);
	}
	np = name;
	while (! issep(c) && ! iseol(c) && np - name < namelen) {	
		*np++ = c;	
		c = *lp++;
	} 
	lp--;
	if (issep(c) || iseol(c)) {
		if (np - name != namelen) {
			*np = 0;
		}
		if (iseol(c)) {
			*lp = 0;
		} else {
			lp++; 	/* advance over separator */
		}
	} else {
		*linep = lp;
		return(0);
	}
	*linep = lp;
	return(1);
}

/*
 * getclntent reads the line buffer and returns a string of client entry
 * in NIS or the "/etc/bootparams" file. Called by bp_getclntent.
 */
static int
getclntent(lp, clnt_entry)
	register char **lp;			/* function input */
	register char *clnt_entry;		/* function output */
{
	char name[NAMESIZE];
	int append = 0;

	while (getname(name, sizeof(name), " \t", " \t", lp) >= 0) {
		if (!append) {
			strcpy(clnt_entry, name);
			append++;
		} else {
			strcat(clnt_entry, " ");
			strcat(clnt_entry, name);
		}
	}
	return (0);
}	

/*
 * getfileent returns the client entry in the "/etc/bootparams"
 * file given the client name.
 */
static int
getfileent(clnt_name, clnt_entry)
	register char *clnt_name;		/* function input */
	register char *clnt_entry;		/* function output */
{
	FILE *fp; 
	char line[LINESIZE];
	char name[NAMESIZE];
	char **lp;
	int reason;
 
	reason = -1;
	if ((fp = fopen(BOOTPARAMS, "r")) == NULL) {
		return (-1);
	}
	while (lp = getline(line, sizeof(line), fp)) {
		if ((getname(name, sizeof(name), " \t", " \t",
		    lp) >= 0) && (strcmp(name,clnt_name) == 0)) {
			if (getclntent(lp, clnt_entry) == 0)
				reason = 0;
			break;
		}
	}
	fclose(fp);
	return (reason);
}

/*
 * bp_getclntent returns the client entry in either the NIS map or
 * the "/etc/bootparams" file given the client name.
 */
int
bp_getclntent(clnt_name, clnt_entry)
	register char *clnt_name;		/* function input */
	register char *clnt_entry;		/* function output */
{
	char *val, *buf;
	int vallen;
	int reason;
	
	reason = getfileent(clnt_name, clnt_entry);
	if (!reason)
		return 0;
	if (useyp() && ((reason = yp_match(domain, "bootparams", clnt_name,
		     strlen(clnt_name), &val, &vallen) == 0))) {
		buf = val;
		reason = getclntent(&buf, clnt_entry);
		free(val);
		return(reason);
	}
}

/*
 * getclntkey reads the line buffer and returns a string of pathname
 * in NIS or the "/etc/bootparams" file. Called by bp_getclntkey.
 */
static int
aliasmatch(clnt_name, buf)
	register char *clnt_name;		/* function input */
	register char **buf;			/* function output */
{
	char name[NAMESIZE];
	char *val, *val1;
	int  reason;
	int  vallen;

	if (reason = yp_match(domain, "hosts.name", clnt_name,
		     strlen(clnt_name), &val, &vallen)) {
		if (debug)
			syslog(LOG_ERR, "aliasmatch: invalid host=%s",
				clnt_name);
		return(reason);
	}
	if (debug)
		syslog(LOG_ERR, "aliasmatch: host=%s", val);

	val1 = val;
	while (getname(name, sizeof(name), " \t", " \t", &val) >= 0) {
		if (debug)
			syslog(LOG_ERR, "aliasmatch: name=%s", name);

		reason = yp_match(domain, "bootparams", name,
		    		 strlen(name), buf, &vallen);
		if (!reason) {
			break;
		}
	}
	free(val1);
	return (reason);
}

/*
 * getclntkey reads the line buffer and returns a string of pathname
 * in NIS or the "/etc/bootparams" file. Called by bp_getclntkey.
 */
static int
getclntkey(lp, clnt_key, clnt_entry)
	register char **lp;			/* function input */
	register char *clnt_key;		/* function input */
	register char *clnt_entry;		/* function output */
{
	char name[NAMESIZE];
	char *cp;

	while (getname(name, sizeof(name), " \t", " \t", lp) >= 0) {
		if (debug)
			syslog(LOG_ERR, "getclntkey: name=%s", name);
		if ((cp = (char *)index(name, '=')) == 0) {
			if (debug)
				syslog(LOG_ERR, " no =");
			return (-1);
		}
		*cp++ = '\0';
		if (strcmp(name, clnt_key) == 0) {
			strcpy(clnt_entry, cp);
			return (0);
		}
	}
	if (strcmp(clnt_key, "dump") == 0) {
		/*
		 * This is a gross hack to handle the case where 
		 * no dump file exists in bootparams. The null
		 * server and path names indicate this fact to the
		 * client.
		 */
		strcpy(clnt_entry, ":");
		return (0);
	}
	return (-1);
}

/*
 * getfilekey returns the client's server name and its pathname from
 * the "/etc/bootparams" file given the client name and the key.
 */
static int
getfilekey(clnt_name, clnt_key, clnt_entry)
	register char *clnt_name;		/* function input */
	register char *clnt_key;		/* function input */
	register char *clnt_entry;		/* function output */
{
	FILE *fp; 
	char line[LINESIZE];
	char name[NAMESIZE];
	char **lp;
	int reason;
 
	reason = -1;
	if ((fp = fopen(BOOTPARAMS, "r")) == NULL) {
		if (debug)
			syslog(LOG_ERR,
				"getfilekey: open /etc/bootparams failed");
		return (-1);
	}
	while (lp = getline(line, sizeof(line), fp)) {
		if (debug)
			syslog(LOG_ERR, "getfilekey: line=%s", *lp);
		if ((getname(name, sizeof(name), " \t", " \t",
		    lp) >= 0) && (strcmp(name,clnt_name) == 0)) {
			if (getclntkey(lp, clnt_key, clnt_entry) == 0)
				reason = 0;
			break;
		}
	}
	fclose(fp);
	return (reason);
}

/*
 * bp_getclntkey returns the client's server name and its pathname from either
 * the NIS or the "/etc/bootparams" file given the client name and
 * the key.
 */
int
bp_getclntkey(clnt_name, clnt_key, clnt_entry)
	register char *clnt_name;		/* function input */
	register char *clnt_key;		/* function input */
	register char *clnt_entry;		/* function output */
{
	char *val, *buf;
	int vallen;
	int reason;
 
	if (useyp()) {
		if (debug)
			syslog(LOG_ERR, "yp_match(%s, bootparams, %s)",
				domain, clnt_name);
		if (reason = yp_match(domain, "bootparams", clnt_name,
		     strlen(clnt_name), &val, &vallen)) {
			if (debug)
				syslog(LOG_ERR, "bp_getclntkey: %d", reason);

			if (reason == YPERR_MAP)
				return(getfilekey(clnt_name,
						clnt_key, clnt_entry));

			/* try aliases */
			if (reason == YPERR_KEY) {
				reason = aliasmatch(clnt_name, &val);
			}

			if (reason) {
			    return(getfilekey(clnt_name,clnt_key,clnt_entry));
			}
		}
		buf = val;
		reason = getclntkey(&buf, clnt_key, clnt_entry);
		free(val);
		return(reason);
	}
	return(getfilekey(clnt_name, clnt_key, clnt_entry));
}

/*
 * Determine whether or not to use the NIS service to do lookups.
 */
static int initted;
static int usingyp;
static int
useyp()
{
	if (!initted) {
		getdomainname(domain, sizeof(domain));
		usingyp = !yp_bind(domain);
		initted = 1;
	}
	return (usingyp);
}

/*
 * Determine if a descriptor belongs to a socket or not
 */
issock(fd)
	int fd;
{
	struct stat st;

	if (fstat(fd, &st) < 0) {
		return (0);
	} 
	/*	
	 * SunOS returns S_IFIFO for sockets, while 4.3 returns 0 and
	 * does not even have an S_IFIFO mode.  Since there is confusion 
	 * about what the mode is, we check for what it is not instead of 
	 * what it is.
	 */
	switch (st.st_mode & S_IFMT) {
	case S_IFCHR:
	case S_IFREG:
	case S_IFLNK:
	case S_IFDIR:
	case S_IFBLK:
		return (0);
	default:	
		return (1);
	}
}
