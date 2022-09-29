/*
 * Copyright 1996, Silicon Graphics, Inc. 
 * All Rights Reserved.
 *
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
 */

#ident  "$Revision: 1.2 $"

#include <sys/types.h>
#include <sys/mac.h>
#include <sys/mac_label.h>
#include <sys/capability.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <pwd.h>
#include <shadow.h>
#include <clearance.h>
#include <capability.h>
#include <crypt.h>
#include <unistd.h>

#define CMW_ANNOUNCE	"/usr/CMW/CMWannounce"
#define CMW_HOWTO	"/usr/CMW/CMWhowto"
#define CMW_UID0	"/usr/CMW/CMWuid0"
#define CMW_DISABLETIME	"/usr/CMW/CMWdisabletime"
#define CMW_IDLEWEEKS	"/usr/CMW/CMWidleweeks"

#define CMW_PASSWD	"/usr/CMW/CMWuser-passwd"
#define CMW_SITECHECK	"/usr/CMW/CMWuser-sitecheck"
#define CMW_USER_NAME	"/usr/CMW/CMWuser-name"
#define CMW_USER_MAC	"/usr/CMW/CMWuser-mac"
#define CMW_USER_CAP	"/usr/CMW/CMWuser-cap"

#define XSITECHECK	"/usr/bin/X11/CMWsitecheck"
#define XPASSWD		"/usr/bin/X11/CMWpasswd"
#define XCONFIRM	"/usr/bin/X11/xconfirm"
#define XWSH		"/usr/sbin/xwsh"

#define TERMINATORS	"#;\n"
#define LOGINDEFAULTS	"login"
#define CONSOLE		"/dev/console"

/*
 * XXX:casey These SITE_ defines ought to go into a header file
 */
#define SITE_OK		0
#define SITE_FAIL	1
#define SITE_AGAIN	2
#define SITE_CONTINUE	3

/*
 * From libcmd
 */
extern FILE *defopen(char *);
extern char *defread(FILE *, char *);

#define UNKNOWN	-1
#define UNSET	-2

#define DFLT_MAXTRYS	3
#define MAX_TIMEOUT	(15 * 60)
#define MAX_LOGFAILURES	20
#define MAX_SLEEPTIME	60

#define LOGFAILURE_BUFSIZE	100
#define LOGFAILURE_FILE		"/var/adm/loginlog"

char *program;

/*
 * Get an integer value from a defread string.
 */
static void
defvalue(FILE *deffp, char *name, int *valuep, int max)
{
	char *cp;

	if (*valuep != UNKNOWN)
		return;

	if ((cp = defread(deffp, name)) == NULL)
		*valuep = UNSET;
	else if ((*valuep = atoi(cp)) <= 0)
		*valuep = UNSET;
	else if (max != UNSET && *valuep > max)
		*valuep = max;
}


static void
xconfirm(char *file, char *message, char *header)
{
	FILE *fp;
	char *option = "-t";
	char *parameter;
	pid_t sitepid;
	int wstatus;
	cap_t ocap;
	cap_value_t cap_xtcb = CAP_XTCB;

	switch (sitepid = fork()) {
	case -1:
		return;
	case 0:
		if (file && (fp = fopen(file, "r"))) {
			fclose(fp);
			option = "-file";
			parameter = file;
		}
		else
			parameter = message;

		ocap = cap_acquire(1, &cap_xtcb);
		execl(XCONFIRM, "xconfirm", "-c", "-exclusive",
		    "-header", header ? header : " ",
		    "-t", " ",
		    option, parameter,
		    "-t", " ",
		    NULL);
		cap_surrender(ocap);
		exit(1);
	default:
		break;
	}

	(void) waitpid(sitepid, &wstatus, 0);
}

static void
xconfirm_disabletime(void)
{
	FILE *fp;
	char *option = "-t";
	char *parameter;
	cap_t ocap;
	cap_value_t cap_xtcb = CAP_XTCB;

	if (fork() != 0)
		return;

	if (fp = fopen(CMW_DISABLETIME, "r")) {
		fclose(fp);
		option = "-file";
		parameter = CMW_DISABLETIME;
	}
	else
		parameter = "System is locked.";

	ocap = cap_acquire(1, &cap_xtcb);
	execl(XCONFIRM, "xconfirm", "-c", "-exclusive",
	    "-header", "System Lockout Information",
	    "-t", " ",
	    option, parameter,
	    "-t", " ",
	    NULL);
	cap_surrender(ocap);

	exit(2);
}

static void
do_disabletime(FILE *deffp)
{
	static int disabletime = UNKNOWN;
	char *cp;

	defvalue(deffp, "DISABLETIME", &disabletime, UNSET);

	if (disabletime == 0)
		return;

	xconfirm(CMW_DISABLETIME,
	    "System is locked.",
	    "System Lockout Information");

	signal(SIGHUP, SIG_IGN);
	signal(SIGINT, SIG_IGN);
	signal(SIGQUIT, SIG_IGN);
	signal(SIGTERM, SIG_IGN);
	signal(SIGTSTP, SIG_IGN);
	signal(SIGWINCH, SIG_IGN);

	sleep(disabletime);
	exit(3);
}

static int
do_maxtrys(FILE *deffp, int try, struct passwd *pwent)
{
	static int maxtrys = UNKNOWN;
	static int logfailures = UNKNOWN;
	static char *failure[MAX_LOGFAILURES];
	time_t now;
	char *cp;

	defvalue(deffp, "MAXTRYS", &maxtrys, UNSET);
	defvalue(deffp, "LOGFAILURES", &logfailures, MAX_LOGFAILURES);

	if (logfailures != UNSET) {
		failure[try] = malloc(LOGFAILURE_BUFSIZE);
		now = time(NULL);
		sprintf(failure[try], "%s:/dev/console:%s",
		    pwent ? pwent->pw_name : "?", ctime(&now)); 
	}
	if (logfailures == try + 1) {
		FILE *fp;
		int i;

		if (fp = fopen(LOGFAILURE_FILE, "r")) {
			fclose(fp);
			if (fp = fopen(LOGFAILURE_FILE, "a")) {
				for (i = 0; i < logfailures; i++)
					fprintf(fp, "%s\n", failure[i]);
				fclose(fp);
			}
		}
		do_disabletime(deffp);
	}
	if (maxtrys == try + 1)
		do_disabletime(deffp);

	return 1;
}

static void
timeout_handler(int i)
{
	cap_value_t cap_audit_write = CAP_AUDIT_WRITE;
	cap_t ocap = cap_acquire(1, &cap_audit_write);
	ia_audit("CMWdialog", "", 0, "TIMEOUT exceeded");
	cap_surrender(ocap);
	exit(4);
}

static int timeout = UNKNOWN;

static void
timeout_set(FILE *deffp)
{
	char *cp;

	defvalue(deffp, "TIMEOUT", &timeout, MAX_TIMEOUT);

	if (timeout == UNSET)
		return;

	signal(SIGALRM, timeout_handler);
	alarm(timeout);
}

static void
timeout_clear(void)
{
	if (timeout == UNSET)
		return;
	alarm(0);
	signal(SIGALRM, SIG_DFL);
}

static int
xpasswd(struct passwd *pwent)
{
	FILE *fp;
	pid_t sitepid;
	int wstatus;
	cap_value_t cap_audit_write = CAP_AUDIT_WRITE;
	cap_value_t cap_xtcb = CAP_XTCB;
	cap_t ocap;

	switch (sitepid = fork()) {
	case -1:
		return 0;
	case 0:
		signal(SIGALRM, SIG_DFL);
		signal(SIGHUP, SIG_DFL);
		ocap = cap_acquire(1, &cap_xtcb);
		execl(XWSH, XWSH,
		    "-noscrollbar", "-nomenu", "-ut",
		    "-display", ":0",
		    "-title", "Password Update Dialog",
		    "-icontitle", "CMWpasswd",
		    "-transient", "root",
		    "-e", XPASSWD, pwent->pw_name, NULL);
		cap_surrender(ocap);

		ocap = cap_acquire(1, &cap_audit_write);
		ia_audit("CMWdialog", pwent->pw_name, 0,
			 "can't exec CMWpasswd program");
		cap_surrender(ocap);
		return 0;
	}

	if (waitpid(sitepid, &wstatus, 0) < 0)
		return 0;

	if ((fp = fopen(CMW_PASSWD, "r")) == NULL)
		return 0;

	unlink(CMW_PASSWD);

	if (fscanf(fp, "%d", &wstatus) != 1)
		return 0;

	fclose(fp);

	return !wstatus;
}

static int
do_passwordcheck(FILE *deffp, struct passwd *pwent)
{
	time_t now;
	struct spwd *spent = NULL;
	char *pwdp = NULL;
	char *cp;
	static int mandpass = UNKNOWN;
	static int passreq = UNKNOWN;
	static int idleweeks = UNKNOWN;
	static int lockout = UNKNOWN;

	if (mandpass == UNKNOWN) {
		if ((cp = defread(deffp, "MANDPASS")) && !strcmp(cp, "YES"))
			mandpass = 1;
		else
			mandpass = UNSET;
	}

	if (passreq == UNKNOWN) {
		if ((cp = defread(deffp, "PASSREQ")) && !strcmp(cp, "YES"))
			passreq = 1;
		else
			passreq = UNSET;
	}

	defvalue(deffp, "IDLEWEEKS", &idleweeks, UNSET);
	defvalue(deffp, "LOCKOUT", &lockout, 255);

	if (pwent) {
		spent = getspnam(pwent->pw_name);
		pwdp = pwent->pw_passwd;
	}

	if (pwdp && *pwdp == '\0')
		pwdp = NULL;

	/*
	 * A password is required, but the account has none.
	 */
	if (pwent && pwdp == NULL && passreq != UNSET)
		return xpasswd(pwent);

	/*
	 * If passwords are required always prompt for one, even if we
	 * already know that we're not going to let'em in.
	 */
	if (pwent == NULL || pwdp || (mandpass != UNSET && passreq != UNSET)) {
		timeout_set(deffp);
		cp = getpass("Password:     ");
		timeout_clear();
		if (cp == NULL)
			return 0;
	}

	/*
	 * Return if the only purpose was to look like we recognized
	 * the user when in fact there was no passwd entry.
	 */
	if (pwent == NULL)
		return 0;

	/*
	 * User had no password. Return 1 if neither
	 * mandpass or passreq is set, zero if either is set.
	 */
	if (pwdp == NULL)
		return (mandpass == UNSET && passreq == UNSET);

	/*
	 * There is a password. Return 0 if the user's response
	 * is not correct.
	 */
	if (strcmp(pwdp, crypt(cp, pwdp)))
		return 0;

	/*
	 * The user got it right. Time to check if the
	 * password has ripened sufficiently to be picked.
	 * Aging information comes from shadow. No shadow,
	 * no aging.
	 */
	if (spent == NULL)
		return 1;

	/*
	 * XXX:casey incredibly ugly macro from shadow.h
	 */
	now = DAY_NOW;

	/*
	 * Password expiration rules:
	 *	if lastchanged is 0, the password has expired.
	 *	if lastchanged is in the future, the password has expired.
	 *	if maximum is set (i.e. not -1)		-and-
	 *	   maximum is not smaller than minimum	-and-
	 *	   lastchanged + maximum is in the past,
	 *	   the password has expired.
	 *	
	 * This test is the inverse of what's in login, BTW.
	 * Check for an unexpired password, and return in that case.
	 */
	if ((spent->sp_lstchg != 0) &&
	    (spent->sp_lstchg <= now) &&
            ((spent->sp_max < 0) ||
	     (now <= (spent->sp_lstchg + spent->sp_max)) ||
	     (spent->sp_max < spent->sp_min)))
		return 1;
	/*
	 * Password has expired. Check if it's been expired
	 * beyond reason, as defined by IDLEWEEKS.
	 */
	if (spent->sp_lstchg != 0 &&
	    (idleweeks == 0 ||
	     (idleweeks > 0 && now > (spent->sp_lstchg + (7 * idleweeks))))) {
		xconfirm(CMW_IDLEWEEKS,
		    "Your password has been expired for too long.",
		    "User Lockout Information");
		return 0;
	}
	/*
	 * Password is stale, but not too stale. Update it.
	 */
	return xpasswd(pwent);
}
/*
 * CMW/X environment version of the classic function from login(1).
 * Use the CMWsitecheck script to squirrel away the exit status of
 * the real sitecheck program because "xwsh -e cmd" does not propagate
 * the exit status of cmd.
 */
static int
do_sitecheck(FILE *deffp, char *name)
{
	char *path;
	FILE *fp;
	int wstatus;
	struct stat sbuf;
	mac_t mp;
	mac_t mok;
	pid_t sitepid;
	int i = 0;
	cap_value_t cap_audit_write = CAP_AUDIT_WRITE;
	cap_value_t cap_xtcb = CAP_XTCB;
	cap_t ocap;

	if ((path = defread(deffp, "SITECHECK")) == NULL)
		return SITE_CONTINUE;
	/*
	 * Enforce security items.  Check that the program is owned by root,
	 * and is not world-writable.  Return SITE_CONTINUE if it is not,
	 * thus causing a traditional unix authentication.
	 */

	if (stat(path, &sbuf) != 0) {
		ocap = cap_acquire(1, &cap_audit_write);
		ia_audit("CMWdialog", name, 0, "cannot access sitecheck");
		cap_surrender(ocap);
		return SITE_CONTINUE;
	}

	if (sbuf.st_uid != 0) {
		ocap = cap_acquire(1, &cap_audit_write);
		ia_audit("CMWdialog", name, 0, "bad sitecheck ownership");
		cap_surrender(ocap);
		return SITE_CONTINUE;
	}

	if (sbuf.st_mode & 0022) {
		ocap = cap_acquire(1, &cap_audit_write);
		ia_audit("CMWdialog", name, 0, "bad sitecheck permissions");
		cap_surrender(ocap);
		return SITE_CONTINUE;
	}

	if ((mok = mac_from_text("msenlow/minthigh")) == NULL) {
		ocap = cap_acquire(1, &cap_audit_write);
		ia_audit("CMWdialog", name, 0, "bad system label names");
		cap_surrender(ocap);
		return SITE_CONTINUE;
	}

	if ((mp = mac_get_file(path)) == NULL) {
		ocap = cap_acquire(1, &cap_audit_write);
		ia_audit("CMWdialog", name, 0, "bad sitecheck MAC");
		cap_surrender(ocap);
		mac_free(mok);
		return SITE_CONTINUE;
	}

	i = (!mac_equal(mok, mp) ||
	    mp->ml_msen_type == MSEN_EQUAL_LABEL ||
	    mp->ml_mint_type == MINT_EQUAL_LABEL);

	mac_free(mp);
	mac_free(mok);

	if (i) {
		ocap = cap_acquire(1, &cap_audit_write);
		ia_audit("CMWdialog", name, 0, "bad sitecheck MAC");
		cap_surrender(ocap);
		return SITE_CONTINUE;
	}

	/* fork, exec, and wait for return code of siteprog */

	switch (sitepid = fork()) {
	case -1:
		return SITE_FAIL;
	case 0:
		signal(SIGALRM, SIG_DFL);
		signal(SIGHUP, SIG_DFL);
		ocap = cap_acquire(1, &cap_xtcb);
		execl(XWSH, XWSH,
		    "-noscrollbar", "-nomenu", "-ut",
		    "-display", ":0",
		    "-title", "Site authentication Dialog",
		    "-icontitle", "sitecheck",
		    "-transient", "root",
		    "-e", XSITECHECK, path, name, NULL);
		cap_surrender(ocap);

		ocap = cap_acquire(1, &cap_audit_write);
		ia_audit("CMWdialog", name, 0, "can't exec sitecheck program");
		cap_surrender(ocap);
		return SITE_CONTINUE;
	}

	if (waitpid(sitepid, &wstatus, 0) < 0)
		return SITE_FAIL;

	if ((fp = fopen(CMW_SITECHECK, "r")) == NULL)
		return SITE_CONTINUE;

	unlink(CMW_SITECHECK);

	if (fscanf(fp, "%d", &i) != 1)
		return SITE_CONTINUE;

	fclose(fp);

	return i;
}

/*
 * Determine if root logins are allowed on only one device, and if so,
 * if this is it.
 */
static int
do_console(FILE *deffp)
{
	char *console;

	if ((console = defread(deffp, "CONSOLE")) == NULL)
		return 1;
	/*
	 * Do the CONSOLE login default option check.
	 */
	if (console && strcmp(console, CONSOLE))
		return 0;
	return 1;
}

static void
xconfirm_root(void)
{
	char message[256];
	char *fmt = "Your user ID is zero (0). Please note:\n%s\n";

	switch (sysconf(_SC_CAP)) {
	case CAP_SYS_NO_SUPERUSER:
		sprintf(message, fmt, "There is no Superuser here.");
		break;
	case CAP_SYS_SUPERUSER:
		sprintf(message, fmt, "The Superuser is effective.");
		break;
	case CAP_SYS_DISABLED:
		sprintf(message, fmt, "Capabilities are disabled.");
		break;
	default:
		sprintf(message, fmt, "Capabilities are unknown.");
		break;
	}

	xconfirm(CMW_UID0, message, "Root User Special Information");
}

static void
terminate_line(char *line)
{
	char *cp;
	char *tp;

	for (tp = TERMINATORS ; *tp ; tp++)
		if (cp = strchr(line, *tp))
			*cp = '\0';
}

static int
capable(struct user_cap *capent, char *cap_requested)
{
	int result = 0;
	cap_t rcap = cap_from_text(cap_requested);
	cap_t pcap = cap_from_text(capent->ca_allowed);

	/*
	 * verify that requested capabilities are permitted
	 */
	if (pcap && rcap &&
	    CAP_ID_ISSET(rcap->cap_effective, pcap->cap_effective) &&
	    CAP_ID_ISSET(rcap->cap_permitted, pcap->cap_permitted) &&
	    CAP_ID_ISSET(rcap->cap_inheritable, pcap->cap_inheritable))
		result = 1;

	cap_free(pcap);
	cap_free(rcap);

	return result;
}

static void
cmw_message(char *path, char *message)
{
	FILE *fp;

	if ((fp = fopen(path, "w")) == NULL) 
		exit(5);

	fprintf(fp, "%s\n", message);
	fclose(fp);
}

static void
report_error(FILE *deffp, char *username, char *message)
{
	static int sleeptime = UNKNOWN;
	char *cp;
	cap_value_t cap_audit_write = CAP_AUDIT_WRITE;
	cap_t ocap;

	defvalue(deffp, "SLEEPTIME", &sleeptime, MAX_SLEEPTIME);
	if (sleeptime != UNSET)
		sleep(sleeptime);

	ocap = cap_acquire(1, &cap_audit_write);
	ia_audit(program, username, 0, message);
	cap_surrender(ocap);
	fprintf(stdout, "Login Incorrect.\n\n");
}

static void
enable_caps()
{
	cap_value_t cap_priv_port = CAP_PRIV_PORT;
	cap_t oldcap;

	oldcap = cap_acquire(1, &cap_priv_port);
	cap_free(oldcap);
}

main(int argc, char *argv[])
{
	char line[512];
	char *username = NULL;
	char *mac_requested = NULL;
	char *cap_requested = NULL;
	struct passwd *pwent = NULL;
	struct user_cap *capent;
	struct clearance *macent;
	FILE *fp;
	FILE *defaultsfp = defopen(LOGINDEFAULTS);
	int trys;
	int c;
	cap_value_t cap_audit_write = CAP_AUDIT_WRITE;
	cap_t ocap;

	setbuf(stdout, NULL);
	setbuf(stderr, NULL);
	program = argv[0];
	enable_caps();

	/*
	 * Print instructions regarding use of the Trusted Path.
	 * Allow a site specific (CMW_ANNOUNCE) message.
	 */
	if (fp = fopen(CMW_ANNOUNCE, "r")) {
		while ((c = fgetc(fp)) != EOF)
			fputc(c, stdout);
		fclose(fp);
	}
	else {
		fprintf(stdout, "\n\n");
		fprintf(stdout, "\t\t>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>>\n");
		fprintf(stdout, "\t\t^                                v\n");
		fprintf(stdout, "\t\t^ Be sure the Trusted Path is on v\n");
		fprintf(stdout, "\t\t^ before you attempt to log in.  v\n");
		fprintf(stdout, "\t\t^                                v\n");
		fprintf(stdout, "\t\t<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<\n");
		fprintf(stdout, "\n\n");
	}
	/*
	 * Print instructions for login.
	 */
	if (fp = fopen(CMW_HOWTO, "r")) {
		while ((c = fgetc(fp)) != EOF)
			fputc(c, stdout);
		fclose(fp);
	}
	else {
		if (gethostname(line, 500) == 0)
			fprintf(stdout, "Machine Name\t%s\n", line);
		if (getdomainname(line, 500) == 0)
			fprintf(stdout, "Domain Name\t%s\n", line);
		fprintf(stdout, "\n");
		fprintf(stdout, "This is the IRIX/CMW login dialog.\n");
		fprintf(stdout, "You will be prompted for:\n\n");
		fprintf(stdout, "\tUser Name\n");
		fprintf(stdout, "\tMandatory Access Control Label\n");
		fprintf(stdout, "\tCapability Set\n");
		fprintf(stdout, "\tPassword (if you have one)\n\n");
		fprintf(stdout, "\"Enter\" by itself will get your default\n");
		fprintf(stdout, "for MAC and Capabilities.\n");
		fprintf(stdout, "\n");
	}

	for (trys = 0; ; do_maxtrys(defaultsfp, trys++, pwent)) {
		if (cap_requested) {
			free(cap_requested);
			cap_requested = NULL;
		}
		if (mac_requested) {
			free(mac_requested);
			mac_requested = NULL;
		}
		if (username) {
			free(username);
			username = NULL;
		}
		/*
		 * Get the User name.
		 */
		fprintf(stdout, "User Name:    ");
		if (fgets(line, 511, stdin) == NULL)
			exit(6);
		terminate_line(line);
		if (line[0] == '\0')
			continue;

		username = strdup(line);
		pwent = getpwnam(username);
		capent = sgi_getcapabilitybyname(username);
		macent = sgi_getclearancebyname(username);
		/*
		 * Get the MAC label.
		 */
		fprintf(stdout, "MAC label:    ");
		timeout_set(defaultsfp);
		if (fgets(line, 511, stdin) == NULL)
			exit(7);
		timeout_clear();
		terminate_line(line);
		if (strlen(line) > 0)
			mac_requested = strdup(line);
		/*
		 * Get the capability set.
		 */
		fprintf(stdout, "Capabilities: ");
		timeout_set(defaultsfp);
		if (fgets(line, 511, stdin) == NULL)
			exit(8);
		timeout_clear();
		terminate_line(line);
		if (strlen(line) > 0)
			cap_requested = strdup(line);

		/*
		 * The sitecheck semantics allow for an exclusive check.
		 */
		c = do_sitecheck(defaultsfp, pwent ? pwent->pw_name : "____");

		switch (c) {
		case SITE_OK:
			/*
			 * The sitecheck program says they're OKay.
			 */
			break;
		case SITE_FAIL:
			/*
			 * The sitecheck program says they're baaaad.
			 */
			report_error(defaultsfp, username, "Sitecheck denial");
			continue;
		case SITE_AGAIN:
			/*
			 * The sitecheck program wants to retry.
			 */
			continue;
		case SITE_CONTINUE:
		default:
			/*
			 * The sitecheck program doesn't care, or wants
			 * the password checked anyway.
			 */
			if (do_passwordcheck(defaultsfp, pwent) == 0) {
				report_error(defaultsfp, username,
				    "Bad password");
				continue;
			}
			break;
		}

		/*
		 * If any of the *ent pointers are NULL
		 * then the user's account is damaged, or non-existant.
		 * If that's the case, fake the authentication, but
		 * discard the result unexamined.
		 */
		if (pwent == NULL || capent == NULL || macent == NULL) {
			report_error(defaultsfp, username,
				     "Account inconsistant");
			continue;
		}

		if (cap_requested == NULL)
			cap_requested = strdup(capent->ca_default);

		if (!capable(capent, cap_requested)) {
			sprintf(line, "Bad CAP=%s", cap_requested);
			report_error(defaultsfp, username, line);
			continue;
		}

		if (mac_requested == NULL)
			mac_requested = strdup(macent->cl_default);

		if (mac_cleared(macent, mac_requested) != MAC_CLEARED) {
			sprintf(line, "Bad MAC=%s", mac_requested);
			report_error(defaultsfp, username, line);
			continue;
		}
		/*
		 * If logging in as root (uid 0)
		 * special processing may be required
		 */
		if (pwent->pw_uid == 0 && !do_console(defaultsfp)) {
			report_error(defaultsfp, username,
				     "root not allowed on console");
			continue;
		}
		/*
		 * At this point the user is validated, and the security
		 * attribute information has been verified. Save it.
		 */
		cmw_message(CMW_USER_NAME, username);
		cmw_message(CMW_USER_MAC, mac_requested);
		cmw_message(CMW_USER_CAP, cap_requested);
		ocap = cap_acquire(1, &cap_audit_write);
		ia_audit(program, username, 1, "CMW Login");
		cap_surrender(ocap);
		/*
		 * If logging in as root (uid 0) put up a special message.
		 */
		if (pwent->pw_uid == 0)
			xconfirm_root();
		/*
		 * Done. Exit.
		 */
		exit(0);
	}
}
