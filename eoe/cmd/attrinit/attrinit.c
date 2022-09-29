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

#ident "$Revision: 1.3 $"
/*
 * Set large numbers of file attributes.
 */
#include <sys/types.h>
#include <sys/attributes.h>
#include <sys/acl.h>
#include <sys/capability.h>
#include <sys/mac_label.h>
#include <sys/mac.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <ctype.h>
#include <stdio.h>
#include <pwd.h>
#include <grp.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * Limit the user to MAX_ARB arbitrary attributes in each of the
 * root and user address spaces.
 * XXX:casey This is kinda bogus, but it beats doing memory management.
 */
#define MAX_ARB	8
/*
 * File types beyond the norm.
 */
#define S_IS_A_TREE	-1
#define S_IS_A_MOLDTREE	-2
#define DONTCARE	-3
/*
 * Flags
 */
#define	A_ROOT		0x01	/* Root Attribute */
#define A_REMOVE	0x02	/* Delete the attribute */

struct action {
	int		a_verify;
	int		a_edit;
	char		*a_filetypename; /* Not necessary to duplicate */
	int		a_filetype;
	uid_t		a_userid;
	gid_t		a_groupid;
	mode_t		a_mode;
	char		*a_attr_name[MAX_ARB];
	char		*a_attr_data[MAX_ARB];
	int		a_attr_size[MAX_ARB];
	int		a_attr_flag[MAX_ARB];
};
typedef struct action action_t;

void
clear(char **cpp)
{
	if (cpp == NULL)
		return;
	if (*cpp == NULL)
		return;
	free(*cpp);
	*cpp = NULL;
}

/*
 * Verification modes
 */
#define	DO_NOT_VERIFY	0	/* Don't verify at all */
#define VERIFY_ALL	1	/* Produce a message for everything */
#define VERIFY_EXISTING	2	/* Produce a message iff it's there and wrong */

char *program;

int
this_attr(char *name, action_t *ap)
{
	int i;

	for (i = 0 ; i < MAX_ARB; i++) {
		if (ap->a_attr_name[i] == NULL)
			return i;
		if (name == NULL)
			continue;
		if (strcmp(ap->a_attr_name[i], name))
			return i;
	}

	/*
	 * Too many attributes specified!
	 */
	fprintf(stderr, "%s: There is a limit of %d -attr specifications.\n",
	    program, MAX_ARB);
	exit(1);

	return 0;
}

action_t *
new_action(void)
{
	action_t *ap = (action_t *)calloc(1, sizeof(action_t));

	ap->a_edit = 1;
	ap->a_verify = DO_NOT_VERIFY;

	ap->a_userid = DONTCARE;
	ap->a_groupid = DONTCARE;
	ap->a_mode = DONTCARE;
	ap->a_filetype = DONTCARE;
	return ap;
}

void
arg_error(char *arg, char *value)
{
	fprintf(stderr, "%s: argument %s cannot use value \"%s\".\n",
	    program, arg, value);
	exit(1);
}

action_t *
copy_action(action_t *oap)
{
	int i;
	action_t *ap;

	if (oap == NULL)
		return new_action();
	
	ap = (action_t *)calloc(1, sizeof(action_t));
	*ap = *oap;
	for (i = 0 ; i < MAX_ARB; i++) {
		if (oap->a_attr_name[i] == NULL) 
			break;
		ap->a_attr_name[i] = strdup(oap->a_attr_name[i]);
		ap->a_attr_data[i] = calloc(oap->a_attr_size[i], sizeof(char));
		memcpy(ap->a_attr_data[i], oap->a_attr_data[i],
		       oap->a_attr_size[i]);
	}
	return ap;
}

void
free_action(action_t *ap)
{
	int i;

	if (!ap)
		return;
	for (i = 0 ; i < MAX_ARB; i++) {
		if (ap->a_attr_name[i] == NULL) 
			break;
		clear(&ap->a_attr_data[i]);
		clear(&ap->a_attr_name[i]);
	}
	free(ap);
}

action_t *
add_action(action_t *ap, char *arg)
{
	int i;
	char *value;

	if (*arg != '-')
		return NULL;
		
	if ((value = strchr(arg, '=')) == NULL || *(value + 1) == '\0') {
		fprintf(stderr, "%s: required value missing for \"%s\".\n",
			program, arg);
		return NULL;
	}
	*value++ = '\0';

	ap = ap ? ap : new_action();

	if (!strcmp(arg, "-verify")) {
		if (!strcmp(value, "all")) {
			ap->a_verify = VERIFY_ALL;
			ap->a_edit = 0;
		}
		else if (!strcmp(value, "also")) {
			ap->a_verify = VERIFY_ALL;
			ap->a_edit = 1;
		}
		else if (!strcmp(value, "installed")) {
			ap->a_verify = VERIFY_EXISTING;
			ap->a_edit = 0;
		}
		else {
			free_action(ap);
			arg_error(arg, value);
			return NULL;
		}
		return ap;
	}
	if (!strcmp(arg, "-filetype") || !strcmp(arg, "-type")) {

		if (!strcmp(value, "d") || !strcmp(value, "directory")) {
			ap->a_filetype = S_IFDIR;
			ap->a_filetypename = "directory";
		}
		else if (!strcmp(value, "f") || !strcmp(value, "file")) {
			ap->a_filetype = S_IFREG;
			ap->a_filetypename = "file";
		}
		else if (!strcmp(value, "l") ||
		    !strcmp(value, "link") || !strcmp(value, "symlink")) {
			ap->a_filetype = S_IFLNK;
			ap->a_filetypename = "link";
		}
		else if (!strcmp(value, "p") || !strcmp(value, "pipe") ||
		    !strcmp(value, "fifo") || !strcmp(value, "FIFO")) {
			ap->a_filetype = S_IFIFO;
			ap->a_filetypename = "FIFO";
		}
		else if (!strcmp(value, "c") ||
		    !strcmp(value, "char") || !strcmp(value, "chardev")) {
			ap->a_filetype = S_IFCHR;
			ap->a_filetypename = "char";
		}
		else if (!strcmp(value, "b") ||
		    !strcmp(value, "blk") || !strcmp(value, "block") ||
		    !strcmp(value, "blockdev")) {
			ap->a_filetype = S_IFBLK;
			ap->a_filetypename = "block";
		}
		else if (!strcmp(value, "s") || !strcmp(value,"socket")) {
			ap->a_filetype = S_IFSOCK;
			ap->a_filetypename = "socket";
		}
		else if (!strcmp(value, "t") || !strcmp(value, "tree")) {
			ap->a_filetype = S_IS_A_TREE;
			ap->a_filetypename = "tree";
		}
		else if (!strcmp(value, "m") || !strcmp(value, "mld") ||
		    !strcmp(value, "moldy")) {
			ap->a_filetype = S_IS_A_MOLDTREE;
			ap->a_filetypename = "moldtree";
		}
		else {
			free_action(ap);
			arg_error(arg, value);
			return NULL;
		}
		return ap;
	}
	/*
	 * attribute types.
	 */
	if (!strcmp(arg, "-attr")) {
		char *data;

		if ((data = strchr(value, ',')) == NULL)
			*data++ = '\0';
		i = this_attr(value, ap);
		clear(&ap->a_attr_name[i]);
		clear(&ap->a_attr_data[i]);

		if (data == NULL) {
			ap->a_attr_flag[i] = A_REMOVE;
			ap->a_attr_size[i] = 0;
		}
		else {
			ap->a_attr_flag[i] = 0;
			ap->a_attr_data[i] = strdup(data);
			ap->a_attr_size[i] = strlen(data);
			*(data - 1) = ',';
		}
		ap->a_attr_name[i] = strdup(value);
		return ap;
	}
	if (!strcmp(arg, "-cap") || !strcmp(arg, "-capability")) {
		i = this_attr(NULL, ap);
		ap->a_attr_data[i] = (char *)cap_from_text(value);
		if (ap->a_attr_data[i] == NULL) {
			arg_error(arg, value);
			free_action(ap);
			return NULL;
		}
		ap->a_attr_flag[i] = A_ROOT;
		ap->a_attr_name[i] = strdup(SGI_CAP_FILE);
		ap->a_attr_size[i] = cap_size((cap_t)ap->a_attr_data[i]);
		return ap;
	}
	if (!strcmp(arg, "-mac")) {
		i = this_attr(NULL, ap);
		ap->a_attr_data[i] = (char *)mac_from_text(value);
		if (ap->a_attr_data[i] == NULL){
			arg_error(arg, value);
			free_action(ap);
			return NULL;
		}
		ap->a_attr_flag[i] = A_ROOT;
		ap->a_attr_name[i] = strdup(SGI_MAC_FILE);
		ap->a_attr_size[i] = mac_size((mac_t)ap->a_attr_data[i]);
		return ap;
	}
	if (!strcmp(arg, "-acl")) {
		i = this_attr(NULL, ap);
		ap->a_attr_data[i] = (char *)acl_from_text(value);
		if (ap->a_attr_data[i] == NULL){
			arg_error(arg, value);
			free_action(ap);
			return NULL;
		}
		ap->a_attr_flag[i] = A_ROOT;
		ap->a_attr_name[i] = strdup(SGI_ACL_FILE);
		ap->a_attr_size[i] = acl_size((acl_t)ap->a_attr_data[i]);
		return ap;
	}
	if (!strcmp(arg, "-acldefault")) {
		i = this_attr(NULL, ap);
		ap->a_attr_data[i] = (char *)acl_from_text(value);
		if (ap->a_attr_data[i] == NULL){
			arg_error(arg, value);
			free_action(ap);
			return NULL;
		}
		ap->a_attr_flag[i] = A_ROOT;
		ap->a_attr_name[i] = strdup(SGI_ACL_DEFAULT);
		ap->a_attr_size[i] = acl_size((acl_t)ap->a_attr_data[i]);
		return ap;
	}
	if (!strcmp(arg, "-mode")) {
		if (sscanf(value, "%o", &ap->a_mode) != 1) {
			arg_error(arg, value);
			free_action(ap);
			return NULL;
		}
		return ap;
	}
	if (!strcmp(arg, "-user")) {
		struct passwd *pwd;

		ap->a_userid = DONTCARE;
		if (isdigit(*value))
			(void)sscanf(value, "%d", &ap->a_userid);
		else if (pwd = getpwnam(value))
			ap->a_userid = pwd->pw_uid;
		if (ap->a_userid == DONTCARE) {
			arg_error(arg, value);
			free_action(ap);
			return NULL;
		}
		return ap;
	}
	if (!strcmp(arg, "-group")) {
		struct group *grp;

		ap->a_groupid = DONTCARE;
		if (isdigit(*value))
			(void)sscanf(value, "%d", &ap->a_groupid);
		else if (grp = getgrnam(value))
			ap->a_groupid = grp->gr_gid;
		if (ap->a_groupid == DONTCARE) {
			arg_error(arg, value);
			free_action(ap);
			return NULL;
		}
		return ap;
	}
	/*
	 * Not something we know about.
	 */
	fprintf(stderr, "%s: unknown argument \"%s\".\n", program, arg);
	free_action(ap);
	return NULL;
}

void
script_error(char *script, int line)
{
	fprintf(stderr, "%s: error in script \"%s\"(line %d).\n",
		program, script, line);
}

char *
verify_attrs(char *path, action_t *ap)
{
	static char message[512];
	static char buffer[512];
	int error;
	int size;
	int i;
	cap_t ocap;
	cap_value_t attr_get_caps[] = {CAP_MAC_READ, CAP_DAC_READ_SEARCH,
				       CAP_DEVICE_MGT};

	message[0] = '\0';

	for (i = 0; i < MAX_ARB; i++) {
		if (ap->a_attr_name[i] == NULL)
			break;
		if (ap->a_attr_flag[i] & A_ROOT) {
			ocap = cap_acquire(3, attr_get_caps);
			error = attr_get(path, ap->a_attr_name[i], buffer,
					 &size, ATTR_DONTFOLLOW | ATTR_ROOT);
			cap_surrender(ocap);
		} else {
			ocap = cap_acquire(2, attr_get_caps);
			error = attr_get(path, ap->a_attr_name[i], buffer,
					 &size, ATTR_DONTFOLLOW);
			cap_surrender(ocap);
		}
		if (error) {
			fprintf(stderr,
				"%s: Attribute (%s) read failure (%s)",
				program, ap->a_attr_name[i], path);
			perror("");
			sprintf(buffer, ":%s=UNDEFINED", ap->a_attr_name[i]);
			strcat(message, buffer);
		}
		else if (size != ap->a_attr_size[i] ||
			 memcmp(buffer, ap->a_attr_data[i], size)) {
			sprintf(buffer, ":%s=MISMATCHED", ap->a_attr_name[i]);
			strcat(message, buffer);
		}
	}
	return message;
}

void
update_attrs(char *path, action_t *ap)
{
	int i;
	int error;
	cap_t ocap;
	cap_value_t attr_set_caps[] = {CAP_MAC_READ, CAP_DAC_READ_SEARCH,
				       CAP_DAC_WRITE, CAP_MAC_WRITE,
				       CAP_DEVICE_MGT};

	for (i = 0; i < MAX_ARB; i++) {
		if (ap->a_attr_name[i] == NULL)
			break;
		if (ap->a_attr_flag[i] & A_ROOT) {
			ocap = cap_acquire(5, attr_set_caps);
			error = attr_set(path, ap->a_attr_name[i],
					 ap->a_attr_data[i],
					 ap->a_attr_size[i],
					 ATTR_DONTFOLLOW | ATTR_ROOT);
			cap_surrender(ocap);
		} else {
			ocap = cap_acquire(4, attr_set_caps);
			error = attr_set(path, ap->a_attr_name[i],
					 ap->a_attr_data[i],
					 ap->a_attr_size[i],
					 ATTR_DONTFOLLOW);
			cap_surrender(ocap);
		}
		if (error) {
			fprintf(stderr,
				"%s: Attribute (%s) write failure (%s)",
				program, ap->a_attr_name[i], path);
			perror("");
		}
	}
}

int
count_lines(FILE *fp)
{
	char buffer[512];
	int lines = 0;

	while (fgets(buffer, 510, fp))
		lines++;
	rewind(fp);
	return lines;
}

int
doone(char *path, action_t *ap)
{
	int error = 0;
	struct passwd *pwd;
	struct group *grp;
	struct stat64 filestatbuf;
	char *cp;

	if (!path || *path == '\0')
		return (0);

	if (lstat64(path, &filestatbuf) < 0) {
		if (ap->a_verify == VERIFY_ALL) {
			fprintf(stderr, "%s: \"%s\" is inaccessible. ",
				program, path);
			perror("Reason");
		}
		return (1);
	}

	switch (ap->a_verify) {
	case DO_NOT_VERIFY:
		break;
	case VERIFY_ALL:
		printf("%s", path);
		if (ap->a_userid != DONTCARE &&
		    ap->a_userid != filestatbuf.st_uid) {
			if (pwd = getpwuid(filestatbuf.st_uid))
				printf(":uid=%s", pwd->pw_name);
			else
				printf(":uid=%d", filestatbuf.st_uid);
			if (pwd = getpwuid(ap->a_userid))
				printf("(%s)", pwd->pw_name);
			else
				printf("(%d)", ap->a_userid);
			error++;
		}
		if (ap->a_groupid != DONTCARE &&
		    ap->a_groupid != filestatbuf.st_gid) {
			if (grp = getgrgid(filestatbuf.st_gid))
				printf(":gid=%s", grp->gr_name);
			else
				printf(":gid=%d", filestatbuf.st_gid);
			if (grp = getgrgid(ap->a_groupid))
				printf("(%s)", grp->gr_name);
			else
				printf("(%d)", ap->a_groupid);
			error++;
		}
		if (ap->a_mode != DONTCARE &&
		    ap->a_mode != (filestatbuf.st_mode &06777)) {
			printf(":mode=%04o(%04o)",
			       filestatbuf.st_mode & 06777,
			       ap->a_mode & 06777);
			error++;
		}
		if ((cp = verify_attrs(path, ap)) && *cp) {
			printf("%s", cp);
			error++;
		}
		if (ap->a_filetype != DONTCARE &&
		    ap->a_filetype != S_IS_A_TREE &&
		    ap->a_filetype != S_IS_A_MOLDTREE &&
		    ap->a_filetype != (S_IFMT & filestatbuf.st_mode)) {
			if (S_ISREG(filestatbuf.st_mode))
				printf(":type=file");
			else if (S_ISDIR(filestatbuf.st_mode))
				printf(":type=directory");
			else if (S_ISLNK(filestatbuf.st_mode))
				printf(":type=symlink");
			else if (S_ISFIFO(filestatbuf.st_mode))
				printf(":type=FIFO");
			else if (S_ISSOCK(filestatbuf.st_mode))
				printf(":type=socket");
			else if (S_ISCHR(filestatbuf.st_mode))
				printf(":type=cdev");
			else if (S_ISBLK(filestatbuf.st_mode))
				printf(":type=bdev");
			else
				printf(":type=UNKNOWN");
			printf("(%s)", ap->a_filetypename);
			error++;
		}
		printf("\n");
		break;
	case VERIFY_EXISTING:
		if (ap->a_userid != DONTCARE &&
		    ap->a_userid != filestatbuf.st_uid) {
			printf("%s", path);
			if (pwd = getpwuid(filestatbuf.st_uid))
				printf(":uid=%s", pwd->pw_name);
			else
				printf(":uid=%d", filestatbuf.st_uid);
			if (pwd = getpwuid(ap->a_userid))
				printf("(%s)", pwd->pw_name);
			else
				printf("(%d)", ap->a_userid);
			error++;
		}
		if (ap->a_groupid != DONTCARE &&
		    ap->a_groupid != filestatbuf.st_gid) {
			if (!error++)
				printf("%s", path);
			if (grp = getgrgid(filestatbuf.st_gid))
				printf(":gid=%s", grp->gr_name);
			else
				printf(":gid=%d", filestatbuf.st_gid);
			if (grp = getgrgid(ap->a_groupid))
				printf("(%s)", grp->gr_name);
			else
				printf("(%d)", ap->a_userid);
		}
		if (ap->a_mode != DONTCARE &&
		    ap->a_mode != (filestatbuf.st_mode &06777)) {
			if (!error++)
				printf("%s", path);
			printf(":mode=%04o(%04o)",
			       (filestatbuf.st_mode & 06777),
			       (ap->a_mode & 06777));
		}
		if ((cp = verify_attrs(path, ap)) && *cp){
			if (!error++)
				printf("%s", path);
			printf("%s", cp);
		}
		if (ap->a_filetype != DONTCARE &&
		    ap->a_filetype != S_IS_A_TREE &&
		    ap->a_filetype != S_IS_A_MOLDTREE &&
		    ap->a_filetype != (S_IFMT & filestatbuf.st_mode)) {
			if (!error++)
				printf("%s", path);
			if (S_ISREG(filestatbuf.st_mode))
				printf(":type=file");
			else if (S_ISDIR(filestatbuf.st_mode))
				printf(":type=directory");
			else if (S_ISLNK(filestatbuf.st_mode))
				printf(":type=symlink");
			else if (S_ISFIFO(filestatbuf.st_mode))
				printf(":type=FIFO");
			else if (S_ISSOCK(filestatbuf.st_mode))
				printf(":type=socket");
			else if (S_ISCHR(filestatbuf.st_mode))
				printf(":type=cdev");
			else if (S_ISBLK(filestatbuf.st_mode))
				printf(":type=bdev");
			else
				printf(":type=UNKNOWN");
			printf("(%s)", ap->a_filetypename);
		}
		if (error)
			printf("\n");
		break;
	}

	if (ap->a_edit) {
#ifdef	CHANGE_REGULAR
		if (ap->a_userid != DONTCARE || ap->a_groupid != DONTCARE) {
			if (chown(path,
			    (ap->a_userid == DONTCARE) ? -1 : ap->a_userid,
			    (ap->a_groupid == DONTCARE) ? -1 : ap->a_groupid)
			    < 0) {
				fprintf(stderr, "Changing owner/group of ");
				perror(path);
			}
		}
		if (ap->a_mode != DONTCARE) {
			if (chmod(path, ap->a_mode) < 0) {
				fprintf(stderr, "Changing mode of ");
				perror(path);
			}
		}
#endif	/* CHANGE_REGULAR */
		update_attrs(path, ap);
	}
	return (error);
}

int
dotree(char *path, action_t *ap, int verbose)
{
	FILE *tfp;
	char *tmpfile = "/var/tmp/attrinit.XXXXXX";
	struct stat64 filestatbuf;
	int error = 0;

	if (lstat64(path, &filestatbuf) < 0) {
		if (ap->a_verify == VERIFY_ALL) {
			fprintf(stderr, "%s: \"%s\" is inaccessible. ",
				program, path);
			perror("Reason");
		}
		return (1);
	}
	if (!(S_ISDIR(filestatbuf.st_mode))) {
		if (ap->a_verify == VERIFY_ALL) {
			fprintf(stderr, "%s: \"%s\" is not a directory. ",
				program, path);
		}
		return (1);
	}
	if (!mktemp(tmpfile))
		return 1;
	/*
	 * Use find(1) to search the tree.
	 */
	switch (fork()) {
	case -1:
		return 1;
	case 0:
		/*
		 * Toss stderr unless verification was requested.
		 * find tends to complain if the target isn't there.
		 */
		if (ap->a_verify != VERIFY_ALL)
			(void)freopen("/dev/null", "w", stderr);
		if (!freopen(tmpfile, "w", stdout))
			exit(1);
		execl("/sbin/find","/sbin/find",path,"-local","-print",NULL);
		exit(1);
		break;
	default:
		wait(&error);
		if (error &= 0xffff) {
			unlink(tmpfile);
			return error;
		}
		break;
	}
	if (tfp = fopen(tmpfile, "r")) {
		char subpath[MAXPATHLEN + 1];
		char *cp;
		int line = 0;
		int line_total;

		if (verbose)
			line_total = count_lines(tfp);

		while (fgets(subpath, MAXPATHLEN, tfp)) {
			if (verbose)
				fprintf(stderr,
				    "\b\b\b\b\b\b\b\b\b\b\b\b\b(%5d/%5d)",
				    ++line, line_total);
			if ((cp = strchr(subpath, '\n')) == NULL) {
				error = 1;
				break;
			}
			*cp = '\0';
			error += doone(subpath, ap);
		}
		if (verbose)
			fprintf(stderr,
			    "\b\b\b\b\b\b\b\b\b\b\b\b\b             ");
		fclose(tfp);
	}
	else
		error = 1;

	unlink(tmpfile);
	return error;
}

/*
 * Keep these lists in sync!
 */
char *moldy_names[] = {
	"L-h", "H-e", "A-h", "T-b", "T-bzz",
};
char *moldy_labels[] = {
	"dblow", "msenhigh/mintequal", "dbadmin", "userlow", "binary",
};

int
domoldtree(char *path, action_t *base_ap, int verbose)
{
	action_t *ap;
	char subpath[MAXPATHLEN];
	char args[80];
	int i;
	int error = 0;

	/*
	 * Do the subdirectories First.
	 */
	for (i = 0; i < sizeof(moldy_names)/sizeof(moldy_names[0]); i++) {
		sprintf(subpath, "%s/%s", path, moldy_names[i]);
		sprintf(args, "-mac=%s", moldy_labels[i]);
		ap = copy_action(base_ap);
		add_action(ap, args);
		ap->a_filetype = DONTCARE;
		error += dotree(subpath, ap, verbose);
		free_action(ap);
	}
	/*
	 * And finally the directory itself.
	 */
	ap = copy_action(base_ap);
	ap->a_filetype = S_IFDIR;
	ap->a_filetypename = "directory";
	error += doone(path, ap);
	free_action(ap);

	return error;
}

enum state {S_DONE, S_QUOTE, S_COPY, S_ERROR, S_EATWHITE, S_STRING};
enum error_type {CLOSE_QUOTE, NO_MEMORY};

/*
 * print a message regarding errors during parsing and exit
 */
static void
parse_error (const char *line, enum error_type e)
{
    switch (e)
    {
	case CLOSE_QUOTE:
	    fprintf (stderr, "unterminated string: %s\n", line);
	    break;
	case NO_MEMORY:
	    fprintf (stderr, "out of memory\n", line);
	    break;
    }
    exit (1);
}

/*
 * free a list of words
 */
static void
free_words (char **words)
{
    char **tmp;

    if (words == (char **) NULL)
	return;

    tmp = words;
    while (*tmp != (char *) NULL)
	free (*tmp++);
    free (words);
}

/*
 * copy interval bytes ahead into the current position
 */
static void
copy_down (char *a, int interval)
{
    char *b = a + interval;

    while (*a++ = *b++)
	/* empty loop */;
}

/*
 * Break up a line into space-delimited words. Words may be grouped by ' or ".
 * Adjacent groups delimited by " or ' are separate entities.
 *
 * Return a list of strings terminated by a NULL pointer. Use free_words()
 * to deallocate space reserved for parse_line()s return value.
 */
static char **
parse_line (const char *line)
{
    int quotechar, i, nwords;
    char *begin, *end, *copy, **words;
    enum error_type e;
    enum state state;

    if (line == NULL)
	return ((char **) NULL);

    /*
     * allocate and initialize array of possible words
     */
    nwords = (strlen (line) + 1) / 2 + 1;
    words = (char **) malloc (nwords * sizeof (*words));
    if (words == NULL)
	parse_error (line, NO_MEMORY);
    for (i = 0; i < nwords; i++)
	words[i] = NULL;
    nwords = 0;

    /*
     * make working copy of input
     */
    copy = strdup (line);
    if (copy == NULL)
	parse_error (line, NO_MEMORY);

    end = copy;
    state = S_EATWHITE;

    do
    {
	switch (state)
	{
	    case S_EATWHITE:
		while (isspace (*end))
		    end++;
		begin = end;
		state = (*end == '\0' ? S_DONE : S_STRING);
		break;
	    case S_STRING:
		switch (*end)
		{
		    case '\0':
			state = S_COPY;
			break;
		    case '\'':
		    case '"':
			quotechar = *end;
			copy_down (end, 1);
			state = S_QUOTE;
			break;
		    default:
			if (isspace (*end))
			{
			    *end++ = '\0';
			    state = S_COPY;
			}
			else
			    end++;
		}
		break;
	    case S_QUOTE:
		while (*end != '\0' && *end != quotechar)
		    end++;
		if (*end == '\0')
		{
		    e = CLOSE_QUOTE;
		    state = S_ERROR;
		}
		else
		{
		    *end++ = '\0';
		    state = S_COPY;
		}
		break;
	    case S_COPY:
		words[nwords] = malloc (strlen (begin) + 1);
		if (words[nwords] == (char *) NULL)
		{
		    e = NO_MEMORY;
		    state = S_ERROR;
		}
		else
		{
		    strcpy(words[nwords++], begin);
		    state = S_EATWHITE;
		}
		break;
	    case S_ERROR:
		free (copy);
		free_words (words);
		parse_error (line, e);
		return ((char **) NULL);
	}
    } while (state != S_DONE);

    free (copy);
    return (words);
}

int
doscript(char *script, action_t *base_ap, int verbose)
{
	action_t *ap;
	int error = 0;
	int line = 0;
	int line_total = 0;
	char buffer[512];
	char *cp;
	char **words, **twords;
	FILE *fp;

	if ((fp = fopen(script, "r")) == NULL)
		arg_error("script", script);

	/*
	 * Count the lines if a running tally has been requested.
	 */
	if (verbose) {
		line_total = count_lines(fp);
		fprintf(stderr, "\n");
	}
	
	while (fgets(buffer, 510, fp)) {
		line++;
		if (verbose)
			fprintf(stderr, "\r%d/%d             ",line,line_total);

		if (cp = strchr(buffer, '#'))
			*cp = '\0';
		if (cp = strchr(buffer, '\n'))
			*cp = '\0';
		if (buffer[0] == '\0')
			continue;
		/*
		 * Retain what was specifed on the command line.
		 */
		ap = copy_action(base_ap);

		/*
		 * Find the new arguments.
		 */
		twords = words = parse_line (buffer);
		for (cp = *twords; cp != NULL; cp = *++twords) {
			if (*cp != '-')
				break;
			if ((ap = add_action(ap, cp)) == NULL) {
				script_error(script, line);
				error++;
				break;
			}
		}
		if (ap == NULL)
			continue;
		/*
		 * Get the pathnames
		 */
		for (; cp != (char *) NULL; cp = *++twords) {
			if (*cp == '-') {
				script_error(script, line);
				error++;
				break;
			}
			switch(ap->a_filetype) {
			case S_IS_A_TREE:
				error += dotree(cp, ap, verbose);
				break;
			case S_IS_A_MOLDTREE:
				error += domoldtree(cp, ap, verbose);
				break;
			default:
				error += doone(cp, ap);
			}
		}
		free_words (words);
		free_action(ap);
	}
	if (verbose)
		fprintf(stderr, "\nDone.\n");
	return error;
}

int
main(int argc, char *argv[])
{
	int i;
	int error = 0;
	int ssl = strlen("-script=");
	int verbose = 0;
	char *script = NULL;
	action_t *ap = NULL;

	program = argv[0];

	for (i = 1; i < argc; i++) {
		/*
		 * No '-' means the option list is complete.
		 */
		if (argv[i][0] != '-')
			break;
		if (!strncmp(argv[i], "-script=", ssl)) {
			script = argv[i] + ssl;
			continue;
		}
		if (!strcmp(argv[i], "-verbose")) {
			verbose = 1;
			continue;
		}
		if ((ap = add_action(ap, argv[i])) == NULL)
			exit(1);
	}

	if (ap == NULL && script == NULL) {
		fprintf(stderr, "%s: No attributes specified.\n", program);
		exit(1);
	}
	/*
	 * Do the script if it was specified.
	 */
	if (script)
		error += doscript(script, ap, verbose);

	/*
	 * Now loop through the desired paths.
	 */
	for (; i < argc; i++) {
		switch(ap->a_filetype) {
		case S_IS_A_TREE:
			error += dotree(argv[i], ap, verbose);
			break;
		case S_IS_A_MOLDTREE:
			error += domoldtree(argv[i], ap, verbose);
			break;
		default:
			error += doone(argv[i], ap);
		}
	}
	return(error);
}
