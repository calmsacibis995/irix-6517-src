
#include <getopt.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <syslog.h>
#include <malloc.h>
#include <stdlib.h>
#include <bstring.h>

#include <sys/types.h>
#include <sys/fcntl.h>

#include <sys/syssgi.h>
#include <sys/param.h>
#include <sys/conf.h>
#include <sys/fstyp.h>
#include <sys/mload.h>

/*
 * ml - module loader
 */

/* 
 * load, unload, register, unregister
 * [ld, unld, reg, unreg]
 */

static int mlist(int, char **);
static int mload(int, char **);
static int munload(int, char **);
static int mreg(int, char **);
static int munreg(int, char **);
static int mdebug(int, char **);

static void musage(void);
static void mlistusage(void);
static void mldregusage(void);
static void mldusage(void);
static void mregusage(void);
static void munldregusage(void);
static void mdebugusage(void);

struct acttab {
    char *name;
    int nlen;
    int (*func)(int, char **);
    void (*usage)(void);
} act[] = {
    {"list", 4, mlist, mlistusage},
    {"ld", 2, mload, mldusage},
    {"unld", 4, munload, munldregusage},
    {"load", 4, mload, 0},
    {"unload", 6, munload, 0},
    {"reg", 3, mreg, mregusage},
    {"unreg", 5, munreg, munldregusage},
    {"debug", 5, mdebug, mdebugusage},
};

static int loadmodule(int, int, char *, char *, void *, int, int);
static int unloadmodule(int, long);
static void prmodule(mod_info_t *);
static char *ml_err(int);
#ifdef DEBUG
static int prinfo(int, char *, char *, int);
#endif
#define DPRINTF printf

char *cmdname, *actname;
#define uprintf(x) fprintf(stderr, "usage: %s %s %s\n", cmdname, actname, x);

int majors[DRV_MAXMAJORS];
int nmajors = 0;

int
main(int argc, char **argv)
{
    int i, rv;

    if (syssgi (SGI_MCONFIG, CF_LIST, 0) < 0) {
	fprintf (stderr, "Module loader not enabled.\n");
	exit (-1);
    }

    if (argc == 1) {
	mlist(--argc, ++argv);
	exit(0);
    }

    opterr = 0;
    cmdname = *argv;

    --argc; 
    ++argv;

    for (i = 0; i < sizeof(act)/sizeof(struct acttab); i++)
	if (!strncmp (*argv, act[i].name, act[i].nlen)) {
	    actname = act[i].name;
	    rv = (*act[i].func)(argc, argv);
	    exit(rv);
	}

    musage();
    return -1;
}

void
musage(void)
{
    int i;

    for (i = 0; i < sizeof(act)/sizeof(struct acttab); i++) {
	actname = act[i].name;
	if (*act[i].usage)
	    (*act[i].usage)();
    }
}

/************************************************************************
 *
 * debug
 *
 */

static void
mdebugusage(void)
{
    uprintf ("[-s|v|n]");
}

int
mdebug(int argc, char **argv)
{
    int i, num = 0;

    if (getuid() != 0 && geteuid() != 0) {
	    fprintf (stderr, "Sorry, must be superuser to run %s %s.\n",
		cmdname, actname);
	    return -1;
    }

    if (argc != 2) {
	mdebugusage(); 
	return -1;
    } else while ((i = getopt(argc, argv, "svn")) != GETOPTDONE) {
	switch (i) {
	case 's':  num = 0; break;	/* silent */
	case 'v':  num = 1; break;	/* verbose */
	case 'n':  num = 0xdead; break;	/* no loading allowed */

	case '?':  
	default:   mlistusage(); return -1;
	}
    }

    syssgi (SGI_MCONFIG, CF_DEBUG, num);
 
    return 0;
}

/************************************************************************
 *
 * list
 *
 */

static void
mlistusage(void)
{
    uprintf ("[-r|l|b]");
}

int
mlist(int argc, char **argv)
{
    int size, i, modcnt;
    mod_info_t *mod, *modspc;
    int reg = 0, load = 0, both = 0;

    if (!argc || argc == 1)
	reg = load = both = 1;
    else while ((i = getopt(argc, argv, "rlb")) != GETOPTDONE) {
	switch (i) {
	case 'r':  reg = 1; break;
	case 'l':  load = 1; break;
	case 'b':  both = 1; break;

	case '?':  mlistusage(); return -1;
	}
    }

    if (!(size = (int)syssgi (SGI_MCONFIG, CF_LIST, 0))) {
	printf ("No modules loaded or registered.\n");
	return -1;
    }

    modspc = (mod_info_t *)malloc(size);
    if (!modspc) {
	printf ("No memory for module list.\n");
	return -1;
    }

    if (syssgi (SGI_MCONFIG, CF_LIST, modspc, size) != size) {
	printf ("Error copying module list.\n");
	return -1;
    }

    if (modspc->m_cfg_version != M_CFG_CURRENT_VERSION) {
	printf ("Version mismatch with kernel\n");
	return -1;
    }

    if (load) {
	printf ("\nLoaded Modules:\n");
	modcnt = 0;
	for (mod = modspc, i = 0; i < size/sizeof(mod_info_t); ++i, ++mod) {
	    if ((mod->m_flags & (M_REGISTERED|M_LOADED)) != M_LOADED)
		continue;

	    modcnt++;
	    prmodule(mod);
	}
	if (!modcnt)
	    printf ("None.\n");
	printf ("\n");
    }

    if (reg) {
	printf ("\nRegistered Modules:\n");
	modcnt = 0;
	for (mod = modspc, i = 0; i < size/sizeof(mod_info_t); ++i, ++mod) {
	    if ((mod->m_flags & (M_REGISTERED|M_LOADED)) != M_REGISTERED)
		continue;

	    modcnt++;
	    prmodule(mod);
	}
	if (!modcnt)
	    printf ("None.\n");
	printf ("\n");
    }

    if (both) {
	printf ("\nRegistered and Currently Loaded Modules:\n");
	modcnt = 0;
	for (mod = modspc, i = 0; i < size/sizeof(mod_info_t); ++i, ++mod) {
	    if ((mod->m_flags & (M_REGISTERED|M_LOADED)) 
		    != (M_REGISTERED|M_LOADED))
		continue;
	    
	    modcnt++;
	    prmodule(mod);
	}
	if (!modcnt)
	    printf ("None.\n");
    }

    return 0;
}

/************************************************************************
 *
 * load/register
 *
 */

static void
mldusage(void)
{
    uprintf ("[-v] [-d] -[c|b|B|f] modulename -p prefix [-s majornumber majornumber ...] -t autounload_delay");
    uprintf ("[-v] [-d] -m modulename -p prefix [-a streams_module_name] -t autounload_delay");
    uprintf ("[-v] [-d] -j modulename -p prefix [-a file_system_name]");
    uprintf ("[-v] [-d] -l libname");
    uprintf ("[-v]  -r rtsymtabname [-u] [-t autounload_delay]");
    uprintf ("[-v] [-d] -i idbgname -p prefix");
}

static void
mregusage(void)
{
    uprintf ("[-v] [-d] -[c|b|B|f] modulename -p prefix [-s majornumber majornumber ...] -t autounload_delay");
    uprintf ("[-v] [-d] -m modulename -p prefix [-a streams_module_name] -t autounload_delay");
}

static void
mldregusage(void)
{
	mldusage(); mregusage();
}

static int 
mloadreg(int command, int argc, char **argv)
{
    int c;
    char *fname = 0, *prefix = 0, *fmname = 0;
    int chardev=0, blockdev=0, fundrv=0, funmod=0, fsys=0, enet=0, other=0, bothdev=0;
    int timeout=M_UNLDDEFAULT;
    int idbgmod=0;
    int libmod=0;
    int symtabmod=0;
    int devflag, type, id;
    int verbose = 0;
    int symdebug = 0;
    int nmajorany = 0;
    int unixsymtab = 0;
    int i;

    mod_streams_t str;
    mod_driver_t drv;
    mod_idbg_t idbg;
    mod_lib_t lib;
    mod_symtab_t symtab;
    mod_fsys_t fs;
    mod_enet_t e;
    void *mdata;

    if (getuid() != 0 && geteuid() != 0) {
	    fprintf (stderr, "Sorry, must be superuser to run %s %s.\n",
		cmdname, actname);

	    return -1;
    }

    while ((c = getopt(argc, argv, "uvdp:s:n:a:c:b:B:f:m:j:e:o:i:l:t:r:")) != GETOPTDONE) {
	switch (c) {
	case 'c':  chardev = 1; fname = optarg; break;
	case 'b':  bothdev = 1; fname = optarg; break;
	case 'B':  blockdev = 1; fname = optarg; break;
	case 'f':  fundrv = 1; fname = optarg; break;
	case 'm':  funmod = 1; fname = optarg; break;
	case 'i':  idbgmod = 1; fname = optarg; break;
	case 'l':  libmod = 1; fname = optarg; break;
	case 'r':  symtabmod = 1; fname = optarg; break;
	case 'j':  fsys = 1; fname = optarg; break;
	case 'e':  enet = 1; fname = optarg; break;
	case 'o':  other = 1; break;
	case 'v':  verbose = 1; break;
	case 'd':  symdebug = 1; break;
	case 'p':  prefix = optarg; break;
	case 'u':  unixsymtab = 1; break;
	case 's':  
		for (;;) {
			majors[nmajors++] = (int)atol(optarg);
			if (nmajors > DRV_MAXMAJORS) {
				fprintf (stderr,
					"Too many major numbers specified.\n");
				return -1;
			}
			if (optind >= argc || *argv[optind] == '-')
				break;
			optarg = argv[optind++];
		}
		break;
	case 'n':  
		nmajorany = (int)atol(optarg);
		if ((nmajorany + nmajors) > DRV_MAXMAJORS) {
			fprintf (stderr, "Too many major numbers specified.\n");
			return -1;
		}
		for (i=0; i < nmajorany; i++) {
			majors[nmajors++] = MAJOR_ANY;
		}
		break;
	case 'a':  fmname = optarg; break;
	case 't':  timeout = (int)atol(optarg); break;

	case '?':  mldregusage(); return -1;
	}
    }

    if (chardev + bothdev + blockdev + fundrv + funmod + fsys + 
	    idbgmod + libmod + symtabmod + enet + other != 1) {
	mldregusage();
	return -1;
    }
    if (idbgmod && !prefix) {
	mldregusage();
	return -1;
    }
    if (libmod && !prefix)
	prefix = "lib";
    if (symtabmod && !prefix)
	prefix = "symtab";
    if (!fname || !prefix) {
	mldregusage();
	return -1;
    }
    if (libmod && !fname) {
	mldregusage();
	return -1;
    }

    type = M_DRIVER;
    if (chardev)
	devflag = MDRV_CHAR;
    else if (blockdev)
	devflag = MDRV_BLOCK;
    else if (bothdev)
	devflag = MDRV_CHAR|MDRV_BLOCK;
    else if (fundrv)
	devflag = MDRV_STREAM;
    else if (funmod)
	type = M_STREAMS;
    else if (fsys)
	type = M_FILESYS;
    else if (enet)
	type = M_ENET;
    else if (idbgmod)
	type = M_IDBG;
    else if (libmod)
	type = M_LIB;
    else if (symtabmod)
	type = M_SYMTAB;
    else if (other)
	type = M_OTHER;

    switch (type) {
    case M_DRIVER:
	drv.d_edtp = 0;		/*XXXkevinm*/
	drv.d_cpulock = 0;	/*XXXkevinm*/
	drv.d_type = devflag;

	for (i=0; i<nmajors; i++)
	    drv.d_majors[i] = majors[i];
	drv.d_nmajors = nmajors;
	mdata = &drv;
	break;

    case M_STREAMS:
	str.s_name[0] = 0;
	if (!fmname || !fmname[0]) {
	    int i;
	    char *base;
	    /* Get the fmodsw name from the file name */
	    if ((base = strrchr(fname, '/')) == NULL)
		base = fname;
	    else
		base++;
	    for (i = 0; i < FMNAMESZ + 1; ++i) {
		str.s_name[i] = base[i];
		if (!strncmp (&base[i],".o", 2))
		    str.s_name[i] = '\0';

		if (str.s_name[i] == '\0')
		    break;
	    }
	} else {
	    strncpy (str.s_name, fmname, FMNAMESZ+1);
	}
	if (str.s_name[0] == 0) {
	    mldregusage();
	    return -1;
	}
	mdata = &str;
	break;

    case M_IDBG:
	idbg.i_value = 0;
	mdata = &idbg;
	break;

    case M_LIB:
	lib.id = 0;
	mdata = &lib;
	break;

    case M_SYMTAB:
	symtab.id = 0;
	mdata = &symtab;
	strcpy (symtab.symtab_name, prefix);
	if (unixsymtab || strstr(fname, "unix"))
		symtab.type = M_UNIXSYMTAB;
	else
		symtab.type = 0;
	break;

    case M_FILESYS:
	fs.vsw_name[0] = 0;
	if (!fmname || !fmname[0]) {
	    int i;
	    char *base;
	    /* Get the vfssw name from the file name */
	    if ((base = strrchr(fname, '/')) == NULL)
		base = fname;
	    else
		base++;
	    for (i = 0; i < FSTYPSZ + 1; ++i) {
		fs.vsw_name[i] = base[i];
		if (!strncmp (&base[i],".o", 2))
		    fs.vsw_name[i] = '\0';

		if (fs.vsw_name[i] == '\0')
		    break;
	    }
	} else {
	    strncpy (fs.vsw_name, fmname, FSTYPSZ+1);
	}
	if (fs.vsw_name[0] == 0) {
	    mldregusage();
	    return -1;
	}
	mdata = &fs;
	break;

    case M_ENET:
	e.id = 0;
	e.e_edtp = 0;	/* XXX */
	mdata = &e;
	break;

    case M_OTHER:
	/*XXXkevinm*/
	fprintf (stderr, "Other module type not allowed yet.\n"); return -1;
    }

    /* Change relative pathnames to absolute
     */
    if (*fname != '/') {
	char *cwd;

	if ((cwd = getcwd((char *)NULL, MAXPATHLEN)) == NULL) {
	    fprintf (stderr, "Pathname not found.\n");
	    return -1;
	}
	if (strlen(cwd) + strlen(fname) + 2 > MAXPATHLEN) {
	    fprintf (stderr, "Pathname is too long.\n");
	    return -1;
	}
	sprintf(cwd + strlen(cwd), "/%s", fname);

	fname = cwd;
    }

#ifdef DEBUG
    prinfo(type, fname, prefix, devflag);
#endif

    if ((id = loadmodule (command, type, fname, prefix, mdata, timeout, symdebug)) < 0) {
	fprintf (stderr, "Error %s module %s:  %s.\n", 
		command == CF_LOAD ? "loading" : "registering", prefix,
		ml_err(errno));
	return -1;
    } else {
	openlog("lboot", LOG_ODELAY, LOG_AUTH);
	syslog (LOG_NOTICE, "Module %s dynamically %s.\n",
		fname,
		((command == CF_LOAD) ? "loaded" : "registered"));
    }

    if (verbose)
	printf ("Module id is %d.\n", id);

    return 0;
}

static int 
mload(int argc, char **argv)
{
    return mloadreg(CF_LOAD, argc, argv);
}

static int 
mreg(int argc, char **argv)
{
    return mloadreg(CF_REGISTER, argc, argv);
}

/************************************************************************
 *
 * unload/unregister
 *
 */

static void
munldregusage(void)
{
    uprintf ("[-v] id [id id ...]");
}

static int 
munloadreg(int command, int argc, char **argv)
{
    long id;
    int errcount = 0;
    int verbose = 0;

    if (getuid() != 0 && geteuid() != 0) {
	    fprintf (stderr, "Sorry, must be superuser to run %s %s.\n",
		cmdname, actname);
	    return -1;
    }

    --argc;
    ++argv;

    if (!argc) {
	munldregusage();
	return -1;
    }

    if (!strncmp(*argv, "-v", 2)) {
	--argc;
	++argv;
	verbose = 1;
	if (!argc) {
	    munldregusage();
	    return -1;
	}
    }

    while (argc--) {
	id = (int)atol(*argv++);

	if (unloadmodule(command, id) < 0) {
	    errcount++;
	    fprintf (stderr, "Error %s module %d:  %s.\n", 
		    command == CF_UNLOAD ? "unloading" : "unregistering", id,
		    ml_err(errno));
	} else if (verbose)
	    printf ("Module %d %s.\n", id, 
		    command == CF_UNLOAD ? "unloaded" : "unregistered");
    }


    return errcount ? -1 : 0;
}

static int 
munload(int argc, char **argv)
{
    return munloadreg(CF_UNLOAD, argc, argv);
}

static int 
munreg(int argc, char **argv)
{
    return munloadreg(CF_UNREGISTER, argc, argv);
}

/************************************************************************
 *
 * utilities
 *
 */

static int
loadmodule(int command, int type, char *fname, char *prefix, void *mdata, int timeout, int symdebug)
{
    cfg_desc_t descriptor, *desc = &descriptor;
    int rv;

    bzero (desc, sizeof(*desc));
    desc->m_data = mdata;
    desc->m_fname = fname;
    desc->m_type = type;
    if (command == CF_REGISTER ||
	(command == CF_LOAD && type == M_SYMTAB && timeout != M_UNLDDEFAULT))
	desc->m_delay = timeout;
    else
	desc->m_delay = M_NOUNLD;
    if (symdebug)
	desc->m_flags = M_SYMDEBUG;
    else
	desc->m_flags = 0;
    desc->m_cfg_version = M_CFG_CURRENT_VERSION;
    strncpy (desc->m_prefix, prefix, M_PREFIXLEN);

    rv = (int)syssgi (SGI_MCONFIG, command, desc);

    if (rv)
	return -1;
    
    return (int)desc->m_id;
}

static int
unloadmodule(int command, long id)
{
    return (int)syssgi (SGI_MCONFIG, command, id);
}

static void 
prmodule(mod_info_t *mod)
{
    int i;

    printf ("Id: %d\t", mod->m_id);
    switch (mod->m_type) {
    case M_DRIVER:
	if (mod->m_driver.d_type == MDRV_CHAR)
	    printf ("Character ");
	else if (mod->m_driver.d_type == MDRV_BLOCK)
	    printf ("Block ");
	else if (mod->m_driver.d_type == (MDRV_BLOCK|MDRV_CHAR))
	    printf ("Character/Block ");
	else if (mod->m_driver.d_type == MDRV_STREAM)
	    printf ("Streams ");
	else
	    printf ("Unknown ");

	printf ("device driver: prefix %s, ", mod->m_prefix);
	for (i=0; i<mod->m_driver.d_nmajors; i++)
	    printf ("major %d, ", mod->m_driver.d_majors[i]);
	if (mod->m_delay != M_NOUNLD) {
	    printf ("unload delay %d minutes, ", mod->m_delay);
	} else
	    printf ("no auto-unload, ");
	printf ("filename %s\n", mod->m_fname);
	break;
    case M_STREAMS:
	printf ("Streams module: ");
	printf ("prefix %s, fmodsw name %s, ", mod->m_prefix, 
	    mod->m_stream.s_name);
	if (mod->m_delay != M_NOUNLD) {
	    printf ("unload delay %d minutes, ", mod->m_delay);
	} else
	    printf ("no auto-unload, ");
	printf ("filename %s\n", mod->m_fname);
	break;
    case M_IDBG:
	printf ("Idbg module: filename %s\n", mod->m_fname);
	break;
    case M_LIB:
	printf ("Library module: filename %s\n", mod->m_fname);
	break;
    case M_SYMTAB:
	printf ("Symbol Table module: ");
	if (mod->m_delay != M_NOUNLD) {
	    printf ("unload delay %d minutes, ", mod->m_delay);
	} else
	    printf ("no auto-unload, ");
	printf ("filename %s\n", mod->m_fname);
	break;
    case M_FILESYS:
	printf ("File system: filename %s \n", mod->m_fname);
	break;
    case M_ENET:
	printf ("Ethernet module: ");
	printf ("prefix %s, filename %s\n", mod->m_prefix, mod->m_fname);
    case M_OTHER:
	printf ("Other module\n");
	break;
    default:
	printf ("Unknown module type\n");
	break;
    }
}

static char *
ml_err(int error)
{
	switch (error) {

	case	EPERM:
		return ("Must be super-user to use this command");

	case	MERR_ENOENT:
		return ("No such file or directory");

	case	MERR_ENAMETOOLONG:
		return ("Path name too long");
		
	case	MERR_VREG:
		return ("File not a regular file");

	case	EIO:
		return ("I/O error");

	case	ENXIO:
		return ("No such device or address");

	case	MERR_VOP_OPEN:
		return ("VOP_OPEN failed");

	case	MERR_VOP_CLOSE:
		return ("VOP_CLOSE failed");

	case	MERR_COPY:
		return ("Copyin/copyout failed");

	case	MERR_BADMAGIC:
		return ("Object file format not ELF");

	case	MERR_BADFH:
		return ("Error reading file header");

	case	MERR_BADAH: 
		return ("Error reading a.out header");

	case	MERR_BADSH:
		return ("Error reading section header");

	case	MERR_BADTEXT:
		return ("Error reading text section");

	case	EBUSY:
		return ("Device busy");

	case	MERR_BADDATA:
		return ("Error reading data section");

	case	MERR_BADREL:
		return ("Error reading relocation records");

	case	MERR_BADSYMHEAD:
		return ("Error reading symbol header");

	case	MERR_BADEXSTR:
		return ("Error reading external string table");

	case	MERR_BADEXSYM:
		return ("Error reading external symbol table");

	case	MERR_NOSEC:
		return ("Missing text, data and bss sections");

	case	MERR_NOSTRTAB:
		return ("No string table found for object");

	case	MERR_NOSYMS:
		return ("No symbols found for object");

	case	MERR_BADARCH:
		return ("Bad EF_MIPS_ARCH arch type");

	case	MERR_SHSTR:
		return ("Error reading section header string table");

	case	MERR_SYMTAB:
		return ("Error reading section symbol table");

	case	MERR_STRTAB:
		return ("Error reading section string table");

	case	MERR_NOTEXT:
		return ("No text or textrel section found in object");

	case	MERR_SHNDX:
		return ("Bad section index");

	case	MERR_UNKNOWN_CFCMD:
		return ("Unknown syssgi command");

	case	MERR_UNKNOWN_SYMCMD:
		return ("Unknown syssgi sym command");

	case	MERR_FINDADDR:
		return ("Address not found in symbol tables");

	case	MERR_FINDNAME:
		return ("Name not found in symbol tables");

	case	MERR_SYMEFAULT:
		return ("Address fault on sym command");

	case	MERR_NOELF:
		return ("Dynamically loadable ELF modules not supported");

	case	MERR_UNSUPPORTED:
		return ("Feature unsupported");

	case	MERR_LOADOFF:
		return ("Dynamic loading turned off");

	case	MERR_BADID:
		return ("Module id not found");

	case	MERR_NOTLOADED:
		return ("Module not loaded");

	case	MERR_NOTREGED:
		return ("Module not registered");

	case	MERR_OPENBUSY:
		return ("Can not open, device busy");

	case	MERR_UNLOADBUSY:
		return ("Can not unload module, device busy");

	case	MERR_UNREGBUSY:
		return ("Can not unregister module, device busy");

	case	MERR_ILLDRVTYPE:
		return ("Illegal driver type");

	case	MERR_NOMAJOR:
		return ("No major passed in cfg information");

	case	MERR_NOMAJORS:
		return ("Out of major numbers");

	case	MERR_ILLMAJOR:
		return ("Illegal major number");

	case	MERR_MAJORUSED:
		return ("Major number already in use");

	case	MERR_SWTCHFULL:
		return ("Switch table is full");

	case	MERR_UNLDFAIL:
		return ("Unload function failed");

	case	MERR_DOLD:
		return ("D_OLD drivers not supported as loadable modules");

	case	MERR_NODEVFLAG:
		return ("No xxxdevflag found");

	case	MERR_NOINFO:
		return ("No xxxinfo found");

	case	MERR_NOOPEN:
		return ("No xxxopen found");

	case	MERR_NOCLOSE:
		return ("No xxxclose found");

	case	MERR_NOMMAP:
		return ("No xxxmmap found");

	case	MERR_NOSTRAT:
		return ("No xxxstrategy found");

	case	MERR_NOSIZE:
		return ("No xxxsize found");

	case	MERR_NOUNLD:
		return ("No xxxunload found");

	case	MERR_NOVFSOPS:
		return ("No xxxvfsops found");

	case	MERR_NOVNODEOPS:
		return ("No xxxvnodops found");

	case	MERR_NOINIT:
		return ("No xxxinit found");

	case	MERR_NOINTR:
		return ("No xxxintr found");

	case	MERR_NOEDTINIT:
		return ("No xxxedtinit found");

	case	MERR_NOSTRNAME:
		return ("Stream name missing");

	case	MERR_STRDUP:
		return ("Duplicate streams name");

	case	MERR_NOPREFIX:
		return ("No prefix sent in cfg information");

	case	MERR_NOMODTYPE:
		return ("No module type sent in descriptor");

	case	MERR_BADMODTYPE:
		return ("Bad module type sent in descriptor");

	case	MERR_BADCFGVERSION:
		return ("Lboot version out of date with kernel version");

	case	MERR_BADVERSION:
		return ("Module version out of date with kernel version");

	case	MERR_NOVERSION:
		return ("Module version string is missing");

	case	MERR_BADLINK:
		return ("Can't resolve all symbols in object");

	case	MERR_BADJMP:
		return ("Address outside of jal range - compile with -jalr");

	case	MERR_BADRTYPE:
		return ("Bad relocation type");

	case	MERR_GP:
		return ("GP section unsupported - compile with -G0");

	case	MERR_BADSC:
		return ("Unexpected storage class encountered");

	case	MERR_REFHI:
		return ("Unexpected REFHI relocation type");

	case	MERR_NORRECS:
		return ("No relocation records found in object");

	case	MERR_SCNDATA:
		return ("Bad data section encountered in object");

	case	MERR_COMMON:
		return ("Common storage class in object not supported");

	case	MERR_JMP256:
		return ("Can't relocate j in a 256MB boundary - compile with -jalr");

	case	MERR_LIBUNLD:
		return ("Library modules can not be unloaded");

	case	MERR_LIBREG:
		return ("Library modules can not be registered/unregistered");

	case	MERR_NOLIBIDS:
		return ("Out of library module ids");

	case	MERR_IDBG:
		return ("Idbg.o module already loaded or statically linked into kernel");

	case	MERR_IDBGREG:
		return ("The idbg.o module can not be registered/unregistered");

	case	MERR_NOENETIDS:
		return ("Out of ethernet module ids");

	case	MERR_ENETREG:
		return ("Ethernet modules can not be registered");

	case	MERR_ENETUNREG:
		return ("Ethernet modules can not be unregistered");

	case	MERR_ENETUNLOAD:
		return ("Ethernet modules can not be unloaded");

	case	MERR_NOFSYSNAME:
		return ("No filesystem name specified");

	case	MERR_DUPFSYS:
		return ("filesystem already exists in vfssw table");

	case	MERR_VECINUSE:
		return ("Interrupt vector in use");

	case	MERR_BADADAP:
		return ("No adapter found");

	case	MERR_NOEDTDATA:
		return ("No edt data for ethernet driver");

	case	MERR_ELF64:
		return ("A 64bit kernel requires a 64bit loadable module");

	case	MERR_ELF32:
		return ("A 32bit kernel requires a 32bit loadable module");

	case	MERR_ELFN32:
		return ("An N32 kernel requires an N32 loadable module");

	case	MERR_ELF64COFF:
		return ("A 64bit kernel requires a 64bit ELF loadable module");

	case	MERR_ET_REL:
		return ("Loadable modules must be built as relocatable");

	case	MERR_SYMTABREG:
		return ("Can not reg/unreg a symbol table module");

	case	MERR_NOSYMTABIDS:
		return ("Out of symbol table module ids");

	case	MERR_NOSYMTAB:
		return ("No symbol table found in object");

	case	MERR_DUPSYMTAB:
		return ("Symbol table for this filename already exists");

	case	MERR_NOSYMTABAVAIL:
		return ("No runtime symbol table available for running kernel");

	case	MERR_SYMTABMISMATCH:
		return ("Symbol table does not match running kernel");

	case	MERR_NOIDBG:
		return ("Idbg.o must be loaded before xxxidbg.o modules");

	case	MERR_UNREGFAIL:
		return ("xxxunreg failed");

	default:
		return ("Error unknown");
	}
}

#ifdef DEBUG
static int
prinfo(int type, char *fname, char *prefix, int devflag)
{
    printf ("module type is ");
    switch (type) {
    case M_DRIVER: printf ("DRIVER\n"); break;
    case M_STREAMS: printf ("STREAM\n"); break;
    case M_IDBG: printf ("IDBG\n"); break;
    case M_LIB: printf ("LIB\n"); break;
    case M_SYMTAB: printf ("SYMTAB\n"); break;
    case M_FILESYS: printf ("FILESYS\n"); break;
    case M_ENET: printf("ENET\n"); break;
    case M_OTHER: printf ("OTHER\n"); break;
    default: printf ("UNKNOWN\n"); break;
    }
    printf ("file name is %s\n", fname);
    printf ("prefix is %s\n", prefix);

    if (type == M_DRIVER) {
	printf ("device driver type is ");
	switch (devflag) {
	case MDRV_CHAR: printf ("CHAR\n"); break;
	case MDRV_CHAR|MDRV_BLOCK: printf ("CHAR/BLOCK\n"); break;
	case MDRV_BLOCK: printf ("BLOCK\n"); break;
	case MDRV_STREAM: printf ("MDRV_STREAM\n"); break;
	default: printf ("OTHER\n"); break;
	}
    }

    return (0);
}
#endif /* DEBUG */
