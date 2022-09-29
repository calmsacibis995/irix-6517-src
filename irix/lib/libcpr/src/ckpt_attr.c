/**************************************************************************
 *									  *
 * 		 Copyright (C) 1995 Silicon Graphics, Inc.	  *
 *									  *
 *  These coded instructions, statements, and computer programs  contain  *
 *  unpublished  proprietary  information of Silicon Graphics, Inc., and  *
 *  are protected by Federal copyright law.  They  may  not be disclosed  *
 *  to  third  parties  or copied or duplicated in any form, in whole or  *
 *  in part, without the prior written consent of Silicon Graphics, Inc.  *
 *									  *
 **************************************************************************/
#ident "$Revision: 1.42 $"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <fcntl.h>
#include <pwd.h>
#include <ctype.h>
#include <widec.h>
#include <wctype.h>
#include <unistd.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/procfs.h>
#include <sys/ckpt_procfs.h>
#include <sys/stat.h>
#include "ckpt.h" /* for now */
#include "ckpt_internal.h"

typedef struct ckpt_func {
	struct ckpt_func *f_next;
	int		f_func;		/* func name */
	void		*f_instance;	/* instance if any */
	int		f_action;	/* action that users select */
} ckpt_func_t;

/*
 * CPR attribute record defined in the CPR control file $(HOME)/.cpr 
 * of THE USER WHO STARTS A CPR.
 */
typedef struct ckpt_attr_rec {
	struct ckpt_attr_rec *ar_next;	/* link to all other in-core records */
	pid_t		ar_id;		/* POSIX id value */
	u_long		ar_type;	/* what type of record this is */
	ckpt_func_t	*ar_funcs;	/* func list in this record */
} car_t;


static struct strmap rec_type[] = {
	CKPT_PROCESS,	"PID",		/* control cpr by proc id */
	CKPT_PGROUP,	"GID",		/* by proc group id */
	CKPT_SESSION,	"SID",		/* by session id */
	CKPT_ARSESSION,	"ASH",		/* by array session id */
	CKPT_HIERARCHY,	"HID",		/* process hierarchy rooted at pid */
	CKPT_SGROUP,	"SGP"		/* share group */
};

static struct strmap rec_funcs[] = {
	CKPT_FILE,	"FILE",
	CKPT_WILL,	"WILL",
	CKPT_CDIR,	"CDIR",
	CKPT_RDIR,	"RDIR",
	CKPT_FORK,	"FORK",
	CKPT_RELOCDIR,	"RELOCATE",
};

static struct strmap rec_actions[] = {
	CKPT_MEGRE,		"MERGE",
	CKPT_REPLACE,		"REPLACE",
	CKPT_SUBSTITUTE,	"SUBSTITUTE",
	CKPT_APPEND,		"APPEND",
	CKPT_IGNORE,		"IGNORE",
	CKPT_RELOCATE,		"RELOCATE",
	CKPT_EXIT,		"EXIT",
	CKPT_EXIT,		"KILL",
	CKPT_CONT,		"CONT",
	CKPT_FORKORIG,		"ORIGINAL",
	CKPT_FORKANY,		"ANY",
};

#define	CKPTIDLEN	64

#define	INSERT_HEAD	0
#define	INSERT_TAIL	1

#define	WILDCHAR(c)	((c) == '*')
#define	SKIPLINE(c)	((c) == '#' || (c) == '!')
#define	ISEND(c)	((c) == '\n' || (c) == 0)
#define	SKIPSPACE(p)	{while (!isalnum(*(p)) && !WILDCHAR(*(p)) \
			&& !ISEND(*(p) && *(p)!='}')) (p)++;}
#define	SKIPALNUM(p)	{while (isalnum(*(p)) && !ISEND(*(p))) (p)++;}

#define	FOR_EACH_RECORD(first, rp)	for (rp = first; rp; rp = rp->ar_next)

static void ckpt_load_record(ch_t *);
static void ckpt_read_rec(ch_t *, char *, int);
static int ckpt_match_wildpath(char *, char *);
static int ckpt_check_one_record(car_t *, char *, dev_t, ino_t, int);

static car_t *car_head = NULL;
static int ckpt_file_default_mode = CKPT_MODE_MERGE;

/*
 * Definitions for cache_flags
 */
static int cache_flags = 0;

#define	HEAD_ATTR_INSERTED	0x1
#define	TAIL_ATTR_INSERTED	0x2
#define	NO_ATTR_EXIST		0x4


int
ckpt_attr_get_mode(	ch_t *ch,
			char *path,
			dev_t dev,
			ino_t ino,
			mode_t mode,
			int unlinked,
			int ispsema)
{
	car_t *rp;
	int rc;

	if ((S_ISFIFO(mode)) || (ispsema))
		return (CKPT_MODE_REPLACE);

	ckpt_load_record(ch);

	/*
	 * If there are multiple records for a same target, the first one
	 * in the list will be chosen to return the mode.
	 */
	FOR_EACH_RECORD(car_head, rp) {
		if (rp->ar_id != ch->ch_id || rp->ar_type != ch->ch_type)
			continue;

		if ((rc = ckpt_check_one_record(rp, path, dev, ino, unlinked)) != -1)
			return (rc);
	}
        if (unlinked)
                return (CKPT_MODE_REPLACE);

	/* default */
	return (ckpt_file_default_mode);
}

static int
ckpt_check_one_record(car_t *rp, char *path, dev_t dev, ino_t ino, int unlinked)
{
	ckpt_func_t *fp;

	fp = rp->ar_funcs;	
	for (; fp; fp = fp->f_next) {
		ckpt_flist_t *file = (ckpt_flist_t *)fp->f_instance;

		if (fp->f_func != CKPT_FILE)
			continue;
		/*
		 * If the file is matched, get its user defined action.
		 */
		if ((file->cl_ino == ino) && (file->cl_dev == dev)) {
			IFDEB2(cdebug("Found path=%s action=%s\n",
				path, rec_actions[fp->f_action].s_id));
			return (fp->f_action);
		}
	}
	/*
	 * Test if users are using wildcards
	 */
	fp = rp->ar_funcs;	
	for (; fp; fp = fp->f_next) {
		ckpt_flist_t *file = (ckpt_flist_t *)fp->f_instance;

		if (fp->f_func != CKPT_FILE)
			continue;

		if (file->cl_dev == -1 && file->cl_wildpath && path) {
			if (unlinked) {
				char tmp[MAXPATHLEN];
				char *dir;
				int lchar;
  
				strcpy(tmp, file->cl_wildpath);
				dir = strtok(tmp, "*");
				lchar = (int)strlen(dir)-1;
  
				while(dir[lchar] == '/') {
					dir[lchar] = '\0';
					lchar--;
				}
				if (strcmp(dir, path) == 0)
					return (fp->f_action);
  
			} else if (ckpt_match_wildpath(file->cl_wildpath, path)) {
				IFDEB2(cdebug("Found wildmatch path=%s action=%s\n",
					path, rec_actions[fp->f_action].s_id));
				return (fp->f_action);
			}
		}
	}
	return (-1);
}

int
ckpt_attr_use_default(ch_t *ch, int func, int default_action)
{
	car_t *rp;
	ckpt_func_t *fp;

	ckpt_load_record(ch);

	FOR_EACH_RECORD(car_head, rp) {
		if (rp->ar_id != ch->ch_id || rp->ar_type != ch->ch_type)
			continue;

		IFDEB2(cdebug("recp=%x id=%d type=%d\n", rp, rp->ar_id, rp->ar_type));
		fp = rp->ar_funcs;	
		for (; fp; fp = fp->f_next) {
			if (fp->f_func == func) {
				IFDEB2(cdebug("func=%s action=%s (default=%s)\n",
					rec_funcs[func].s_id,
					rec_actions[fp->f_action].s_id,
					rec_actions[default_action].s_id));

				if (fp->f_action != default_action)
					return (0);

				return (1);
			}
		}
	}
	return (1);
}

int
atol_type(u_long *type, char *p)
{
	int i;

	for (i = 0; i < sizeof (rec_type)/sizeof (struct strmap); i++) {
		if (strlen(rec_type[i].s_id) != strlen(p))
			continue;
		if (!(strncmp(p, rec_type[i].s_id, strlen(rec_type[i].s_id)-1))) {
			*type = rec_type[i].id;
			return (0);
		}
	}
	cerror("Unrecoginized checkpoint type %s\n", p); 
	return (-1);
}

char *
ckpt_type_str(ckpt_type_t type)
{
	int i;

	for (i = 0; i < sizeof (rec_type)/sizeof (struct strmap); i++) {
		if (rec_type[i].id == type)
			return (rec_type[i].s_id);
	}
	cerror("Unrecoginized checkpoint type %d\n", type); 
	return (NULL);
}

char *
ckpt_action_str(u_long mode)
{
	int i;

	for (i = 0; i < sizeof (rec_actions)/sizeof (struct strmap); i++) {
		if (rec_actions[i].id == mode)
			return (rec_actions[i].s_id);
	}
	cerror("Unrecoginized checkpoint mode %d\n", mode); 
	return (NULL);
}

static int
find_type(char *p, ckpt_type_t type)
{
	int i;

	if (WILDCHAR(*p))
		return (1);

	for (i = 0; i < sizeof (rec_type)/sizeof (struct strmap); i++) {
		if ((u_long)rec_type[i].id == type)
			break;
	}
	IFDEB2(cdebug("requested type: %s v.s. attr type: %4.4s\n", 
		rec_type[i].s_id, p));

	if (!(strncmp(p, rec_type[i].s_id, strlen(rec_type[i].s_id)-1)))
		return (1);

	return (0);
}

static int
find_id(char *p, pid_t id)
{
	char s_id[CKPTIDLEN];
	int i = 0;

	if (WILDCHAR(*p))
		return (1);

	while (isdigit(*p)) s_id[i++] = *p++;

	s_id[i] = 0;

	IFDEB2(cdebug("requested id: %d v.s. attr id: %d\n", id, atoi(s_id)));

	if (id == atoi(s_id))
		return (1);

	return (0);
}

static int
find_func(char *p)
{
	int i = 0;

	for (i = 0; i < sizeof (rec_funcs)/sizeof (struct strmap); i++) {
		if (!(strncmp(p, rec_funcs[i].s_id, strlen(rec_type[i].s_id)-1)))
			return (rec_funcs[i].id);
	}
	/* NOTREACHED */
	return (0);
}

static int
find_action(char *p)
{
	int i = 0;

	while (*p == '"' || *p == ':' || isspace(*p)) p++;

	for (i = 0; i < sizeof (rec_actions)/sizeof (struct strmap); i++) {
		if (!(strncmp(p, rec_actions[i].s_id, strlen(rec_actions[i].s_id)-1)))
			return (rec_actions[i].id);
	}
	/* NOTREACHED */
	return (0);
}

static car_t *
rec_alloc(int where)
{
	car_t *rp = car_head, *newp;

	if ((newp = (car_t *)malloc(sizeof (car_t))) == NULL) {
		cerror("Failed to allocate memory (%s)\n", STRERR);
		return (NULL);
	}
	newp->ar_next = NULL;

	if (car_head == NULL) {
		car_head = newp;
		return (newp);
	}
	if (where == INSERT_TAIL) {
		while (rp->ar_next != NULL) rp = rp->ar_next; 
		rp = newp;
	} else if (where == INSERT_HEAD) {
		newp->ar_next = car_head;	
		car_head = newp;
	}
	return (newp);
}

static void
ckpt_load_record(ch_t *ch)
{
	char *home;
	struct passwd *pp;
	char attrfile[CPATHLEN];

	if (!(cache_flags & HEAD_ATTR_INSERTED)) {
		sprintf(attrfile, "%s.%d.%lld", CKPT_ATTRFILE_GUI, getuid(), ch->ch_id);
		ckpt_read_rec(ch, attrfile, INSERT_HEAD);
	}
	/*
	 * return if we have in-core records already , or if no attrfile is found
	 */
	if (cache_flags & (TAIL_ATTR_INSERTED|NO_ATTR_EXIST))
		return;
	/*
	 * The attribute control file is defined by the user who initaites the cpr 
	 * request not the user who started the application, assuming that the first 
	 * user has the necessary permission to checkpoint the application.
	 */
	if ((home = getenv("HOME")) != NULL) {
		/*
		 * prefer the environ variable
		 */
		sprintf(attrfile, "%s/%s", home, CKPT_ATTRFILE);
		IFDEB2(cdebug("attrfile=%s HOME=%s\n", attrfile, home));
	} else {
		/*
		 * resort to pw entry
		 */
		pp = getpwuid(getuid());
		if (pp == NULL) {
#ifdef DEBUG
			cnotice("getpwuid returned NULL (%s)\n", STRERR);
#endif
			return;
		}
		sprintf(attrfile, "%s/%s", pp->pw_dir, CKPT_ATTRFILE);
		IFDEB2(cdebug("attrfile=%s pw_dir=%s pw_uid=%d\n",
			attrfile, pp->pw_dir, pp->pw_uid));
		endpwent();
	}
	ckpt_read_rec(ch, attrfile, INSERT_TAIL);
}

static void
ckpt_read_rec(ch_t *ch, char *path, int where)
{
	char line[CPATHLEN];
	int rec_start = 0, count = 0;
	pid_t id = (pid_t)ch->ch_id;
	ckpt_type_t type = ch->ch_type;
	FILE *fp;
	car_t *rp;
	struct stat sb;

	if ((fp = fopen(path, "r")) == NULL) {
		if (where == INSERT_TAIL) {
			if (!(cpr_flags & CKPT_NQE)) {
				cnotice("CPR attrifile %s is not found\n", path);
			}
			cache_flags |= NO_ATTR_EXIST;
		}
		return;
	}
	if (where == INSERT_TAIL)
		cache_flags |= TAIL_ATTR_INSERTED;
	else if (where == INSERT_HEAD)
		cache_flags |= HEAD_ATTR_INSERTED;

	/*
	 * scan each processing record in the attribute control file and
	 * create in-core copy until we find the func we are looking for
	 */
	while (fgets(line, CPATHLEN, fp)) {
		char *p = line;
		ckpt_func_t *f, *fprev;

		count++;

		if (SKIPLINE(*p))
			continue;
		
		SKIPSPACE(p);

		if (ISEND(*p))
			continue;

		IFDEB2(cdebug("record line=%s", p));

		if (!strncmp(p, "CKPT", 4)) {
			rec_start++;
			p += 4;
			SKIPSPACE(p);

			if (!find_type(p, type)) {
				rec_start = 0;
				continue;
			}

			SKIPALNUM(p);
			SKIPSPACE(p);

			if (!find_id(p, id)) {
				rec_start = 0;
				continue;
			}

			if ((rp = rec_alloc(where)) == NULL)
				break;

			rp->ar_type = (u_long)type;
			rp->ar_id = id;
			rp->ar_funcs = NULL;

			continue;
		} else if (rec_start && *p == '}') {
			rec_start = 0;
			break;		/* found and finished reading the record */

		} else if (!rec_start) {
			/* skip the line */
			continue;
		}

		/* now we are processing the body of a record */
		if ((f = (ckpt_func_t *)malloc(sizeof(ckpt_func_t))) == NULL) {
			cerror("Failed to allocate memory (%s)\n", STRERR);
			break;
		}
		switch (f->f_func = find_func(p)) {
		case CKPT_FILE:
		{
			ckpt_flist_t *fp;
			char path[MAXPATHLEN];
			int is_partial_wildcard = 0;
			int n;

			/* skip the junk */
			while (*p != '/' && !WILDCHAR(*p))  p++;
			if (ISEND(*p)) {
				cerror("File %s has a bad record entry in line %d\n",
					path, count);
				break;
			}	
			n = 0;
			while (*p != '"' && *p != ':' &&
				!isspace(*p) && n < (MAXPATHLEN-1)) {
				if (WILDCHAR(*p))
					is_partial_wildcard = 1;
				path[n++] = *p++;
			}
			path[n] = 0;
			IFDEB2(cdebug("path=%s\n", path));

			/*
			 * If users gives system level wild card,
			 * change the file disposition default then
			 */
			if (strcmp(path, "*") == 0 || strcmp(path, "/*") == 0) {
				IFDEB2(cdebug("changing file default\n"));
				ckpt_file_default_mode = find_action(p);
				free(f);
				f = NULL;
				break;
			}
			if ((fp = (ckpt_flist_t *)malloc(sizeof(ckpt_flist_t)))
				== NULL) {
				cerror("Failed to alloc memory (%s)\n", STRERR);
				free(f);
				f = NULL;
				break;
			}
			/*
			 * If partial wild card, create a directory wild card list
			 */
			if (is_partial_wildcard) {
				if ((fp->cl_wildpath = malloc(CPATHLEN)) == NULL) {
					cerror("Failed to alloc mem (%s)\n", STRERR);
					free(f);
					f = NULL;
					break;
				}
				fp->cl_dev = -1;
				fp->cl_next = NULL;
				strcpy(fp->cl_wildpath, path);
			} else {
				/*
				 * Normal file disposition mode
				 */
				if (stat(path, &sb) < 0) {
					IFDEB1(cdebug("ATTR: %s doesn't exist\n", path));
					free(f);
					free(fp);
					f = NULL;
					break;
				}
				IFDEB2(cdebug("dev=%d ino=%d\n",sb.st_dev,sb.st_ino));
				fp->cl_dev = sb.st_dev;
				fp->cl_ino = sb.st_ino;
				fp->cl_next = NULL;
				fp->cl_wildpath = NULL;
			}
			f->f_instance = (void *)fp;
			f->f_action = find_action(p);
			f->f_next = NULL;
			break;
		}
		case CKPT_RELOCDIR:
		{
			char path[MAXPATHLEN];
			int n;
			/*
			 * Not adding anything to action list
			 */
			free(f);
			f = NULL;

			/* skip the junk */
			while (*p != '/')  p++;
			if (ISEND(*p)) {
				cerror("File %s has a bad record entry in line %d\n",
					path, count);
				break;
			}
			n = 0;
			while (*p != '"' && !isspace(*p) && n < (MAXPATHLEN-1))
{
				path[n++] = *p++;
			}
			path[n] = 0;
			IFDEB2(cdebug("path=%s\n", path));

			if (stat(path, &sb) < 0) {
				IFDEB1(cdebug("ATTR: %s doesn't exist\n", path));
				cnotice("RELOCATE directory %s does not exist\n",
					path);
				break;
			}
			if ((sb.st_mode & S_IFMT) != S_IFDIR) {
				cnotice("RELOCATE path %s is not a directory\n",					path);
				break;
			}
			strcpy(ch->ch_relocdir, path);
			break;
		}
		case CKPT_WILL:
		case CKPT_CDIR:
		case CKPT_RDIR:
		case CKPT_FORK:
		{
			if (ISEND(*p)) {
				cerror("File %s has a bad record entry in line %d\n",
					path, count);
				break;
			}	
			SKIPALNUM(p);
			SKIPSPACE(p);
			f->f_instance = NULL;
			f->f_action = find_action(p);
			f->f_next = NULL;

			IFDEB2(cdebug("func=%s action=%s\n",
				rec_funcs[f->f_func].s_id,
				rec_actions[f->f_action].s_id));
			break;
		}
		default:
			rec_start = 0;
			cerror("Bad entry in %s record %s %d\n",
				path, rec_type[rp->ar_id].s_id, rp->ar_id);
			break;
		}

		/*
		 * link any valid record func (an entry)
		 */
		if (f == NULL)
			continue;

		if (fprev = rp->ar_funcs) {
			while (fprev->f_next) fprev = fprev->f_next;
			fprev->f_next = f;
		} 
		else 
			rp->ar_funcs = f;

	}
	fclose(fp);
}

static int
ckpt_match_wildpath(char *wildcard, char *path)
{
	ulong token_len, path_len, loc = 0;
	char *token, f_char, l_char, w_tmp[CPATHLEN], last[CPATHLEN];

	assert(wildcard && path);

	f_char = *wildcard;
	l_char = *(wildcard+strlen(wildcard)-1);
	path_len = strlen(path);
	IFDEB2(cdebug("wildcard=%s path=%s (len=%d)\n", wildcard, path, path_len));

	strcpy(w_tmp, wildcard);
	while ((token = strtok(w_tmp, "*")) != 0) {
		token_len = strlen(token);
		while (strncmp(token, path+loc, token_len) != 0) {
			loc++;
			if (f_char != '*' || (loc + token_len) > path_len)
			        return (0);
		}
		IFDEB2(cdebug("partial match: %s with path %s\n", token, path+loc));
		loc += token_len;
		w_tmp[0] = NULL;
		strcpy(last, token);
	}
	/*
	 * If last token is *
	 */
	if (l_char != '*') {
		if (strncmp(last, path+path_len-token_len, token_len) != 0)
			return (0);
	}
	return (1);
}
