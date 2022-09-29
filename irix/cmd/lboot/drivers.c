/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

#ident "$Revision: 2.129 $"

#include "tune.h"
#include <a.out.h>
#include <ar.h>
#include <elf.h>
#include <sys/types.h>
#include <sys/param.h>
#include <syslog.h>
#include <sys/sysmacros.h>
#define vme_intrs kernel_vme_intrs
#define iospace kernel_iospace
#define edt kernel_edt
#include <sys/edt.h>
#include <sys/vmereg.h>
#include <sys/conf.h>
#include <limits.h>
#undef vme_intrs 
#undef iospace 
#undef edt
#include "lboot.h"
#include "boothdr.h"

static char kernel_object[1024];	/* kernel.o */

#define UNKNOWN_FMT	1
#define COFF_FMT	2
#define ELFO32_FMT	3
#define ELFN32_FMT	4
#define ELF64_FMT	5

static char *format_names[] = {
				"unknown",
				"COFF",
				"ELF O32",
				"ELF N32",
				"64-bit ELF"
			};
			
#define FORMAT_NAME(fmt)	(format_names[(fmt)-1])

#define LOAD_RESERVE	-1	/* reserve this position in major table 
				   for a loadable driver*/

static void generate(FILE *), edt_gen(FILE *);
#ifdef NEVER
static void devflags_print(FILE *, uint);
#endif
static void load_value_for_object(char *, void *, void *, void *);

extern char any_error;			/* any error seen in master.d */
extern void warn(char *, ...);


struct kernel kernel;			/* head of object file linked list */
struct driver *driver;			/* head of struct driver linked list */

extern char   *mtune_kernel;		/* mtune/kernel file name */

static int	number_drivers;		/* total # of drivers to be loaded */

static int got_init, got_halt, got_start, got_reg, got_unreg, got_sanity;

struct rtname {
	char *name;
	char *routine;
} rtname[] = {
	/* functions defined in the UNIX kernel */
	/* these must be in one-to-one correspondence with RNULL,RNOSYS,... */
	/* [RNOTHING]*/	{ "",		"{ }"	},
	/* [RNULL] */	{ "nulldev",	"{ return(nulldev());}"	},
	/* [RNOSYS] */	{ "nosys",	"{ return(nosys());}"	},
	/* [RNODEV] */	{ "nodev",	"{ return(nodev());}"	},
	/* [RTRUE] */	{ "true",	"{ return(1);}"	},
	/* [RFALSE] */	{ "false",	"{ return(0);}"	},
	/* [RFSNULL]*/	{ "fsnull",	"{ return(fsnull());}"	},
	/* [RFSSTRAY]*/	{ "fsstray",	"{ return(fsstray());}"	},
	/* [RNOPKG]*/	{ "nopkg",	"{ return(nopkg());}"	},
	/* [RNOREACH]*/	{ "noreach",	"{ noreach(); }"	},
	/* [RNODEVFLAG]*/{ "nodevflag",	"{ }"	},
	/* [RZERO]*/	{ "0",		"{ }"	},
			{  0,		0	}
};

char Nzero[] = "0";
char Nnofile[] = "nofile";

/*
 * private copies of variables that will be generated for UNIX
 */
int             cdevcnt;
int             bdevcnt;
int		fmodcnt;
int		nfstype;

char		*bootrootfile = NULL;
char		*bootswapfile = NULL;
char		*bootdumpfile = NULL;

dev_t           rootdev = NODEV;
dev_t           swapdev = NODEV;
dev_t           dumpdev = NODEV;

daddr_t         swplo = -1;
int             nswap = -1;
int		system_version = -1;
int		noprobe;		/* set means this is a ship kernel */
extern int	nointr_count;
extern cpuid_t	nointr_list[]; /* CPUs that shouldn't take intrs */
char 		cpuipl[MAXIPL] = { -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1 };

/*
 * structures used to generate io subsystem code
 */
struct Bdevsw {
	char	*d_prefix;
	char 	*d_flags;
	int	d_cpulock;
	char	*d_open;
	char	*d_close;
	char	*d_strategy;
	char	*d_print;
	char	*d_map;
	char	*d_unmap;
	char	*d_dump;
	char	*d_size;
	char	*d_size64;
	char	*d_bdevsw;
};
struct Cdevsw {
	char	*d_prefix;
	char	*d_flags;
	int	d_cpulock;
	char	*d_open;
	char	*d_close;
	char	*d_read;
	char	*d_write;
	char	*d_ioctl;
	char 	*d_mmap;
	char	*d_map;
	char	*d_unmap;
	char	*d_poll;
	char	*d_attach;
	char	*d_detach;
	char	*d_enable;
	char	*d_disable;
	char	*d_ttys;
	char	*d_str;
	char	*d_cdevsw;
};
struct Fmodsw {
	char	*f_name;
	char	*f_strmtab;
	char	*f_flags;
};

struct Vfssw {
	char *vfs_name;
	char *vfs_init;
	char *vfs_vfsops;
	char *vfs_vnodeops;
	char *vfs_dying_vnodeops;
	long vfs_flag;
};

struct Bdevsw *bdevswp;
struct Cdevsw *cdevswp;

int Major[NMAJORS];
struct Fmodsw *fmodswp;
struct Vfssw *vfsswp;
char **io_init;
char **next_init;
char **io_start;
char **next_start;
char **io_reg;
char **next_reg;
char **io_unreg;
char **next_unreg;
char **io_halt;
char **next_halt;


/*
 * Static function declarations for this file
 */
static void             build_io_subsys(FILE *);
static void             dependency(register struct driver *);
static char		*dependld(register struct driver *, char *);
static struct driver	*finddriver(char *);
static void             print_configuration(void);
static void		edt_gen(register FILE *);
static void		print_configuration(void);
static void		dump_source(struct driver *dp, FILE *file);
static void		initialize_subsys(void);
static void		load_values(struct driver *);
static void		generate(register FILE *);
static int		get_ar_fmt(int);
static int		driver_matches_kernel(struct driver *);

/*
 * finddriver()
 *
 * Search the master directory for master file "name".
 */
static struct driver *
finddriver (char *name)
{
	register struct master *mp;
	struct driver *dp;
	register size_t i;
	int fd = -1;
	char *tmpstr;
	struct stat statbuf;

	dp = NULL;
	mp = NULL;

	dp = (struct driver*)mymalloc(sizeof(struct driver));

	dp->flag = 0;
	dp->nctl = 0;
	dp->edtp = 0;
	dp->mname = (char *)NULL;
	dp->dname = (char *)NULL;
	dp->magic = 0;

	if ((dp->opthdr = mp = mkboot(name)) == 0) {
		goto badriver;
	}
	strcpy(mp->name, name);

/*
 * SARAH: See comments in mkboot.y. We currently parse the alias
 * strings from a master file, if they exist, and place the info
 * in the master structure, but don't use this information yet.
 * The following code is here for debugging to make sure that 
 * strings were added.
 */
#ifdef	NOTYET
	if (mp->naliases) {
		register struct alias *al;
		register char *sp;

		al = (struct alias *) POINTER(mp->o_alias, mp);
		for (i=0; i<mp->naliases; i++, al++) {
			sp=POINTER(al->str,mp);
			printf ("load_value_for_object sp=%s\n", sp);
		}
	}
#endif

	if (mp->flag & KOBJECT)
		/*
		 * ignore kernel objects
		 */
		goto badriver;

	for (i=0; i<mp->nsoft; i++) {
		if (mp->soft[i] >= NMAJORS) {
			/* Driver <name>: major number greater than NMAJORS */
			error(ER13, name);
			goto badriver;
		}
	}

	dp->mname = mymalloc( strlen(name) + 1 );
	strcpy(dp->mname,name);

	/*
	 * see if we have an object associated with the master
	 */

	i = strlen(name);
	dp->dname = mymalloc(i + 3);
	(void) strcat(strcpy(dp->dname,name),".o");

	if (stat(dp->dname,&statbuf) == -1) {
		dp->dname[i+1] = 'a';
		if (stat(dp->dname,&statbuf) == -1) {
			free(dp->dname);
			dp->dname = (char *) NULL;
			goto nodriver;
		}
	} else {
		tmpstr = mymalloc(i + 3);
		(void) strcat(strcpy(tmpstr,name),".a");
		if (stat(tmpstr,&statbuf) == 0) {
			printf ("WARNING: lboot found both %s and %s in the boot directory\n",
				dp->dname, tmpstr);	
			syslog (LOG_WARNING, 
				"WARNING found both %s and %s in the boot directory\n",
				dp->dname, tmpstr);	
		}
		free (tmpstr);
	}

	if ((statbuf.st_mode & S_IFMT) != S_IFREG) {
		goto badriver;
	}

nodriver:
	/*
 	 * master is OK
	 */
	dp->next = driver;
	driver = dp;

	return(dp);

badriver:
	if (mp)
		free((char*)mp);
	if (dp->mname)
		free(dp->mname);
	if (dp->dname)
		free(dp->dname);
	if (dp)
		free((char*)dp);

	if (fd != -1) {
		(void) close(fd);
		fd = -1;
	}

	return((struct driver *)0);
}


/*
 * Print_configuration()
 *
 * Print the configuration table; this is only done if Debug is set
 */
static void
print_configuration(void)
{

	struct driver *dp;
	struct master *mp;
	boolean issued;
	int i;

	printf("\nCONFIGURATION SUMMARY\n=====================\n     ----driver---- #devices,imajor,emajor\n");

	/*
	 * handle drivers
	 */
	for (dp = driver; dp != 0; dp = dp->next) {
		if (! (dp->flag & LOAD) ||
		   ((mp=dp->opthdr)->flag & NOTADRV))
			continue;
		if ((mp->flag & (CHAR|BLOCK|FUNMOD|FUNDRV)) == FUNMOD)
			continue;

		printf("    %15s %d,%d", dp->mname, dp->nctl, dp->int_major);
		
		if (! (mp->flag & (BLOCK|CHAR|FUNDRV)))
			printf("\n");
		else {
			for (i=0; i<mp->nsoft; i++)
				printf(",%d", mp->soft[i]);
			printf ("\n");	
		}
	}

	/*
	 * handle modules
	 */
	issued = FALSE;
	for (dp = driver; dp != 0; dp = dp->next) {
		mp = dp->opthdr;
		if (!(dp->flag & LOAD) ||
		   !(mp->flag & (NOTADRV | FUNMOD)))
			continue;

		if (! issued) {
			/*
			 * print heading for first module found
			 */
			issued = TRUE;
			printf("\n     ----module----\n");
		}

		printf("     %s\n", dp->mname);
	}

	printf("\n");
}

/*
 * searchdriver(name)
 *
 * Search the driver linked-list to locate driver `name'; when found, return
 * the pointer to the struct driver entry; if not found return NULL.
 */
struct driver *
searchdriver(char *name)
{
	register struct driver *dp;

	for (dp = driver; dp != 0; dp = dp->next) {
		if (EQUAL(dp->mname,name))
			return(dp);
	}

	return(finddriver(name));
}

/*
 * Include(dname, number)
 *
 * Mark driver `dname' to be included for `number' more controllers.
 *
 * If the driver was found in the EDT, then `number' is ignored since the
 * number of controllers is already determined.  Likewise, if the driver
 * is a required driver, then the number of controllers must be set to one,
 * so `number' is ignored.
 *
 * If the driver was not found in the EDT, then it cannot be included
 * unless it is a software driver, or it is just an independent module.
 */
void
include(register char *dname, int number)
{
	register struct driver *dp;
	register struct master *mp;

	if ((dp=searchdriver(dname)) == NULL)
		/* INCLUDE: <dname>; driver not found */
		error(ER18, dname);
	else {
		if (dp->flag & EXCLUDE) {
			/* INCLUDE: <dname>; driver is EXCLUDED */
			error(ER19, dname);
			return;
		}

		if (dp->flag & INEDT) {
			/*
			 * it will be included based on number of times in EDT
			 */
			dp->flag |= INCLUDE;
			return;
		}

		mp = dp->opthdr;
		if (mp->flag & STUB)
			dp->flag |= LOADLAST;

		if (mp->flag & (NOTADRV | FUNMOD)) {
			dp->flag |= INCLUDE;
			dp->nctl += number;
			return;
		}

		if (mp->flag & SOFT) {
			dp->flag |= INCLUDE;
			if (! (mp->flag & REQ))
				dp->nctl += number;
		} else
			/* INCLUDE: <dname>; device not equipped */
			error(ER20, dname);
	}
}

/*
 * Exclude(dname)
 *
 * Mark driver `dname' to be excluded
 */
void
exclude(register char *dname)
{

	register struct driver *dp;

	dp=searchdriver(dname);
	if (dp != 0) {
		if (dp->flag & INCLUDE)
			/* EXCLUDE: <dname>; driver is INCLUDED */
			error(ER21, dname);
		else
			dp->flag |= EXCLUDE;
	}
}


/*
 * Dependency(pdriver)
 *
 * Driver *pdriver has dependencies.  Find them in the driver linked-list, mark
 * them to be loaded, and follow their dependencies.
 */
static void
dependency(register struct driver *pdriver)
{
	struct driver *dp;
	struct master *m, *mp = pdriver->opthdr;
	struct depend *dep;
	int count;
	char name[MAXNAMLEN+3];

	dep = (struct depend*) POINTER(mp->o_depend, mp);

	for (count=0; count<mp->ndep; ++count, ++dep) {

		dp=searchdriver(strcpy(name,(char *)POINTER(dep->name,mp)));

		/* <pdriver->name>: dependent driver <name> not available */
		if (dp==(struct driver *)NULL) {
			error(ER22, pdriver->mname, name);
			continue;
		}

		if (dp->flag & LOAD)
			/*
			 * already marked to be loaded
			 */
			continue;

		/* <pdriver->name>: dependent driver <name> is EXCLUDED */
		if (dp->flag & EXCLUDE) {
			error(ER23, pdriver->mname, name);
			continue;
		}

		m = dp->opthdr;
		if (!((m->flag & (SOFT|NOTADRV)) ||
		    ((m->flag & (CHAR | BLOCK | FUNMOD | FUNDRV)) == FUNMOD)) &&
		   !(dp->flag & INEDT)) {
			/*
			 * driver is not a software driver (ie. it is a
			 * hardware driver) but the hardware does not exist
			 *
			 * <pdriver->name>: device not equipped for dependent driver <name>
			 */
			error(ER24, pdriver->mname, name);
			continue;
		}

		/* Check if object format matches kernel */
		if (!driver_matches_kernel(dp)) 
			continue;

		dp->flag |= LOAD;

		if (m->ndep > 0)
			/*
			 * follow the dependency chain
			 */
			dependency(dp);

		if ((m->flag & (SOFT|NOTADRV) ||
		   ((m->flag & (CHAR | BLOCK | FUNMOD | FUNDRV)) == FUNMOD)) &&
		    ! (dp->flag & INEDT) && dp->nctl == 0)
			/*
			 * make sure that #C is set if not done already
			 */
			dp->nctl = 1;
	}
}

/*
 * dependld(pdriver)
 *
 * Driver *pdriver has dependencies.  Find them and if they are .a's
 * add them to the passed in list..
 */
static char *
dependld(register struct driver *pdriver, char *ldopt)
{
	struct driver *dp;
	struct master *mp = pdriver->opthdr;
	struct depend *dep;
	int count;
	char name[MAXNAMLEN+3];

	dep = (struct depend*) POINTER(mp->o_depend, mp);

	for (count=0; count<mp->ndep; ++count, ++dep) {

		dp=searchdriver(strcpy(name,(char *)POINTER(dep->name,mp)));

		/* <pdriver->name>: dependent driver <name> not available */
		if (dp==(struct driver *)NULL) {
			error(ER22, pdriver->mname, name);
			continue;
		}

		/* only interested if these are marked to be loaded */
		if (!(dp->flag & LOAD))
			continue;

		if (dp->dname != 0 && suffix(dp->dname, "a")) {
			ldopt = concat_free(ldopt, dp->dname);
			ldopt = concat_free(ldopt, " ");
		}
		/*
		 * XXX don't follow dependencies since there are lots
		 * of loops in the dependency tree...
		 */
#ifdef NEVER
		if (dp->opthdr->ndep > 0)
			/*
			 * follow the dependency chain
			 */
			ldopt = dependld(dp, ldopt);
#endif
	}
	return ldopt;
}

/*
 * Given a filename, determine if its a loadable module or not.
 * 
 * NOTE: this routine assumes that the driver list has already been
 *	 created.
 */
static int
isloadable (char *fname)
{
	struct driver *dp;
	char tmpname[MAXNAMELEN];

	if (strlen(fname) < 3)	/* the file better have a .o extension */
		return 0;

	strncpy (tmpname, fname, (strlen(fname) - 2));
	tmpname[strlen(fname) - 2] = '\0';

	dp = driver;
	while (dp) {
		if (strcmp (dp->mname, tmpname) == 0)
			if (dp->opthdr->flag & DYNAMIC)
				return 1;
		dp = dp->next;
	}

	return 0;
}

/*
 *	This routine links the objects specified by the system list
 */
void
loadunix(void)
{
	register struct driver *dp, *dpp = NULL;
	register struct master *mp;
	int i, j;
	register int fd;
	register FILE *stream;
	int b_major, c_major, bc_major;
	union commonhdr {
		FILHDR fhdr;
		Elf32_Ehdr elfhdr;
		char armag[SARMAG];
	} commonhdr;
	struct stat statbuf;
	register FILE *nfdot;
	char master_dot_c[MAXPATHLEN+14];
	char master_dot_o[MAXPATHLEN+14];
	char new_edt_dot[MAXPATHLEN+14];
	char old_edt_dot[MAXPATHLEN+14];
	char reason[MAXPATHLEN+14+MAXPATHLEN+14+10];
	char *cc_cmd, *ld_cmd, *tmpdir;
	register int c1, c2;
	register FILE *olds;
	register DIR *dirp;
	register struct dirent *dep;
	struct stat nsb, osb;
	int relink;
	time_t lmtime;		/* largest mtime */

	lmtime = 0;
	cdevcnt = bdevcnt = fmodcnt = nfstype = 0;

	openlog("lboot", LOG_ODELAY, LOG_AUTH);

	/*
	 * Parse the system file.  Quit if it is bad.
	 *
	 * If we fail and need to exit somewhere w/i the path
	 * through parse_system(), enable switch to print some
	 * warnings to user about possible incomplete autoregistration.
	 */
	do_mreg_diags = 1;
	if (0 != parse_system()) {
		myexit(1);
	}
	do_mreg_diags = 0;

	for (i=0; i< NMAJORS; ++i)
		Major[i] = DONTCARE;

	/* 
	 * Register all loadable modules requested for pre-registration.
	 */
	if (Smart & !Noautoreg)
		do_mregister ();
	if (Autoreg) {
		printf ("Warning: lboot using -a - performing auto-register only\n");
		do_mregister ();
		exit(0);
	}

	/*
	 * save EDT structures
	 */
	(void) sprintf(old_edt_dot, "%s/edt.list", slash_runtime);
	(void) sprintf(new_edt_dot, "%s/edt.list.new", slash_runtime);
	stream = fopen(new_edt_dot, "w");
	if (stream != 0) {
		edt_gen(stream);
		(void)fclose(stream);
	}


	if (Smart) {
		if (Nogo && Verbose)
			printf ("\nPerforming dry run to determine if a new kernel should be built...\n");

		relink = 0;
		if (0 == stream) {
			sprintf (reason, "fopen %s failed", new_edt_dot);
			relink = 1;
		}
		if (!relink && 0 != stat(unix_name, &statbuf)) {
			sprintf (reason, "stat %s failed", unix_name);
			relink = 1;
		}
	}

	/* we go through the files in var/sysgen even if we're
	 * not Smart in case any is timestamped in the future.
	 * if that's the case, we'll timestamp unix in the future
	 * too
	 */

	/* if master.d/x is newer than unix, reconfig */
	if (0 != (dirp = opendir(master_dot_d))) {
		(void)chdir(master_dot_d);
		while (0 != (dep = readdir(dirp))) {
			if (dep->d_ino != 0
			    && stat(dep->d_name, &nsb) == 0
			    && (nsb.st_mode & S_IFMT) == S_IFREG
			    && statbuf.st_mtime < nsb.st_mtime) {

				if (nsb.st_mtime > lmtime)
					lmtime = nsb.st_mtime;

				if (!Smart || relink) /* already have reason */
					continue;

				relink = 1;
				sprintf (reason, "%s/%s newer than %s",
					master_dot_d, dep->d_name,
					unix_name);
			}
		}
		(void)chdir(slash_boot);
		(void)closedir(dirp);
	}

	/* if boot/x is newer than unix, reconfig */
	if (0 != (dirp = opendir("."))) {
		while (0 != (dep = readdir(dirp))) {
			if (dep->d_ino != 0
			    && 0 != strcmp(dep->d_name, "edt.list.new")
			    && stat(dep->d_name, &nsb) == 0
			    && (nsb.st_mode & S_IFMT) == S_IFREG
			    && statbuf.st_mtime < nsb.st_mtime) {
				/* 
				 * If its a loadable module, don't
				 * reconfig for it.
				 */
				if (isloadable(dep->d_name))
					continue;

				if (nsb.st_mtime > lmtime)
					lmtime = nsb.st_mtime;

				if (!Smart || relink)
					continue;

				relink = 1;
				sprintf (reason, "%s/%s newer than %s",
					slash_boot, dep->d_name, 
					unix_name);
			}
		}
		(void)closedir(dirp);
	}


        /* if /var/sysgen/stune is missing or newer than unix, reconfig */
	if (stat(stune_file, &nsb) < 0){
	    if (Smart && !relink) {
		relink = 1;
		sprintf (reason, "%s is missing", stune_file);
	    }
	} else if (statbuf.st_mtime < nsb.st_mtime) {
	    if (nsb.st_mtime > lmtime)
		lmtime = nsb.st_mtime;
	    if (Smart && !relink) {
		relink = 1;
		sprintf (reason, "%s is newer than %s", stune_file,
							unix_name);
	    }
	}

	/* if /var/sysgen/boot is missing or newer than unix, reconfig */
	if (stat(slash_boot, &nsb) < 0){
	    if (Smart && !relink) {
		relink = 1;
	    	sprintf (reason, "%s is missing", slash_boot);
	    }
	} else if (statbuf.st_mtime < nsb.st_mtime) {
	    if (nsb.st_mtime > lmtime)
		lmtime = nsb.st_mtime;
	    if (Smart && !relink) {
		relink = 1;
	    	sprintf (reason, "%s is newer than %s", slash_boot, unix_name);
	    }
	}
		
		
	/* if /var/sysgen/system/x is newer than unix, reconfig */
	if (stat(etcsystem, &nsb) < 0) {
		if (Smart && !relink) {
		    relink = 1;
	   	    sprintf (reason, "%s stat failed", etcsystem);
		}
	}
	else if (S_ISDIR(nsb.st_mode)) {
		int i, nentries;
		char system_path[PATH_MAX];
		struct dirent **systems;

		if ((nentries = scandir(etcsystem, &systems, sys_select, alphasort)) < 0) {
			if (Smart && !relink) {
			    relink = 1;
			    sprintf (reason, "scandir %d failed", etcsystem);
			}
		}
		else {
			for (i = 0; i < nentries; i++) {
				strcpy(system_path, etcsystem);
				strcat(system_path, "/");
				strcat(system_path, systems[i]->d_name);
				if ((stat(system_path, &nsb) < 0) ||
						statbuf.st_mtime < nsb.st_mtime) {
					if (nsb.st_mtime > lmtime)
						lmtime = nsb.st_mtime;
					if (!Smart || relink)
						continue;

					relink = 1;
					sprintf (reason, "%s is newer than %s",
						system_path, unix_name);
				}
				free(systems[i]);
			}
			free(systems);
		}
	} else {
		if (Smart && !relink) {
		    relink = 1;
		    sprintf (reason, "%s is not a directory", 
			     etcsystem);
		}
	}

	if (Smart) {
		/* if the edt files seem similar, do a slow but easy comparison */
		olds = stream = NULL;
		if (!relink
		    && (olds = fopen(old_edt_dot, "r")) != 0
		    && fstat(fileno(olds), &osb) >= 0
		    && (stream = fopen(new_edt_dot, "r")) != 0
		    && fstat(fileno(stream), &nsb) >= 0
		    && osb.st_size == nsb.st_size) {
			for (;;) {
				i = getc(olds);
				if (i != getc(stream)) {
					relink = 1;
		    			sprintf (reason, "%s and %s differ",
						old_edt_dot, new_edt_dot);
					break;
				}
				if (i == EOF) {
					if (statbuf.st_mtime > time(0)) {
						syslog(LOG_WARNING, "WARNING: kernel seems current but has a modification time in the future");
						fprintf(stderr, "lboot: WARNING: kernel seems current but has a modification time in the future\n");
					}
					if (Verbose)
					    printf("kernel already current\n");
						(void)unlink(new_edt_dot);
						exit(0);
				}
			}
		} else {
			if (!relink) {
				relink = 1;
				sprintf (reason, "%s and %s differ",
					old_edt_dot, new_edt_dot);
			}
		}
		if (stream)
			(void)fclose(stream);
		if (olds)
			(void)fclose(olds);

		if (Nogo) {
			printf ("New kernel would have been built: %s.\n",
				reason);
			(void)unlink(new_edt_dot);
			exit(1);
		}

		/*
		 * Autoconfig is 1 by default, which means just go ahead and
		 * do the autoconfig. This can be overridden by the 
		 * autoconfig.options file, in which case the Auto question
		 * will be asked.
		 */
		if (!Autoconfig) {
			if (Verbose)
				printf ("\n%s...\n", reason);
			printf("Automatically reconfigure the operating system (y or n)? ");
			c1 = c2 = getchar();
			while (c2 != EOF && c2 != '\n')
				c2 = getchar();
			if (c1 != 'y' && c1 != 'Y') {
				(void)unlink(new_edt_dot);
				exit(1);
			}
		} else {
			if (Verbose)
				printf ("\n%s...\n", reason);
			printf ("Automatically reconfiguring the operating system.\n");
			syslog (LOG_NOTICE, 
				"Automatically reconfiguring the operating system");
		}
	}

	/*
	 * open the kernel object file, and do sanity checks
	 */
	if (!kernel_master)
		error(ER101, etcsystem);

	strcat(strcpy(kernel_object,kernel_master),".o");

	if (stat(kernel_object,&statbuf) < 0
		|| (fd=open(kernel_object,O_RDONLY)) < 0)
		error(ER2, kernel_object);

	/*
	 * Read file header and verify it.
	 */
	read_and_check(fd, kernel_object, (char*)&commonhdr, sizeof(commonhdr));
	(void)close(fd);

	if (!IS_ELF(commonhdr.elfhdr) &&
	    !IS_MIPSEBMAGIC(commonhdr.fhdr.f_magic) &&
	    !IS_MIPSELMAGIC(commonhdr.fhdr.f_magic) &&
	    !IS_SMIPSEBMAGIC(commonhdr.fhdr.f_magic) &&
	    !IS_SMIPSELMAGIC(commonhdr.fhdr.f_magic)) {
		/* <kernel_object>: not an object file */
		error(ER32, kernel_object);
	}

	if (IS_ELF(commonhdr.elfhdr)) {
		/* Determine ABI of kernel.o */
		if (commonhdr.elfhdr.e_ident[EI_CLASS] == ELFCLASS64)
			kernel.magic = ELF64_FMT;
		else if (commonhdr.elfhdr.e_ident[EI_CLASS] == ELFCLASS32) {
			if (commonhdr.elfhdr.e_flags & EF_MIPS_ABI2)
				kernel.magic = ELFN32_FMT;
			else
				kernel.magic = ELFO32_FMT;
		}
	}
	else
		kernel.magic = COFF_FMT;

	kernel.mname = kernel_master;
	kernel.dname = kernel_object;
	kernel.next = driver;

	if ((driver == NULL) || (driver->next == driver))
		error(ER1, "no driver linked-list was built");

	/*
	 * make sure we have everything we need
	 */
	if (bootrootfile == NULL)
		error(ER1, "no ROOTDEV specified in system file");

	/*
	 * Determine all drivers to be loaded.  We compare object
	 * file types here with the base kernel module to make sure
	 * that everything is in sync.  If for some reason we have
	 * an entry in this list that is bogus, then we remove and
	 * free up the memory.
	 */
	for (dp = driver; dp; dpp = dp, dp = dp->next) {			
		/* Skip driver if we've already decided it is the wrong format */
		if (dp->magic !=0 && !dp->dname)
			continue;
			
		/*
		 * If dp->magic is 0, we need to assign it now.  This
		 * is rather expensive, but we have no choice.
		 */
		if (dp->magic == 0 && dp->dname) {
			if ((fd = open(dp->dname, O_RDONLY)) == -1) {
				/* <name>: perror() message */
				error(ER7, dp->dname);
				if (dp == driver)
					driver = dp->next;
				else
					dpp->next = dp->next;
				if (dp->opthdr)
					free((char*)dp->opthdr);
				if (dp->mname)
					free(dp->mname);
				if (dp->dname)
					free(dp->dname);
				if (dp)
					free((char*)dp);
				if (fd == -1)
					(void) close(fd);
				continue;
			}

			read_and_check(fd, dp->dname, (char *)&commonhdr,
				       sizeof (commonhdr));

			if (!IS_ELF(commonhdr.elfhdr) &&
			    !IS_MIPSEBMAGIC(commonhdr.fhdr.f_magic) &&
			    !IS_MIPSELMAGIC(commonhdr.fhdr.f_magic) &&
			    !IS_SMIPSEBMAGIC(commonhdr.fhdr.f_magic) &&
			    !IS_SMIPSELMAGIC(commonhdr.fhdr.f_magic) &&
			    (memcmp(&commonhdr.armag, ARMAG, SARMAG))) {
				/* Driver <name>: not a valid object file */
				error(ER15, dp->dname);
				if (dp == driver)
					driver = dp->next;
				else
					dpp->next = dp->next;
				if (dp->opthdr)
					free((char*)dp->opthdr);
				if (dp->mname)
					free(dp->mname);
				if (dp->dname)
					free(dp->dname);
				if (dp)
					free((char*)dp);	
				close(fd);
				continue;
			}         

			if (IS_ELF(commonhdr.elfhdr)) {
				if (commonhdr.elfhdr.e_ident[EI_CLASS] == ELFCLASS64)
					dp->magic = ELF64_FMT;
				else if (commonhdr.elfhdr.e_ident[EI_CLASS] == ELFCLASS32) {
					if (commonhdr.elfhdr.e_flags & EF_MIPS_ABI2)
						dp->magic = ELFN32_FMT;
					else
						dp->magic = ELFO32_FMT;
				}
			}
			else if (memcmp(&commonhdr.armag, ARMAG, SARMAG) == 0)
				dp->magic = get_ar_fmt(fd);
			else
				dp->magic = COFF_FMT;				
						
			close(fd);
			
			/*
		 	 * Magic number test.  The object format of dp must
			 * match that of kernel.
			 */
			if (dp->magic != kernel.magic) {
				/* Driver <name>: <driver_format> object is incompatible with <kernel_format> kernel; object ignored */
				error(ER16, dp->dname, FORMAT_NAME(dp->magic), FORMAT_NAME(kernel.magic));
				syslog (LOG_WARNING,
				        "WARNING %s: object file not in ELF format; object ignored\n",
				        dp->dname);
				free(dp->dname);
				dp->dname = NULL;
				dp->flag &= ~LOAD;
				continue;
			}
		}

		if ((mp=dp->opthdr)->flag & REQ) {
			if (dp->flag & EXCLUDE) {
				/* <dp->mname>: required driver is EXCLUDED */
				error(ER36, dp->mname);
				continue;
			}

			dp->flag |= LOAD;
			dp->nctl = 1;
		}

		if (dp->flag & EXCLUDE)
			continue;

		if (dp->flag & INEDT)
			dp->flag |= LOAD;

		if (dp->flag & INCLUDE)
			dp->flag |= LOAD;

		if ((dp->flag & LOAD) && mp->ndep > 0)
			dependency(dp);

		if ((mp->flag & DYNAMIC) && !Dolink) {
		    dp->flag |= DYNLOAD;
		    dp->flag &= ~LOAD;
		}		
	}
	
	/*
	 * If a loadable driver specifies its major number, we need to 
	 * temporarily set that slot aside in the MAJOR table.
	 */

	dp = driver;
	while (dp) {
		if ((dp->dname) && (dp->opthdr->flag & DYNAMIC) && 
				(dp->opthdr->flag & DYNREG) &&
				(dp->opthdr->soft[0] != DONTCARE))
			for (i=0; i<dp->opthdr->nsoft; i++)
				Major[dp->opthdr->soft[i]] = LOAD_RESERVE;
		dp = dp->next;
	}

	/*
	 * Assign the internal major numbers.  This is a two pass approach,
	 * first the drivers which are both BLOCK and CHAR are assigned numbers
	 * then the remaining drivers are assigned.  This will minimize the size
	 * of the character and block device switch tables. Also count the
	 * number of cdevs and bdevs here.
	 */

	bc_major = 0;
	for (i=0; i<2; ++i) {
		b_major = c_major = bc_major;
		dp = driver;
		do {
			if (! (dp->flag & LOAD))
				continue;

			mp = dp->opthdr;
			if (!(mp->flag & (BLOCK|CHAR|FUNDRV))) /* SOFT? */
				continue;

			if (i == 0) {
				if ((mp->flag & (BLOCK|CHAR)) == (BLOCK|CHAR)) {
					dp->int_major = bc_major++;
					cdevcnt++;
					bdevcnt++;
				} else {
					continue;
				}
			} else {
				if ((mp->flag & (BLOCK|CHAR)) == BLOCK) {
					dp->int_major = b_major++;
					bdevcnt++;
				} else {
					if ((mp->flag & (BLOCK|CHAR)) == CHAR
					    || mp->flag & FUNDRV) {
						dp->int_major = c_major++;
						cdevcnt++;
					} else
						continue;
				}
			}

			/*
			 * Assign internal major number to external major
			 * numbers. There can be multiple external major
			 * numbers, but they all map to one internal number.
			 * This is the case where there are multiple major
			 * numbers specified in one master file.
			 */
			for (j=0; j<mp->nsoft; j++) {
				if (mp->soft[j] != DONTCARE) {
					if (Major[mp->soft[j]] != DONTCARE &&
					    Major[mp->soft[j]] != LOAD_RESERVE)
						fprintf(stderr,
			"Warning: major number collision -- %d\n", mp->soft[j]);
					Major[mp->soft[j]] = dp->int_major;
				}
			}
		} while (dp = dp->next);
	}

	dp = driver;

	for (i=0; i< NMAJORS; ++i)
		if (Major[i] == LOAD_RESERVE)
			Major[i] = DONTCARE;


	/*
	 * Compute fmodcnt; also count the total number of drivers 
	 * to be loaded, and interrupt routines that will be needed.
	 */

	dp = driver;
	do	{

		if (!(dp->flag & LOAD))
			continue;

		++number_drivers;

		if ((mp=dp->opthdr)->flag & ONCE && dp->nctl != 1) {
			/* <dp->mname>: flagged as ONCE only; #C set to 1 */
			error(ER37, dp->mname);
			dp->nctl = 1;
		}

		if (mp->flag & FSTYP) {
			++nfstype;
		}

		if (mp->flag & NOTADRV)
			continue;

		if (mp->flag & FUNMOD) {
			++fmodcnt;
			if (!(mp->flag & (CHAR | BLOCK | FUNDRV)))
				continue;
		}

	} while (dp = dp->next);

	++nfstype;

	/*
	 * Print configuration table
	 */
	if (Debug)
		print_configuration();

	/*
	 * build the "master.c" file -- this defines all constants
	 * and will instantiate all extern structures.
	 */

	(void) sprintf(master_dot_c, "%s/master.c", slash_runtime);
	if (tmpdir = getenv("TMPDIR"))
		(void) sprintf(master_dot_o, "%s/master%d.o", tmpdir,
			getpid());
	else
		(void) sprintf(master_dot_o, "/tmp/master%d.o", getpid());

	(void)unlink(master_dot_c);
	if ((nfdot=fopen(master_dot_c,"w")) == (FILE *)NULL)
		error(ER2, master_dot_c);

	/* if -Iroot/usr/include, doesn't come last, then people working
	 *	with their own trees (like many developers) can get mucked up
	 *	because their changed .h files haven't yet (if they ever will
	 *	be) copied to the root/usr/include tree (and they won't be
	 *	able to override the /usr/include tree without resorting to
	 *	full pathnames).  Olson, 4/89 */
	if (0 == cc)
		cc = concat(toolpath, "cc");
	if (noprobe)
		ccopts = concat_free(ccopts, "-DNOPROBE ");
	if (!ccopts) {
		error(ER122, "CCOPTS");
		ccopts = "";
	}
	cc_cmd = mymalloc(64+strlen(cc)+strlen(root)+strlen(ccopts)
		      +strlen(slash_boot)+strlen(master_dot_c)+strlen(master_dot_o));
	sprintf(cc_cmd, "%s -c -I %s -I%s/usr/include -I%s %s -o %s",
		cc, ccopts, root, slash_boot,master_dot_c, master_dot_o);


	fprintf(nfdot,"\n\n/*** DO NOT EDIT THIS FILE - THIS FILE IS GENERATED BY LBOOT ***/\n\n");
	dump_source((struct driver *)&kernel, nfdot);
	gen_tune((struct driver *)&kernel, mtune_kernel);

	for (dp = driver; dp != 0; dp = dp->next) {
		if (dp->flag & (LOAD|DYNLOAD)) {
			dump_source(dp, nfdot);
			gen_tune(dp, (char *)0);
		}
	}
	rdstune();

	if (0 == ld)
		ld = concat(toolpath, "ld");
	if (0 == ldopts)
		error(ER122, "LDOPTS");
	ld_cmd = concat(ld, " ");
	ld_cmd = concat_free(ld_cmd, ldopts);
	ld_cmd = concat_free(ld_cmd, " ");
	ld_cmd = concat_free(ld_cmd, kernel.dname);
	ld_cmd = concat_free(ld_cmd, " ");
	ld_cmd = concat_free(ld_cmd, master_dot_o);
	ld_cmd = concat_free(ld_cmd, " -o ");
	ld_cmd = concat_free(ld_cmd, unix_name);
	if (Smart)
		ld_cmd = concat_free(ld_cmd, ".install");
	ld_cmd = concat_free(ld_cmd, " ");

	/*
	 * Put .o's before .a's ("except after ``c''...")
	 */
	for (dp = driver; dp != 0; dp = dp->next) {
		if (((dp->flag & (LOAD|LOADLAST)) == LOAD)
		    && dp->dname != 0
		    && suffix(dp->dname, "o")) {
			ld_cmd = concat_free(ld_cmd, dp->dname);
			ld_cmd = concat_free(ld_cmd, " ");
		}
	}
	for (dp = driver; dp != 0; dp = dp->next) {
		if (((dp->flag & (LOAD|LOADLAST)) == LOAD)
		    && dp->dname != 0
		    && suffix(dp->dname, "a")) {
			ld_cmd = concat_free(ld_cmd, dp->dname);
			ld_cmd = concat_free(ld_cmd, " ");

			/*
			 * if this .a has any dependencies that are .a's
			 * add them AFTER this .a - this does no harm
			 * but fixes archive dependency problems...
			 */
			ld_cmd = dependld(dp, ld_cmd);
		}
	}
	/* load stub modules last */
	for (dp = driver; dp != 0; dp = dp->next) {
		if (((dp->flag & (LOAD|LOADLAST)) == (LOAD|LOADLAST))
		    && dp->dname != 0) {
			ld_cmd = concat_free(ld_cmd, dp->dname);
			ld_cmd = concat_free(ld_cmd, " ");
		}
	}

	/*
	 * Build all interrupt routines, pcb's,
	 * kernel data structures for I/O, etc.
	 */
	build_io_subsys(nfdot);


	symbol_scan(((struct driver *)&kernel)->dname, find_kernel_sanity,
		    (void *)&kernel,
		    (void *)gfind(kernel.mname),
		    (void *)NULL);
	/* small backward compat - since kernel used to have all of os & ml
	 * in it, look in os.a as well as kernel for sanity functions
	 */
	dp = searchdriver("os");
	if (dp)
		symbol_scan(dp->dname, find_kernel_sanity,
		    (void *)dp,
		    (void *)gfind(kernel.mname),
		    (void *)NULL);

	print_tune(nfdot);

	fprintf(nfdot,"\n\n/* cc %s */\n", cc_cmd);
	fprintf(nfdot,"/* ld %s */\n", ld_cmd);
	(void)fclose(nfdot);

	/* print the device / driver administration support info table
	 * in the master.c file
	 */
	print_dev_admin();

	fflush(stderr);

	/*
	 * Compile new master_dot_c...
	 */
	if (Verbose) {
		printf("%s\n", cc_cmd);
		fflush(stdout);
	}

	/*
	 * Quit if we could not parse any of the master.d files
	 */
	if (any_error)
		exit(1);

	/* 
	 * Now compile it.
	 */
	if ((i=system(cc_cmd)) != 0) {
		fprintf(stderr,"lboot: cc returned %d--failed\n", i);
		(void)unlink(master_dot_o);
		exit(1);
	}

	/*
	 * link master_dot_o, kernel_object, all ``loaded'' drivers
	 */
	if (Verbose) {
		printf("%s\n", ld_cmd);
		fflush(stdout);
	}

	if ((i=system(ld_cmd)) != 0) {
		printf("lboot: ld returned %d--failed\n", i);
		(void)unlink(master_dot_o);
		exit(1);
	}

	if (lmtime) {

		/* lmtime is the largest modification time (mtime)
		 * of any file in /var/sysgen. we just wrote unix
		 * so if mtime(unix) < lmtime then lmtime must be
		 * in the future and we set the unix mtime in the
		 * future as well. this way, autoconfig won't fire
		 * at every reboot.
		 */
		struct utimbuf utb;
		char *kernel_touch;
		kernel_touch = concat(unix_name, (Smart) ? ".install" : "");
		if (stat(kernel_touch, &statbuf) == 0) {
			if (lmtime > statbuf.st_mtime) {
				syslog (LOG_WARNING, "WARNING: there are files in /var/sysgen with mtime in the future");
				fprintf (stderr, "lboot: WARNING: there are files in /var/sysgen with mtime in the future\n");
				utb.actime = statbuf.st_atime;
				utb.modtime = lmtime;
				if (utime(kernel_touch, &utb) == 0) {
					syslog (LOG_WARNING, "WARNING: setting %s mtime to %s", kernel_touch, ctime(&lmtime));
					fprintf (stderr, "lboot: WARNING: setting %s mtime to %s", kernel_touch, ctime(&lmtime));
				}
			}
		}
	}

	if (Smart) {
		printf("\nReboot to start using the reconfigured kernel.\n");
		syslog (LOG_NOTICE, 
			"Reboot to start using the reconfigured kernel");
	}
	(void)rename(new_edt_dot, old_edt_dot);
	(void)unlink(master_dot_o);

	exit(0);
}


extern char master_file[];		/* current master file entity */

/*
 * dump "parameters" sections of master.d file
 */
static void
dump_source(struct driver *dp, FILE *file)
{
	FILE *master;
	register c;
	char *p;
	char sCDM[12];
	char line[LSIZE];
	int nCDM;

	strcpy( master_file, master_dot_d );
	strcat( master_file, "/" );
	strcat( master_file, dp->mname );

	p=strrchr(master_file,'/');
	if ( (p=strchr(p,'.')) != NULL )
		*p = '\0';

	if ((master = fopen(master_file,"r")) == NULL)
		error(ER2, master_file);
	while ((fgets(line, LSIZE, master)) != NULL) {
		if (line[0] == '*')
			continue;
		if (line[0] == '$')
			break;
	}
	while ((c=getc(master)) != EOF) {
		if (c == '#') {
			if ((c=getc(master)) == '#') {
				switch(c=getc(master)) {

				case 'C':
				case 'c':
					nCDM = dp->nctl;
					break;
				case 'D':
				case 'd':
					nCDM = dp->opthdr->ndev;
					break;
				case 'M':
				case 'm':
					nCDM = dp->int_major;
					break;
				case 'E':
				case 'e':
					nCDM = (int) dp->opthdr->soft[0];
					break;
				default:
					fprintf(stderr,"data initializer ##%c unknown -- using zero\n", c);
					nCDM = 0;
					break;
				}
				sprintf(sCDM,"%d",nCDM);
					p = sCDM;
					while (*p)
						(void)putc(*p++,file);
				} else	{
					(void)putc('#',file);
					(void)putc(c,file);
				}
		} else (void)putc(c,file);
	}
	(void) fclose(master);
}

static void
build_io_subsys(FILE *f)
{
	struct driver *dp;

	/*
	 * generate structures and set up default values
	 */
	initialize_subsys();


	dp = driver;

	do {
		if (! (dp->flag & LOAD))
			continue;
		/*
		 * Fill in tables with symbol table values.
		 * We call load_values even though the object
		 * might not exist because we have to set up
		 * driver semaphoring structures.
		 */
		load_values(dp);

	} while (dp = dp->next);

	/* sanity check on the cpu intr locking option */
	intrlock_sanity_check();

	/*
	 * copy the generated data into the temp file
	 */
	generate(f);
}

/*
 * initialize structures for the i/o subsystem
 */
static void
initialize_subsys(void)
{
	register i;
	struct Bdevsw *bp;
	struct Cdevsw *cp;
	struct Fmodsw *fp;
	struct Vfssw *vfsp;

	bdevswp = (struct Bdevsw *)mymalloc(bdevcnt*sizeof(struct Bdevsw));
	for (i=0, bp=bdevswp; i<bdevcnt; i++, bp++) {
		bp->d_prefix = NULL;
		bp->d_open = bp->d_close =
		bp->d_strategy = bp->d_print =
		bp->d_map = bp->d_unmap =
		bp->d_dump = rtname[RNODEV].name;
		bp->d_size = bp->d_size64 = rtname[RNULL].name;
		bp->d_flags = rtname[RNODEVFLAG].name;
		bp->d_bdevsw = NULL;
	}

	cdevswp = (struct Cdevsw *)mymalloc(cdevcnt*sizeof(struct Cdevsw));
	for (i=0, cp=cdevswp; i<cdevcnt; i++, cp++) {
		cp->d_prefix = NULL;

		cp->d_open = cp->d_close = cp->d_map 
		= cp->d_mmap
		= cp->d_unmap = cp->d_read = cp->d_write 
		= cp->d_ioctl = rtname[RNODEV].name;

		cp->d_flags = rtname[RNODEVFLAG].name;

		cp->d_poll = cp->d_attach =
		cp->d_detach = cp->d_enable = 
		cp->d_disable = rtname[RNULL].name;

		cp->d_ttys = cp->d_str = Nzero;
		cp->d_cdevsw = NULL;
	}

	fmodswp = (struct Fmodsw *)mymalloc(fmodcnt*sizeof(struct Fmodsw));
	for (i=0, fp=fmodswp; i<fmodcnt; i++, fp++) {
		fp->f_name = Nnofile;
		fp->f_strmtab = Nzero;
		fp->f_flags = rtname[RNODEVFLAG].name;
	}

	vfsswp = (struct Vfssw *) mycalloc(nfstype*sizeof(struct Vfssw),1);
	vfsp=vfsswp;
	vfsp->vfs_name = "BADFS";
	vfsp->vfs_vfsops = "vfs_strayops";
	vfsp->vfs_vnodeops = "dead_vnodeops";

	io_init = next_init = (char **) mycalloc((number_drivers+1)
						 *(sizeof(io_init)
						   +sizeof(io_start)
						   +sizeof(io_reg)
						   +sizeof(io_unreg)
						   +sizeof(io_halt)), 1);
	io_start = next_start = io_init + (number_drivers+1) ;
	io_reg = next_reg = io_start + (number_drivers+1);
	io_unreg = next_unreg = io_reg + (number_drivers+1);
	io_halt=next_halt=io_unreg + (number_drivers+1);
}

/*
 * load_values
 *
 * Fill in correct values for generated structures
 */
static void
load_values(struct driver *dp)
{
	struct master *mp;
	register struct Bdevsw *b;
	register struct Cdevsw *c;
	register struct Vfssw *vfsp;
	register struct Fmodsw *fp;
	int cpulock;

	got_init = got_halt = got_start = got_reg = got_unreg = 0;

	mp = dp->opthdr;

	b = bdevswp+dp->int_major;
	c = cdevswp+dp->int_major;

	/*
	** for drivers that has edt list, lets go thru. ea edt entry
	** and set the flag v_cpuintr based on the set value of IPL.
	** If the driver is of processor locking type, then check to make sure
	** that the ipl level of all edt entries are locked on the same cpu,
	** also set the d_cpulock flag in bdevsw/cdevsw
	** The return value of lock_on_cpu() is only useful for
	** processor locked drivers
	*/
	if ((dp->flag & INEDT) || (mp->sema == S_PROC))
		/* fig out which cpu to lock to, and error checking */
		cpulock = lock_on_cpu(mp, dp);

	/*
	 * We need to know what type of locking the
	 * driver requires (locking on major number,
	 * processor locking, or none).
	 * If a driver has both raw (char) and block
	 * routines, the char device switch table
	 * must point to the same semaphore structure
	 * as the block device.
	 *
	 */
	if (mp->flag & BLOCK) {
		b->d_cpulock = -1;

		/* set d_cpulock for proc locked driver */
		if (mp->sema == S_PROC)
			b->d_cpulock = cpulock;
		/*
		** if this is also a char then lock to same cpu as the
		** block device
		*/
		if (mp->flag & (FUNDRV|CHAR))
			c->d_cpulock = b->d_cpulock;

	} else if (mp->flag & (FUNDRV|CHAR)) {
	    	if (dp->edtp && dp->edtp->v_setcpusyscall)
		    c->d_cpulock = dp->edtp->v_cpusyscall;
		else
		    c->d_cpulock = -1;
		if (mp->sema == S_PROC)
			c->d_cpulock = cpulock;
	}

	if (!dp->dname) {
		fprintf(stderr,"Warning: no object file for %s\n", dp->mname);
		return;
	}

	if (mp->flag & FSTYP) {
		for (vfsp=vfsswp; vfsp->vfs_name != (char *)0; vfsp++)
			continue;
		vfsp->vfs_name = concat(dp->mname, (char *)0);
		vfsp->vfs_init = concat(mp->prefix, "init");
		vfsp->vfs_vfsops = concat(mp->prefix, "vfsops");
		vfsp->vfs_vnodeops = concat(mp->prefix, "vnodeops");
	}

	/*
	 * inviolate names
	 */

	if (mp->flag & TTYS) {
		c->d_ttys = concat(mp->prefix, "_tty");
	}

	if (mp->flag & FUNMOD) {
		for (fp=fmodswp; strcmp(fp->f_name,Nnofile); fp++)
			continue;
		fp->f_name = mp->name;
		fp->f_strmtab = concat(mp->prefix, "info");
	}

	if (mp->flag & BLOCK) {
		b->d_prefix = mp->prefix;
		b->d_bdevsw = concat(mp->prefix, "bdevsw");
	}

	if (mp->flag & FUNDRV)
		c->d_str = concat(mp->prefix, "info");

	if (mp->flag & (CHAR | FUNDRV)) {
		c->d_prefix = mp->prefix;
		c->d_cdevsw = concat(mp->prefix, "cdevsw");
	}

	symbol_scan(dp->dname, load_value_for_object, dp, gfind(dp->mname), fp);
}

/*
 * load_value_for_object
 *
 * Does much of the dirty work for load_values() above.
 * Called for each global procedure symbol.
 */
static void
load_value_for_object(char *procname, void *arg0, void *arg1, void *arg2)
{
	struct master *mp, *mmp;
	struct driver *dp = (struct driver *)arg0;
	struct tunemodule *gp = (struct tunemodule*)arg1;
	register struct Fmodsw *fp = (struct Fmodsw *)arg2;
	register struct Bdevsw *b;
	register struct Cdevsw *c;
	struct lboot_edt *ep;
	struct driver *ddp;
	struct tunemodule *tmpgp;
	char mgname[80];
	register int i;

	mp = dp->opthdr;

	/*
	 * stub out unnecessary stubs
	 */
	ddp = driver;
	do {

		mmp = driver->opthdr;
		if (mmp->flag & LOAD)
			continue;
		if (mmp->nrtn) {
			register struct routine *rp;
			register char *sp;

			rp = (struct routine *) POINTER(mmp->o_routine, mmp);
			for (i=0; i<mmp->nrtn; ++i, ++rp)
				if ((sp=POINTER(rp->name,mmp)) && ! (strcmp(sp, procname)))
					rp->name = 0;
		}
	} while (ddp = ddp->next);

	/* build up tune_sanity functions */
	tmpgp = gp;
	got_sanity = 0;
	while ((!got_sanity) &&(tmpgp != NULL) && 
	      EQUAL(tmpgp->t_mname, dp->mname)) {
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

		
	if (!(mp->flag & FSTYP)) 
		if ((!got_init) && function(mp->prefix, "init", procname)) {
			*next_init++ =
				strcpy(mymalloc(strlen(procname)+1), procname);
			got_init++;
		}
	if ((!got_start) && function(mp->prefix, "start", procname)) {
		*next_start++ = strcpy(mymalloc(strlen(procname)+1), procname);
		got_start++;
	}
	if ((!got_reg) && function(mp->prefix, "reg", procname)) {
		*next_reg++ = strcpy(mymalloc(strlen(procname)+1), procname);
		got_reg++;
	}
	if ((!got_unreg) && function(mp->prefix, "unreg", procname)) {
		*next_unreg++ = strcpy(mymalloc(strlen(procname)+1), procname);
		got_unreg++;
	}
	if ((!got_halt) && function(mp->prefix, "halt", procname)) {
		*next_halt++ = strcpy(mymalloc(strlen(procname)+1), procname);
		got_halt++;
	}
	if (mp->flag & FUNMOD) {
		if (function(mp->prefix, "devflag", procname)) 
			fp->f_flags = strcpy(mymalloc(strlen(procname)+1),
					     procname);
	}

	b = bdevswp+dp->int_major;
	c = cdevswp+dp->int_major;

	/*
	 * no more special names if not a driver
	 */
	if ((mp->flag & NOTADRV) ||
	    ((mp->flag & (CHAR | BLOCK | FUNMOD | FUNDRV)) == FUNMOD))
		return;

	if (function(mp->prefix, "devflag", procname)) {
		if (mp->flag & BLOCK)
			b->d_flags = strcpy(mymalloc(strlen(procname)+1),
					    procname);
		if (mp->flag & (CHAR | FUNDRV))
			c->d_flags = strcpy(mymalloc(strlen(procname)+1),
					    procname);
	}
	if (function(mp->prefix, "open", procname)) {
		if (mp->flag & BLOCK)
			b->d_open =
				strcpy(mymalloc(strlen(procname)+1), procname);
		if (mp->flag & CHAR)
			c->d_open =
				strcpy(mymalloc(strlen(procname)+1), procname);
	}
	if (function(mp->prefix, "close", procname)) {
		if (mp->flag & BLOCK)
			b->d_close = strcpy(mymalloc(strlen(procname)+1),
					    procname);
		if (mp->flag & CHAR)
			c->d_close = strcpy(mymalloc(strlen(procname)+1),
					    procname);
	}
	if (mp->flag & BLOCK) {
		if (function(mp->prefix,"strategy",procname))
			b->d_strategy = strcpy(mymalloc(strlen(procname)+1),
					       procname);
		if (function(mp->prefix,"print",procname))
			b->d_print = strcpy(mymalloc(strlen(procname)+1),
					    procname);
		if (function(mp->prefix,"dump",procname))
			b->d_dump = strcpy(mymalloc(strlen(procname)+1),
					   procname);
		if (function(mp->prefix,"size",procname))
			b->d_size = strcpy(mymalloc(strlen(procname)+1),
					   procname);
		if (function(mp->prefix,"size64",procname))
			b->d_size64 = strcpy(mymalloc(strlen(procname)+1),
					   procname);
		if (function(mp->prefix,"map",procname))
			b->d_map = strcpy(mymalloc(strlen(procname)+1),
					  procname);
		if (function(mp->prefix,"unmap",procname))
			b->d_unmap = strcpy(mymalloc(strlen(procname)+1),
					    procname);
	}
	if (mp->flag & CHAR) {
		if (function(mp->prefix, "read", procname))
			c->d_read = strcpy(mymalloc(strlen(procname)+1),
					   procname);
		if (function(mp->prefix, "write", procname))
			c->d_write = strcpy(mymalloc(strlen(procname)+1),
					    procname);
		if (function(mp->prefix, "ioctl", procname))
			c->d_ioctl = strcpy(mymalloc(strlen(procname)+1),
					    procname);
		if (function(mp->prefix, "mmap", procname))
			c->d_mmap = strcpy(mymalloc(strlen(procname)+1),
					   procname);
		if (function(mp->prefix, "map", procname))
			c->d_map = strcpy(mymalloc(strlen(procname)+1),
					  procname);
		if (function(mp->prefix, "unmap", procname))
			c->d_unmap = strcpy(mymalloc(strlen(procname)+1),
					    procname);
		if (function(mp->prefix, "poll", procname) ||
		function(mp->prefix, "chpoll", procname))
			c->d_poll = strcpy(mymalloc(strlen(procname)+1),
					   procname);
		if (function(mp->prefix, "attach", procname))
			c->d_attach = strcpy(mymalloc(strlen(procname)+1),
					     procname);
		if (function(mp->prefix, "detach", procname))
			c->d_detach = strcpy(mymalloc(strlen(procname)+1),
					     procname);
		if (function(mp->prefix, "enable", procname))
			c->d_enable = strcpy(mymalloc(strlen(procname)+1),
					     procname);
		if (function(mp->prefix, "disable", procname))
			c->d_disable = strcpy(mymalloc(strlen(procname)+1),
					     procname);
	}
	if (mp->flag & FUNDRV) {
		if (function(mp->prefix, "attach", procname))
			c->d_attach = strcpy(mymalloc(strlen(procname)+1),
					     procname);
		if (function(mp->prefix, "detach", procname))
			c->d_detach = strcpy(mymalloc(strlen(procname)+1),
					     procname);
		if (function(mp->prefix, "enable", procname))
			c->d_enable = strcpy(mymalloc(strlen(procname)+1),
					     procname);
		if (function(mp->prefix, "disable", procname))
			c->d_disable = strcpy(mymalloc(strlen(procname)+1),
					     procname);
	}
	if (dp->flag & INEDT) {
		if (function(mp->prefix, "edtinit", procname)) {
			for (ep=dp->edtp; ep; ep=ep->e_next)
				ep->e_init = strcpy(mymalloc(strlen(procname)+1),procname);
		}
	}
}


/*
 * generate EDT structures
 */
static void
edt_gen(register FILE *f)
{
	register struct driver *dp;
	register struct lboot_edt *ep;
	register struct lboot_vme_intrs *vp;
	int i, vec, sub;
	register int flag;
#	define NUM_VECTS (MAX_VME_INTRS+1)
	struct {
		int	sub;
		struct driver *driver;
		struct lboot_vme_intrs *intr;
	} vme_intrs[NUM_VECTS];


	for (dp = driver, flag=0; dp != 0; dp = dp->next) {
		if ((!(dp->flag & INEDT)) || (dp->flag & DYNLOAD))
			continue;

		for (ep=dp->edtp; ep; ep=ep->e_next)
			for (vp=ep->v_next; vp; vp=vp->v_next)
				fprintf(f, !(flag++)
					   ? "\nextern int %s()"
					   : ", %s()",
					vp->v_vname);
	}
	if (flag)
		fprintf(f, ";\n" );


	bzero((char*)&vme_intrs[0], sizeof(vme_intrs));
	sub = -1;
	fprintf(f, "struct vme_intrs vme_intrs[] = {");
	for (dp = driver; dp != 0; dp = dp->next) {
		if ((!(dp->flag & INEDT)) || (dp->flag & DYNLOAD))
			continue;


		for (ep=dp->edtp,i=0; ep; ep=ep->e_next,++i) {

			if (!(vp=ep->v_next))
				continue;

			if (++sub >= MAX_VME_INTRS)
				error(ER108);
			do {
				fprintf(f, "\n\t{ %-10s, %#4x, %d, %d, ",
					vp->v_vname,
					vp->v_vec, vp->v_brl, vp->v_unit);
				fprintf(f, "}," );

				/* notice if an interrupt vector is assigned
				 *	more than once
				 */
				vec = vp->v_vec;
				if (vec) {
					if (0 != vme_intrs[vec].driver) {
						error(ER107,
					         vme_intrs[vec].driver->mname,
					          vme_intrs[vec].intr->v_unit,
						    dp->mname,
						    vp->v_unit,
						    vec);
					}
					vme_intrs[vec].driver = dp;
					vme_intrs[vec].intr = vp;
					vme_intrs[vec].sub = sub;
				}

			} while (vp=vp->v_next);
		}

	}
	if (sub <= 0)
		fprintf(f, "\n\t{ 0 }");
	fprintf(f, "\n};\n");


	for (dp = driver, flag = 0; dp != 0; dp = dp->next) {
		if ((!(dp->flag & INEDT)) || (dp->flag & DYNLOAD))
			continue;

		for (ep=dp->edtp; ep; ep=ep->e_next) {
			if (ep->e_init)
				fprintf(f, (!(flag++)
					    ? "\nextern int %s()"
					    : ", %s()"),
					ep->e_init);
		}
	}
	if (flag)
		fprintf(f, ";\n" );


	fprintf(f, "\nstruct edt edt[] = {\n" );
	sub = -1;
	for (dp = driver; dp != 0; dp = dp->next) {
		if ((!(dp->flag & INEDT)) || (dp->flag & DYNLOAD))
			continue;

		for (ep=dp->edtp,i=0; ep; ep=ep->e_next,++i) {
			fprintf(f, "\t{ %2d, %3d, %d, %2d, %2d, ",
				ep->e_bus_type, ep->v_cpuintr,
				ep->v_setcpuintr,
				ep->e_adap, ep->e_ctlr);
			if (ep->v_next) {
				fprintf(f, "&vme_intrs[%2d], ",++sub);
			} else {
				fprintf( f, "0,              " );
			}
			fprintf(f, "%s",
				(ep->e_init!=0 ? ep->e_init : "0"));
	
			for (i=0; i < NBASE; i++)
				fprintf(f, ",%2d, %#10x, %#10x, (caddr_t)%#10llxl",
					ep->e_space[i].ios_type, 
					ep->e_space[i].ios_iopaddr,
					ep->e_space[i].ios_size,
					ep->e_space[i].ios_vaddr);
			fprintf(f, "},\n");
		}

	}

	fprintf(f, "};\n\nint nedt = sizeof (edt)/ sizeof (struct edt);\n" );
}


/*
 * mindlessly spew out generated structures
 */
static void
generate(register FILE *f)
{
	register struct driver *dp;
	register struct master *mp;
	register struct Bdevsw *bp;
	register struct Cdevsw *cp;
	register struct Fmodsw *fp;
	register struct Vfssw *vfsp;
	int i, j, devtotal, bextra, cextra;
	struct tune *t;

	next_init = io_init;
	if ((NULL != next_init) && (*next_init != NULL)) {
		fprintf( f, "\nextern void %s()", *next_init);
		while (*++next_init)
			fprintf( f, ", %s()", *next_init);
		fprintf( f, ";\n" );
	}
	fprintf( f, "void (*io_init[])() = {\n" );
	for (next_init = io_init; *next_init; next_init++)
		fprintf( f, "	%s,\n", *next_init);
	fprintf( f, "	0\n};\n" );

	next_start = io_start;
	if ((NULL != next_start) && (*next_start != NULL)) {
		fprintf( f, "\nextern void %s()", *next_start);
		while (*++next_start)
			fprintf( f, ", %s()", *next_start);
		fprintf( f, ";\n" );
	}
	fprintf( f, "void (*io_start[])() = {\n" );
	for (next_start = io_start; *next_start; next_start++)
		fprintf( f, "	%s,\n", *next_start);
	fprintf( f, "	0\n};\n" );

	next_reg = io_reg;
	if ((NULL != next_reg) && (*next_reg != NULL)) {
		fprintf( f, "\nextern void %s()", *next_reg);
		while (*++next_reg)
			fprintf( f, ", %s()", *next_reg);
		fprintf( f, ";\n" );
	}
	fprintf( f, "void (*io_reg[])() = {\n" );
	for (next_reg = io_reg; *next_reg; next_reg++)
		fprintf( f, "	%s,\n", *next_reg);
	fprintf( f, "	0\n};\n" );

	next_unreg = io_unreg;
	if ((NULL != next_unreg) && (*next_unreg != NULL)) {
		fprintf( f, "\nextern void %s()", *next_unreg);
		while (*++next_unreg)
			fprintf( f, ", %s()", *next_unreg);
		fprintf( f, ";\n" );
	}
	fprintf( f, "void (*io_unreg[])() = {\n" );
	for (next_unreg = io_unreg; *next_unreg; next_unreg++)
		fprintf( f, "	%s,\n", *next_unreg);
	fprintf( f, "	0\n};\n" );

	next_halt = io_halt;
	if ((NULL != next_halt) && (*next_halt != NULL)) {
		fprintf( f, "\nextern void %s()", *next_halt);
		while (*++next_halt)
			fprintf( f, ", %s()", *next_halt);
		fprintf( f, ";\n" );
	}
	fprintf( f, "void (*io_halt[])() = {\n" );
	for (next_halt = io_halt; *next_halt; next_halt++)
		fprintf( f, "	%s,\n", *next_halt);
	fprintf( f, "	0\n};\n" );

	fprintf( f, "\nshort MAJOR[%d] = { ",  NMAJORS);
	for (i=0; i< NMAJORS; i++) {
		if (!(i % 16))
			fprintf( f, "\n\t");
		fprintf( f, "%3d," , Major[i]);
	}
	/* { */ fprintf( f, " };\n\n");

	/*
	 * Output block devices
	 */
	for (i=0, bp=bdevswp; i<bdevcnt; i++, bp++) 
		if (strcmp(bp->d_flags, rtname[RNODEVFLAG].name) == 0) {
			error(ER121, bp->d_prefix);
			/* NOTREACHED */
		}
	for (i=0, bp=bdevswp; i<bdevcnt; i++, bp++) 
		fprintf(f, "extern int %s;\n", bp->d_flags);

	fprintf( f, "\nextern nodev()" );
	j = 16;
	for (i=0, bp=bdevswp; i<bdevcnt; i++, bp++) {
		j = dcl_fnc(f,j, bp->d_open);
		j = dcl_fnc(f,j, bp->d_close);
		j = dcl_fnc(f,j, bp->d_strategy);
		j = dcl_fnc(f,j, bp->d_print);
		j = dcl_fnc(f,j, bp->d_map);
		j = dcl_fnc(f,j, bp->d_unmap);
		j = dcl_fnc(f,j, bp->d_dump);
		j = dcl_fnc(f,j, bp->d_size);
		j = dcl_fnc(f,j, bp->d_size64);
	}
	fprintf( f, ", nulldev();\n" );

	bextra = (t = tunefind("bdevsw_extra")) ? t->svalue : 0;
	cextra = (t = tunefind("cdevsw_extra")) ? t->svalue : 0;

	/* if adding entries to both switches, then be sure bdevsw is long
	 * enough for devices to have the same index into both switches
	 */
	if (bextra && cextra) {
	    devtotal = max(bextra+bdevcnt, cextra+cdevcnt);
	    bextra = devtotal - bdevcnt;
	    cextra = devtotal - cdevcnt;
	}

	fprintf( f, "\nstruct bdevsw bdevsw[] = {\n");
	for (i=0, bp=bdevswp; i<bdevcnt; i++, bp++) {
		fprintf(f, "\t{ ");
		fprintf(f, "&%s," , bp->d_flags);
		fprintf( f, "%d, ", bp->d_cpulock);
		fprintf(f, "%s, %s, %s, %s,\n" ,
			bp->d_open, bp->d_close, bp->d_strategy, bp->d_print);
		fprintf(f, "\t    %s, %s, %s, %s, %s, 0, 0},\n",
			bp->d_map, bp->d_unmap, bp->d_dump, bp->d_size,
			bp->d_size64);
	}
	if (bextra)
	    for (i = 0; i < bextra; i++)
		fprintf(f, "\t{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},\n");
	/* { */ fprintf(f, "};\n" );
	fprintf(f, "\nint bdevcnt = %d;\n", bdevcnt);
	fprintf(f, "int bdevmax = %d;\n\n", bextra + bdevcnt);

	/* 
	 * Make a second pass to create prefixbdevsw entries for
	 * hardware graph-aware drivers.
	 */
	for (i=0, bp=bdevswp; i<bdevcnt; i++, bp++) {
		if (bp->d_bdevsw == NULL)
			continue;

		fprintf(f, "struct bdevsw %s_s = \n\t{", bp->d_bdevsw);
		fprintf(f, "&%s," , bp->d_flags);
		fprintf( f, "%d, ", bp->d_cpulock);
		fprintf(f, "%s, %s, %s, %s,\n" ,
			bp->d_open, bp->d_close, bp->d_strategy, bp->d_print);
		fprintf(f, "\t    %s, %s, %s, %s, %s, 0, 0};\n",
			bp->d_map, bp->d_unmap, bp->d_dump, bp->d_size,
			bp->d_size64);

		fprintf(f, "struct bdevsw *%s = &%s_s;\n\n",
					bp->d_bdevsw, bp->d_bdevsw);
	}


	for (i=0, cp=cdevswp; i<cdevcnt; i++, cp++) 
		if (strcmp(cp->d_flags, rtname[RNODEVFLAG].name) == 0) {
			error(ER121, cp->d_prefix);
			/* NOTREACHED */
		}
	for (i=0, cp=cdevswp; i<cdevcnt; i++, cp++) 
		fprintf( f, "extern int %s;\n", cp->d_flags);

	fprintf( f, "\nextern nodev()" );
	j = 16;
	for (i=0, cp=cdevswp; i<cdevcnt; i++, cp++) {
		j = dcl_fnc(f,j, cp->d_open);
		j = dcl_fnc(f,j, cp->d_close);
		j = dcl_fnc(f,j, cp->d_read);
		j = dcl_fnc(f,j, cp->d_write);
		j = dcl_fnc(f,j, cp->d_ioctl);
		j = dcl_fnc(f,j, cp->d_mmap);
		j = dcl_fnc(f,j, cp->d_map);
		j = dcl_fnc(f,j, cp->d_unmap);
		j = dcl_fnc(f,j, cp->d_poll);
		j = dcl_fnc(f,j, cp->d_attach);
		j = dcl_fnc(f,j, cp->d_detach);
		j = dcl_fnc(f,j, cp->d_enable);
		j = dcl_fnc(f,j, cp->d_disable);
	}
	fprintf( f, ", nulldev();\n" );
	for (i=0, cp=cdevswp; i<cdevcnt; i++, cp++) {
		if (strcmp("0", cp->d_ttys))
			fprintf( f, "extern struct tty %s;\n", cp->d_ttys);
		if (strcmp("0", cp->d_str))
			fprintf( f, "extern struct streamtab %s;\n", cp->d_str);
	}

	fprintf( f, "\n" );

	fprintf( f, "\nstruct cdevsw cdevsw[] = {\n"); /* } */
	for (i=0, cp=cdevswp; i<cdevcnt; i++, cp++) {
		fprintf(f, "\t{ ");
		fprintf( f, "&%s,", cp->d_flags);
		fprintf( f, "%d, ", cp->d_cpulock);
		fprintf( f, "%s, %s, %s, %s,\n" ,
			cp->d_open, cp->d_close, cp->d_read, cp->d_write);
		fprintf( f, "\t    %s, %s, %s, %s, %s,\n",
		cp->d_ioctl, cp->d_mmap, cp->d_map, cp->d_unmap, cp->d_poll);

		fprintf( f, "\t    %s, %s, %s, %s, ",
		cp->d_attach, cp->d_detach, cp->d_enable, cp->d_disable);

		if (strcmp(cp->d_ttys, "0"))
			fprintf( f, "&%s, ", cp->d_ttys);
		else
			fprintf( f, "0, ");
		if (strcmp(cp->d_str, "0"))
			fprintf( f, "&%s, 0, 0},\n", cp->d_str);
		else
			fprintf( f, "0, 0, 0 },\n");
	}
	if (cextra)
	    for (i = 0; i < cextra; i++)
		    fprintf(f, "\t{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},\n");
	/* { */ fprintf( f, "};\n" );
	fprintf( f, "\nint cdevcnt = %d;\n", cdevcnt);
	fprintf( f, "int cdevmax = %d;\n\n", cextra + cdevcnt);

	/*
	 * Make a second pass to create prefixcdevsw entries for
	 * hardware graph-aware drivers.
	 */
	for (i=0, cp=cdevswp; i<cdevcnt; i++, cp++) {
		if (cp->d_cdevsw == NULL)
			continue;

		fprintf(f, "struct cdevsw %s_s = \n\t{", cp->d_cdevsw);
		fprintf( f, "&%s,", cp->d_flags);
		fprintf( f, "%d, ", cp->d_cpulock);
		fprintf( f, "%s, %s, %s, %s,\n" ,
			cp->d_open, cp->d_close, cp->d_read, cp->d_write);

		fprintf( f, "\t    %s, %s, %s, %s, %s,\n",
		cp->d_ioctl, cp->d_mmap, cp->d_map, cp->d_unmap, cp->d_poll);

		fprintf( f, "\t    %s, %s, %s, %s, ",
		cp->d_attach, cp->d_detach, cp->d_enable, cp->d_disable);

		if (strcmp(cp->d_ttys, "0"))
			fprintf( f, "&%s, ", cp->d_ttys);
		else
			fprintf( f, "0, ");
		if (strcmp(cp->d_str, "0"))
			fprintf( f, "&%s, 0, 0};\n", cp->d_str);
		else
			fprintf( f, "0, 0, 0 };\n");

		fprintf( f, "struct cdevsw *%s = &%s_s;\n\n", 
					cp->d_cdevsw, cp->d_cdevsw);
	}

	/*
	 * Create a table that associates prefix names for each driver with
	 * the appropriate cdevsw/bdevsw descriptors.
	 */
	fprintf(f, "struct static_device_driver_desc_s static_device_driver_table[] = {");
	for (i=0, bp=bdevswp; i<bdevcnt; i++, bp++) {
		if (bp->d_bdevsw == NULL)
			continue;
		if (bp->d_prefix == NULL)
			continue;
		fprintf(f, "\n\t{\"%s\", &%s_s, NULL},",
			bp->d_prefix, bp->d_bdevsw);
	}
	for (i=0, cp=cdevswp; i<cdevcnt; i++, cp++) {
		if (cp->d_cdevsw == NULL)
			continue;
		if (cp->d_prefix == NULL)
			continue;
		fprintf(f, "\n\t{\"%s\", NULL, &%s_s},",
			cp->d_prefix, cp->d_cdevsw);
	}
	fprintf(f, "\n};\n");
	fprintf(f, "int static_devsw_count = sizeof(static_device_driver_table)/sizeof(struct static_device_driver_desc_s);\n\n");

	for (i=0, fp=fmodswp; i<fmodcnt; i++, fp++)
		if (strcmp(fp->f_flags, rtname[RNODEVFLAG].name) == 0) {
			error(ER121, fp->f_name);
			/* NOTREACHED */
		}
	for (i=0, fp=fmodswp; i<fmodcnt; i++, fp++) {
		if (strcmp("0", fp->f_strmtab))
			fprintf(f, "extern struct streamtab %s;\n",
				fp->f_strmtab);
	}

	for (i=0, fp=fmodswp; i<fmodcnt; i++, fp++) 
		fprintf( f, "extern int %s;\n", fp->f_flags);

	fprintf( f, "\nstruct fmodsw fmodsw[] = {\n"); /* } */
	for (i=0, fp=fmodswp; i<fmodcnt; i++, fp++) {
		char tmpstr[FMNAMESZ+1];

		if (strlen(fp->f_name) > FMNAMESZ) {
			strncpy (tmpstr, fp->f_name, FMNAMESZ);
			tmpstr[FMNAMESZ] = 0;
			fprintf( f, "\t{ \"%s\", &%s, ", 
				tmpstr, fp->f_strmtab ); /* } */
		} else
			fprintf( f, "\t{ \"%s\", &%s, ", 
				fp->f_name, fp->f_strmtab ); /* } */
		fprintf( f, "&%s,", fp->f_flags);
		/* { */ fprintf( f, "},\n");
	}
	devtotal = (t = tunefind("fmodsw_extra")) ? t->svalue : 0;
	for (; i<fmodcnt + devtotal; i++)
		fprintf(f, "\t{ \"\", 0, 0},\n");
	/* { */ fprintf( f, "};\n" );
	fprintf( f, "\nint fmodcnt = %d;\n" , fmodcnt);
	fprintf( f, "int fmodmax = %d;\n\n" , fmodcnt + devtotal);

	/* vfssw table */
	for (i=0, vfsp=vfsswp; i<nfstype; i++, vfsp++) {
		if (vfsp->vfs_init && strlen(vfsp->vfs_init))
			fprintf(f, "extern int %s();\n", vfsp->vfs_init);
		if (vfsp->vfs_vfsops && strlen(vfsp->vfs_vfsops))
			fprintf(f, "extern struct vfsops %s;\n",
				vfsp->vfs_vfsops);
		if (vfsp->vfs_vnodeops && strlen(vfsp->vfs_vnodeops))
			fprintf(f, "extern struct vnodeops %s;\n",
				vfsp->vfs_vnodeops);
	}
	fprintf( f, "\nstruct vfssw vfssw[] = {\n" ); /* } */
	for (i=0, vfsp=vfsswp; i<nfstype; i++, vfsp++) {
		if (vfsp->vfs_name && strlen(vfsp->vfs_name))
			fprintf(f, "\t{ \"%s\", ", vfsp->vfs_name);
		else
			fprintf(f, "\t{ \"BADFS\", ");
		if (vfsp->vfs_init && strlen(vfsp->vfs_init))
			fprintf(f, "%s, ", vfsp->vfs_init);
		else
			fprintf(f, "0, ");
		if (vfsp->vfs_vfsops && strlen(vfsp->vfs_vfsops))
			fprintf(f, "&%s, ", vfsp->vfs_vfsops);
		else
			fprintf(f, "0, ");
		if (vfsp->vfs_vnodeops && strlen(vfsp->vfs_vnodeops))
			fprintf(f, "&%s, ", vfsp->vfs_vnodeops);
		else
			fprintf(f, "0, ");
		fprintf(f, "&%s, 0 },\n", vfsswp[0].vfs_vnodeops);
	}
	devtotal = (t = tunefind("vfssw_extra")) ? t->svalue : 0;
	for (; i<nfstype + devtotal; i++)
		fprintf(f, "\t{ \"\", 0, 0, 0, 0, 0},\n");
	fprintf( f, "};\n" );
	fprintf(f, "int nfstype = %d;\n" , nfstype);
	fprintf( f, "int vfsmax = %d;\n\n" , nfstype + devtotal);


	fprintf( f, "int noprobe = %d;\n",noprobe);

	fprintf( f, "cpuid_t nointr_list[] = {\n");
	for (i=0; i<nointr_count;) {
		for (j=0; (j<16) && (i<nointr_count); i++, j++)
			fprintf( f, "%d, ", nointr_list[i]);
		fprintf( f, "\n");
	}
	fprintf( f, "CPU_NONE };\n\n");

	fprintf( f, "/* system file version %d */\n\n", system_version);
	fprintf( f, "\ndev_t rootdev = 0x%x;\n", rootdev);
	if (bootrootfile)
		fprintf(f, "char *bootrootfile = \"%s\";\n\n", bootrootfile);
	else
		fprintf(f, "char *bootrootfile = NULL;\n\n");

	fprintf( f, "\ndev_t dumpdev = 0x%x;\n", dumpdev);
	if (bootdumpfile)
		fprintf(f, "char *bootdumpfile = \"%s\";\n\n", bootdumpfile);
	else
		fprintf(f, "char *bootdumpfile = NULL;\n\n");

	fprintf( f, "dev_t swapdev = 0x%x;\n", swapdev);
	if (bootswapfile)
		fprintf(f, "char *bootswapfile = \"%s\";\n", bootswapfile);
	else
		fprintf(f, "char *bootswapfile = NULL;\n");
	fprintf( f, "daddr_t swplo = %ld;\n", swplo);
	fprintf( f, "int nswap = %d;\n\n", nswap);

	/* Dump ibus adapter table */
	if (ibus_numadaps > 0) {
		fprintf(f, "\nint ibus_numadaps = %d;\n", ibus_numadaps);
		fprintf(f, "ibus_t ibus_adapters[] = {\n");

		for (i = 0; i < ibus_numadaps; i++) {
			ibus_t *ib = &ibus_adapters[i];
			fprintf(f, "\t{ %d, %d,    %d, %d,   %d },\n",
				ib->ibus_module, ib->ibus_unit,
				ib->ibus_adap, ib->ibus_ctlr, ib->ibus_proc);
		}
		fprintf(f, "\t{ 0, 0,   0, 0,   0}\n};\n\n");
	}

	/* map interrupt req. level to cpu */
	fprintf( f, "char cpuipl[] = {");
	for (i=0; i<MAXIPL-1; i++)
		fprintf( f, "%d, ",cpuipl[i]);
	fprintf( f, "%d };\n",cpuipl[i]);

	edt_gen(f);			/* generate EDT structures */

	fprintf( f, "\n/* stubs */\n" );
	dp = driver;
	do {
		if (dp->flag & LOAD)
			continue;

		mp = dp->opthdr;
		if (mp->nrtn) {
			struct routine *rp;

			rp = (struct routine *) POINTER(mp->o_routine, mp);
			for (i=0; i<mp->nrtn; ++i, ++rp)
				if (rp->name) {
					if (rp->id == RNOTHING ||
							rp->id == RNOREACH)
					    fprintf( f, "void %s() %s\n",
						POINTER(rp->name,mp),
						rtname[rp->id].routine);
					else
					    fprintf( f, "%s() %s\n",
						POINTER(rp->name,mp),
						rtname[rp->id].routine);
				}
		}
	} while (dp=dp->next);
}

#ifdef NEVER
static void
devflags_print(FILE *f, uint flags)
{
	register int i;
	register int gotone;

	gotone = 0;
	if (!(flags & DLOCK_MASK)) {
		/* default to processor locked */
		fprintf(f, "D_PROCESSOR");
		gotone++;
	}
	for (i = 0; i < sizeof(flags) * 8; i++) {
		if (flags & (1 << i)) {
			if (gotone++)
				fprintf(f, "|");
			switch (1 << i) {
			case D_MP:
				fprintf(f, "D_MP");
				break;
			case D_PROCESSOR:
				fprintf(f, "D_PROCESSOR");
				break;
#ifdef DLOCK_NETWORK
			case DLOCK_NETWORK:
				fprintf(f, "DLOCK_NETWORK");
				break;
#endif /* DLOCK_NETWORK */
#ifdef D_WBACK
			case D_WBACK:
				fprintf(f, "D_WBACK");
				break;
#endif /* D_WBACK */
			default:
				/* Unknown flag.  Not good, but don't
				 * break anything.
				 */
				fprintf(f, "0");
				break;
			}
		}
	}
	fprintf(f, ", ");
}
#endif

/*
 * get_ar_fmt()
 *
 * This routine attempts to determine the underlying
 * object file format of an archive library by checking
 * the magic # of the first .o member in the archive.
 */
static int
get_ar_fmt(int fd)
{
	__int64_t offset = SARMAG;
	struct ar_hdr arhdr;
	int rc = UNKNOWN_FMT;
	Elf32_Ehdr elfhdr;
	
	/* seek to start of first arhdr */
	(void) lseek(fd, offset, 0);

	/* scan member entries until we get an object file */
	for ( ; ; ) {
		/* read next arhdr -- truncate indicates ??? */
		if (read(fd, (void *)&arhdr, sizeof (arhdr)) != sizeof (arhdr))
			break;

		(void) sscanf(arhdr.ar_size, "%lld", &offset);
		
		/* skip optional symtab/strtab member */
		if (IS_ELF_AR_SYMTAB(arhdr.ar_name) ||
		    IS_ELF_AR64_SYMTAB(arhdr.ar_name) ||
		    IS_ELF_AR_STRTAB(arhdr.ar_name)) {
			(void) lseek(fd, offset, 1);
			continue;
		}

		/* we've got what should be our first real object file */
		if (read(fd, (void *)&elfhdr, sizeof (elfhdr)) != sizeof (elfhdr))
			break;

		/* ELF or COFF? */
		if (IS_ELF(elfhdr)) {
			/* Determine ABI of kernel.o */
			if (elfhdr.e_ident[EI_CLASS] == ELFCLASS64)
				rc = ELF64_FMT;
			else if (elfhdr.e_ident[EI_CLASS] == ELFCLASS32) {
				if (elfhdr.e_flags & EF_MIPS_ABI2)
					rc = ELFN32_FMT;
				else
					rc = ELFO32_FMT;
			}
			break;
		} else {
			rc = COFF_FMT;
			break;
		}
	}

	return (rc);
}

/*
 * driver_matches_kernel()
 *
 * Determines object format/ABI of driver and checks it against kernel.
 * If incompatible, generates an error and returns FALSE.  
 * Otherwise, returns TRUE.
 */
static int
driver_matches_kernel(struct driver *dp)
{
	int fd;
	union commonhdr {
		FILHDR fhdr;
		Elf32_Ehdr elfhdr;
		char armag[SARMAG];
	} commonhdr;
	
	if (!dp->dname)
		return FALSE;		
		 
	if ((fd = open(dp->dname, O_RDONLY)) == -1) {
		/* <name>: perror() message */
		error(ER7, dp->dname);
		return FALSE;
	}
 	
	read_and_check(fd, dp->dname, (char *)&commonhdr,
	               sizeof (commonhdr));

	if (!IS_ELF(commonhdr.elfhdr) &&
        !IS_MIPSEBMAGIC(commonhdr.fhdr.f_magic) &&
	    !IS_MIPSELMAGIC(commonhdr.fhdr.f_magic) &&
	    !IS_SMIPSEBMAGIC(commonhdr.fhdr.f_magic) &&
	    !IS_SMIPSELMAGIC(commonhdr.fhdr.f_magic) &&
	    (memcmp(&commonhdr.armag, ARMAG, SARMAG))) {
		/* Driver <name>: not a valid object file */
		error(ER15, dp->dname);
		close (fd);
		free(dp->dname);
		dp->dname = NULL;
		dp->magic = UNKNOWN_FMT;
		dp->flag &= ~LOAD;
		return FALSE;
	}                

	if (IS_ELF(commonhdr.elfhdr)) {
		if (commonhdr.elfhdr.e_ident[EI_CLASS] == ELFCLASS64)
			dp->magic = ELF64_FMT;
		else if (commonhdr.elfhdr.e_ident[EI_CLASS] == ELFCLASS32) {
			if (commonhdr.elfhdr.e_flags & EF_MIPS_ABI2)
				dp->magic = ELFN32_FMT;
			else
				dp->magic = ELFO32_FMT;
		}
	}
	else if (memcmp(&commonhdr.armag, ARMAG, SARMAG) == 0)
		dp->magic = get_ar_fmt(fd);
	else
		dp->magic = COFF_FMT;				
					
	close(fd);
			
	/*
	 * Magic number test.  The object format of dp must
	 * match that of kernel.
	 */
	if (dp->magic != kernel.magic) {
		/* Driver <name>: <driver_format> object format is incompatible with <kernel_format> kernel; object ignored */
		error(ER16, dp->dname, FORMAT_NAME(dp->magic), FORMAT_NAME(kernel.magic));
		syslog (LOG_WARNING,
		        "WARNING %s: %s object format is incompatible with %s kernel; object ignored\n",
	   	        dp->dname, FORMAT_NAME(dp->magic), FORMAT_NAME(kernel.magic));
		free(dp->dname);
		dp->dname = NULL;
		dp->flag &= ~LOAD;
		return FALSE;
	}
	
	return TRUE;
}
