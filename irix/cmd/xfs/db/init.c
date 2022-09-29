#ident "$Revision: 1.18 $"

#include <sys/param.h>
#include <sys/uuid.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/fs/xfs_types.h>
#include <sys/fs/xfs_inum.h>
#include <sys/fs/xfs_sb.h>
#include <sys/fs/xfs_alloc_btree.h>
#include "sim.h"
#include "command.h"
#include "data.h"
#include "init.h"
#include "input.h"
#include "io.h"
#include "mount.h"
#include "sig.h"
#include "output.h"

char		*fsdevice;
char		*progname;

static void	usage(void);

void
init(
	int		argc,
	char		**argv)
{
	int		c;
	FILE		*cfile = NULL;

#if _MIPS_SIM == _ABI64
	progname = "xfs_db64";
#else
	progname = "xfs_db";
#endif
	while ((c = getopt(argc, argv, "c:fp:rx")) != EOF) {
		switch (c) {
		case 'c':
			if (cfile == NULL)
				cfile = tmpfile();
			fprintf(cfile, "%s\n", optarg);
			break;
		case 'f':
			simargs.disfile = 1;
			break;
		case 'p':
			progname = optarg;
			break;
		case 'r':
			simargs.isreadonly = XFS_SIM_ISREADONLY;
			flag_readonly = 1;
			break;
		case 'x':
			flag_expert_mode = 1;
			break;
		case '?':
			usage();
			/*NOTREACHED*/
		}
	}
	if (optind + 1 != argc) {
		usage();
		/*NOTREACHED*/
	}
	fsdevice = argv[optind];
	if (!simargs.disfile)
		simargs.volname = fsdevice;
	else
		simargs.dname = fsdevice;
	simargs.notvolok = 1;
	if (!xfs_sim_init(&simargs)) {
		usage();
		/*NOTREACHED*/
	}
	mp = dbmount();
	if (mp == NULL) {
		dbprintf("%s: %s is not a valid filesystem\n",
			progname, fsdevice);
		exit(1);
		/*NOTREACHED*/
	}
	blkbb = 1 << mp->m_blkbb_log;
	push_cur();
	init_commands();
	init_sig();
	if (cfile) {
		fprintf(cfile, "q\n");
		rewind(cfile);
		pushfile(cfile);
	}
}

static void
usage(void)
{
	dbprintf("usage:\t%s [-c cmd]... [-p prog] [-frx] devname\n", progname);
	exit(1);
	/*NOTREACHED*/
}
