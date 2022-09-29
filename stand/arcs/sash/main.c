#ident	"sash/main.c: $Revision: 1.114 $"

/*
 * main.c -- main line sash code
 */

#include <arcs/errno.h>
#include <arcs/io.h>

#include <saioctl.h>
#include <stringlist.h>
#include <parser.h>
#include <setjmp.h>
#include <gfxgui.h>
#include <string.h>
#include <arcs/restart.h>
#include <arcs/signal.h>
#include <arcs/io.h>
#include <arcs/pvector.h>
#include <arcs/spb.h>
#include <libsc.h>

extern jmp_buf restart_buf;

#define	PROMPT	"sash: "

static char *fixuppath(char *);
static int version(int, char **, char **, struct cmd_table *);
static void main_intr (void);
static void sash_quit(char *, char *, LONG);
static LONG sash_autoboot(int, char **, char **);
static char *kernel_name(char *, char *);
static void adjustmrargs(int, char **, int, int *);
int setup_specialswap_vh(void);
extern void set_vhchksum(struct volume_header *);
extern void dump_vh(struct volume_header *, char *);


/*
 * commands
 */

#ifdef DEBUG
extern int mcopy(), mcmp(), mfind(), memlist();
extern int dev_test(), checksum();
#endif

extern int modem_init (int argc, char **argv);

/*
 * cmd_table -- interface between parser and command execution routines
 * Add new commands by making entry here.
 */
static struct cmd_table cmd_table[] = {
	{ "boot",	boot,		"boot:\t\tboot [-f FILE] [-n] [ARGS]" },
	{ "cat",	cat,		"cat:\t\tcat FILE_LIST" },
	{ "cp",		copy,		"copy:\t\tcp [-b BLOCKSIZE] [-c BCOUNT] SRC_FILE DST_FILE" },
	{ "go",		go_cmd,		"go:\t\tgo [INITIAL_PC]" },
	{ "help",	help,		"help:\t\thelp [COMMAND]" },
	{ "?",		help,		"help:\t\t? [COMMAND]" },
	{ "hinv",	hinv,		"inventory:\thinv [-v] [-t [-p]]" },
	{ "install",	mrboot,		"install:\tinstall" },
	{ "ls",	ls,		"ls:\t\tls DIRECTORY|DEVICE" },
	{ "printenv",	printenv_cmd,	"printenv:\tprintenv [ENV_VAR_LIST]" },
	{ "setenv",	setenv_cmd,	"setenv:\t\tsetenv ENV_VAR STRING" },
	{ "unsetenv",	unsetenv_cmd,	"unsetenv:\tunsetenv ENV_VAR" },
	{ "version",	version,	"version:\tversion" },
#ifdef DEBUG
	{ "checksum",	checksum,	"checksum:\tchecksum (RANGE | -f FILE)" },
	{ "dev",	dev_test,	"device test:\t\tdev [-d pattern] [-w write addr] [-r read addr] -b blkcnt  TEST_DEVICE" },
	{ "mcmp",	mcmp,		"mcmp:\t\tmcmp [-(b|h|w)] FROMRANGE TO" },
	{ "mcopy",	mcopy,		"mcopy:\t\tmcopy [-(b|h|w)] FROMRANGE TO" },
	{ "mfind",	mfind,		"mfind:\t\tmfind [-(b|h|w)] [-n] RANGE VALUE" },
	{ "mlist",	memlist,	"mlist:\t\tmlist" },
#endif
	{ 0,		0,		"" }
};

extern void sdvh_install(void);
extern void mr_setinst_state(int);
extern int mr_interrupted_inst(int, int);
extern int mr_iscustom(int, char**);

/*
 * main -- called from csu after data area has been cleared
 */
int
main(int argc, char **argv, char **envp)
{
	size_t s;
	LONG rc;
	int modemstat, domrb, mrspecial=0, mrok=0, dovhsetup=0, i;
	char *osopts;

	updatetv();			/* check for backlevel spb tv */

	rbclrbs(BS_BSTARTED);		/* clear boot started flag (for ARC) */

	if(getenv("DEBUG")) {	/* mostly for partitioning debugging */
		atob(getenv("DEBUG"),&Debug);
		if(!Debug)
			Debug = 1;
	}

	_init_malloc();			/* initialize malloc for efs_install */
	efs_install();			/* install EFS */
	xfs_install();			/* install XFS */
	sdvh_install();			/* install volhdr filesystem with rewrite */

	/* We add modem initialization / remote diagnostics capability here.
	 * The exit status from this call tells what to do next: if "0",
	 * either there was no modem, or there was a modem but boot
	 * is to proceed normally. If "1", then there was a modem and
	 * sash is to exit back to the interactive prompt.  If "2"
	 * (or greater) then sash is to ignore the boot file and continue
	 * to its command interpreter.
	 */
	modemstat = modem_init (argc, argv);	/* add modem hook here */

	initGfxGui(0);			/* we use gui for install */

	ioctl(StandardIn,TIOCSETXOFF,0);

	/*
	 * Deal with autoboots, remote debugging boots, and two-level
	 * boots.  fix up path first, so that one can just say
	 * 'boot unix' from the PROM or 'unix' from sash and have it work.
	 */

	*argv = fixuppath(*argv);

	ioctl (0, TIOCINTRCHAR, (long)"\003\033");	/* ^C and ESC */

	/* see if we're in a special miniroot mode */ 
	mrspecial = mr_iscustom(argc-1, argv+1);

	if(mrspecial)
	    for (i=1; i<argc; ++i)
	        if (!strcmp(argv[i], "disksetup=true")) {
		    dovhsetup = 1;
		    break;
		}

	/* avoid doing this check in two places */
	domrb = argc>1 && (!strcmp(argv[1], "OSLoadOptions=mini") || 
	        !strcmp(argv[1], "-m") || mrspecial); 

	/* see if we either had an interrupted miniroot install (INST*),
	 * or completed a miniroot inst normally (inst*) and do the right
	 * thing in either case.  See mrboot_cmd.c for guts of it */
	if(osopts=getenv("OSLoadOptions")) {
		if(!strncmp("INST", osopts, 4)) {
			if(!domrb) switch(mr_interrupted_inst(0,mrspecial)) {
				case 0: goto alldone;
				case -1: mrok=1; /* so autoboot can continue after fixup */
					break;
			}
		}
		else if(!strncmp("inst", osopts, 4)) {
			mr_setinst_state(1);	/* reset inststate back to normal */
			mrok = 1;	/* done because prom will not set OSLoadOptions
				if already set, so this lets us fake it...  worse yet,
				it doesn't put the OSLoadOptions=xxx in the same position
				if the prom thinks it was set explictly... */
		}
	}

	/* Hook for sash to be installed on the volume header as
	 * ide to load /stand/ide for machines that do not know
	 * how to do a two level boot for ide.
	 */
	if (argv[0] && (s = strlen(argv[0])) &&
	    (strcmp(argv[0]+s-3,"ide") == 0)) {
		char *s = *argv;
		char *p;
		*argv = kernel_name(s,"/usr/stand/ide");
		rc = sash_autoboot(argc, argv, envp);
		/* get here iff sash_autoboot failed
		 * if the partition ends in a 0, i.d. dksc(0,1,0)
		 * then check for dksc(0,1,6), i.e. change the
		 * last 0 into a 6 and see if that is bootable
		 */
		s = *argv = kernel_name(s,"/stand/ide");
		for(p = s + strlen(s) - 1; p != s; p--) {
			if(*p == '0') {
				*p = '6';
				break;
			}
		}
		rc = sash_autoboot(argc, argv, envp);
		sash_quit("IDE boot failed",argv[0],rc);
	}

	if (argc >= 2) {
		if(modemstat == 0) {	/* normal */
			if (argv[0] && (s = strlen(argv[0])) &&
				strcmp(argv[0]+s-4, "unix") == 0) {
				/* diskless boot */
				*argv = kernel_name(*argv, "unix.auto");
				rc = sash_autoboot(argc, argv, envp);
				sash_quit("Autoboot failed",*argv,rc);
			} else if (!strcmp(argv[1], "OSLoadOptions=auto") ||
				!strcmp(argv[1], "-a") ||
				((osopts=getenv("OSLoadOptions"))
				    && !strcmp(osopts,"auto")) ) {
			    /* check vh for custom boot file for auto mr */
			    if (do_custom_boot(argc-2,&argv[2]) != 0)
				sash_quit("Custom boot failed",0,0);
			    else {
autoboot:                       /* autoboot */
				*argv = kernel_name(*argv,0);
				rc = sash_autoboot(argc, argv, envp);
				sash_quit("Autoboot failed",*argv,rc);
			    }
			} else if (domrb) {
			    int argoffset;
				if(dovhsetup && setup_specialswap_vh())
					sash_quit(0,0,0);
			    adjustmrargs(argc,argv,mrspecial,&argoffset);
			    /* miniroot boot */
			    mrboot(argc-argoffset, &argv[argoffset], argv, 0);
			    /* mrboot prints error message */
			    sash_quit(0,0,0);
			} else if ((strcmp(argv[1], "OSLoadOptions=remote") == 0) ||
				 (strcmp(argv[1], "-r") == 0)) {
				/* remote debugging */
				setenv("dbgmon", "1");
				setenv("rdebug", "1");
				*argv = kernel_name(*argv,0);
				rc = sash_autoboot(argc, argv, envp);
				sash_quit("Remote debug boot failed",*argv,rc);
			} else if ((argv[1][0] != '-') && !index(argv[1],'=')) {
				/* boot passed filename */
				rc = sash_autoboot(argc-1, &argv[1], envp);
				sash_quit("Autoboot failed",argv[1],rc);
			}
			else if(mrok)
				goto autoboot;	/* reboot after mr isntall */
		}
		else if(modemstat == 1) {
			/* since we are aborting a normal boot here, we need to
			 * make sure that this path can not be taken indefinitely.
			 * We do this by limiting this exit to the case that sash
			 * was invoked by autoboot or with a boot file name.
			 * This will allow the direct invocation of sash from the
			 * interactive menu, which can then be used to boot UNIX.
			 */
			if ((strcmp(argv[1], "OSLoadOptions=auto") == 0) ||
					(strcmp(argv[1], "-a") == 0) ||
					((osopts=getenv("OSLoadOptions"))
					   && !strcmp(osopts,"auto")) ) {
				/* autoboot */
				sash_quit (NULL, NULL, 0);
			}
			else if ((argv[1][0] != '-') && !index(argv[1],'=')) {
				/* boot passed filename */
				sash_quit (NULL, NULL, 0);
			}
		}
		/* else we will fall through to sash command mode */
	}

	/* NO flags and no osloadfilename set in argv  - go to cmd mode */
	ioctl (0, TIOCINTRCHAR, (long)"\003");	/* ^C only */
	if ( getenv("VERBOSE") )
		atob(getenv("VERBOSE"),&Verbose);
	else
		Verbose = 1;
	version(argc, argv, argv, 0);	/* these args are all dummy */
	if (setjmp(restart_buf)) {
		putchar('\n');
		close_noncons();	/* close leftover fd's */
	}
	Signal (SIGINT, main_intr);
	command_parser(cmd_table, PROMPT, 1, 0);

alldone:
	putchar('\n');
	fs_unregister("efs");
	fs_unregister("xfs");
	return(0);
}

/*
 * where sash goes on a sigint
 */
static void
main_intr (void)
{
    if (Verbose)
	longjmp (restart_buf, 1);
    else {
	fs_unregister("efs");
	EnterInteractiveMode();
    }
}

/*
 * prepend a complete pathname onto a path such as "sash"
 */
static char *
fixuppath(char *path)
{
	static char newpath[2*64+1];
	char *p, *cp;

	/* if we didn't find a '(', then prepend the default path to path */
	if (!(cp=rindex(path, '('))) {
		/* get a copy of the current default boot device path */
		cp = path;
		if (!(p = getenv("SystemPartition")))
	    		return (path);
		strcpy (newpath, p);
		strcat (newpath, cp);
		return (newpath);
	} else
		return (path);
}

static void
sash_quit(char *string, char *file, LONG errno)
{
	static int sq_buttons[] = {DIALOGCONTINUE,DIALOGPREVDEF,-1};
	char buf[240];
	char fbuf[160];	/* kernel_name assumes file can be 128 */

	if (string) {
		if (file)
			sprintf(fbuf,"\n%s: %s",file,arcs_strerror(errno));
		else
			*fbuf = '\0';

		if (isGuiMode()) {
			setGuiMode(1,0);
			sprintf(buf,"%s.%s",string,fbuf);
		}
		else
			sprintf(buf,"\n\n%s.%s.\n\nHit Enter to continue.",
				string,fbuf);
		popupDialog(buf,sq_buttons,DIALOGWARNING,DIALOGCENTER);
	}

	fs_unregister("efs");
	EnterInteractiveMode();
}

static LONG
sash_autoboot(int argc, char **argv, char **envp)
{
	setenv ("kernname", *argv);
	return(Execute(*argv, (LONG)argc, argv, envp));
}
		

/*ARGSUSED*/
static int
version(int a, char **b, char **c, struct cmd_table *d)
{
	extern char *getversion(void);
#if _MIPS_SIM == _ABI64
	char *size = "64";
#else
	char *size = "32";
#endif

	printf("Standalone Shell %s (%s Bit)\n", getversion(), size);
	return 0;
}

/*
 * kernel_name - construct a rational path name for the kernel given the file 
 * that sash was loaded from. The sash path name should have been "fixed up"
 * by calling fixuppath before passing it here.
 */
static char *
kernel_name(char *oboot, char *name)
{
	char *cp, *bp;
	int cnt = 0;
	static char bootfile[128];
	size_t s;

	bp = bootfile;
	
	/* 
	 * If sash was booted off of the net, replace "sash" in the
	 * boot string with "unix" and try to autoboot unix off of
	 * the net.
	 *
	 * For diskless boots, we may have been invoked as "...unix".
	 */
	if (!strncmp("bootp", oboot, 5) || !strncmp("net", oboot, 3)) {
		cp = oboot;
		cp = &cp[strlen(cp)];
		
		if (name) {
			while (cp > oboot && *cp != ')' && *cp != ':' &&
			       *cp != '/') {
				cnt++; cp--;
			}
		} else {
			while (cp > oboot && *cp != ')' && *cp != ':') {
				cnt++; cp--;
			}
		}

		cnt--;
		if ((cnt = ((int)strlen(oboot)-cnt)) <= 0) {
			printf("bad sash filename format, can't load kernel\n");
			return NULL;
	
		}
		strncpy (bp, oboot, cnt);
		if (name)
			strcat (bp, name);
		else
			strcat (bp, getenv("OSLoadFilename"));
	/* 
	 * If sash was booted off of the disk, get the unix partition and
	 * filename out of the volume header.
	 */

	} else {
		strcpy (bp, getenv("OSLoadPartition"));
		if (name)
			strcat (bp, name);
		else
			strcat (bp, getenv("OSLoadFilename"));
	}

	return(bootfile);
}


/*  adjust argc/argv arguments to be used when booting the 
    miniroot kernel.  In the special miniroot modes, we need to pass
    the mrmode nvram variable to the kernel.  Also, if we are in
    a special miniroot mode, try to derive tapedevice if it is not
    already set since we want the install to be non interactive     */

static void
adjustmrargs(int argc, char **argv, int special, int *argoffset)
{

    *argoffset = 2;
    if (special) {
	/* don't require user to have to set tapedevice in prom */
	if (argc > 0 && getenv("tapedevice") == NULL) {
	    char taped[256], *ptr, *bp;
	    *taped = '\0';
	    /* user accessed sash from the sa so just remove (sashxxxx) */
	    if (argv[0][strlen(argv[0])-1] == ')' && 
		(ptr=rindex(argv[0],'('))) {
	       strncpy(taped,argv[0],ptr-argv[0]);
	       taped[ptr-argv[0]] = '\0';
	    } else if (bp = strstr(argv[0],"bootp()")) {
		/* sash is separate program; replace sash with sa */
		if ((ptr=rindex(argv[0],'/')) == NULL)
		    if ((ptr = rindex(argv[0],':')) == NULL)
			ptr=bp+6;  /* to the ')' */
		if (ptr != NULL) {
		    strncpy(taped,argv[0],ptr-argv[0]+1);
		    strcpy(taped+(ptr-argv[0]+1),"sa");
		}
	    }
	    if (*taped)
		setenv("tapedevice",taped);
	}
	(*argoffset)--;  /* need to pass mrmode to the kernel */
    }
    /* just in case mrmode was previously set in env */
    setenv("mrmode","");
}

#ifdef DEBUG	/* so ASSERT's can be used */
int doass = 1;
void
assfail(char *a, char *f, char *l)
{
	panic("assertion failed: %s, file: %s, line: %d", a, f, l);
}
#endif


/* open the SystemPartition device, and if swap doesn't immediately
 * follow the volhdr, create a "standard" irix 6.5 root filesystem.
*/
#include <sys/dvh.h>
#include <sys/dkio.h>

#define VH_AMT 4096 /* read size in bytes for vhdr; allows for multiple
	* block sizes of devices; avoids problems with cdrom, etc. */

dopart(char *part, ULONG fd)
{
	struct volume_header *vh = calloc(VH_AMT, 1);
	LONG rval;
	ULONG cc, cc2;
	LARGE offset;
	struct partition_table *pt = vh->vh_pt;

	if(!vh) {
		printf("unable to malloc space for volume header\n");
		return 1;
	}

	if((rval = Read (fd, vh, VH_AMT, &cc)) || cc<sizeof(*vh) || !is_vh(vh)) {
		unsigned drivecap = 0;
		printf("%s: invalid partition table, initializing disk\n",
			part);
		cc = VH_AMT;	/* for write below */
		bzero(vh, cc);
		pt[8].pt_nblks = PTYPE_VOLHDR_DFLTSZ;	/* same as fx; from dvh.h */
		pt[8].pt_firstlbn = 0;
		pt[8].pt_type = PTYPE_VOLHDR;
		pt[0].pt_nblks = pt[1].pt_nblks = 0;	/* force setup below */
		pt[10].pt_firstlbn = 0;
		pt[10].pt_type = PTYPE_VOLUME;
		/* if this fails, we are sort of hosed... */
		(void)ioctl(fd, (long)DIOCREADCAPACITY, (LONG)&drivecap);
		vh->vh_dp.dp_drivecap = pt[10].pt_nblks = drivecap;
		vh->vh_swappt = 1;
		vh->vh_dp.dp_secbytes = 512; /* should get from driver at some point */
		strcpy(vh->vh_bootfile, "/unix");
	}

	/* if swap (part 1) doesn't immediately follow vh, or 
	 * part 1 is < 40MB, then force a partitioning.  Note that
	 * we pay no attention to swapdev or anything else.  When
	 * this option is invoked, it's based entirely on
	 * systempartition, which is OK, since there is a special
	 * option that invokes this behavior, and it's normally only
	 * going to be invoked by roboinst when requested.
	 */
	if(pt[1].pt_firstlbn != pt[8].pt_nblks ||
		pt[1].pt_nblks < 102400) {
		pt[8].pt_nblks = PTYPE_VOLHDR_DFLTSZ;	/* same as fx; from dvh.h */
		pt[8].pt_firstlbn = 0;
		pt[8].pt_type = PTYPE_VOLHDR;
		pt[1].pt_firstlbn = pt[8].pt_nblks;
		pt[1].pt_nblks = 102400;
		pt[1].pt_type = PTYPE_RAW;
		pt[0].pt_firstlbn = pt[1].pt_firstlbn + pt[1].pt_nblks;
		pt[0].pt_nblks = pt[10].pt_nblks - pt[0].pt_firstlbn;
		pt[0].pt_type = PTYPE_XFS;
		set_vhchksum(vh);
		offset.hi = 0; offset.lo = 0;
		if(rval=ioctl(fd, (long)DIOCSETVH, (LONG)vh))
			printf("failed to set new partition table in driver: error %ld\n", rval);
		if(Seek(fd, &offset, SeekAbsolute) ||
			(rval = Write(fd, vh, cc, &cc2))!=ESUCCESS)
			printf("failed to write new partition table to disk: error %ld\n", rval);
		if(Debug)
			dump_vh(vh, "new partition table for miniroot");
	}
	else if(Debug)
		printf("partitions already OK, not changed\n");

	(void)free(vh);
	(void)Close(fd);

	return rval;

}


setup_specialswap_vh()
{
	ULONG fd;
	LONG rval;
	char *part, *s = "SystemPartition";

	part = getenv(s);
	if(!part) {
		printf("%s not set, can't repartition for miniroot\n", s);
		return 1;
	}

	rval = Open (part, OpenReadWrite, &fd);
    if (rval != ESUCCESS) {
		printf ("Unable to open %s %s to repartition: error %ld\n", s, part, rval);
		return 1;
	}
	return dopart(part, fd);
}
