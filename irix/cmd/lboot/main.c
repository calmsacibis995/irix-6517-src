/*	Copyright (c) 1984 AT&T	*/
/*	  All Rights Reserved  	*/

#ident "$Revision: 2.44 $"

#include <sys/types.h>
#include <a.out.h>
#include <sys/param.h>
#include "lboot.h"

#include <sys/syssgi.h>
#include <sys/conf.h>
#include <sys/fstyp.h>
#include <sys/mload.h>
#include <getopt.h>

/*
 * lboot facts ... good things to know
 *
 * 1. How does lboot come up with the order of the object files sent to ld ?
 *
 *    lboot parses the system files, parses all of the vector, include and
 *    use lines, and creates a driver list. It then goes thru that list and
 *    adds all of the dependencies from the master files, so that they get
 *    added in the order that the other modules were put in the list. When 
 *    the ld line is created, lboot puts all of the .o's from the list first,
 *    then the .a's, then the stubs. The order in the ld line is the reverse
 *    order as things appear in the system files. The following comment exists
 *    in system.gen:
 *
 *       VECTOR lines must occur in the opposite order that the devices
 *               should be probed.  Do not change the order of the VECTOR
 *               lines in this file, unless you understand the implications.
 *
 * 2. Why is autoconfig building a new kernel and how does it decide ?
 *
 *    Use autoconfig -v and lboot will tell you why it has decided that a new
 *    kernel should be built.
 *
 *    lboot via autoconfig decides to generate a new kernel if any of the 
 *    following are true:
 *
 *    - /unix does not exist 
 *    - one or more files in /var/sysgen/master.d are newer than /unix 
 *    - one or more files in /var/sysgen/boot are newer than /unix 
 *    - if /var/sysgen/boot is newer than /unix 
 *    - if /var/sysgen/stune is newer than /unix 
 *    - one or more files in /var/sysgen/system are newer than /unix 
 *    - if new/different hardware is detected, in which case the edt file 
 *        generated is different from the previous one 
 *
 */

char *root;				/* root path prefix */
char *toolpath;				/* path to tools */
char *cc;
char *ccopts;
char *ld;
char *ldopts;
char *slash_boot = "/var/sysgen/boot";
int  bspec = 0;				/* user overrode boot directory */
char *slash_runtime;
int wspec = 0;
char *slash_mtune = "/var/sysgen/mtune";
int  mtspec = 0;			/* user overrode mtune directory */
char *stune_file = "/var/sysgen/stune";
int  stspec = 0;			/* user overrode stune file */
char *master_dot_d = "/var/sysgen/master.d";
int  mspec = 0;				/* user overrode master directory */
char *unix_name = 0;
char *etcsystem = "/var/sysgen/system";
int  esspec = 0;			/* user overrode system file */
char *kernel_master = 0;		/* master.d/kernel */
char *mtune_kernel = 0;			/* mtune/kernel file */

static char *cwd;
char *cwd_slash;			/* current working directory */

#define MAXMOD 1024

int modloadc;
char *modloadv[MAXMOD];
int modregc;
char *modregv[MAXMOD];
int modunldc;
int modunldv[MAXMOD];
int modunregc;
int modunregv[MAXMOD];

int do_mreg_diags;

int Debug = 0;
int Verbose = 0;
int Smart = 0;
int Autoconfig = 1;
int Autoreg = 0;		/* perform an autoregister only */
int Noautoreg = 0;		/* do normal autoconfig, but don't autoreg */
int Dolink = 0;
int Nogo = 0;

int
main(int argc, char *argv[])
{
	struct stat statbuffer;
	int c;

	cwd = getcwd(0, MAXPATHLEN);
	if (cwd == 0)
		error(ER2, "getcwd");
	cwd_slash = concat(cwd, "/");

	while ((c = getopt(argc, argv, "NeladfvtTu:m:r:b:w:s:n:c:L:R:VU:W:O:")) != EOF)
		switch (c) {
		case 'N':
			Noautoreg = TRUE;
			break;

		case 'a':
			Autoreg = TRUE;
			break;

		case 'd':
			Debug = TRUE;
			break;

		case 'v':
			Verbose = TRUE;
			break;

		case 'e':
			Nogo = TRUE;
			Noautoreg = TRUE;
			break;

		case 't':
			Smart = TRUE;
			Autoconfig = FALSE;
			if (!unix_name)
				unix_name = "unix";
			break;

		case 'T':
			Smart = TRUE;
			Autoconfig = TRUE;
			if (!unix_name)
				unix_name = "unix";
			break;
		
		case 'O':
			do_tunetag(1, &optarg, NULL);
			break;

		case 'u':
			unix_name = optarg;
			break;

		case 'l':
			Dolink = TRUE;
			break;

		case 'r':
			root = optarg;
			break;

		case 'm':
			master_dot_d = optarg;
			if (master_dot_d[0] != '/')
				master_dot_d = concat(cwd_slash,
							  master_dot_d);

			if (stat(master_dot_d, &statbuffer) < 0)
				error(ER2, master_dot_d);
			if (!(statbuffer.st_mode & S_IFDIR))
				error(ER100, master_dot_d);
			mspec++;
			break;

		case 'w': 
			slash_runtime = optarg;
			if (slash_runtime[0] != '/')
				slash_runtime = concat(cwd_slash, slash_runtime);
			if (stat(slash_runtime, &statbuffer) < 0)
				error(ER2, slash_runtime);
			if (!(statbuffer.st_mode & S_IFDIR))
				error(ER100, slash_runtime);
			wspec++;
			break;

		case 's':
			etcsystem = optarg;
			if (etcsystem[0] != '/')
				etcsystem = concat(cwd_slash, etcsystem);
			esspec++;
			break;

		case 'b':
			slash_boot = optarg;
			if (slash_boot[0] != '/')
				slash_boot = concat(cwd_slash, slash_boot);

			if (stat(slash_boot, &statbuffer) < 0)
				error(ER2, slash_boot);
			if (!(statbuffer.st_mode & S_IFDIR))
				error(ER100, slash_boot);
			bspec++;
			break;

		case 'c':
			stune_file = optarg;
			if (stune_file[0] != '/')
				stune_file = concat(cwd_slash, stune_file);

			stspec++;
			break;

		case 'n':
			slash_mtune = optarg;
			if (slash_mtune[0] != '/')
				slash_mtune = concat(cwd_slash, slash_mtune);

			if (stat(slash_mtune, &statbuffer) < 0)
				error(ER2, slash_mtune);
			if (!(statbuffer.st_mode & S_IFDIR))
				error(ER100, slash_mtune);
			mtspec++;
			break;
		
		case 'L': /* dynamic load */
			for (;;) {
				char *modname;

				for (modname = strtok(optarg,", 	");
					modname && modloadc < MAXMOD;
						modname = strtok(0,", 	"))

					modloadv[modloadc++] = modname;
				if ( optind >= argc || *argv[optind] == '-' )
					break;
				optarg = argv[optind++];
			}
			break;

		case 'R': /* dynamic register */
			for (;;) {
				char *modname;

				for (modname = strtok(optarg,", 	");
					modname && modregc < MAXMOD;
						modname = strtok(0,", 	"))
					modregv[modregc++] = modname;
				if ( optind >= argc || *argv[optind] == '-' )
					break;
				optarg = argv[optind++];
			}
			break;

		case 'U': /* dynamic unload */
			for (;;) {
				modunldv[modunldc++] = (int)atol(optarg);
				if ( optind >= argc || *argv[optind] == '-' )
					break;
				optarg = argv[optind++];
			}
			break;

		case 'W': /* dynamic unregister */
			for (;;) {
				modunregv[modunregc++] = (int)atol(optarg);
				if ( optind >= argc || *argv[optind] == '-' )
					break;
				optarg = argv[optind++];
			}
			break;

		case 'V': /* list loaded/registered modules */
			do_mlist();
			break;

		case '?':
			fprintf(stderr,"[-dv] [-t|T] [-a] [-N] [-l] [-u /unix] [-m master.d] [-b boot] [-w workarea]\n\t[-s system] [-p /toolroot] [-r /root] [-c stune]\n\t[-n mtune] [-L master] [-R master] [-U id] [-W id] [-V] [-O tag]\n");
			exit(2);
		}

	if (!unix_name)
		unix_name = "unix.new";
	if (unix_name[0] != '/')
		unix_name = concat(cwd_slash, unix_name);

	/*
	 * If "ROOT" was specified in some way, re-wire the paths
	 * (if the user didn't explicitly set them) to be at the
	 * proper place.
	 */
	if (!root) {
		root = getenv("ROOT");
		if (root && strcmp(root, "/"))
			printf ("Lboot using ROOT=%s\n", root);
	}
	if (!root) {
		root = "";
	} else {
		if (!bspec) {
			slash_boot = concat(root, slash_boot);
		}
		if (!mspec) {
			master_dot_d = concat(root, master_dot_d);
		}
		if (!esspec) {
			etcsystem = concat(root, etcsystem);
		}
		if (!stspec) {
			stune_file = concat(root, stune_file);
		}
		if (!mtspec) {
			slash_mtune = concat(root, slash_mtune);
		}
	}
	if (!wspec) {
		slash_runtime = slash_boot;
	}
		

	/*
	 * If "TOOLROOT" was specified in some way, re-wire the paths
	 *  to be at the proper place.
	 */
	toolpath = getenv("TOOLROOT");
	if (toolpath != 0) {
		if(strcmp(toolpath, "/var/sysgen/root"))
			printf ("Lboot using TOOLROOT=%s\n", toolpath);
		toolpath = concat(toolpath, "/usr/bin/");
	}

	init_dev_admin();

	{ char *c_default;
	/* this is just a warning; we don't use it directly, unlike
	 * ROOT and TOOLROOT.  This is new in mipspro 7.1 */
	if(c_default=getenv("COMPILER_DEFAULTS_PATH"))
			printf ("Lboot using compiler defaults in COMPILER_DEFAULTS_PATH=%s\n",
				c_default);
	}

	if (chdir(slash_boot) == -1)
		error(ER2, slash_boot);

	if (modloadc || modregc || modunldc || modunregc) {
		if (modloadc)
			do_mloadreg(modloadc,modloadv,CF_LOAD);
		if (modregc)
			do_mloadreg(modregc,modregv,CF_REGISTER);
		if (modunldc)
			do_munloadreg(modunldc,modunldv,CF_UNLOAD);
		if (modunregc)
			do_munloadreg(modunregc,modunregv,CF_UNREGISTER);
	} else
		loadunix();			/* do the work */
	return 0;
}
