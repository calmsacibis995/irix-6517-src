/*	  All Rights Reserved  	*/

/*	THIS IS UNPUBLISHED PROPRIETARY SOURCE CODE OF AT&T	*/
/*	The copyright notice above does not evidence any   	*/
/*	actual or intended publication of such source code.	*/

#ident "$Revision: 2.77 $"

#include <sys/types.h>
#include <a.out.h>
#define edt kernel_edt
#define vme_intrs kernel_vme_intrs
#define iospace kernel_iospace
#include <sys/edt.h>
#include <sys/vmereg.h>
#undef edt
#undef vme_intrs
#undef iospace
#include <sys/sysmacros.h>
#include <errno.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/syssgi.h>
#include <sys/pio.h>
#include <limits.h>
#include <stdio.h>
#include <paths.h>
#include "lboot.h"
#include "boothdr.h"

extern char any_error;			/* any error seen in master.d */
extern int noprobe;

/* _PATH_LS is only defined on 6.5 and beyond */
#ifdef _PATH_LS
#define __PATH_LS _PATH_LS
#else
#define __PATH_LS "/sbin/ls"
#endif

#define probe_base 3
struct t_edt {
	kernptr_t t_base, t_base2, t_base3;
	char *v_ifcn;
	uint_t e_bustype;
	uint_t e_adap;
	uint_t e_ctlr;
	unsigned char v_vec;
	unsigned char v_ipl;
	unsigned char v_unit;
	struct lboot_iospace e_space[4];
	int cpusyscall;
	int cpuintr;
	char *probe_path;
};


/*
 * Static function declarations for this file
 */
static char	*do_kernel(int, char **, dev_t *);

static char	*do_device_admin(int,char **,dev_t *);
static char	*do_driver_admin(int,char **,dev_t *);
static char	*do_devicefile(int, char **, char **);
static char	*do_exclude(int, char **, dev_t *);
static char	*do_include(int, char **, dev_t *);
static char	*do_use(int, char **, dev_t *);
static char	*do_rootdevice(int, char **, dev_t *);
static char	*do_dumpdevice(int, char **, dev_t *);
static char	*do_swapdevice(int, char **, dev_t *);
static char	*do_vector(int, char **, dev_t *);
static char	*do_ccopts(int, char **, dev_t *);
static char	*do_cc(int, char **, dev_t *);
static char	*do_ldopts(int, char **, dev_t *);
static char	*do_ld(int, char **, dev_t *);
static char	*do_ipl(int, char **, dev_t *);
static char	*do_nointr(int, char **, dev_t *);
static char	*do_noprobe(int, char **, dev_t *);
static char	*do_link(int, char **, dev_t *);
static char	*do_version(int, char **, dev_t *);
static char	*interpret(char *);
static boolean  numeric(int *, char *);
static int	parse_base(char *, struct t_edt *, int);
static void	gen_entry(char *, struct t_edt *);
static int	parse_system_file(char *);
void		epc_module(char *, struct t_edt *);

/*
 * IBUS data structure 
 */

#define MAX_IBUS_ADAPS		60
int ibus_numadaps;
ibus_t ibus_adapters[MAX_IBUS_ADAPS];

/*
 * Process system files.  If etcsystem is a directory, then read and parse
 * all files in that directory.
 */
int					/* number of errors found */
parse_system(void)
{
	struct stat stb;
	struct dirent **systems;
	char system_path[PATH_MAX];
	int i, errcnt, nentries; 

	if (stat(etcsystem, &stb) == -1) {
		error(ER7, etcsystem);
		return 1;
	}

	if (S_ISDIR(stb.st_mode)) {
		/* sort directory contents alphabetically */
		if ((nentries = scandir(etcsystem, &systems, sys_select, alphasort)) < 0) {
			error(ER7, etcsystem);
			return 1;
		}

		errcnt = 0;

		for (i = 0; i < nentries; i++) {
			strcpy(system_path, etcsystem);
			strcat(system_path, "/");
			strcat(system_path, systems[i]->d_name);
			errcnt += parse_system_file(system_path);
			free(systems[i]);
		}
		free(systems);
	} else
		errcnt = parse_system_file(etcsystem);

	return errcnt;
}

/*
 * Select files that look like system files
 */
int
sys_select(struct dirent *d)
{
	struct stat stb;
	char system_path[PATH_MAX];

	/*
	 * Make sure pathname doesn't get too long
	 * ("+2" accounts for extra "/" and NULL byte)
	 */

	if (strlen(etcsystem) + strlen(d->d_name) + 2 >
	    PATH_MAX) {
		fprintf(stderr, "%s/%s: pathname too long\n",
			etcsystem, d->d_name);
		return 0;
	}

	strcpy(system_path, etcsystem);
	strcat(system_path, "/");
	strcat(system_path, d->d_name);

	/*
	 * Skip subdirectories and other non-regular files.  
	 * This is so we don't try to interpret ".", "..", 
	 * "RCS", "SCCS", etc., as system files.
	 */

	if (stat(system_path, &stb) == -1) {
		error(ER7, system_path);
		return 0;
	}

	if (!S_ISREG(stb.st_mode))
		return 0;

	/*
	 * Legal system files end in ".sm", skip all others.
	 * This makes sure we don't try to process SCCS "s."
	 * files, RCS ",v" files, etc.
	 */

	if (!suffix(d->d_name, "sm"))
		return 0;
	return 1;
}

/*
 * Read an individual system file, parse and extract all the necessary 
 * information.
 */

static int
parse_system_file(char *systemfile)
{
	register FILE *stream;
	char line[4096];
	register char *msg;
	register lineno = 1;
	int error_cnt = 0;

	if ((stream=fopen(systemfile,"r")) == 0) {
		error(ER7, systemfile);
		return 1;
	}

	while (fgets(line,sizeof(line),stream) != NULL) {
		if (strlen(line) >= sizeof(line)-1) {
			error(ER53, systemfile, lineno, "too long");
			error_cnt++;
			continue;
		}

		if (line[0] != '*'
		    && !blankline(line)
		    && (msg=interpret(line)) != NULL) {
			error(ER53, systemfile, lineno, msg);
			error_cnt++;
		}

		++lineno;
	}

	(void)fclose(stream);

	return error_cnt|any_error;
}

/*
 * Interpret(line)
 *
 * Interpret the line from the /etc/system file
 */


struct	syntax {
	char	*keyword;
	char	*(*process)(int, char **, dev_t *);
	dev_t	*argument;
};

static struct syntax syntax[] ={
			{ "KERNEL", do_kernel, 0, },
			{ "DEVICE_ADMIN",do_device_admin,0},	
			{ "DRIVER_ADMIN",do_driver_admin,0},	
			{ "EXCLUDE", do_exclude, 0, },
			{ "INCLUDE", do_include, 0, },
			{ "USE", do_use, 0 },
			{ "DUMPDEV", do_dumpdevice, 0, },
			{ "ROOTDEV", do_rootdevice, 0 },
			{ "SWAPDEV", do_swapdevice, 0 },
			{ "VECTOR", do_vector, 0 },
			{ "CCOPTS", do_ccopts, 0 },
			{ "CC",	do_cc, 0 },
			{ "LDOPTS", do_ldopts, 0 },
			{ "LD",	do_ld, 0 },
			{ "IPL", do_ipl, 0},
			{ "NOINTR", do_nointr, 0},
			{ "NOPROBE", do_noprobe, 0},
			{ "VERSION", do_version, 0},
			{ "LINKMODULES", do_link, 0},
			{ "TUNE-TAG", do_tunetag, 0},
			0
};

static char *
interpret(char *line)
{
	register int argc;
	char *argv[256];
	register struct syntax *p;

	argc = parse(argv,sizeof(argv)/sizeof(argv[0]),line);
	if (argc > sizeof(argv)/sizeof(argv[0]))
		return("line too long");

	if (argc == 0)
		return(NULL);

	if (argc <= 2 || *argv[1] != ':')
		return("syntax error");


	for (p=syntax; p->keyword; ++p) {
		if (0 == strcmp(*argv,p->keyword)) 
			return((*(p->process))(argc-2, &argv[2], (dev_t *) p->argument));
	}

	return("syntax error");
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
	register char **argvp = argv;
	register char **maxvp = argv + sizeof_argv;
	register c;

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
		 * quotes unparsed 
		 */
		case '"':
		        *line ='\0';
			*argvp++ = ++line;
			while(*line != '"') line++;
			*line = '\0';
			line++;
			break;

		/*
		 * end of line
		 */
		case '\n':
			*line = '\0';
			*argvp = NULL;
			return((int)(argvp - argv));

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
			return(sizeof_argv + 1);
	}

	*argvp = NULL;
	return((int)(argvp - argv));
}

/*
 * Blankline(line)
 *
 * Return TRUE if the line is entirely blank or null; return FALSE otherwise.
 */
boolean
blankline(char *line)
{
	return(strspn(line," \t\r\n") == strlen(line));
}


/*
 * Numeric(assign, string)
 *
 * If the string is a valid numeric string, then set *assign to its numeric
 * value and return TRUE; otherwise return FALSE.
 */
static boolean
numeric(int *assign, char *string)
{
	register int value;
	char *next;

	value = (int)strtoul(string, &next, 0);

	if (*next)
		/*
		 * bad number
		 */
		return(FALSE);

	*assign = value;

	return(TRUE);
}

/*
 * Do_??????(argc, argv, optional)
 *
 * Handle the processing for each type of line in /etc/system
 */


/*
 * KERNEL: master
 */
/* ARGSUSED */
static char *
do_kernel(int argc, char **argv, dev_t *dummyvar)
{
	if (argc > 1)
		return("syntax error");

	if (kernel_master)
		return("duplicate KERNEL specification");

	kernel_master=mymalloc(	strlen(*argv) + 1);
	(void) strcpy(kernel_master,*argv);

	if ((kernel.opthdr = mkboot(kernel_master)) == 0)
		return("not a master file");

	if (!(kernel.opthdr->flag & KOBJECT))
		return("not a kernel master file");

	return(NULL);
}


/*
 * EXCLUDE: driver ...
 */
/* ARGSUSED */
static char *
do_exclude(int argc, char **argv, dev_t *dummyvar)
{
	while (argc-- > 0) {
		if (*argv[0] != ',')
			exclude(*argv);

		++argv;
	}

	return(NULL);
}


/*
 * INCLUDE: driver(optional-number) ...
 */
/* ARGSUSED */
static char *
do_include(int argc, char **argv, dev_t *dummyvar)
{
	register char *p;
	int n;
	register struct driver *dp;

	while (argc > 0) {

		if (*argv[0] == ',') {
			--argc;
			++argv;
			continue;
		}

		p = *argv;
		n = 1;

		if (argc >= 4 && *argv[1] == '(') {
			if (*argv[3] != ')')
				return("syntax error");

			if (! numeric(&n, argv[2]))
				return("count must be numeric");

			argc -= 3;
			argv += 3;
		}
		else
			if (*p == '(' || *p == ')')
				return("syntax error");

		dp = searchdriver(p);
		if (!dp || !dp->dname) {
			if (dp) {
				fprintf(stderr,"Warning: no object file %s%s or %s%s for INCLUDED subsystem %s\n", 
					dp->mname, ".o", dp->mname, ".a", dp->mname);
				error (ER2, dp->mname);
			}
		} else
			include(p, n);

		--argc;
		++argv;
	}

	return(NULL);
}


/*
 * USE: driver(optional-number) ...
 *	If we can find the driver, include it.  Otherwise, exclude it.
 */
/* ARGSUSED */
static char *
do_use(int argc, char **argv, dev_t *dummyvar)
{
	register char *p;
	int n;
	register struct driver *dp;
	int incr = 0;

	while (argc > 0) {

		if (*argv[0] == ',') {
			--argc;
			++argv;
			continue;
		}

		p = *argv;
		n = 1;

		if (argc >= 4 && *argv[1] == '(') {
			if (*argv[3] != ')')
				return("syntax error");

			if (! numeric(&n, argv[2]))
				return("count must be numeric");

			argc -= 3;
			argv += 3;
		}
		else
			if (*p == '(' || *p == ')')
				return("syntax error");

		--argc;
		++argv;

		if (argc > 0 && EQUAL("exprobe", argv[0])) {
			if (argc == 1 || !EQUAL("=", argv[1]))
				return ("USE: exprobe: syntax error");
			if (incr = exprobe_parse(argc, argv)) {
				argc -= incr;
				argv += incr;
			} else
				return ("USE: exprobe: syntax error");
		}
		dp = searchdriver(p);
		if (!dp || !dp->dname) {
			exclude(p);
		} else if (incr) {
			if (exprobe())
				include(p, n);
		} else {
			include(p, n);
		}
		if (incr) {
			free_Vtuple_list();
			incr = 0;
		}
	}

	return(NULL);
}



/*
 * NOPROBE: 1|0
 */
/* ARGSUSED */
static char *
do_noprobe(int argc, char **argv, dev_t *dummyvar)
{
	char *p;

	if (argc != 1)
		return("NOPROBE syntax error");
	noprobe = (int)strtoul(*argv, &p, 0);
	if (*p != '\0') 
		return("NOPROBE syntax error");
	return(0);
}

/*
 * LINKMODULES: 1|0
 */
/* ARGSUSED */
static char *
do_link(int argc, char **argv, dev_t *dummyvar)
{
	char *p;
	int dolink;

	if (argc != 1)
		return("LINKMODULES syntax error");
	dolink = (int)strtoul(*argv, &p, 0);
	if (*p != '\0') 
		return("LINKMODULES syntax error");
	Dolink = dolink;
	return(0);
}

/*
 * IPL: level, cpu#
 * build the array cpuipl[]
 */
/* ARGSUSED */
static char *
do_ipl(int argc, char **argv, dev_t *dummyvar)
{
	register int cpu, ipl;
	char *p;

	if (argc % 2 || argc == 0) 
		return("IPL syntax error");
	while (argc) {
		ipl = (int)strtoul(*argv, &p, 0);
		if (*p != '\0' || ipl >= MAXIPL)
			return("IPL syntax error");
		cpu = (int)strtoul(*(argv+1), &p, 0);
		if (*p != '\0')
			return("IPL syntax error");
		argc -= 2;
		argv += 2;
		if (cpuipl[ipl] != (char)-1 && cpuipl[ipl] != cpu) 
			error(ER104, ipl);
		else
			cpuipl[ipl] = cpu;
	}
	return(0);
}

#define NOINTR_MAX 128
cpuid_t nointr_list[NOINTR_MAX];
int nointr_count = 0;

/*
 * NOINTR: cpu#
 * build the nointr list
 */
/* ARGSUSED */
static char *
do_nointr(int argc, char **argv, dev_t *dummyvar)
{
	register cpuid_t cpu;
	char *p;

	if (argc == 0) 
		return("NOINTR syntax error");

	/* TBD: Might be nice if we accepted a range of cpus... */
	while (argc) {
		cpu = (cpuid_t)strtoul(*argv, &p, 0);
		if ((*p != '\0') || (cpu < 1) )
			error(ER109, cpu);
		argc -= 1;
		argv += 1;
		if (nointr_count >= NOINTR_MAX)
			return("NOINTR too many cpus");
		nointr_list[nointr_count++] = cpu;
	}
	return(0);
}


/*
 * TUNE-TAG:
 */
/* ARGSUSED */
char *
do_tunetag(int argc, char **argv, dev_t *dummyvar)
{
	if (argc < 1) 
		return("TUNE-TAG: tag [tag ...]");

	while (argc-- && (tune_num < MAXTAGS))
		tune_tag[tune_num++] = strdup(*argv++);
	if (argc > 0)
		return("Too many tags");
	return NULL;
}

/*
 * VERSION:
 */
/* ARGSUSED */
static char *
do_version(int argc, char **argv, dev_t *dummyvar)
{
	char *p;

	if (argc != 1) 
		return("VERSION: #");

	if (system_version != -1)
		return("more than one VERSION line");

	system_version = (int)strtoul(*argv, &p, 0);

	if (*p != '\0' || system_version <= 0)
		return("not a valid number after VERSION:");

	if (system_version > MAX_VERSION)
		return(MAX_VERSION_MSG);

	if (system_version < MIN_VERSION)
		return(MAX_VERSION_MSG);

	return(0);
}

/*
 * Handle { path } syntax in ROOTDEV/SWAPDEV/DUMPDEV specs.
 */
static char *
do_devicefile(int argc, char **argv, char **devpath)
{
	if (argc == 1) {
		if (devpath)
			*devpath = strdup(*argv);
		return(NULL);
	}

	return ("syntax error");
}

/*
 * ROOTDEV: { path }
 */
/* ARGSUSED */
static char *
do_rootdevice(int argc, char **argv, dev_t *dummyvar)
{
	return (do_devicefile(argc, argv, &bootrootfile));
}

/*
 * SWAPDEV: { path }  swplo  nswap
 */
/* ARGSUSED */
static char *
do_swapdevice(int argc, char **argv, dev_t *dummyvar)
{
	register char *p;
	int temp;

	if (argc == 3) {
		/*
		 * path swplo nswap
		 *
		 * swapdev = path
		 * swplo = number
		 * nswap = number
		 */
		if (*argv[1] == '=')
			/*
			 * internal prompt response
			 */
			{
			if (0 == strcmp("swapdev",argv[0]))
				return(do_devicefile(1, &argv[2],
						 &bootswapfile));

			if (0 == strcmp("swplo",argv[0])) {
				if (! numeric(&temp,argv[2]))
					return("must be numeric");

				swplo = temp;
				}

			if (0 == strcmp("nswap",argv[0]))
				if (! numeric(&nswap,argv[2]))
					return("must be numeric");

			return(NULL);
			}

		if ((p=do_devicefile(1,argv,&bootswapfile)) != NULL)
			return(p);

		--argc;
		++argv;
	} else
		return("syntax error");

	if (! numeric(&temp,argv[0]))
		return("must be numeric");
	swplo = temp;

	if (! numeric(&nswap,argv[1]))
		return("must be numeric");

	return(NULL);
}

/*
 * DUMPDEV: { path }
 */
/*ARGSUSED*/
static char *
do_dumpdevice(int argc, char **argv, dev_t *device)
{
	return (do_devicefile(argc, argv, &bootdumpfile));
}

#define freet(t) \
	if ((t).v_ifcn) free ((t).v_ifcn); 

/*
 * VECTOR: bustype module [intr] [ipl vector unit base] [probe [probe_size]]
 */
/* ARGSUSED */
static char *
do_vector(int argc, char **argv, dev_t *dummyvar)
{
	paddr_t probe_addr = 0;
	int probe_size = 4;
	int probe_iospace = 0;
	int unit = 0;
	int fd;
	char pbuf[16];
	struct t_edt t_edt;
	char *mod_name = 0;
	int incr = 0;
	int esincr = 0;
	int i, adapters;
	char *type;
	static char errbuf[128];

	bzero(&t_edt, sizeof(t_edt));
	/* default to -1 */
	t_edt.v_ipl = -1;
	t_edt.e_adap = 0;
	t_edt.cpusyscall = -1;
	t_edt.cpuintr = -1;

	while (argc > 0) {
		if (EQUAL("bustype",argv[0])) {
			if (!EQUAL("=",argv[1]) || argc < 2) {
				freet(t_edt);
				return("VECTOR: bustype = ???");
			}
			if (EQUAL("VME",argv[2])) 
				t_edt.e_bustype = ADAP_VME;
			else if (EQUAL("GFX",argv[2])) 
			   	t_edt.e_bustype = ADAP_GFX;
			else if (EQUAL("SCSI",argv[2])) 
			   	t_edt.e_bustype = ADAP_SCSI;
			else if (EQUAL("LOCAL",argv[2])) 
			   	t_edt.e_bustype = ADAP_LOCAL;
			else if (EQUAL("GIO",argv[2])) 
			   	t_edt.e_bustype = ADAP_GIO;
			else if (EQUAL("EISA",argv[2]))
				t_edt.e_bustype = ADAP_EISA;
			else if (EQUAL("IBUS",argv[2]))
				t_edt.e_bustype = ADAP_IBUS;
			else if (EQUAL("EPC",argv[2]))
				t_edt.e_bustype = ADAP_EPC;
			else if (EQUAL("DANG",argv[2]))
				t_edt.e_bustype = ADAP_DANG;
			else if (EQUAL("PCI",argv[2]))
				t_edt.e_bustype = ADAP_PCI;
			else 
			   	t_edt.e_bustype = ADAP_NULL;
			argc -= 3, argv += 3;

		} else if (EQUAL("module",argv[0])) {
			if (!EQUAL("=",argv[1]) || argc < 2) {
				freet(t_edt);
				return("VECTOR: module = ???");
			}
			mod_name=argv[2];
			argc -= 3, argv += 3;

		} else if (EQUAL("intr",argv[0])) {
			if (!(EQUAL("=",argv[1])) || (argc < 2)) {
				freet(t_edt);
				return("VECTOR: intr = ???");
			}
			t_edt.v_ifcn=strcpy(mymalloc(strlen(argv[2])+1),argv[2]);
			argc -= 3, argv += 3;

		} else if (EQUAL("ipl",argv[0])) {
			if (!(EQUAL("=",argv[1])) || (argc < 2)) {
				freet(t_edt);
				return("VECTOR: ipl = ???");
			}
			if( t_edt.e_bustype != ADAP_VME ) {
				freet(t_edt);
				return("VECTOR: VME doesn't support old style vector line");
			}
			t_edt.v_ipl = (unsigned char)strtoul(argv[2], 0, 0);
			argc -= 3, argv += 3;

		} else if (EQUAL("vector",argv[0])) {
			if (!(EQUAL("=",argv[1])) || (argc < 2)) {
				freet(t_edt);
				return("VECTOR: vector = ???");
			}
			t_edt.v_vec = (unsigned char)strtoul(argv[2], 0, 0);
			argc -= 3, argv += 3;

		} else if (EQUAL("unit",argv[0])) {
			if (!(EQUAL("=",argv[1])) || (argc < 2)) {
				freet(t_edt);
				return("VECTOR: unit = ???");
			}
			if( t_edt.e_bustype == ADAP_VME ) {
				freet(t_edt);
				return("VECTOR: VME doesn't support unit=");
			}
			unit = TRUE;
			t_edt.v_unit = (unsigned char)strtoul(argv[2], 0, 0);
			argc -= 3, argv += 3;

		} else if (EQUAL("ctlr",argv[0])) {
			if (!(EQUAL("=",argv[1])) || (argc < 2)) {
				freet(t_edt);
				return("VECTOR: ctlr = ???");
			}
			t_edt.e_ctlr = (uint_t)strtoul(argv[2], 0, 0);
			argc -= 3, argv += 3;

		} else if (EQUAL("adapter",argv[0])) {
			if (!(EQUAL("=",argv[1])) || (argc < 2)) {
				freet(t_edt);
				return("VECTOR: adapter = ???");
			}
			if (EQUAL("*",argv[2])) {
				t_edt.e_adap = (uint_t)-1;
			} else
				t_edt.e_adap = (uint_t)strtoul(argv[2], 0, 0);
			argc -= 3, argv += 3;

		} else if (EQUAL("slot", argv[0])) {
			if (!(EQUAL("=",argv[1])) || (argc < 2)) {
				freet(t_edt);
				return("VECTOR: slot = ???");
			}
			if (t_edt.e_adap != 0) {
				freet(t_edt);
				return("VECTOR: slot: adapter already set");
			}
			t_edt.e_adap = (uint_t)strtoul(argv[2], 0, 0) << IBUS_SLOTSHFT; 
			argc -= 3; argv += 3;

		} else if (EQUAL("ioa", argv[0])) {
			if (!(EQUAL("=", argv[1])) || (argc < 2)) {
				freet(t_edt);
				return("VECTOR: ioa = ???");
			}
			t_edt.e_adap |= (strtoul(argv[2], 0, 0) & IBUS_IOAMASK);
			argc -= 3; argv += 3;
	
		} else if (EQUAL("base",argv[0])) {
			if (!(EQUAL("=",argv[1])) || (argc < 2)) {
				freet(t_edt);
				return("VECTOR: base = ???");
			}
			t_edt.t_base = (kernptr_t)strtoull(argv[2], 0, 0);
			argc -= 3, argv += 3;

		} else if (EQUAL("base2",argv[0])) {
			if (!(EQUAL("=",argv[1])) || (argc < 2)) {
				freet(t_edt);
				return("VECTOR: base2 = ???");
			}
			t_edt.t_base2 = (kernptr_t)strtoull(argv[2], 0, 0);
			argc -= 3, argv += 3;

		} else if (EQUAL("base3",argv[0])) {
			if (!(EQUAL("=",argv[1])) || (argc < 2)) {
				freet(t_edt);
				return("VECTOR: base3 = ???");
			}
			t_edt.t_base3 = (kernptr_t)strtoull(argv[2], 0, 0);
			argc -= 3, argv += 3;

		} else if (EQUAL("iospace",argv[0])) {
			if (argc >= 9 && EQUAL("=",argv[1]) &&
				EQUAL("(", argv[2]) && EQUAL(")",argv[8])) {
				type=strcpy(mymalloc(strlen(argv[3])+1),argv[3]);
				if (parse_base(type, &t_edt, 0)) {
					freet(t_edt);
					sprintf(errbuf, "VECTOR: ios type= %s", type);
					return errbuf;
				}
				t_edt.e_space[0].ios_iopaddr = 
					strtoul(argv[5], 0, 0);
				if (*argv[7] != '*')
					t_edt.e_space[0].ios_size = 
						strtoul(argv[7], 0, 0);
				free(type);
				argc -= 9, argv += 9;
			} else {
				freet(t_edt);
				return("VECTOR: iospace = ???");
			}

		} else if (EQUAL("iospace2",argv[0])) {
			if (argc >= 9 && EQUAL("=",argv[1]) &&
				EQUAL("(", argv[2]) && EQUAL(")",argv[8])) {
				type=strcpy(mymalloc(strlen(argv[3])+1),argv[3]);
				if (parse_base(type, &t_edt, 1)) {
					freet(t_edt);
					sprintf(errbuf, "VECTOR: ios type= %s", type);
					return errbuf;
				}
				t_edt.e_space[1].ios_iopaddr = 
					strtoul(argv[5], 0, 0);
				if (*argv[7] != '*')
					t_edt.e_space[1].ios_size = 
						strtoul(argv[7], 0, 0);
				free(type);
				argc -= 9, argv += 9;
			} else {
				freet(t_edt);
				return("VECTOR: iospace2 = ???");
			}

		} else if (EQUAL("iospace3",argv[0])) {
			if (argc >= 9 && EQUAL("=",argv[1]) &&
				EQUAL("(", argv[2]) && EQUAL(")",argv[8])) {
				type=strcpy(mymalloc(strlen(argv[3])+1),argv[3]);
				if (parse_base(type, &t_edt, 2)) {
					freet(t_edt);
					sprintf(errbuf, "VECTOR: ios type= %s", type);
					return errbuf;
				}
				t_edt.e_space[2].ios_iopaddr = 
					strtoul(argv[5], 0, 0);
				if (*argv[7] != '*')
					t_edt.e_space[2].ios_size = 
						strtoul(argv[7], 0, 0);
				free(type);
				argc -= 9, argv += 9;
			} else {
				freet(t_edt);
				return("VECTOR: iospace3 = ???");
			}

		} else if (EQUAL("probe",argv[0])) {
			if (!(EQUAL("=",argv[1])) || (argc < 2)) {
				freet(t_edt);
				return("VECTOR: probe = ???");
			}
			probe_addr = strtoul(argv[2], 0, 0);
			argc -= 3, argv += 3;

		} else if (EQUAL("probe_size",argv[0])) {
			if (!(EQUAL("=",argv[1])) || (argc < 2)) {
				freet(t_edt);
				return("VECTOR: probe_size = ???");
			}
			probe_size = (int)strtoul(argv[2], 0, 0);
			argc -= 3, argv += 3;
		} else if (EQUAL("exprobe",argv[0])) {
			if (!EQUAL("=",argv[1])) {
				freet(t_edt);
				return ("VECTOR: exprobe: syntax error");
			}
			if (incr = exprobe_parse(argc, argv)) {
				argc -= incr;
				argv += incr;
			} else {
				freet(t_edt);
				return ("VECTOR: exprobe: syntax error");
			}

		} else if (EQUAL("exprobe_space",argv[0])) {
			if (!EQUAL("=",argv[1])) {
				freet(t_edt);
				return ("VECTOR: exprobe_space: syntax error");
			}
			if (esincr = exprobesp_parse(argc, argv,
				t_edt.e_bustype)) {
				argc -= esincr;
				argv += esincr;
			} else {
				freet(t_edt);
				return ("VECTOR: exprobe_space: syntax error");
			}

		} else if (EQUAL("probe_space",argv[0])) {
			if (argc >= 9 && EQUAL("=",argv[1]) &&
				EQUAL("(", argv[2]) && EQUAL(")",argv[8])) {
				type=strcpy(mymalloc(strlen(argv[3])+1),argv[3]);
				if (parse_base(type, &t_edt, probe_base)) {
					freet(t_edt);
					sprintf(errbuf, "VECTOR: ios type= %s", type);
					return errbuf;
				}
				t_edt.e_space[probe_base].ios_iopaddr = 
					strtoul(argv[5], 0, 0);
				if (*argv[7] != '*')
					t_edt.e_space[probe_base].ios_size = 
						strtoul(argv[7], 0, 0);
				probe_iospace = 1;
				free(type);
				argc -= 9, argv += 9;
			} else {
				freet(t_edt);
				return("VECTOR: probe_space = ???");
			}

		} else if (EQUAL("syscallcpu", argv[0])) {
		    if (argc >= 3 && EQUAL("=", argv[1])) {
			char *c;
			for(c = argv[2]; *c; c++)
			    if (!isdigit(*c))
				goto syscallcpuerr;
			t_edt.cpusyscall = atoi(argv[2]);
			argc -= 3;
			argv += 3;
		    }
		    else {
		      syscallcpuerr:
			freet(t_edt);
			return("VECTOR: syscallcpu = ???");
		    }
			
		} else if (EQUAL("intrcpu", argv[0])) {
		    if (argc >= 3 && EQUAL("=", argv[1])) {
			char *c;
			for(c = argv[2]; *c; c++)
			    if (!isdigit(*c))
				goto intrcpuerr;
			t_edt.cpuintr = atoi(argv[2]);
			argc -= 3;
			argv += 3;
		    }
		    else {
		      intrcpuerr:
			freet(t_edt);
			return("VECTOR: intrcpu = ???");
		    }
	        } else if (EQUAL("probe_path", argv[0])) {
			if (!EQUAL("=",argv[1]) || argc < 2) {
				freet(t_edt);
				return("VECTOR: probe_path = ???");
			}
			if (strlen(argv[2]) > MAXPATHLEN) {
			        freet(t_edt);
				return("VECTOR: probe_path pathname too long.");
			}
			t_edt.probe_path = (char *)malloc(MAXPATHLEN);
			if (t_edt.probe_path == NULL) {
			        freet(t_edt);
				return("VECTOR:probe_path no memory left");
			}
			strcpy(t_edt.probe_path, argv[2]);
			argc -= 3, argv += 3;

		} else {
			freet(t_edt);
			sprintf(errbuf, "VECTOR: unknown specifier %s", argv[0]);
			return errbuf;
		}
	}

	if (!mod_name)
		return("VECTOR: missing module name");

	if ((t_edt.v_vec) && (t_edt.e_adap == (uint_t)-1))
		return("VECTOR: vector is hardwired, but adapter is unknow???");
	
	/* If the probe path is specified, check to see if it exists
	 * in the system. If not, don't configure. */
	if (t_edt.probe_path != NULL) {
	         char command[MAXPATHLEN + 40];  /* 40 is sort of random */
		 sprintf(command, "%s -d %s > /dev/null 2> /dev/null", 
			 __PATH_LS, t_edt.probe_path);
		 if (!system(command))
		         gen_entry(mod_name, &t_edt);
		 else
			return NULL;
	}


	/*
	 * For wildcards other than DANG chips, we will iterate over
	 * all possible adapters here.
	 */
	if (t_edt.e_adap == (uint_t)-1 && t_edt.e_bustype != ADAP_DANG) {
		i = 0;
		adapters = (int)syssgi(SGI_CONFIG, ADAP_READ, t_edt.e_bustype);
	}
	else {
		i = t_edt.e_adap;
		adapters = i+1;
	}

	for ( ; i < adapters; i++ ) {
		t_edt.e_adap = i;
		/* XXX syssgi() should be modify to give the number of
		 * DANG chips, and edt_init() in kern/os/main.c
		 * should be changed to get the number of DANG chips
		 * from readadapters(), and then here and above should
		 * generate separate EDT entries for each DANG chip.
		 *
		 * Until then, let the drivers to all of the work of 
		 * scanning for their boards.
		 */
		if (t_edt.e_bustype == ADAP_DANG && i == -1)
			t_edt.e_adap = 0;

		/* perform old style probe */
		if (probe_addr) {
			fd = open("/dev/kmem", O_RDONLY);
			if (fd < 0)
				error(ER2, "/dev/kmem");

			if (0 > lseek(fd, (off_t)probe_addr & LBOOT_LSEEK_MASK, 0))
				error(ER2, "/dev/kmem");

			if (probe_size != read(fd, pbuf, probe_size)) {
				if (Verbose)
					printf("%s, unit %d probe failed\n",
					       mod_name, 
						((unit == 0) ? t_edt.e_ctlr:t_edt.v_unit));
				(void)close(fd);
				freet(t_edt);
				/*
				 * stub-out missing hardware
				 */
				(void)searchdriver(mod_name);
				continue;
			}

			if (Verbose)
				printf("probed %s unit %d\n", mod_name, 
					((unit == 0) ? t_edt.e_ctlr:t_edt.v_unit));
			(void)close(fd);
		}
		/* perform new style probe space */
		if ((noprobe == 0) && (probe_iospace == 1) ) {
			if (syssgi(SGI_IOPROBE, t_edt.e_bustype, i, 
				&t_edt.e_space[probe_base], IOPROBE_READ, 0)) {
				if (Verbose)
					printf("%s, adapter %d probe space failed\n",
						mod_name, i);
				/*
				 * stub-out missing hardware
				 */
				(void)searchdriver(mod_name);
				continue;
			} else {
				if (Verbose)
					printf("probe spaced %s adapter %d\n", 
						mod_name, i);
			}
		}
		/* perform old sytle extended probe */
		if (incr) {
			if (!exprobe()) {
				if (Verbose)
					printf("%s, unit %d exprobe failed\n",
					       mod_name, 
						((unit == 0)
						 ? t_edt.e_ctlr
						 : t_edt.v_unit));
				freet(t_edt);
				/*
				 * stub-out missing hardware
				 */
				(void)searchdriver(mod_name);
				continue;
			}
			if (Verbose)
				printf("exprobed %s unit %d\n", mod_name, 
					((unit == 0)
					 ? t_edt.e_ctlr
					 : t_edt.v_unit));
		}
		/* perform new style extended probe */
		if ((noprobe == 0) && esincr) {
			if( !exprobesp(i,t_edt.e_bustype) ) {
				if (Verbose)
					printf("%s, adapter %d exprobe space failed\n",
						mod_name, i);
				/*
				 * stub-out missing hardware
				 */
				(void)searchdriver(mod_name);
				continue;
			}
			if (Verbose)
				printf("exprobe spaced %s adapter %d\n",
					mod_name, i);
		}

		if (t_edt.e_bustype == ADAP_EPC)
			epc_module(mod_name, &t_edt);
		else
			gen_entry(mod_name, &t_edt);
	}

	/* free up extended probe lists */
	if (incr)
		free_Vtuple_list();

	if (esincr)
		free_Ntuple_list();

	return(NULL);
}

static void
gen_entry(char *mod_name, struct t_edt *t_edt)
{
	struct driver *dp = 0;
	struct master *mp;
	struct lboot_vme_intrs *vp;
	struct lboot_edt *ep;
	int i;
	int maxnctl;
	int adap;


	dp = searchdriver(mod_name);
	if (!dp || !dp->dname) {
		if (t_edt->v_ifcn) 
			free (t_edt->v_ifcn);
		if (Verbose)
			printf("Driver not found for %s device\n", mod_name);
		return;
	}

	mp = dp->opthdr;
	dp->flag |= INEDT;
	/*
	 * Enforce some slightly obscure semantics regarding nctl: the
	 * number of controllers should never be less than the maximum
	 * unit number probed (plus 1). Then, drivers can use the ##C
	 * hook, without fear of a unit #1 with no unit #0.
	 */
	if (noprobe == 0) {
		adap = (int)syssgi(SGI_CONFIG, ADAP_READ, t_edt->e_bustype);
		if (t_edt->e_adap == -1)
			maxnctl = max(t_edt->e_ctlr * adap + t_edt->e_adap + 1,
				      t_edt->v_unit + 1);
		else
			maxnctl = max(t_edt->e_ctlr + 1, t_edt->v_unit + 1);
		dp->nctl = max(dp->nctl, maxnctl);
	} else {
		maxnctl = max(t_edt->e_ctlr + 1, t_edt->v_unit+1);
		dp->nctl = max(dp->nctl+1, maxnctl);
	}


	/*
	 * see if there is edt structure existant to which to attach this
	 */
	for (ep=dp->edtp; ep; ep=ep->e_next) {
		if (t_edt->t_base == ep->e_space[0].ios_vaddr
		    && t_edt->t_base2 == ep->e_space[1].ios_vaddr
		    && t_edt->t_base3 == ep->e_space[2].ios_vaddr
		    && t_edt->e_ctlr  == ep->e_ctlr
		    && t_edt->e_adap  == ep->e_adap)
			break;
	}
	if (!ep) {
		ep = (struct lboot_edt *)mycalloc(sizeof(struct lboot_edt),1);
		ep->e_next = dp->edtp;
		dp->edtp = ep;
		ep->e_bus_type = t_edt->e_bustype;
		ep->e_adap = t_edt->e_adap;
		ep->e_ctlr = t_edt->e_ctlr;
		for (i=0; i < NBASE; i++)
			ep->e_space[i] = t_edt->e_space[i];
		
		if (t_edt->t_base)
			ep->e_space[0].ios_vaddr = t_edt->t_base;
		if (t_edt->t_base2)
			ep->e_space[1].ios_vaddr = t_edt->t_base2;
		if (t_edt->t_base3)
			ep->e_space[2].ios_vaddr = t_edt->t_base3;
		vp = (struct lboot_vme_intrs *)ep;
		if (t_edt->cpusyscall >= 0) {
		    ep->v_cpusyscall = t_edt->cpusyscall;
		    ep->v_setcpusyscall = 1;
		}
		else
		    ep->v_setcpusyscall = 0;
		if (t_edt->cpuintr >= 0) {
		    ep->v_cpuintr = t_edt->cpuintr;
		    ep->v_setcpuintr = 1;
		}
		else
		    ep->v_setcpuintr = 0;

	} else {
		for (vp=(struct lboot_vme_intrs *)ep; vp->v_next; vp=vp->v_next)
			continue;
	}

	/*
	 * funny case -- an edt without a interrupt routine
	 *  we build the edt with a null vme_intrs pointer
	 */
	if (!t_edt->v_vec && (t_edt->v_ipl == (uchar_t)-1))
		return;

	vp->v_next = (struct lboot_vme_intrs *)mycalloc(sizeof(struct lboot_vme_intrs),1);
	vp = vp->v_next;
	if (t_edt->v_ifcn) {
		vp->v_vname = t_edt->v_ifcn;
	} else {
		/*
		 * use prefix|"intr" if interrupt function not specified
		 * use tag|"intr" if interrupt function not specified ???
		 */
		vp->v_vname = concat(mp->prefix, "intr");
	}
	vp->v_vec = t_edt->v_vec;
	vp->v_brl = t_edt->v_ipl;
	vp->v_unit = t_edt->v_unit;

	return;
}



struct {
	char *mod_name;
	int  mod_num;
} mod_names[]  = {
	{ "epcserial", EPC_SERIAL },
	{ "epcether",	EPC_ETHER  },
	{ "", 0			   }
};


void
epc_module(char *module, struct t_edt *t_edt)
{
	ibus_t *ib = &ibus_adapters[ibus_numadaps++];
	int i;
	for (i = 0; mod_names[i].mod_num != 0; i++) {
		if (strcasecmp(mod_names[i].mod_name, module) == 0)
			break;
	}

	if (mod_names[i].mod_num == 0) {
		
		printf("Error: Invalid EPC module name: '%s'\n", 
			module);
		myexit(1);
	}
	ib->ibus_module = mod_names[i].mod_num;
	ib->ibus_unit	= t_edt->v_unit;
	ib->ibus_adap	= t_edt->e_adap;
	ib->ibus_ctlr	= t_edt->e_ctlr;
	ib->ibus_proc	= -1;
}


int
get_base(int bustype, char *type)
{
	switch (bustype) {

	case ADAP_VME:
		if (EQUAL("A16NP",type)) 
		   return PIOMAP_A16N;
		else if (EQUAL("A16S",type)) 
		   return PIOMAP_A16S;
		else if (EQUAL("A24NP",type)) 
		   return PIOMAP_A24N;
		else if (EQUAL("A24S",type)) 
		   return PIOMAP_A24S;
		else if (EQUAL("A32NP",type)) 
		   return PIOMAP_A32N;
		else if (EQUAL("A32S",type)) 
		   return PIOMAP_A32S;
		else if (EQUAL("A64",type)) 
		   return PIOMAP_A64;
		else 
			return(-1);
	
	case ADAP_EISA:
		if (EQUAL("EISAIO",type))
		   return PIOMAP_EISA_IO;
		else if (EQUAL("EISAMEM",type))
		   return PIOMAP_EISA_MEM;
		else
			return -1;

	case ADAP_LOCAL:

		if (EQUAL("LANRAM",type)) 
		   return LAN_RAM;
		else if (EQUAL("LANIO",type)) 
		   return LAN_IO;
		else if (EQUAL("ETMEM",type)) 
		   return ET_MEM;
		else if (EQUAL("ETIO",type)) 
		   return ET_IO;
		else 
			return(-1);

	case ADAP_IBUS:
		if (EQUAL("FCI",type)) 
		   return PIOMAP_FCI;
		else
			return(-1);

	case ADAP_DANG:
		if (EQUAL("DANG",type)) 
		   return PIOMAP_FCI;
		else
			return(-1);

	case ADAP_PCI:
		if (EQUAL("PCIID",type)) 
		   return PIOMAP_PCI_ID;
		else
			return(-1);
	}

	return -1;
}

static int
parse_base(char *type, struct t_edt *t_edt, int nbase)
{
	int val;

	val = get_base(t_edt->e_bustype,type);

	if( val == -1 )
		return -1;

	t_edt->e_space[nbase].ios_type = val;
	
	return(0);
}

/* 
 * recombine parts of "parsed" string
 */
static char *
unparse(int argc, char **argv)
{
	char *s;

	s = 0;
	while (argc--) {
		s = concat_free(s, argv[0]);
		/* combine isolated '=' and ',' with neighbors */
		while (argc && (argv[1][0] == '=' || argv[1][0] == ',' ||
				argv[1][0] == ':')) {
			argv++; argc--;
			s = concat_free(s, argv[0]);
			if (argc) {
				argv++; argc--;
				s = concat_free(s, argv[0]);
			}
		}
		argv++;
		s = concat_free(s, " ");
	}
	return s;
}


/* 
 * recombine parts of "parsed" command, and add toolpath to the right part
 *	If we see TOOLROOT=xxx before the command, then do not fiddle with
 *	the command.  Otherwise, add toolpath when we first see the command.
 */
static char *
unparse_cmd(int argc, char **argv, char *cmd)
{
	char *s;
	int clen, l;

	clen = (int)strlen(cmd);
	s = 0;
	while (argc--) {
		if (0 != cmd
		    && (l = (int)strlen(argv[0])-clen) >= 0
		    && !strcmp(cmd,argv[0]+l)) {
			cmd = concat(toolpath,argv[0]);
			s = concat_free(s, cmd);
			cmd = 0;
		} else {
			s = concat_free(s, argv[0]);
		}
		/* combine isolated '=' and ',' with neighbors */
		if (argc
		    && (argv[1][0] == '=' || argv[1][0] == ',')) {
			if (0 != cmd
			    && !strcmp("TOOLROOT",argv[0])
			    && argv[1][0] == '=')
				cmd = 0;
			argv++; argc--;
			s = concat_free(s, argv[0]);
			if (argc) {
				argv++; argc--;
				s = concat_free(s, argv[0]);
			}
		}
		argv++;
		s = concat_free(s, " ");
	}
	return s;
}


/* 
 * CC: strings...
 */
/* ARGSUSED */
static char *
do_cc(int argc, char **argv, dev_t *dummyvar)
{
	if (cc)
		return("CC already specified");
	cc = unparse_cmd(argc, argv, "cc");
	return(0);
}


/*
 * CCOPTS: strings...
 *
 * ``deparse'' CCOPTS string into ccopts[]
 */
/* ARGSUSED */
static char *
do_ccopts(int argc, char **argv, dev_t *dummyvar)
{
	if (ccopts)
		return("CCOPTS already specified");
	ccopts = unparse(argc,argv);
	return(NULL);
}


/* 
 * LD: strings...
 */
/* ARGSUSED */
static char *
do_ld(int argc, char **argv, dev_t *dummyvar)
{
	if (ld)
		return("LD already specified");
	ld = unparse_cmd(argc, argv, "ld");
	return(0);
}


/*
 * LDOPTS: strings...
 *
 * ``deparse'' LDOPTS string into ldopts[]
 */
/* ARGSUSED */
static char *
do_ldopts(int argc, char **argv, dev_t *dummyvar)
{
	if (ldopts)
		return("LDOPTS already specified");
	while (argc--) {
		if (EQUAL(argv[0],"-o") && argc>0) {
			ldopts = concat_free(ldopts, argv[0]);
			ldopts = concat_free(ldopts, " ");
			argv++;
			argc--;
			if (argv[0][0] != '/') {
				ldopts = concat_free(ldopts, cwd_slash);
			}
		}
		ldopts = concat_free(ldopts, argv[0]);
		while (argc && argv[1][0] == ',') {
			argv++, argc--;
			ldopts = concat_free(ldopts, argv[0]);
			if (argc) {
				argv++; argc--;
				ldopts = concat_free(ldopts, argv[0]);
			}
		}
		ldopts = concat_free(ldopts, " ");
		argv++;
	}
	return(NULL);
}


/*
 * Read with error checking; if an I/O error occurs, quit.
 */
void
read_and_check(int fildes, char *fname, char *buf, unsigned nbyte)
{
	register ssize_t rbyte;

	if ((rbyte = read(fildes,buf,nbyte)) < 0)
		error(ER2, fname);
	
	if (rbyte != nbyte)
		error(ER2, fname);
}

/*
 * Function(prefix, name, symbol)
 *
 * Test whether the symbol is prefix-name
 */
boolean
function(char *prefix, char *name, char *symbol)
{
	register size_t len = strlen(prefix);

	return(0 == strncmp(prefix,symbol,len) &&
	       0 == strcmp(name,&symbol[len]));
}

/*
 * Suffix (string, suffix)
 * Given "string" and 'suffix', suffix()
 * returns true if "string" ends with .suffix
 */
int
suffix(char *string, char *suf)
{
	char *dotpos;

	if ((dotpos = strrchr(string, '.')) == NULL)
		return(0);
	if (EQUAL(++dotpos, suf))
		return(1);
	return(0);
}

/*
** 
** Figure out which processor this driver should be locked into
** default is 0, if no info  was given
**
** if the driver has an edt structure with an ipl level, then the cpu to
** lock this driver on is given from the array cpuipl[]
**
** Error checking
** 1. if one edt entry that wish to send intr to a particular cpu then
** for a processor locked driver, the driver has to be locked on to 
** that cpu and all other edt entries of this driver 
** must also send their intr to that cpu regardless of their IRQ level
** 
** 2. if there are two edt entries of the same IRQ with conflicting intr 
** destinations then the error is unrecoverable. The user has to reconfig 
** the system file. 
** 
** 3. One driver can have multiple edt entries, each might be configured
** for a diffrent ipl levels, ea ipl level can be locked on a different cpu
**
** This check is done by intrlock_sanity_check() later across all drivers
** after lock_on_cpu() was done on all processor locked drivers
*/
int
lock_on_cpu(struct master *mp, struct driver *dp)
{
	struct lboot_edt     *edtp;		/* pointer to edt structure */
	char cpu = -1;
	int ipl;
	struct lboot_vme_intrs *intrs;

	if (dp->flag & INEDT) {
		/* 
		** if driver has edt structure with a valid ipl level
		** select the edt entry with a valid ipl and set v_cpuintr
		** based on the cpuipl[] array
		*/	
		for (edtp = dp->edtp; edtp; edtp = edtp->e_next) {
			intrs = edtp->v_next;
			if (!intrs || intrs->v_brl == (unsigned char)-1)
				continue;
			ipl = intrs->v_brl;
			cpu = cpuipl[ipl];
			/* if was not specified then default to 0 */
			if (cpu == (char)-1) {
				cpuipl[ipl] = 0;
				cpu = 0;
			}
			edtp->v_cpuintr = cpu;
			edtp->v_setcpuintr = 1;
			
		}
	}
	/* 
	** by now all the edt entries of the driver are set up with
	** the given info from the system file
	** lets do some error checking
	*/
	if (mp->sema == S_PROC) {
		/* 
		** if driver is of processor locking type, then
		** all edt entries's ipl levels must be locked to the same
		** cpu. 
		*/
		/* use earlier result if there is any */
		if (cpu == (char)-1)
			cpu = 0;

		/* if driver has no edt list then done */	
		if (!(dp->flag & INEDT))
			goto lock_done;
			
		/* now go thru. the edt list of the driver again and make sure
		** that all edt's ipl levels are locked to the same cpu
		*/
		for (edtp = dp->edtp; edtp; edtp = edtp->e_next) {
			intrs = edtp->v_next;
			if (!intrs || intrs->v_brl == (unsigned char)-1)
				continue;
			if (edtp->v_cpuintr != cpu) {
				error(ER106,
				      dp->mname,intrs->v_brl,edtp->v_cpuintr);
			}
		}
		goto lock_done;

	}
	/* regular driver just return -1 */
	cpu = -1;
lock_done:
	return(cpu);
}


/*
** If there are two edt entries of the same IRQ with conflicting intr 
** destinations then the error is unrecoverable. The user has to reconfig 
** the system file. 
*/
void
intrlock_sanity_check(void)
{
	struct driver *dp;
	struct lboot_edt *ep;
	struct dcpu {
		char cpu;
		char *mname;
	} cpu_of_irq[MAXIPL], *cpuirq;
	int iplcount = 0;
	int i;
	struct lboot_vme_intrs *intrs;

	/* IRQ start from 0*/
	for (i=0; i<MAXIPL; i++) {
		cpu_of_irq[i].cpu = -1;
		cpu_of_irq[i].mname = 0;
	}

	/*
	** go thru all driver, for ea IRQ level look for first edt entry 
	** with matching IRQ and v_setcpuintr set
	*/ 
	for (dp = driver; dp; dp = dp->next) {
		for (ep = dp->edtp; ep; ep = ep->e_next) {
			intrs = ep->v_next;
			if (intrs && (intrs->v_brl != (unsigned char)-1) &&
					ep->v_setcpuintr && 
					cpu_of_irq[intrs->v_brl].cpu == (char)-1) {
				cpuirq = &cpu_of_irq[intrs->v_brl];
				cpuirq->cpu = ep->v_cpuintr;
				cpuirq->mname = dp->mname;
				iplcount++;
				if (iplcount == MAXIPL)
					break;
			}
		}
		if (iplcount == MAXIPL)
			break;
	}
				
	/* now go thru all driver, check out every edt entries to make sure
	** that no edt entries with the same IRQ can have different intr 
	** destinations. 
	*/	
	for (dp = driver; dp; dp = dp->next) {
		for (ep = dp->edtp; ep; ep = ep->e_next) {
			intrs = ep->v_next;
			if (!intrs || intrs->v_brl == (unsigned char)-1)
				continue;
			/* is this edt's intr IRQ locked to a particular cpu */
			if (cpu_of_irq[intrs->v_brl].cpu != (char)-1) {
				/* can this edt intr destination still
				** be moved around?
				*/
				cpuirq = &cpu_of_irq[intrs->v_brl];
				if (!ep->v_setcpuintr) {
					ep->v_cpuintr = cpuirq->cpu;
					ep->v_setcpuintr = 1;
				}
				else if (ep->v_cpuintr != cpuirq->cpu) {
					if (!strcmp(dp->mname,cpuirq->mname)) 
						error(ER102, dp->mname, ep->v_cpuintr);
					else
					error(ER103, intrs->v_brl,dp->mname,ep->v_cpuintr,cpuirq->mname,cpuirq->cpu);
				}
					
			}
		}
	}
		
		
}

static	int	dev_first_call = 1;
static	int	drv_first_call = 1;

/*
 * Called at exit to get rid of the device admin .c file.
 */
void
cleanup_dev_admin(void)
{
	char cmd[1024];

	sprintf(cmd, "rm -f %s\n", tmp_dev_admin_dot_c);
	system(cmd);
}

/*
 * Called at exit to get rid of the driver admin .c file.
 */
void
cleanup_drv_admin(void)
{
	char cmd[1024];

	sprintf(cmd, "rm -f %s\n", tmp_drv_admin_dot_c);
	system(cmd);
}

/*
 * create the device administration info table in master.c
 * this function processes a single device / driver administration
 * directive in the system.gen file. basically it parses the various
 * fields in the directive and builds the equivalent entries in the
 * static device administration info table.	
 */
char *
do_device_admin(int argc,char **argv,dev_t *devp)
{
	char			**p = argv;
	char			*name;
	FILE 			*fp;
	
	devp = devp;		/* satisfy the compiler */

	/* make sure that we have atleast 3 arguments corr. to
	 * <parameter-name> = <parameter-value>
	 */
	if (argc < 3)
		return NULL;

	if (dev_first_call) {
		
		dev_first_call = 0;
		if ((fp=fopen(tmp_dev_admin_dot_c,"w")) == NULL) {
			fprintf(stderr, "lboot: unable to create %s.\n",
				tmp_dev_admin_dot_c);
			exit (1);
		}
		/* Make sure the file gets removed. */
		atexit(cleanup_dev_admin);
		generate_dev_admin_table_head(fp);
		generate_dev_admin_table_tail(fp);
	} else {
		if ((fp = fopen(tmp_dev_admin_dot_c,"r+")) == NULL)
			return NULL;
		fseek64(fp,0,SEEK_END);	/* skip over all the previous
					 * material in temporary dev_admin.c
					 * file
					 */
	}



	if (*p)
		name = *p;
	/* skip over the device / driver name */
	p++;
	argc--;

	
	fseek64(fp,-(long)DAT_DEV_TAILLEN,SEEK_END);

	/* for each <parameter> = < value> sequence iterate */
	while (argc) {
		p = device_admin(name,p,&argc,fp);
	}
	generate_dev_admin_table_tail(fp);
	fflush(fp);
	fclose(fp);
	return NULL;


}
/*
 * create the driver administration info table in master.c
 * this function processes a single device / driver administration
 * directive in the system.gen file. basically it parses the various
 * fields in the directive and builds the equivalent entries in the
 * static driver administration info table.	
 */
char *
do_driver_admin(int argc,char **argv,dev_t *devp)
{
	char			**p = argv;
	char			*name;
	FILE 			*fp;

	devp = devp;		/* satisfy the compiler */

	/* make sure that we have atleast 3 arguments corr. to
	 * <parameter-name> = <parameter-value>
	 */
	if (argc < 3)
		return NULL;
	if (drv_first_call) {
		
		drv_first_call = 0;
		if ((fp=fopen(tmp_drv_admin_dot_c,"w")) == NULL) {
			fprintf(stderr, "lboot: unable to create %s.\n",
				tmp_drv_admin_dot_c);
			exit (1);
		}
		/* Make sure the file gets removed. */
		atexit(cleanup_drv_admin);
		generate_drv_admin_table_head(fp);
		generate_drv_admin_table_tail(fp);
	} else {
		if ((fp = fopen(tmp_drv_admin_dot_c,"r+")) == NULL)
			return NULL;
		fseek64(fp,0,SEEK_END);	/* skip over all the previous
					 * material in temporary drv_admin.c
					 * file
					 */
	}



	if (*p)
		name = *p;
	/* skip over the device / driver name */
	p++;
	argc--;

	
	fseek64(fp,-(long)DAT_DRV_TAILLEN,SEEK_END);

	/* for each <parameter> = < value> sequence iterate */
	while (argc) {
		p = device_admin(name,p,&argc,fp);
	}
	generate_drv_admin_table_tail(fp);
	fflush(fp);
	fclose(fp);
	return NULL;


}
