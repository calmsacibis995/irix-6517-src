
#ident "$Revision: 1.17 $"

/* Config for Tunable Parameters */

#include "lboot.h"
#include "boothdr.h"
#include "tune.h"
#include "errno.h"
#include <sys/types.h>
#include <stdio.h>
#include <sys/conf.h>

extern struct kernel kernel;
extern struct driver *driver;
extern char *slash_mtune;
extern char *stune_file;
char *tune_tag[MAXTAGS];		/* tag(s) on tune variables to pick */
int tune_num;				/* next free tune entry */
char mtune_file[LSIZE];
char linebuf[LSIZE];
struct tune *tune_info = NULL;
struct tunemodule *group_info = NULL;
int group_num;

static int getline(FILE *fp, int *);
static char *parse_mtune(char *mname, int *groups);
static char *parse_stune(struct stune *sp);

void
gen_tune(struct driver *dp, char *alternate_file)
{
	FILE *mtune;
	char *p;
	char *msg;
	int groups = 0, linenum = 0;
	struct tunemodule *gp;

	if (alternate_file)
		strcpy( mtune_file, alternate_file );
	else {
		strcpy( mtune_file, slash_mtune );
		strcat( mtune_file, "/" );
		strcat( mtune_file, dp->mname );
	}

	p=strrchr(mtune_file,'/');
	if ( (p=strchr(p,'.')) != NULL )
		*p = '\0';

	if ((mtune = fopen(mtune_file,"r")) == NULL)
		return;

	while (getline(mtune, &linenum) != 0) {
		if ((msg = parse_mtune(dp->mname, &groups)) != NULL) 
			error(ER55, mtune_file, linenum, msg);
	}
	if (groups == 0) {
		/* generate  tunemodule structure */
		gp = (struct tunemodule *)mymalloc(sizeof(struct tunemodule));
		gp->t_mname = mymalloc( strlen( dp->mname) + 1);
		strcpy(gp->t_mname, dp->mname);
		gp->t_gname = NULL;
		gp->t_flags = N_STATIC;
		gp->t_sanity = NULL;
		gp->t_next = group_info;
		group_info = gp;
		group_num++;
	}
}


/*
 * Read a line. Skip lines beginning with '*' or '#', and blank lines.
 * Return 0 if EOF. Return 1 otherwise.
 */
static int
getline(FILE *fp, int *ln)
{
	int linenum = *ln;

	for (;;) {
		if (fgets(linebuf, LSIZE, fp) == NULL) {
			linebuf[0] = '\0';
			return(0);
		}
		linenum++;
		if (*linebuf != '*' && *linebuf != '#' && *linebuf != '\n') {
			*ln = linenum;
			return(1);
		}
        }
}

static char *
chkstrtoull(char *str, long long *val, char *desc)
{
	char *chkp;
	unsigned long long v;
	static char ebuf[80];

	errno = 0;
	v = strtoull(str, &chkp, 0);
	if (errno == ERANGE) {
		sprintf(ebuf, "number out of range for %s value:%s", desc, str);
		return ebuf;
	}
	if (str == chkp) {
		sprintf(ebuf, "invalid number for %s value:%s", desc, str);
		return ebuf;
	}
	*val = (long long)v;
	return NULL;
}

static char *
chkstrtoul(char *str, long long *val, char *desc)
{
	char *chkp;
	unsigned long v;
	static char ebuf[80];

	errno = 0;
	v = strtoul(str, &chkp, 0);
	if (errno == ERANGE) {
		sprintf(ebuf, "number out of range for %s value:%s", desc, str);
		return ebuf;
	}
	if (str == chkp) {
		sprintf(ebuf, "invalid number for %s value:%s", desc, str);
		return ebuf;
	}
	*val = (long long)v;
	return NULL;
}

/*
 * parse a tune file
 * there are 2 main formats - a grtoup identifier and the tune variables
 * themselves
 * name[,tag] default [min [max [size]]]
 */
static char *
parse_mtune(char *mname, int *groups)
{
	register int argc;
	char *argv[256], **argvp;
	struct tune *tp;
	struct tunemodule *gp;
	static char ebuf[80], *errstr;

	argc = parse(argv,sizeof(argv)/sizeof(argv[0]),linebuf);
	if (argc > sizeof(argv)/sizeof(argv[0]))
		return("line too long");
	if (argc < 2)
		return("not enough fields in line!");

	if (*argv[1] != ':') {
	
		/*
		 * tunable parameter
		 * If the tunable has a 'tag' then check that
		 * If they haven't provided ANY tag value (via -O) we
		 * ignore ALL tagged values.
		 * We set argvp to point to the value for 'default'
		 * We also adjust argc to reflect # of args AFTER name/tag
		 */
		if (*argv[1] == ',') {
			int i;
			if (argc <= 2)
				return("Not enough args for tagged variable");

			for (i = 0; i < tune_num; i++)
			    if (strcmp(argv[2], tune_tag[i]) == 0)
				break; /* match */
			if (i >= tune_num)
				/* no match */
				return NULL;
			argvp = &argv[3];
			argc -= 3;
		} else {
			argvp = &argv[1];
			argc--;
		}
		if (argc < 1)
			return("no default value");

		if (tunefind(argv[0])) {
			sprintf(ebuf, "duplicate tune variable:%s", argv[0]);
			return ebuf;
		}

		tp = (struct tune *)mymalloc(sizeof(struct tune));
		tp->tname = mymalloc( strlen(argv[0]) + 1);
		strcpy(tp->tname, argv[0]);
		tp->tmin = 0;
		tp->tmax = 0;
		tp->type = TUNE_TYPE_32;

		if ((argc > 3) && ((strcmp(argvp[3], "LL") == 0) ||
				   (strcmp(argvp[3], "ll") == 0))) {
			tp->type = TUNE_TYPE_64;
			if ((errstr = chkstrtoull(argvp[0], &tp->def,
							"default")) != NULL)
				return errstr;
			if (argc > 1) {
				if ((errstr = chkstrtoull(argvp[1], &tp->tmin,
							"minimum")) != NULL)
					return errstr;
			}
			if (argc > 2) {
				/* don't check for valid 'max' - some folks
				 * like to put words in as comments ...
				 * should actually change tmax to be __uint64_t...
				 */
				tp->tmax = (long long)strtoull(argvp[2], 0, 0);
			}
		} else {
			if ((errstr = chkstrtoul(argvp[0], &tp->def, "default")) != NULL)
				return errstr;
			if (argc > 1) {
				if ((errstr = chkstrtoul(argvp[1], &tp->tmin,
							"minimum")) != NULL)
					return errstr;
			}
			if (argc > 2) {
				/* don't check for valid 'max' - some folks
				 * like to put words in as comments ...
				 */
				tp->tmax = (long long)strtoul(argvp[2], 0, 0);
			}
		}

		tp->svalue = tp->def;
		tp->conf = 0;
		if (!*groups) {
			tp->group = group_num;
		}
		else {
			tp->group = group_num - 1;
		}
		tp->next = tune_info;
		tune_info = tp;
	} else {
		
		/* group name */

		gp = (struct tunemodule *)mymalloc(sizeof(struct tunemodule));
		gp->t_mname = mymalloc( strlen(mname)+ 1);
		strcpy(gp->t_mname, mname);
		gp->t_gname = mymalloc( strlen(argv[0]) + 1);
		strcpy(gp->t_gname, argv[0]);
		if ((argc > 2) && (EQUAL("run", argv[2])))
			gp->t_flags = N_RUN;
		else 
			gp->t_flags = N_STATIC;
		gp->t_sanity = NULL;
		gp->t_next = group_info;
		group_info = gp;
		(*groups)++;
		group_num++;
		
	}
	return(NULL);
}

/* 
 * rdstune - System tunable parameter file 
 */
void
rdstune(void)
{
	struct stune stune;
	register struct tune *tune;
	FILE *stunep;
	char *msg, ebuf[128];
	int linenum = 0;

	if ((stunep = fopen(stune_file, "r")) == NULL)
		return;
	
	while (getline(stunep, &linenum) != 0) {
		if ((msg = parse_stune(&stune)) != NULL) {
			error(ER110, stune_file, linenum, msg);
			continue;
		}
		if (stune.s_name[0] == '\0')
			continue;

		/* find tunable in Parameter table */
		tune = tunefind(stune.s_name);
	
		if (tune == NULL) {
			sprintf(ebuf, "unknown tunable parameter '%s' - Ignoring",
				stune.s_name);
			error(ER110, stune_file, linenum, ebuf);
			continue;
		}

                /* check if already specified */
                if (tune->conf) {
                        sprintf(ebuf, "tunable parameter '%s' respecified - ignoring 2nd definition", 
					stune.s_name);
			error(ER110, stune_file, linenum, ebuf);
                        continue;
                }

		/* make sure types match */
		if (stune.s_type != tune->type) {
			sprintf(ebuf, " Ignoring '%s' because the type doesn't match\n\tCheck for missing \" LL\" or incorrect \" LL\" at end of line",
				stune.s_name);
			error(ER110, stune_file, linenum, ebuf);
			continue;
		}
                /* check whether parameter is within min and max */
		if (tune->tmax != 0) {
			if (stune.s_value < tune->tmin ||
			    stune.s_value > tune->tmax) {
				sprintf(ebuf, "the value of parameter '%s', %lld, must be within (%lld, %lld) - Ignoring", 
						stune.s_name, stune.s_value, 
						tune->tmin, tune->tmax);
				error(ER110, stune_file, linenum, ebuf);
				continue;
			}
		} else if (tune->tmin != 0) {
			if (stune.s_value < tune->tmin) {
				sprintf(ebuf, "the value of parameter '%s', %lld, must be greater than %lld - Ignoring",
					stune.s_name, stune.s_value,tune->tmin);
				error(ER110, stune_file, linenum, ebuf);
				continue;
			}
		}

                /* store value in Parameter table */
		tune->svalue = stune.s_value;

                /* indicate tunable parameter specified */
                tune->conf = 1;
        }

}

/* This routine is used to search the Parameter table
 * for the keyword that was specified in the configuration.  If the
 * keyword cannot be found in this table, a NULL is returned.
 * If the keyword is found, a pointer to that entry is returned.
 */
struct tune * 
tunefind(char *keyword)
{
        struct tune *tune;

	for (tune = tune_info; tune != NULL; tune = tune->next) {
                if (EQUAL(keyword, tune->tname))
                        return(tune);
        }
        return(NULL);
}

struct tunemodule *
gfind(char *keyword)
{
        register struct tunemodule *tune;

	for (tune = group_info; tune != NULL; tune = tune->t_next) {
                if (EQUAL(keyword, tune->t_mname))
                        return(tune);
        }
        return(NULL);
}

/*
 * stune file:
 * name[,tag] = value [ll]
 */
static char *
parse_stune(struct stune *sp)
{
	register int argc;
	char *argv[256], **argvp, *errstr;

	sp->s_name[0] = '\0';
	argc = parse(argv,sizeof(argv)/sizeof(argv[0]),linebuf);
	if (argc > sizeof(argv)/sizeof(argv[0]))
		return("line too long");

	/*
	 * check for tag - re-position argvp and argc to point to '='
	 */
	if (argc>1 && *argv[1] == ',') {
		int i;
		if (argc <= 2)
			return("syntax error<name[,tag] = value [ll]>");

		for (i = 0; i < tune_num; i++)
		    if (strcmp(argv[2], tune_tag[i]) == 0)
			break; /* match */
		if (i >= tune_num)
			/* no match */
			return NULL;
		argvp = &argv[3];
		argc -= 3;
	} else {
		argvp = &argv[1];
		argc--;
	}
	if (argc < 2 || *argvp[0] != '=')
		return("syntax error<name[,tag] = value [ll]>");

	strcpy(sp->s_name, argv[0]);
	sp->s_type = TUNE_TYPE_32;

	if ((argc == 3) && ((strcmp(argvp[2], "LL") == 0) ||
			    (strcmp(argvp[2], "ll") == 0))) {
		sp->s_type = TUNE_TYPE_64;
		if ((errstr = chkstrtoull(argvp[1], &sp->s_value, "value")) != NULL)
			return errstr;
	} else
		if ((errstr = chkstrtoul(argvp[1], &sp->s_value, "value")) != NULL)
			return errstr;
	return(NULL);
}

/* 
 * print out tunable Parameter table
 */
void
print_tune(FILE *f)
{
        register struct tune *tune;
        register struct tunemodule *gp;
	int tuneentries = 0;

	for (gp = group_info; gp != NULL; gp = gp->t_next) {
		if (gp->t_sanity)
			fprintf(f, "extern %s();\n", gp->t_sanity);
	}
	fprintf( f, "\nstruct tunetable tunetable[] = {\n");
	for (gp = group_info; gp != NULL; gp = gp->t_next) {
		fprintf(f, "\t{ ");
		if (gp->t_gname)
			fprintf(f, "\"%s\",", gp->t_gname);
		else
			fprintf(f, "\"%s\",", gp->t_mname);
		if (gp->t_flags & N_RUN)
			fprintf(f, "N_RUN,");
		else if (gp->t_flags & N_STATIC)
			fprintf(f, "N_STATIC,");
		if (gp->t_sanity)
			fprintf(f, "%s},\n", gp->t_sanity);
		else 
			fprintf(f, "0},\n");
		tuneentries++;
	}
	fprintf( f, "   {0, 0, 0},\n};\n" );
        
        fprintf(f, "\n/* defines for each tunable parameter */\n");

	for (tune = tune_info; tune != NULL; tune = tune->next) {
		if (tune->type == TUNE_TYPE_64)
                	fprintf(f, "long long\t%s = \t0x%llxll;\n",
                        	tune->tname, tune->svalue);
		else {
			int val = (int)tune->svalue;	/* avoid overflow */

                	fprintf(f, "int\t%s = \t0x%x;\n",
                        	tune->tname, val);
		}
	}
	fprintf(f, "int\ttuneentries = \t%d;\n\n", tuneentries+1);

	/* Generate the tunename table.  It is used to match up the name
	 * of a tuneable with its address and tunetable group index.
	 */

	fprintf( f, "\nstruct tunename tunename[] = {\n");
	tuneentries = 0;
	for (tune = tune_info; tune != NULL; tune = tune->next) {
		fprintf(f, "\t{\"%s\", (int *)&%s, sizeof(%s), %d},\n", 
			tune->tname, tune->tname, tune->tname, 
			group_num - tune->group - 1);
		tuneentries++;
	}
	fprintf( f, "   {0, 0, 0, 0},\n};\n" );
	fprintf(f, "int\ttunename_cnt = \t%d;\n\n", tuneentries+1);
}

/*
 * find_kernel_sanity
 *
 * Called for each global procedure to detect if it
 * is a tune sanity procedure, and if so, register it.
 * XXX no real reason this can't be used for any  sanity....
 */
/* ARGSUSED */
void
find_kernel_sanity(char *procname, void *arg0, void *arg1, void *arg2)
{
	struct tunemodule *tmpgp;
	char mgname[80];
	int got_sanity;
	struct tunemodule *gp = (struct tunemodule*)arg1;

	/* build up tune_sanity functions */
	tmpgp = gp;
	got_sanity = 0;
	while ((!got_sanity) && tmpgp) {
		strcpy(mgname, "_");
		if (tmpgp->t_gname) 
			strcat(mgname, tmpgp->t_gname);
		else
			strcat(mgname, tmpgp->t_mname);
		if (function(mgname, "_sanity", procname)) {
			tmpgp->t_sanity =
				strcpy(mymalloc(strlen(procname)+1),procname);
			got_sanity++;
		}
		tmpgp = tmpgp->t_next;
	}
}
