#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include <unistd.h>
#include <sys/param.h>
#include <rpc/rpc.h>
#include <rpcsvc/yp_prot.h>
#include <rpcsvc/ypclnt.h>

static char* ypfindalias(char *ap);

static char *dflt_aliases = "/etc/rfind.aliases";
static char buf[MAXHOSTNAMELEN + PATH_MAX];

static char *lgets (FILE *fp, char *bp, unsigned size) {
	register int c;
	register char *cs;

	cs = bp;
	while ((--size > 0) && ((c = getc (fp)) >= 0)) {
		if (c == '\n')
			goto newline;
		*cs++ = c;
	}
	if ((c < 0) && (cs == bp))
		return NULL;
	if (c > 0)
		while ((c = getc (fp)) >= 0)
			if (c == '\n')
				break;
newline:
	*cs++ = '\0';
	return bp;
}

/*
 * The format for an rfind.aliases file is:
 *	1) Comments extend from first '#' char on a line to the end of the line.
 *	2) Only the first two fields on a line are considered
 *	3) Spaces and tabs separate fields
 *	4) The first field is taken as an alias name
 *	5) The second field is taken as the <hostname>:<mount-point> for that alias.
 *	6) Lines with less than two fields (after comment trimming) are ignored.
 *	7) The first field will tested for equality with the
 *		first argument <file-system> supplied to rfind.
 *	8) If the first field matches, then the second field must have:
 *		a] a network valid hostname
 *		b] the ':' colon character, and
 *		c] a file system mount point.
 */

static char *findalias(char *fnp, char *ap) {
	FILE *fp;
	char *bp = NULL;

	if ((fp = fopen (fnp, "r")) == NULL)
		return NULL;

	while (lgets (fp, buf, sizeof(buf))) {
		char *fld1, *fld2, *px;

		if ((px = strchr(buf,'#')) != NULL) *px = 0;	/* trim comment */

		if ((fld1 = strtok (buf, " \t")) == NULL)	/* first field */
			continue;
		if ((fld2 = strtok (NULL, " \t")) == NULL)	/* second field */
			continue;

		if (strcmp (fld1, ap) == 0) {
			if (bp != NULL)
				free ((void *)bp);
			bp = strdup (fld2);
		}
	}

	fclose (fp);
	return bp;
}

struct {
	char *alias;
	char *server;
} servers[] = {
	{ "root", "localhost:/" },
	{ "usr", "localhost:/usr" },
	{ NULL, NULL}
};

/*
 * Translate the <file-system> first argument (ap) into canonical form:
 *	<host>:<pathname>
 *
 * 1) If first arg has ':' return untouched.
 * 2) If first arg is listed in the file $RFIND_ALIASES return match
 * 3) If first arg is listed in /etc/rfind.aliases return match
 * 4) If first arg is listed in NIS (YP) rfind.aliases map return match
 * 5) If first arg is one of "root" or "usr" return "localhost:/[usr]"
 * 6) If first arg is EFS or NFS file return full <host>:<remotepath>
 * 7) If all else fails return NULL
 */

char *getaliases (char *ap){
	char *env_aliases;
	char *bp;
	int i;
	extern char * pathcanon (char *);

	if (strchr (ap, ':') != NULL)
		return ap;

	env_aliases = getenv ("RFIND_ALIASES");

	if (env_aliases && (bp = findalias (env_aliases, ap)) != NULL)
		return bp;

	if ((bp = findalias (dflt_aliases, ap)) != NULL)
		return bp;

	if ((bp = ypfindalias(ap)) != NULL)
		return bp;

	for (i = 0; servers[i].alias != NULL; i++)
		if (strcmp (ap, servers[i].alias) == 0)
			return servers[i].server;

	if ((bp = pathcanon (ap)) != NULL)
		return bp;

	return NULL;
}

void showfile (FILE *fp) {
	while (lgets (fp, buf, sizeof(buf))) {
		char *fld1, *fld2, *px;

		if ((px = strchr(buf,'#')) != NULL) *px = 0;	/* trim comment */

		if ((fld1 = strtok (buf, " \t")) == NULL)	/* first field */
			continue;
		if ((fld2 = strtok (NULL, " \t")) == NULL)	/* second field */
			continue;

		fprintf (stderr, "\t%-12s %s\n", fld1, fld2);
	}
}

/*
 * Show known aliases to user, called if provided alias not recognized.
 */

void showaliases() {
	char *env_aliases;
	FILE *fp;

	if ((fp = fopen (dflt_aliases, "r")) != NULL) {
		fprintf (stderr, "Known aliases in <%s> are:\n", dflt_aliases);
		showfile (fp);
		(void) fclose (fp);
	}

	if ((env_aliases = getenv ("RFIND_ALIASES")) != NULL) {
		if ((fp = fopen (env_aliases, "r")) == NULL)
			return;
		fprintf (stderr, "Known aliases in $RFIND_ALIASES <%s> are:\n", env_aliases);
		showfile (fp);
		(void) fclose (fp);
	}
}

 char *
ypfindalias (char *key)
{
	char	domain_name[YPMAXDOMAIN];
	char	*val;
	int	len;

	if (getdomainname(domain_name, YPMAXDOMAIN))
		return(NULL);

	if (strlen(domain_name) == 0)
		return(NULL);

	if (yp_match(domain_name, "rfind.aliases", key, (int)strlen(key), &val, &len) != 0)
		return NULL;

	val[len] = '\0';	/* strip off \n at EOL or else no workee */
	return(val);
}

static int nloops = 0;

/* ARGSUSED */
int foreach (int status, char *key, int keylen, char *val, int vallen, char *foo) {
	if (status != YP_TRUE)
		return 1;
	if (nloops++ == 0)
		fprintf (stderr, "Known aliases in NIS (YP) rfind.aliases are:\n");

	key[keylen] = 0;
	val[vallen] = 0;
	fprintf (stderr, "\t%-12s %s\n", key, val);
	return 0;
}

struct ypall_callback callback = { foreach, 0 };

void ypshowaliases() {
	char    domain_name[YPMAXDOMAIN];

	if (getdomainname(domain_name, YPMAXDOMAIN))
		return;

	if (strlen(domain_name) == 0)
		return;

	yp_all (domain_name, "rfind.aliases", &callback);
}
