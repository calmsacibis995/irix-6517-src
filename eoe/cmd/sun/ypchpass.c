/*
 * Copyright (C) 1990 Silicon Graphics, Inc.  All rights reserved.
 *
 * NAME
 *	ypchpass - change selected NIS passwd fields
 * SYNOPSIS
 *	ypchpass [-f fullname] [-h home] [-s shell] [name]
 * TODO
 *	password update with aging
 *	/etc/shells?
 */
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <errno.h>
#include <pwd.h>
#include <stdarg.h>
#include <string.h>
#include <rpc/rpc.h>
#include <rpcsvc/ypclnt.h>
#include <rpcsvc/yppasswd.h>

#define	MAXGECOSLEN	256	/* gecos sanity */

static char	*progname;
static char	badgecos[] = ":";
static char	badgecosfield[] = ":,";
static char	badhome[] = ": \t";
static char	badshell[] = ": *\t";
static char	byname[] = "passwd.byname";

static char	prifield[] = "pri=";
#define prifldlen	(sizeof(prifield)-1)
#define has_pri_field(str)	(!strncmp(prifield, str, prifldlen))
#define CHROOT	'*'	/* char. before shell to cause chroot to home */

static void		usage();
static int		wellformed(const char *, const char *, const char *);
static struct passwd	*ypgetpwnam(const char *);
static struct passwd	*ypgetpwuid(uid_t);
static int		interact(const struct passwd *, struct passwd *);
static int		fixargs(struct passwd *, struct passwd *);

main(int argc, char *argv[])
{
	struct yppasswd yppw;
	int ask, opt, uid, port;
	char *user, *master;
	struct passwd *pw;
	CLIENT *client;
	enum clnt_stat stat;
	struct timeval timeout;	/* NOT USED */
	extern char *optarg;
	extern int optind, optopt;

	/*
	 * Initialize.
	 */
	progname = argv[0];
	if (!_yellowup(1)) {
		fprintf(stderr, "%s: can't make use of NIS.\n",
			progname);
		exit(1);
	}

	/*
	 * Collect options, remembering to be interactive if there are none.
	 */
	bzero(&yppw, sizeof yppw);
	ask = 1;
	while ((opt = getopt(argc, argv, "f:h:s:")) != EOF) {
		switch (opt) {
		  case 'f':
			if (!wellformed(optarg, badgecos, "-f"))
				exit(2);
			if (has_pri_field(optarg)) {
				fprintf(stderr,
				    "%s: illegal \"%s\" in the \"-f\" field.\n",
				    progname, prifield);
				exit(2);
			}
			yppw.newpw.pw_gecos = optarg;
			break;
		  case 'h':
			if (!wellformed(optarg, badhome, "-h"))
				exit(2);
			yppw.newpw.pw_dir = optarg;
			break;
		  case 's':
			if (!wellformed(optarg, badshell, "-s"))
				exit(2);
			yppw.newpw.pw_shell = optarg;
			break;
		  default:
			usage();
		}
		ask = 0;
	}

	if (optind < argc - 1)
		usage();

	/*
	 * A given user can change an entry only if it has that user's uid.
	 * We don't trust root, nor do we want to send the current password
	 * in the clear unless we're changing it.
	 */
	uid = getuid();
	if (optind == argc) {
		pw = ypgetpwuid(uid);
		if (pw == 0) {
			fprintf(stderr, "%s: who are you?\n", progname);
			exit(2);
		}
		user = pw->pw_name;
	} else {
		user = argv[optind];
		pw = ypgetpwnam(user);
		if (pw == 0) {
			fprintf(stderr, "%s: %s: unknown user.\n",
				progname, user);
			exit(2);
		}
	}

	if (uid != pw->pw_uid) {
		fprintf(stderr, "%s: %s: %s.\n",
			progname, user, strerror(EACCES));
		exit(1);
	}

	/*
	 * Find our NIS master and make sure it's running a trustworthy
	 * NIS passwd daemon.
	 */
	if (yp_master(_yp_domain, byname, &master) != 0) {
		fprintf(stderr, "%s: can't find NIS master for %s in %s.\n",
			progname, byname, _yp_domain);
		exit(1);
	}
	port = getrpcport(master, YPPASSWDPROG, YPPASSWDVERS_NEWSGI,
			  IPPROTO_UDP);
	if (port == 0) {
		fprintf(stderr,"%s: %s is not running the NIS passwd daemon.\n",
			progname, master);
		exit(1);
	}
	if (port >= IPPORT_RESERVED) {
		fprintf(stderr,
	    "%s: the NIS passwd daemon on %s is not using a privileged port.\n",
			progname, master);
		exit(1);
	}

	/*
	 * Fill in the new passwd entry.  Pass an empty string for the old
	 * password as we are not changing pw_passwd.  If we were to attempt
	 * to change another uid's entry, we'd have to set oldpass.
	 */
	yppw.oldpass = "";
	yppw.newpw.pw_name = pw->pw_name;
	yppw.newpw.pw_passwd = pw->pw_passwd;

	if (ask) {
		if (!interact(pw, &yppw.newpw)) {
			printf("NIS passwd entry for %s not updated.\n", user);
			exit(1);
		}
	} else {
		if (!fixargs(pw, &yppw.newpw))
			exit(1);
	}

	if (yppw.newpw.pw_gecos == 0)
		yppw.newpw.pw_gecos = pw->pw_gecos;
	if (yppw.newpw.pw_dir == 0)
		yppw.newpw.pw_dir = pw->pw_dir;
	if (yppw.newpw.pw_shell == 0)
		yppw.newpw.pw_shell = pw->pw_shell;

#ifdef DEBUG
	printf("gecos = '%s'\n", yppw.newpw.pw_gecos);
	printf("dir   = '%s'\n", yppw.newpw.pw_dir);
	printf("shell = '%s'\n", yppw.newpw.pw_shell);
#else
	/*
	 * Call the NIS passwd daemon using Unix-flavor authentication.
	 */
	client = clnt_create(master, YPPASSWDPROG, YPPASSWDVERS_NEWSGI, "udp");
	if (client == 0) {
		fprintf(stderr, "%s: can't call NIS passwd daemon on %s.\n",
			progname, clnt_spcreateerror(master));
		exit(1);
	}
	client->cl_auth = authunix_create_default();
	stat = clnt_call(client, YPPASSWDPROC_UPDATE, xdr_yppasswd, &yppw,
			 xdr_int, &opt, timeout);
	if (stat != RPC_SUCCESS) {
		fprintf(stderr,
			"%s: call to NIS passwd daemon on %s failed: %s.\n",
			progname, master, clnt_sperrno(stat));
		exit(1);
	}

	/*
	 * The passwd daemon returns non-zero on error, zero on success.
	 */
	if (opt != 0) {
		fprintf(stderr, "%s: couldn't update NIS passwd entry.\n",
			progname);
		exit(1);
	}
	printf("NIS passwd entry for %s updated on %s.\n", user, master);
#endif
	exit(0);
	/*NOTREACHED*/
}

static int
wellformed(const char *contents, const char *badchars, const char *prompt)
{
	if (strpbrk(contents, badchars)) {
		fprintf(stderr,
			"%s: illegal character(s) in the \"%s\" field.\n",
			progname, prompt);
		return 0;
	}
	return 1;
}

static void
usage()
{
	fprintf(stderr,
		"usage: %s [-f fullname] [-h home] [-s shell] [user]\n",
		progname);
	exit(2);
}

static struct passwd *
ypgetpwnam(const char *name)
{
	char *val;
	int len;
	struct passwd *pw;

	if (yp_match(_yp_domain, byname, name, strlen(name), &val, &len) != 0)
		return 0;
	pw = (struct passwd *) _pw_interpret(val, len, 0);
	free(val);
	return pw;
}

static struct passwd *
ypgetpwuid(uid_t uid)
{
	char buf[8], *val;
	int cc, len;
	struct passwd *pw;

	cc = sprintf(buf, "%u", uid);
	if (yp_match(_yp_domain, "passwd.byuid", buf, cc, &val, &len) != 0)
		return 0;
	pw = (struct passwd *) _pw_interpret(val, len, 0);
	free(val);
	return pw;
}

static void	list(const struct passwd *, FILE *);
static int	collect(FILE *, const struct passwd *, struct passwd *);

static int
interact(const struct passwd *oldpw, struct passwd *newpw)
{
	int tfd, done, c;
	FILE *tfp;
	char *editor, buf[BUFSIZ];
	static char tmpname[] = "/tmp/ypchpassXXXXXX";

	/*
	 * Create a temporary file for editing the new entry's fields.
	 */
	if ((tfd = mkstemp(tmpname)) < 0 || (tfp = fdopen(tfd, "w+")) == 0) {
		fprintf(stderr, "%s: can't create temporary file: %s.\n",
			progname, strerror(errno));
		return 0;
	}

	/*
	 * List editable fields in the tmp file and prepare to edit it.
	 */
	list(oldpw, tfp);
	fflush(tfp);
	editor = getenv("EDITOR");
	if (editor == 0 || *editor == '\0')
		editor = "vi";
	sprintf(buf, "%s %s", editor, tmpname);

	/*
	 * Keep trying to collect good field contents from the edited file.
	 */
	done = 0;
	for (;;) {
		fclose(tfp);
		if (system(buf) != 0) {
			fprintf(stderr, "%s: edit failed.\n", progname);
			break;
		}

		if ((tfp = fopen(tmpname, "r+")) == 0) {
			fprintf(stderr, "%s: temporary file disappeared: %s\n",
			progname, strerror(errno));
			return 0;
		}
		if (collect(tfp, oldpw, newpw)) {
			done = 1;
			break;
		}
		printf("Re-edit user information? [y]: ");
		fflush(stdout);
		c = getchar();
		if (c == 'n')
			break;
		while (c != EOF && c != '\n')
			c = getchar();
	}
	fclose(tfp);
	unlink(tmpname);
	return done;
}

/*
 * Logical field descriptors for the data-driven list and collect routines.
 * Collect also uses the gecos sub-fields' "contents" members to accumulate
 * new values.
 */
static struct field {
	char	*prompt;
	int	(*save)(const char *, struct field *,
			struct passwd *, const struct passwd *);
	char	*badchars;
	char	*contents;
};

int savegecos(const char *, struct field *, struct passwd *,
    const struct passwd *);
int savehome(const char *, struct field *, struct passwd *,
    const struct passwd *);
int saveshell(const char *, struct field *, struct passwd *,
    const struct passwd *);

struct field fields[] = {
#define	FULLNAME	0
	{ "Full Name",		savegecos,	badgecosfield, },
#define	LOCATION	1
	{ "Location",		savegecos,	badgecosfield, },
#define	OFFICEPHONE	2
	{ "Office Phone",	savegecos,	badgecosfield, },
#define	HOMEPHONE	3
	{ "Home Phone",		savegecos,	badgecosfield, },
#define	HOMEDIRECTORY	4
	{ "Home Directory",	savehome,	badhome, },
#define	SHELL		5
	{ "Shell",		saveshell,	badshell, },
	{ 0, }
};

static int
prilen(register char *pri)
{
	register int len = 0;
	if (has_pri_field(pri)) {
		len = prifldlen;
		pri += len;
		if (*pri == '-') {
			pri++;
			len++;
		}
		while (isdigit(*pri)) {
			pri++;
			len++;
		}
	}
	return len;
}

static void
list(const struct passwd *pw, FILE *fp)
{
	char *p, buf[BUFSIZ];
	static char form[] = "%s: %s\n";
	static char comma[] = ",";
	int plen = prilen(pw->pw_gecos);
	
	fprintf(fp, "# Changing user information for %s.\n", pw->pw_name);
	p = strtok(strcpy(buf, pw->pw_gecos + plen), comma);
	fprintf(fp, form, fields[FULLNAME].prompt, p);
#define	nonull(p)	((p) ? (p) : "")
	p = strtok(0,  comma);
	fprintf(fp, form, fields[LOCATION].prompt, nonull(p));
	p = strtok(0,  comma);
	fprintf(fp, form, fields[OFFICEPHONE].prompt, nonull(p));
	p = strtok(0,  comma);
	fprintf(fp, form, fields[HOMEPHONE].prompt, nonull(p));
#undef	nonull
	fprintf(fp, form, fields[HOMEDIRECTORY].prompt, pw->pw_dir);
	fprintf(fp, form, fields[SHELL].prompt,
		*pw->pw_shell == CHROOT ? pw->pw_shell+1 : pw->pw_shell);
}

static int	savestr(const char *, char **);
static int	savestr2(int, const char *, const char *, char **);

static int
collect(FILE *fp, const struct passwd *opw, struct passwd *pw)
{
	struct field *f;
	char *p, buf[BUFSIZ];
	int n, l, len;
	int plen;

	/*
	 * Free old contents left over from failed edits.
	 */
	for (f = fields; f->prompt; f++) {
		if (f->contents) {
			free(f->contents);
			f->contents = 0;
		}
	}

	/*
	 * Read lines from fp skipping comments and saving field contents.
	 */
	n = l = 0;
	while (fgets(buf, sizeof buf, fp)) {
		n++;
		len = strlen(buf) - 1;
		if (buf[len] != '\n') {
			fprintf(stderr, "%s: line %d too long.\n",
				progname, n);
			return 0;
		}
		if (buf[0] == '#')
			continue;
		buf[len] = '\0';
		p = strchr(buf, ':');
		if (p == 0) {
			fprintf(stderr, "%s: line %d is corrupted.\n",
				progname, n);
			return 0;
		}
		*p++ = '\0';

		for (f = fields; ; f++) {
			if (f->prompt == 0) {
				fprintf(stderr,
					"%s: unrecognized field \"%s\".\n",
					progname, buf);
				return 0;
			}
			if (!strcasecmp(f->prompt, buf)) {
				while (isspace(*p))
					p++;
				if (!wellformed(p, f->badchars, f->prompt))
					return 0;
				if (!(*f->save)(p, f, pw, opw))
					return 0;
				break;
			}
		}
		l++;
	}

	/*
	 * If the user deleted all non-comment lines, list the defaults again.
	 */
	if (l == 0) {
		rewind(fp);
		list(opw, fp);
		fflush(fp);
		return 0;
	}

	/*
	 * Form a gecos field from its sub-fields.
	 */
	if (fields[FULLNAME].contents)
		len = sprintf(buf, "%s", fields[FULLNAME].contents);
	else
		len = 0;
	for (n = LOCATION; n <= HOMEPHONE; n++) {
		len += sprintf(&buf[len], ",%s",
			       fields[n].contents ? fields[n].contents : "");
	}
	if (len < 4)
		return 1;
	if (len > MAXGECOSLEN) {
		fprintf(stderr, "%s: gecos field too long.\n", progname);
		return 0;
	}
	if (has_pri_field(buf)) {
		fprintf(stderr, "%s: illegal \"%s\" field in gecos.\n",
			progname, prifield);
		return 0;
	}
	/*
	 * User might have deleted these prompt lines.
	 */
	if (pw->pw_shell == 0)
		pw->pw_shell = opw->pw_shell;
	if (pw->pw_dir == 0)
		pw->pw_dir = opw->pw_dir;

	plen = prilen(opw->pw_gecos);
	if (!strcmp(opw->pw_gecos+plen, buf)
	    && !strcmp(opw->pw_dir, pw->pw_dir)
	    && !strcmp(opw->pw_shell, pw->pw_shell)) {
		return 0;
	}
	/*
	 * If the original entry begins with the priority, disallow
	 * leading digits the user may have added. If the original priority 
	 * was lacking a value, remove the field because it's useless.
	 */
	if (plen > prifldlen) {
		if (isdigit(*buf)) {
			fprintf(stderr,"%s: illegal leading digits in gecos.\n",
			    progname);
			return 0;
		}
		return savestr2(plen, opw->pw_gecos, buf, &pw->pw_gecos);
	} else 
		return savestr(buf, &pw->pw_gecos);
}

int
savegecos(const char *p, struct field *f, struct passwd *pw,
    const struct passwd *opw)
{
	return savestr(p, &f->contents);
}

int
savehome(const char *p, struct field *f, struct passwd *pw,
    const struct passwd *opw)
{
	if (*p == '\0') {
		fprintf(stderr, "%s: empty home directory field.\n", progname);
		return 0;
	}
	return savestr(p, &pw->pw_dir);
}

int
saveshell(const char *p, struct field *f, struct passwd *pw,
    const struct passwd *opw)
{
	if (*opw->pw_shell == CHROOT) {
		if (*p == '\0') {
			pw->pw_shell = "*/bin/sh";
			return 1;
		}
		return savestr2(1, "*", p, &pw->pw_shell);
	} else {
		if (*p == '\0') {
			pw->pw_shell = "/bin/sh";
			return 1;
		}
		return savestr(p, &pw->pw_shell);
	}
}

static int
savestr(const char *s, char **sp)
{
	return savestr2(0, 0, s, sp);
}

static int
savestr2(int len1, const char *s1, const char *s2, char **sp)
{
	char *ns;

	ns = malloc(len1 + strlen(s2)+1);
	if (ns == 0) {
		fprintf(stderr, "%s: %s.\n", progname, strerror(errno));
		*sp = 0;
		return 0;
	}
	if (*sp)
		free(*sp);
	if (len1)
		(void) strncpy(ns, s1, len1);
	ns[len1] = '\0';
	*sp = strcat(ns, s2);
	return 1;
}

/*
 * Make sure the leading * in the shell field and the leading
 * priority in the gecos field are preserved.
 */
static int
fixargs(struct passwd *opw, struct passwd *pw)
{
	int plen;
	char *p;

	if (pw->pw_shell) {
		p = pw->pw_shell;
		pw->pw_shell = 0;
		if (!saveshell(p, 0, pw, opw))
			return 0;
	}

	if (pw->pw_gecos) {
		plen = prilen(opw->pw_gecos);
		p = pw->pw_gecos;
		if (isdigit(*p)) {
			fprintf(stderr,"%s: illegal leading digits in gecos.\n",
			    progname);
			return 0;
		}
		pw->pw_gecos = 0;
		if (!savestr2(plen, opw->pw_gecos, p, &pw->pw_gecos))
			return 0;
	}
	return 1;
}
