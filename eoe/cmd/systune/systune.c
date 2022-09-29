
#ident "$Revision: 1.24 $"

#include "tune.h"
#include <nlist.h>
#include <ctype.h>
#include <fcntl.h>
#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/sysmacros.h>
#include <sys/conf.h>
#include <sys/syssgi.h>
#include <sys/sysget.h>
#include <sys/capability.h>

#ifdef DOCOFF
#include <a.out.h>
#include <filehdr.h>
#include <syms.h>
#include <ldfcn.h>
#endif
#ifdef DOELF
#include <libelf.h>
#endif

char *root;
char *slash_mtune = "/var/sysgen/mtune";
char *stune_file = "/var/sysgen/stune";
char *system_file = "/var/sysgen/system/irix.sm";
char *tag_sign = "TUNE-TAG";
char mtune_file[LSIZE];
char linebuf[LSIZE];
struct tunemodule *group_info = NULL;
struct tunetable *tunetable;
char *unixname = "/unix";
char *unixinstall = "/unix.install";
int both = 1;
int running = 0;
int force = 0;
int interactive = 0;
#define MAXTAGS 32		/* Also defined in lboot/lboot.h */
char *tune_tags[MAXTAGS];
int tags = 0;

#if (_MIPS_SZLONG == 64)
#define MASK 0x7fffffffffffffff
#define NLIST nlist64
#else
#define MASK 0x7fffffff
#define NLIST nlist
#endif

struct NLIST nl[2];
struct nlist64 nltest;
struct nlist nltest_32;

/*
 * This is currently the only case where the value
 * is negative.  Any additional variables should
 * be added here.
 */
#define SIGNED(x) ((strcmp("reboot_on_panic",x) == 0) ||\
		   (strcmp("timetrim",x) == 0))

#define ATOL(s)		((long long) strtoull((s), 0, 0))

static int askname(char **s, long long *count);
static long find_data_offset(int);
static int getline(FILE *fp);
static struct tunemodule *gfind(char *);
static int parse(char **, unsigned , char *);
static char *parse_mtune(int *);
static char *mymalloc(size_t);
static void dumpgroup(struct tunemodule *);
static int dumpvariable(char *);
static char *findvariables(char *, int *);
static void setvalue(char *, long long);
static int tfind(char *, struct tune **, struct tunemodule **);
static char *parse_stune(struct stune *sp);
static char *concat(char *, char*);
static void gen_tune(char *);
static void ed_stune(struct tune *, long long);
static void rdstune(void);
static void read_tunables(void);
static void read_tunetable(void);
static void help(void);

void usage(void)
{
	fprintf(stderr,
		"Usage: systune [-brf] "
		"[-n system] [-p root path] "
		"[-i | variable [value]]\n");
	exit(1);
}

void
main(int argc, char **argv)
{
	int c;
	int cp = 0;
	int sfd;
	char *s, *sp;
	long long count;
        struct tunemodule *gp;
	int vtype;
	int mode;
#ifdef DOCOFF
	SCNHDR  shdr;
	LDFILE *ldptr = NULL;
#endif
#ifdef DOELF
	long offset;
#endif
	FILE *fp;
	char tempbuf[LSIZE];
	
	while ((c = getopt(argc, argv, "brfin:p:")) != EOF)

		switch (c) {

                case 'b':
                        both++;
                        break;

                case 'r':
                        running++;
			both = 0;
                        break;

                case 'f':
                        force++;
                        break;

                case 'i':
                        interactive++;
                        break;

		case 'n':
			unixname = optarg;
			break;

		case 'p':
			root = optarg;
			break;

                default:
			usage();
                }

	if (optind != argc) {
		if (interactive || argc - optind > 2)
			usage();
		interactive++;
	}

#ifdef DOELF
	if (elf_version(EV_CURRENT) == EV_NONE) {
		fprintf(stderr,
		"systune out of date with respect to current ELF version\n");
		exit(-1);
	}
#endif

	if (root) {
		slash_mtune = concat(root, slash_mtune);
		stune_file = concat(root, stune_file);
		system_file = concat(root, system_file);
	}

	/* Lets check name list to see if this version works on
	 * this machine
	 */
	
	if (NLIST(unixname, nl) < 0) {
		fprintf(stderr, "This version of systune does not work on this machine\n");
		if (!(nlist64(unixname, &nltest))) {
			fprintf(stderr, "systune is -n32 while %s is -64\n", unixname);
		}
		else if (!(nlist(unixname, &nltest_32))) {
			fprintf(stderr, "systune is -64 while %s is -n32\n", unixname);
		}
		else
			fprintf(stderr, "Unable to get symbol table for (%s),  may be due to permission problems.\n", unixname);
		exit(1);
	}

	/* Load tags from system file */
	if((fp = fopen(system_file, "r"))==NULL){
	  perror("open system_file");
	  exit(1);
	}
	while(!feof(fp)){
	  char *str;

	  if (fgets(tempbuf, LSIZE, fp) == NULL)
	    break;
	  if (strncasecmp(tempbuf, tag_sign, strlen(tag_sign)) == 0) {
	    str = strchr(tempbuf, ':');
	    if (str == NULL) { /* Malformed line */
	      perror("malformed tune tag line in system file");
	      exit(-1);
	    }
	    str++;

	    str = strtok(str, " \t\n");
	    while (str != NULL) {
	      if (tags >= MAXTAGS) {
		perror("too many tags in system file");
		exit(-1);
	      }
	      if ((tune_tags[tags++] = strdup(str)) == NULL) {
		perror("out of memory--strdup(3) failed");
		exit(-1);
	      }
	      str = strtok(NULL, " \t\n");
	    }
	  }
	}

	if (both && interactive){
		if (force)
			unixinstall = unixname;
		else {
			if ((sfd = open(unixinstall, O_RDONLY)) < 0) {
				/* copy unixname to /unix.install */
				cp++;
			} else {
				/* /unix.install exist */
#ifdef DOCOFF
				ldptr = ldopen(unixinstall, ldptr);
				ldnshread(ldptr,".data",&shdr);
				ldclose(ldptr);
				lseek(sfd, shdr.s_scnptr + nl[0].n_value 
					- shdr.s_vaddr,0);
#endif
#ifdef DOELF
				if ((offset = find_data_offset(sfd)) == 0L) {
					fprintf(stderr,
			"systune: can't find offset of .data section in %s\n",
						unixinstall);
					exit(1);
				}
				lseek(sfd,
				      offset + (long) nl[0].n_value,
				      SEEK_SET);
#endif
				close(sfd);
			}
			if (cp) {
				/* copy unixname to /unix.install */
				char cmdline[BSIZE];
				sprintf(cmdline, "cp %s %s ", 
						unixname, unixinstall);
				if (system(cmdline) != 0){
					fprintf(stderr, "\tThe work will done on %s\n", unixname);
					unixinstall = unixname;
				}
			}
		}
	}

	/* read in tunable parameters */

	read_tunetable();
	read_tunables();
	if (both)
		rdstune();

	if (optind != argc) {
		char *v = argv[optind++];
		sp = findvariables(v, &vtype);
		if (vtype == V_GROUP)
			dumpgroup((struct tunemodule *)sp);
		else if (optind < argc) {
			if (dumpvariable(v) == 0)
				setvalue(v, ATOL(argv[optind++]));
		} else
			dumpvariable(v);
		exit(0);
	}

	if (interactive) {
		if (both)
			printf("Updates will be made to running system and %s\n", unixinstall);
		else if (running)
			printf("Updates will be made to running system only\n");
		else
			printf("Updates will be made to /unix only\n");

		for (;;) {
			mode = askname(&s, &count);
			switch (mode) {

			case ALLMODE:

				/* dump all the tunables */

				for (gp = group_info;gp != NULL;gp = gp->t_next)
					dumpgroup(gp);
				break;

			case READMODE:

				sp = findvariables(s, &vtype);
				if (vtype == V_GROUP)
					dumpgroup((struct tunemodule *)sp);
				else
					dumpvariable(s);
				break;

			case WRITEMODE:
		
				if (dumpvariable(s) == 0)
					setvalue(s, count);
				break;

			case HELP:
				help();
				break;
			}
		}
	} else {

		/* dump all the tunables */

		for (gp = group_info; gp != NULL; gp = gp->t_next) {
			dumpgroup(gp);
		}
	}
}

/*
 * askname - ask for symbol/number
 */
static int
askname(
char **s,	/* pointer to string name */
long long *count)
{
	char buf[BSIZE];
	static char name[BSIZE];
	char *tn;
	int i;
	int mode = 0;
	char tmpname[BSIZE];

	*s = name;
	for (;;) {
		printf("\nsystune-> ");

		if (fgets(buf, BSIZE, stdin) == NULL)
			exit(0);
		tn = buf;
		while (*tn && isspace(*tn))
			tn++;
		if (isalpha(*tn) || !strncmp(tn, "?", 1)) {
			i = sscanf(buf, "%s%s", name, tmpname);
		} else {
			continue;
		}
		if (i == 0 || i == EOF )
			continue;
		if (i == 2 && !strncmp(tmpname, "=", 1)){
			i = sscanf(buf, "%s %*c%lli", name, count);
		}else
			i = sscanf(buf, "%s%lli", name, count);
		/* now deal with read or write mode */
		if (i == 1){
			
			if (strcasecmp("quit", name)== 0 || 
				strcasecmp("q", name) == 0)
				exit(0);
			if ((!strcasecmp("?", name)) || 
				(!strcasecmp("help", name)))
				mode = HELP;
			else if (!strcasecmp("all", name))
				mode = ALLMODE;
			else
				mode = READMODE;
		} else
			mode = WRITEMODE;
		return(mode);
	}
}

static void
read_tunetable(void)
{
	sgt_cookie_t ck;
	sgt_info_t info;
	int res;

	/* Get size of tunetable */

	SGT_COOKIE_SET_KSYM(&ck, KSYM_TUNETABLE);
	SGT_COOKIE_INIT(&ck);
	res = sysget(SGT_KSYM, (char *)&info, sizeof(info), SGT_INFO, &ck);
	if (res < 0) {
		perror("sysget");
		fprintf(stderr, "SGT_INFO of KSYM_TUNETABLE failed\n");
		exit(1);
	}

	/* Allocate and read tunetable */

	tunetable = (struct tunetable *)mymalloc(info.si_num * 
		sizeof(struct tunetable));
	SGT_COOKIE_INIT(&ck);
	res = sysget(SGT_KSYM, (char *)tunetable, info.si_num * 
		(int)sizeof(struct tunetable), SGT_READ, &ck);
	if (res < 0) {
		perror("sysget");
		fprintf(stderr, "SGT_READ of KSYM_TUNETABLE failed\n");
		exit(1);
	}
}

static int
tunetablefind(char *keyword)
{
        struct tunetable *kt;

	for (kt = tunetable; *(kt->t_name) != NULL; kt++) {
                if (!strcmp(keyword, kt->t_name))
                        return(1);
        }
        return(0);
}

static void
read_tunables(void)
{
	DIR * dir;
	struct dirent *p;
	struct tunemodule *gp, **oldgp;

	dir = opendir(slash_mtune);
	if (dir) {
		while ( (p = readdir(dir)) != 0) {
			
			/* Check for .. here to avoid unnecessary operations */
			
			if ( strcmp(p->d_name,"..") && strcmp(p->d_name,".")) {

				gen_tune(p->d_name);
			}
		}
	}

	/* check tunables */


	oldgp = &group_info;
	for (gp = group_info; gp != NULL; gp = gp->t_next) {
		if (tunetablefind(gp->t_gname) == 0) {
			/* not in kernel tunetable */
			*oldgp = gp->t_next;
		} else
			oldgp = &gp->t_next;
        }
	
}


void
gen_tune(char *fname)
{
	FILE *mtune;
	char *p;
	int type = 0;
	struct tunemodule *gp;
	struct tune *tunep;
	char *tmp;

	strcpy( mtune_file, slash_mtune );
	strcat( mtune_file, "/" );
	strcat( mtune_file, fname );

	p=strrchr(mtune_file,'/');
	if ( (p=strchr(p,'.')) != NULL )
		*p = '\0';

	if ((mtune = fopen(mtune_file,"r")) == NULL)
		return;

	do {
		if (getline(mtune) == 0)
			return;

		if ((tmp = parse_mtune(&type)) == NULL) 
			exit(1);
	} while (type == V_NOMATCH);

	if (type == V_GROUP) {

		/* link to group_info */

		gp = (struct tunemodule *)tmp;
		gp->t_next = group_info;
		group_info = gp;
	} else {
		
		/* use module name */

		gp = (struct tunemodule *)mymalloc(sizeof(struct tunemodule));
		gp->t_gname = mymalloc(strlen( fname) + 1);
		strcpy(gp->t_gname, fname);
		gp->t_flags = N_STATIC;
		gp->t_count = 0;
		gp->t_tunep = NULL;
		gp->t_next = group_info;
		group_info = gp;

		/* save the first variable */
		tunep = (struct tune *)tmp;
		gp = group_info;
		tunep->next = gp->t_tunep;
		gp->t_tunep = tunep;
		gp->t_count++;
	}

	while (getline(mtune) != 0) {
		type = 0;
		if ((tmp = parse_mtune(&type)) == NULL) 
			exit(1);
		if (type == V_GROUP) {

			/* create new group */

			gp = (struct tunemodule *)tmp;
			gp->t_next = group_info;
			group_info = gp;

		} else if (type == V_VARIABLE){

			/* link to the same group */

			tunep = (struct tune *)tmp;
			gp = group_info;
			tunep->next = gp->t_tunep;
			gp->t_tunep = tunep;
			gp->t_count++;
		}
	}
}


/*
 * Read a line. Skip lines beginning with '*' or '#', and blank lines.
 * Return 0 if EOF. Return 1 otherwise.
 */
static int
getline(FILE *fp)
{

	for (;;) {
		if (fgets(linebuf, LSIZE, fp) == NULL) {
			linebuf[0] = '\0';
			return(0);
		}
		if (*linebuf != '*' && *linebuf != '#' && *linebuf != '\n')
			return(1);
        }
}

static char
*parse_mtune(int *type)
{
	
	int argc;
	char *argv[100];
	struct tune *tp;
	struct tunemodule *gp;
	int tag;
	char *tagmatched;

	static char *nomatch = "\0";

	argc = parse(argv,sizeof(argv)/sizeof(argv[0]),linebuf);
	if (argc > sizeof(argv)/sizeof(argv[0])){
		fprintf(stderr, "line too long in %s", mtune_file);
		return(NULL);
	}

	if (argc < 2){
		fprintf(stderr, "Line too short, no name in %s\n", mtune_file);
		return(NULL);
	}
	if (*argv[1] != ':') {

	  if(*argv[1] == ','){
		/* when there is a tag following the name */
		if(argc < 4){
			fprintf(stderr,
				"Line too short, no default value in %s\n",
				mtune_file);
			return(NULL);
		}

		/* check whether the tag matches */
		tagmatched = NULL;
		for (tag = 0; tag < tags; tag++) {
			if (strcasecmp(argv[2], tune_tags[tag]) == 0) {
				tagmatched = tune_tags[tag];
				break;
			}
		}
		if (tagmatched == NULL) {
			*type = V_NOMATCH;
			return nomatch;
		}

		tp = (struct tune *)mymalloc(sizeof(struct tune));
		tp->tname = mymalloc(strlen(argv[0]) + 1);
		strcpy(tp->tname, argv[0]);
		tp->def = ATOL(argv[3]);
		tp->tmin = 0LL;
		tp->tmax = 0LL;
		tp->type = TUNE_TYPE_32;
	    
		if (argc > 4)
			tp->tmin = ATOL(argv[4]);
		if (argc > 5)
			tp->tmax = ATOL(argv[5]);
		if ((argc > 6) && ((strcmp(argv[6], "LL") == 0) ||
			(strcmp(argv[6], "ll") == 0)))
				tp->type = TUNE_TYPE_64;

		tp->svalue = tp->def;
		tp->conf = 0;
		*type = V_VARIABLE;
		return((char *)tp);

	  }
	  else{
		if (argc < 2) {
			fprintf(stderr,
				"Line too short, no default value in %s\n",
				mtune_file);
			return(NULL);
		}
		/* tunable parameter */

		tp = (struct tune *)mymalloc(sizeof(struct tune));
		tp->tname = mymalloc( strlen(argv[0]) + 1);
		strcpy(tp->tname, argv[0]);
		tp->def = ATOL(argv[1]);
		tp->tmin = 0LL;
		tp->tmax = 0LL;
		tp->type = TUNE_TYPE_32;

		if (argc > 2)
			tp->tmin = ATOL(argv[2]);
		if (argc > 3)
			tp->tmax = ATOL(argv[3]);
		if ((argc > 4) && ((strcmp(argv[4], "LL") == 0) ||
			(strcmp(argv[4], "ll") == 0)))
				tp->type = TUNE_TYPE_64;

		tp->svalue = tp->def;
		tp->conf = 0;
		*type = V_VARIABLE;
		return((char *)tp);
	      }
	} else {
		
		/* group name */

		gp = (struct tunemodule *)mymalloc(sizeof(struct tunemodule));
		gp->t_gname = mymalloc( strlen(argv[0]) + 1);
		strcpy(gp->t_gname, argv[0]);
		if ((argc > 2) && (!strcmp("run", argv[2])))
			gp->t_flags = N_RUN;
		else 
			gp->t_flags = N_STATIC;
		gp->t_count = 0;
		gp->t_tunep = NULL;
		*type = V_GROUP;
		return((char *)gp);
	}
}

static char
*parse_stune(struct stune *sp)
{
	
	int argc;
	char *argv[100];

	argc = parse(argv,sizeof(argv)/sizeof(argv[0]),linebuf);
	if (argc > sizeof(argv)/sizeof(argv[0]))
		return("line too long");

	if (argc < 2)
		return("syntax error");
	if (*argv[1] != '=') 
		return("syntax error");
	else {
	
		if (argc < 3)
			return("syntax error");

		strcpy(sp->s_name, argv[0]);
		sp->s_value = ATOL(argv[2]);
		sp->s_type = TUNE_TYPE_32;
		
		if ((argc == 4) && ((strcmp(argv[3], "LL") == 0) ||
			(strcmp(argv[3], "ll") == 0)))
				sp->s_type = TUNE_TYPE_64;
	}
	return(NULL);
}


/* This routine is used to search the Parameter table
 * for the keyword that was specified in the configuration.  If the
 * keyword cannot be found in this table, a NULL is returned.
 * If the keyword is found, a pointer to that entry is returned.
 */
static int
tfind(char *keyword, struct tune **tune, struct tunemodule **group)
{
        struct tune *tp;
        struct tunemodule *gp;

	*group = NULL;
	for (gp = group_info; gp != NULL; gp = gp->t_next) {
		for (tp = gp->t_tunep; tp != NULL ; tp = tp->next) {
			if (!strcmp(keyword, tp->tname)){
				*tune = tp;
				*group = gp;
				return(1);
			}
		}
        }
	*tune = NULL;
	*group = NULL;
        return(0);
}

/*
 * malloc() with error checking
 */
static char *
mymalloc(size_t size)
{
        char *p = malloc(size);
        if (p == (char *) NULL) {
                fprintf(stderr, "no memory--malloc(3) failed\n");
                exit(2);
        }
        return(p);
}


/*
 * Parse(argv, sizeof(argv), line)
 *
 * Parse a line from the /etc/system file; argc is returned, and argv is
 * initialized with pointers to the elements of the line.  The contents
 * of the line are destroyed.
 */
int
parse(char **argv, unsigned sizeof_argv, char *line)
{
	char **argvp = argv;
	char **maxvp = argv + sizeof_argv;
	int c;

	while (c = *line) {
		switch (c) {
		/*
		 * white space
		 */
		case ' ':
		case '\t':
		case '\r':
			*line++ = '\0';
			line += strspn(line," \t\r");
			continue;
		/*
		 * special characters
		 */
		case ':':
			*line = '\0';
			*argvp++ = ":";
			++line;
			break;
		case ',':
			*line = '\0';
			*argvp++ = ",";
			++line;
			break;
		case '(':
			*line = '\0';
			*argvp++ = "(";
			++line;
			break;
		case ')':
			*line = '\0';
			*argvp++ = ")";
			++line;
			break;
		case '?':
			*line = '\0';
			*argvp++ = "?";
			++line;
			break;
		case '=':
			*line = '\0';
			*argvp++ = "=";
			++line;
			break;
		case '*':
			*line = '\0';
			*argvp++ = "*";
			++line;
			break;
		/*
		 * end of line
		 */
		case '\n':
			*line = '\0';
			*argvp = NULL;
			return (int) (argvp - argv);
		/*
		 * words and numbers
		 */
		default:
			*argvp++ = line;
			line += strcspn(line,":,()?= \t\r\n");
			break;
		}

		/*
		 * don't overflow the argv array
		 */
		if (argvp >= maxvp)
			return (sizeof_argv + 1);
	}

	*argvp = NULL;
	return (int) (argvp - argv);
}

static void
dumpgroup(struct tunemodule *gp)
{
	struct tune *tp;

	if (gp->t_tunep == NULL)
		return;
	if (gp->t_tunep->tname == NULL)
		return;

	printf("\n group: %s (%s changeable)\n",
		gp->t_gname,
		gp->t_flags & N_RUN ? "dynamically" : "statically");
	for (tp = gp->t_tunep; tp != NULL ; tp = tp->next) {
		dumpvariable(tp->tname);
	}
}

static int
dumpvariable(char *s)
{
	char buf[LSIZE];
	struct tunemodule *group;
	struct tune *tune;
	long long value;

	nl[1].n_name = NULL;
	nl[0].n_name = s;
	if (NLIST(unixname, nl) < 0) {
		fprintf(stderr, "systune: nlist failed for %s\n",
			unixname);
		exit(1);
	}
	if (nl[0].n_type == 0) {
		fprintf(stderr, "symbol: %s not found\n", s);
		return(-1);
	}
	if (tfind(s, &tune, &group) == 0) {
		fprintf(stderr, "symbol: %s not a tuneable parameter\n", s);
		return(-1);
	}

	if (syssgi(SGI_TUNE_GET, s, buf) < 0) {
		fprintf(stderr, "SGI_TUNE_GET of symbol %s failed\n", s);
		return(-1);
	}

	printf(" \t%s = ", s);
	if (tune->type == TUNE_TYPE_64)
		value = *((long long *)(&buf[0]));
	else
		value = *((int *)(&buf[0]));

	if (SIGNED(s))
		printf("%lld (0x%llx)", value, value);
	else
		printf("%llu (0x%llx)", value, value);

	if (tune->type == TUNE_TYPE_64) {
		printf(" ll");
	}
	if ((group->t_flags == N_STATIC) && (tune->conf == 1) 
				&& (tune->svalue != value)) {
		if (SIGNED(s)) {
			printf(" [dev/kmem]\t%lld (0x%llx) [%s]\n",
			       tune->svalue, tune->svalue, unixinstall);
		} else {
			printf(" [dev/kmem]\t%llu (0x%llx) [%s]\n",
			       tune->svalue, tune->svalue, unixinstall);
		}
	} else {
		printf("\n");
	}
		
	return(0);
}

static struct tunemodule *
gfind(char *keyword)
{
        struct tunemodule *tune;

	for (tune = group_info; tune != NULL; tune = tune->t_next) {
                if (!strcmp(keyword, tune->t_gname))
                        return(tune);
        }
        return(NULL);
}

static char *
findvariables(char *s, int *vtype)
{
        struct tunemodule *gp;

	gp =gfind(s);
	if (gp != NULL) {
		*vtype = V_GROUP;
		return((char *)gp);
	} 

	*vtype = V_VARIABLE;
	return(NULL);

}
		
static void
setvalue(char *s, long long value)
{
	char buf[20];
        static char answer[20];
        char *tn;
	struct tune *tune;
	struct tunemodule *group;
	
	printf("\tDo you really want to change %s to %lld (0x%llx)? (y/n)  ", 
						s, value, value);

	if (fgets(buf, BSIZE, stdin) == NULL)
		exit(0);
	tn = buf;
	while (*tn && isspace(*tn))
		tn++;
	sscanf(buf, "%s", answer);
	if (strncasecmp("yes", answer, 1))
		return;

	/* verify value */

	if (tfind(s, &tune, &group) == 0) {
		fprintf(stderr, "\tUnknown tunable parameter '%s'\n",
			s);
		return;
	}

	/* check whether parameter is within min and max */
	if (tune->tmax != 0) {
		if (value != tune->def && ( value < tune->tmin ||
		    value > tune->tmax)) {
			fprintf(stderr, "\tThe value of parameter '%s', %lld, must be within (%lld, %lld)\n\tor the default (%lld)\n", 
			s, value, 
			tune->tmin, tune->tmax, tune->def);
			return;
		}
	} else if (tune->tmin != 0) {
		if (value != tune->def && value < tune->tmin) {
			fprintf(stderr, "\tThe value of parameter '%s', %lld, must be greater than %lld\n\tor the default (%lld)\n",
				s, value,tune->tmin, tune->def);
			return;
		}
	}


	/* static or runtime */

	if (group->t_flags & N_RUN) {
		cap_t ocap;
		cap_value_t cap_sysinfo_mgt = CAP_SYSINFO_MGT;

		if (tune->type == TUNE_TYPE_64) {
			ocap = cap_acquire(1, &cap_sysinfo_mgt);
			if (syssgi(SGI_TUNE_SET, group->t_gname, s, &value)) {
				cap_surrender(ocap);
				fprintf(stderr, "\tChange %s to %lld failed!!\n", s, value);
				return;
			}
			cap_surrender(ocap);
		} else {
			int int_value = (int) value;
			ocap = cap_acquire(1, &cap_sysinfo_mgt);
			if (syssgi(SGI_TUNE_SET, group->t_gname, s,
					 &int_value)) {
				cap_surrender(ocap);
				fprintf(stderr, "\tChange %s to %d failed!!\n", s, int_value);
				return;
			}
			cap_surrender(ocap);
		}
		if (both) {
			/* ed stune */
			ed_stune(tune, value);
			/* store value in Parameter table */
			tune->svalue = value;

			/* indicate tunable parameter specified */
			tune->conf = 1;
		}
	} else {
		/* static */
		if (both) {
			/* ed stune */
			ed_stune(tune, value);
			if (strcmp(unixinstall, "/unix")) {
				printf("\nIn order for the change in parameter %s ", s);
				printf("to become effective,");
				printf("\nreboot the system \n");
			}
			/* store value in Parameter table */
			tune->svalue = value;

			/* indicate tunable parameter specified */
			tune->conf = 1;

		} else {
			fprintf(stderr, "\tCannot change %s on the running system.\n", s);
		}
	}
		
}

/*
 * create a copy of the concatenation of 2 strings.
 *      The original string should be freed by the caller.
 *      Either string may be null.
 */
static char *
concat(char *prefix, char* suffix)
{
        char *str;
        size_t plen, slen;

        plen = (prefix!=0 ? strlen(prefix) : 0);
        slen = (suffix!=0 ? strlen(suffix) : 0);
        str = mymalloc(plen + 1 + slen);
        if (plen != 0)
                (void)strcpy(str, prefix);
        else
                str[0] = '\0';
        if (slen != 0)
                (void)strcat(str, suffix);

        return(str);
}

#ifdef DOCOFF
static void
ed_stune(struct tune *table, long long value)
{
	SCNHDR  shdr;
	LDFILE *ldptr = NULL;
	char cmdline[BSIZE];
	int sfd;
	int int_val;

	/* change stune file */

	if (table->type == TUNE_TYPE_64) {
		sprintf(cmdline, "edstune %s %s %lli ll",
			stune_file, table->tname, value);
	} else {
		sprintf(cmdline, "edstune %s %s %lli",
			stune_file, table->tname, value);
	}
	if (system(cmdline)) {
		fprintf(stderr, "\tCannot change %s on %s\n", table->tname, stune_file);
		return;
	}

	/* change /unix.install */

	/* get file offset of .data */

	if ((ldptr = ldopen(unixinstall, ldptr)) == NULL){
		fprintf(stderr, "\tCannot open %s file\n", stune_file);
		return;
	}
	ldnshread(ldptr,".data",&shdr);
	if(ldclose(ldptr) != SUCCESS){
		fprintf(stderr, "\tCannot ldclose %s file\n", stune_file);
		return;
	}

	if ((sfd = open(unixinstall, O_WRONLY)) < 0) {
		perror(unixinstall);
		return;
	}
	if (lseek(sfd,shdr.s_scnptr + nl[0].n_value - shdr.s_vaddr,0) == -1){
		fprintf(stderr, "\tCannot lseek %s file\n", stune_file);
		return;
	}

	if (table->type == TUNE_TYPE_64) {
		if (write(sfd, &value, sizeof(long long)) != sizeof(long long)){
			fprintf(stderr, "\tWrite to %s failed!!\n", stune_file);
			return;
		}
	} else {
		int_val = (int)value;
		if (write(sfd, &int_val, sizeof(int)) != sizeof(int)){
			fprintf(stderr, "\tWrite to %s failed!!\n", stune_file);
			return;
		}
	}

	close(sfd);
	
}
#endif	/* DOCOFF */

/* 
 * rdstune - System tunable parameter file 
 */

static void
rdstune(void)
{
	struct stune stune;
	struct tune *tune;
	FILE *stunep;
	char *msg;
	struct tunemodule *group;

	if ((stunep = fopen(stune_file, "r")) == NULL)
		return;
	
	while (getline(stunep) != 0) {
		if ((msg = parse_stune(&stune)) != NULL) {
			fprintf(stderr, "%s: %s\n", stune_file, msg);
			continue;
		}

		/* find tunable in Parameter table */
		if (tfind(stune.s_name, &tune, &group) == 0) {
			continue;
		}
	
                /* check whether parameter is within min and max */
		if (tune->tmax != 0) {
			if (stune.s_value != tune->def && (stune.s_value < tune->tmin ||
			    stune.s_value > tune->tmax)) {
				continue;
			}
		} else if (tune->tmin != 0) {
			if (stune.s_value != tune->def && stune.s_value < tune->tmin) {
				continue;
			}
		}

                /* store value in Parameter table */
		tune->svalue = stune.s_value;

                /* indicate tunable parameter specified */
                tune->conf = 1;
        }

}

static void
help(void)
{

	struct tunemodule *gp;
	int i = 0;

	printf("help or ?: help command\n");
	printf("quit: quit from the command\n");
	printf("all:  display all tunable parameters\n");
	printf("group names are:\n");
	for (gp = group_info;gp != NULL;gp = gp->t_next){
		printf("\t%s", gp->t_gname);
		if (++i % 4 == 0)
			printf("\n");
	}
}

#ifdef DOELF

static void
ed_stune(struct tune *table, long long value)
{
	char cmdline[BSIZE];
	int sfd;
	long offset;

	/* change stune file */


	if (table->type == TUNE_TYPE_64) {
		sprintf(cmdline, "edstune %s %s %lli ll",
			stune_file, table->tname, value);
	} else {
		sprintf(cmdline, "edstune %s %s %lli",
			stune_file, table->tname, value);
	}
	if (system(cmdline)) {
		fprintf(stderr, "\tCannot change %s on %s\n", table->tname, stune_file);
		return;
	}

	/* change /unix.install */

	if ((sfd = open(unixinstall, O_RDWR)) < 0) {
		perror(unixinstall);
		return;
	}

	/* get file offset of .data */

	if ((offset = find_data_offset(sfd)) == 0L) {
		fprintf(stderr, "systune: can't find offset of .data section in %s\n",
				unixinstall);
		close(sfd);
		return;
	}

	if (lseek(sfd, offset + (long) nl[0].n_value , SEEK_SET) == -1){
		fprintf(stderr, "\tCannot lseek %s file\n", unixinstall);
		close(sfd);
		return;
	}

	if (table->type == TUNE_TYPE_64) {
		if (write(sfd, &value, sizeof(long long)) != sizeof(long long)){
			fprintf(stderr, "\tWrite to %s failed!!\n", unixinstall);
			close(sfd);
			return;
		}
	} else {
		int intval = (int)value;
		if (write(sfd, &intval, sizeof(int)) != sizeof(int)){
			fprintf(stderr, "\tWrite to %s failed!!\n", unixinstall);
			close(sfd);
			return;
		}
	}

	close(sfd);
}

/*
 * Returns the file offset of the .data section less the beginning address.
 */
static long
find_data_offset(int fd)
{
	Elf *elf;
	Elf_Cmd cmd;
	Elf_Scn *scn = NULL;

#if (_MIPS_SZLONG == 64)
	Elf64_Ehdr *ehdr;
	Elf64_Shdr *shdr;
#define GET_EHDR elf64_getehdr
#define GET_SHDR elf64_getshdr
#else
	Elf32_Ehdr *ehdr;
	Elf32_Shdr *shdr;
#define GET_EHDR elf32_getehdr
#define GET_SHDR elf32_getshdr
#endif

	if (elf_version(EV_CURRENT) == EV_NONE) 
		return(0);

	cmd = ELF_C_READ;
	if ((elf = elf_begin(fd, cmd, (Elf *)0)) == NULL) {
		return(0);
	}

	if ((ehdr = GET_EHDR(elf)) == NULL)
		return(0);
	do {
		if ((scn = elf_nextscn(elf, scn)) == NULL)
			break;
		if ((shdr = GET_SHDR(scn)) == NULL)
			return(0);
	} while (strcmp(".data", elf_strptr(elf, ehdr->e_shstrndx,
						  shdr->sh_name)));
	elf_end(elf);
	return (long) (shdr->sh_offset - shdr->sh_addr);
}
#endif	/* DOELF */
